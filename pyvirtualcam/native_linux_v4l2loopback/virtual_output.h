#pragma once

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

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
static std::set<size_t> active_devices;

class VirtualOutput {
  private:
    bool output_running = false;
    int camera_fd;
    std::string camera_device;
    size_t camera_device_idx;
    uint32_t frame_fmt;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t out_frame_size;
    std::vector<uint8_t> buffer_output;

  public:
    VirtualOutput(uint32_t width, uint32_t height, uint32_t fourcc) {
        frame_width = width;
        frame_height = height;
        frame_fmt = libyuv::CanonicalFourCC(fourcc);
        out_frame_size = i420_frame_size(width, height);

        uint32_t out_frame_fmt_v4l;

        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
                // RGB|BGR -> I420
                buffer_output.resize(out_frame_size);
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUV420;
                break;
            case libyuv::FOURCC_J400:
                out_frame_fmt_v4l = V4L2_PIX_FMT_GREY;
                break;
            case libyuv::FOURCC_I420:
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUV420;
                break;
            case libyuv::FOURCC_YUY2:
                out_frame_fmt_v4l = V4L2_PIX_FMT_YUYV;
                break;
            default:
                throw std::runtime_error(
                    "Unsupported image format, must be RGB, BGR, GRAY, I420, or YUYV."
                );
        }

        char device_name[14];
        int device_idx = -1;

        for (size_t i = 0; i < 100; i++) {
            if (active_devices.count(i)) {
                continue;
            }
            sprintf(device_name, "/dev/video%zu", i);
            camera_fd = open(device_name, O_WRONLY | O_SYNC);
            if (camera_fd == -1) {
                if (errno == EACCES) {
                    throw std::runtime_error(
                        "Could not access " + std::string(device_name) + " due to missing permissions. "
                        "Did you add your user to the 'video' group? "
                        "Run 'usermod -a -G video myusername' and log out and in again."
                    );
                }
                continue;
            }

            struct v4l2_capability camera_cap;

            if (ioctl(camera_fd, VIDIOC_QUERYCAP, &camera_cap) == -1) {
                continue;
            }
            if (!(camera_cap.capabilities & V4L2_CAP_VIDEO_OUTPUT)) {
                continue;
            }
            if (strcmp((const char*)camera_cap.driver, "v4l2 loopback") != 0) {
                continue;
            }
            device_idx = i;
            break;
        }
        if (device_idx == -1) {
            throw std::runtime_error(
                "No v4l2 loopback device found at /dev/video[0-99]. "
                "Did you run 'modprobe v4l2loopback'? "
                "See also pyvirtualcam's documentation.");
        }

        uint32_t half_width = width / 2;
        uint32_t half_height = height / 2;

        v4l2_format v4l2_fmt;
        memset(&v4l2_fmt, 0, sizeof(v4l2_fmt));
        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        v4l2_pix_format& pix = v4l2_fmt.fmt.pix;
        pix.width = width;
        pix.height = height;
        pix.pixelformat = out_frame_fmt_v4l;

        // v4l2loopback sets bytesperline, sizeimage, and colorspace for us.

        if (ioctl(camera_fd, VIDIOC_S_FMT, &v4l2_fmt) == -1) {
            close(camera_fd);
            throw std::runtime_error(
                "Virtual camera device " + std::string(device_name) + 
                " could not be configured: " + std::string(strerror(errno))
            );
        }
        
        output_running = true;
        camera_device = std::string(device_name);
        camera_device_idx = device_idx;

        active_devices.insert(device_idx);
    }

    void stop() {
        if (!output_running) {
            return;
        }

        close(camera_fd);
        
        output_running = false;
        active_devices.erase(camera_device_idx);
    }

    void send(const uint8_t* frame) {
        if (!output_running)
            return;

        uint8_t* out_frame;

        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:
                out_frame = buffer_output.data();
                rgb_to_i420(frame, out_frame, frame_width, frame_height);
                break;
            case libyuv::FOURCC_24BG:
                out_frame = buffer_output.data();
                bgr_to_i420(frame, out_frame, frame_width, frame_height);
                break;
            case libyuv::FOURCC_J400:
            case libyuv::FOURCC_I420:
            case libyuv::FOURCC_YUY2:
                out_frame = const_cast<uint8_t*>(frame);
                break;
            default:
                throw std::logic_error("not implemented");
        }

        ssize_t n = write(camera_fd, out_frame, out_frame_size);
        if (n == -1) {
            // not an exception, in case it is temporary
            fprintf(stderr, "error writing frame: %s", strerror(errno));
        }
    }

    std::string device() {
        return camera_device;
    }
};
