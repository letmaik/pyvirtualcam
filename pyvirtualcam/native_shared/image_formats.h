#pragma once

#include <libyuv.h>

static void rgb_to_argb(const uint8_t *rgb, uint8_t* argb, uint32_t width, uint32_t height) {
    libyuv::RAWToARGB(
        rgb, width * 3,
        argb, width * 4,
        width, height);
}

static void rgb_to_i420(const uint8_t *rgb, uint8_t* i420, uint32_t width, uint32_t height) {
    uint32_t half_width = width / 2;
    uint32_t half_height = height / 2;

    libyuv::RAWToI420(
        rgb, width * 3,
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        width, height);
}

static void i420_to_nv12(const uint8_t *i420, uint8_t* nv12, uint32_t width, uint32_t height) {
    uint32_t half_width = width / 2;
    uint32_t half_height = height / 2;

    libyuv::I420ToNV12(
        i420, width,
        i420 + width * height, half_width,
        i420 + width * height + half_width * half_height, half_width,
        nv12, width,
        nv12 + width * height, width,
        width, height);
}

static void argb_to_nv12(const uint8_t *argb, uint8_t* nv12, uint32_t width, uint32_t height) {
    libyuv::ARGBToNV12(
        argb, width * 4,
        nv12, width,
        nv12 + width * height, width,
        width, height);
}

static void argb_to_uyvy(const uint8_t *argb, uint8_t* uyvy, uint32_t width, uint32_t height) {
    libyuv::ARGBToUYVY(
        argb, width * 4,
        uyvy, width * 2,
        width, height);
}

static uint32_t uyvy_frame_size(uint32_t width, uint32_t height) {
    return height * width * 2;
}

static uint32_t i420_frame_size(uint32_t width, uint32_t height) {
    return (width + width / 2) * height;
}

#define nv12_frame_size i420_frame_size
