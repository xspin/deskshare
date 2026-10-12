#include "h264.hpp"
#include <cassert>
#include <iostream>
#include <libyuv.h>

H264Encoder::H264Encoder() : encoder_(nullptr) {
    memset(&pic_in_, 0, sizeof(pic_in_));
}

H264Encoder::~H264Encoder() {
    release();
}

void H264Encoder::release() {
    if (pic_in_.img.plane[0]) {
        x264_picture_clean(&pic_in_);
        memset(&pic_in_, 0, sizeof(pic_in_));
    }
    if (encoder_) {
        x264_encoder_close(encoder_);
        encoder_ = nullptr;
    }
}

bool H264Encoder::init(size_t width, size_t height, float quality, int fps) {
    x264_param_t param;
    if (x264_param_default_preset(&param, "ultrafast", "zerolatency")) {
        std::cerr << "Failed to preset x264 param\n";
        return false;
    }

    assert(quality >= 0 && quality <= 1);

    // 视觉无损 / 存档	17 - 18	与原始画面几乎无差别，文件较大 (0.64~0.66)
    // 高质量分发 (推荐)	20 - 23	画质优秀，文件大小合理，适合大多数场景 (0.54~0.6)
    // 平衡画质与体积	24 - 28	画质可接受，文件明显减小，适合网络传输 (0.44~0.52)
    // 低质量 / 预览	30+	画质损失明显，仅用于快速预览 (~0.4)
    // 0~51  default:23   quality: 0~1

    float crf = (1 - quality) * 50;
    param.rc.f_rf_constant = crf < 1 ? 1 : crf;
    param.rc.i_rc_method = X264_RC_CRF;

    // 设置 VBV 限制，防止码率失控
    param.rc.i_vbv_max_bitrate = 21000; // 你允许的最大瞬时码率（kbps），根据网络带宽设定
    param.rc.i_vbv_buffer_size = 21000; // 缓冲区大小，通常设为与 max_bitrate 相等或略大

    param.i_bframe = 0;
    param.i_threads = 1;
    param.i_scenecut_threshold = 0;
    param.i_width = width;
    param.i_height = height;
    param.b_annexb = 1;         // 输出 Annex B 格式的起始码，方便流式传输
    param.b_repeat_headers = 1; // 关键帧前重复 SPS/PPS，确保接收端能随时解码
    param.i_keyint_max = fps;   // 通常设为帧率的 1-2 倍
    param.i_keyint_min = fps;
    param.i_fps_num = fps;
    param.i_fps_den = 1;

    // param.b_vfr_input = 1;
    // param.i_timebase_num = 1;
    // param.i_timebase_den = 1000;

    if (x264_param_apply_profile(&param, "baseline")) {
        std::cerr << "Failed to apply x264 profile\n";
        return false;
    }

    release();

    encoder_ = x264_encoder_open(&param);
    if (!encoder_) {
        std::cerr << "Failed to open x264 encoder\n";
        return false;
    }

    if (x264_picture_alloc(&pic_in_, X264_CSP_I420, width, height)) {
        std::cerr << "Failed to alloc x264 picture\n";
        return false;
    }

    width_ = width;
    height_ = height;
    i420_buffer_.resize(width * height * 3 / 2);
    // frame_idx_ = 0;

    // start_time_ = std::chrono::steady_clock::now();
    return true;
}

bool H264Encoder::rgbaToI420(const std::vector<uint8_t> &rgba) {
    // 计算各 plane 的指针
    size_t width = width_;
    size_t height = height_;
    uint8_t *y_plane = i420_buffer_.data();
    uint8_t *u_plane = y_plane + width * height;
    uint8_t *v_plane = u_plane + width * height / 4;

    // I420 各 plane 的 stride = 对应宽度
    size_t stride = (width + 1) / 2;
    int ret = libyuv::ABGRToI420(rgba.data(), width * 4, // src, src_stride
                                 y_plane, width,         // dst_y, dst_stride_y
                                 u_plane, stride,        // dst_u, dst_stride_u
                                 v_plane, stride,        // dst_v, dst_stride_v
                                 width, height           // width, height
    );

    if (ret) {
        std::cerr << "Failed to ABGRToI420\n";
        return false;
    }
    // -------- 填充 x264 输入图像 --------
    // 将 libyuv 转换结果拷贝到 x264 的 picture 结构中
    // 注意：实际生产代码可直接操作 pic_in.img.plane 避免额外拷贝
    memcpy(pic_in_.img.plane[0], y_plane, width * height);
    memcpy(pic_in_.img.plane[1], u_plane, width * height / 4);
    memcpy(pic_in_.img.plane[2], v_plane, width * height / 4);
    return true;
}

std::pair<int, bool> H264Encoder::encode(const std::vector<uint8_t> &rgba, bool key_frame,
                                         bool flush) {
    if (!rgbaToI420(rgba))
        return {-1, false};

    // auto now = std::chrono::steady_clock::now();
    // auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now -
    // start_time_).count(); pic_in_.i_pts = elapsed;

    x264_picture_t pic_out;
    x264_nal_t *nals = nullptr;
    int i_nals = 0;
    int frame_size;

    if (key_frame) {
        pic_in_.i_type = X264_TYPE_IDR; // 强制为 IDR 帧，确保后续帧不参考此帧之前的数据
    } else {
        pic_in_.i_type = X264_TYPE_AUTO; // 恢复自动决策
    }

    auto p = flush ? nullptr : &pic_in_;

    bool kf = false;
    if ((frame_size = x264_encoder_encode(encoder_, &nals, &i_nals, p, &pic_out)) > 0) {
        kf = pic_out.b_keyframe != 0;
        for (int i = 0; i < i_nals; i++) {
            pktHandle_(nals[i].p_payload, nals[i].i_payload);
        }
    }
    return {frame_size, kf};
}
