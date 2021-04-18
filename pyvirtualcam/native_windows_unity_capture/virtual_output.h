#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "../native_shared/image_formats.h"
#include "shared_memory/shared.inl"

#ifdef _WIN64
#define GUID_OFFSET 0x10
#else
#define GUID_OFFSET 0x20
#endif

static constexpr int MAX_CAPNUM = SharedImageMemory::MAX_CAPNUM;

static void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch) && !std::iscntrl(ch);
    }).base(), s.end());
}

static bool get_name(int num, std::string& str) {
    constexpr size_t key_size = 45;
    char key[key_size];
    snprintf(key, key_size, "CLSID\\{5C2CD55C-92AD-4999-8666-912BD3E700%02X}", GUID_OFFSET + num + !!num); // 1 is reserved by the library
    DWORD size; // includes terminating null character(s)
    if (RegGetValueA(HKEY_CLASSES_ROOT, key, NULL, RRF_RT_REG_SZ, NULL, NULL, &size) != ERROR_SUCCESS)
        return false;
    str.resize(size - 1);
    if (RegGetValueA(HKEY_CLASSES_ROOT, key, NULL, RRF_RT_REG_SZ, NULL, str.data(), &size) != ERROR_SUCCESS)
        return false;
    rtrim(str);
    return true;
}

class VirtualOutput {
  private:
    int32_t _width;
    int32_t _height;
    uint32_t _fourcc;
    std::string _device;
    int _size;
    std::vector<uint8_t> _tmp;
    std::vector<uint8_t> _out;
    std::unique_ptr<SharedImageMemory> _sender;
    bool _running = false;

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc, std::optional<std::string> device) {
        int i;
        if (device.has_value()) {
            std::string name = *device;
            for (i = 0; i < MAX_CAPNUM; i++) {
                if (get_name(i, _device) && _device == name)
                    break;
            }
            if (i == MAX_CAPNUM) {
                throw std::runtime_error("No camera registered with this name.");
            }
        } else {
            for (i = 0; i < MAX_CAPNUM; i++) {
                if (get_name(i, _device))
                    break;
            }
            if (i == MAX_CAPNUM) {
                throw std::runtime_error("No camera registered. Did you install any camera?");
            }
        }
        _sender = std::make_unique<SharedImageMemory>(i);
        _width = width;
        _height = height;
        _fourcc = libyuv::CanonicalFourCC(fourcc);
        _size = bgra_frame_size(width, height);
        if (_fourcc != libyuv::FOURCC_ABGR) {
            _tmp.resize(_size);
        }
        _out.resize(_size);

        _running = true;
    }

    void stop() {
        if (!_running)
            return;
        _sender = nullptr;
        _running = false;
    }

    void send(const uint8_t *frame) {
        if (!_running)
            return;
        if (!_sender->SendIsReady()) {
            // happens when no app is capturing the camera yet
            return;
        }

        uint8_t* tmp = _tmp.data();
        uint8_t* out = _out.data();

        // vertical flip
        int32_t height_invert = -static_cast<int32_t>(_height);

        switch (_fourcc) {
            case libyuv::FOURCC_RAW:
                rgb_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_24BG:
                bgr_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_J400:
                gray_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_I420:
                i420_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_NV12:
                nv12_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_YUY2:
                yuyv_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_UYVY:
                uyvy_to_bgra(frame, tmp, _width, _height);
                bgra_to_rgba(tmp, out, _width, height_invert);
                break;
            case libyuv::FOURCC_ABGR:
                rgba_to_rgba(frame, out, _width, height_invert);
                break;
            default:
                throw std::logic_error("This format is currently not supported.");
        }
        
        _sender->Send(_width, _height, _size, out);
    }

    std::string device() {
        return _device;
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_ABGR;
    }
};
