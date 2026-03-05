#include "ProcessThread.h"

ProcessThread::ProcessThread(QObject *parent) : QThread(parent)
{
    stopFlag = false;
    offsets.resize(AppConfig::MAX_VIDEO_CNT);

    // 清空总画布
    totalCanvas.setTo(cv::Scalar(0, 0, 0, 0));

    matrixOff[0] = cv::Point(50,100);
    matrixOff[1] = cv::Point(100,250);
    matrixOff[2] = cv::Point(600,20);
}

ProcessThread::~ProcessThread()
{
    stop();
    wait();
}

void ProcessThread::setConfig(const int &cnt, const QMap<QString, cv::Mat> &map, const QVector<cv::Point> &off)
{
    count = cnt;
    matrixMap = map;
    offsets = off;

    expandedSize = cv::Size(AppConfig::getInstance().width + EXPAND, AppConfig::getInstance().height + EXPAND);

    cv::Size totolSize = cv::Size(AppConfig::getInstance().width + EXPAND + (cnt - 1) * AppConfig::getInstance().width * 0.5, AppConfig::getInstance().height + EXPAND);

    totalCanvas = cv::Mat::zeros(totolSize, CV_8UC4);

    for (int i=0;i<count;i++) {
        if (matrixMap.contains(key[i])) {
            // 给矩阵增加平移补偿，将图像向右下移动，使其在更大画布的中心
            matrixMap[key[i]].at<double>(0, 2) += matrixOff[i].x;
            matrixMap[key[i]].at<double>(1, 2) += matrixOff[i].y;
        }
        // 初始化为黑色
        resultMat[i] = cv::Mat::zeros(expandedSize, CV_8UC4);
        // 初始化覆盖顺序
        displayOrder.append(i);
    }
}






void ProcessThread::stop()
{
    stopFlag = true;
}

void ProcessThread::updateOffset(const int &ID, const int &x, const int &y)
{
    QMutexLocker locket(&mutex);
    offsets[ID].x += x;
    offsets[ID].y += y;
}

void ProcessThread::bringToFront(const int &ID)
{
    QMutexLocker locker(&mutex);
    if (displayOrder.contains(ID)) {
        displayOrder.removeAll(ID);
        displayOrder.append(ID); // 末尾 = 最后画 = 最顶层
    }
}

QVector<cv::Point> ProcessThread::getOffset()
{
    QMutexLocker locker(&mutex);
    return offsets;
}

void ProcessThread::newFrameMat(cv::Mat mat, int ID)
{
    QMutexLocker locker(&mutex);

    QQueue<cv::Mat> &queue = matQueues[ID];

    // 限制队列的长度
    while (queue.size() >= AppConfig::MAX_QUEUE_SIZE) {
        queue.dequeue();
    }

    queue.enqueue(mat);
}

void ProcessThread::labelSizeChange(cv::Size size, VideoType type)
{
    QMutexLocker locker(&mutex);
    switch (type) {
    case VideoType::OriginVideo:
        orginTargetSize = size;
        break;
    case VideoType::TranformVideo:
        transformTargetSize = size;
        break;
    default:
        break;
    }
}



void ProcessThread::run()
{

    while (!stopFlag) {
        bool hasNewData = false; // 核心：标记本轮是否有新帧

        for (int j=0;j<count;j++) {

            // 取源图像加锁
            mutex.lock();
            // 获取当前层级对应的相机 ID
            int i = displayOrder[j];

            cv::Mat mat;
            cv::Point offset = offsets[i];
            if (matQueues.contains(i) && !matQueues[i].isEmpty() && matrixMap.contains(key[i])) {
                mat = matQueues[i].dequeue();
                hasNewData = true;

            }
            mutex.unlock();
            // 执行投影变换
            if (!mat.empty()) {
                cv::cuda::GpuMat gpuSrc, gpuDst;
                gpuSrc.upload(mat);

                // 执行硬件加速变换
                // 注意：matrixMap[key[i]] 建议也预先上传为 GpuMat 以提升效率
                cv::cuda::warpPerspective(gpuSrc, gpuDst, matrixMap[key[i]], expandedSize, cv::INTER_LINEAR);

                // 将结果下载回 CPU 内存
                gpuDst.download(resultMat[i]);

                // 处理并发送原始图像
                cv::Mat resizeMat;
                double aspectRatio = (double)mat.cols / (double)mat.rows;

                // 1. 以宽度为基准计算目标高度
                int targetW = orginTargetSize.width;
                int targetH = static_cast<int>(targetW / aspectRatio); // 注意：高度 = 宽度 / 比例

                // 2. 执行缩放
                cv::resize(mat, resizeMat, cv::Size(targetW, targetH));

                // 3. 创建黑色底板 (假设 orginTargetSize 是 16:9 或其它，颜色空间与 mat 一致)
                // 使用 mat.type() 确保通道数（如 BGRA）一致
                cv::Mat finalCanvas = cv::Mat::zeros(orginTargetSize.height, orginTargetSize.width, mat.type());

                // 4. 计算粘贴位置 (垂直居中)
                // 如果 targetH > orginTargetSize.height，说明高度超出了，需要做截断处理或调整逻辑
                // 这里假设 orginTargetSize.height 足够放下 targetH
                int yOffset = (orginTargetSize.height - targetH) / 2;

                // 保护逻辑：防止计算出的 offset 为负数导致崩溃
                if (yOffset < 0) yOffset = 0;
                int copyHeight = std::min(targetH, orginTargetSize.height);

                // 5. 将缩放后的图像贴到黑色底板的 ROI 区域
                // cv::Rect(x, y, width, height)
                cv::Rect roi(0, yOffset, targetW, copyHeight);
                resizeMat(cv::Rect(0, 0, targetW, copyHeight)).copyTo(finalCanvas(roi));

                // 6. 最后转换颜色空间给 Qt
                cv::Mat qtReadyMat;
                cv::cvtColor(finalCanvas, qtReadyMat, cv::COLOR_BGRA2RGBA);

                emit newResultMatOri(qtReadyMat, i);

            }
            // 计算ROI并处理越界
            cv::Rect roi(offset, expandedSize);
            cv::Rect canvasRect(0, 0, totalCanvas.cols, totalCanvas.rows);

            // 求交集，裁去越界部分
            cv::Rect intersection = roi & canvasRect;

            if (intersection.width > 0 && intersection.height > 0) {
                // 计算在 warped 图中的对应起始区域
                cv::Rect srcRect(intersection.x - offset.x, intersection.y - offset.y,
                                 intersection.width, intersection.height);

                // 使用 Alpha 通道掩码进行拷贝 (解决黑边遮挡)
                cv::Mat matROI = resultMat[i](srcRect);
                cv::Mat mask;
                // 提取 RGBA 的第4通道作为 mask，只有非透明像素才会被贴上去
                cv::extractChannel(matROI, mask, 3);
                // 拼接到画布
                matROI.copyTo(totalCanvas(intersection), mask);
            }



        }
        // 核心拦截：只有当确实提取到了新画面，或者强制需要刷新时，才发给主 UI
        if (hasNewData) {
            // 处理并发送拼接的图像
            cv::Mat resizeMat;
            cv::resize(totalCanvas, resizeMat, transformTargetSize, 0, 0, cv::INTER_NEAREST);

            // 转换颜色空间给 Qt 用 (OpenCV 默认是 BGR，Qt 是 RGB)
            cv::Mat finalMat;
            cv::cvtColor(resizeMat, finalMat, cv::COLOR_BGRA2RGBA);

            emit newResultMat(finalMat.clone());
            totalCanvas.setTo(cv::Scalar(0, 0, 0, 0)); // 发送完再清空画布

        }

        // msleep(15);

        // // 1. 创建一个用于显示的临时 Mat
        // cv::Mat showMat;

        // // 2. 设定缩放比例（例如 0.5 倍，即 1500x750）
        // // 或者设定固定大小，如 cv::Size(1280, 720)
        // double scale = 0.5;
        // cv::resize(totalCanvas, showMat, cv::Size(), scale, scale, cv::INTER_AREA);

        // // 3. 在窗口显示缩放后的图像
        // cv::imshow("Stitching Result (Reduced)", showMat);
        // cv::waitKey(1);

        // 适当休眠，避免抢占 CPU。10ms 其实有点快，30ms (约 30fps) 足够了

    }

}
