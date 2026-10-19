#include "capturer.h"
#include "libyuv/scale.h"
#include <cassert>
#include <cstddef>
#include <libyuv.h>
#include <vector>

#if defined(__APPLE__) && defined(__MACH__)
#include "darwin/ScreenCapture.hpp"
#include "darwin/capturer_macos.cpp"
#elif defined(_WIN32)
#include "windows/ScreenCapture.hpp"
#include "windows/capturer_win.cpp"
#endif

namespace impl {
bool captureBgra(std::vector<unsigned char> &bgra, size_t &width, size_t &height);
bool captureRgbaScaled(std::vector<unsigned char> &bgra, size_t width, size_t height);
bool captureJpeg(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality);
std::pair<size_t, size_t> getCursorLoc();
std::pair<size_t, size_t> getScreenResolution();
} // namespace impl

Capturer::Capturer() {
}

Capturer::~Capturer() {
}

void Capturer::stop() {
    auto &sc = ScreenCapture::instance();
    sc.stop();
}

bool Capturer::captureJpg(std::vector<unsigned char> &jpg, size_t &width, size_t &height,
                          float quality) {
    assert(0 < quality && quality <= 1);
    return impl::captureJpeg(jpg, width, height, quality);
}

bool Capturer::captureBgra(std::vector<unsigned char> &bgra, size_t &width, size_t &height) {
    static std::vector<uint8_t> src;
    float scale = ScreenCapture::getScale();
    if (scale > 1) {
        size_t src_w;
        size_t src_h;

        if (!impl::captureBgra(src, src_w, src_h)) {
            return false;
        }
        width = src_w / scale;
        height = src_h / scale;
        width += width & 1;
        height += height & 1;

        if (libyuv::ARGBScale(src.data(), src_w * 4, src_w, src_h, bgra.data(), width * 4, width,
                              height, libyuv::kFilterNone)) {
            std::cerr << "ARGBScale failed";
            return false;
        }
        return true;
    } else {
        return impl::captureBgra(bgra, width, height);
    }
}

bool Capturer::captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    return impl::captureRgbaScaled(rgba, width, height);
    // size_t src_w, src_h;
    // static std::vector<uint8_t> src;
    //
    // if (!impl::captureBgra(src, src_w, src_h))
    //     return false;
    //
    // rgba.resize(width * height * 4);
    // if (libyuv::ARGBScale(src.data(), src_w * 4, src_w, src_h, rgba.data(), width * 4, width,
    //                       height, libyuv::kFilterNone))
    //     return false;
    //
    // for (size_t i = 0; i < width * height; ++i) {
    //     std::swap(rgba[i * 4 + 0], rgba[i * 4 + 2]); // 交换 R 和 B
    // }
    // return true;
}

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
