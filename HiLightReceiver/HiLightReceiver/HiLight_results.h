//
//  HiLight_results.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 8/11/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

// HiLight results data structure
@interface HiLight_results : NSObject{
@public
    NSInteger screen_position[9];
    NSString *output_text;
    bool denoise_check;
    bool first_bit_check;
    bool output_check;
    
    int hilight_line_index[1];
    char hilight_results[36];
}

@property (nonatomic, strong) NSString *output_text;
@property (nonatomic) bool output_check;

@end