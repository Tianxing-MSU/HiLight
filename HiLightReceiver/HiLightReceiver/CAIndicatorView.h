//
//  CAIndicatorView.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//


#import <Foundation/Foundation.h>
#import <QuartzCore/QuartzCore.h>

@interface CAIndicatorView : UIView

@property (nonatomic, assign) BOOL on;
-(void)setScreen: (int*) points;

@end

bool screen_detected;
int screen_points[8];