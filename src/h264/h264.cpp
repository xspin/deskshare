#include <iostream>

#include "h264.hpp"

extern "C" {
#include <libavutil/opt.h>
}

H264Encoder::H264Encoder() {
}

H264Encoder::~H264Encoder() {
    sws_freeContext(sws_ctx_);
    avcodec_free_context(&codec_ctx_);
    av_packet_free(&pkt_);
    av_frame_free(&frame_);
}

bool H264Encoder::init(size_t width, size_t height, int fps) {
    assert(width > 0 && height > 0 && fps > 0);

    this->width_ = width;
    this->height_ = height;
    this->idx_ = 0;

    sws_ctx_ = sws_getContext(width, height, AV_PIX_FMT_RGBA,    // 输入：RGBA
                              width, height, AV_PIX_FMT_YUV420P, // 输出：YUV420P
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) {
        std::cerr << "sws_getContext failed" << std::endl;
        return false;
    }

    const AVCodec *codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        std::cerr << "find_encoder of libx264 failed" << std::endl;
        return false;
    }
    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) {
        std::cerr << "avcodec_alloc_context3 failed" << std::endl;
        return false;
    }

    codec_ctx_->width = width;
    codec_ctx_->height = height;
    codec_ctx_->time_base = (AVRational){1, fps};
    codec_ctx_->framerate = (AVRational){fps, 1};
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->gop_size = 30;
    codec_ctx_->bit_rate = 400000;

    // 视觉无损 / 存档	17 - 18	与原始画面几乎无差别，文件较大
    // 高质量分发 (推荐)	20 - 23	画质优秀，文件大小合理，适合大多数场景
    // 平衡画质与体积	24 - 28	画质可接受，文件明显减小，适合网络传输
    // 低质量 / 预览	30+	画质损失明显，仅用于快速预览
    if (av_opt_set(codec_ctx_->priv_data, "crf", "23", 0)) {
        std::cerr << "av_opt_set crf failed" << std::endl;
        return false;
    }

    if (av_opt_set(codec_ctx_->priv_data, "preset", "medium", 0)) {
        std::cerr << "av_opt_set preset failed" << std::endl;
        return false;
    }

    if (avcodec_open2(codec_ctx_, codec, nullptr)) {
        std::cerr << "avcodec_open2 failed" << std::endl;
        return false;
    }

    pkt_ = av_packet_alloc();
    if (!pkt_) {
        std::cerr << "av_packet_alloc failed" << std::endl;
        return false;
    }

    frame_ = av_frame_alloc();
    if (!frame_) {
        std::cerr << "av_frame_alloc failed" << std::endl;
        return false;
    }
    frame_->format = AV_PIX_FMT_YUV420P;
    frame_->width = width;
    frame_->height = height;
    av_frame_get_buffer(frame_, 32);

    return true;
}

void H264Encoder::encode(uint8_t *rgba, bool key_frame) {
    const uint8_t *src_data[1] = {rgba}; // RGBA 是单平面交错
    int src_linesize[1] = {(int)width_ * 4};

    if (key_frame) {
        frame_->pict_type = AV_PICTURE_TYPE_I;
        frame_->flags |= AV_FRAME_FLAG_KEY;
    } else {
        frame_->pict_type = AV_PICTURE_TYPE_NONE;
        frame_->flags &= ~AV_FRAME_FLAG_KEY;
    }

    // rgba to YUV420P frame
    sws_scale(sws_ctx_, src_data, src_linesize, 0, height_, frame_->data, frame_->linesize);

    frame_->pts = idx_++;
    avcodec_send_frame(codec_ctx_, frame_);

    while (avcodec_receive_packet(codec_ctx_, pkt_) >= 0) {
        pktHandle_(pkt_->data, pkt_->size);
        av_packet_unref(pkt_);
    }
}
