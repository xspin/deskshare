// FrameProcessor.cpp
#include <cstring>
#include <vector>

#include "FrameProcessor.hpp"

void FrameProcessor::onI420Frame_(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                                  size_t uStride, const uint8_t *vPlane, size_t vStride,
                                  size_t width, size_t height) {
    onI420Frame(yPlane, yStride, uPlane, uStride, vPlane, vStride, width, height);
}

void FrameProcessor::onNV12Frame_(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                                  size_t uvStride, size_t width, size_t height) {
    // 注意：yStride 可能大于 width（内存对齐 padding）
    // 需要逐行处理，不能整体 memcpy

    // 示例：拷贝到连续缓冲区
    // yPlane_.resize(width * height);
    // for (size_t row = 0; row < height; row++) {
    //     memcpy(yPlane_.data() + row * width, yData + row * yStride, width);
    // }

    // UV 平面：高度是 Y 的一半，每像素 2 字节（U + V 交错）
    // uvPlane_.resize(width * (height / 2));
    // for (size_t row = 0; row < height / 2; row++) {
    //     memcpy(uvPlane_.data() + row * width, uvData + row * uvStride, width);
    // }

    // 此时 yPlane 和 uvPlane 是紧凑的 NV12 数据
    // 可以进一步处理或编码
    onNV12Frame(yData, yStride, uvData, uvStride, width, height);
}

void FrameProcessor::onBGRAFrame_(const uint8_t *data, size_t bytesPerRow, size_t width,
                                  size_t height) {
    // 示例：逐行拷贝到紧凑缓冲区
    // rgba_.resize(width * height * 4);
    // for (size_t row = 0; row < height; row++) {
    //     std::memcpy(bgra_.data() + row * width * 4, data + row * bytesPerRow, width * 4);
    // }

    onBGRAFrame(data, width, height);
}

void FrameProcessor::wait() {
    std::unique_lock<std::mutex> lock(mtx_);
    cv_.wait(lock, [] {
        printf("ready=%d\n", ready_);
        return ready_;
    });
    ready_ = false;
}

void FrameProcessor::notify() {
    ready_ = true;
    cv_.notify_all();
}
