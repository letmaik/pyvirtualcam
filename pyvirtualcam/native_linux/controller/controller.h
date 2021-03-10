#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Context;

Context* virtual_output_start(uint32_t width, uint32_t height);
void virtual_output_stop(Context* ctx);
void virtual_output_send(Context* ctx, uint8_t *rgb);
const char* virtual_output_device(Context* ctx);
void virtual_output_free(Context* ctx);

#ifdef __cplusplus
}
#endif
