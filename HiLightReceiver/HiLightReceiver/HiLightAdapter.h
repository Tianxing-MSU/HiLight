//
//  EdgeDetectorAdapter.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#import <Foundation/Foundation.h>
#include "HiLight_Receiver.h"

@interface HiLightAdapter : NSObject{
    class HiLight *testObj;
}

- (void) reset;

- (bool) processFrame:(const cv::Mat&) inputFrame :(int*)screen_position :(double)current_timestamp into:(char*) results into:(bool*)denoise_check into:(bool*)first_bit_check into:(int*)hilight_line_index into:(char*)hilight_results;
@end
