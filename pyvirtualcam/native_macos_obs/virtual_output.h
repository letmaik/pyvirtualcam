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
#include "../native_shared/image_formats.h"

class VirtualOutput {
  private:
    OBSDALMachServer* mach_server = nil;
    uint32_t frame_width;
    uint32_t frame_height;
    uint32_t frame_fmt;
    uint32_t fps_num;
    uint32_t fps_den;
    std::vector<uint8_t> buffer_tmp;
    std::vector<uint8_t> buffer_output;

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc) {
        NSString *dal_plugin_path = @"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin";
        NSFileManager *file_manager = [NSFileManager defaultManager];
        BOOL dal_plugin_installed = [file_manager fileExistsAtPath:dal_plugin_path];
        if (!dal_plugin_installed) {
            throw std::runtime_error(
                "OBS Virtual Camera is not installed in your system. "
                "Use the Virtual Camera function in OBS to trigger installation."
                );
        }

        frame_fmt = libyuv::CanonicalFourCC(fourcc);
        frame_width = width;
        frame_height = height;
        fps_num = fps * 1000;
        fps_den = 1000;

        uint32_t out_frame_size = uyvy_frame_size(width, height);

        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
                // RGB|BGR -> ARGB -> UYVY
                buffer_tmp.resize(argb_frame_size(width, height));
                buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_I420:
                // I420 -> UYVY
                buffer_output.resize(out_frame_size);
                break;
            case libyuv::FOURCC_YUY2:
                // YUYV -> I422 -> UYVY
                buffer_tmp.resize(i422_frame_size(width, height));
                buffer_output.resize(out_frame_size);
                break;
            default:
                throw std::runtime_error(
                    "Unsupported image format, must be RGB, BGR, I420, or YUYV."
                );
        }

        mach_server = [[OBSDALMachServer alloc] init];
        BOOL started = [mach_server run];
        if (!started) {
            throw std::runtime_error("virtual camera output could not be started");
        }
    }

    void stop() {
        if (mach_server == nil) {
            return;
        }

        [mach_server stop];
        [mach_server dealloc];
        mach_server = nil;
        
        // When the named server port is invalidated, the effect is not immediate.
        // Starting a new server immediately afterwards may then fail.
        // There doesn't seem to be an easy way to wait for the invalidation to finish.
        // As a work-around, we sleep for a short period and hope for the best.
        [NSThread sleepForTimeInterval:0.2f];
    }

    void send(const uint8_t* frame) {
        if (mach_server == nil) {
            return;
        }

        // We must handle port messages, and somehow our RunLoop isn't normally active.
        // Handle exactly one message. If no message is queued, return without blocking.
        NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
        NSDate *now = [NSDate date];
        [runLoop runMode:NSDefaultRunLoopMode beforeDate:now];
        
        uint64_t timestamp = mach_absolute_time();

        uint8_t* tmp = buffer_tmp.data();
        uint8_t* uyvy = buffer_output.data();
        
        switch (frame_fmt) {
            case libyuv::FOURCC_RAW:            
                rgb_to_argb(frame, tmp, frame_width, frame_height);
                argb_to_uyvy(tmp, uyvy, frame_width, frame_height);
                break;
            case libyuv::FOURCC_24BG:
                bgr_to_argb(frame, tmp, frame_width, frame_height);
                argb_to_uyvy(tmp, uyvy, frame_width, frame_height);
                break;
            case libyuv::FOURCC_I420:
                i420_to_uyvy(frame, uyvy, frame_width, frame_height);
                break;
            case libyuv::FOURCC_YUY2:
                yuyv_to_i422(frame, tmp, frame_width, frame_height);
                i422_to_uyvy(tmp, uyvy, frame_width, frame_height);
                break;
            default:
                throw std::logic_error("not implemented");
        }

        CGFloat width = frame_width;
        CGFloat height = frame_height;

        [mach_server sendFrameWithSize:NSMakeSize(width, height)
                timestamp:timestamp
            fpsNumerator:fps_num
            fpsDenominator:fps_den
                frameBytes:uyvy];
    }

    std::string device()
    {
        // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/mac-virtualcam/src/dal-plugin/OBSDALDevice.mm#L106
        return "OBS Virtual Camera";
    }
};
