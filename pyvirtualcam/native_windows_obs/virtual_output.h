#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "queue/shared-memory-queue.h"
#include "../native_shared/nv12.h"

static bool output_running = false;
static video_queue_t *vq;
static uint32_t cam_output_width;
static uint32_t cam_output_height;
static std::vector<uint8_t> buffer;

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

const char* virtual_output_device()
{
    // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/win-dshow/virtualcam-module/virtualcam-module.cpp#L196
    return "OBS Virtual Camera";
}

void virtual_output_start(uint32_t width, uint32_t height, double fps)
{
    if (output_running) {
        throw std::logic_error("virtual camera output already started");
    }

    // https://github.com/obsproject/obs-studio/blob/9da6fc67/.github/workflows/main.yml#L484
    LPCWSTR guid = L"CLSID\\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}";
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CLASSES_ROOT, guid, 0, KEY_READ, &key) != ERROR_SUCCESS) {
        throw std::runtime_error(
            "OBS Virtual Camera device not found! "
            "Did you install OBS?"
        );
	}

    bool start = false;
    uint64_t interval = (uint64_t)(10000000.0 / fps);

    vq = video_queue_create(width, height, interval);

    if (!vq) {
        throw std::logic_error("virtual camera output could not be started");
    }

    cam_output_width = width;
    cam_output_height = height;
    buffer.resize((width + width / 2) * height); // NV12
    output_running = true;
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
void virtual_output_send(uint8_t *rgb)
{
    if (!output_running)
        return;

    uint8_t* nv12 = buffer.data();

    nv12_frame_from_rgb(rgb, nv12, cam_output_width, cam_output_height);

    // NV12 has two planes
    uint8_t* y = nv12;
    uint8_t* uv = nv12 + cam_output_width * cam_output_height;

    // One entry per plane
    uint32_t linesize[2] = { cam_output_width, cam_output_width / 2 };
    uint8_t* data[2] = { y, uv };

    uint64_t timestamp = get_timestamp_ns();

    video_queue_write(vq, data, linesize, timestamp);
}
