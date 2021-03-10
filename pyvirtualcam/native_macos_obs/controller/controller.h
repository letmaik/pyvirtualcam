#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void virtual_output_start(uint32_t width, uint32_t height, double fps);
void virtual_output_stop();
void virtual_output_send(uint8_t *rgb);
const char* virtual_output_device();

#ifdef __cplusplus
}
#endif
