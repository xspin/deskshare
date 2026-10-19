#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <cstddef>
#include <iostream>
#include <iterator>
#include <vector>

#include "ScreenCapture.hpp"

namespace impl {

float getScale() {
    static float scale = 0;
    if (scale > 0)
        return scale;
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());
    size_t real_width = CGDisplayModeGetPixelWidth(mode);
    size_t logical_width = CGDisplayModeGetWidth(mode);
    CGDisplayModeRelease(mode);
    scale = (float)real_width / logical_width;
    return scale;
}

std::pair<size_t, size_t> getCursorLoc() {
    CGEventRef event = CGEventCreate(nullptr);
    CGPoint pos = CGEventGetLocation(event);
    CFRelease(event);
    return {pos.x, pos.y};
}

std::pair<size_t, size_t> getScreenResolution() {
    CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    size_t width = CGRectGetWidth(bounds);
    size_t height = CGRectGetHeight(bounds);
    // ensure even size
    width += width & 1;
    height += height & 1;
    return {width, height};
}

std::pair<size_t, size_t> getScreenResolutionPhysical() {
    CGDisplayModeRef mode = CGDisplayCopyDisplayMode(CGMainDisplayID());

    // real pixel size (maybe equals logical pix times 2)
    size_t width = CGDisplayModeGetPixelWidth(mode);
    size_t height = CGDisplayModeGetPixelHeight(mode);

    // logical pixel size
    // size_t width = CGDisplayModeGetWidth(mode);
    // size_t height = CGDisplayModeGetHeight(mode);
    CGDisplayModeRelease(mode);

    // CGRect bounds = CGDisplayBounds(CGMainDisplayID());
    // size_t width = CGRectGetWidth(bounds);
    // size_t height = CGRectGetHeight(bounds);
    return {width, height};
}

// 截取指定显示器的全屏图像
static inline CGImageRef captureFullScreen(CGDirectDisplayID displayID) {
    // 直接从显示器帧缓冲区创建图像（无数据拷贝，高效）
    CGImageRef image = CGDisplayCreateImage(displayID);
    return image;
}

/*
// 截取指定显示器的矩形区域
static CGImageRef captureScreenRect(CGDirectDisplayID displayID, CGRect rect) {
    // 仅捕获指定区域，减少数据量（比全屏更高效）
    CGImageRef image = CGDisplayCreateImageForRect(displayID, rect);
    if (!image) {
        std::cerr << "Failed to capture screen rect" << std::endl;
    }
    return image;
}
*/

static bool CGImageToRGBA(std::vector<unsigned char> &rgba, CGImageRef image, size_t width,
                          size_t height) {
    static size_t w = 0;
    static size_t h = 0;
    static CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    static CGContextRef ctx = nullptr;

    size_t bytesPerRow = width * 4; // RGBA 每像素 4 字节
    rgba.resize(bytesPerRow * height);

    if (width != w || height != h) {
        w = width;
        h = height;

        if (ctx) {
            CGContextRelease(ctx);
        }
        ctx = CGBitmapContextCreate(
            rgba.data(), width, height,
            8, // 每通道 8 位
            bytesPerRow, colorSpace,
            kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big // RGBA ignore alpha
        );
    } else {
        CGContextClearRect(ctx, CGRectMake(0, 0, width, height));
    }

    // 把 CGImage 绘制进去，完成格式转换
    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);

    return true;
}

static bool CGImageToRGBAData(std::vector<unsigned char> &rgba, CGImageRef image, size_t width,
                              size_t height) {
    size_t bytesPerRow = width * 4; // RGBA 每像素 4 字节

    // 创建 RGBA 色彩空间
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // 分配缓冲区
    rgba.resize(bytesPerRow * height);

    // 创建位图上下文，强制指定为 RGBA 格式
    CGContextRef ctx = CGBitmapContextCreate(
        rgba.data(), width, height,
        8, // 每通道 8 位
        bytesPerRow, colorSpace,
        kCGImageAlphaNoneSkipLast | kCGBitmapByteOrder32Big // RGBA ignore alpha
    );

    // 把 CGImage 绘制进去，完成格式转换
    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
    CGColorSpaceRelease(colorSpace);
    CGContextRelease(ctx);
    return true;
}

// 现在 pixelData 里就是明确的 RGBA 数据了
// 可以送入 x264 或做色彩转换
static bool CGImageToJPEGData(std::vector<unsigned char> &jpegData, CGImageRef image,
                              size_t logicalWidth, float quality) {
    if (!image)
        return false;

    // 1. 创建内存数据容器（CFMutableDataRef）
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (!data)
        return false;

    // 2. 创建 JPEG 输出目标（写入内存数据）
    CFStringRef jpegType = kUTTypeJPEG;
    // CFStringRef jpegType = UTTypeJPEG;
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, jpegType, 1, NULL);
    if (!destination) {
        CFRelease(data);
        return false;
    }

    // 3. 设置压缩质量
    CFMutableDictionaryRef options = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFNumberRef maxPix =
        CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &logicalWidth); // 通常以宽度为准
    CFNumberRef qualityNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &quality);
    CFDictionarySetValue(options, kCGImageDestinationLossyCompressionQuality, qualityNum);
    CFDictionarySetValue(options, kCGImageDestinationImageMaxPixelSize, maxPix);

    // 4. 添加图像并完成转换
    CGImageDestinationAddImage(destination, image, options);
    bool success = CGImageDestinationFinalize(destination);

    // 5. 将 CFDataRef 转换为 vector（便于 C++ 处理）
    if (success) {
        const unsigned char *bytes = CFDataGetBytePtr(data);
        CFIndex length = CFDataGetLength(data);
        jpegData.assign(bytes, bytes + length);
    }

    // 6. 释放资源
    CFRelease(qualityNum);
    CFRelease(maxPix);
    CFRelease(options);
    CFRelease(destination);
    CFRelease(data);

    return true;
}

bool captureJpeg(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality) {
    // todo select display id
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef) {
        // std::cerr << "Failed to capture screen" << std::endl;
        width = 0;
        height = 0;
        return false;
    }

    // width = CGImageGetWidth(imgRef);
    // height = CGImageGetHeight(imgRef);
    auto r = getScreenResolution();
    width = r.first;
    height = r.second;

    bool ret = CGImageToJPEGData(jpeg, imgRef, width, quality);
    CGImageRelease(imgRef);

    return ret;
}

CGImageRef BGRAToCGImage(std::vector<uint8_t> &bgra, size_t width, size_t height) {
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, bgra.data(), bgra.size(), NULL);

    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    CGImageRef cgImage =
        CGImageCreate(width, height,
                      8,         // bitsPerComponent
                      32,        // bitsPerPixel
                      width * 4, // bytesPerRow
                      colorSpace,
                      kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little, // BGRA
                      provider, NULL, false, kCGRenderingIntentDefault);

    CGColorSpaceRelease(colorSpace);
    CGDataProviderRelease(provider);
    return cgImage;
}

bool captureBgra(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
    auto &sc = ScreenCapture::instance();
    return sc.captureBGRA(rgba, width, height);
}

bool captureRgba(std::vector<unsigned char> &rgba, size_t &width, size_t &height) {
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef) {
        width = 0;
        height = 0;
        return false;
    }
    auto r = getScreenResolution();
    width = r.first;
    height = r.second;
    // width = CGImageGetWidth(imgRef); // real pix size
    // height = CGImageGetHeight(imgRef);
    bool ret = CGImageToRGBA(rgba, imgRef, width, height);
    CGImageRelease(imgRef);
    return ret;
}

bool captureRgbaScaled(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef)
        return false;
    bool ret = CGImageToRGBAData(rgba, imgRef, width, height);
    CGImageRelease(imgRef);
    return ret;
}

} // namespace impl
