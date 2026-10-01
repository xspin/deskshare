
#include "capture/capturer.h"
#include <chrono>
#include <gtest/gtest.h>
#include <vector>

TEST(performance, captureRgba) {
    Capturer &cap = Capturer::getInstance();
    std::vector<unsigned char> img;
    auto [width, height] = cap.getResolution();

    printf("testing for captureRgba with resolution %lu x %lu\n", width, height);
    for (float scale = 1; scale > 0; scale -= 0.3) {
        size_t w = width * scale;
        size_t h = height * scale;
        cap.captureRgba(img, w, h);
        printf("testing for scale %.2f and size: %lu x %lu\n", scale, w, h);
        int frames = 100;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < frames; i++) {
            cap.captureRgba(img, w, h);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        float fps = frames * 1000.0 / duration;
        printf("   capture rgba fps: %.2f\n", fps);
    }
}
