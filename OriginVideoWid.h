#ifndef ORIGINVIDEOWID_H
#define ORIGINVIDEOWID_H

#include <QWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QLabel>
#include "Utils.h"
#include "ProcessThread.h"

namespace Ui {
class OriginVideoWid;
}

class OriginVideoWid : public QWidget
{
    Q_OBJECT

public:
    explicit OriginVideoWid(QWidget *parent = nullptr);
    ~OriginVideoWid();

signals:
    void labelSizeChange(cv::Size size, ProcessThread::VideoType type);
public slots:
    void newResultMatOri(cv::Mat mat, int ID);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    Ui::OriginVideoWid *ui;
    QImage currentImg[MAX_VIDEO_CNT];
    QLabel *labels[MAX_VIDEO_CNT];
    int id;

};

#endif // ORIGINVIDEOWID_H
