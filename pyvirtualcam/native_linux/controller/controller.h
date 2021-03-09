#pragma once

#include <cstdint>
#include <string>

struct Context {
    int camera_fd = -1;
    std::string camera_device;
    size_t camera_device_idx;
    bool output_running = false;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
};

void virtual_output_start(Context& ctx, uint32_t width, uint32_t height);
void virtual_output_stop(Context& ctx);
void virtual_output_send(Context& ctx, uint8_t *rgb);
std::string virtual_output_device(Context& ctx);
