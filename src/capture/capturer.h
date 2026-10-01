#pragma once

#include <string>
#include <vector>

class Capturer {

  public:
    Capturer(const Capturer &) = delete;
    Capturer &operator=(const Capturer &) = delete;
    Capturer(Capturer &&) = delete;
    Capturer &operator=(Capturer &&) = delete;

    static Capturer &getInstance() {
        static Capturer instance; // C++11 起，局部静态变量初始化是线程安全的
        return instance;
    }

    static std::pair<size_t, size_t> getCursorPos();
    static std::pair<size_t, size_t> getResolution();

    bool capture(float quality);
    bool captureRgba(std::vector<unsigned char> &rgba, size_t w, size_t h);

    std::vector<unsigned char> jpg;
    size_t width;
    size_t height;

  private:
    Capturer() = default;
    ~Capturer() = default;
};

