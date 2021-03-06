#include <stdio.h>
#include <Windows.h>
#include "queue/shared-memory-queue.h"
#include "controller.h"

static video_queue_t *vq;
static bool output_running = false;

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;

static uint64_t get_timestamp_ns()
{
    LARGE_INTEGER current_time;
    double time_val;

    if (!have_clockfreq) {
        QueryPerformanceFrequency(&clock_freq);
        have_clockfreq = true;
    }

    QueryPerformanceCounter(&current_time);
    time_val = (double)current_time.QuadPart;
    time_val *= 1000000000.0;
    time_val /= (double)clock_freq.QuadPart;

    return (uint64_t)time_val;
}

static void YfromRGB(uint8_t* y, uint8_t r, uint8_t g, uint8_t b) {
    *y = (uint8_t)( 0.257 * r + 0.504 * g + 0.098 * b +  16);
}
static void UVfromRGB(uint8_t* u, uint8_t* v, uint8_t r, uint8_t g, uint8_t b) {
    *u = (uint8_t)(-0.148 * r - 0.291 * g + 0.439 * b + 128);
    *v = (uint8_t)( 0.439 * r - 0.368 * g - 0.071 * b + 128);
}

bool virtual_output_start(uint32_t width, uint32_t height, double fps)
{
    if (output_running) {
        fprintf(stderr, "virtual camera output already started\n");
        return false;
    }

    bool start = false;
    uint64_t interval = (uint64_t)(10000000.0 / fps);

    vq = video_queue_create(width, height, interval);

    if (vq) {
        output_running = true;
    } else {
        output_running = false;
        fprintf(stderr, "video_queue_create() failed\n");
        return false;
    }

    return true;
}

void virtual_output_stop()
{
    if (!output_running) {
        return;
    }	
    video_queue_close(vq);
    output_running = false;
}

// data is in RGB format (packed RGB, 24bpp, RGBRGB...)
// queue expects NV12 (semi-planar YUV, 12bpp)
void virtual_video(uint8_t *rgb)
{
    if (!output_running)
        return;

    uint32_t cx, cy;
    uint64_t interval;
    video_queue_get_info(vq, &cx, &cy, &interval);

    uint8_t* nv12 = (uint8_t*)malloc((cx + cx / 2) * cy );
    if (!nv12) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    // NV12 has two planes
    uint8_t* y = nv12;
    uint8_t* uv = nv12 + cx * cy;

    // Compute Y plane
    for (uint32_t i=0; i < cx * cy; i++) {
        YfromRGB(&y[i], rgb[i * 3 + 0], rgb[i * 3 + 1], rgb[i * 3 + 2]);
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
            UVfromRGB(u, v, r, g, b);
        }
    }

    // One entry per plane
    uint32_t linesize[2] = { cx, cx / 2 };
    uint8_t* data[2] = { y, uv };

    uint64_t timestamp = get_timestamp_ns();

    video_queue_write(vq, data, linesize, timestamp);
    free(nv12);
}

bool virtual_output_is_running()
{
    return output_running;
}
