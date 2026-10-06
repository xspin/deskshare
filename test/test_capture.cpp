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
    Capturer &cap = Capturer::getInstance();
    cap.captureJpg(jpg, width, height, 0.01);
    EXPECT_GT(jpg.size(), 0);
    EXPECT_GT(width, 0);
    EXPECT_GT(height, 0);

    cap.captureJpg(jpg, width, height, 1);
    EXPECT_GT(jpg.size(), 0);
    EXPECT_GT(width, 0);
    EXPECT_GT(height, 0);
}
