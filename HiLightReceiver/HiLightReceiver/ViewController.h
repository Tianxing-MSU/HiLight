//
//  ViewController.h
//  SlowMotionVideoRecorder
//
//  Created by shuichi on 12/17/13.
//  Copyright (c) 2013 Shuichi Tsutsumi. All rights reserved.
//

#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import "AVCaptureManager.h"

@interface ViewController : UIViewController <AVCaptureVideoDataOutputSampleBufferDelegate>

@property (nonatomic) float fps;
@property (nonatomic, strong) AVCaptureVideoDataOutput *captureOutput;
@property (nonatomic, strong) UILabel *fpsLabel;

@end
