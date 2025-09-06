// (C) 2025 Sebastian Beckmann
// (C) 2021 Jannik Vogel
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

#import <CoreMedia/CoreMedia.h>
#import <CoreMediaIO/CoreMediaIO.h>

#include <stdexcept>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include "../native_shared/image_formats.h"


// This is pulled out of OBS. We can probably assume that if this changes, the camera will be incompatible anyways.
constexpr const char *OBS_DEVICE_UUID = "7626645E-4425-469E-9D8B-97E0FA59AC75";

class VirtualOutput {
  private:
    static std::mutex mutex;

    std::unique_lock<std::mutex> lock;
    CVPixelBufferPoolRef pixelBufferPool;

    CMIOObjectID deviceID{0};
    CMIOStreamID streamID{0};
    CMSimpleQueueRef queue;
    CMFormatDescriptionRef formatDescription;

    uint32_t frameWidth;
    uint32_t frameHeight;
    uint32_t frameFourCC;
    uint32_t frameSize;
    std::vector<uint8_t> bufferTmp;
    std::vector<uint8_t> bufferOutput;

  public:
    VirtualOutput(uint32_t width, uint32_t height, uint32_t fourcc, std::optional<std::string> device_) : lock(mutex, std::try_to_lock) {
        if (device_.has_value() && device_ != device()) {
            throw std::invalid_argument(
                "This backend supports only the '" + device() + "' device."
            );
        }

        if (!lock.owns_lock()) {
            throw std::runtime_error("Multiple cameras are not supported by this backend.");
        }

        frameFourCC = libyuv::CanonicalFourCC(fourcc);
        frameWidth = width;
        frameHeight = height;

        frameSize = uyvy_frame_size(width, height);

        switch (frameFourCC) {
            case libyuv::FOURCC_RAW:
            case libyuv::FOURCC_24BG:
            case libyuv::FOURCC_J400:
                // RGB|BGR|GRAY -> BGRA -> UYVY
                bufferTmp.resize(bgra_frame_size(width, height));
                bufferOutput.resize(frameSize);
                break;
            case libyuv::FOURCC_I420:
                // I420 -> UYVY
                bufferOutput.resize(frameSize);
                break;
            case libyuv::FOURCC_NV12:
                // NV12 -> I420 -> UYVY
                bufferTmp.resize(i420_frame_size(width, height));
                bufferOutput.resize(frameSize);
                break;
            case libyuv::FOURCC_YUY2:
                // YUYV -> I422 -> UYVY
                bufferTmp.resize(i422_frame_size(width, height));
                bufferOutput.resize(frameSize);
                break;
            case libyuv::FOURCC_UYVY:
                break;
            default:
                throw std::runtime_error("Unsupported image format.");
        }

        FourCharCode videoFormat = kCVPixelFormatType_422YpCbCr8; // UYVY

        NSDictionary *pAttr = @{};
        NSDictionary *pbAttr = @{
            (id)kCVPixelBufferPixelFormatTypeKey: @(videoFormat),
            (id)kCVPixelBufferWidthKey: @(frameWidth),
            (id)kCVPixelBufferHeightKey: @(frameHeight),
            (id)kCVPixelBufferIOSurfacePropertiesKey: @{}
        };
        CVReturn status = CVPixelBufferPoolCreate(
            kCFAllocatorDefault, (__bridge CFDictionaryRef)pAttr,
            (__bridge CFDictionaryRef)pbAttr, &pixelBufferPool);
        if (status != kCVReturnSuccess) {
            throw std::runtime_error("unable to allocate pixel buffer pool");
        }

        UInt32 size;
        UInt32 used;

        CMIOObjectPropertyAddress address {.mSelector = kCMIOHardwarePropertyDevices,
                                           .mScope = kCMIOObjectPropertyScopeGlobal,
                                           .mElement = kCMIOObjectPropertyElementMain};
        CMIOObjectGetPropertyDataSize(kCMIOObjectSystemObject, &address, 0, NULL, &size);
        std::vector<CMIOObjectID> devices;
        devices.resize(size / sizeof(CMIOObjectID));
        CMIOObjectGetPropertyData(kCMIOObjectSystemObject, &address, 0, NULL, size, &used, devices.data());

        address.mSelector = kCMIODevicePropertyDeviceUID;
        for (auto device : devices) {
            CMIOObjectGetPropertyDataSize(device, &address, 0, NULL, &size);
            CFStringRef uidCF;
            CMIOObjectGetPropertyData(device, &address, 0, NULL, size, &used, &uidCF);
            const char *uid = CFStringGetCStringPtr(uidCF, kCFStringEncodingUTF8);
            if (strcmp(uid, OBS_DEVICE_UUID) == 0) {
                CFRelease(uidCF);
                deviceID = device;
                break;
            } else {
                CFRelease(uidCF);
            }
        }

        if (deviceID == 0) {
            throw std::runtime_error("OBS Virtual Camera is not installed in your system. Use the Virtual Camera function in OBS 30.0 or later to trigger the installation and activate the System Extension in System Settings. You may need to restart your machine after that. [Device not found]");
        }

        address.mSelector = kCMIODevicePropertyStreams;
        CMIOObjectGetPropertyDataSize(deviceID, &address, 0, NULL, &size);
        std::vector<CMIOStreamID> streams;
        streams.resize(size / sizeof(CMIOStreamID));
        CMIOObjectGetPropertyData(deviceID, &address, 0, NULL, size, &used, streams.data());

        // As far as I can tell, there's no such thing as kCMIOStreamPropertyStreamUID.
        // We'll just have to pray it's always at position 2 (index 1).
        if (streams.size() < 2) {
            throw std::runtime_error("Stream not found");
        }
        streamID = streams[1];


        CMIOStreamCopyBufferQueue(streamID, [](CMIOStreamID, void *, void *){}, NULL, &queue);
        CMVideoFormatDescriptionCreate(kCFAllocatorDefault, videoFormat, frameWidth, frameHeight, NULL, &formatDescription);

        OSStatus result = CMIODeviceStartStream(deviceID, streamID);
        if (result != noErr) {
            throw std::runtime_error("Couldn't start stream");
        }
    }

    void stop() {
        CMIODeviceStopStream(deviceID, streamID);
        CFRelease(formatDescription);
        CVPixelBufferPoolRelease(pixelBufferPool);
    }

    void send(const uint8_t* frame) {
        if (streamID == 0) {
            throw std::runtime_error("Stream does not exist.");
        }

        uint8_t* tmp = bufferTmp.data();
        uint8_t* outFrame;

        switch (frameFourCC) {
            case libyuv::FOURCC_RAW:
                outFrame = bufferOutput.data();
                rgb_to_bgra(frame, tmp, frameWidth, frameHeight);
                bgra_to_uyvy(tmp, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_24BG:
                outFrame = bufferOutput.data();
                bgr_to_bgra(frame, tmp, frameWidth, frameHeight);
                bgra_to_uyvy(tmp, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_J400:
                outFrame = bufferOutput.data();
                gray_to_bgra(frame, tmp, frameWidth, frameHeight);
                bgra_to_uyvy(tmp, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_I420:
                outFrame = bufferOutput.data();
                i420_to_uyvy(frame, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_NV12:
                outFrame = bufferOutput.data();
                nv12_to_i420(frame, tmp, frameWidth, frameHeight);
                i420_to_uyvy(tmp, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_YUY2:
                outFrame = bufferOutput.data();
                yuyv_to_i422(frame, tmp, frameWidth, frameHeight);
                i422_to_uyvy(tmp, outFrame, frameWidth, frameHeight);
                break;
            case libyuv::FOURCC_UYVY:
                outFrame = const_cast<uint8_t*>(frame);
                break;
            default:
                throw std::logic_error("not implemented");
        }

        CVPixelBufferRef frameRef;
        CVReturn status = CVPixelBufferPoolCreatePixelBuffer(
            kCFAllocatorDefault, pixelBufferPool, &frameRef);

        if (status != kCVReturnSuccess) {
            // not an exception, in case it is temporary
            fprintf(stderr, "unable to allocate pixel buffer (error %d)",
                status);
            return;
        }

        CVPixelBufferLockBaseAddress(frameRef, 0);

        uint8_t *dst = (uint8_t *)CVPixelBufferGetBaseAddress(frameRef);
        int dst_size = CVPixelBufferGetDataSize(frameRef);

        if (dst_size == frameSize) {
			memcpy(dst, outFrame, frameSize);
        } else {
            fprintf(stderr, "Pixel buffer size mismatch");
        }

        CVPixelBufferUnlockBaseAddress(frameRef, 0);

        CMSampleBufferRef sampleBuffer;
        CMSampleTimingInfo timingInfo = {
            .presentationTimeStamp = CMTimeMake(clock_gettime_nsec_np(CLOCK_UPTIME_RAW), 1000000000ull),
        };
        CMSampleBufferCreateForImageBuffer(kCFAllocatorDefault, frameRef, true, NULL, NULL, formatDescription, &timingInfo, &sampleBuffer);
        CMSimpleQueueEnqueue(queue, sampleBuffer);

        CVPixelBufferRelease(frameRef);
    }

    std::string device()
    {
        // https://github.com/obsproject/obs-studio/blob/7778070cbd8e4689d91d90068091ced467c5fdef/plugins/mac-virtualcam/src/camera-extension/OBSCameraProviderSource.swift#L22
        return "OBS Virtual Camera";
    }

    uint32_t native_fourcc() {
        return libyuv::FOURCC_UYVY;
    }
};

std::mutex VirtualOutput::mutex;
