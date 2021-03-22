#pragma once

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include <string>
#include <vector>
#include <set>
#include <stdexcept>

#include "../native_shared/image_formats.h"

// v4l2loopback allows opening a device multiple times.
// To avoid selecting the same device more than once,
// we keep track of the ones we have open ourselves.
// Obviously, this won't help if multiple processes are used
// or if devices are opened by other tools.
// In this case, explicitly specifying the device seems the only solution.
static std::set<std::string> ACTIVE_DEVICES;

class VirtualOutput {
  private:
    bool _output_running = false;
    int _camera_fd;
    std::string _camera_device;
    uint32_t _frame_fourcc;
    uint32_t _native_fourcc;
    uint32_t _frame_width;
    uint32_t _frame_height;
    uint32_t _out_frame_size;
    std::vector<uint8_t> _buffer_output;

  public:
    VirtualOutput(uint32_t width, uint32_t height, uint32_t fourcc,
                  std::optional<std::string> device_) {
        _frame_width = width;
        _frame_height = height;
        _frame_fourcc = libyuv::CanonicalFourCC(fourcc);
        
        uint32_t out_frame_fmt_v4l;

        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
                // RGB|BGR -> I420
                _out_frame_size = i420_frame_size(width, height);
                _buffer_output.resize(_out_frame_size);
                _native_fourcc = libyuv::FOURCC_I420;
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUV420;
                break;
            case libyuv::FOURCC_J400:
                _out_frame_size = gray_frame_size(width, height);
                _native_fourcc = _frame_fourcc;
                out_frame_fmt_v4l = V4L2_PIX_FMT_GREY;
                break;
            case libyuv::FOURCC_I420:
                _out_frame_size = i420_frame_size(width, height);
                _native_fourcc = _frame_fourcc;
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUV420;
                break;
            case libyuv::FOURCC_NV12:
                _out_frame_size = nv12_frame_size(width, height);
                _native_fourcc = _frame_fourcc;
                out_frame_fmt_v4l = V4L2_PIX_FMT_NV12;
                break;
            case libyuv::FOURCC_YUY2:
                _out_frame_size = yuyv_frame_size(width, height);
                _native_fourcc = _frame_fourcc;
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUYV;
                break;
            case libyuv::FOURCC_UYVY:
                _out_frame_size = uyvy_frame_size(width, height);
                _native_fourcc = _frame_fourcc;
                out_frame_fmt_v4l = V4L2_PIX_FMT_UYVY;
                break;
            default:
                throw std::runtime_error("Unsupported image format.");
        }

        auto try_open = [&](const std::string& device_name) {
            if (ACTIVE_DEVICES.count(device_name)) {
                throw std::invalid_argument(
                    "Device " + device_name + " is already in use."
                );
            }
            _camera_fd = open(device_name.c_str(), O_WRONLY | O_SYNC);
            if (_camera_fd == -1) {
                if (errno == EACCES) {
                    throw std::runtime_error(
                        "Could not access " + device_name + " due to missing permissions. "
                        "Did you add your user to the 'video' group? "
                        "Run 'usermod -a -G video myusername' and log out and in again."
                    );
                } else if (errno == ENOENT) {
                    throw std::invalid_argument(
                        "Device " + device_name + " does not exist."
                    );
                } else {
                    throw std::invalid_argument(
                        "Device " + device_name + " could not be opened: " + 
                        std::string(strerror(errno))
                    );
                }
            }

            struct v4l2_capability camera_cap;

            if (ioctl(_camera_fd, VIDIOC_QUERYCAP, &camera_cap) == -1) {
                throw std::invalid_argument(
                    "Device capabilities of " + device_name + " could not be queried."
                );
            }
            if (!(camera_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
                throw std::invalid_argument(
                    "Device " + device_name + " is not a video output device."
                );
            }
            if (strcmp((const char*)camera_cap.driver, "v4l2 loopback") != 0) {
                throw std::invalid_argument(
                    "Device " + device_name + " is not a V4L2 device."
                );
            }
        };

        std::string device_name;

        if (device_.has_value()) {
            device_name = device_.value();
            try_open(device_name);
        } else {
            bool found = false;
            for (size_t i = 0; i < 100; i++) {
                std::ostringstream device_name_s;
                device_name_s << "/dev/video" << i;
                device_name = device_name_s.str();
                try {
                    try_open(device_name);
                } catch (std::invalid_argument&) {
                    continue;
                }
                found = true;
                break;
            }
            if (!found) {
                throw std::runtime_error(
                    "No v4l2 loopback device found at /dev/video[0-99]. "
                    "Did you run 'modprobe v4l2loopback'? "
                    "See also pyvirtualcam's documentation.");
            }
        }

        v4l2_format v4l2_fmt;
        memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        v4l2_pix_format& pix = v4l2_fmt.fmt.pix;
        pix.width = width;
        pix.height = height;
        pix.pixelformat = out_frame_fmt_v4l;

        // v4l2loopback sets bytesperline, sizeimage, and colorspace for us.

        if (ioctl(_camera_fd, VIDIOC_S_FMT, &v4l2_fmt) == -1) {
            close(_camera_fd);
            throw std::runtime_error(
                "Virtual camera device " + device_name + 
                " could not be configured: " + std::string(strerror(errno))
            );
        }
        
        _output_running = true;
        _camera_device = device_name;

        ACTIVE_DEVICES.insert(_camera_device);
    }

    void stop() {
        if (!_output_running) {
            return;
        }

        close(_camera_fd);
        
        _output_running = false;
        ACTIVE_DEVICES.erase(_camera_device);
    }

    void send(const uint8_t* frame) {
        if (!_output_running)
            return;

        uint8_t* out_frame;

        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:
                out_frame = _buffer_output.data();
                rgb_to_i420(frame, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_24BG:
                out_frame = _buffer_output.data();
                bgr_to_i420(frame, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_J400:
            case libyuv::FOURCC_I420:
            case libyuv::FOURCC_NV12:
            case libyuv::FOURCC_YUY2:
            case libyuv::FOURCC_UYVY:
                out_frame = const_cast<uint8_t*>(frame);
                break;
            default:
                throw std::logic_error("not implemented");
        }

        ssize_t n = write(_camera_fd, out_frame, _out_frame_size);
        if (n == -1) {
            // not an exception, in case it is temporary
            fprintf(stderr, "error writing frame: %s", strerror(errno));
        }
    }

    std::string device() {
        return _camera_device;
    }

    uint32_t native_fourcc() {
        return _native_fourcc;
    }
};
