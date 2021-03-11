#pragma once

#include <stdint.h>
#include "yuv.h"

static void nv12_frame_from_rgb(uint8_t *rgb, uint8_t* nv12, uint32_t width, uint32_t height) {
    const uint32_t cx = width;
    const uint32_t cy = height;
    
    // NV12 has two planes
    uint8_t* y = nv12;
    uint8_t* uv = nv12 + cx * cy;

    // Compute Y plane
    for (uint32_t i=0; i < cx * cy; i++) {
        y_from_rgb(&y[i], rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }
    // Compute UV plane
    for (uint32_t y=0; y < cy; y = y + 2) {
        for (uint32_t x=0; x < cx; x = x + 2) {
            // Downsample by 2
            uint8_t* rgb1 = rgb + ((y * cx + x) * 3);
            uint8_t* rgb2 = rgb + (((y + 1) * cx + x) * 3);
            uint8_t r = (rgb1[0+0] + rgb1[3+0] + rgb2[0+0] + rgb2[3+0]) / 4;
            uint8_t g = (rgb1[0+1] + rgb1[3+1] + rgb2[0+1] + rgb2[3+1]) / 4;
            uint8_t b = (rgb1[0+2] + rgb1[3+2] + rgb2[0+2] + rgb2[3+2]) / 4;

            uint8_t* u = &uv[y / 2 * cx + x];
            uint8_t* v = u + 1;
            uv_from_rgb(u, v, r, g, b);
        }
    }
}

static uint32_t nv12_frame_size(uint32_t width, uint32_t height) {
    return (width + width / 2) * height;
}
