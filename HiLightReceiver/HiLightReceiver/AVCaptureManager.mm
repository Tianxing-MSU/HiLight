//
//  AVCaptureManager.mm
//  SlowMotionVideoRecorder
//  https://github.com/shu223/SlowMotionVideoRecorder
//
//  Created by shuichi on 12/17/13.
//  Copyright (c) 2013 Shuichi Tsutsumi. All rights reserved.
//


#import "AVCaptureManager.h"
#import "HiLight_Receiver.h"
#import "HiLightAdapter.h"
#import "GLESImageView.h"
#import "ScreenDetector.h"
#import "ScreenDetectorAdapter.h"

using namespace cv;

@interface AVCaptureManager ()
{
    CMTime defaultVideoMaxFrameDuration;
    bool screen_detector_start;
    bool screen_detected;
    bool hilight_start;
    HiLightAdapter *hiLightAdapter;
    ScreenDetectorAdapter *screenDetectorAdapter;
    cv::Mat outputFrame;
    CGFloat current_FPS_setting;
    
    // The OpenGL ES names for the framebuffer and renderbuffer used to render to this view.
    GLuint defaultFramebuffer, colorRenderbuffer;
    GLuint backgroundTextureId;
    GLint framebufferWidth, framebufferHeight;
    
    cv::Point Top_left;
    cv::Point Top_right;
    cv::Point Bottom_left;
    cv::Point Bottom_right;
    char output_text[12];
    bool output_bits_check;
    bool denoise_check;
    bool first_bit_check;
    int screen_position_tmp[9];
}

@property (nonatomic, strong) GLESImageView *imageView;
@property (nonatomic, strong) HiLight_results *results;
@end

@implementation AVCaptureManager
@synthesize context;
@synthesize results;

- (id)initWithPreviewView:(UIView *)previewView {
    
    self = [super init];
    current_FPS_setting = 30;
    screen_detector_start = false;
    screen_detected = false;
    
    if (self) {
        
        NSError *error;
        
        // Create the session
        self.captureSession = [[AVCaptureSession alloc] init];
        // Configure the session to produce lower resolution video frames, if your processing algorithm can cope.
        self.captureSession.sessionPreset = AVCaptureSessionPreset1280x720;
        
        // Find a suitable AVCaptureDevice
        AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        // Create a device input with the device and add it to the session.
        AVCaptureDeviceInput *videoIn = [AVCaptureDeviceInput deviceInputWithDevice:videoDevice error:&error];
        hiLightAdapter = [[HiLightAdapter alloc] init];
        screenDetectorAdapter = [[ScreenDetectorAdapter alloc] init];
        
        //Create HiLight results struct
        self.results = [[HiLight_results alloc] init];
        
        if (error) {
            NSLog(@"Video input creation failed");
            return nil;
        }
        
        if (![self.captureSession canAddInput:videoIn]) {
            NSLog(@"Video input add-to-session failed");
            return nil;
        }
        [self.captureSession addInput:videoIn];
        
        // save the default format
        self.defaultFormat = videoDevice.activeFormat;
        defaultVideoMaxFrameDuration = videoDevice.activeVideoMaxFrameDuration;
        
        //default preview layer commented
        self.previewLayer = [[AVCaptureVideoPreviewLayer alloc] initWithSession:self.captureSession];
        self.previewLayer.frame = previewView.bounds;
        self.previewLayer.contentsGravity = kCAGravityResizeAspectFill;
        self.previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
        [previewView.layer insertSublayer:self.previewLayer atIndex:0];
        
        _imageView = [[GLESImageView alloc] initWithFrame:previewView.bounds];
        [_imageView setAutoresizingMask:(UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight)];
        [previewView addSubview:_imageView];
        [previewView sendSubviewToBack:_imageView];
    }
    return self;
}

// =============================================================================
#pragma mark - Public

- (void)toggleContentsGravity {
    
    if ([self.previewLayer.videoGravity isEqualToString:AVLayerVideoGravityResizeAspectFill]) {
        
        self.previewLayer.videoGravity = AVLayerVideoGravityResizeAspect;
    }
    else {
        self.previewLayer.videoGravity = AVLayerVideoGravityResizeAspectFill;
    }
}

- (void)resetFormat {
    
    BOOL isRunning = self.captureSession.isRunning;
    
    if (isRunning) {
        [self.captureSession stopRunning];
    }
    
    AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    [videoDevice lockForConfiguration:nil];
    videoDevice.activeFormat = self.defaultFormat;
    videoDevice.activeVideoMaxFrameDuration = defaultVideoMaxFrameDuration;
    [videoDevice unlockForConfiguration];
    
    if (isRunning) {
        [self.captureSession startRunning];
    }
}

- (void)switchFormatWithDesiredFPS:(CGFloat)desiredFPS
{
    BOOL isRunning = self.captureSession.isRunning;
    
    if (isRunning)  [self.captureSession stopRunning];
    
    AVCaptureDevice *videoDevice = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    AVCaptureDeviceFormat *selectedFormat = nil;
    int32_t maxWidth = 0;
    AVFrameRateRange *frameRateRange = nil;
    
    for (AVCaptureDeviceFormat *format in [videoDevice formats]) {
        
        for (AVFrameRateRange *range in format.videoSupportedFrameRateRanges) {
            
            CMFormatDescriptionRef desc = format.formatDescription;
            CMVideoDimensions dimensions = CMVideoFormatDescriptionGetDimensions(desc);
            int32_t width = dimensions.width;
            
            if (range.minFrameRate <= desiredFPS && desiredFPS <= range.maxFrameRate && width >= maxWidth) {
                
                selectedFormat = format;
                frameRateRange = range;
                maxWidth = width;
            }
        }
    }
    
    if (selectedFormat) {
        
        if ([videoDevice lockForConfiguration:nil]) {
            
            if (desiredFPS == 30){
                if ([videoDevice isFocusModeSupported:AVCaptureFocusModeAutoFocus]) {
                    videoDevice.focusMode = AVCaptureFocusModeAutoFocus;
                    NSLog(@"Focus unlocked");
                }
            }else{
                if ([videoDevice isFocusModeSupported:AVCaptureFocusModeLocked]) {
                    videoDevice.focusMode = AVCaptureFocusModeLocked;
                    NSLog(@"Focus locked");
                }
            }
            
            
            NSLog(@"selected format:%@", selectedFormat);
            videoDevice.activeFormat = selectedFormat;
            videoDevice.activeVideoMinFrameDuration = CMTimeMake(1, (int32_t)desiredFPS);
            videoDevice.activeVideoMaxFrameDuration = CMTimeMake(1, (int32_t)desiredFPS);
            [videoDevice unlockForConfiguration];
        }
    }
    
    current_FPS_setting = desiredFPS;
    
    if (isRunning) [self.captureSession startRunning];
}

// Delegate routine that is called when a sample buffer was written
- (HiLight_results*)didSampleBuffer:(CMSampleBufferRef)sampleBuffer :(AVCaptureVideoOrientation)FrameOrientation :(double)current_timestamp
{
    // init screen position flag
    self.results->screen_position[0] = 0;
    self.results->first_bit_check = false;
    self.results->denoise_check = false;
    for(int i = 0; i < 36+1; i++){
        self.results->hilight_results[i] = -1;
    }
    
    output_bits_check = false;
    // Create a UIImage from the sample buffer data
    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    
    // Lock the image buffer
    CVPixelBufferLockBaseAddress(imageBuffer,0);
    
    // Get information about the image
    uint8_t *baseAddress = (uint8_t *)CVPixelBufferGetBaseAddress(imageBuffer);
    size_t width = CVPixelBufferGetWidth(imageBuffer);
    size_t height = CVPixelBufferGetHeight(imageBuffer);
    size_t stride = CVPixelBufferGetBytesPerRow(imageBuffer);
    cv::Mat frame(static_cast<int>(height), static_cast<int>(width), CV_8UC4, (void*)baseAddress, stride);
    
    if (FrameOrientation == AVCaptureVideoOrientationLandscapeLeft)
    {
        cv::flip(frame, frame, 0);
    }
    
    //HiLight only works on 60 FPS frame rate
    if (current_FPS_setting == 60 && hilight_start && screen_detected){
        //assign cropped screen position
        screen_position_tmp[1] = Top_left.x;
        screen_position_tmp[2] = Top_left.y;
        screen_position_tmp[3] = Top_right.x;
        screen_position_tmp[4] = Top_right.y;
        screen_position_tmp[5] = Bottom_right.x;
        screen_position_tmp[6] = Bottom_right.y;
        screen_position_tmp[7] = Bottom_left.x;
        screen_position_tmp[8] = Bottom_left.y;
        
        //HiLight receiver
        output_bits_check = [hiLightAdapter processFrame:frame :screen_position_tmp :current_timestamp into:output_text into:&denoise_check into:&first_bit_check into:self.results->hilight_line_index into:self.results->hilight_results];
        self.results->first_bit_check = first_bit_check;
        self.results->denoise_check = denoise_check;
    }
    
    if (screen_detector_start) {
        //HiLight screen detector
        screen_detected = [screenDetectorAdapter processFrame:frame into:Top_left into:Top_right into:Bottom_left into:Bottom_right];
        if(screen_detected){
            self.results->screen_position[0] = 1;
            self.results->screen_position[1] = Top_left.x;
            self.results->screen_position[2] = Top_left.y;
            self.results->screen_position[3] = Top_right.x;
            self.results->screen_position[4] = Top_right.y;
            self.results->screen_position[5] = Bottom_right.x;
            self.results->screen_position[6] = Bottom_right.y;
            self.results->screen_position[7] = Bottom_left.x;
            self.results->screen_position[8] = Bottom_left.y;
            
            screen_position_tmp[0] = 1;
            screen_position_tmp[1] = Top_left.x;
            screen_position_tmp[2] = Top_left.y;
            screen_position_tmp[3] = Top_right.x;
            screen_position_tmp[4] = Top_right.y;
            screen_position_tmp[5] = Bottom_right.x;
            screen_position_tmp[6] = Bottom_right.y;
            screen_position_tmp[7] = Bottom_left.x;
            screen_position_tmp[8] = Bottom_left.y;
        }
        screen_detector_start = false;
    }
    frame.release();
    
    if(output_bits_check){
        self.results.output_check = true;
        self.results.output_text = [NSString stringWithCString:output_text encoding:NSASCIIStringEncoding];
    }else{
        self.results->output_check = false;
    }
    
    // We unlock the  image buffer
    CVPixelBufferUnlockBaseAddress(imageBuffer,0);
    return self.results;
}

- (void) detectScreenSwitch{
    screen_detector_start = true;
}

- (void) HiLight_start{
    hilight_start = true;
}

- (void) HiLight_end{
    hilight_start = false;
    [hiLightAdapter reset];
}

@end

