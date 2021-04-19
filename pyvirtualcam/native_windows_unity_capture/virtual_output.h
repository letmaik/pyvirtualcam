#pragma once

#include <stdio.h>
#define NOMINMAX
#include <Windows.h>
#include <vector>
#include <limits>
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
    // https://github.com/schellingb/UnityCapture/blob/fe461e8f/Source/UnityCaptureFilter.cpp#L39
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

// Unity Capture does not have an exclusive access / locking mechanism.
// To avoid selecting the same device more than once,
// we keep track of the ones we use ourselves.
// Obviously, this won't help if multiple processes are used
// or if devices are used by other tools.
// In this case, explicitly specifying the device seems the only solution.
static std::set<std::string> ACTIVE_DEVICES;

class VirtualOutput {
  private:
    uint32_t _width;
    uint32_t _height;
    uint32_t _fourcc;
    std::string _device;
    std::vector<uint8_t> _tmp;
    std::vector<uint8_t> _out;
    std::unique_ptr<SharedImageMemory> _shm;
    bool _running = false;

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc, std::optional<std::string> device) {
        int i;
        if (device.has_value()) {
            std::string name = *device;
            if (ACTIVE_DEVICES.count(name)) {
                throw std::invalid_argument(
                    "Device " + name + " is already in use."
                );
            }
            for (i = 0; i < MAX_CAPNUM; i++) {
                if (get_name(i, _device) && _device == name)
                    break;
            }
            if (i == MAX_CAPNUM) {
                throw std::runtime_error("No camera registered with this name.");
            }
        } else {
            bool found_one = false;
            for (i = 0; i < MAX_CAPNUM; i++) {
                if (get_name(i, _device)) {
                    found_one = true;
                    if (!ACTIVE_DEVICES.count(_device))
                        break;
                }
            }
            if (i == MAX_CAPNUM) {
                if (found_one) {
                    throw std::runtime_error("All cameras are already in use.");
                } else {
                    throw std::runtime_error("No camera registered. Did you install any camera?");
                }
            }
        }
        _shm = std::make_unique<SharedImageMemory>(i);
        _width = width;
        _height = height;
        _fourcc = libyuv::CanonicalFourCC(fourcc);
        _out.resize(rgba_frame_size(width, height));
        switch(_fourcc) {
            case libyuv::FOURCC_ABGR:
            case libyuv::FOURCC_I420:
            case libyuv::FOURCC_NV12:
                // RGBA|I420|NV12 -> RGBA
                // Note: RGBA -> RGBA is needed for vertical flipping.
                break;
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
            case libyuv::FOURCC_J400:
            case libyuv::FOURCC_YUY2:
            case libyuv::FOURCC_UYVY:
                // RGB|BGR|GRAY|YUYV|UYVY -> BGRA -> RGBA
                _tmp.resize(bgra_frame_size(width, height));
                break;
            default:
                throw std::runtime_error(
                    "Unsupported image format."
                );
        }
        ACTIVE_DEVICES.insert(_device);
        _running = true;
    }

    void stop() {
        if (!_running)
            return;
        _shm = nullptr;
        _running = false;
        ACTIVE_DEVICES.erase(_device);
    }

    void send(const uint8_t *frame) {
        if (!_running)
            return;
        if (!_shm->SendIsReady()) {
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
                i420_to_rgba(frame, out, _width, height_invert);
                break;
            case libyuv::FOURCC_NV12:
                nv12_to_rgba(frame, out, _width, height_invert);
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
                throw std::logic_error("not implemented");
        }
        
        int stride = _width;
        auto format = SharedImageMemory::FORMAT_UINT8;
        // Note: RESIZEMODE_LINEAR means nearest neighbor scaling.
        auto resize_mode = SharedImageMemory::RESIZEMODE_LINEAR;
        auto mirror_mode = SharedImageMemory::MIRRORMODE_DISABLED;
        // Keep showing last received frame after stopping while receiving app is still capturing.
        constexpr int timeout = std::numeric_limits<int>::max() - SharedImageMemory::RECEIVE_MAX_WAIT;
        _shm->Send(_width, _height, stride, _out.size(), format, resize_mode, mirror_mode, timeout, out);
    }

    std::string device() {
        return _device;
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_ABGR;
    }
};
