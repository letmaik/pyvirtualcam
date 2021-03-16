#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "queue/shared-memory-queue.h"
#include "../native_shared/image_formats.h"

class VirtualOutput {
  private:
    bool output_running = false;
    video_queue_t *vq;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_fmt;
    std::vector<uint8_t> buffer_tmp;
    std::vector<uint8_t> buffer_output;
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
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc) {
        // https://github.com/obsproject/obs-studio/blob/9da6fc67/.github/workflows/main.yml#L484
        LPCWSTR guid = L"CLSID\\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}";
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, guid, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            throw std::runtime_error(
                "OBS Virtual Camera device not found! "
                "Did you install OBS?"
            );
        }

        frame_fmt = libyuv::CanonicalFourCC(fourcc);
        frame_width = width;
        frame_height = height;

        uint32_t out_frame_size = nv12_frame_size(width, height);

        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
                // RGB|BGR -> I420 -> NV12
                buffer_tmp.resize(i420_frame_size(width, height));
                buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_J400:
                // GRAY -> ARGB -> NV12
                buffer_tmp.resize(argb_frame_size(width, height));
                buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_I420:
            case libyuv::FOURCC_YUY2:
                // I420|YUYV -> NV12
                buffer_output.resize(out_frame_size);
                break;
            default:
                throw std::runtime_error(
                    "Unsupported image format, must RGB, BGR, or I420."
                );
        }
        
        uint64_t interval = (uint64_t)(10000000.0 / fps);

        vq = video_queue_create(width, height, interval);

        if (!vq) {
            throw std::runtime_error("virtual camera output could not be started");
        }

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

    void send(const uint8_t *frame)
    {
        if (!output_running)
            return;

        uint8_t* tmp = buffer_tmp.data();
        uint8_t* nv12 = buffer_output.data();
        
        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:
                rgb_to_i420(frame, tmp, frame_width, frame_height);
                i420_to_nv12(tmp, nv12, frame_width, frame_height);
                break;
            case libyuv::FOURCC_24BG:
                bgr_to_i420(frame, tmp, frame_width, frame_height);
                i420_to_nv12(tmp, nv12, frame_width, frame_height);
                break;
            case libyuv::FOURCC_J400:
                gray_to_argb(frame, tmp, frame_width, frame_height);
                argb_to_nv12(tmp, nv12, frame_width, frame_height);
                break;
            case libyuv::FOURCC_I420:
                i420_to_nv12(frame, nv12, frame_width, frame_height);
                break;
            case libyuv::FOURCC_YUY2:
                yuyv_to_nv12(frame, nv12, frame_width, frame_height);
                break;
            default:
                throw std::logic_error("not implemented");
        }

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
