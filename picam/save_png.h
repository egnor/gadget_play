// Utility to save pixel data as .png for debugging

#pragma once

#include <vector>

void save_png(
    int width, int height, int stride, uint8_t const* rgb888,
    std::string const& filename
);
