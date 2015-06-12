//
//  EdgeDetectorAdapter.m
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#import "HiLightAdapter.h"
#include "HiLight_Receiver.h"

@implementation HiLightAdapter

- (id) init {
    if (self = [super init]) {
        testObj = new class HiLight();
    }
    
    return self;
}

- (void) dealloc {
    if (testObj != NULL) {
        delete testObj;
        testObj = NULL;
    }
}

- (void) reset{
    testObj->reset();
}

-(bool) processFrame:(const cv::Mat&) inputFrame :(int*)screen_position :(double)current_timestamp into:(char*) results into:(bool*)denoise_check into:(bool*)first_bit_check into:(int*)hilight_line_index into:(char*) hilight_results{
    bool check = testObj->processFrame(inputFrame, screen_position, current_timestamp, results, denoise_check, first_bit_check, hilight_line_index, hilight_results);
    return check;
}

@end
