#include "capture/capturer.h"
#include "h264/h264.hpp"
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <vector>

TEST(encoder, h264) {
    auto &cap = Capturer::instance();
    auto [width, height] = cap.getResolution();
    std::vector<unsigned char> rgba;

    std::cout << "size: " << width << " x " << height << std::endl;

    ASSERT_GT(width, 0);
    ASSERT_GT(height, 0);

    std::string path = "test.h264";
    std::ofstream of(path, std::ios::binary);

    H264Encoder encoder;

    encoder.onPackets([&of](const void *data, size_t size) {
        // std::cout << "  write: " << data << " " << size << "bytes" << std::endl;
        of.write((const char *)data, size);
    });

    bool ret = encoder.init(width, height, 0.6, 30);
    ASSERT_TRUE(ret);

    printf("\nTesting captureRgba with H264 encode %lu x %lu...\n", width, height);
    auto start = std::chrono::high_resolution_clock::now();
    int frames = 50;
    for (int i = 0; i < frames; i++) {
        size_t w, h;
        cap.captureRgba(rgba, w, h);
        auto [fs, kf] = encoder.encode(rgba);
        fprintf(stdout, "\r  frame: %d, %d bytes, %lux%lu, key: %d     ", i, fs, w, h, kf);
        fflush(stdout);
    }
    encoder.encode(rgba, false, true);

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    float fps = frames * 1000.0 / duration;
    printf(" \nfps: %.2f\n", fps);
    printf("Saved to %s\n", path.c_str());

    ASSERT_TRUE(ret);

    printf("\nTesting captureRgba without H264 encode %lu x %lu...\n", width, height);
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < frames; i++) {
        cap.captureRgba(rgba);
        std::cout << "\r  frame: " << i << std::flush;
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    fps = frames * 1000.0 / duration;
    printf(" \nwithout h264 encode fps: %.2f\n", fps);

    of.close();
}
