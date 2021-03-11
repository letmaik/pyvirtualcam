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

#pragma once

#include <stdexcept>
#include <cstdint>
#include <string>
#include <vector>
#include <mach/mach_time.h>
#include "server/OBSDALMachServer.h"
#include "../native_shared/uyvy.h"

static OBSDALMachServer *sMachServer = nil;

static uint32_t cam_output_width;
static uint32_t cam_output_height;
static uint32_t cam_fps_num;
static uint32_t cam_fps_den;

static std::vector<uint8_t> buffer;

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
    buffer.resize(uyvy_frame_size(width, height));

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

void virtual_output_send(uint8_t* rgb) {
    if (sMachServer == nil) {
        return;
    }

    // We must handle port messages, and somehow our RunLoop isn't normally active.
    // Handle exactly one message. If no message is queued, return without blocking.
    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
    NSDate *now = [NSDate date];
    [runLoop runMode:NSDefaultRunLoopMode beforeDate:now];
    
    uint64_t timestamp = mach_absolute_time();

    uint8_t* uyvy = buffer.data();

    uyvy_frame_from_rgb(rgb, uyvy, cam_output_width, cam_output_height);

    CGFloat width = cam_output_width;
    CGFloat height = cam_output_height;

    [sMachServer sendFrameWithSize:NSMakeSize(width, height)
             timestamp:timestamp
          fpsNumerator:cam_fps_num
        fpsDenominator:cam_fps_den
            frameBytes:uyvy];
}

const char* virtual_output_device()
{
    // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/mac-virtualcam/src/dal-plugin/OBSDALDevice.mm#L106
    return "OBS Virtual Camera";
}
