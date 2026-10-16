#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>

#include "capframe.hpp"

class FrameProcessor {
  public:
    FrameProcessor() = delete;
    FrameProcessor(const FrameProcessor &) = delete;
    FrameProcessor &operator=(const FrameProcessor &) = delete;

    // NV12 帧回调：Y 平面 + UV 交错平面
    // yData/uvData: 平面起始地址
    // yStride/uvStride: 每行实际字节数（可能含 padding）
    // width/height: Y 平面的逻辑宽高
    static void onNV12Frame_(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                             size_t uvStride, size_t width, size_t height);

    static void onI420Frame_(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                             size_t uStride, const uint8_t *vPlane, size_t vStride, size_t width,
                             size_t height);

    // RGBA/BGRA 帧回调：单平面连续内存
    // data: 起始地址
    // bytesPerRow: 每行实际字节数（可能 > width * 4）
    static void onBGRAFrame_(const uint8_t *data, size_t bytesPerRow, size_t width, size_t height);

    // static void onJPEGFrame(const uint8_t *data, size_t size);

    static void wait();
    static void notify();

    inline static I420Callback onI420Frame = nullptr;
    inline static NV12Callback onNV12Frame = nullptr;
    inline static BGRACallback onBGRAFrame = nullptr;

  private:
    inline static bool ready_ = false;
    inline static std::mutex mtx_;
    inline static std::condition_variable cv_;
};

// TDODO
