#include <ApplicationServices/ApplicationServices.h>
#include <iostream>
#include "capturer_macos.h"

// 截取指定显示器的全屏图像
static CGImageRef captureFullScreen(CGDirectDisplayID displayID) {
    // 直接从显示器帧缓冲区创建图像（无数据拷贝，高效）
    CGImageRef image = CGDisplayCreateImage(displayID);
    if (!image) {
        // std::cerr << "Failed to capture full screen" << std::endl;
    }
    return image;
}

// 截取指定显示器的矩形区域
static CGImageRef captureScreenRect(CGDirectDisplayID displayID, CGRect rect) {
    // 仅捕获指定区域，减少数据量（比全屏更高效）
    CGImageRef image = CGDisplayCreateImageForRect(displayID, rect);
    if (!image) {
        std::cerr << "Failed to capture screen rect" << std::endl;
    }
    return image;
}

std::vector<unsigned char> CGImageToJPEGData(CGImageRef image, float quality) {
    std::vector<unsigned char> jpegData;
    if (!image) return jpegData;

    // 1. 创建内存数据容器（CFMutableDataRef）
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (!data) return jpegData;

    // 2. 创建 JPEG 输出目标（写入内存数据）
    CFStringRef jpegType = kUTTypeJPEG;
    // CFStringRef jpegType = UTTypeJPEG;
    CGImageDestinationRef destination = CGImageDestinationCreateWithData(
        data,
        jpegType,
        1, // 单张图像
        NULL
    );
    if (!destination) {
        CFRelease(data);
        return jpegData;
    }

    // 3. 设置压缩质量
    CFMutableDictionaryRef options = CFDictionaryCreateMutable(
        kCFAllocatorDefault,
        0,
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks
    );
    CFNumberRef qualityNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberFloatType, &quality);
    CFDictionarySetValue(options, kCGImageDestinationLossyCompressionQuality, qualityNum);
    CFRelease(qualityNum);

    // 4. 添加图像并完成转换
    CGImageDestinationAddImage(destination, image, options);
    bool success = CGImageDestinationFinalize(destination);

    // 5. 将 CFDataRef 转换为 vector（便于 C++ 处理）
    if (success) {
        const unsigned char* bytes = CFDataGetBytePtr(data);
        CFIndex length = CFDataGetLength(data);
        jpegData.assign(bytes, bytes + length);
    }

    // 6. 释放资源
    CFRelease(options);
    CFRelease(destination);
    CFRelease(data);

    return jpegData;
}

std::vector<unsigned char> captureScreen(size_t& width, size_t& height, float quality) {
    CGImageRef imgRef = captureFullScreen(kCGDirectMainDisplay);
    if (!imgRef) {
        // std::cerr << "Failed to capture screen" << std::endl;
        width = 0;
        height = 0;
        return {};
    }

    width = CGImageGetWidth(imgRef);
    height = CGImageGetHeight(imgRef);

    auto jpeg = CGImageToJPEGData(imgRef, quality);
    CGImageRelease(imgRef);

    return jpeg;
}
