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

#include "../native_shared/yuv.h"
#include "controller.h"

// v4l2loopback allows opening a device multiple times.
// To avoid selecting the same device more than once,
// we keep track of the ones we have open ourselves.
// Obviously, this won't help if multiple processes are used
// or if devices are opened by other tools.
// In this case, explicitly specifying the device seems the only solution.
static std::set<size_t> active_devices;

void virtual_output_start(Context& ctx, uint32_t width, uint32_t height)
{
    if (ctx.output_running) {
        throw std::logic_error("virtual camera output already started");
    }

    char device_name[14];
    size_t device_idx = -1;

    for (size_t i = 0; i < 100; i++) {
        if (active_devices.count(i)) {
            continue;
        }
        sprintf(device_name, "/dev/video%zu", i);
        ctx.camera_fd = open(device_name, O_WRONLY | O_SYNC);
        if (ctx.camera_fd == -1) {
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

        if (ioctl(ctx.camera_fd, VIDIOC_QUERYCAP, &camera_cap) == -1) {
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

    struct v4l2_pix_format pix_fmt = {
        .width = width,
        .height = height,
        .pixelformat = V4L2_PIX_FMT_UYVY,
        .field = V4L2_FIELD_NONE,
        .bytesperline = width * 2,
        .sizeimage = width * height * 2,
        .colorspace = V4L2_COLORSPACE_JPEG        
    };

    struct v4l2_format camera_format = {
        .type = V4L2_BUF_TYPE_VIDEO_OUTPUT,
        .fmt = { .pix = pix_fmt }
    };

    if (ioctl(ctx.camera_fd, VIDIOC_S_FMT, &camera_format) == -1) {
        close(ctx.camera_fd);
        throw std::runtime_error(
            "Virtual camera device " + std::string(device_name) + 
            " could not be configured: " + std::string(strerror(errno))
        );
    }

    ctx.output_running = true;
    ctx.camera_device = std::string(device_name);
    ctx.camera_device_idx = device_idx;
    ctx.frame_width = width;
    ctx.frame_height = height;
    ctx.buffer.resize(ctx.frame_height * ctx.frame_width * 2); // UYVY

    active_devices.insert(device_idx);
}

std::string virtual_output_device(Context& ctx)
{
    return ctx.camera_device;
}

void virtual_output_stop(Context& ctx)
{
    if (!ctx.output_running) {
        return;
    }

    close(ctx.camera_fd);
    
    ctx.output_running = false;
    active_devices.erase(ctx.camera_device_idx);
}

// data is in RGB format (packed RGB, 24bpp, RGBRGB...)
// queue expects UYVY (packed YUV, 12bpp)
void virtual_output_send(Context& ctx, uint8_t *rgb)
{
    if (!ctx.output_running)
        return;

    uint8_t* uyvy = ctx.buffer.data();

    uyvy_frame_from_rgb(rgb, uyvy, ctx.frame_width, ctx.frame_height);

    ssize_t n = write(ctx.camera_fd, uyvy, ctx.buffer.size());
    if (n == -1) {
        // not an exception, in case it is temporary
        fprintf(stderr, "error writing frame: %s", strerror(errno));
    }
}
