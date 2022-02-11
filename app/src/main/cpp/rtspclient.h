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
}

#include "android/log.h"

#define FFMPEG_ANDROID_RTSPCLIENT_H
#define DETECT_RUNNING 1
#define DETECT_STOP 2

class rtspclient {
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
    AVFrame *frame{};

    rtspclient();

    ~rtspclient();

public:
    bool open(const char *rtspUrl);

    void close();

    void set_status(int detect_status);

    static rtspclient &getInstance() {
        static rtspclient instance;
        return instance;
    }

    rtspclient(rtspclient const &) = delete;

    void operator=(rtspclient const &) = delete;
};


#endif //FFMPEG_ANDROID_RTSPCLIENT_H
