#include <stdio.h>
#include "queue/share_queue.h"
#include "queue/share_queue_write.h"
#include "controller.h"

struct virtual_out_data {
	share_queue video_queue;
	int width = 0;
	int height = 0;
	int delay = 0;
	int64_t last_video_ts = 0;
};

struct virtual_out_data *virtual_out;
bool output_running = false;

bool virtual_output_start(int width, int height, double fps, int delay)
{
    if (virtual_out) {
        fprintf(stderr, "virtual camera output already started\n");
        return false;
    }
	virtual_out = (virtual_out_data*) malloc(sizeof(struct virtual_out_data));

	bool start = false;
	virtual_out_data* out_data = (virtual_out_data*)virtual_out;
    out_data->delay = delay;
	out_data->width = width;
	out_data->height = height;
	uint64_t interval = static_cast<int64_t>(1000000000 / fps);

	start = shared_queue_create(&out_data->video_queue,
		ModeVideo, AV_PIX_FMT_RGBA, out_data->width, out_data->height,
		interval, out_data->delay + 10);

	if (start) {
		output_running = true;
		shared_queue_set_delay(&out_data->video_queue, out_data->delay);
	} else {
		output_running = false;
		shared_queue_write_close(&out_data->video_queue);

		fprintf(stderr, "shared_queue_create() failed\n");
	}

	return start;
}

void virtual_output_stop()
{
    if (!virtual_out) {
        return;
	}	
	shared_queue_write_close(&virtual_out->video_queue);
    free(virtual_out);
	output_running = false;
}

// data is in RGBA format (packed RGBA 8:8:8:8, 32bpp, RGBARGBA...)
// TODO RGB24 would be better but not supported in obs-virtual-cam
void virtual_video(int64_t timestamp, uint8_t **data)
{
	if (!output_running)
		return;

    uint32_t linesize[1] = {virtual_out->width * 4};

	virtual_out_data *out_data = (virtual_out_data*)virtual_out;
	out_data->last_video_ts = timestamp;
    shared_queue_push_video(&out_data->video_queue, linesize, 
        out_data->width, out_data->height, data, timestamp);
}

bool virtual_output_is_running()
{
	return output_running;
}
