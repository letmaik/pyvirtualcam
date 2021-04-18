#pragma once

#include <stdio.h>
#include <Windows.h>
#include <vector>
#include "../native_shared/image_formats.h"
#include "shared.inl"

#ifdef _WIN64
#define GUID_OFFSET 0x10
#else
#define GUID_OFFSET 0x20
#endif

int parse_int(char const *s) {
    if (s == NULL || *s == '\0') return -1;
    int result = 0;
    while (*s) {
        if (*s < '0' || *s > '9') return -1;
        result = result * 10 + *s - '0';
        s++;
    }
    return result;
}

bool get_name(int num, std::wstring str) {
    wchar_t guid[45];
    swprintf(guid, 45, L"CLSID\\{5C2CD55C-92AD-4999-8666-912BD3E700%02X}", GUID_OFFSET + num + !!num); // 1 is reserved by the library
    str.resize(0);
    str.resize(256);
    DWORD size;
    if (RegGetValueW(HKEY_CLASSES_ROOT, guid, L"", RRF_RT_REG_SZ, NULL, &str, &size) != ERROR_SUCCESS) return false;
    str.resize(size / sizeof(wchar_t));
    return true;
}

class VirtualOutput {
  private:
    uint32_t _width;
    uint32_t _height;
    uint32_t _fourcc;
    std::wstring _device;
    int _size;
    std::vector<uint8_t> _tmp;
    std::vector<uint8_t> _out;
    SharedImageMemory* _sender;
    bool _running = false;

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc, std::optional<std::wstring> device) {
        int i;
        if (device.has_value()) {
            std::wstring name = *device;
            if ((i = parse_int(name.c_str())) != -1 && i < 74) { // maximum number defined in the library
                if (!get_name(i, _device)) throw std::runtime_error("No camera registered with this index.");
            } else if (name != NULL && name.size()) {
                for (i = 0; i < 74; i++) if (get_name(i, _device) && _device == name) break;
                if (i == 74) throw std::runtime_error("No camera registered with this name.");
            }
        } else {
            for (i = 0; i < 74; i++) if (get_name(i, _device)) break;
            if (i == 74) throw std::runtime_error("No camera registered. Did you install any camera?");
        }
        _sender = new SharedImageMemory(i);
        _width = width;
        _height = height;
        _fourcc = libyuv::CanonicalFourCC(fourcc);
        _size = argb_frame_size(width, height);
        _tmp.resize(_size);
        _out.resize(_size);
        if (!_sender->SendIsReady()) throw std::runtime_error("The camera cannot be properly initialized.");
        _running = true;
    }

    void stop() {
        if (!_running) return;
        delete _sender; // invoke destructor
        _running = false;
    }

    void send(const uint8_t *frame) {
        if (!_running) return;
        switch (_fourcc) {
            case libyuv::FOURCC_RAW:
                tmp = _tmp.data();
                rgb_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_24BG:
                tmp = _tmp.data();
                bgr_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_J400:
                tmp = _tmp.data();
                gray_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_I420:
                tmp = _tmp.data();
                i420_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_NV12:
                tmp = _tmp.data();
                nv12_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_YUY2:
                tmp = _tmp.data();
                yuyv_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_UYVY:
                tmp = _tmp.data();
                uyvy_to_argb(frame, tmp, _width, _height);
                break;
            case libyuv::FOURCC_ARGB:
                tmp = const_cast<uint8_t*>(frame);
                break;
            default:
                throw std::logic_error("This format is currently not supported.");
        }
        uint8_t* out = _out.data();
        argb_to_argb(tmp, out, _width, -_height); // vertical flip
        _sender->Send(_width, _height, _size, out);
    }

    std::wstring device() {
        return _device;
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_ARGB;
    }
};
