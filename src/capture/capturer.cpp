#include <cassert>
#include "capturer.h"

#ifdef PLATFORM_MACOS
#include "capturer_macos.hpp"
#elif defined(PLATFORM_WINDOWS)
#include "capturer_win.hpp"
#endif

bool Capturer::capture(float quality) {
    assert(0 < quality && quality <= 1);
    jpg = captureScreen(width, height, quality);
    return !jpg.empty();
}