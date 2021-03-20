// CommandCam - A command line image grabber
// Copyright (C) 2012-2013 Ted Burke
// Copyright (C) 2016 Matjaz Rihtar
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program (see the file "COPYING").
// If not, see <http://www.gnu.org/licenses/>.
//
// Website: https://batchloaf.wordpress.com/commandcam/
//          https://github.com/mrihtar/CommandCam
//
// To compile using the MSVC++ compiler:
//   cl CommandCam.cpp ole32.lib strmiids.lib oleaut32.lib
// To compile using the MinGW compiler:
//   g++ CommandCam.cpp -lole32 -lstrmiids -loleaut32 -static -o CommandCam.exe
//
// Version 2.6, last modified 16-Nov-2016
//
#ifdef __MINGW32__
#define STRSAFE_NO_DEPRECATE // MinGW: ignore strcpy/sprintf warnings
#pragma GCC diagnostic ignored "-Wnarrowing"
#define strcasecmp strcmpi
#else
#pragma warning (disable:4995) // W3: name was marked as #pragma deprecated
#pragma warning (disable:4996) // W3: deprecated declaration (_CRT_SECURE_NO_WARNINGS)
#pragma warning (disable:4711) // W1: function selected for inline expansion
#pragma warning (disable:4710) // W4: function not inlined
#pragma warning (disable:4127) // W4: conditional expression is constant
#pragma warning (disable:4820) // W4: bytes padding added
#pragma warning (disable:4668) // W4: symbol not defined as a macro
#pragma warning (disable:4365) // W4: conversion -> signed/unsigned mismatch
#pragma warning (disable:4242) // W4: conversion -> possible loss of data
#pragma warning (disable:4244) // W3,W4: conversion -> possible loss of data
#pragma warning (disable:4917) // W1: GUID can only be associated with a class
#pragma warning (disable:4309) // W2: truncation of constant value
#define strcasecmp _stricmp
#endif

#include <stdexcept>
#include <vector>

#include <math.h>

#include <libyuv.h>

// DirectShow header file
#include <dshow.h>

// Missing from amvideo.h
#ifndef SIZE_PREHEADER
#define SIZE_PREHEADER (FIELD_OFFSET(VIDEOINFOHEADER,bmiHeader))
#endif
#ifndef HEADER
#define HEADER(pVideoInfo) (&(((VIDEOINFOHEADER *)(pVideoInfo))->bmiHeader))
#endif

// Some common Webcam compression formats
#define sRGB 0x00000000 // RGB888, 24 bits (no compression)

#define AYUV 0x56555941 // 4:4:4, 32 bits (no downsampling of chroma channels)

#define YUY2 0x32595559 // 4:2:2, 16 bits (2:1 hor. downsamp.)
#define YUYV 0x56595559 // 4:2:2, 16 bits (2:1 hor. downsamp., same as YUY2)
#define YVYU 0x55595659 // 4:2:2, 16 bits (2:1 hor. downsamp.)
#define UYVY 0x59565955 // 4:2:2, 16 bits (2:1 hor. downsamp., reversed YUY2)

#define IMC1 0x31434D49 // 4:2:0, 16 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar)
#define IMC3 0x33434D49 // 4:2:0, 16 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar, reversed IMC1)

#define IMC2 0x32434D49 // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar)
#define IMC4 0x34434D49 // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar)
#define YV12 0x32315659 // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar)
#define NV12 0x3231564E // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar)
#define I420 0x30323449 // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar, reversed YV12)
#define IYUV 0x56555949 // 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar, same as I420)

#define MJPG 0x47504A4D // 24 bits

#ifdef __MINGW32__
#include <qedit.h>
#else
// This is a workaround for the missing header
// file qedit.h which seems to be absent from the
// Windows SDK versions 7.0 and 7.1.
// To use the items defined in this dll, the
// DexterLib namespace must be specified.
// The items in question are:
//
//   DexterLib::_AMMediaType
//   DexterLib::ISampleGrabber
//   DexterLib::IID_ISampleGrabber
//
#import "qedit.dll" raw_interfaces_only named_guids
#endif

// For some reason, these are not included in the
// DirectShow headers. However, they are exported
// by strmiids.lib, so I'm just declaring them
// here as extern.
EXTERN_C const CLSID CLSID_NullRenderer;
EXTERN_C const CLSID CLSID_SampleGrabber;

namespace commandcam {

// DirectShow objects
HRESULT hr;
ICreateDevEnum *pDevEnum = NULL;
IEnumMoniker *pEnum = NULL;
IMoniker *pMoniker = NULL;
IPropertyBag *pPropBag = NULL;
IGraphBuilder *pGraph = NULL;
ICaptureGraphBuilder2 *pBuilder = NULL;
IBaseFilter *pCap = NULL;
IEnumPins *pEnumPins = NULL;
IAMStreamConfig *pConfig = NULL;
IBaseFilter *pSampleGrabberFilter = NULL;
#ifdef __MINGW32__
ISampleGrabber *pSampleGrabber = NULL;
#else
DexterLib::ISampleGrabber *pSampleGrabber = NULL;
#endif
IBaseFilter *pNullRenderer = NULL;
IMediaControl *pMediaControl = NULL;
char *imgBuffer = NULL;
char *rgbBuffer = NULL;

// YUV color conversions
// - Recommendation ITU-R BT.601 (CCIR 601, older, SDTV)
// - Recommendation ITU-R BT.709 (newer, HDTV)
int colors = 601; // Default: BT.601 color conversion


// Print error message and exit program with specified error code
void exit_message(const char* error_message, int error)
{
  // Print an error message
  //fprintf(stderr, error_message);

  // Clean up DirectShow / COM stuff
  if (rgbBuffer != NULL) delete[] rgbBuffer;
  if (imgBuffer != NULL) delete[] imgBuffer;
  if (pMediaControl != NULL) pMediaControl->Release();
  if (pNullRenderer != NULL) pNullRenderer->Release();
  if (pSampleGrabber != NULL) pSampleGrabber->Release();
  if (pSampleGrabberFilter != NULL)
      pSampleGrabberFilter->Release();
  if (pConfig != NULL) pConfig->Release();
  if (pEnumPins != NULL) pEnumPins->Release();
  if (pCap != NULL) pCap->Release();
  if (pBuilder != NULL) pBuilder->Release();
  if (pGraph != NULL) pGraph->Release();
  if (pPropBag != NULL) pPropBag->Release();
  if (pMoniker != NULL) pMoniker->Release();
  if (pEnum != NULL) pEnum->Release();
  if (pDevEnum != NULL) pDevEnum->Release();
  CoUninitialize();

  // Exit the program
  //exit(error);
  if (error != 0) {
    throw std::runtime_error(error_message);
  }
} // exit_message 


// Round half up if positive, round half down if negative (= round to nearest)
// (round is missing from Microsoft C)
int xround(double d)
{
  int i;

  if (d > 0) i = (int)floor(d + 0.50000000000001);
  else i = (int)ceil(d - 0.50000000000001);
  return i;
} // xround


// Clip color value to [0, 255]
char clip(int value)
{
  if (value < 0) return 0;
  else if (value > 255) return 255;
  else return value;
} // clip


// Decode YUY2 frame buffer to RGB
// YUY2 = 4:2:2, 16 bits (2:1 hor. downsamp.)
void yuy2_rgb(long width, long height, long *rgbBufferSize)
{
  int y1, u, y2, v;
  int c, d, e;

  *rgbBufferSize = width * height * 3;

  // Allocate buffer for RGB image
  rgbBuffer = new char[*rgbBufferSize];
  if (!rgbBuffer)
    exit_message("Could not allocate data buffer for RGB image\n", 44);

  char *imgP = imgBuffer;
  // Windows bitmap rows are upside down!
  char *rgbP = rgbBuffer + *rgbBufferSize;

  for (int ii = 0; ii < height; ii++) { // y
    rgbP -= width * 3;
    for (int jj = 0; jj < width/2; jj++) { // x
      y1 = imgP[0] & 0xFF;
      u  = imgP[1] & 0xFF;
      y2 = imgP[2] & 0xFF;
      v  = imgP[3] & 0xFF;
      imgP += 4;

      c = y1 - 16; // luma
      d = u - 128; // Cb
      e = v - 128; // Cr
      if (colors == 601) { // BT.601
        rgbP[2] = clip(xround(1.16438356 * c                  + 1.59602679 * e)); // red
        rgbP[1] = clip(xround(1.16438356 * c - 0.39176229 * d - 0.81296765 * e)); // green
        rgbP[0] = clip(xround(1.16438356 * c + 2.01723214 * d                 )); // blue
      }
      else { // BT.709
        rgbP[2] = clip(xround(1.16438356 * c                  + 1.79274107 * e)); // red
        rgbP[1] = clip(xround(1.16438356 * c - 0.21324861 * d - 0.53290933 * e)); // green
        rgbP[0] = clip(xround(1.16438356 * c + 2.11240179 * d                 )); // blue
      }
      c = y2 - 16;
      if (colors == 601) { // BT.601
        rgbP[5] = clip(xround(1.16438356 * c                  + 1.59602679 * e)); // red
        rgbP[4] = clip(xround(1.16438356 * c - 0.39176229 * d - 0.81296765 * e)); // green
        rgbP[3] = clip(xround(1.16438356 * c + 2.01723214 * d                 )); // blue
      }
      else { // BT.709
        rgbP[5] = clip(xround(1.16438356 * c                  + 1.79274107 * e)); // red
        rgbP[4] = clip(xround(1.16438356 * c - 0.21324861 * d - 0.53290933 * e)); // green
        rgbP[3] = clip(xround(1.16438356 * c + 2.11240179 * d                 )); // blue
      }
      rgbP += 6;
    } // for jj
    rgbP -= width * 3;
  } // for ii
} // yuy2_rgb


// Decode I420 frame buffer to RGB
// I420 = 4:2:0, 12 bits (2:1 hor. downsamp., 2:1 vert. downsamp., planar, reversed YV12)
void i420_rgb(long width, long height, long *rgbBufferSize)
{
  int y, u, v, yp, up, vp;
  int c, d, e;

  *rgbBufferSize = width * height * 3;

  // Allocate buffer for RGB image
  rgbBuffer = new char[*rgbBufferSize];
  if (!rgbBuffer)
    exit_message("Could not allocate data buffer for RGB image\n", 45);

  char *imgP = imgBuffer;
  // Windows bitmap rows are upside down!
  char *rgbP = rgbBuffer + *rgbBufferSize;

  int tot_len = width * height;
  for (int ii = 0; ii < height; ii++) { // y
    rgbP -= width * 3;
    for (int jj = 0; jj < width; jj++) { // x
      yp = ii*width + jj;
      up = (ii/2)*(width/2) + (jj/2) + tot_len;
      vp = (ii/2)*(width/2) + (jj/2) + tot_len + (tot_len/4);
      y = imgP[yp] & 0xFF;
      u = imgP[up] & 0xFF;
      v = imgP[vp] & 0xFF;

      c = y - 16;  // luma
      d = u - 128; // Cb
      e = v - 128; // Cr
      if (colors == 601) { // BT.601
        rgbP[2] = clip(xround(1.16438356 * c                  + 1.59602679 * e)); // red
        rgbP[1] = clip(xround(1.16438356 * c - 0.39176229 * d - 0.81296765 * e)); // green
        rgbP[0] = clip(xround(1.16438356 * c + 2.01723214 * d                 )); // blue
      }
      else { // BT.709
        rgbP[2] = clip(xround(1.16438356 * c                  + 1.79274107 * e)); // red
        rgbP[1] = clip(xround(1.16438356 * c - 0.21324861 * d - 0.53290933 * e)); // green
        rgbP[0] = clip(xround(1.16438356 * c + 2.11240179 * d                 )); // blue
      }
      rgbP += 3;
    } // for jj
    rgbP -= width * 3;
  } // for ii
} // i420_rgb


// Fix BITMAPINFOHEADER for 24 bit RGB .bmp
void fix_bmiHeader(VIDEOINFOHEADER *pVih, long bufferSize)
{
  pVih->bmiHeader.biPlanes = 1;
  pVih->bmiHeader.biBitCount = 24;
  pVih->bmiHeader.biCompression = BI_RGB;
  pVih->bmiHeader.biSizeImage = bufferSize; // can be 0 for RGB
  pVih->bmiHeader.biXPelsPerMeter = 0;
  pVih->bmiHeader.biYPelsPerMeter = 0;
  pVih->bmiHeader.biClrUsed = 0;
  pVih->bmiHeader.biClrImportant = 0;
} // fix_bmiHeader


const char jpeg_header[] = {
  0xff, 0xd8, // SOI
  0xff, 0xe0, // APP0
  0x00, 0x10, // APP0 header size (including this field, but excluding preceding)
  0x4a, 0x46, 0x49, 0x46, 0x00, // ID string 'JFIF\0'
  0x01, 0x01, // version
  0x00,       // bits per type
  0x00, 0x00, // X density
  0x00, 0x00, // Y density
  0x00,       // X thumbnail size
  0x00        // Y thumbnail size
};

const int dht_segment_size = 420;
const char dht_segment_head[] = { 0xFF, 0xC4, 0x01, 0xA2, 0x00 };
const char dht_segment_frag[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09,
  0x0a, 0x0b, 0x01, 0x00, 0x03, 0x01, 0x01, 0x01, 0x01, 0x01,
  0x01, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00
};

const char mjpeg_bits_dc_luminance[17] =
{ /* 0-base */ 0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 };
const char mjpeg_val_dc[12] =
{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

const char mjpeg_bits_ac_luminance[17] =
{ /* 0-base */ 0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7d };
const char mjpeg_val_ac_luminance[] = {
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12,
  0x21, 0x31, 0x41, 0x06, 0x13, 0x51, 0x61, 0x07,
  0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xa1, 0x08,
  0x23, 0x42, 0xb1, 0xc1, 0x15, 0x52, 0xd1, 0xf0,
  0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0a, 0x16,
  0x17, 0x18, 0x19, 0x1a, 0x25, 0x26, 0x27, 0x28,
  0x29, 0x2a, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49,
  0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
  0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
  0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79,
  0x7a, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8a, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98,
  0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
  0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4, 0xb5, 0xb6,
  0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3, 0xc4, 0xc5,
  0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2, 0xd3, 0xd4,
  0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda, 0xe1, 0xe2,
  0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea,
  0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

const char mjpeg_bits_ac_chrominance[17] =
{ /* 0-base */ 0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77 };
const char mjpeg_val_ac_chrominance[] = {
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21,
  0x31, 0x06, 0x12, 0x41, 0x51, 0x07, 0x61, 0x71,
  0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91,
  0xa1, 0xb1, 0xc1, 0x09, 0x23, 0x33, 0x52, 0xf0,
  0x15, 0x62, 0x72, 0xd1, 0x0a, 0x16, 0x24, 0x34,
  0xe1, 0x25, 0xf1, 0x17, 0x18, 0x19, 0x1a, 0x26,
  0x27, 0x28, 0x29, 0x2a, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3a, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
  0x49, 0x4a, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
  0x59, 0x5a, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
  0x69, 0x6a, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
  0x79, 0x7a, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
  0x88, 0x89, 0x8a, 0x92, 0x93, 0x94, 0x95, 0x96,
  0x97, 0x98, 0x99, 0x9a, 0xa2, 0xa3, 0xa4, 0xa5,
  0xa6, 0xa7, 0xa8, 0xa9, 0xaa, 0xb2, 0xb3, 0xb4,
  0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xc2, 0xc3,
  0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xd2,
  0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xda,
  0xe2, 0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9,
  0xea, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8,
  0xf9, 0xfa
};

char *append(char *buf, const char *src, int size)
{
  memcpy(buf, src, size);
  return buf + size;
}

char *append_dht_segment(char *buf)
{
  buf = append(buf, dht_segment_head, sizeof(dht_segment_head));
  buf = append(buf, mjpeg_bits_dc_luminance + 1, 16);
  buf = append(buf, dht_segment_frag, sizeof(dht_segment_frag));
  buf = append(buf, mjpeg_val_dc, 12);
  *(buf++) = 0x10;
  buf = append(buf, mjpeg_bits_ac_luminance + 1, 16);
  buf = append(buf, mjpeg_val_ac_luminance, 162);
  *(buf++) = 0x11;
  buf = append(buf, mjpeg_bits_ac_chrominance + 1, 16);
  buf = append(buf, mjpeg_val_ac_chrominance, 162);
  return buf;
}

// Add missing default huffman table to MJPG image header
void mjpeg_fix(long *imgBufferSize)
{
  int skip = (imgBuffer[4] << 8) + imgBuffer[5] + 4;
  if (*imgBufferSize < skip) return; // input is truncated

  long tmpBufferSize = sizeof(jpeg_header) + dht_segment_size +
                       *imgBufferSize - skip;

  char *tmpBuffer = new char[tmpBufferSize];
  if (!tmpBuffer)
    exit_message("Could not allocate data buffer for temp image\n", 46);

  char *tmpP = tmpBuffer;
  tmpP = append(tmpP, jpeg_header, sizeof(jpeg_header));
  tmpP = append_dht_segment(tmpP);
  tmpP = append(tmpP, imgBuffer + skip, *imgBufferSize - skip);

  delete[] imgBuffer;
  imgBuffer = tmpBuffer; 
  *imgBufferSize = tmpBufferSize;
} // mjpeg_fix


// Convert string to fourcc code
DWORD str2fourcc(char *sCode)
{
  DWORD nCode = BI_RGB;
  int len;

  if (strcasecmp(sCode, "RGB") == 0) return BI_RGB;
  else if (strcasecmp(sCode, "RGB888") == 0) return BI_RGB;
  else if (strcasecmp(sCode, "sRGB") == 0) return BI_RGB;
  else {
    len = strlen(sCode);
    if (len > 3) nCode |= (sCode[3] & 0xFF) << 24;
    if (len > 2) nCode |= (sCode[2] & 0xFF) << 16;
    if (len > 1) nCode |= (sCode[1] & 0xFF) << 8;
    if (len > 0) nCode |= (sCode[0] & 0xFF) << 0;
  }
  return nCode;
} // str2fourcc


// Convert fourcc code to string
const char *fourcc2str(DWORD nCode)
{
  static char sCode[5];

  if (nCode == BI_RGB) return "RGB"; // RGB888, sRGB
  else if (nCode == BI_BITFIELDS) return "RGB555";
  else {
    sCode[0] = (nCode >> 0) & 0xFF;
    sCode[1] = (nCode >> 8) & 0xFF,
    sCode[2] = (nCode >> 16) & 0xFF,
    sCode[3] = (nCode >> 24) & 0xFF,
    sCode[4] = 0;
  }
  return sCode;
} // fourcc2str


// Release the format block for a media type
void _FreeMediaType(AM_MEDIA_TYPE& mt)
{
  if (mt.cbFormat != 0) {
    CoTaskMemFree((PVOID)mt.pbFormat);
    mt.cbFormat = 0;
    mt.pbFormat = NULL;
  }
  if (mt.pUnk != NULL) {
    // pUnk should not be used.
    mt.pUnk->Release();
    mt.pUnk = NULL;
  }
} // _FreeMediaType

// Delete a media type structure that was allocated on the heap
void _DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
  if (pmt != NULL) {
    _FreeMediaType(*pmt);
    CoTaskMemFree(pmt);
  }
} // _DeleteMediaType


// Convert string to uppercase
char *strupcase(char *s)
{
  int len = strlen(s);
  for (int ii = 0; ii < len; ii++)
    s[ii] = toupper(s[ii]);
  return s;
} // strupcase


int main(int argc, char **argv,
         std::vector<uint8_t>& out_img,
         uint32_t& out_width, uint32_t& out_height,
         bool& out_is_nv12)
{
  // Capture settings
  int quiet = 0;
  int snapshot_delay = 2000;
  int show_preview_window = 0;
  int list_devices = 0;
  int details = 0;
  int device_number = 1;
  char device_name[256];
  char filename[256];
  bool fn_forced = false;
  bool want_size = false;
  int width = 0;
  int height = 0;
  bool want_fourcc = false;
  DWORD fourcc = BI_RGB;

  // Other variables
  char char_buffer[256];

  // Default device name and output filename
  strcpy(device_name, "");
  strcpy(filename, "capture.bmp");

  // First check if output messages should be suppressed
  for (int n = 1; n < argc; n++) {
    // Check next command line argument
    if (strcasecmp(argv[n], "/quiet") == 0) {
      // Don't output any messages
      quiet = 1;
    }
  }

  // Set stdout as unbuffered
  setvbuf(stdout, NULL, _IONBF, 0);

  // Information message
  if (!quiet) {
    fprintf(stdout, "CommandCam 2.8  Copyright (c) 2016 Ted Burke, Matjaz Rihtar  (Nov 17, 2016)\n");
    fprintf(stdout, "https://batchloaf.wordpress.com, https://github.com/mrihtar/CommandCam\n");
    fprintf(stdout, "\n");
  }

  // Parse command line arguments. Available options:
  //
  //	/delay <DELAY_IN_MILLISECONDS>
  //	/filename <OUTPUT_FILENAME>
  //	/devnum <DEVICE_NUMBER>
  //	/devname <DEVICE_NAME>
  //	/size <WIDTH>x<HEIGHT>
  //	/fourcc <FOURCC>
  //	/colors <BT.601 | BT.709>
  //	/preview
  //	/devlist
  //	/details
  //	/quiet
  //
  int n = 1;
  while (n < argc) {
    // Process next command line argument
    if (strcasecmp(argv[n], "/quiet") == 0) {
      // This command line argument has already been
      // processed above, so just ignore it now.
    }
    else if (strcasecmp(argv[n], "/preview") == 0) {
      // Enable preview window
      show_preview_window = 1;
    }
    else if (strcasecmp(argv[n], "/devlist") == 0) {
      // Set flag to list devices rather than capture image
      list_devices = 1;
    }
    else if (strcasecmp(argv[n], "/details") == 0) {
      // Set flag to list devices with details rather than capture image
      list_devices = 1;
      details = 1;
    }
    else if (strcasecmp(argv[n], "/filename") == 0) {
      // Set output filename to specified string
      if (++n < argc) {
        // Copy provided string into filename
        strncpy(filename, argv[n], 255); filename[255] = '\0';

        fn_forced = true;
      }
      else
        exit_message("Error: no filename specified\n", 2);
    }
    else if (strcasecmp(argv[n], "/delay") == 0) {
      // Set snapshot delay to specified value
      if (++n < argc) snapshot_delay = atoi(argv[n]);
      else exit_message("Error: no delay specified\n", 3);

      if (snapshot_delay < 0)
        exit_message("Error: invalid delay specified\n", 3);
    }
    else if (strcasecmp(argv[n], "/devnum") == 0) {
      // Set device number to specified value
      if (++n < argc) device_number = atoi(argv[n]);
      else exit_message("Error: no device number specified\n", 4);

      if (device_number <= 0)
        exit_message("Error: invalid device number\n", 4);
    }
    else if (strcasecmp(argv[n], "/devname") == 0) {
      // Set device name to specified value
      if (++n < argc) {
        // Copy provided string into device_name
        strncpy(device_name, argv[n], 255); device_name[255] = '\0';

        // Remember to choose by name rather than number
        device_number = 0;
      }
      else
        exit_message("Error: no device name specified\n", 5);
    }
    else if (strcasecmp(argv[n], "/size") == 0) {
      // Set capture size to specified value
      if (++n < argc) {
        // Copy size specification into char buffer
        strncpy(char_buffer, argv[n], 255); char_buffer[255] = '\0';

        int rn = sscanf(char_buffer, "%dx%d", &width, &height);
        if (rn != 2)
          exit_message("Error: invalid size specified\n", 6);

        want_size = true;
      }
      else exit_message("Error: no size specified\n", 6);
    }
    else if (strcasecmp(argv[n], "/fourcc") == 0) {
      // Set desired fourcc to specified string
      if (++n < argc) {
        // Copy provided string into char buffer
        strncpy(char_buffer, argv[n], 255); char_buffer[255] = '\0';
        fourcc = str2fourcc(strupcase(char_buffer));

        // Remember to check for desired fourcc
        want_fourcc = true;
      }
      else
        exit_message("Error: no fourcc specified\n", 7);
    }
    else if (strcasecmp(argv[n], "/colors") == 0) {
      // Set desired color conversion standard
      if (++n < argc) {
        if (strcasecmp(argv[n], "bt.601") == 0)
          colors = 601;
        else if (strcasecmp(argv[n], "bt.709") == 0)
          colors = 709;
        else
          exit_message("Error: invalid color standard specified\n", 8);
      }
      else
        exit_message("Error: no color standard specified\n", 8);
    }
    else {
      // Unknown command line argument
      fprintf(stderr, "Unrecognized option: %s\n", argv[n]);
      fprintf(stderr, "\n");
      fprintf(stderr, "Available options:\n");
      fprintf(stderr, "  /delay <DELAY_IN_MILLISECONDS>    (default: 2000 ms)\n");
      fprintf(stderr, "  /filename <OUTPUT_FILENAME>       (default: capture.bmp)\n");
      fprintf(stderr, "  /devnum <DEVICE_NUMBER>           (default: 1st in list)\n");
      fprintf(stderr, "  /devname <DEVICE_NAME>\n");
      fprintf(stderr, "  /size <WIDTH>x<HEIGHT>            (default: 1st in list)\n");
      fprintf(stderr, "  /fourcc <FOURCC>                  (default: RGB)\n");
      fprintf(stderr, "  /colors <BT.601 | BT.709>         (default: BT.601)\n");
      fprintf(stderr, "  /preview\n");
      fprintf(stderr, "  /devlist\n");
      fprintf(stderr, "  /details\n");
      fprintf(stderr, "  /quiet\n");
      exit_message("", 1);
    }

    // Increment command line argument counter
    n++;
  }

  // Intialise COM
  hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  //if (hr != S_OK)
  //  exit_message("Could not initialise COM\n", 9);

  // Create filter graph
  hr = CoCreateInstance(CLSID_FilterGraph, NULL,
                        CLSCTX_INPROC_SERVER, IID_IGraphBuilder,
                        (void**)&pGraph);
  if (hr != S_OK)
    exit_message("Could not create filter graph\n", 10);

  // Create capture graph builder
  hr = CoCreateInstance(CLSID_CaptureGraphBuilder2, NULL,
                        CLSCTX_INPROC_SERVER, IID_ICaptureGraphBuilder2,
                        (void **)&pBuilder);
  if (hr != S_OK)
    exit_message("Could not create capture graph builder\n", 11);

  // Attach capture graph builder to graph
  hr = pBuilder->SetFiltergraph(pGraph);
  if (hr != S_OK)
    exit_message("Could not attach capture graph builder to graph\n", 12);

  // Create system device enumerator
  hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL,
                        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pDevEnum));
  if (hr != S_OK)
    exit_message("Could not create system device enumerator\n", 13);

  // Video input device enumerator
  hr = pDevEnum->CreateClassEnumerator(
          CLSID_VideoInputDeviceCategory, &pEnum, 0);
  if (hr != S_OK)
    exit_message("No video devices found\n", 14);

  // If the user has included the "/devlist" command line
  // argument, just list available devices, then exit.
  if (list_devices != 0) {
    if (!quiet) fprintf(stdout, "Available capture devices:\n");
    n = 0;
    while (1) {
      // Find next device
      hr = pEnum->Next(1, &pMoniker, NULL);
      if (hr == S_OK) {
        // Increment device counter
        n++;

        // Get device name
        hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
        VARIANT var;
        VariantInit(&var);
        hr = pPropBag->Read(L"FriendlyName", &var, 0);
        if (!quiet) fprintf(stdout, " %2d. %ls\n", n, var.bstrVal);
        VariantClear(&var);

        // If the user has included "/details" in addition to "/devlist"
        // list pins and capabilites of this device
        if (details) {
          // Create base filter
          hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&pCap);
          if (hr != S_OK)
            continue;

          hr = pCap->EnumPins(&pEnumPins);
          if (hr != S_OK)
            continue;

          IPin *pPin = NULL;
          while (pEnumPins->Next(1, &pPin, NULL) == S_OK) {
            if (pPin) {
              PIN_INFO PinInformation;
              hr = pPin->QueryPinInfo(&PinInformation);
              if (hr != S_OK)
                exit_message("Could not get pin information\n", 15);
              if (!quiet) fwprintf(stdout, L"     Pin: %s", PinInformation.achName);

              pConfig = NULL;
              hr = pPin->QueryInterface(IID_IAMStreamConfig, (void**)&pConfig);
              if (hr == S_OK) {
                int count = 0; int size = 0;
                hr = pConfig->GetNumberOfCapabilities(&count, &size);
                if (hr != S_OK)
                  exit_message("\nCould not get number of capabilities\n", 16);
                if (!quiet) fprintf(stdout, ", %d caps\n", count);

                BYTE *pSCC = new BYTE[size];

                for (int ii = 0; ii < count; ii++) {
                  AM_MEDIA_TYPE *pmt = NULL;
                  hr = pConfig->GetStreamCaps(ii, &pmt, pSCC);
                  if (hr != S_OK)
                    exit_message("Could not get stream capability\n", 17);

                  if (pmt->formattype == FORMAT_VideoInfo) {
                    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)pmt->pbFormat;

                    double fps = 1.0/pVih->AvgTimePerFrame*10000000.0;
                    if (!quiet)
                      fprintf(stdout, "       Cap %2d: FORMAT_VideoInfo, %s, %d bits, %ldx%ld @ %d fps\n",
                              ii, fourcc2str(pVih->bmiHeader.biCompression),
                              pVih->bmiHeader.biBitCount,
                              pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight,
                              (int)fps);
                  } // if FORMAT_VideoInfo
                  else if (pmt->formattype == FORMAT_VideoInfo2) {
                    if (!quiet) fprintf(stdout, "       Cap %2d: FORMAT_VideoInfo2\n", ii);
                  }
                  else {
                    wchar_t guid[256];
                    StringFromGUID2(pmt->formattype, (wchar_t *)&guid, 256);
                    if (!quiet) fwprintf(stdout, L"       Cap %2d: FORMAT_%s\n", ii, guid);
                  }

                  _DeleteMediaType(pmt);
                } // for

                delete pSCC;
              } // else got IAMStreamConfig
              else {
                if (!quiet) fprintf(stdout, "\n");
              }

              pPin->Release();
            } // if pPin
          } // while pEnumPins
        } // if details
      } // if pEnum
      else {
        // Finished listing device, so exit program
        if (n == 0) exit_message("No devices found\n", 18);
        else exit_message("", 0);
      }
    }
  }

  // Get moniker for specified video input device,
  // or for the first device if no device number
  // was specified.
  VARIANT var;
  n = 0;
  while (1) {
    // Access next device
    hr = pEnum->Next(1, &pMoniker, NULL);
    if (hr == S_OK)
      n++; // increment device count
    else {
      if (device_number == 0)
        fprintf(stderr, "Device \"%s\" not found\n", device_name);
      else
        fprintf(stderr, "Device number %d not found\n", device_number);
      exit_message("", 19);
    }

    // If device was specified by name rather than number...
    if (device_number == 0) {
      // Get video input device name
      hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
      if (hr == S_OK) {
        // Get current device name
        VariantInit(&var);
        hr = pPropBag->Read(L"FriendlyName", &var, 0);

        // Convert to a normal C string, i.e. char*
        sprintf(char_buffer, "%ls", var.bstrVal);
        VariantClear(&var);

        pPropBag->Release();
        pPropBag = NULL;

        // Exit loop if current device name matched devname
        if (strcasecmp(device_name, char_buffer) == 0) break;
      }
      else
        exit_message("Error getting device name\n", 20);
    }
    else if (n >= device_number) break;
  }

  // Get video input device name
  hr = pMoniker->BindToStorage(0, 0, IID_PPV_ARGS(&pPropBag));
  VariantInit(&var);
  hr = pPropBag->Read(L"FriendlyName", &var, 0);
  if (!quiet) fprintf(stdout, "Capture device: %ls\n", var.bstrVal);
  VariantClear(&var);

  // Create capture filter and add to graph
  hr = pMoniker->BindToObject(0, 0,
                              IID_IBaseFilter, (void**)&pCap);
  if (hr != S_OK)
    exit_message("Could not create capture filter\n", 21);

  // Add capture filter to graph
  hr = pGraph->AddFilter(pCap, L"Capture Filter");
  if (hr != S_OK)
    exit_message("Could not add capture filter to graph\n", 22);

  // Create sample grabber filter
  hr = CoCreateInstance(CLSID_SampleGrabber, NULL,
                        CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                        (void**)&pSampleGrabberFilter);
  if (hr != S_OK)
    exit_message("Could not create Sample Grabber filter\n", 23);

  // Query the ISampleGrabber interface of the sample grabber filter
#ifdef __MINGW32__
  hr = pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber,
                                            (void**)&pSampleGrabber);
#else
  hr = pSampleGrabberFilter->QueryInterface(DexterLib::IID_ISampleGrabber,
                                            (void**)&pSampleGrabber);
#endif
  if (hr != S_OK)
    exit_message("Could not get ISampleGrabber interface to sample grabber filter\n", 24);

  // Enable sample buffering in the sample grabber filter
  hr = pSampleGrabber->SetBufferSamples(true);
  if (hr != S_OK)
    exit_message("Could not enable sample buffering in the sample grabber\n", 25);

  // Set media type in sample grabber filter
  hr = pCap->EnumPins(&pEnumPins);
  if (hr != S_OK)
    exit_message("Could not enumerate pins in base filter\n", 26);

  IPin *pPin = NULL;
  bool formatSet = false;
  while (pEnumPins->Next(1, &pPin, NULL) == S_OK && !formatSet) {
    if (pPin) {
      PIN_INFO PinInformation;
      hr = pPin->QueryPinInfo(&PinInformation);
      if (hr != S_OK)
        exit_message("Could not get pin information\n", 27);

      pConfig = NULL;
      hr = pPin->QueryInterface(IID_IAMStreamConfig, (void**)&pConfig);
      if (hr == S_OK) {
        int count = 0; int size = 0;
        hr = pConfig->GetNumberOfCapabilities(&count, &size);
        if (hr != S_OK)
          exit_message("Could not get number of capabilities\n", 28);

        BYTE *pSCC = new BYTE[size];

        for (int ii = 0; ii < count && !formatSet; ii++) {
          AM_MEDIA_TYPE *pmt = NULL;
          hr = pConfig->GetStreamCaps(ii, &pmt, pSCC);
          if (hr != S_OK)
            exit_message("Could not get stream capability\n", 29);

          if (pmt->formattype == FORMAT_VideoInfo) {
            VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)pmt->pbFormat;

            if (want_size) {
              if (pVih->bmiHeader.biWidth == width &&
                  pVih->bmiHeader.biHeight == height) {
                if (want_fourcc) {
                  if (pVih->bmiHeader.biCompression == fourcc) {
                    hr = pConfig->SetFormat(pmt);
                    if (hr != S_OK)
                      exit_message("Could not set media type in sample grabber\n", 30);

                    formatSet = true;
                  } // if fourcc
                }
                else { // any fourcc
                  hr = pConfig->SetFormat(pmt);
                  if (hr != S_OK)
                    exit_message("Could not set media type in sample grabber\n", 31);

                  formatSet = true;
                }
              } // if size
            } // if want_size
            else if (want_fourcc) {
              if (pVih->bmiHeader.biCompression == fourcc) {
                hr = pConfig->SetFormat(pmt);
                if (hr != S_OK)
                  exit_message("Could not set media type in sample grabber\n", 32);

                formatSet = true;
              } // if fourcc
            } // if want_fourcc
          } // if FORMAT_VideoInfo

          _DeleteMediaType(pmt);
        } // for

        delete pSCC;
      } // else got IAMStreamConfig

      pPin->Release();
    } // if pPin
  } // while pEnumPins
  if ((want_size || want_fourcc) && !formatSet)
    fprintf(stderr, "Could not set desired format\n");

  // Add sample grabber filter to filter graph
  hr = pGraph->AddFilter(pSampleGrabberFilter, L"SampleGrab");
  if (hr != S_OK)
    exit_message("Could not add Sample Grabber to filter graph\n", 33);

  // Create Null Renderer filter
  hr = CoCreateInstance(CLSID_NullRenderer, NULL,
                        CLSCTX_INPROC_SERVER, IID_IBaseFilter,
                        (void**)&pNullRenderer);
  if (hr != S_OK)
    exit_message("Could not create Null Renderer filter\n", 34);

  // Add Null Renderer filter to filter graph
  hr = pGraph->AddFilter(pNullRenderer, L"NullRender");
  if (hr != S_OK)
    exit_message("Could not add Null Renderer to filter graph\n", 35);

  // Connect up the filter graph's capture stream
  hr = pBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
                              pCap,  pSampleGrabberFilter, pNullRenderer);
  if (hr != S_OK)
    exit_message("Could not render capture video stream\n", 36);

  // Connect up the filter graph's preview stream
  if (show_preview_window > 0) {
    hr = pBuilder->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video,
                                pCap, NULL, NULL);
    if (hr != S_OK && hr != VFW_S_NOPREVIEWPIN)
      exit_message("Could not render preview video stream\n", 37);
  }

  // Get media control interfaces to graph builder object
  hr = pGraph->QueryInterface(IID_IMediaControl,
                              (void**)&pMediaControl);
  if (hr != S_OK)
    exit_message("Could not get media control interface\n", 38);

  if (!quiet) fprintf(stdout, "Capturing...\n");
  // Run graph
  while (1) {
    hr = pMediaControl->Run();

    // Hopefully, the return value was S_OK or S_FALSE
    if (hr == S_OK) break; // graph is now running
    if (hr == S_FALSE) continue; // graph still preparing to run

    // If the Run function returned something else,
    // there must be a problem
    exit_message("Could not run filter graph\n", 39);
  }

  // Wait for specified time delay (if any)
  Sleep(snapshot_delay);

  // Grab a sample
  // First, find the required buffer size
  long imgBufferSize = 0;
  while (1) {
    // Passing in a NULL pointer signals that we're just checking
    // the required buffer size; not looking for actual data yet.
    hr = pSampleGrabber->GetCurrentBuffer(&imgBufferSize, NULL);

    // Keep trying until buffer size is set to non-zero value
    if (hr == S_OK && imgBufferSize != 0) break;

    // If the return value isn't S_OK or VFW_E_WRONG_STATE
    // then something has gone wrong. VFW_E_WRONG_STATE just
    // means that the filter graph is still starting up and
    // no data has arrived yet in the sample grabber filter.
    if (hr != S_OK && hr != VFW_E_WRONG_STATE)
      exit_message("Could not get buffer size\n", 40);
  }

  // Stop the graph
  pMediaControl->Stop();

  // Allocate buffer for image
  imgBuffer = new char[imgBufferSize];
  if (!imgBuffer)
    exit_message("Could not allocate data buffer for image\n", 41);

  // Retrieve image data from sample grabber buffer
  hr = pSampleGrabber->GetCurrentBuffer(&imgBufferSize, (long *)imgBuffer);
  if (hr != S_OK)
    exit_message("Could not get buffer data from sample grabber\n", 42);

  // Get the media type from the sample grabber filter
  AM_MEDIA_TYPE mt;
#ifdef __MINGW32__
  hr = pSampleGrabber->GetConnectedMediaType((_AMMediaType *)&mt);
#else
  hr = pSampleGrabber->GetConnectedMediaType((DexterLib::_AMMediaType *)&mt);
#endif
  if (hr != S_OK)
    exit_message("Could not get media type\n", 43);

  bool unknown_format = false;
  bool write_header = true;
  // Retrieve format information
  VIDEOINFOHEADER *pVih = NULL;
  out_is_nv12 = false;
  if ((mt.formattype == FORMAT_VideoInfo) &&
      (mt.cbFormat >= sizeof(VIDEOINFOHEADER)) &&
      (mt.pbFormat != NULL)) {
    // Get video info header structure from media type
    pVih = (VIDEOINFOHEADER*)mt.pbFormat;

    long rgbBufferSize = 0;
    fourcc = pVih->bmiHeader.biCompression;
    if (fourcc == YUY2) {
      // Convert YUY2 image to RGB image
      yuy2_rgb(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &rgbBufferSize); // e44
      fix_bmiHeader(pVih, rgbBufferSize);
    }
    else if (fourcc == I420) {
      // Convert I420 image to RGB image
      i420_rgb(pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, &rgbBufferSize); // e45
      fix_bmiHeader(pVih, rgbBufferSize);
    }
    else if (fourcc == NV12) {
      // Convert NV12 image to RGB image
      rgbBufferSize = pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight * 3;
      rgbBuffer = new char[rgbBufferSize];

      libyuv::NV12ToRAW(
              (uint8_t*)imgBuffer, pVih->bmiHeader.biWidth,
              (uint8_t*)imgBuffer + pVih->bmiHeader.biWidth * pVih->bmiHeader.biHeight, pVih->bmiHeader.biWidth,
              (uint8_t*)rgbBuffer, pVih->bmiHeader.biWidth * 3,
              pVih->bmiHeader.biWidth,
              pVih->bmiHeader.biHeight);

      fix_bmiHeader(pVih, rgbBufferSize);
      out_is_nv12 = true;
    }
    else if (fourcc == MJPG) {
      // Fix MJPG header (add default huffman table) if necessary
      if (memcmp("AVI1", imgBuffer + 6, 4) == 0) mjpeg_fix(&imgBufferSize); // e46

      // Allocate buffer for RGB image
      rgbBuffer = new char[imgBufferSize];
      if (!rgbBuffer)
        exit_message("Could not allocate data buffer for RGB image\n", 47);

      memcpy(rgbBuffer, imgBuffer, imgBufferSize);
      rgbBufferSize = imgBufferSize;
      write_header = false;
    }
    else { // RGB or any other fourcc
      // Allocate buffer for RGB image
      rgbBuffer = new char[imgBufferSize];
      if (!rgbBuffer)
        exit_message("Could not allocate data buffer for RGB image\n", 48);

      memcpy(rgbBuffer, imgBuffer, imgBufferSize);
      rgbBufferSize = imgBufferSize;

      if (fourcc != BI_RGB) {
        fprintf(stderr, "Unsupported image format (%s) captured\n",
                         fourcc2str(fourcc));
        unknown_format = true;
        write_header = false;
      }
    }

    // Print the format of the captured image
    if (!quiet) fprintf(stdout, "Captured format: %s, %ldx%ld\n",
                        fourcc2str(fourcc),
                        pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight);

    out_width = pVih->bmiHeader.biWidth;
    out_height = pVih->bmiHeader.biHeight;
    out_img.resize(rgbBufferSize);
    out_img.insert(out_img.begin(), rgbBuffer, rgbBuffer + rgbBufferSize);
    

    /*
    // Create bitmap structure
    long cbBitmapInfoSize = mt.cbFormat - SIZE_PREHEADER;
    BITMAPFILEHEADER bfh;
    ZeroMemory(&bfh, sizeof(bfh));
    bfh.bfType = 0x4D42; // Little-endian for "BM"
    bfh.bfSize = sizeof(bfh) + rgbBufferSize + cbBitmapInfoSize;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + cbBitmapInfoSize;

    if (!fn_forced) {
      char *s;
      if (fourcc == MJPG) {
        // For MJPG change filename extension to .jpg
        if ((s = strrchr(filename, '.')) != NULL) *s = '\0';
        strcat(filename, ".jpg");
      }
      else if (unknown_format) {
        // For unknown format change filename extension to .img
        if ((s = strrchr(filename, '.')) != NULL) *s = '\0';
        strcat(filename, ".img");
      }
    }

    // Open output file
    HANDLE hf = CreateFile(filename, GENERIC_WRITE, FILE_SHARE_WRITE,
                           NULL, CREATE_ALWAYS, 0, NULL);
    if (hf == INVALID_HANDLE_VALUE)
      exit_message("Error opening output file\n", 49);

    DWORD dwWritten = 0;

    if (write_header) {
      // Write the file header
      WriteFile(hf, &bfh, sizeof(bfh), &dwWritten, NULL);
      WriteFile(hf, HEADER(pVih), cbBitmapInfoSize, &dwWritten, NULL);
    }

    // Write pixel data to file
    WriteFile(hf, rgbBuffer, rgbBufferSize, &dwWritten, NULL);
    CloseHandle(hf);
    */
  }
  else
    exit_message("Wrong media type\n", 50);

  // Clean up and exit
  _FreeMediaType(mt);

  //if (!quiet) fprintf(stdout, "Captured image saved to %s\n", filename);
  exit_message("", 0);
} // main

} // namespace commandcam
