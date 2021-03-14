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
    uint32_t frame_width;
    uint32_t frame_height;
    std::vector<uint8_t> buffer_output;

  public:
    VirtualOutput(uint32_t width, uint32_t height) {
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
        // v4l2loopback requires V4L2_BUF_TYPE_VIDEO_OUTPUT even
        // for multi-plane formats. Seems non-standard.
        v4l2_fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        v4l2_pix_format& pix = v4l2_fmt.fmt.pix;
        pix.width = width;
        pix.height = height;
        pix.pixelformat = V4L2_PIX_FMT_YUV420;
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
        frame_width = width;
        frame_height = height;
        buffer_output.resize(i420_frame_size(width, height));

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

    void send(const uint8_t *rgb) {
        if (!output_running)
            return;

        uint8_t* i420 = buffer_output.data();

        rgb_to_i420(rgb, i420, frame_width, frame_height);

        ssize_t n = write(camera_fd, i420, buffer_output.size());
        if (n == -1) {
            // not an exception, in case it is temporary
            fprintf(stderr, "error writing frame: %s", strerror(errno));
        }
    }

    std::string device() {
        return camera_device;
    }
};
