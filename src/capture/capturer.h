#pragma once

#include <string>
#include <vector>

#include "capframe.hpp"
#include "singleton.hpp"

// class ScreenCapture;

class Capturer : public Singleton<Capturer> {
    friend Singleton<Capturer>;

  public:
    static std::pair<size_t, size_t> getCursorPos();
    static std::pair<size_t, size_t> getResolution();

    static bool captureJpg(std::vector<unsigned char> &jpg, size_t &width, size_t &height,
                           float quality);
    static bool captureRgba(std::vector<unsigned char> &rgba);
    static bool captureRgba(std::vector<unsigned char> &rgba, size_t &width, size_t &height);
    static bool captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height);
    static bool captureRgbaMock(std::vector<unsigned char> &rgba, size_t &width, size_t &height);

    static void rgbaToGray(std::vector<unsigned char> &rgba, size_t width, size_t height);

    // void captureI420Async(I420Callback);

  private:
    Capturer();
    ~Capturer();

    void init();

    // ScreenCapture *sc_ = nullptr;
};
