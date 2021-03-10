#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct Context {
    bool output_running = false;
    int camera_fd;
    std::string camera_device;
    size_t camera_device_idx;
    uint32_t frame_width;
    uint32_t frame_height;
    std::vector<uint8_t> buffer;
};

void virtual_output_start(Context& ctx, uint32_t width, uint32_t height);
void virtual_output_stop(Context& ctx);
void virtual_output_send(Context& ctx, uint8_t *rgb);
std::string virtual_output_device(Context& ctx);
