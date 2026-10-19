#pragma once
#include "capture/capturer.h"
#include "h264/h264.hpp"
#include "logger.h"
#include <cstddef>
#include <iterator>
#include <utility>
#include <vector>

class Frame {
  public:
    Frame() = default;
    virtual ~Frame() = default;

    virtual size_t next() = 0;
    void setup(float quality, size_t fps);
    std::pair<const void *, size_t> getFrame();
    std::pair<size_t, size_t> getSize();
    void setSize(size_t width, size_t height);
    std::pair<size_t, size_t> getCursor();
    void setCursor(size_t x, size_t y);

  protected:
    std::vector<unsigned char> _frame;
    float _quality = 0.6;
    size_t _fps = 10;

  private:
    size_t _width = 0;
    size_t _height = 0;
    size_t _x = 0;
    size_t _y = 0;
};

class FrameJpeg : public Frame {
  public:
    FrameJpeg();
    ~FrameJpeg();
    size_t next();
};

class FrameH264 : public Frame {
  public:
    FrameH264();
    ~FrameH264();

    size_t next();
    void setKeyFrame();
    bool isKeyFrame();

  private:
    std::vector<uint8_t> _buf;
    H264Encoder _h264;
    double _key = 0;
    bool _key_frame;
    bool _current_key_frame = false;
};
