//
//  AVCaptureManager.h
//  SlowMotionVideoRecorder
//  https://github.com/shu223/SlowMotionVideoRecorder
//
//  Created by shuichi on 12/17/13.
//  Copyright (c) 2013 Shuichi Tsutsumi. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#import "ViewController.h"
#import "HiLight_results.h"


@protocol AVCaptureManagerDelegate <NSObject>
- (void)didFinishRecordingToOutputFileAtURL:(NSURL *)outputFileURL
                                      error:(NSError *)error;
@end


@interface AVCaptureManager : NSObject<AVCaptureVideoDataOutputSampleBufferDelegate>


@property id<AVCaptureManagerDelegate> delegate;

@property (nonatomic, strong) AVCaptureSession *captureSession;
@property (nonatomic, strong) AVCaptureMovieFileOutput *fileOutput;
@property (nonatomic, strong) AVCaptureDeviceFormat *defaultFormat;
@property (nonatomic, strong) AVCaptureVideoPreviewLayer *previewLayer;
@property (nonatomic, retain) EAGLContext *context;

- (id)initWithPreviewView:(UIView *)previewView;
- (void)toggleContentsGravity;
- (void)resetFormat;
- (void)switchFormatWithDesiredFPS:(CGFloat)desiredFPS;
- (void)detectScreenSwitch;
- (HiLight_results*)didSampleBuffer:(CMSampleBufferRef)sampleBuffer :(AVCaptureVideoOrientation)FrameOrientation :(double)current_timestamp;
- (void) HiLight_start;
- (void) HiLight_end;

@end
