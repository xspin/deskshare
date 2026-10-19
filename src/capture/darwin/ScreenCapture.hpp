#pragma once

#include <cstddef>
#include <future>
#include <vector>

#include "ScreenCaptureImpl.hpp"
#include "singleton.hpp"

class ScreenCapture : public Singleton<ScreenCapture> {
    friend Singleton<ScreenCapture>;

  public:
    bool init();
    void stop();

    static void _setScale(float scale);
    static float getScale();

    static void _onNV12Frame(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                             size_t uvStride, size_t width, size_t height);

    static void _onI420Frame(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                             size_t uStride, const uint8_t *vPlane, size_t vStride, size_t width,
                             size_t height);
    static void _onBGRAFrame(const uint8_t *data, size_t bytesPerRow, size_t width, size_t height);

    static bool _onFrameStart();
    static void _onFrameEnd();

    bool captureBGRA(std::vector<uint8_t> &bgra, size_t &width, size_t &height);
    bool captureNV12(std::vector<uint8_t> &data, size_t &yStride, size_t &uvStride, size_t &width,
                     size_t &height);

  private:
    ScreenCapture() = default;
    ~ScreenCapture();

    static void wait();
    static void notify();

  private:
    ScreenCaptureImpl impl_;
    bool started_ = false;
    inline static float scale_ = 0;

    inline static std::vector<uint8_t> *buf_;
    inline static size_t width_ = 0;
    inline static size_t height_ = 0;
    inline static std::atomic<bool> running_ = true;

    inline static bool put_ready_ = false;
    inline static std::mutex put_mtx_;
    inline static std::condition_variable put_cv_;

    inline static bool get_ready_ = false;
    inline static std::mutex get_mtx_;
    inline static std::condition_variable get_cv_;

    inline static bool stopped_ = false;

    // inline static I420Callback onI420Frame_ = nullptr;
    // inline static NV12Callback onNV12Frame_ = nullptr;
    // inline static BGRACallback onBGRAFrame_ = nullptr;
};
