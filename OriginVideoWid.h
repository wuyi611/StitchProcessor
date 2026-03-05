#ifndef ORIGINVIDEOWID_H
#define ORIGINVIDEOWID_H

#include <QWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QLabel>
#include "AppConfig.h"
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

    void setVideoCount(const int &cnt);

signals:
    void labelSizeChange(cv::Size size, ProcessThread::VideoType type);
public slots:
    void newResultMatOri(cv::Mat mat, int ID);

protected:
    bool eventFilter(QObject *obj, QEvent *event);
private:
    void addNewLabels();


    Ui::OriginVideoWid *ui;
    QImage currentImg[AppConfig::MAX_VIDEO_CNT];
    QVector<QLabel*> labels;
    int id;
    int videoCnt;

    void clearOldLabels();
};

#endif // ORIGINVIDEOWID_H
