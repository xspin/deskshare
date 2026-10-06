#pragma once

#include <functional>
#include <memory>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class H264Encoder {
  public:
    using packetHandler = std::function<void(const void *data, size_t size)>;

    H264Encoder();
    ~H264Encoder();

    bool init(size_t width, size_t height, int fps);
    void encode(uint8_t *rgba, bool key_frame = false);

    void onPackets(packetHandler hd) {
        pktHandle_ = hd;
    }

  private:
    size_t width_;
    size_t height_;
    SwsContext *sws_ctx_;
    AVCodecContext *codec_ctx_;
    AVPacket *pkt_;
    AVFrame *frame_;
    int idx_;

    packetHandler pktHandle_;
};
