#include "ScreenCapture.hpp"
#include "FrameProcessor.hpp"

bool ScreenCapture::init() {
    started_ = impl_.start();
    return started_;
}

void ScreenCapture::captureI420Async(I420Callback cb) {
    if (!started_) {
        init();
    }
    if (started_) {
        FrameProcessor::onI420Frame = std::move(cb);
        FrameProcessor::notify();
    }
}
