#include "rtspcamera.h"


RtspCamera::RtspCamera() {
    ifmt_ctx = nullptr;
    ofmt_ctx = nullptr;
    in_codecpar = nullptr;
    ofmt = nullptr;
    src_frame = av_frame_alloc();
    dst_frame = av_frame_alloc();
}

RtspCamera::~RtspCamera() = default;

int RtspCamera::saveJpg(AVFrame *pFrame, char *out_name) const {
    int ret;
    int width = pFrame->width;
    int height = pFrame->height;
    AVCodecContext *pCodeCtx = nullptr;

    AVFormatContext *pFormatCtx = avformat_alloc_context();
    pFormatCtx->oformat = av_guess_format("mjpeg", nullptr, nullptr);

//    if (avio_open(&pFormatCtx->pb, out_name, AVIO_FLAG_READ_WRITE) < 0) {
//        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Couldn't open output file");
//        return -1;
//    }

    AVStream *pAVStream = avformat_new_stream(pFormatCtx, 0);
    if (pAVStream == nullptr) {
        return -1;
    }

    AVCodecParameters *parameters = pAVStream->codecpar;
    parameters->codec_id = pFormatCtx->oformat->video_codec;
    parameters->codec_type = AVMEDIA_TYPE_VIDEO;
    parameters->format = AV_PIX_FMT_YUVJ420P;
    parameters->width = pFrame->width;
    parameters->height = pFrame->height;

    AVCodec *pCodec = avcodec_find_encoder(pAVStream->codecpar->codec_id);

    if (!pCodec) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Could not find encoder");
        return -1;
    }

    pCodeCtx = avcodec_alloc_context3(pCodec);
    if (!pCodeCtx) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera",
                            "Could not allocate video codec context");
        return -99;
    }

    if ((avcodec_parameters_to_context(pCodeCtx, pAVStream->codecpar)) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera",
                            "Failed to copy %s codec parameters to decoder context\n",
                            av_get_media_type_string(AVMEDIA_TYPE_VIDEO));
        return -1;
    }

    pCodeCtx->time_base = (AVRational) {1, 25};

    if (avcodec_open2(pCodeCtx, pCodec, nullptr) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Could not open codec");
        return -1;
    }

//    ret = avformat_write_header(pFormatCtx, nullptr);
//    if (ret < 0) {
//        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "write_header fail");
//        return -1;
//    }

    int y_size = width * height;

    AVPacket pkt;
    av_new_packet(&pkt, y_size * 3);

    ret = avcodec_send_frame(pCodeCtx, pFrame);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Could not avcodec_send_frame");
        return -1;
    }

    ret = avcodec_receive_packet(pCodeCtx, &pkt);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Could not avcodec_receive_packet");
        return -1;
    }

    // ret = av_write_frame(pFormatCtx, &pkt);

    cv::Mat rgb(704, 1280, CV_8UC3);
    __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "pkt.len=%d", pkt.size);
    rgb.data = pkt.data;
    on_image(rgb);

    if (ret < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "Could not av_write_frame");
        return -1;
    }

    av_packet_unref(&pkt);


    // av_write_trailer(pFormatCtx);


    avcodec_close(pCodeCtx);
    avio_close(pFormatCtx->pb);
    avformat_free_context(pFormatCtx);

    return 0;
}


void
RtspCamera::decode(AVCodecContext *dec_ctx, AVFrame *_src_frame, AVFrame *_dst_frame, AVPacket *pkt,
                   const char *filename) const {
    char buf[1024];
    int ret;

    ret = avcodec_send_packet(dec_ctx, pkt);
    if (ret < 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "Error sending a packet for decoding:%d",
                            ret);
        return;
    }

    while (ret >= 0) {
        ret = avcodec_receive_frame(dec_ctx, _src_frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            return;
        else if (ret < 0) {
            __android_log_print(ANDROID_LOG_ERROR, "ffmpeg", "Error during decoding");
            return;
        }

        /* the picture is allocated by the decoder. no need to free it */
        snprintf(buf, sizeof(buf), "%s_%d.jpg", filename, dec_ctx->frame_number);
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "saving __src_frame %05d  shape(%d,%d)",
                            dec_ctx->frame_number, src_frame->width, src_frame->height);


        SwsContext *sws_ctx = sws_getContext(1280, 704,
                                             AV_PIX_FMT_YUV420P,
                                             1280, 704,
                                             AV_PIX_FMT_RGB24,
                                             SWS_BICUBIC,
                                             nullptr,
                                             nullptr,
                                             nullptr);

        sws_scale(sws_ctx, _src_frame->data, _src_frame->linesize,
                  0, 704,
                  _dst_frame->data, _dst_frame->linesize);

        cv::Mat rgb(704, 1280, CV_8UC3);
        rgb.data = _dst_frame->data[0];
        __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "src_frame.len=%d", rgb.flags);

        on_image(rgb);

        // saveJpg(src_frame, buf);
    }
}

bool RtspCamera::open(const char *rtspUrl) {
    __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "open rtsp:%s", rtspUrl);
    running = true;
    const char *outfilename = "/sdcard/img";
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

    src_frame = av_frame_alloc();
    if (!src_frame) {
        __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "Could not allocate video src_frame");
        return -6;
    }


    int count_bytes_src = avpicture_get_size(AV_PIX_FMT_YUV420P,1280, 704);
    int count_bytes_dst = avpicture_get_size(AV_PIX_FMT_RGB24,1280, 704);

    auto* src_buff = (uint8_t*)av_malloc(count_bytes_src);
    auto* dst_buff = (uint8_t*)av_malloc(count_bytes_dst);

    avpicture_fill((AVPicture*)src_frame, src_buff, AV_PIX_FMT_YUV420P,1280, 704);
    avpicture_fill((AVPicture*)dst_frame, dst_buff, AV_PIX_FMT_RGB24,1280, 704);


    while (running) {
        AVStream *in_stream, *out_stream;
        av_usleep(10000);
        ret = av_read_frame(ifmt_ctx, pkt);
        if (ret == AVERROR_EOF) {
            __android_log_print(ANDROID_LOG_DEBUG, "ffmpeg", "NO FRAME:%d", ret);
            break;
        }
        in_stream = ifmt_ctx->streams[pkt->stream_index];
        if (pkt->stream_index != video_trade_index) {
            av_packet_unref(pkt);
            continue;
        }

        decode(h264, src_frame, dst_frame, pkt, outfilename);

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

void RtspCamera::close() {
    running = false;
}

void RtspCamera::set_status(int detect_status) {

}

void RtspCamera::on_image(const cv::Mat &rgb) const {
    __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "RtspCamera on_image");
}

void RtspCameraWindow::on_image(const cv::Mat &rgb) const {
    __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "RtspCameraWindow on_image");

    on_image_render(rgb);
}

void RtspCameraWindow::on_image_render(const cv::Mat &rgb) const {
    __android_log_print(ANDROID_LOG_DEBUG, "rtspcamera", "RtspCameraWindow on_image_render");
}

RtspCameraWindow::RtspCameraWindow() = default;

RtspCameraWindow::~RtspCameraWindow() = default;

void RtspCameraWindow::set_window(ANativeWindow *_win) {
    if (win) {
        ANativeWindow_release(win);
    }

    win = _win;
    ANativeWindow_acquire(win);
}

