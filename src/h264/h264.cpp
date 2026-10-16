#include "h264.hpp"
#include <cassert>
#include <cstring>
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
    if (x264_param_default_preset(&param_, "ultrafast", "zerolatency")) {
        std::cerr << "Failed to preset x264 param\n";
        return false;
    }
    width_ = width;
    height_ = height;
    // make size even
    width = width + (width & 1);
    height = height + (height & 1);
    // assert(width % 2 == 0 && height % 2 == 0);

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

    if (x264_picture_alloc(&pic_in_, X264_CSP_I420, width, height)) {
        std::cerr << "Failed to alloc x264 picture\n";
        return false;
    }

    // i420_buffer_.resize(width * height * 3 / 2);
    // frame_idx_ = 0;

    // start_time_ = std::chrono::steady_clock::now();
    return true;
}

bool H264Encoder::rgbaToI420(const std::vector<uint8_t> &rgba) {
    // 计算各 plane 的指针
    size_t width = param_.i_width;
    size_t height = param_.i_height;

    // uint8_t *y_plane = i420_buffer_.data();
    // uint8_t *u_plane = y_plane + width * height;
    // uint8_t *v_plane = u_plane + width * height / 4;
    uint8_t *y_plane = pic_in_.img.plane[0];
    uint8_t *u_plane = pic_in_.img.plane[1];
    uint8_t *v_plane = pic_in_.img.plane[2];

    int ret;
    if (width_ & 1 || height_ & 1) {
        int ySize = width_ * height_; // 实际大小(奇数)
        int yWidth = width_;
        int uvWidth = width / 2;
        int uvHeight = height / 2;
        int uvSize = uvWidth * uvHeight;

        static std::vector<uint8_t> yBuffer;
        static std::vector<uint8_t> uBuffer;
        static std::vector<uint8_t> vBuffer;

        yBuffer.resize(ySize);
        uBuffer.resize(uvSize);
        vBuffer.resize(uvSize);

        // 注意：stride 参数需要根据实际内存布局填写，这里假设是紧凑的
        ret = libyuv::ABGRToI420(rgba.data(), width_ * 4, // 源数据及 stride
                                 yBuffer.data(), yWidth,  // Y 平面及 stride
                                 uBuffer.data(), uvWidth, // U 平面及 stride（使用向上取整后的宽度）
                                 vBuffer.data(), uvWidth, // V 平面及 stride
                                 width_, height_          // 实际宽高（奇数）
        );
        if (ret == 0) {
            ret = libyuv::I420Scale(yBuffer.data(), yWidth, // 源 Y 平面
                                    uBuffer.data(),
                                    uvWidth,                 // stride 用向上取偶的宽度
                                    vBuffer.data(), uvWidth, // 源 V 平面
                                    width_, height_,         // 源尺寸（奇数）
                                    y_plane, width,          // 目标 Y 平面（偶数尺寸）
                                    u_plane, width / 2,      // 目标 U 平面
                                    v_plane, width / 2,      // 目标 V 平面
                                    width, height,           // 目标尺寸（偶数）
                                    libyuv::kFilterNone      // 过滤模式
            );
        }

    } else {
        // I420 各 plane 的 stride = 对应宽度
        size_t stride = width / 2;
        ret = libyuv::ABGRToI420(rgba.data(), width * 4, // src, src_stride
                                 y_plane, width,         // dst_y, dst_stride_y
                                 u_plane, stride,        // dst_u, dst_stride_u
                                 v_plane, stride,        // dst_v, dst_stride_v
                                 width, height           // width, height
        );
    }

    if (ret) {
        std::cerr << "Failed to ABGRToI420\n";
        return false;
    }
    // -------- 填充 x264 输入图像 --------
    // 将 libyuv 转换结果拷贝到 x264 的 picture 结构中
    // 注意：实际生产代码可直接操作 pic_in.img.plane 避免额外拷贝
    // memcpy(pic_in_.img.plane[0], y_plane, width * height);
    // memcpy(pic_in_.img.plane[1], u_plane, width * height / 4);
    // memcpy(pic_in_.img.plane[2], v_plane, width * height / 4);
    return true;
}

std::pair<int, bool> H264Encoder::encode(const std::vector<uint8_t> &rgba, bool key_frame,
                                         bool flush) {
    if (!rgbaToI420(rgba))
        return {-1, false};

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
