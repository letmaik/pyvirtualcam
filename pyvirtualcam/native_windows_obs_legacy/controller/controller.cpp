#include <stdio.h>
#include "queue/share_queue.h"
#include "queue/share_queue_write.h"
#include "controller.h"

struct virtual_out_data {
	share_queue video_queue;
	int width = 0;
	int height = 0;
};

static struct virtual_out_data *virtual_out;
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

bool virtual_output_start(int width, int height, double fps, int delay)
{
    if (output_running) {
        fprintf(stderr, "virtual camera output already started\n");
        return false;
    }
	virtual_out = (virtual_out_data*) malloc(sizeof(struct virtual_out_data));

	bool start = false;
	virtual_out_data* out_data = (virtual_out_data*)virtual_out;
	out_data->width = width;
	out_data->height = height;
	uint64_t interval = static_cast<int64_t>(1000000000 / fps);

	start = shared_queue_create(&out_data->video_queue,
		ModeVideo, AV_PIX_FMT_RGBA, out_data->width, out_data->height,
		interval, delay + 10);

	if (start) {
		output_running = true;
		shared_queue_set_delay(&out_data->video_queue, delay);
	} else {
		output_running = false;
		shared_queue_write_close(&out_data->video_queue);
		free(virtual_out);

		fprintf(stderr, "shared_queue_create() failed\n");
	}

	return start;
}

void virtual_output_stop()
{
    if (!output_running) {
        return;
	}	
	shared_queue_write_close(&virtual_out->video_queue);
    free(virtual_out);
	output_running = false;
}

// data is in RGBA format (packed RGBA 8:8:8:8, 32bpp, RGBARGBA...)
void virtual_video(uint8_t *rgba)
{
	if (!output_running)
		return;

	// single plane
    uint32_t linesize[1] = {virtual_out->width * 4};
	uint8_t* data[1] = {rgba};

	uint64_t timestamp = get_timestamp_ns();

	virtual_out_data *out_data = (virtual_out_data*)virtual_out;
    shared_queue_push_video(&out_data->video_queue, linesize, 
        out_data->width, out_data->height, data, timestamp);
}

bool virtual_output_is_running()
{
	return output_running;
}
