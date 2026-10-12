#pragma once

#include <functional>
#include <vector>
#include <x264.h>

class H264Encoder {
  public:
    using packetHandler = std::function<void(const uint8_t *data, size_t size)>;

    H264Encoder();
    ~H264Encoder();

    bool init(size_t width, size_t height, float quality, int fps);
    std::pair<int, bool> encode(const std::vector<uint8_t> &rgba, bool key_frame = false,
                                bool flush = false);

    void onPackets(packetHandler hd) {
        pktHandle_ = std::move(hd);
    }

  private:
    bool rgbaToI420(const std::vector<uint8_t> &rgba);
    void release();

    size_t width_;
    size_t height_;
    std::vector<uint8_t> i420_buffer_;
    x264_picture_t pic_in_;
    x264_t *encoder_;
    // size_t frame_idx_;
    packetHandler pktHandle_;
    // std::chrono::steady_clock::time_point start_time_;
};
