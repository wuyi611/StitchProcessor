#include "Widget.h"
#include "ui_Widget.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);
    sysInit();
}

Widget::~Widget()
{
    for (int i=0;i<videoCnt;i++) {
        if (decodeThread[i]) {
            decodeThread[i]->stop();
            decodeThread[i]->wait();
        }
    }
    if (processThread) {
        processThread->stop();
        processThread->wait();
        saveOffset(processThread->getOffset());
    }
    delete ui;
}


bool Widget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->label && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        emit labelSizeChange(cv::Size(resizeEvent->size().width(), resizeEvent->size().height()), ProcessThread::VideoType::TranformVideo);
    }
    // 记得返回基类的过滤结果
    return QWidget::eventFilter(obj, event);
}




void Widget::newResultMat(cv::Mat mat)
{
    // mat转为qimage
    QImage img((const uchar*)mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);

    ui->label->setPixmap(QPixmap::fromImage(img.copy()));
}

void Widget::saveOffset(const QVector<cv::Point> &off)
{

    offsets = off;
    QString offPt = QApplication::applicationDirPath() + BasePt + offsetPt;
    QSettings settings(offPt, QSettings::IniFormat);
    for (int i = 0; i < videoCnt; i++) {
        QString xKey = QString("Video%1/xOffset").arg(i);
        QString yKey = QString("Video%1/yOffset").arg(i);


        settings.setValue(xKey, offsets[i].x);
        settings.setValue(yKey, offsets[i].y);
        qDebug() << "Saving" << xKey << offsets[i].x;
    }
    settings.sync(); // 强制刷新到磁盘
}

int Widget::sysInit()
{
    // 当前窗口获取焦点
    this->setFocusPolicy(Qt::StrongFocus);

    qRegisterMetaType<cv::Mat>("cv::Mat");

    currentID = 0;

    offsets.resize(MAX_VIDEO_CNT);

    ui->label->installEventFilter(this);

    QSize qSize = ui->label->size();
    displaySize = cv::Size(qSize.width(), qSize.height());

    QString cfgPt = QApplication::applicationDirPath() + BasePt + CfgPt;
    if (loadConfig(cfgPt) < 0) {
        qWarning() << "config文件读取失败";
        return -1;
    }
    QString mtxPt = QApplication::applicationDirPath() + BasePt + matrixPt;
    if (loadMatrix(mtxPt) < 0) {
        qWarning() << "yaml文件读取失败";
        return -2;
    }

    QString offPt = QApplication::applicationDirPath() + BasePt + offsetPt;
    if (loadOffset(offPt) < 0) {
        qWarning() << "offset文件读取失败";
        return -3;
    }

    QString rtspUrl[3] = {"rtsp://192.168.9.82:554/11",
                          "rtsp://192.168.9.83:554/11",
                          "rtsp://192.168.9.84:554/11"};

    //videoCnt = 1;

    processThread = new ProcessThread(this);
    processThread->setMatrix(matrixMap, videoCnt);
    processThread->setOffset(offsets);
    connect(processThread, &ProcessThread::newResultMat, this, &Widget::newResultMat);
    connect(this, &Widget::labelSizeChange, processThread, &ProcessThread::labelSizeChange);
    //emit labelSizeChange(cv::Size(ui->label->size().width(), ui->label->size().height()), ProcessThread::VideoType::TranformVideo);
    processThread->start();

    ovw = new OriginVideoWid();
    connect(processThread, &ProcessThread::newResultMatOri, ovw, &OriginVideoWid::newResultMatOri);
    connect(ovw, &OriginVideoWid::labelSizeChange, processThread, &ProcessThread::labelSizeChange);
    ovw->show();

    for(int i=0;i<videoCnt;i++) {
        decodeThread[i] = new DecodeThread(this);
        decodeThread[i]->setID(i);
        decodeThread[i]->setUrl(rtspUrl[i]);
        connect(decodeThread[i], &DecodeThread::newFrameMat, processThread, &ProcessThread::newFrameMat);
        decodeThread[i]->start();
    }

    return 0;

}

int Widget::loadConfig(const QString &filePt)
{
    // 判断文件是否存在
    if (!QFile::exists(filePt)) {
        qWarning() << "config文件不存在：" << filePt;
        return -1;
    }
    // 创建ini对象
    QSettings settings(filePt, QSettings::IniFormat);

    // 读取值
    videoCnt = settings.value("Main/VideoCount", 0).toInt();
    matrixPt = settings.value("Main/MatrixPath", "").toString();
    offsetPt = settings.value("Main/OffsetPath", "").toString();

    return 0;
}

int Widget::loadOffset(const QString &filePt)
{
    // 判断文件是否存在
    if (!QFile::exists(filePt)) {
        qWarning() << "offset文件不存在：" << filePt;
        return -1;
    }
    // 创建ini对象
    QSettings settings(filePt, QSettings::IniFormat);

    // 读取值
    for (int i=0;i<videoCnt;i++) {
        QString xKey = QString("Video%1/xOffset").arg(i);
        QString yKey = QString("Video%1/yOffset").arg(i);
        int x = settings.value(xKey, 0).toInt();
        int y = settings.value(yKey, 0).toInt();
        offsets[i] = cv::Point(x, y);
    }
    return 0;
}


int Widget::loadMatrix(const QString &filePt)
{
    QFileInfo fileInfo(filePt);
    QString cleanPath = fileInfo.absoluteFilePath();

    // 1. 判断文件物理存在
    if (!fileInfo.exists()) {
        qWarning() << "matrix文件不存在：" << cleanPath;
        return -1;
    }

    // 2. 打开文件 (使用 Local8Bit 适配 Windows 路径)
    cv::FileStorage fs;
    try {
        fs.open(cleanPath.toLocal8Bit().constData(), cv::FileStorage::READ);
    } catch (const cv::Exception& e) {
        qWarning() << "OpenCV打开文件异常:" << e.what();
        return -2;
    }

    if (!fs.isOpened()) {
        qWarning() << "无法解析文件格式，请检查YAML头信息:" << cleanPath;
        return -3;
    }

    // 清空矩阵
    matrixMap.clear();

    // 3. 获取根节点并迭代
    cv::FileNode root = fs.root();

    // 遍历所有顶级节点
    for (cv::FileNodeIterator it = root.begin(); it != root.end(); ++it) {
        cv::FileNode node = *it;
        std::string keyName = node.name();

        // 核心改进：直接尝试读取 Mat
        // 如果节点不是矩阵，mat.empty() 会为真，这样更安全
        cv::Mat mat;
        try {
            node >> mat;
            if (!mat.empty()) {
                matrixMap.insert(QString::fromStdString(keyName), mat);
                // qDebug() << "成功加载矩阵:" << QString::fromStdString(keyName)
                //          << "[" << mat.rows << "x" << mat.cols << "]";
            }
        } catch (...) {
            //qWarning() << "跳过非矩阵节点:" << QString::fromStdString(keyName);
            continue;
        }
    }

    fs.release();

    if (matrixMap.isEmpty()) {
        qWarning() << "警告：文件中没有找到任何有效的矩阵数据";
        return -4;
    }

    return 0; // 必须返回0表示成功！
}



void Widget::keyPressEvent(QKeyEvent *event) {
    int step = event->modifiers() & Qt::ShiftModifier ? 10 : 2; // 按住 Shift 步进变大

    switch (event->key()) {
    case Qt::Key_W:
        processThread->updateOffset(currentID, 0, -step);
        break;
    case Qt::Key_S:
        processThread->updateOffset(currentID, 0, step);
        break;
    case Qt::Key_A:
        processThread->updateOffset(currentID, -step, 0);
        break;
    case Qt::Key_D:
        processThread->updateOffset(currentID, step, 0);
        break;
    case Qt::Key_PageUp:
        processThread->bringToFront(currentID); // 之前说的置顶逻辑
        break;
    case Qt::Key_1:
    case Qt::Key_2:
    case Qt::Key_3:
        currentID = event->key() - Qt::Key_1; // 切换相机 ID
        qDebug() << "Current Camera:" << currentID;
        break;
    default:
        QWidget::keyPressEvent(event); // 其他按键交给父类处理
    }
}


