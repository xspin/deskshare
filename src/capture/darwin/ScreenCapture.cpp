#include "ScreenCapture.hpp"
#include <cassert>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <vector>

ScreenCapture::~ScreenCapture() {
    running_ = false;
}

void ScreenCapture::stop() {
    running_ = false;
    impl_.stop();
    cv_.notify_all();
}

bool ScreenCapture::start() {
    if (!running_) {
        running_ = impl_.start();
    }
    return running_;
}

void ScreenCapture::_setScale(float scale) {
    scale_ = scale;
}

float ScreenCapture::getScale() {
    return scale_;
}

bool ScreenCapture::_onFrameStart() {
    return running_;
}

void ScreenCapture::_onFrameEnd() {
}

void ScreenCapture::_onI420Frame(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                                 size_t uStride, const uint8_t *vPlane, size_t vStride,
                                 size_t width, size_t height) {
    fprintf(stderr, "Not implemented yet!\n");
    assert(false);
}

void ScreenCapture::_onNV12Frame(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                                 size_t uvStride, size_t width, size_t height) {
    // Y 平面：height 行，每行 yStride 字节
    size_t ySize = yStride * height;
    // UV 平面：height/2 行，每行 uvStride 字节
    size_t uvSize = uvStride * (height / 2);
    fprintf(stderr, "Not implemented yet!\n");

    // {
    //     std::lock_guard<std::mutex> lock(put_mtx_);
    //     put_ready_ = true;
    //     width_ = width;
    //     height_ = height;
    //     buf_->resize(ySize + uvSize);
    //     std::memcpy(buf_->data(), yData, ySize);
    //     std::memcpy(buf_->data() + ySize, uvData, uvSize);
    // }
    // put_cv_.notify_one();
}

bool ScreenCapture::captureNV12(std::vector<uint8_t> &data, size_t &yStride, size_t &uvStride,
                                size_t &width, size_t &height) {
    assert(false); // not supported yet
    return true;
}

void ScreenCapture::_onBGRAFrame(const uint8_t *data, size_t bytesPerRow, size_t width,
                                 size_t height) {
    {
        // std::lock_guard<std::mutex> lock(mtx_);
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [] { return ready_ <= 0 || !running_; });
        if (!running_)
            return;
        ready_ = 1;
        width_ = width;
        height_ = height;
        buf_.resize(width * height * 4);
        for (size_t row = 0; row < height; row++) {
            std::memcpy(buf_.data() + row * width * 4, data + row * bytesPerRow, width * 4);
        }
    }
    cv_.notify_one();
}

bool ScreenCapture::captureBGRA(std::vector<uint8_t> &bgra, size_t &width, size_t &height) {

    if (!running_) {
        std::cerr << "Not running!\n";
        return false;
    }

    {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [] { return ready_ > 0 || !running_; });
        if (!running_)
            return false;
        ready_--;

        width = width_;
        height = height_;
        bgra = buf_;
    }
    cv_.notify_one();
    return true;
}
