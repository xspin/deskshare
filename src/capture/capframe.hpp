#pragma once

#include <cstdint>
#include <functional>

using NV12Callback = std::function<void(const uint8_t *yData, size_t yStride, const uint8_t *uvData,
                                        size_t uvStride, size_t width, size_t height)>;

using I420Callback =
    std::function<void(const uint8_t *yPlane, size_t yStride, const uint8_t *uPlane, size_t uStride,
                       const uint8_t *vPlane, size_t vStride, size_t width, size_t height)>;

using BGRACallback = std::function<void(const uint8_t *data, size_t width, size_t height)>;
