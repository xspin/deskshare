#include "capturer.h"
#include <cassert>
#include <cstddef>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include "darwin/ScreenCapture.hpp"
#include "darwin/capturer_macos.cpp"
#elif defined(_WIN32)
#include "windows/capturer_win.cpp"
#endif

namespace impl {
bool captureRgba(std::vector<unsigned char> &rgba, size_t &width, size_t &height);
bool captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height);
bool captureJpeg(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality);
std::pair<size_t, size_t> getCursorLoc();
std::pair<size_t, size_t> getScreenResolution();
} // namespace impl

Capturer::Capturer() {
    // sc_ = &ScreenCapture::instance();
}

Capturer::~Capturer() {
}

bool Capturer::captureJpg(std::vector<unsigned char> &jpg, size_t &width, size_t &height,
                          float quality) {
    assert(0 < quality && quality <= 1);
    return impl::captureJpeg(jpg, width, height, quality);
}

bool Capturer::captureRgba(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
    return impl::captureRgba(rgba, width, height);
}

bool Capturer::captureRgba(std::vector<unsigned char> &rgba) {
    size_t width, height;
    return impl::captureRgba(rgba, width, height);
}

bool Capturer::captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    return impl::captureRgbaScaled(rgba, width, height);
}

// bool Capturer::captureRgbaEven(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
//     auto [w, h] = impl::getScreenResolution();
//     if (w % 2 || h % 2) {
//         width = w - (w & 1);
//         height = h - (h & 1);
//         return impl::captureRgbaScaled(rgba, width, height);
//     }
//     return impl::captureRgba(rgba, width, height);
// }

bool Capturer::captureRgbaMock(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
    static int frame_idx = 0;
    auto r = impl::getScreenResolution();
    width = r.first;
    height = r.second;
    rgba.resize(width * height * 4);

    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            size_t idx = (y * width + x) * 3;
            rgba[idx] = (x + frame_idx) % 256;     // R
            rgba[idx + 1] = (y + frame_idx) % 256; // G
            rgba[idx + 2] = (x + y) % 256;         // B
                                                   // A
        }
    }
    frame_idx++;
    return true;
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

// void Capturer::captureI420Async(I420Callback cb) {
//     sc_->captureI420Async(std::move(cb));
// }
