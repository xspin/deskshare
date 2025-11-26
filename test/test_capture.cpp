#include <gtest/gtest.h>
#include "capture/capturer.h"


TEST(cpature, Capturer) {
    auto [w, h] = Capturer::getResolution();
    EXPECT_GT(w, 0);
    EXPECT_GT(h, 0);

    auto [x, y] = Capturer::getCursorPos();
    EXPECT_LT(x, w);
    EXPECT_LT(y, h);
    
    Capturer cap;
    cap.capture(0.01);
    EXPECT_GT(cap.jpg.size(), 0);
    EXPECT_GT(cap.width, 0);
    EXPECT_GT(cap.height, 0);

    cap.capture(1);
    EXPECT_GT(cap.jpg.size(), 0);
    EXPECT_GT(cap.width, 0);
    EXPECT_GT(cap.height, 0);
}