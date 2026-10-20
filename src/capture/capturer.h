#pragma once

#include <string>
#include <vector>

#include "singleton.hpp"

class Capturer : public Singleton<Capturer> {
    friend Singleton<Capturer>;

  public:
    static std::pair<size_t, size_t> getCursorPos();
    static std::pair<size_t, size_t> getResolution();

    static bool captureJpg(std::vector<unsigned char> &jpg, size_t &width, size_t &height,
                           float quality);
    static bool captureBgra(std::vector<unsigned char> &bgra, size_t &width, size_t &height);

    // static bool captureNV12(std::vector<unsigned char> &data, size_t &width, size_t &height);

    static bool captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height);

    static bool captureRgbaMock(std::vector<unsigned char> &rgba, size_t &width, size_t &height);
    static void rgbaToGray(std::vector<unsigned char> &rgba, size_t width, size_t height);

    static void stop();
    static bool start();

  private:
    Capturer();
    ~Capturer();
};
