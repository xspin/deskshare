#include "capturer.h"
#include <cassert>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include "capturer_macos.cpp"
#elif defined(_WIN32)
#include "capturer_win.cpp"
#endif

namespace impl {
bool captureRgba(std::vector<unsigned char> &rgba, size_t width, size_t height);
bool captureScreen(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality);
std::pair<size_t, size_t> getCursorLoc();
std::pair<size_t, size_t> getScreenResolution();
} // namespace impl

bool Capturer::captureJpg(std::vector<unsigned char> &jpg, size_t &width, size_t &height,
                          float quality) {
    assert(0 < quality && quality <= 1);
    return impl::captureScreen(jpg, width, height, quality);
}

bool Capturer::captureRgba(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    return impl::captureRgba(rgba, width, height);
}

std::pair<size_t, size_t> Capturer::getCursorPos() {
    return impl::getCursorLoc();
}

// @return {width, height}
std::pair<size_t, size_t> Capturer::getResolution() {
    return impl::getScreenResolution();
}

void Capturer::rgbaToGray(std::vector<unsigned char> &data, size_t w, size_t h) {
    for (size_t i = 0; i < w * h; i++) {
        unsigned char r = data[i * 4 + 0];
        unsigned char g = data[i * 4 + 1];
        unsigned char b = data[i * 4 + 2];
        // Alpha 在 data[i * 4 + 3]，不动它

        unsigned char gray = (unsigned char)((r * 299 + g * 587 + b * 114) / 1000);

        data[i * 4 + 0] = gray;
        data[i * 4 + 1] = gray;
        data[i * 4 + 2] = gray;
    }
}
