#pragma once

#include <stdint.h>
#include "yuv.h"

static void uyvy_frame_from_rgb(uint8_t *rgb, uint8_t* uyvy, uint32_t width, uint32_t height) {
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width / 2; x++) {
            uint8_t* uyvy_ = &uyvy[((y * width + x * 2) * 2)];
            const uint8_t* rgb_ = rgb + ((y * width + x * 2) * 3);

            // Downsample
            uint8_t mix_rgb[3];
            mix_rgb[0] = (rgb_[0+0] + rgb_[3+0]) / 2;
            mix_rgb[1] = (rgb_[0+1] + rgb_[3+1]) / 2;
            mix_rgb[2] = (rgb_[0+2] + rgb_[3+2]) / 2;

            // Get U and V
            uint8_t u;
            uint8_t v;
            uv_from_rgb(&u, &v, mix_rgb[0], mix_rgb[1], mix_rgb[2]);

            // Variable for Y
            uint8_t y;

            // Pixel 1
            y_from_rgb(&y, rgb_[0+0], rgb_[0+1], rgb_[0+2]);
            uyvy_[0] = u;
            uyvy_[1] = y;

            // Pixel 2
            y_from_rgb(&y, rgb_[3+0], rgb_[3+1], rgb_[3+2]);
            uyvy_[2] = v;
            uyvy_[3] = y;
        }
    }
}

static uint32_t uyvy_frame_size(uint32_t width, uint32_t height) {
    return height * width * 2;
}