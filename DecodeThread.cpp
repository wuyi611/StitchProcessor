#include "DecodeThread.h"

DecodeThread::DecodeThread(QObject *parent) : QThread(parent)
{
    stopFlag = false;
    url = "";
}

DecodeThread::~DecodeThread()
{
    stop();
    wait();
}

void DecodeThread::setUrl(const QString &url)
{
    this->url = url;
}

void DecodeThread::setID(const int &ID)
{
    this->ID = ID;
}

void DecodeThread::stop()
{
    stopFlag = true;
}


void DecodeThread::run()
{
    avformat_network_init();
    runGpu();
}

void DecodeThread::ffDebug(const QString &msg)
{
    if (DEBUG) {
        qDebug() << "[DecoderThread]" << msg;
    }
}

// 硬件加速回调：优先选择 CUDA
enum AVPixelFormat DecodeThread::get_hw_format(AVCodecContext *ctx, const enum AVPixelFormat *pix_fmts)
{
    Q_UNUSED(ctx);
    for (int i = 0; pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        if (pix_fmts[i] == AV_PIX_FMT_D3D11) {
            return pix_fmts[i];
        }
    }
    return AV_PIX_FMT_NONE;
}

// 超时中断回调
int DecodeThread::ReadTimeoutCallback(void *ctx)
{
    DecodeThread *self = static_cast<DecodeThread*>(ctx);
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - self->lastReadTime).count();

    // 超过3秒无数据或收到停止信号，中断阻塞
    if (elapsed > 3000 || self->stopFlag) {
        return 1; // 返回 1 表示中断
    }
    return 0;
}



void DecodeThread::runGpu()
{
    cudaSetDevice(0);

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();          // GPU 显存帧
    AVFrame *swFrame = av_frame_alloc();        // CPU 内存帧
    while (!stopFlag) {
        AVFormatContext *fmtCtx = nullptr;
        AVCodecContext *codecCtx = nullptr;
        AVBufferRef *hwDeviceCtx = nullptr;         // 硬件格式上下文
        SwsContext *swsCtx = nullptr;               // 格式转换上下文
        int videoStream = -1;

        do {
            if (url.isEmpty()) { QThread::msleep(100); break; }

            // 1. 设置 RTSP 参数并打开流
            AVDictionary *opts = nullptr;
            av_dict_set(&opts, "rtsp_transport", "tcp", 0);
            av_dict_set(&opts, "flags", "low_delay", 0);

            fmtCtx = avformat_alloc_context();
            fmtCtx->interrupt_callback.callback = ReadTimeoutCallback;
            fmtCtx->interrupt_callback.opaque = this;
            this->lastReadTime = std::chrono::high_resolution_clock::now();

            if (avformat_open_input(&fmtCtx, url.toStdString().c_str(), nullptr, &opts) < 0) {
                av_dict_free(&opts); break;
            }
            av_dict_free(&opts);
            if (avformat_find_stream_info(fmtCtx, nullptr) < 0) break;

            videoStream = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
            if (videoStream < 0) break;

            // 2. 初始化硬件解码器
            const AVCodec *codec = avcodec_find_decoder(fmtCtx->streams[videoStream]->codecpar->codec_id);
            if (!codec) break;

            codecCtx = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecCtx, fmtCtx->streams[videoStream]->codecpar);

            // 创建 CUDA 硬件上下文
            if (av_hwdevice_ctx_create(&hwDeviceCtx, AV_HWDEVICE_TYPE_D3D11VA, nullptr, nullptr, 0) >= 0) {
                codecCtx->hw_device_ctx = av_buffer_ref(hwDeviceCtx);
                codecCtx->get_format = get_hw_format;
            } else { break; }


            if (avcodec_open2(codecCtx, codec, nullptr) < 0) break;

            // qDebug() << "GPU 解码已启动...";

            // 3. 解码循环
            while (!stopFlag) {
                this->lastReadTime = std::chrono::high_resolution_clock::now();
                if (av_read_frame(fmtCtx, pkt) < 0) break;

                if (pkt->stream_index == videoStream) {
                    if (avcodec_send_packet(codecCtx, pkt) == 0) {
                        while (avcodec_receive_frame(codecCtx, frame) == 0) {

                            // 检查是否为 CUDA 硬件格式
                            if (frame->format == AV_PIX_FMT_D3D11) {
                                // A. 将数据从 GPU 下载到 CPU (swFrame)
                                // 此处会将 NV12 显存数据转为 NV12 内存数据
                                if (av_hwframe_transfer_data(swFrame, frame, 0) >= 0) {

                                    int w = swFrame->width;
                                    int h = swFrame->height;


                                    swsCtx = sws_getCachedContext(swsCtx,
                                                                  w, h, (AVPixelFormat)swFrame->format,
                                                                  w, h, AV_PIX_FMT_BGRA,
                                                                  SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);

                                    if (swsCtx) {
                                        cv::Mat currentMat(h, w, CV_8UC4);

                                        // 将 Mat 的数据指针和每行字节数传给 FFmpeg
                                        uint8_t *destData[4] = { currentMat.data, nullptr, nullptr, nullptr };
                                        int destLinesize[4] = { (int)currentMat.step, 0, 0, 0 }; // step 相当于 bytesPerLine

                                        // 直接转换到 Mat 内存中
                                        sws_scale(swsCtx, swFrame->data, swFrame->linesize,
                                                  0, h, destData, destLinesize);

                                        //发送 Mat
                                        emit newFrameMat(currentMat.clone(), this->ID);
                                    }
                                }
                                av_frame_unref(swFrame);
                            }
                            av_frame_unref(frame);
                        }
                    }
                }
                av_packet_unref(pkt);
            }
            // 统一解引用防止失败直接跳出循环造成数据未释放
            av_packet_unref(pkt);
            av_frame_unref(frame);
            av_frame_unref(swFrame);
        } while (false);

        // 4. 资源清理
        if (swsCtx) sws_freeContext(swsCtx);
        if (codecCtx) avcodec_free_context(&codecCtx);
        if (fmtCtx) avformat_close_input(&fmtCtx);
        if (hwDeviceCtx) av_buffer_unref(&hwDeviceCtx);

        if (!stopFlag) QThread::sleep(2);
    }
    if (swFrame) av_frame_free(&swFrame);
    if (frame) av_frame_free(&frame);
    if (pkt) av_packet_free(&pkt);
}
