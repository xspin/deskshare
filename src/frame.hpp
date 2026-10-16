#pragma once
#include "capture/capturer.h"
#include "h264/h264.hpp"
#include "logger.h"
#include <cstddef>
#include <utility>
#include <vector>

class Frame {
  public:
    Frame(){};
    virtual ~Frame() = default;

    void setup(float quality, size_t fps) {
        LOG_INFO("setup quality=%f, fps=%lu", quality, fps);
        _quality = quality;
        _fps = fps;
    }

    virtual size_t next() = 0;

    std::pair<const void *, size_t> getFrame() {
        return {_frame.data(), _frame.size()};
    }

    std::pair<size_t, size_t> getSize() {
        return {_width, _height};
    }

    void setSize(size_t width, size_t height) {
        if (_width != width || _height != height) {
            LOG_INFO("Frame size: %lu x %lu", width, height);
            _width = width;
            _height = height;
        }
    }

  protected:
    std::vector<unsigned char> _frame;
    float _quality = 0.6;
    size_t _fps = 10;

  private:
    size_t _width = 0;
    size_t _height = 0;
};

class FrameJpeg : public Frame {
  public:
    FrameJpeg() {
    }
    ~FrameJpeg() {
    }

    size_t next() {
        size_t w, h;
        if (!Capturer::captureJpg(_frame, w, h, _quality)) {
            LOG_ERROR_STREAM << "Failed to capture jpeg";
        }
        setSize(w, h);
        return _frame.size();
    }
};

class FrameH264 : public Frame {
  public:
    FrameH264() {
        _h264.onPackets([this](const uint8_t *data, size_t size) {
            this->_frame.insert(this->_frame.end(), data, data + size);
        });
    }

    ~FrameH264() {
    }

    size_t next() {
        size_t w, h;
        if (!Capturer::captureRgba(_rgba, w, h)) {
            LOG_ERROR_STREAM << "Failed to capture rgba";
            return 0;
        }
        setSize(w, h);
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

        auto [fs, kf] = _h264.encode(_rgba, _key_frame);
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

    void setKeyFrame() {
        _key_frame = true;
    }

    bool isKeyFrame() {
        return _current_key_frame;
    }

  private:
    std::vector<unsigned char> _rgba;
    H264Encoder _h264;
    double _key = 0;
    bool _key_frame;
    bool _current_key_frame = false;
};
