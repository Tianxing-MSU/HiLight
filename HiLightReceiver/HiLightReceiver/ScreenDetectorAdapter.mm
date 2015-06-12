//
//  ScreenDetectorAdapter.m
//  HiLightReceiver
//
//  Created by Tianxing Li on 8/4/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#import "ScreenDetectorAdapter.h"
#include "ScreenDetector.h"

using namespace cv;

@implementation ScreenDetectorAdapter

- (id) init {
    if (self = [super init]) {
        testObj1 = new class ScreenDetector();
    }
    
    return self;
}

- (void) dealloc {
    if (testObj1 != NULL) {
        delete testObj1;
        testObj1 = NULL;
    }
}

-(bool) processFrame:(cv::Mat&) inputFrame into:(cv::Point&)Top_left into:(cv::Point&)Top_right into:(cv::Point&)Bottom_left into:(cv::Point&)Bottom_right {
    bool check = testObj1->processFrame(inputFrame, Top_left, Top_right, Bottom_left, Bottom_right);
    return check;
}

-(void) Addrect:(cv::Mat&) inputFrame :(cv::Point)Top_left :(cv::Point)Top_right :(cv::Point)Bottom_left :(cv::Point)Bottom_right{
    testObj1->Addrect(inputFrame, Top_left, Top_right, Bottom_left, Bottom_right);
}

@end