#include "frame.hpp"
#include "capturer.h"
#include <cstddef>
#include <utility>
#include <vector>

void Frame::setup(float quality, size_t fps) {
    LOG_INFO("setup quality=%f, fps=%lu", quality, fps);
    _quality = quality;
    _fps = fps;
}

std::pair<const void *, size_t> Frame::getFrame() {
    return {_frame.data(), _frame.size()};
}

std::pair<size_t, size_t> Frame::getSize() {
    return {_width, _height};
}

void Frame::setSize(size_t width, size_t height) {
    if (_width != width || _height != height) {
        LOG_INFO("Frame size: %lu x %lu", width, height);
        _width = width;
        _height = height;
    }
}

std::pair<size_t, size_t> Frame::getCursor() {
    return {_x, _y};
}

void Frame::setCursor(size_t x, size_t y) {
    _x = x;
    _y = y;
}

FrameJpeg::FrameJpeg() {
}

FrameJpeg::~FrameJpeg() {
}

size_t FrameJpeg::next() {
    size_t w, h;
    if (!Capturer::captureJpg(_frame, w, h, _quality)) {
        LOG_ERROR_STREAM << "Failed to capture jpeg";
    }
    setSize(w, h);
    auto [x, y] = Capturer::getCursorPos();
    setCursor(x, y);
    return _frame.size();
}

FrameH264::FrameH264() {
    _h264.onPackets([this](const uint8_t *data, size_t size) {
        this->_frame.insert(this->_frame.end(), data, data + size);
    });
}

FrameH264::~FrameH264() {
}

size_t FrameH264::next() {
    size_t w, h;
    if (!Capturer::captureBgra(_buf, w, h)) {
        LOG_ERROR_STREAM << "Failed to capture rgba";
        return 0;
    }
    setSize(w, h);

#if defined(__APPLE__) && defined(__MACH__)
    setCursor(0, 0);
#else
    auto [x, y] = Capturer::getCursorPos();
    setCursor(x, y);
#endif

    double key = w * h * _quality * _fps;
    if (key != _key) {
        LOG_INFO("Init H264 %lux%lu, quality %f, fps %lu", w, h, _quality, _fps);
        if (!_h264.init(w, h, _quality, _fps)) {
            LOG_ERROR_STREAM << "Failed to H264 init";
            return 0;
        }
        _key = key;
    }

    _frame.clear();

    auto [fs, kf] = _h264.encodeBGRA(_buf, _key_frame);
    _current_key_frame = kf;
    if (fs <= 0) {
        LOG_ERROR_STREAM << "Failed to H264 encode";
        return 0;
    }
    _key_frame = false;
    if ((size_t)fs != _frame.size()) {
        LOG_WARNING_STREAM << "H264 encode mismatched size " << fs << " " << _frame.size();
    }
    return fs;
}

void FrameH264::setKeyFrame() {
    _key_frame = true;
}

bool FrameH264::isKeyFrame() {
    return _current_key_frame;
}
