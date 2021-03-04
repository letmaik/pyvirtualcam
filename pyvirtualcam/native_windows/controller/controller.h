#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool virtual_output_start(int width, int height, double fps);
void virtual_output_stop();
void virtual_video(uint8_t *rgba);
bool virtual_output_is_running();

#ifdef __cplusplus
}
#endif
