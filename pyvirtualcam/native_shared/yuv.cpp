#include "yuv.h"

static void y_from_rgb(uint8_t* y, uint8_t r, uint8_t g, uint8_t b) {
    *y = (uint8_t)( 0.257 * r + 0.504 * g + 0.098 * b +  16);
}
static void uv_from_rgb(uint8_t* u, uint8_t* v, uint8_t r, uint8_t g, uint8_t b) {
    *u = (uint8_t)(-0.148 * r - 0.291 * g + 0.439 * b + 128);
    *v = (uint8_t)( 0.439 * r - 0.368 * g - 0.071 * b + 128);
}

void nv12_frame_from_rgb(uint8_t *rgb, uint8_t* nv12, uint32_t width, uint32_t height) {
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

void uyvy_frame_from_rgb(uint8_t *rgb, uint8_t* uyvy, uint32_t width, uint32_t height) {
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
