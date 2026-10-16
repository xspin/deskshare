// ScreenCapture.mm
#import <CoreServices/CoreServices.h>
#import <ImageIO/ImageIO.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <future>

#import "FrameProcessor.hpp"
#import "ScreenCaptureImpl.hpp"

// ---------- Objective-C 辅助类 ----------
@interface ScreenCaptureHelper : NSObject <SCStreamOutput>
@property(nonatomic, strong) SCStream *stream;
- (void)startCaptureWithCompletion:(void (^)(BOOL success, NSError *error))completion;
- (void)stopCapture;
@end

@implementation ScreenCaptureHelper

- (void)stream:(SCStream *)stream
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
                   ofType:(SCStreamOutputType)type {
    @autoreleasepool {
        if (type != SCStreamOutputTypeScreen)
            return;

        CVPixelBufferRef pixelBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
        if (!pixelBuffer)
            return;

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

            // width/height 用整体帧尺寸即可
            // 如果送给 x264，通常传 yPlane/uPlane/vPlane 和对应 stride
            FrameProcessor::onI420Frame_(yPlane, yStride, uPlane, uStride, vPlane, vStride, width,
                                         height);
        } else if (pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange ||
                   pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
            uint8_t *yPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 0);
            size_t yStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 0);
            uint8_t *uvPlane = (uint8_t *)CVPixelBufferGetBaseAddressOfPlane(pixelBuffer, 1);
            size_t uvStride = CVPixelBufferGetBytesPerRowOfPlane(pixelBuffer, 1);
            size_t width = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
            size_t height = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

            FrameProcessor::onNV12Frame_(yPlane, yStride, uvPlane, uvStride, width, height);
        } else if (pixelFormat == kCVPixelFormatType_32BGRA) {
            uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(pixelBuffer);
            size_t bytesPerRow = CVPixelBufferGetBytesPerRow(pixelBuffer);
            size_t width = CVPixelBufferGetWidth(pixelBuffer);
            size_t height = CVPixelBufferGetHeight(pixelBuffer);

            FrameProcessor::onBGRAFrame_(baseAddress, bytesPerRow, width, height);
        }

        CVPixelBufferUnlockBaseAddress(pixelBuffer, kCVPixelBufferLock_ReadOnly);
        FrameProcessor::wait();
    }
}

/* - (void)startCapture { */
- (void)startCaptureWithCompletion:(void (^)(BOOL, NSError *))completion {
    [SCShareableContent getShareableContentWithCompletionHandler:^(SCShareableContent *content,
                                                                   NSError *error) {
      if (error || !content.displays.count) {
          completion(
              NO,
              error
                  ?: [NSError
                         errorWithDomain:@"ScreenCapture"
                                    code:-1
                                userInfo:@{NSLocalizedDescriptionKey : @"No display available"}]);
          return;
      }

      SCDisplay *display = content.displays.firstObject;
      SCContentFilter *filter = [[SCContentFilter alloc] initWithDisplay:display
                                                        excludingWindows:@[]];

      SCStreamConfiguration *config = [[SCStreamConfiguration alloc] init];
      config.width = display.width;
      config.height = display.height;
      config.pixelFormat = kCVPixelFormatType_420YpCbCr8Planar;
      config.minimumFrameInterval = CMTimeMake(1, 60); // 60ps
      config.showsCursor = true;

      self.stream = [[SCStream alloc] initWithFilter:filter configuration:config delegate:nil];
      if (!self.stream) {
          NSLog(@"Failed to create SCStream");
          completion(
              NO, [NSError
                      errorWithDomain:@"ScreenCapture"
                                 code:-2
                             userInfo:@{NSLocalizedDescriptionKey : @"Failed to create SCStream"}]);
          return;
      }

      NSError *addError = nil;
      [self.stream addStreamOutput:self
                              type:SCStreamOutputTypeScreen
                sampleHandlerQueue:nil
                             error:&addError];
      if (addError) {
          NSLog(@"addStreamOutput error: %@", addError);
          self.stream = nil;
          completion(NO, addError);
          return;
      }

      NSLog(@"start stream: %@", self.stream);
      [self.stream startCaptureWithCompletionHandler:^(NSError *_Nullable error) {
        if (error) {
            self.stream = nil;
            NSLog(@"startCapture error: %@", error);
            completion(NO, error);
        } else {
            completion(YES, nil);
        }
      }];
    }];
}

- (void)stopCapture {
    if (self.stream) {
        [self.stream stopCaptureWithCompletionHandler:^(NSError *_Nullable error){
        }];
        self.stream = nil;
    }
}

@end

// ---------- C++ 桥接类 ----------
struct ScreenCaptureImpl::Impl {
    ScreenCaptureHelper *helper = nil;
};

ScreenCaptureImpl::ScreenCaptureImpl() : impl_(new Impl()) {
}

ScreenCaptureImpl::~ScreenCaptureImpl() {
    stop();
    delete impl_;
}

bool ScreenCaptureImpl::start() {
    impl_->helper = [[ScreenCaptureHelper alloc] init];

    auto promise = std::make_shared<std::promise<bool>>();
    std::future<bool> future = promise->get_future();

    [impl_->helper startCaptureWithCompletion:^(BOOL ok, NSError *error) {
      promise->set_value(ok);
    }];

    return future.get(); // 阻塞等待
}

/* bool ScreenCaptureImpl::start() { */
/*     dispatch_semaphore_t sem = dispatch_semaphore_create(0); */
/*     __block BOOL success = NO; */
/**/
/*     impl_->helper = [[ScreenCaptureHelper alloc] init]; */
/*     [impl_->helper startCaptureWithCompletion:^(BOOL ok, NSError *error) { */
/*       success = ok; */
/*       if (!ok) { */
/*           NSLog(@"startCapture failed: %@", error); */
/*       } */
/*       dispatch_semaphore_signal(sem); // 唤醒等待 */
/*     }]; */
/**/
/*     dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER); */
/**/
/*     return success; */
/* } */

void ScreenCaptureImpl::stop() {
    if (impl_->helper) {
        [impl_->helper stopCapture]; // 由 helper 停止
        impl_->helper = nil;
    }
}

// capture() 删掉，或改成调用 startCapture
