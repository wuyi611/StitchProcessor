#include "ProcessThread.h"

ProcessThread::ProcessThread(QObject *parent) : QThread(parent)
{
    stopFlag = false;
    offsets.resize(MAX_VIDEO_CNT);
    offsets[0] = cv::Point(100, 100);
    offsets[1] = cv::Point(300, 300);
    offsets[2] = cv::Point(500, 500);

    // 清空总画布
    totalCanvas.setTo(cv::Scalar(0, 0, 0, 0));
}

ProcessThread::~ProcessThread()
{
    stop();
    wait();
}



void ProcessThread::setMatrix(const QMap<QString, cv::Mat> &map, const int &cnt)
{
    matrixMap = map;
    count = cnt;
    // 输出单画布大小
    expandedSize = cv::Size(WIDTH + EXPAND, HEIGHT + EXPAND);

    // 给矩阵增加平移补偿，将图像向右下移动，使其在更大画布的中心
    for (int i=0;i<count;i++) {
        if (matrixMap.contains(key[i])) {
            matrixMap[key[i]].at<double>(0, 2) += EXPAND / 2;
            matrixMap[key[i]].at<double>(1, 2) += EXPAND / 2;
        }
        resultMat[i] = cv::Mat::zeros(expandedSize, CV_8UC4);
        displayOrder.append(i);
    }
    totalCanvas = cv::Mat::zeros(cv::Size(4000, 1500), CV_8UC4);




}

void ProcessThread::setOffset(const QVector<cv::Point> &off)
{
    offsets = off;
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
    while (queue.size() >= MAX_QUEUE_SIZE) {
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

                // 2. 执行硬件加速变换
                // 注意：matrixMap[key[i]] 建议也预先上传为 GpuMat 以提升效率
                cv::cuda::warpPerspective(gpuSrc, gpuDst, matrixMap[key[i]], expandedSize, cv::INTER_LINEAR);

                // 3. 将结果下载回 CPU 内存
                gpuDst.download(resultMat[i]);

                cv::Mat resizeMat;
                cv::resize(mat, resizeMat, orginTargetSize, 0, 0, cv::INTER_NEAREST);

                // 转换颜色空间给 Qt 用 (OpenCV 默认是 BGR，Qt 是 RGB)
                cv::Mat finalMat;
                cv::cvtColor(resizeMat, finalMat, cv::COLOR_BGRA2RGBA);

                emit newResultMatOri(finalMat.clone(), j);

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
            // 在子线程提前做好 Resize，减轻主线程负担！
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
