#include "capture/capturer.h"
#include <gtest/gtest.h>
#include <vector>

TEST(cpature, Capturer) {
    auto [w, h] = Capturer::getResolution();
    EXPECT_GT(w, 0);
    EXPECT_GT(h, 0);

    auto [x, y] = Capturer::getCursorPos();
    EXPECT_LT(x, w);
    EXPECT_LT(y, h);

    size_t width, height;

    std::vector<unsigned char> jpg;
    Capturer &cap = Capturer::instance();
    cap.captureJpg(jpg, width, height, 0.01);
    EXPECT_GT(jpg.size(), 0);
    EXPECT_EQ(width, w);
    EXPECT_EQ(height, h);

    cap.captureJpg(jpg, width, height, 1);
    EXPECT_GT(jpg.size(), 0);
    EXPECT_EQ(width, w);
    EXPECT_EQ(height, h);

    std::vector<unsigned char> rgba;
    cap.captureRgba(jpg, width, height);
    EXPECT_EQ(width, w);
    EXPECT_EQ(height, h);
}

/*
TEST(cpature, async) {
    auto &cap = Capturer::instance();
    size_t fid = 0;
    printf("Testing captureI420Async...\n");
    while (fid < 100) {
        cap.captureI420Async([&fid](const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane,
                                    size_t uStride, const uint8_t *vPlane, size_t vStride,
                                    size_t width, size_t height) {
            printf("[%lu] %lu x %lu\n", fid++, width, height);
            printf("y %p %lu\n", yPlane, yStride);
            printf("u %p %lu\n", uPlane, uStride);
            printf("v %p %lu\n", vPlane, vStride);
        });
    }
}
*/
