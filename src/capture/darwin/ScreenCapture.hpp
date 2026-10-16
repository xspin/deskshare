#pragma once

#include <cstddef>

#include "FrameProcessor.hpp"
#include "ScreenCaptureImpl.hpp"
#include "singleton.hpp"

class ScreenCapture : public Singleton<ScreenCapture> {
    friend Singleton<ScreenCapture>;

  public:
    bool init();

    void captureI420Async(I420Callback cb);

  private:
    ScreenCapture() = default;
    ~ScreenCapture() = default;

    ScreenCaptureImpl impl_;
    bool started_ = false;
};
