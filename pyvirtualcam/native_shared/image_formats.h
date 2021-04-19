#pragma once

#include <cmath>
#include <libyuv.h>

// libyuv names RGBA-type formats after the order in a *register*,
// whereas we name it after the order in *memory*.
// For example, libyuv ARGB is referred to as BGRA in function names below.

// Passing a negative height to the functions below flips the image vertically.

// copy
static void gray_to_bgra(const uint8_t *gray, uint8_t* bgra, int32_t width, int32_t height) {
    libyuv::J400ToARGB(
        gray, width,
        bgra, width * 4,
        width, height);
}

// copy
static void rgb_to_bgra(const uint8_t *rgb, uint8_t* bgra, int32_t width, int32_t height) {
    libyuv::RAWToARGB(
        rgb, width * 3,
        bgra, width * 4,
        width, height);
}

// copy
static void bgra_to_rgba(const uint8_t *bgra, uint8_t* rgba, int32_t width, int32_t height) {
    libyuv::ARGBToABGR(
        bgra, width * 4,
        rgba, width * 4,
        width, height);
}

// copy
static void bgra_to_bgra(const uint8_t *src, uint8_t* dist, int32_t width, int32_t height) {
    libyuv::ARGBCopy(
        src, width * 4,
        dist, width * 4,
        width, height);
}

#define rgba_to_rgba bgra_to_bgra

// horizontal and vertical subsampling and yuv conversion
static void rgb_to_i420(const uint8_t *rgb, uint8_t* i420, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::RAWToI420(
        rgb, width * 3,
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        width, height_);
}

// copy
static void bgr_to_bgra(const uint8_t *bgr, uint8_t* bgra, int32_t width, int32_t height) {
    libyuv::RGB24ToARGB(
        bgr, width * 3,
        bgra, width * 4,
        width, height);
}

// horizontal and vertical subsampling and yuv conversion
static void bgr_to_i420(const uint8_t *bgr, uint8_t* i420, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::RGB24ToI420(
        bgr, width * 3,
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        width, height_);
}

// horizontal and vertical subsampling and yuv conversion
static void bgra_to_nv12(const uint8_t *bgra, uint8_t* nv12, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    libyuv::ARGBToNV12(
        bgra, width * 4,
        nv12, width,
        nv12 + width * height, width,
        width, height_);
}

// horizontal subsampling and yuv conversion
static void bgra_to_uyvy(const uint8_t *bgra, uint8_t* uyvy, int32_t width, int32_t height) {
    libyuv::ARGBToUYVY(
        bgra, width * 4,
        uyvy, width * 2,
        width, height);
}

// copy
static void i420_to_nv12(const uint8_t *i420, uint8_t* nv12, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::I420ToNV12(
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        nv12, width,
        nv12 + width * height, width,
        width, height_);
}

// horizontal and vertical upsampling and yuv conversion
static void i420_to_bgra(const uint8_t *i420, uint8_t* bgra, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::I420ToARGB(
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        bgra, width * 4,
        width, height_);
}

// horizontal and vertical upsampling and yuv conversion
static void i420_to_rgba(const uint8_t *i420, uint8_t* rgba, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::I420ToABGR(
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        rgba, width * 4,
        width, height_);
}

// copy
static void nv12_to_i420(const uint8_t *nv12, uint8_t* i420, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::NV12ToI420(
        nv12, width,
        nv12 + width * height, width,
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        width, height_);
}

// horizontal and vertical upsampling and yuv conversion
static void nv12_to_bgra(const uint8_t *nv12, uint8_t* bgra, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    libyuv::NV12ToARGB(
        nv12, width,
        nv12 + width * height, width,
        bgra, width * 4,
        width, height_);
}

// horizontal and vertical upsampling and yuv conversion
static void nv12_to_rgba(const uint8_t *nv12, uint8_t* rgba, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    libyuv::NV12ToABGR(
        nv12, width,
        nv12 + width * height, width,
        rgba, width * 4,
        width, height_);
}

// vertical upsampling
static void i420_to_uyvy(const uint8_t *i420, uint8_t* uyvy, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::I420ToUYVY(
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        uyvy, width * 2,
        width, height_);
}

// vertical subsampling
static void yuyv_to_nv12(const uint8_t *yuyv, uint8_t* nv12, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    libyuv::YUY2ToNV12(
        yuyv, width * 2,
        nv12, width,
        nv12 + width * height, width,
        width, height_);
}

// vertical subsampling
static void yuyv_to_i420(const uint8_t *yuyv, uint8_t* i420, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;
    int32_t half_height = height / 2;

    libyuv::YUY2ToI420(
        yuyv, width * 2,
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        width, height_);
}

// copy
static void yuyv_to_i422(const uint8_t *yuyv, uint8_t* i422, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;

    libyuv::YUY2ToI422(
        yuyv, width * 2,
        i422, width,
        i422 + width * height, half_width,
        i422 + width * height + half_width * height, half_width,
        width, height_);
}

// horizontal upsampling and yuv conversion
static void yuyv_to_bgra(const uint8_t *yuyv, uint8_t* bgra, int32_t width, int32_t height) {
    libyuv::YUY2ToARGB(
        yuyv, width * 2,
        bgra, width * 4,
        width, height);
}

// vertical subsampling
static void uyvy_to_nv12(const uint8_t *uyvy, uint8_t* nv12, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    libyuv::UYVYToNV12(
        uyvy, width * 2,
        nv12, width,
        nv12 + width * height, width,
        width, height_);
}

// copy
static void i422_to_uyvy(const uint8_t *i422, uint8_t* uyvy, int32_t width, int32_t height) {
    int32_t height_ = height;
    height = std::abs(height);
    int32_t half_width = width / 2;

    libyuv::I422ToUYVY(
        i422, width,
        i422 + width * height, half_width,
        i422 + width * height + half_width * height, half_width,
        uyvy, width * 2,
        width, height_);
}

// horizontal upsampling and yuv conversion
static void uyvy_to_bgra(const uint8_t *uyvy, uint8_t* bgra, int32_t width, int32_t height) {
    libyuv::UYVYToARGB(
        uyvy, width * 2,
        bgra, width * 4,
        width, height);
}

static int32_t bgra_frame_size(int32_t width, int32_t height) {
    return width * height * 4;
}

static int32_t gray_frame_size(int32_t width, int32_t height) {
    return width * height;
}

static int32_t i420_frame_size(int32_t width, int32_t height) {
    return width * height * 3 / 2;
}

static int32_t i422_frame_size(int32_t width, int32_t height) {
    return width * height * 2;
}

#define rgba_frame_size bgra_frame_size
#define nv12_frame_size i420_frame_size
#define uyvy_frame_size i422_frame_size
#define yuyv_frame_size i422_frame_size