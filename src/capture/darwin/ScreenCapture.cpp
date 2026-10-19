#include "ScreenCapture.hpp"
#include <cstddef>
#include <mutex>
#include <vector>

ScreenCapture::~ScreenCapture() {
    stopped_ = true;
}

void ScreenCapture::stop() {
    impl_.stop();
    stopped_ = true;
    get_cv_.notify_one();
    put_cv_.notify_one();
}

bool ScreenCapture::init() {
    if (!started_) {
        started_ = impl_.start();
    }
    return started_;
}

void ScreenCapture::_setScale(float scale) {
    scale_ = scale;
}

float ScreenCapture::getScale() {
    return scale_;
}

bool ScreenCapture::_onFrameStart() {
    std::unique_lock<std::mutex> lock(get_mtx_);
    get_cv_.wait(lock, [] { return get_ready_ || stopped_; });
    get_ready_ = false;
    return true;
}

void ScreenCapture::_onFrameEnd() {
}

void ScreenCapture::_onI420Frame(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                                 size_t uStride, const uint8_t *vPlane, size_t vStride,
                                 size_t width, size_t height) {
    // if (onI420Frame_) {
    // onI420Frame_(yPlane, yStride, uPlane, uStride, vPlane, vStride, width, height);
    // }
    fprintf(stderr, "Not implemented yet!\n");
    assert(false);
}

void ScreenCapture::_onNV12Frame(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                                 size_t uvStride, size_t width, size_t height) {
    // Y 平面：height 行，每行 yStride 字节
    size_t ySize = yStride * height;
    // UV 平面：height/2 行，每行 uvStride 字节
    size_t uvSize = uvStride * (height / 2);

    {
        std::lock_guard<std::mutex> lock(put_mtx_);
        put_ready_ = true;
        width_ = width;
        height_ = height;
        buf_->resize(ySize + uvSize);
        std::memcpy(buf_->data(), yData, ySize);
        std::memcpy(buf_->data() + ySize, uvData, uvSize);
    }
    put_cv_.notify_one();
}

bool ScreenCapture::captureNV12(std::vector<uint8_t> &data, size_t &yStride, size_t &uvStride,
                                size_t &width, size_t &height) {
    if (!init()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(get_mtx_);
        get_ready_ = true;
        buf_ = &data;
    }
    get_cv_.notify_one();

    {
        std::unique_lock<std::mutex> lock(put_mtx_);
        put_cv_.wait(lock, [] { return put_ready_ || stopped_; });
        put_ready_ = false;

        width = width_;
        height = height_;
    }
    return true;
}

void ScreenCapture::_onBGRAFrame(const uint8_t *data, size_t bytesPerRow, size_t width,
                                 size_t height) {
    {
        std::lock_guard<std::mutex> lock(put_mtx_);
        put_ready_ = true;
        // 示例：逐行拷贝到紧凑缓冲区
        buf_->resize(width * height * 4);
        for (size_t row = 0; row < height; row++) {
            std::memcpy(buf_->data() + row * width * 4, data + row * bytesPerRow, width * 4);
        }
        width_ = width;
        height_ = height;
        // onBGRAFrame_(bgra.data(), width, height);
    }
    put_cv_.notify_one();
}

bool ScreenCapture::captureBGRA(std::vector<uint8_t> &bgra, size_t &width, size_t &height) {

    if (!init()) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(get_mtx_);
        get_ready_ = true;
        buf_ = &bgra;
    }
    get_cv_.notify_one();

    {
        std::unique_lock<std::mutex> lock(put_mtx_);
        put_cv_.wait(lock, [] { return put_ready_ || stopped_; });
        put_ready_ = false;

        width = width_;
        height = height_;
    }
    return true;
}
