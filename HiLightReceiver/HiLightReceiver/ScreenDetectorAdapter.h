//
//  ScreenDetectorAdapter.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 8/4/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#import <Foundation/Foundation.h>
#include "ScreenDetector.h"

using namespace cv;

Class ScreenDetector;

@interface ScreenDetectorAdapter : NSObject{
    class ScreenDetector *testObj1;
}

- (bool) processFrame:(cv::Mat&) inputFrame into:(cv::Point&)Top_left into:(cv::Point&)Top_right into:(cv::Point&)Bottom_left into:(cv::Point&)Bottom_right;
- (void) Addrect:(cv::Mat&) inputFrame :(cv::Point)Top_left :(cv::Point)Top_right :(cv::Point)Bottom_left :(cv::Point)Bottom_right;
@end