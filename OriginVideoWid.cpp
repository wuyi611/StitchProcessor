#include "OriginVideoWid.h"
#include "ui_OriginVideoWid.h"

OriginVideoWid::OriginVideoWid(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OriginVideoWid)
{
    ui->setupUi(this);
    labels.resize(AppConfig::MAX_VIDEO_CNT);
}

OriginVideoWid::~OriginVideoWid()
{
    delete ui;
}

void OriginVideoWid::clearOldLabels() {
    for (QLabel* label : labels) {
        if (label) {
            label->removeEventFilter(this);
            label->deleteLater();
        }
    }

    QLayoutItem *item;
    while ((item = ui->horizontalLayout->layout()->takeAt(0)) != nullptr) {
        if (item->widget()) {
            item->widget()->deleteLater(); // 安全删除控件
        }
        delete item;
    }
    labels.clear(); // 清空存储指针的容器
}

void OriginVideoWid::setVideoCount(const int &cnt)
{
    videoCnt = cnt;
    addNewLabels();
}

void OriginVideoWid::newResultMatOri(cv::Mat mat, int ID)
{
    // mat转为qimage
    QImage img((const uchar*)mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);

    labels[ID]->setPixmap(QPixmap::fromImage(img.copy()));
}



bool OriginVideoWid::eventFilter(QObject *obj, QEvent *event) {
    if (obj == labels[0] && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        emit labelSizeChange(cv::Size(resizeEvent->size().width(), resizeEvent->size().height()), ProcessThread::VideoType::OriginVideo);
        //qDebug()<<resizeEvent->size();
    }
    // 记得返回基类的过滤结果
    return QWidget::eventFilter(obj, event);
}

void OriginVideoWid::addNewLabels()
{
    if (videoCnt <= 0)
        return;
    QLayout* layout = qobject_cast<QLayout*>(ui->horizontalLayout);
    if (!layout) return;

    clearOldLabels();
    // 2. 从单例获取动态数量
    for (int i = 0; i < videoCnt; ++i) {
        // 3. 创建实例（必须指定父对象，或稍后加入布局）
        labels.append(new QLabel(QString("视频窗口 %1").arg(i + 1), this));

        // 4. 设置属性
        labels[i]->setStyleSheet("border: 1px solid black; background: grey;");
        labels[i]->setAlignment(Qt::AlignCenter);
        labels[i]->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

        // 5. 加入布局（这一步会自动处理 Label 的位置和大小）
        layout->addWidget(labels[i]);

        // 只有第一个 label 需要监听大小变化
        if (i == 0) {
            labels[i]->installEventFilter(this);
        }
    }
}
