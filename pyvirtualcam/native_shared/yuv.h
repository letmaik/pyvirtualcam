#include <cstdint>

void nv12_frame_from_rgb(uint8_t* rgb, uint8_t* nv12, uint32_t width, uint32_t height);
void uyvy_frame_from_rgb(uint8_t* rgb, uint8_t* uyvy, uint32_t width, uint32_t height);
