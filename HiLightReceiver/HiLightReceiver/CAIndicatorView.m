//
//  CAIndicatorView.m
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//


#import "CAIndicatorView.h"

@interface CAIndicatorView()
{
    float resize_ratio_x;
    float resize_ratio_y;
}
@property (nonatomic, assign) NSTimer *screen_detector_timer;
@end

@implementation CAIndicatorView

- (id)initWithFrame:(CGRect)frame {
    if (self = [super initWithFrame:frame]) {
        // Initialization code
    }
    return self;
}

-(void)setScreen: (int*) points{
    screen_detected = true;
    
    // This number only for iPhone 5s
    resize_ratio_x = 568.0/1280;
    resize_ratio_y = 320.0/720;
    
    screen_points[0] = points[1] * resize_ratio_x;
    screen_points[1] = points[2] * resize_ratio_y;
    screen_points[2] = points[3] * resize_ratio_x;
    screen_points[3] = points[4] * resize_ratio_y;
    screen_points[4] = points[5] * resize_ratio_x;
    screen_points[5] = points[6] * resize_ratio_y;
    screen_points[6] = points[7] * resize_ratio_x;
    screen_points[7] = points[8] * resize_ratio_y;
}


- (void)drawRect:(CGRect)rect {
    [super drawRect:rect];
	
	// Get the graphics context and clear it
	CGContextRef ctx = UIGraphicsGetCurrentContext();
	CGContextClearRect(ctx, rect);
	
	// Draw a green rectangle
    if (screen_detected){
        CGContextSetRGBStrokeColor(ctx, 0, 255, 0, 1);
        CGPoint points[8] = { CGPointMake(screen_points[0], screen_points[1]), CGPointMake(screen_points[2], screen_points[3]),
            CGPointMake(screen_points[2], screen_points[3]), CGPointMake(screen_points[4], screen_points[5]),
            CGPointMake(screen_points[4], screen_points[5]), CGPointMake(screen_points[6], screen_points[7]),
            CGPointMake(screen_points[6], screen_points[7]), CGPointMake(screen_points[0], screen_points[1])};
        CGContextSetLineWidth(ctx, 5);
        CGContextStrokeLineSegments(ctx, points, 8);
    }
}

@end