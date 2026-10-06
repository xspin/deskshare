
#include "capture/capturer.h"
#include <chrono>
#include <gtest/gtest.h>
#include <vector>

TEST(performance, capture) {

    Capturer &cap = Capturer::getInstance();

    std::vector<unsigned char> jpg;
    size_t w, h;
    cap.captureJpg(jpg, w, h, 0.1);
    for (float q = 0.2; q <= 1; q += 0.2) {
        auto start = std::chrono::high_resolution_clock::now();
        int frames = 100;
        fprintf(stdout, "testing for quality %.2f\n", q);
        fflush(stdout);
        for (int i = 0; i < frames; i++) {
            cap.captureJpg(jpg, w, h, q);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        float fps = frames * 1000.0 / duration;
        fprintf(stdout, "    quality: %.0f%%  fps: %.2f\n", q * 100, fps);
        fflush(stdout);
    }
}
