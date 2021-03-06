#pragma once

#include <cstdint>
#include <string>

bool virtual_output_start(uint32_t width, uint32_t height, double fps);
void virtual_output_stop();
void virtual_video(uint8_t *rgba);
bool virtual_output_is_running();
std::string virtual_output_device();
