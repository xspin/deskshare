#ifndef CAPTURE_MACOS_H
#define CAPTURE_MACOS_H

#include <vector>

extern std::vector<unsigned char> captureScreen(size_t& width, size_t& height, float quality);

#endif