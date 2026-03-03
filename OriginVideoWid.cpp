#include "OriginVideoWid.h"
#include "ui_OriginVideoWid.h"

OriginVideoWid::OriginVideoWid(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::OriginVideoWid)
{
    ui->setupUi(this);
    ui->label->installEventFilter(this);
    labels[0] = ui->label;
    labels[1] = ui->label_2;
    labels[2] = ui->label_3;
}

OriginVideoWid::~OriginVideoWid()
{
    delete ui;
}

void OriginVideoWid::newResultMatOri(cv::Mat mat, int ID)
{
    // mat转为qimage
    QImage img((const uchar*)mat.data, mat.cols, mat.rows, mat.step, QImage::Format_RGBA8888);

    labels[ID]->setPixmap(QPixmap::fromImage(img.copy()));


    update();
}



bool OriginVideoWid::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->label && event->type() == QEvent::Resize) {
        QResizeEvent *resizeEvent = static_cast<QResizeEvent*>(event);
        emit labelSizeChange(cv::Size(resizeEvent->size().width(), resizeEvent->size().height()), ProcessThread::VideoType::OriginVideo);
    }
    // 记得返回基类的过滤结果
    return QWidget::eventFilter(obj, event);
}
