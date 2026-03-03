#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QDebug>
#include <QMap>
#include <QFile>
#include <QFileInfo>
#include <QMetaType>
#include <QMutexLocker>
#include <QMutex>
#include <QKeyEvent>
#include <opencv2/opencv.hpp>
#include <QPainter>
#include <OriginVideoWid.h>

#include "DecodeThread.h"
#include "ProcessThread.h"
#include "Utils.h"

// 资源路径

#if defined(DEBUG) || defined(_DEBUG)
#define BasePt "/../../../"
#else
#define BasePt "/"
#endif

#define CfgPt       "config.ini"

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class Widget : public QWidget
{
    Q_OBJECT

public:
    Widget(QWidget *parent = nullptr);
    ~Widget();

signals:
    void labelSizeChange(cv::Size size, ProcessThread::VideoType type);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void newResultMat(cv::Mat mat);


private:
    Ui::Widget *ui;

    int sysInit();
    int loadConfig(const QString &filePt);
    int loadMatrix(const QString &filePt);
    int loadOffset(const QString &filePt);
    int uploadOffset(const QString &filePt);
    void saveOffset(const QVector<cv::Point> &off);

    int videoCnt;
    QString matrixPt;
    QString offsetPt;
    QMap<QString, cv::Mat> matrixMap;
    QQueue<cv::Mat> ImgQueues;
    QMutex mutex;
    cv::Size displaySize;
    int currentID;
    QVector<cv::Point> offsets;
    QImage currentImg;

    DecodeThread *decodeThread[MAX_VIDEO_CNT];
    ProcessThread *processThread;
    OriginVideoWid *ovw;


    bool eventFilter(QObject *obj, QEvent *event);
};
#endif // WIDGET_H
