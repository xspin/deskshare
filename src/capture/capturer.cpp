#include <cassert>
#include "capturer.h"

#ifdef PLATFORM_MACOS
#include "capturer_macos.hpp"
#elif defined(PLATFORM_WINDOWS)
#include "capturer_win.hpp"
#endif

std::vector<unsigned char> captureScreen(size_t& width, size_t& height, float quality);
std::pair<int,int> getCursorLoc();
std::pair<int,int> getScreenResolution();

bool Capturer::capture(float quality) {
    assert(0 < quality && quality <= 1);
    jpg = captureScreen(width, height, quality);
    return !jpg.empty();
}

std::pair<int,int> Capturer::getCursorPos() {
    return getCursorLoc();
}

std::pair<int,int> Capturer::getResolution() {
    return getScreenResolution();
}