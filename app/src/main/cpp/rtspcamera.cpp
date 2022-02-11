//
// Created by sjh on 2022/2/11.
//

#include "rtspcamera.h"

RtspCamera::RtspCamera() = default;
RtspCamera::~RtspCamera() = default;

static void fill_yuv_image(uint8_t *data[4], const int linesize[4],
                           int width, int height, int frame_index) {
    int x, y;

    /* Y */
    for (y = 0; y < height; y++)
        for (x = 0; x < width; x++)
            data[0][y * linesize[0] + x] = x + y + frame_index * 3;

    /* Cb and Cr */
    for (y = 0; y < height / 2; y++) {
        for (x = 0; x < width / 2; x++) {
            data[1][y * linesize[1] + x] = 128 + y + frame_index * 2;
            data[2][y * linesize[2] + x] = 64 + x + frame_index * 5;
        }
    }
}

void RtspCamera::testScaleVideo() {
    __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "scale video");

    uint8_t *src_data[4], *dst_data[4];
    int src_linesize[4], dst_linesize[4];
    int src_w = 320, src_h = 240, dst_w, dst_h;
    enum AVPixelFormat src_pix_fmt = AV_PIX_FMT_YUV420P, dst_pix_fmt = AV_PIX_FMT_RGB24;
    const char *dst_size;
    const char *dst_filename = nullptr;
    FILE *dst_file = NULL;
    int dst_bufsize;
    struct SwsContext *sws_ctx;
    int i, ret;

    dst_size = "1280x704";

    if (av_parse_video_size(&dst_w, &dst_h, dst_size) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Invalid size '%s', must be in the form WxH or a valid size abbreviation",
                            dst_size);
        return;
    }

    /* create scaling context */
    sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt,
                             dst_w, dst_h, dst_pix_fmt,
                             SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx) {
        __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Impossible to create scale context for the conversion "
                                                                "fmt:%s s:%dx%d -> fmt:%s s:%dx%d",
                            av_get_pix_fmt_name(src_pix_fmt), src_w, src_h,
                            av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h);
        ret = AVERROR(EINVAL);
        goto end;
    }

    /* allocate source and destination image buffers */
    if ((ret = av_image_alloc(src_data, src_linesize,
                              src_w, src_h, src_pix_fmt, 16)) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Could not allocate source image");
        goto end;
    }

    /* buffer is going to be written to rawvideo file, no alignment */
    if ((ret = av_image_alloc(dst_data, dst_linesize,
                              dst_w, dst_h, dst_pix_fmt, 1)) < 0) {
        __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Could not allocate destination image");
        goto end;
    }
    dst_bufsize = ret;

    for (i = 0; i < 100; i++) {
        /* generate synthetic video */
        fill_yuv_image(src_data, src_linesize, src_w, src_h, i);

        /* convert to destination format */
        sws_scale(sws_ctx, (const uint8_t *const *) src_data,
                  src_linesize, 0, src_h, dst_data, dst_linesize);

        __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "dst_bufsize=%d", dst_bufsize);

        /* write scaled image to file */
        dst_filename = "/sdcard/dest-";
        char file_name[128];
        sprintf(file_name, "%s%d", dst_filename, i);
        dst_file = fopen(file_name, "wb");
        if (!dst_file) {
            __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Could not open destination file %s", dst_filename);
            return;
        }

        cv::Mat rgb(dst_h, dst_w, CV_8UC3);
        rgb.data = dst_data[0];
        on_image(rgb);

        // fwrite(dst_data[0], 1, dst_bufsize, dst_file);
    }
    __android_log_print(ANDROID_LOG_DEBUG, "scaling_video", "Scaling succeeded. Play the output file with the command:\n"
                                                            "ffplay -f rawvideo -pix_fmt %s -video_size %dx%d %s",
                        av_get_pix_fmt_name(dst_pix_fmt), dst_w, dst_h, dst_filename);

    end:
    fclose(dst_file);
    av_freep(&src_data[0]);
    av_freep(&dst_data[0]);
    sws_freeContext(sws_ctx);
}

RtspCameraWindow::RtspCameraWindow() {
    sensor_manager = 0;
    sensor_event_queue = 0;
    accelerometer_sensor = 0;
    win = 0;

    accelerometer_orientation = 0;

    // sensor
    sensor_manager = ASensorManager_getInstance();

    accelerometer_sensor = ASensorManager_getDefaultSensor(sensor_manager,
                                                           ASENSOR_TYPE_ACCELEROMETER);
}

void RtspCameraWindow::on_image_render(cv::Mat &rgb) const {
}

RtspCameraWindow::~RtspCameraWindow() {
    if (accelerometer_sensor) {
        ASensorEventQueue_disableSensor(sensor_event_queue, accelerometer_sensor);
        accelerometer_sensor = 0;
    }

    if (sensor_event_queue) {
        ASensorManager_destroyEventQueue(sensor_manager, sensor_event_queue);
        sensor_event_queue = 0;
    }

    if (win) {
        ANativeWindow_release(win);
    }
}

void RtspCameraWindow::set_window(ANativeWindow *_win) {
    if (win) {
        ANativeWindow_release(win);
    }

    win = _win;
    ANativeWindow_acquire(win);
}
