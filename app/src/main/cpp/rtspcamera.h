//
// Created by sjh on 2022/2/11.
//

#ifndef NCNN_ANDROID_NANODET_RTSPCAMERA_H
#define NCNN_ANDROID_NANODET_RTSPCAMERA_H

extern "C" {
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libavutil/parseutils.h>
#include "libswscale/swscale.h"
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

class RtspCamera {
public:
    RtspCamera();
    ~RtspCamera();
    void testScaleVideo();
    virtual void on_image(const cv::Mat& rgb) const;
};



class RtspCameraWindow : public RtspCamera
{
public:
    RtspCameraWindow();
    virtual ~RtspCameraWindow();

    void set_window(ANativeWindow* win);

    virtual void on_image_render(cv::Mat& rgb) const;

public:
    mutable int accelerometer_orientation;

private:
    ASensorManager* sensor_manager;
    mutable ASensorEventQueue* sensor_event_queue;
    const ASensor* accelerometer_sensor;
    ANativeWindow* win;
};


#endif //NCNN_ANDROID_NANODET_RTSPCAMERA_H
