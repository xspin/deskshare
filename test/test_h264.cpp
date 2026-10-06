#include "capture/capturer.h"
#include "h264/h264.hpp"
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <vector>

TEST(encoder, h264) {
    auto &cap = Capturer::getInstance();
    auto [width, height] = cap.getResolution();
    std::vector<unsigned char> rgba;

    std::cout << "size: " << width << " x " << height << std::endl;

    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    std::ofstream of("test.h264", std::ios::binary);

    H264Encoder encoder;

    encoder.onPackets([&of](const void *data, size_t size) { of.write((const char *)data, size); });

    bool ret = encoder.init(width, height, 20);
    ASSERT_TRUE(ret);

    auto start = std::chrono::high_resolution_clock::now();
    int frames = 100;
    for (int i = 0; i < frames; i++) {
        std::cout << "\r frame: " << i << std::flush;
        cap.captureRgba(rgba, width, height);
        encoder.encode(rgba.data());
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    float fps = frames * 1000.0 / duration;
    printf("  fps: %.2f\n", fps);

    of.close();
}
