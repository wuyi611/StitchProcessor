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

#include "AppConfig.h"
#include "DecodeThread.h"
#include "ProcessThread.h"
#include "Utils.h"

using namespace cv;

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
    void labelSizeChange(Size size, ProcessThread::VideoType type);

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void newResultMat(Mat mat);


private:
    Ui::Widget *ui;

    int sysInit();
    int loadConfig(const QString &filePt);
    int loadMatrix(const QString &filePt);
    int loadOffset(const QString &filePt);
    int uploadOffset(const QString &filePt);

    void dataInit();
    void saveOffset(const QVector<Point> &off);
    bool eventFilter(QObject *obj, QEvent *event);

    int videoCnt;
    int currentID;

    QMutex mutex;

    QString matrixPt;
    QString offsetPt;

    QVector<QString>        rtspUrl;
    QVector<Point>      offsets;
    QMap<QString, Mat>  matrixMap;
    QQueue<Mat>         ImgQueues;
    Size                displaySize;

    OriginVideoWid  *ovw;
    ProcessThread   *processThread;
    DecodeThread    *decodeThread[AppConfig::MAX_VIDEO_CNT];


};
#endif // WIDGET_H
