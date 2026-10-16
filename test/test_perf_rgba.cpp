
#include "capture/capturer.h"
#include <chrono>
#include <gtest/gtest.h>
#include <vector>

TEST(performance, captureRgba) {
    Capturer &cap = Capturer::instance();
    std::vector<unsigned char> img;
    auto [width, height] = cap.getResolution();

    printf("\ntesting for mocked rgba with size: %lu x %lu\n", width, height);
    int frames = 60;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < frames; i++) {
        cap.captureRgbaMock(img, width, height);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    float fps = frames * 1000.0 / duration;
    printf("   capture rgba mock fps: %.2f\n", fps);

    printf("\ntesting for captureRgba with resolution %lu x %lu\n", width, height);
    for (float scale = 1; scale > 0; scale -= 0.3) {
        size_t w = width * scale;
        size_t h = height * scale;
        cap.captureRgbaScaled(img, w, h);
        printf("testing for scale %.2f and size: %lu x %lu\n", scale, w, h);
        start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < frames; i++) {
            cap.captureRgbaScaled(img, w, h);
        }
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        float fps = frames * 1000.0 / duration;
        printf("   capture rgba fps: %.2f\n", fps);
    }
}
