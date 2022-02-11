project(nanodetncnn)

cmake_minimum_required(VERSION 3.10)

set(OpenCV_DIR ${CMAKE_SOURCE_DIR}/opencv-mobile-4.5.1-android/sdk/native/jni)

find_package(OpenCV REQUIRED core imgproc)

set(ncnn_DIR ${CMAKE_SOURCE_DIR}/ncnn-20210525-android-vulkan/${ANDROID_ABI}/lib/cmake/ncnn)
find_package(ncnn REQUIRED)

set(CMAKE_CXX_FLAGS "-L${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}")

include_directories(${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/include)

add_library(avcodec SHARED IMPORTED)

set_target_properties(avcodec PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavcodec.so)

add_library(avdevice SHARED IMPORTED)

set_target_properties(avdevice PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavdevice.so)

add_library(avformat SHARED IMPORTED)

set_target_properties(avformat PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavformat.so)

add_library(avutil SHARED IMPORTED)

set_target_properties(avutil PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavutil.so)

add_library(postproc SHARED IMPORTED)

set_target_properties(postproc PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libpostproc.so)

add_library(swresample SHARED IMPORTED)

set_target_properties(swresample PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libswresample.so)

add_library(swscale SHARED IMPORTED)

set_target_properties(swscale PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libswscale.so)

add_library(avfilter SHARED IMPORTED)

set_target_properties(avfilter PROPERTIES IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/ffmpeg/${ANDROID_ABI}/libavfilter.so)

add_library(nanodetncnn SHARED nanodetncnn.cpp nanodet.cpp ndkcamera.cpp rtspclient.cpp rtspcamera.cpp)

target_link_libraries(nanodetncnn
        ncnn
        ${OpenCV_LIBS}
        camera2ndk
        mediandk
        jnigraphics
        vulkan
        avdevice
        avfilter
        avcodec
        avformat
        avutil
        swresample
        swscale
        postproc)
