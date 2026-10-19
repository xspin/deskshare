#pragma once

#include <cstddef>
#include <future>
#include <vector>

#include "singleton.hpp"

class ScreenCapture : public Singleton<ScreenCapture> {
    friend Singleton<ScreenCapture>;

  public:
    bool init() {
        return true;
    }

    void stop() {
    }

    static float getScale() {
        return scale_;
    }

    bool captureBGRA(std::vector<uint8_t> &bgra, size_t &width, size_t &height);

  private:
    ScreenCapture() = default;
    ~ScreenCapture() = default;

  private:
    inline static float scale_ = 0;
};
