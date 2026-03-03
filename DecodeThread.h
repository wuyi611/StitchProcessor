#ifndef DECODETHREAD_H
#define DECODETHREAD_H

#define DEBUG true

#include <QString>
#include <QImage>
#include <QThread>
#include <QDebug>

// FFmpeg 头文件
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

#include <cuda_runtime.h>
#include <opencv2/opencv.hpp>

class DecodeThread : public QThread
{
    Q_OBJECT
public:
    DecodeThread(QObject *parent = nullptr);
    ~DecodeThread();

    // 设置 RTSP 地址
    void setUrl(const QString &url);
    // 设置ID
    void setID(const int &ID);
    // 停止线程
    void stop();


protected:
    // 线程入口
    void run() override;

signals:

    void newFrameMat(cv::Mat img, int ID);

private:

    // GPU 解码主循环 (包含 ffmpeg 解码 -> CUDA 2D 拷贝逻辑)
    void runGpu();
    // 日志辅助
    void ffDebug(const QString &msg);
    // FFmpeg 回调：协商硬件加速格式 (自动选择 CUDA)
    static enum AVPixelFormat get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts);
    // FFmpeg 回调：处理读流超时 (防止网络卡死)
    static int ReadTimeoutCallback(void *ctx);

private:
    // 线程控制
    std::atomic<bool> stopFlag;     // 停止标志位
    QString url;                    // 视频流地址
    int ID;                         // 线程ID

    // 超时计算
    std::chrono::high_resolution_clock::time_point lastReadTime;

};

#endif // DECODETHREAD_H
