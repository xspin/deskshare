#ifndef CAPTURE_H
#define CAPTURE_H

#include <string>
#include <vector>
#include <cassert>
#include "../platform.h"

#ifdef PLATFORM_MACOS
#include "capturer_macos.h"
#else
#include "capturer_win.h"
#endif

class Capturer {

public:
    Capturer() {}
    ~Capturer() {}

    bool capture(float quality) {
        assert(0 < quality && quality < 1);
        jpg = captureScreen(width, height, quality);
        return !jpg.empty();
    }

    std::vector<unsigned char> jpg;
    size_t width;
    size_t height;
};

#endif //CAPTURE_H