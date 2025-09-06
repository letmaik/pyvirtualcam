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
    OBSDALMachServer* _mach_server = nil;
    mach_timebase_info_data_t _timebase_info;
    CVPixelBufferPoolRef _cv_pool;
    FourCharCode _cv_format;
    uint32_t _frame_width;
    uint32_t _frame_height;
    uint32_t _frame_fourcc;
    uint32_t _out_frame_size;
    uint32_t _fps_num;
    uint32_t _fps_den;
    std::vector<uint8_t> _buffer_tmp;
    std::vector<uint8_t> _buffer_output;

    // https://stackoverflow.com/a/23378064
    uint64_t scale_mach_time(uint64_t i) {
        uint32_t numer = _timebase_info.numer;
        uint32_t denom = _timebase_info.denom;
        uint64_t high = (i >> 32) * numer;
        uint64_t low = (i & 0xffffffffull) * numer / denom;
        uint64_t highRem = ((high % denom) << 32) / denom;
        high /= denom;
        return (high << 32) + highRem + low;
    }

  public:
    VirtualOutput(uint32_t width, uint32_t height, double fps, uint32_t fourcc,
                  std::optional<std::string> device_) {
        NSString *dal_plugin_path = @"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin";
        NSFileManager *file_manager = [NSFileManager defaultManager];
        BOOL dal_plugin_installed = [file_manager fileExistsAtPath:dal_plugin_path];
        if (!dal_plugin_installed) {
            throw std::runtime_error(
                "OBS Virtual Camera is not installed in your system. "
                "Use the Virtual Camera function in OBS to trigger installation."
                );
        }
        NSDictionary *dal_plugin_info_plist = [NSDictionary
            dictionaryWithContentsOfURL:
                [NSURL fileURLWithPath:
                        @"/Library/CoreMediaIO/Plug-Ins/DAL/obs-mac-virtualcam.plugin/Contents/Info.plist"]];
        NSString *dal_plugin_version = [dal_plugin_info_plist
            valueForKey:@"CFBundleShortVersionString"];
        if ([dal_plugin_version hasPrefix:@"26."] || [dal_plugin_version hasPrefix:@"27."]) {
            throw std::runtime_error(
                "Your OBS Virtual Camera version is not supported. "
                "Upgrade to OBS 28 or higher. "
                "After upgrading, use the Virtual Camera function "
                "once in OBS to trigger installation of the virtual camera."
                );
        }

        if (device_.has_value() && device_ != device()) {
            throw std::invalid_argument(
                "This backend supports only the '" + device() + "' device."
            );
        }

        _frame_fourcc = libyuv::CanonicalFourCC(fourcc);
        _frame_width = width;
        _frame_height = height;
        _fps_num = fps * 1000;
        _fps_den = 1000;

        _out_frame_size = uyvy_frame_size(width, height);

        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
            case libyuv::FOURCC_J400:
                // RGB|BGR|GRAY -> BGRA -> UYVY
                _buffer_tmp.resize(bgra_frame_size(width, height));
                _buffer_output.resize(_out_frame_size);
                break;
            case libyuv::FOURCC_I420:
                // I420 -> UYVY
                _buffer_output.resize(_out_frame_size);
                break;
            case libyuv::FOURCC_NV12:
                // NV12 -> I420 -> UYVY
                _buffer_tmp.resize(i420_frame_size(width, height));
                _buffer_output.resize(_out_frame_size);
                break;
            case libyuv::FOURCC_YUY2:
                // YUYV -> I422 -> UYVY
                _buffer_tmp.resize(i422_frame_size(width, height));
                _buffer_output.resize(_out_frame_size);
                break;
            case libyuv::FOURCC_UYVY:
                break;
            default:
                throw std::runtime_error("Unsupported image format.");
        }

        _cv_format = kCVPixelFormatType_422YpCbCr8; // UYVY

        NSDictionary *pAttr = @{};
        NSDictionary *pbAttr = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(_cv_format),
            (id)kCVPixelBufferWidthKey: @(_frame_width),
            (id)kCVPixelBufferHeightKey: @(_frame_height),
            (id)kCVPixelBufferIOSurfacePropertiesKey: @{}
        };
        CVReturn status = CVPixelBufferPoolCreate(
            kCFAllocatorDefault, (__bridge CFDictionaryRef)pAttr,
            (__bridge CFDictionaryRef)pbAttr, &_cv_pool);
        if (status != kCVReturnSuccess) {
            throw std::runtime_error("unable to allocate pixel buffer pool");
        }

        _mach_server = [[OBSDALMachServer alloc] init];
        BOOL started = [_mach_server run];
        if (!started) {
            throw std::runtime_error("virtual camera output could not be started");
        }

        kern_return_t mti_status = mach_timebase_info(&_timebase_info);
        assert(mti_status == KERN_SUCCESS);
    }

    void stop() {
        if (_mach_server == nil) {
            return;
        }

        [_mach_server stop];
        [_mach_server dealloc];
        _mach_server = nil;

        CVPixelBufferPoolRelease(_cv_pool);
        
        // When the named server port is invalidated, the effect is not immediate.
        // Starting a new server immediately afterwards may then fail.
        // There doesn't seem to be an easy way to wait for the invalidation to finish.
        // As a work-around, we sleep for a short period and hope for the best.
        [NSThread sleepForTimeInterval:0.2f];
    }

    void send(const uint8_t* frame) {
        if (_mach_server == nil) {
            return;
        }

        // We must handle port messages, and somehow our RunLoop isn't normally active.
        // Handle exactly one message. If no message is queued, return without blocking.
        NSRunLoop *run_loop = [NSRunLoop currentRunLoop];
        NSDate *now = [NSDate date];
        [run_loop runMode:NSDefaultRunLoopMode beforeDate:now];
        
        uint64_t timestamp = scale_mach_time(mach_absolute_time());

        uint8_t* tmp = _buffer_tmp.data();
        uint8_t* out_frame;
        
        switch (_frame_fourcc) {
            case libyuv::FOURCC_RAW:            
                out_frame = _buffer_output.data();
                rgb_to_bgra(frame, tmp, _frame_width, _frame_height);
                bgra_to_uyvy(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_24BG:
                out_frame = _buffer_output.data();
                bgr_to_bgra(frame, tmp, _frame_width, _frame_height);
                bgra_to_uyvy(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_J400:
                out_frame = _buffer_output.data();
                gray_to_bgra(frame, tmp, _frame_width, _frame_height);
                bgra_to_uyvy(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_I420:
                out_frame = _buffer_output.data();
                i420_to_uyvy(frame, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_NV12:
                out_frame = _buffer_output.data();
                nv12_to_i420(frame, tmp, _frame_width, _frame_height);
                i420_to_uyvy(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_YUY2:
                out_frame = _buffer_output.data();
                yuyv_to_i422(frame, tmp, _frame_width, _frame_height);
                i422_to_uyvy(tmp, out_frame, _frame_width, _frame_height);
                break;
            case libyuv::FOURCC_UYVY:
                out_frame = const_cast<uint8_t*>(frame);
                break;
            default:
                throw std::logic_error("not implemented");
        }
        
        CVPixelBufferRef frame_ref = nil;
        CVReturn status = CVPixelBufferPoolCreatePixelBuffer(
            kCFAllocatorDefault, _cv_pool, &frame_ref);

        if (status != kCVReturnSuccess) {
            // not an exception, in case it is temporary
            fprintf(stderr, "unable to allocate pixel buffer (error %d)",
                status);
            return;
        }

        CVPixelBufferLockBaseAddress(frame_ref, 0);

        uint8_t *dst = (uint8_t *)CVPixelBufferGetBaseAddress(frame_ref);
        int dst_size = CVPixelBufferGetDataSize(frame_ref);

        if (dst_size == _out_frame_size) {
			memcpy(dst, out_frame, _out_frame_size);
        } else {
            fprintf(stderr, "pixel buffer size mismatch");
        }

        CVPixelBufferUnlockBaseAddress(frame_ref, 0);

        [_mach_server sendPixelBuffer:frame_ref
            timestamp:timestamp
            fpsNumerator:_fps_num
            fpsDenominator:_fps_den];
        
        CVPixelBufferRelease(frame_ref);
    }

    std::string device()
    {
        // https://github.com/obsproject/obs-studio/blob/eb98505a2/plugins/mac-virtualcam/src/dal-plugin/OBSDALDevice.mm#L106
        return "OBS Virtual Camera";
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_UYVY;
    }
};
