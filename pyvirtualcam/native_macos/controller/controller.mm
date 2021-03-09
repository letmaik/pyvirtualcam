// (C)2021 Jannik Vogel
//
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
//  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
//  DEALINGS IN THE SOFTWARE.

#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>
#include <mach/mach_time.h>
#include "server/OBSDALMachServer.h"
#include "server/Defines.h"
#include "controller/controller.h"

static OBSDALMachServer *sMachServer = nil;

static uint32_t cam_output_width;
static uint32_t cam_output_height;
static uint32_t cam_fps_num;
static uint32_t cam_fps_den;


static void YfromRGB(uint8_t* y, uint8_t r, uint8_t g, uint8_t b) {
    *y = (uint8_t)( 0.257 * r + 0.504 * g + 0.098 * b +  16);
}
static void UVfromRGB(uint8_t* u, uint8_t* v, uint8_t r, uint8_t g, uint8_t b) {
    *u = (uint8_t)(-0.148 * r - 0.291 * g + 0.439 * b + 128);
    *v = (uint8_t)( 0.439 * r - 0.368 * g - 0.071 * b + 128);
}

void virtual_output_start(uint32_t width, uint32_t height, double fps) {
    if (sMachServer != nil) {
        throw std::logic_error("virtual camera output already started");
    }

    NSString *dal_plugin_path = @"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin";
    NSFileManager *file_manager = [NSFileManager defaultManager];
    BOOL dal_plugin_installed = [file_manager fileExistsAtPath:dal_plugin_path];
    if (!dal_plugin_installed) {
        throw std::runtime_error(
            "OBS Virtual Camera is not installed in your system. "
            "Use the Virtual Camera function in OBS to trigger installation."
            );
    }

    cam_output_width = width;
    cam_output_height = height;
    cam_fps_num = fps * 1000;
    cam_fps_den = 1000;

    blog(LOG_DEBUG, "output_create");
    sMachServer = [[OBSDALMachServer alloc] init];

    [sMachServer run];
}

void virtual_output_stop() {
    if (sMachServer == nil) {
        return;
    }

    [sMachServer stop];

    sMachServer = nil;
}

void virtual_output_send(uint8_t* buf) {
    if (sMachServer == nil) {
        return;
    }

    uint32_t frame_width = cam_output_width;
    uint32_t frame_height = cam_output_height;

    // We must handle port messages, and somehow our RunLoop isn't normally active.
    // Handle exactly one message. If no message is queued, return without blocking.
    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
    NSDate *now = [NSDate date];
    [runLoop runMode:NSDefaultRunLoopMode beforeDate:now];
    
    uint64_t timestamp = mach_absolute_time();

    std::vector<uint8_t> out_frame(frame_width * frame_height * 2);
    uint8_t* data = out_frame.data();

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

    CGFloat width = cam_output_width;
    CGFloat height = cam_output_height;

    [sMachServer sendFrameWithSize:NSMakeSize(width, height)
             timestamp:timestamp
          fpsNumerator:cam_fps_num
        fpsDenominator:cam_fps_den
            frameBytes:data];
}

std::string virtual_output_device()
{
    // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/mac-virtualcam/src/dal-plugin/OBSDALDevice.mm#L106
    return "OBS Virtual Camera";
}
