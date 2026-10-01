#include <ApplicationServices/ApplicationServices.h>
#include <CoreGraphics/CoreGraphics.h>
#include <iostream>
#include <string>
#include <vector>

namespace impl {

// 截取指定显示器的全屏图像
static CGImageRef captureFullScreen(CGDirectDisplayID displayID) {
    // 直接从显示器帧缓冲区创建图像（无数据拷贝，高效）
    CGImageRef image = CGDisplayCreateImage(displayID);
    if (!image) {
        // std::cerr << "Failed to capture full screen" << std::endl;
    }
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

static bool CGImageToRGBAData(std::vector<unsigned char> &rgba, CGImageRef image, size_t width,
                              size_t height) {
    // 假设 image 是一个 CGImageRef
    // size_t width = CGImageGetWidth(image);
    // size_t height = CGImageGetHeight(image);
    size_t bytesPerRow = width * 4; // RGBA 每像素 4 字节

    // 创建 RGBA 色彩空间
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    // 分配缓冲区
    rgba.resize(bytesPerRow * height);

    // 创建位图上下文，强制指定为 RGBA 格式
    CGContextRef ctx = CGBitmapContextCreate(rgba.data(), width, height,
                                             8, // 每通道 8 位
                                             bytesPerRow, colorSpace,
                                             kCGImageAlphaPremultipliedLast // RGBA
    );

    // 把 CGImage 绘制进去，完成格式转换
    CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
    CGContextRelease(ctx);
    return true;
}

// 现在 pixelData 里就是明确的 RGBA 数据了
// 可以送入 x264 或做色彩转换
static bool CGImageToJPEGData(std::vector<unsigned char> &jpegData, CGImageRef image,
                              float quality) {
    if (!image)
        return false;

    // 1. 创建内存数据容器（CFMutableDataRef）
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (!data)
        return false;

    // 2. 创建 JPEG 输出目标（写入内存数据）
    CFStringRef jpegType = kUTTypeJPEG;
    // CFStringRef jpegType = UTTypeJPEG;
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(data, jpegType,
                                                                         1, // 单张图像
                                                                         NULL);
    if (!destination) {
        CFRelease(data);
        return false;
    }

    // 3. 设置压缩质量
    CFMutableDictionaryRef options = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFNumberRef qualityNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &quality);
    CFDictionarySetValue(options, kCGImageDestinationLossyCompressionQuality, qualityNum);
    CFRelease(qualityNum);

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
    CFRelease(options);
    CFRelease(destination);
    CFRelease(data);

    return true;
}

static void listDisplays() {
    static bool listed = false;
    if (listed)
        return;

    listed = true;
    std::cout << "Main Display ID: " << CGMainDisplayID() << std::endl;
    std::cout << "Direct Main: " << kCGDirectMainDisplay << std::endl;
    CGDirectDisplayID displays[32];
    uint32_t count = 0;
    CGGetActiveDisplayList(32, displays, &count);

    // displays[0] 是主显示器，displays[1..count-1] 是其他显示器
    for (uint32_t i = 0; i < count; i++) {
        printf("Display %u: %u\n", i, displays[i]);
    }
}

bool captureScreen(std::vector<unsigned char> &jpeg, size_t &width, size_t &height, float quality) {
    // todo select display id
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef) {
        // std::cerr << "Failed to capture screen" << std::endl;
        width = 0;
        height = 0;
        return false;
    }
    listDisplays();

    width = CGImageGetWidth(imgRef);
    height = CGImageGetHeight(imgRef);

    bool ret = CGImageToJPEGData(jpeg, imgRef, quality);
    CGImageRelease(imgRef);

    return ret;
}

bool captureRgba(std::vector<unsigned char> &rgba, size_t width, size_t height) {
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef)
        return false;
    bool ret = CGImageToRGBAData(rgba, imgRef, width, height);
    CGImageRelease(imgRef);
    return ret;
}

std::pair<size_t, size_t> getScreenResolution() {
    static size_t width = 0;
    static size_t height = 0;
    if (width == 0) {
        CGDirectDisplayID main_display = CGMainDisplayID();
        CGRect bounds = CGDisplayBounds(main_display);
        width = CGRectGetWidth(bounds);
        height = CGRectGetHeight(bounds);
    }
    return {width, height};
}

std::pair<size_t, size_t> getCursorLoc() {
    CGEventRef event = CGEventCreate(nullptr);
    CGPoint pos = CGEventGetLocation(event);
    CFRelease(event);
    return {pos.x, pos.y};
}
} // namespace impl
