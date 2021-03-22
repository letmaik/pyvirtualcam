#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "queue/shared-memory-queue.h"
#include "../native_shared/image_formats.h"

class VirtualOutput {
  private:
    bool _output_running = false;
    video_queue_t *_vq;
    uint32_t _frame_width;
    uint32_t _frame_height;
    uint32_t _frame_fourcc;
    std::vector<uint8_t> _buffer_tmp;
    std::vector<uint8_t> _buffer_output;
    bool _have_clockfreq = false;
    LARGE_INTEGER _clock_freq;

    uint64_t get_timestamp_ns()
    {
        if (!_have_clockfreq) {
            QueryPerformanceFrequency(&_clock_freq);
            _have_clockfreq = true;
        }

        LARGE_INTEGER current_time;
        QueryPerformanceCounter(&current_time);
        double time_val = (double)current_time.QuadPart;
        time_val *= 1000000000.0;
        time_val /= (double)_clock_freq.QuadPart;

        return static_cast<uint64_t>(time_val);
    }

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc,
                  std::optional<std::string> device_) {
        // https://github.com/obsproject/obs-studio/blob/9da6fc67/.github/workflows/main.yml#L484
        LPCWSTR guid = L"CLSID\\{A3FCE0F5-3493-419F-958A-ABA1250EC20B}";
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CLASSES_ROOT, guid, 0, KEY_READ, &key) != ERROR_SUCCESS) {
            throw std::runtime_error(
                "OBS Virtual Camera device not found! "
                "Did you install OBS?"
            );
        }

        if (device_.has_value() && device_ != device()) {
            throw std::invalid_argument(
                "This backend supports only the '" + device() + "' device."
            );
        }

        _frame_fourcc = libyuv::CanonicalFourCC(fourcc);
        _frame_width = width;
        _frame_height = height;

        uint32_t out_frame_size = nv12_frame_size(width, height);

        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
                // RGB|BGR -> I420 -> NV12
                _buffer_tmp.resize(i420_frame_size(width, height));
                _buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_J400:
                // GRAY -> ARGB -> NV12
                _buffer_tmp.resize(argb_frame_size(width, height));
                _buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_I420:
            case libyuv::FOURCC_YUY2:
            case libyuv::FOURCC_UYVY:
                // I420|YUYV|UYVY -> NV12
                _buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_NV12:
                break;
            default:
                throw std::runtime_error(
                    "Unsupported image format."
                );
        }
        
        uint64_t interval = (uint64_t)(10000000.0 / fps);

        _vq = video_queue_create(width, height, interval);

        if (!_vq) {
            throw std::runtime_error("virtual camera output could not be started");
        }

        _output_running = true;
    }

    void stop()
    {
        if (!_output_running) {
            return;
        }	
        video_queue_close(_vq);
        _output_running = false;
    }

    void send(const uint8_t *frame)
    {
        if (!_output_running)
            return;

        uint8_t* tmp = _buffer_tmp.data();
        uint8_t* out_frame;
        
        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:
                out_frame = _buffer_output.data();
                rgb_to_i420(frame, tmp, _frame_width, _frame_height);
                i420_to_nv12(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_24BG:
                out_frame = _buffer_output.data();
                bgr_to_i420(frame, tmp, _frame_width, _frame_height);
                i420_to_nv12(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_J400:
                out_frame = _buffer_output.data();
                gray_to_argb(frame, tmp, _frame_width, _frame_height);
                argb_to_nv12(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_I420:
                out_frame = _buffer_output.data();
                i420_to_nv12(frame, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_NV12:
                out_frame = const_cast<uint8_t*>(frame);
                break;
            case libyuv::FOURCC_YUY2:
                out_frame = _buffer_output.data();
                yuyv_to_nv12(frame, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_UYVY:
                out_frame = _buffer_output.data();
                uyvy_to_nv12(frame, out_frame, _frame_width, _frame_height);
                break;
            default:
                throw std::logic_error("not implemented");
        }

        // NV12 has two planes
        uint8_t* y = out_frame;
        uint8_t* uv = out_frame + _frame_width * _frame_height;

        // One entry per plane
        uint32_t linesize[2] = { _frame_width, _frame_width / 2 };
        uint8_t* data[2] = { y, uv };

        uint64_t timestamp = get_timestamp_ns();

        video_queue_write(_vq, data, linesize, timestamp);
    }

    std::string device()
    {
        // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/win-dshow/virtualcam-module/virtualcam-module.cpp#L196
        return "OBS Virtual Camera";
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_NV12;
    }
};
