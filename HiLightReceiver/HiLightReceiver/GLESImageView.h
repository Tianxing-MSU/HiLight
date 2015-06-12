//
//  GLESImageView.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#import <UIKit/UIKit.h>

@interface GLESImageView : UIView

- (void)drawFrame:(const cv::Mat&) bgraFrame;


@end
