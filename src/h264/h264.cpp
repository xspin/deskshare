#include "h264.hpp"
#include <cassert>
#include <cstring>
#include <iostream>
#include <libyuv.h>
#include <sys/types.h>

H264Encoder::H264Encoder() : encoder_(nullptr) {
}

H264Encoder::~H264Encoder() {
    release();
}

void H264Encoder::release() {
    if (encoder_) {
        x264_encoder_close(encoder_);
        encoder_ = nullptr;
    }
}

bool H264Encoder::init(size_t width, size_t height, float quality, int fps) {
    if (x264_param_default_preset(&param_, "ultrafast", "zerolatency")) {
        std::cerr << "Failed to preset x264 param\n";
        return false;
    }
    width_ = width;
    height_ = height;
    // make size even
    // width = width + (width & 1);
    // height = height + (height & 1);
    assert(width % 2 == 0 && height % 2 == 0);

    assert(quality >= 0 && quality <= 1);

    // 视觉无损 / 存档	17 - 18	与原始画面几乎无差别，文件较大 (0.64~0.66)
    // 高质量分发 (推荐)	20 - 23	画质优秀，文件大小合理，适合大多数场景 (0.54~0.6)
    // 平衡画质与体积	24 - 28	画质可接受，文件明显减小，适合网络传输 (0.44~0.52)
    // 低质量 / 预览	30+	画质损失明显，仅用于快速预览 (~0.4)
    // 0~51  default:23   quality: 0~1

    float crf = (1 - quality) * 50;
    param_.rc.f_rf_constant = crf < 1 ? 1 : crf;
    param_.rc.i_rc_method = X264_RC_CRF;

    // 设置 VBV 限制，防止码率失控
    param_.rc.i_vbv_max_bitrate = 21000; // 你允许的最大瞬时码率（kbps），根据网络带宽设定
    param_.rc.i_vbv_buffer_size = 21000; // 缓冲区大小，通常设为与 max_bitrate 相等或略大

    param_.i_bframe = 0;
    param_.i_threads = 1;
    param_.i_scenecut_threshold = 0;
    param_.i_width = width;
    param_.i_height = height;
    param_.b_annexb = 1;         // 输出 Annex B 格式的起始码，方便流式传输
    param_.b_repeat_headers = 1; // 关键帧前重复 SPS/PPS，确保接收端能随时解码
    param_.i_keyint_max = fps;   // 通常设为帧率的 1-2 倍
    param_.i_keyint_min = fps;
    param_.i_fps_num = fps;
    param_.i_fps_den = 1;

    // param_.b_vfr_input = 1;
    // param_.i_timebase_num = 1;
    // param_.i_timebase_den = 1000;

    if (x264_param_apply_profile(&param_, "baseline")) {
        std::cerr << "Failed to apply x264 profile\n";
        return false;
    }

    release();

    encoder_ = x264_encoder_open(&param_);
    if (!encoder_) {
        std::cerr << "Failed to open x264 encoder\n";
        return false;
    }

    // if (x264_picture_alloc(&pic_in_, X264_CSP_I420, width, height)) {
    //     std::cerr << "Failed to alloc x264 picture\n";
    //     return false;
    // }

    x264_picture_init(&pic_in_);

    return true;
}

bool H264Encoder::bgraToI420(const std::vector<uint8_t> &bgra) {
    size_t width = param_.i_width;
    size_t height = param_.i_height;

    static std::vector<uint8_t> y_plane_, u_plane_, v_plane_;

    size_t y_size = width * height;
    size_t uv_size = width * height / 4;
    y_plane_.resize(y_size);
    u_plane_.resize(uv_size);
    v_plane_.resize(uv_size);

    pic_in_.img.i_csp = X264_CSP_I420;
    pic_in_.img.i_plane = 3;
    pic_in_.img.i_stride[0] = width;
    pic_in_.img.i_stride[1] = width / 2;
    pic_in_.img.i_stride[2] = width / 2;
    pic_in_.img.plane[0] = y_plane_.data();
    pic_in_.img.plane[1] = u_plane_.data();
    pic_in_.img.plane[2] = v_plane_.data();

    uint8_t *y_plane = pic_in_.img.plane[0];
    uint8_t *u_plane = pic_in_.img.plane[1];
    uint8_t *v_plane = pic_in_.img.plane[2];

    // I420 各 plane 的 stride = 对应宽度
    size_t stride = width / 2;
    int ret = libyuv::ARGBToI420(bgra.data(), width * 4, // src, src_stride
                                 y_plane, width,         // dst_y, dst_stride_y
                                 u_plane, stride,        // dst_u, dst_stride_u
                                 v_plane, stride,        // dst_v, dst_stride_v
                                 width, height           // width, height
    );

    if (ret) {
        std::cerr << "Failed to ARGBToI420\n";
        return false;
    }

    return true;
}

std::pair<int, bool> H264Encoder::encode(bool key_frame) {

    x264_picture_t pic_out;
    x264_nal_t *nals = nullptr;
    int i_nals = 0;
    int frame_size;

    if (key_frame) {
        pic_in_.i_type = X264_TYPE_IDR; // 强制为 IDR 帧，确保后续帧不参考此帧之前的数据
    } else {
        pic_in_.i_type = X264_TYPE_AUTO; // 恢复自动决策
    }

    bool kf = false;
    if ((frame_size = x264_encoder_encode(encoder_, &nals, &i_nals, &pic_in_, &pic_out)) > 0) {
        kf = pic_out.b_keyframe != 0;
        for (int i = 0; i < i_nals; i++) {
            pktHandle_(nals[i].p_payload, nals[i].i_payload);
        }
    }
    return {frame_size, kf};
}

std::pair<int, bool> H264Encoder::encodeBGRA(const std::vector<uint8_t> &bgra, bool key_frame) {
    if (!bgraToI420(bgra))
        return {-1, false};
    return encode(key_frame);
}

std::pair<int, bool> H264Encoder::encodeNV12(const uint8_t *ydata, size_t y_stride,
                                             const uint8_t *uvdata, size_t uv_stride,
                                             bool key_frame) {
    pic_in_.img.i_csp = X264_CSP_NV12;
    pic_in_.img.i_plane = 2; // NV12 只有 2 个平面

    // 平面 0：Y
    pic_in_.img.plane[0] = (uint8_t *)ydata;
    pic_in_.img.i_stride[0] = y_stride;

    // 平面 1：UV（交错，不要拆）
    pic_in_.img.plane[1] = (uint8_t *)uvdata;
    pic_in_.img.i_stride[1] = uv_stride;
    return encode(key_frame);
}
