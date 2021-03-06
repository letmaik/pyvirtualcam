#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include "controller.h"

static int camera_fd = 0;
static bool output_running = false;
static uint32_t frame_width = 0;
static uint32_t frame_height = 0;

static void YfromRGB(uint8_t* y, uint8_t r, uint8_t g, uint8_t b) {
    *y = (uint8_t)( 0.257 * r + 0.504 * g + 0.098 * b +  16);
}
static void UVfromRGB(uint8_t* u, uint8_t* v, uint8_t r, uint8_t g, uint8_t b) {
    *u = (uint8_t)(-0.148 * r - 0.291 * g + 0.439 * b + 128);
    *v = (uint8_t)( 0.439 * r - 0.368 * g - 0.071 * b + 128);
}

bool virtual_output_start(uint32_t width, uint32_t height, double fps)
{
    if (output_running) {
        fprintf(stderr, "virtual camera output already started\n");
        return false;
    }

    char device_name[13];
    bool found = false;

    for (size_t i = 0; i < 100; i++) {
        sprintf(device_name, "/dev/video%d", i);
        camera_fd = open(device_name, O_WRONLY | O_SYNC);
        if (camera_fd == -1) {
            if (errno == EACCES) {
                fprintf(stderr, "Could not access %s due to missing permissions.\n", device_name);
                fprintf(stderr, "Did you add your user to the 'video' group?\n");
                fprintf(stderr, "Run 'usermod -a -G video myusername' and log out and in again.\n");
                return false;
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
        found = true;
        break;
    }
    if (!found) {
        fprintf(stderr, "no v4l2 loopback device found at /dev/video[0-99]\n");
        return false;
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

    if (ioctl(camera_fd, VIDIOC_S_FMT, &camera_format) == -1) {
        close(camera_fd);
        fprintf(stderr, "virtual camera device %s could not be configured: %s\n", device_name, strerror(errno));
        return false;
    }

    fprintf(stdout, "virtual camera device: %s\n", device_name);

    frame_width = width;
    frame_height = height;
    output_running = true;

    return true;
}

void virtual_output_stop()
{
    if (!output_running) {
        return;
    }

    close(camera_fd);
    
    output_running = false;
}

// data is in RGB format (packed RGB, 24bpp, RGBRGB...)
// queue expects UYVY (packed YUV, 12bpp)
void virtual_video(uint8_t *buf)
{
    if (!output_running)
        return;

    size_t data_size = frame_height * frame_width * 2;
    uint8_t* data = (uint8_t*) malloc(data_size);
    if (!data) {
        fprintf(stderr, "out of memory\n");
        return;
    }

    // Convert RGB to UYVY
    for(uint32_t y = 0; y < frame_height; y++) {
        for(uint32_t x = 0; x < frame_width / 2; x++) {
            uint8_t* uyvy = &data[((y * frame_width + x * 2) * 2)];
            const uint8_t* rgb = buf + ((y * frame_width + x * 2) * 3);

            // Downsample
            uint8_t mixRGB[3];
            mixRGB[0] = (rgb[0+0] + rgb[3+0]) / 2;
            mixRGB[1] = (rgb[0+1] + rgb[3+1]) / 2;
            mixRGB[2] = (rgb[0+2] + rgb[3+2]) / 2;

            // Get U and V
            uint8_t u;
            uint8_t v;
            UVfromRGB(&u, &v, mixRGB[0], mixRGB[1], mixRGB[2]);

            // Variable for Y
            uint8_t y;

            // Pixel 1
            YfromRGB(&y, rgb[0+0], rgb[0+1], rgb[0+2]);
            uyvy[0] = u;
            uyvy[1] = y;

            // Pixel 2
            YfromRGB(&y, rgb[3+0], rgb[3+1], rgb[3+2]);
            uyvy[2] = v;
            uyvy[3] = y;
        }
    }

    write(camera_fd, data, data_size);

    free(data);
}

bool virtual_output_is_running()
{
    return output_running;
}
