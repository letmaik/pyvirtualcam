#include <stdio.h>
#include <Windows.h>
#include "queue/shared-memory-queue.h"
#include "controller.h"

static video_queue_t *vq;
static bool output_running = false;

static bool have_clockfreq = false;
static LARGE_INTEGER clock_freq;

static uint64_t get_timestamp_ns()
{
	LARGE_INTEGER current_time;
	double time_val;

	if (!have_clockfreq) {
		QueryPerformanceFrequency(&clock_freq);
		have_clockfreq = true;
	}

	QueryPerformanceCounter(&current_time);
	time_val = (double)current_time.QuadPart;
	time_val *= 1000000000.0;
	time_val /= (double)clock_freq.QuadPart;

	return (uint64_t)time_val;
}

static void YfromRGB(uint8_t* y, uint8_t r, uint8_t g, uint8_t b) {
    *y = (uint8_t)( 0.257 * r + 0.504 * g + 0.098 * b +  16);
}
static void UVfromRGB(uint8_t* u, uint8_t* v, uint8_t r, uint8_t g, uint8_t b) {
    *u = (uint8_t)(-0.148 * r - 0.291 * g + 0.439 * b + 128);
    *v = (uint8_t)( 0.439 * r - 0.368 * g - 0.071 * b + 128);
}

bool virtual_output_start(int width, int height, double fps)
{
    if (output_running) {
        fprintf(stderr, "virtual camera output already started\n");
        return false;
    }

	bool start = false;
	uint64_t interval = (uint64_t)(10000000.0 / fps);

	vq = video_queue_create(width, height, interval);

	if (vq) {
		output_running = true;
	} else {
		output_running = false;
		fprintf(stderr, "video_queue_create() failed\n");
		return false;
	}

	return true;
}

void virtual_output_stop()
{
    if (!output_running) {
        return;
	}	
	video_queue_close(vq);
	output_running = false;
}

// data is in RGBA format (packed RGBA, 32bpp, RGBARGBA...)
// queue expects NV12 (semi-planar YUV, 12bpp)
void virtual_video(uint8_t *rgba)
{
	if (!output_running)
		return;

	uint32_t cx, cy;
	uint64_t interval;
	video_queue_get_info(vq, &cx, &cy, &interval);

	uint8_t* nv12 = malloc((cx + cx / 2) * cy );
	if (!nv12) {
		fprintf(stderr, "out of memory\n");
		return;
	}

	// NV12 has two planes
	uint8_t* y = nv12;
	uint8_t* uv = nv12 + cx * cy;

	// Compute Y plane
	for (uint32_t i=0; i < cx * cy; i++) {
		YfromRGB(&y[i], rgba[i * 4], rgba[i * 4 + 1], rgba[i * 4 + 1]);
	}
	// Compute UV plane
	for (uint32_t y=0; y < cy; y = y + 2) {
		for (uint32_t x=0; x < cx; x = x + 2) {
			// Downsample by 2
			uint8_t* rgba1 = rgba + ((y * cx + x) * 4);
			uint8_t* rgba2 = rgba + (((y + 1) * cx + x) * 4);
            uint8_t r = (rgba1[0+0] + rgba1[4+0] + rgba2[0+0] + rgba2[4+0]) / 4;
            uint8_t g = (rgba1[0+1] + rgba1[4+1] + rgba2[0+1] + rgba2[4+1]) / 4;
            uint8_t b = (rgba1[0+2] + rgba1[4+2] + rgba2[0+2] + rgba2[4+2]) / 4;

			uint8_t* u = &uv[y * cx / 4 + x / 2];
			uint8_t* v = u + 1;
			UVfromRGB(u, v, r, g, b);
		}
	}

	// One entry per plane
    uint32_t linesize[2] = { cx, cx / 2 };
	uint8_t* data[2] = { y, uv };

	uint64_t timestamp = get_timestamp_ns();

    video_queue_write(vq, data, linesize, timestamp);
	free(nv12);
}

bool virtual_output_is_running()
{
	return output_running;
}
