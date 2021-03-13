#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "queue/shared-memory-queue.h"
#include "../native_shared/nv12.h"
#include "../native_shared/libyuv_wrap.h"

class VirtualOutput {
  private:
    bool output_running = false;
    video_queue_t *vq;
    uint32_t frame_width;
    uint32_t frame_height;
    std::vector<uint8_t> buffer;
    std::vector<uint8_t> buffer_argb;
    bool have_clockfreq = false;
    LARGE_INTEGER clock_freq;

    uint64_t get_timestamp_ns()
    {
        if (!have_clockfreq) {
            QueryPerformanceFrequency(&clock_freq);
            have_clockfreq = true;
        }

        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        double time_val = (double)current_time.QuadPart;
        time_val *= 1000000000.0;
        time_val /= (double)clock_freq.QuadPart;

        return static_cast<uint64_t>(time_val);
    }

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps) {
        // https://github.com/obsproject/obs-studio/blob/9da6fc67/.github/workflows/main.yml#L484
        LPCWSTR guid = L"CLSID\\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}";
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, guid, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            throw std::runtime_error(
                "OBS Virtual Camera device not found! "
                "Did you install OBS?"
            );
        }

        uint64_t interval = (uint64_t)(10000000.0 / fps);

        vq = video_queue_create(width, height, interval);

        if (!vq) {
            throw std::runtime_error("virtual camera output could not be started");
        }

        frame_width = width;
        frame_height = height;
        buffer.resize(nv12_frame_size(width, height));
        buffer_argb.resize(width * height * 4);
        output_running = true;
    }

    void stop()
    {
        if (!output_running) {
            return;
        }	
        video_queue_close(vq);
        output_running = false;
    }

    // data is in RGB format (packed RGB, 24bpp, RGBRGB...)
    // queue expects NV12 (semi-planar YUV, 12bpp)
    void send(const uint8_t *rgb)
    {
        if (!output_running)
            return;

        uint8_t* nv12 = buffer.data();

        uint8_t* argb = buffer_argb.data();
        rgb_to_argb(rgb, argb, frame_width, frame_height);
        argb_to_nv12(argb, nv12, frame_width, frame_height);

        //nv12_frame_from_rgb(rgb, nv12, frame_width, frame_height);

        // NV12 has two planes
        uint8_t* y = nv12;
        uint8_t* uv = nv12 + frame_width * frame_height;

        // One entry per plane
        uint32_t linesize[2] = { frame_width, frame_width / 2 };
        uint8_t* data[2] = { y, uv };

        uint64_t timestamp = get_timestamp_ns();

        video_queue_write(vq, data, linesize, timestamp);
    }

    std::string device()
    {
        // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/win-dshow/virtualcam-module/virtualcam-module.cpp#L196
        return "OBS Virtual Camera";
    }
};
