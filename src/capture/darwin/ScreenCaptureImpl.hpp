#pragma once

class ScreenCaptureImpl {
  public:
    ScreenCaptureImpl();
    ~ScreenCaptureImpl();

    bool start();
    void stop();

  private:
    struct Impl;
    Impl *impl_;
};
