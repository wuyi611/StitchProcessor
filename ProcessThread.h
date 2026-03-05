#ifndef PROCESSTHREAD_H
#define PROCESSTHREAD_H

#include <QString>
#include <QImage>
#include <QThread>
#include <QDebug>
#include <QQueue>
#include <QMutexLocker>
#include <QMutex>
#include <QPixmap>
#include <opencv2/opencv.hpp>
#include <opencv2/cudawarping.hpp>  // 专门负责硬件加速的变换函数
#include <opencv2/cudaarithm.hpp>   // 如果需要矩阵运算

#include "AppConfig.h"

#define EXPAND 600


class ProcessThread : public QThread
{
    Q_OBJECT
public:
    ProcessThread(QObject *parent = nullptr);
    ~ProcessThread();
    // 设置视频信息
    void setConfig(const int &cnt, const QMap<QString, cv::Mat> &map, const QVector<cv::Point> &off);
    // 停止线程
    void stop();
    // 更新偏移量
    void updateOffset(const int &ID, const int &x, const int &y);
    // 图像置顶
    void bringToFront(const int &ID);
    // 获取位移
    QVector<cv::Point> getOffset();

    enum class VideoType {
        OriginVideo,
        TranformVideo
    };

protected:
    // 线程入口
    void run() override;

signals:
    void newResultMat(cv::Mat mat);

    void newResultMatOri(cv::Mat mat, int ID);


public slots:
    void newFrameMat(cv::Mat mat, int ID);

    void labelSizeChange(cv::Size size, VideoType type);

private:
    // yaml里的键
    QString key[AppConfig::MAX_VIDEO_CNT] = {"HA", "HB", "HC", "HD", "HE", "HF", "HG", "HH", "HI", "HJ"};
    std::atomic<bool> stopFlag;     // 停止标志位
    QMap<QString, cv::Mat> matrixMap;
    QMap<int, QQueue<cv::Mat>> matQueues;
    cv::Mat resultMat[AppConfig::MAX_VIDEO_CNT];
    cv::Size expandedSize;
    cv::Size orginTargetSize;
    cv::Size transformTargetSize;
    QMutex mutex;
    cv::Mat totalCanvas;          // 大画布
    cv::Mat originMat[AppConfig::MAX_VIDEO_CNT];
    QVector<cv::Point> offsets;
    QVector<int> displayOrder;
    int count;
    cv::Point matrixOff[3];
    cv::Mat adjustMatrixForHalfScale(const cv::Mat &H);
};

#endif // PROCESSTHREAD_H
