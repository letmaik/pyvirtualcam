#pragma once

#include <cstdint>
#include <vector>

namespace commandcam {
  int main(int argc, char **argv,
         std::vector<uint8_t>& out_img,
         uint32_t& out_width, uint32_t& out_height,
         bool& out_is_nv12);
}