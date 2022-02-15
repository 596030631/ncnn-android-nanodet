// Tencent is pleased to support the open source community by making ncnn available.
//
// Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
// Licensed under the BSD 3-Clause License (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
// https://opensource.org/licenses/BSD-3-Clause
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/bitmap.h>


#include <android/log.h>
#include "opencv2/imgproc.hpp"

#include <jni.h>

#include <string>
#include <vector>

#include <platform.h>
#include <benchmark.h>

#include "nanodet.h"

#include "ndkcamera.h"

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/photo.hpp>

#include "rtspcamera.h"
#include "sys/time.h"

#if __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

JNIEnv *ncnn_env;
jobject ncnn_thiz;
jmethodID ncnn_callback;

struct timeval tv;
struct timezone tz;

static jstring string2jstring(JNIEnv *env, const char *pat) {
    jclass strClass = env->FindClass("java/lang/String");
    jmethodID ctorID = env->GetMethodID(strClass, "<init>", "([BLjava/lang/String;)V");
    jbyteArray bytes = env->NewByteArray((jsize) strlen(pat));
    env->SetByteArrayRegion(bytes, 0, (jsize) strlen(pat), (jbyte *) pat);
    jstring encoding = env->NewStringUTF("utf-8");
    return (jstring) env->NewObject(strClass, ctorID, bytes, encoding);
}

static int draw_unsupported(const cv::Mat &rgb) {
    const char text[] = "unsupported";

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 1.0, 1, &baseLine);

    int y = (rgb.rows - label_size.height) / 2;
    int x = (rgb.cols - label_size.width) / 2;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y),
                                cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0));

    return 0;
}

static int draw_fps(const cv::Mat &rgb) {
    // resolve moving average
    float avg_fps = 0.f;
    {
        static double t0 = 0.f;
        static float fps_history[10] = {0.f};

        double t1 = ncnn::get_current_time();
        if (t0 == 0.f) {
            t0 = t1;
            return 0;
        }

        float fps = 1000.f / (t1 - t0);
        t0 = t1;

        for (int i = 9; i >= 1; i--) {
            fps_history[i] = fps_history[i - 1];
        }
        fps_history[0] = fps;

        if (fps_history[9] == 0.f) {
            return 0;
        }

        for (int i = 0; i < 10; i++) {
            avg_fps += fps_history[i];
        }
        avg_fps /= 10.f;
    }

    char text[32];
    sprintf(text, "FPS=%.2f", avg_fps);

    int baseLine = 0;
    cv::Size label_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseLine);

    int y = 0;
    int x = rgb.cols - label_size.width;

    cv::rectangle(rgb, cv::Rect(cv::Point(x, y),
                                cv::Size(label_size.width, label_size.height + baseLine)),
                  cv::Scalar(255, 255, 255), -1);

    cv::putText(rgb, text, cv::Point(x, y + label_size.height),
                cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0));

    return 0;
}

static NanoDet *g_nanodet = 0;
static ncnn::Mutex lock;

class MyRtspCamera: public RtspCameraWindow {
public:
    virtual void on_image_render(const cv::Mat &rgb) const;
};


static const char *class_names[] = {
        "person", "bicycle", "car", "motorcycle", "airplane", "bus", "train", "truck", "boat",
        "traffic light",
        "fire hydrant", "stop sign", "parking meter", "bench", "bird", "cat", "dog", "horse",
        "sheep", "cow",
        "elephant", "bear", "zebra", "giraffe", "backpack", "umbrella", "handbag", "tie",
        "suitcase", "frisbee",
        "skis", "snowboard", "sports ball", "kite", "baseball bat", "baseball glove", "skateboard",
        "surfboard",
        "tennis racket", "bottle", "wine glass", "cup", "fork", "knife", "spoon", "bowl", "banana",
        "apple",
        "sandwich", "orange", "broccoli", "carrot", "hot dog", "pizza", "donut", "cake", "chair",
        "couch",
        "potted plant", "bed", "dining table", "toilet", "tv", "laptop", "mouse", "remote",
        "keyboard", "cell phone",
        "microwave", "oven", "toaster", "sink", "refrigerator", "book", "clock", "vase", "scissors",
        "teddy bear",
        "hair drier", "toothbrush"
};

void MyRtspCamera::on_image_render(const cv::Mat &rgb) const {
    {
        ncnn::MutexLockGuard g(lock);
        if (g_nanodet) {
            std::vector<Object> objects;

            g_nanodet->detect(rgb, objects);
            int start = tv.tv_usec;
            gettimeofday(&tv,&tz);
            start = (tv.tv_usec - start) / 1000;
            __android_log_print(ANDROID_LOG_WARN, "nanodet", "time use: %dms", start);
            for (int i = 0; i < objects.size(); ++i) {
                Object obj = objects[i];
                __android_log_print(ANDROID_LOG_WARN, "nanodet", "label[%s]  prob[%.5f] at %.2f %.2f %.2f x %.2f", class_names[obj.label], obj.prob,
                                    obj.rect.x, obj.rect.y, obj.rect.width, obj.rect.height);

            }
//            g_nanodet->draw(rgb, objects);
//
//            char str[512] = {0};
//
//            for (auto &object : objects) {
//                strcat(str, class_names[object.label]);
//            }
//            jstring js = string2jstring(ncnn_env, str);
//            ncnn_env->CallVoidMethod(ncnn_thiz, ncnn_callback, js);
            return;

        } else {
//            draw_unsupported(rgb);
        }
    }

//    draw_fps(rgb);
}

static MyRtspCamera *g_camera = nullptr;

extern "C" {

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnLoad");

    g_camera = new MyRtspCamera;

    return JNI_VERSION_1_4;
}

JNIEXPORT void JNI_OnUnload(JavaVM *vm, void *reserved) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "JNI_OnUnload");

    {
        ncnn::MutexLockGuard g(lock);

        delete g_nanodet;
        g_nanodet = 0;
    }

    delete g_camera;
    g_camera = 0;
}

// public native boolean loadModel(AssetManager mgr, int modelid, int cpugpu);
JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_loadModel(JNIEnv *env, jobject thiz, jobject assetManager,
                                                   jint modelid, jint cpugpu) {
    if (modelid < 0 || modelid > 6 || cpugpu < 0 || cpugpu > 1) {
        return JNI_FALSE;
    }

    AAssetManager *mgr = AAssetManager_fromJava(env, assetManager);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "loadModel %p", mgr);

    const char *modeltypes[] =
            {
                    "m",
                    "m-416",
                    "g",
                    "ELite0_320",
                    "ELite1_416",
                    "ELite2_512",
                    "RepVGG-A0_416"
            };

    const int target_sizes[] =
            {
                    320,
                    416,
                    416,
                    320,
                    416,
                    512,
                    416
            };

    const float mean_vals[][3] =
            {
                    {103.53f, 116.28f, 123.675f},
                    {103.53f, 116.28f, 123.675f},
                    {103.53f, 116.28f, 123.675f},
                    {127.f,   127.f,   127.f},
                    {127.f,   127.f,   127.f},
                    {127.f,   127.f,   127.f},
                    {103.53f, 116.28f, 123.675f}
            };

    const float norm_vals[][3] =
            {
                    {1.f / 57.375f, 1.f / 57.12f, 1.f / 58.395f},
                    {1.f / 57.375f, 1.f / 57.12f, 1.f / 58.395f},
                    {1.f / 57.375f, 1.f / 57.12f, 1.f / 58.395f},
                    {1.f / 128.f,   1.f / 128.f,  1.f / 128.f},
                    {1.f / 128.f,   1.f / 128.f,  1.f / 128.f},
                    {1.f / 128.f,   1.f / 128.f,  1.f / 128.f},
                    {1.f / 57.375f, 1.f / 57.12f, 1.f / 58.395f}
            };

    const char *modeltype = modeltypes[(int) modelid];
    int target_size = target_sizes[(int) modelid];
    bool use_gpu = (int) cpugpu == 1;

    // reload
    {
        ncnn::MutexLockGuard g(lock);

        if (use_gpu && ncnn::get_gpu_count() == 0) {
            // no gpu
            delete g_nanodet;
            g_nanodet = 0;
        } else {
            if (!g_nanodet)
                g_nanodet = new NanoDet;
            g_nanodet->load(mgr, modeltype, target_size, mean_vals[(int) modelid],
                            norm_vals[(int) modelid], use_gpu);
        }
    }

    return JNI_TRUE;
}

// public native boolean openCamera(int facing);
JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_openCamera(JNIEnv *env, jobject thiz, jint facing) {
    if (facing < 0 || facing > 1)
        return JNI_FALSE;

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "openCamera %d", facing);

    g_camera->open("rtsp://admin:123456@192.168.31.46/stream0");

    return JNI_TRUE;
}

// public native boolean testImage();
JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_testImage(JNIEnv *env, jobject thiz, jobject bitmap) {
    return JNI_TRUE;
}

// public native boolean closeCamera();
JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_closeCamera(JNIEnv *env, jobject thiz) {
    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "closeCamera");

//    g_camera->close();

    return JNI_TRUE;
}

// public native boolean setOutputWindow(Surface surface);
JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_setOutputWindow(JNIEnv *env, jobject thiz,
                                                         jobject surface) {
    ANativeWindow *win = ANativeWindow_fromSurface(env, surface);

    __android_log_print(ANDROID_LOG_DEBUG, "ncnn", "setOutputWindow %p", win);

    g_camera->set_window(win);

    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_initCallback(JNIEnv *env, jobject thiz) {
    ncnn_env = env;
    ncnn_thiz = thiz;
    jclass jclass = env->FindClass("com/tencent/nanodetncnn/NanoDetNcnn");
    if (jclass == 0) {
        __android_log_print(ANDROID_LOG_ERROR, "ncnn",
                            "can not find com/tencent/nanodetncnn/NanoDetNcnn path.");
        return JNI_FALSE;
    }
    ncnn_callback = env->GetMethodID(jclass, "callback", "(Ljava/lang/String;)V");
    return JNI_TRUE;
}

}


extern "C" JNIEXPORT void JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_open(
        JNIEnv *env,
        jobject /* this */, jstring rtspUrl) {
    const char * c_rtspUrl = env->GetStringUTFChars(rtspUrl,JNI_FALSE);
//    RtspCamera::getInstance().open(c_rtspUrl);
    env->ReleaseStringUTFChars(rtspUrl, c_rtspUrl);
}


extern "C" JNIEXPORT void JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_close(
        JNIEnv *env,
        jobject /* this */) {
//    RtspCamera::getInstance().close();
}

extern "C" JNIEXPORT void JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_start(
        JNIEnv *env,
        jobject /* this */, jstring pathName) {
    const char * c_pathName = env->GetStringUTFChars(pathName,JNI_FALSE);
//    rtspclient::getInstance().start(c_pathName);
    env->ReleaseStringUTFChars(pathName, c_pathName);
}


extern "C" JNIEXPORT void JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_stop(
        JNIEnv *env,
        jobject /* this */) {
//    rtspclient::getInstance().stop();
}


extern "C" JNIEXPORT void JNICALL
Java_com_tencent_nanodetncnn_NanoDetNcnn_scalingVideo(
        JNIEnv *env,
        jobject /* this */) {

}
