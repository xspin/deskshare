#ifndef CAPTURE_WIN_H
#define CAPTURE_WIN_H

#include <vector>

extern std::vector<unsigned char> captureScreen(size_t& width, size_t& height, float quality);

#endif