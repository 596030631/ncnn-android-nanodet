#include "rtspclient.h"

rtspclient::rtspclient() {
    ifmt_ctx = nullptr;
    ofmt_ctx = nullptr;
    in_codecpar = nullptr;
    ofmt = nullptr;
}

rtspclient::~rtspclient() = default;

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize,
                     char *filename) {
    FILE *f;
    int i;

//    __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "pgm_save:%s", filename);

    f = fopen(filename, "w");
    fprintf(f, "P5\n%d %d\n%d\n", xsize, ysize, 255);
    for (i = 0; i < ysize; i++)
        fwrite(buf + i * wrap, 1, xsize, f);
    fclose(f);
}

static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt,
                   const char *filename) {
    char buf[1024];
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "Error sending a packet for decoding:%d", ret);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "Error during decoding");
            return;
        }

        /* the picture is allocated by the decoder. no need to free it */
        snprintf(buf, sizeof(buf), "%s_%d.jpg", filename, dec_ctx->frame_number);
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "saving frame %05d  shape(%d,%d)", dec_ctx->frame_number, frame->width, frame->height);

        pgm_save(frame->data[0], frame->linesize[0], frame->width, frame->height, buf);
    }
}

bool rtspclient::open(const char *rtspUrl) {
    __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "open rtsp:%s", rtspUrl);
    running = true;
    const char *outfilename = "/sdcard/test/img";
    AVPacket *pkt = nullptr;
    int ret;
    pkt = av_packet_alloc();
    if (!pkt) {
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "pkt error");
        return false;
    }
    AVDictionary *rtsp_option = nullptr;
    av_dict_set(&rtsp_option, "rtsp_transport", "tcp", 0);
    ret = avformat_open_input(&ifmt_ctx, rtspUrl, nullptr, &rtsp_option);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "Could not open '%s' %d", rtspUrl, ret);
        return false;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, nullptr)) < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg",
                            "Failed to retrieve input stream information");
        return false;
    }

    av_dump_format(ifmt_ctx, 0, rtspUrl, 0);

    for (int i = 0; i < ifmt_ctx->nb_streams; i++) {
        AVStream *inStream = ifmt_ctx->streams[i];
        in_codecpar = inStream->codecpar;
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "codec_type %d", in_codecpar->codec_type);

        if (in_codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_trade_index = i;
            break;
        }
    }

    __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "rtsp connect success");

    codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "Codec not found codec");
        return -4;
    }

    h264 = avcodec_alloc_context3(codec);

    if (!h264) {
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "Could not allocate video codec context");
        return -5;
    }

    /* open it */
    if (avcodec_open2(h264, codec, nullptr) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "Could not open codec");
        return -6;
    }

    frame = av_frame_alloc();
    if (!frame) {
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "Could not allocate video frame");
        return -6;
    }

    while (running) {
        AVStream *in_stream, *out_stream;
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret < 0) {
            __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "NO FRAME:%d", ret);
            break;
        }
        in_stream = ifmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index != video_trade_index) {
            av_packet_unref(pkt);
            continue;
        }

//        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "stream:%d", video_trade_index);


        decode(h264, frame, pkt, outfilename);

    }

    av_packet_free(&pkt);
    avformat_close_input(&ifmt_ctx);
    if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    if (ofmt_ctx) avformat_free_context(ofmt_ctx);
    if (ret < 0 && ret != AVERROR_EOF) {
        return false;
    }
    return true;
}

void rtspclient::close() {
    running = false;
}

void rtspclient::set_status(int detect_status) {

}
