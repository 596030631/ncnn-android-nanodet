//
// Created by sjh on 2021/12/28.
//

#ifndef FFMPEG_ANDROID_RTSPCLIENT_H

#include "mutex"

extern "C" {
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libswscale/swscale.h"
#include "libavutil/time.h"
#include <libavutil/timestamp.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
}

#include "android/log.h"
#include <android/looper.h>
#include <android/native_window.h>
#include <android/sensor.h>
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <camera/NdkCameraMetadata.h>
#include <media/NdkImageReader.h>
#include <opencv2/core/core.hpp>

#define FFMPEG_ANDROID_RTSPCLIENT_H
#define DETECT_RUNNING 1
#define DETECT_STOP 2

class RtspCamera {
private:
    bool running = true;
    bool recording = false;
    bool frameKeyStart = false;

//    void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag);

    AVFormatContext *ifmt_ctx;
    AVCodecParameters *in_codecpar;
    int video_trade_index = -1;
    AVFormatContext *ofmt_ctx;
    AVOutputFormat *ofmt;
    AVCodec *codec{};
    AVCodecContext *h264{};
    AVFrame *src_frame = nullptr;
    AVFrame* dst_frame = nullptr;


public:
    RtspCamera();

    ~RtspCamera();

    bool open(const char *rtspUrl);

    void close();

    virtual void on_image(const cv::Mat &rgb) const;

    void set_status(int detect_status);

    void operator=(RtspCamera const &) = delete;

    int saveJpg(AVFrame *pFrame, char *out_name) const;

    void decode(AVCodecContext *dec_ctx, AVFrame *_src_frame,AVFrame *_dst_frame, AVPacket *pkt, const char *filename) const;
};


class RtspCameraWindow : public RtspCamera {
public:
    RtspCameraWindow();

    virtual ~RtspCameraWindow();

    void set_window(ANativeWindow *win);

    virtual void on_image_render(const cv::Mat &rgb) const;

public:
    mutable int accelerometer_orientation{};

private:
    ASensorManager *sensor_manager{};
    mutable ASensorEventQueue *sensor_event_queue{};
    const ASensor *accelerometer_sensor{};
    ANativeWindow *win{};

    void on_image(const cv::Mat &rgb) const;
};


#endif //FFMPEG_ANDROID_RTSPCLIENT_H
