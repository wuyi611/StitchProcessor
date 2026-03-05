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

void Widget::dataInit()
{
    // 默认选中的拼接图像ID
    currentID = 0;
    // 拼接点位容器大小初始化
    offsets.resize(AppConfig::MAX_VIDEO_CNT);
    // url容器大小初始化
    rtspUrl.resize(AppConfig::MAX_VIDEO_CNT);

}



int Widget::sysInit()
{
    // 当前窗口获取焦点
    this->setFocusPolicy(Qt::StrongFocus);
    // 注册mat类型
    qRegisterMetaType<Mat>("Mat");
    // 注册事件过滤器
    ui->label->installEventFilter(this);

    ui->label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    // 数据初始化
    dataInit();
    // 加载配置信息
    QString cfgPt = QApplication::applicationDirPath() + BasePt + CfgPt;
    if (loadConfig(cfgPt) < 0) {
        qWarning() << "config文件读取失败";
        return -1;
    }
    // 加载矩阵数据
    QString mtxPt = QApplication::applicationDirPath() + BasePt + matrixPt;
    if (loadMatrix(mtxPt) < 0) {
        qWarning() << "yaml文件读取失败";
        return -2;
    }
    // 加载拼接点位
    QString offPt = QApplication::applicationDirPath() + BasePt + offsetPt;
    if (loadOffset(offPt) < 0) {
        qWarning() << "offset文件读取失败";
        return -3;
    }

    if (videoCnt > 10)
        return -4;

    processThread = new ProcessThread(this);
    processThread->setConfig(videoCnt, matrixMap, offsets);
    connect(processThread, &ProcessThread::newResultMat, this, &Widget::newResultMat);
    connect(this, &Widget::labelSizeChange, processThread, &ProcessThread::labelSizeChange);
    processThread->start();

    ovw = new OriginVideoWid();
    ovw->setVideoCount(videoCnt);
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




bool Widget::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->label && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        emit labelSizeChange(Size(resizeEvent->size().width(), resizeEvent->size().height()), ProcessThread::VideoType::TranformVideo);
    }
    // 记得返回基类的过滤结果
    return QWidget::eventFilter(obj, event);
}




void Widget::newResultMat(Mat mat)
{
    // mat转为qimage
    QImage img((const uchar*)mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);

    ui->label->setPixmap(QPixmap::fromImage(img.copy()));
}

void Widget::saveOffset(const QVector<Point> &off)
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

    AppConfig::getInstance().width = settings.value("VideoSize/Width", 1920).toInt();
    AppConfig::getInstance().height = settings.value("VideoSize/Height", 1080).toInt();

    for (int i=0;i<videoCnt;i++) {
        QString key = QString("VideoSource/Source%1").arg(i+1);
        rtspUrl[i] = settings.value(key, "").toString();
    }

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
        offsets[i] = Point(x, y);
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
    FileStorage fs;
    try {
        fs.open(cleanPath.toLocal8Bit().constData(), FileStorage::READ);
    } catch (const Exception& e) {
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
    FileNode root = fs.root();

    // 遍历所有顶级节点
    for (FileNodeIterator it = root.begin(); it != root.end(); ++it) {
        FileNode node = *it;
        std::string keyName = node.name();

        // 核心改进：直接尝试读取 Mat
        // 如果节点不是矩阵，mat.empty() 会为真，这样更安全
        Mat mat;
        try {
            node >> mat;
            if (!mat.empty()) {
                matrixMap.insert(QString::fromStdString(keyName), mat);
            }
        } catch (...) {
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
        processThread->bringToFront(currentID);
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


