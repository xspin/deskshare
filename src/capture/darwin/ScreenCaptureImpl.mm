// ScreenCapture.mm
#import <AVFoundation/AVFoundation.h>
#import <AppKit/NSScreen.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#include <future>

#import "ScreenCapture.hpp"
#import "ScreenCaptureImpl.hpp"

// ---------- Objective-C 辅助类 ----------
@interface ScreenCaptureHelper : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, strong) AVCaptureSession *session;
@property(nonatomic, strong) AVCaptureScreenInput *screenInput;
@property(nonatomic, strong) AVCaptureVideoDataOutput *videoOutput;
@property(nonatomic, strong) id activityToken;
- (BOOL)startCapture;
- (void)stopCapture;
@end

@implementation ScreenCaptureHelper

// 帧回调（AVFoundation 的代理方法）
- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
           fromConnection:(AVCaptureConnection *)connection {
    @autoreleasepool {
        if (!ScreenCapture::_onFrameStart())
            return;
        CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!pixelBuffer) {
            ScreenCapture::_onFrameEnd();
            return;
        }
        CVPixelBufferLockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);

        OSType pixelFormat = CVPixelBufferGetPixelFormatType(pixelBuffer);

        if (pixelFormat == kCVPixelFormatType_420YpCbCr8Planar ||
            pixelFormat == kCVPixelFormatType_420YpCbCr8PlanarFullRange) {
            size_t width = CVPixelBufferGetWidth(pixelBuffer);
            size_t height = CVPixelBufferGetHeight(pixelBuffer);

            uint8_t *yPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
            size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
            uint8_t *uPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
            size_t uStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
            uint8_t *vPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 2);
            size_t vStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 2);

            ScreenCapture::_onI420Frame(yPlane, yStride, uPlane, uStride, vPlane, vStride, width,
                                        height);
        } else if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
                   pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
            uint8_t *yPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
            size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
            uint8_t *uvPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
            size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
            size_t width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
            size_t height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

            ScreenCapture::_onNV12Frame(yPlane, yStride, uvPlane, uvStride, width, height);
        } else if (pixelFormat == kCVPixelFormatType_32BGRA) {
            uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(pixelBuffer);
            size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
            size_t width = CVPixelBufferGetWidth(pixelBuffer);
            size_t height = CVPixelBufferGetHeight(pixelBuffer);

            ScreenCapture::_onBGRAFrame(baseAddress, bytesPerRow, width, height);
        } else {
            NSLog(@"Unkown pixelFormat: 0x%08X", pixelFormat);
        }

        ScreenCapture::_onFrameEnd();
        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
    }
}

- (BOOL)startCapture {
    self.session = [[AVCaptureSession alloc] init];

    // 1. 创建屏幕输入
    CGDirectDisplayID displayId = CGMainDisplayID();
    self.screenInput = [[AVCaptureScreenInput alloc] initWithDisplayID:displayId];
    if (!self.screenInput) {
        NSLog(@"Failed to create AVCaptureScreenInput");
        return NO;
    }

    CGFloat scale = [[NSScreen mainScreen] backingScaleFactor];
    ScreenCapture::_setScale(scale);

    self.screenInput.minFrameDuration = CMTimeMake(1, 60);
    self.screenInput.capturesCursor = YES;

    if (![self.session canAddInput:self.screenInput]) {
        NSLog(@"Cannot add screen input");
        return NO;
    }
    [self.session addInput:self.screenInput];

    // 2. 创建视频数据输出
    self.videoOutput = [[AVCaptureVideoDataOutput alloc] init];
    self.videoOutput.alwaysDiscardsLateVideoFrames = YES;
    /* NSLog(@"Supported formats: %@", self.videoOutput.availableVideoCVPixelFormatTypes); */
    /* OSType fmt = kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange; // NV12 */
    OSType fmt = kCVPixelFormatType_32BGRA;
    self.videoOutput.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey : @(fmt)};

    dispatch_queue_t queue = dispatch_queue_create("com.deskshare.capture", DISPATCH_QUEUE_SERIAL);
    [self.videoOutput setSampleBufferDelegate:self queue:queue];

    if (![self.session canAddOutput:self.videoOutput]) {
        NSLog(@"Cannot add video output");
        return NO;
    }
    [self.session addOutput:self.videoOutput];

    // 3. 启动
    [self.session startRunning];
    NSLog(@"AVCaptureScreenInput started");

    self.activityToken = [[NSProcessInfo processInfo]
        beginActivityWithOptions:(NSActivityUserInitiated | NSActivityIdleSystemSleepDisabled)
                          reason:@"Screen Capturing"];
    return YES;
}

- (void)stopCapture {
    if (self.session) {
        [self.session stopRunning];
        self.session = nil;
        self.screenInput = nil;
        self.videoOutput = nil;
        NSLog(@"AVCaptureScreenInput stopped");
    }
    if (self.activityToken) {
        [[NSProcessInfo processInfo] endActivity:self.activityToken];
        self.activityToken = nil;
    }
}

@end

// ---------- C++ 桥接类 ----------
struct ScreenCaptureImpl::Impl {
    __strong ScreenCaptureHelper *helper = nil;
};

ScreenCaptureImpl::ScreenCaptureImpl() : impl_(new Impl()) {
}

ScreenCaptureImpl::~ScreenCaptureImpl() {
    stop();
    delete impl_;
}

bool ScreenCaptureImpl::start() {
    if (impl_->helper) {
        NSLog(@"%@ already started", impl_->helper);
        return true;
    }
    impl_->helper = [[ScreenCaptureHelper alloc] init];

    auto promise = std::make_shared<std::promise<bool>>();
    std::future<bool> future = promise->get_future();

    ScreenCaptureHelper *strongHelper = impl_->helper;

    // AVCaptureSession 的启动是相对同步的，但仍放到后台避免阻塞
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
      BOOL ok = [strongHelper startCapture];
      promise->set_value(ok);
    });

    return future.get();
}

void ScreenCaptureImpl::stop() {
    if (impl_->helper) {
        NSLog(@"stopCapture");
        [impl_->helper stopCapture];
        impl_->helper = nil;
    }
}
