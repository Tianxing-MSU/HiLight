//
//  HiLight.cpp
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#include <stdio.h>
#include <algorithm>
#include <Accelerate/Accelerate.h>
#include <ctime>
#include <math.h>
#include <sys/time.h>
#include "HiLight_Receiver.h"
#include "Matrix.h"

HiLight::HiLight(){}

void getGray_HiLight(const cv::Mat& input, cv::Mat& gray)
{
    const int numChannes = input.channels();
    
    if (numChannes == 4)
    {
        cv::cvtColor(input, gray, CV_BGRA2GRAY);
    }
    else if (numChannes == 3)
    {
        cv::cvtColor(input, gray, CV_BGR2GRAY);
    }
    else if (numChannes == 1)
    {
        gray = input;
    }
}

const int LOG_N = HiLight::LOG_N; // Typically this would be at least 10 (i.e. 1024pt FFTs)
const int N = 1 << LOG_N;
const float PI = 4*atan(1);
const int first_bit_window = N;
const int object_pulse_2 = ceil(N/3);
const int object_pulse_threashold = 1;
const int grid_x = HiLight::grid_x;
const int grid_y = HiLight::grid_y;
const int grid_margin = 5;
const int MVDR_frame = HiLight::MVDR_frame;
const int grid_x_MVDR = HiLight::grid_x_MVDR;
const int grid_y_MVDR = HiLight::grid_y_MVDR;
const int output_results_length = HiLight::output_results_length;
const int repeat_time = 3; //Data package retransmission times. Should be matched with the TX settings
const int encode_length = 6; // Each letter is encoded into a 6 bits long stream, using ASCII code. The current HiLight only support 52 English letters (w and w/o capitalize) , 10 numbers from 0-9, blank, and underline symbol.
const int coding_rate = encode_length * repeat_time;
const int output_bit_stck_length = output_results_length * coding_rate;

bool initialized = false;
int window_index = 0;
bool first_bit_1_detected = false;
bool first_bit_2_detected = false;
bool first_bit_detected = false;
int first_bit_index = 0;
int hilight_stack_index = 0;
int hilight_first_bit_index = 0;
int hilight_first_bit_counter = 0;
int hilight_first_bit_position = 0;
double start_time;
double current_time;
bool denoise_start = false;
int MVDR_index = 0;
int bit_counter = 0;
int results_stack_counter = 0;
bool start_receiving = false;
int counter_after_detect_1;
bool isImage = false;
bool isDebug = false;
bool isCalibrate = false;
bool MVDR;
bool demo = true;
bool demo_video = false;

float first_bit_ratio = 0.4;
float first_2_ratio;
float first_bit_voting;

DSPSplitComplex tempSplitComplex;
FFTSetup fftSetup;

int dist_top;
cv::Point image_ROI_position[2];
cv::Point grids_position_video[grid_x][grid_y][2];
cv::Point grids_position_image[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR][2];
cv::Point2f grids_points_top_image[grid_x * grid_x_MVDR + 1];
cv::Point2f grids_points_bottom_image[grid_x * grid_x_MVDR + 1];
cv::Point2f grids_points_left_image[grid_y  * grid_y_MVDR + 1];
cv::Point2f grids_points_right_image[grid_y * grid_y_MVDR +1];
cv::Point2f grids_points_top_video[grid_x + 1];
cv::Point2f grids_points_bottom_video[grid_x + 1];
cv::Point2f grids_points_left_video[grid_y + 1];
cv::Point2f grids_points_right_video[grid_y +1];
cv::Point2f srcTri[3];
cv::Point2f dstTri[3];
cv::Mat rot_mat( 2, 3, CV_32FC1 );
cv::Mat warp_mat( 2, 3, CV_32FC1 );
double transmitter_x = 1920*1.0;
double transmitter_y = 1080*1.0;
double transmitter_screen_ratio = transmitter_y/transmitter_x;

// Set up a bunch of buffers
// Buffers for real (time-domain) input and output signals.
float grid_color_intensity[grid_x][grid_y][N];
int first_bit_color_window[grid_x][grid_y][first_bit_window];
int hilight_results[grid_x][grid_y];
int hilight_results_stack[grid_x][grid_y][N];
int hilight_first_bit_stack[grid_x][grid_y][N*5];//init
double hilight_first_bit_timestamp[N*5];//init
float brightness[grid_x][grid_y];
float grid_color_intensity_MVDR[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR][MVDR_frame];
float MVDR_weighting[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR];
int output_bit_stack[output_bit_stck_length];

//Debug variables
int step1_duration = 0;
int step2_duration = 0;
int step3_duration = 0;
int MVDR_duration = 0;
int step_counter = 0;

float LinearToDecibel(float linear)
{
    float db;
    
    if (linear != 0.0f)
        db = 20.0f * log10(linear);
    else
        db = -144.0f;  // effectively minus infinity
    
    return db;
}

void HiLight::reset(){
    initialized = false;
}

void debug_reset(){
    for(int i = 0; i<grid_x; i++){
        for(int j = 0; j<grid_y; j++ ){
            for (int k = 0; k < first_bit_window; k++){
                first_bit_color_window[i][j][k] = 0;
            }
            for (int k = 0; k < N; k++){
                grid_color_intensity[i][j][k] = 0;
                hilight_results_stack[i][j][k] = 0;
            }
            hilight_results[i][j] = 0;
        }
    }
    window_index = 0;
    first_bit_detected = false;
    first_bit_index = 0;
    hilight_stack_index = 0;
    bit_counter = 0;
    results_stack_counter = 0;
    start_receiving = false;
    counter_after_detect_1= 0;
}

//calibrate the first bit position using preample
void calibrate_first_bit_position(){
    double current_time_tmp;
    double start_time_tmp;
    int counter_2 = 0;
    int i,j,k,m,a,b,counter_1_tmp,counter_2_tmp;
    int first_bit_position = 0;
    if (hilight_first_bit_index - 2.5 * N <= 0){
        for (i = 0; i < hilight_first_bit_index - N; i++){
            start_time_tmp = hilight_first_bit_timestamp[i];
            for (j = i + N/4*3; j < hilight_first_bit_index; j++){
                current_time_tmp = hilight_first_bit_timestamp[j];
                if(current_time_tmp - start_time_tmp >= N/60.0){
                    int counter_for_2_tmp = 0;
                    for (a = 0; a < grid_x; a++){
                        for (b = 0; b <  grid_y; b++){
                            counter_1_tmp = 0;
                            counter_2_tmp = 0;
                            for (k = i; k < j; k++){
                                if (hilight_first_bit_stack[a][b][k] == 1){
                                    counter_1_tmp++;
                                }else{
                                    counter_2_tmp++;
                                }
                            }
                            if (counter_1_tmp < counter_2_tmp){
                                counter_for_2_tmp++;
                            }
                        }
                    }
                    if (counter_2 < counter_for_2_tmp){
                        counter_2 = counter_for_2_tmp;
                        first_bit_position = j;
                    }
                }
            }
        }
    }else{
        for (i = 0; i < hilight_first_bit_index - 2.5*N; i++){
            start_time_tmp = hilight_first_bit_timestamp[i];
            for (j = i + N/4*3; j < hilight_first_bit_index; j++){
                current_time_tmp = hilight_first_bit_timestamp[j];
                if(current_time_tmp - start_time_tmp >= N/60.0){
                    int counter_for_1_tmp = 0;
                    int counter_for_2_tmp = 0;
                    for (a = 0; a < grid_x; a++){
                        for (b = 0; b <  grid_y; b++){
                            counter_1_tmp = 0;
                            counter_2_tmp = 0;
                            for (k = i; k <= j; k++){
                                if (hilight_first_bit_stack[a][b][k] == 1){
                                    counter_1_tmp++;
                                }else{
                                    counter_2_tmp++;
                                }
                            }
                            if (counter_1_tmp >= counter_2_tmp){
                                counter_for_1_tmp++;
                            }
                        }
                    }
                    for (k = i + N*1.5; k < hilight_first_bit_index; k++){
                        current_time_tmp = hilight_first_bit_timestamp[k];
                        if(current_time_tmp - start_time_tmp >= 2*N/60.0){
                            for (a = 0; a < grid_x; a++){
                                for (b = 0; b <  grid_y; b++){
                                    counter_1_tmp = 0;
                                    counter_2_tmp = 0;
                                    for (m = j+1; m <= k; m++){
                                        if (hilight_first_bit_stack[a][b][m] == 1){
                                            counter_1_tmp++;
                                        }else{
                                            counter_2_tmp++;
                                        }
                                    }
                                    if (counter_1_tmp < counter_2_tmp){
                                        counter_for_2_tmp++;
                                    }
                                }
                            }
                        }
                    }
                    if (counter_2 < counter_for_2_tmp + counter_for_1_tmp){
                        counter_2 = counter_for_2_tmp + counter_for_1_tmp;
                        first_bit_position = k;
                    }
                }
            }
        }
    }
    start_time = hilight_first_bit_timestamp[first_bit_position];
    bit_counter = 1;
    int start_bit_tmp = first_bit_position + 1;
    for (i = first_bit_position + 1; i < hilight_first_bit_index; i++){
        current_time_tmp = hilight_first_bit_timestamp[i];
        if (current_time_tmp - start_time >= N/60.0*bit_counter){
            bit_counter++;
            for (a = 0; a < grid_x; a++){
                for (b = 0; b <  grid_y; b++){
                    counter_1_tmp = 0;
                    counter_2_tmp = 0;
                    for (k = start_bit_tmp; k <= i; k++){
                        if (hilight_first_bit_stack[a][b][k] == 1){
                            counter_1_tmp++;
                        }else{
                            counter_2_tmp++;
                        }
                    }
                    if (counter_1_tmp < counter_2_tmp){
                        output_bit_stack[results_stack_counter++] = 2;
                    }else{
                        output_bit_stack[results_stack_counter++] = 1;
                    }
                }
            }
            start_bit_tmp = i+1;
        }
    }
    hilight_stack_index = 0;
    for (i=start_bit_tmp; i < hilight_first_bit_index; i++){
        for (a = 0; a < grid_x; a++){
            for (b = 0; b <  grid_y; b++){
                hilight_results_stack[a][b][hilight_stack_index] = hilight_first_bit_stack[a][b][i];
            }
        }
        hilight_stack_index++;
    }
    if (hilight_stack_index > 0){
        hilight_stack_index--;
    }
    hilight_first_bit_index = 0;
}

// Finds the intersection of two lines, or returns false.
// The lines are defined by (o1, p1) and (o2, p2).
bool intersection(cv::Point2f o1, cv::Point2f p1, cv::Point2f o2, cv::Point2f p2,
                  cv::Point2f &r)
{
    cv::Point2f x = o2 - o1;
    cv::Point2f d1 = p1 - o1;
    cv::Point2f d2 = p2 - o2;
    
    float cross = d1.x*d2.y - d1.y*d2.x;
    if (std::abs(cross) < /*EPS*/1e-8)
        return false;
    
    double t1 = (x.x * d2.y - x.y * d2.x)/cross;
    r = o1 + d1 * t1;
    return true;
}

//Processes a frame and returns output image
bool HiLight::processFrame(const cv::Mat& inputFrame, int* screen_position, double current_timestamp, char *results, bool* denoise_check, bool* first_bit_check, int* hilight_line_index, char* hilight_results_tmp )
{
    *denoise_check = false;
    *first_bit_check = false;
    if(!initialized){
        initialized = true;
        for(int i = 0; i<grid_x; i++){
            for(int j = 0; j<grid_y; j++ ){
                for (int k = 0; k < first_bit_window; k++){
                    first_bit_color_window[i][j][k] = 0;
                }
                for (int k = 0; k < N; k++){
                    grid_color_intensity[i][j][k] = 0;
                    hilight_results_stack[i][j][k] = 0;
                }
                hilight_results[i][j] = 0;
            }
        }
        if (isImage){
            MVDR = true;
            first_2_ratio = 0.6;//0.8
            first_bit_voting = 0.5;//0.7
        }else{
            MVDR = false;
            first_2_ratio = 0.8; //0.7
            first_bit_voting = 0.7; //0.6
        }
        window_index = 0;
        first_bit_detected = false;
        first_bit_index = 0;
        hilight_stack_index = 0;
        start_time = current_timestamp;
        current_time = current_timestamp;
        denoise_start = false;
        MVDR_index = 0;
        bit_counter = 0;
        results_stack_counter = 0;
        fftSetup = vDSP_create_fftsetup(LOG_N, kFFTRadix2);
        start_receiving = false;
        counter_after_detect_1= 0;
        
        first_bit_1_detected = false;
        first_bit_2_detected = false;
        hilight_first_bit_index = 0;
        hilight_first_bit_counter = 0;
        hilight_first_bit_position = 0;
        
        if (screen_position[2] < screen_position[4]){
            image_ROI_position[0].y = screen_position[2];
        }else{
            image_ROI_position[0].y = screen_position[4];
        }
        
        if (screen_position[1] < screen_position[7]){
            image_ROI_position[0].x = screen_position[1];
        }else{
            image_ROI_position[0].x = screen_position[7];
        }
        
        if (screen_position[6] < screen_position[8]){
            image_ROI_position[1].y = screen_position[8];
        }else{
            image_ROI_position[1].y = screen_position[6];
        }
        
        if (screen_position[5] < screen_position[3]){
            image_ROI_position[1].x = screen_position[3];
        }else{
            image_ROI_position[1].x = screen_position[5];
        }
        
        if (isImage){
            // Set grids points to calculate the grid position
            for (int i = 0; i <= grid_x * grid_x_MVDR; i++){
                grids_points_top_image[i].x = screen_position[1] + (screen_position[3] - screen_position[1]) / grid_x / grid_x_MVDR * i;
                grids_points_top_image[i].y = screen_position[2] + (screen_position[4] - screen_position[2]) / grid_x / grid_x_MVDR * i;
                grids_points_bottom_image[i].x = screen_position[7] + (screen_position[5] - screen_position[7]) / grid_x / grid_x_MVDR * i;
                grids_points_bottom_image[i].y = screen_position[8] + (screen_position[6] - screen_position[8]) / grid_x / grid_x_MVDR * i;
            }
            
            for (int i = 0; i <= grid_y * grid_y_MVDR; i++){
                grids_points_left_image[i].x = screen_position[1] + (screen_position[7] - screen_position[1]) / grid_y / grid_y_MVDR * i;
                grids_points_left_image[i].y = screen_position[2] + (screen_position[8] - screen_position[2]) / grid_y / grid_y_MVDR * i;
                grids_points_right_image[i].x = screen_position[3] + (screen_position[5] - screen_position[3]) / grid_y / grid_y_MVDR * i;
                grids_points_right_image[i].y = screen_position[4] + (screen_position[6] - screen_position[4]) / grid_y / grid_y_MVDR * i;
            }
            
            for (int i = 0; i < grid_x * grid_x_MVDR; i++){
                for (int j = 0; j < grid_y * grid_y_MVDR; j++){
                    cv::Point2f r1;
                    cv::Point2f r2;
                    cv::Point2f r3;
                    cv::Point2f r4;
                    intersection(grids_points_top_image[i], grids_points_bottom_image[i], grids_points_left_image[j], grids_points_right_image[j], r1);//top left
                    intersection(grids_points_top_image[i+1], grids_points_bottom_image[i+1], grids_points_left_image[j], grids_points_right_image[j], r2);//top right
                    intersection(grids_points_top_image[i+1], grids_points_bottom_image[i+1], grids_points_left_image[j+1], grids_points_right_image[j+1], r3);// bottom right
                    intersection(grids_points_top_image[i], grids_points_bottom_image[i], grids_points_left_image[j+1], grids_points_right_image[j+1], r4);//bottom left
                    
                    //refine grid_position
                    if (r1.x <= r4.x){
                        grids_position_image[i][j][0].x = r4.x - image_ROI_position[0].x;
                    }else{
                        grids_position_image[i][j][0].x = r1.x - image_ROI_position[0].x;
                    }
                    
                    if (r1.y <= r2.y){
                        grids_position_image[i][j][0].y = r2.y - image_ROI_position[0].y;
                    }else{
                        grids_position_image[i][j][0].y = r1.y - image_ROI_position[0].y;
                    }
                    
                    if (r3.x <= r2.x){
                        grids_position_image[i][j][1].x = r3.x - image_ROI_position[0].x;
                    }else{
                        grids_position_image[i][j][1].x = r2.x - image_ROI_position[0].x;
                    }
                    
                    if (r3.y <= r4.y){
                        grids_position_image[i][j][1].y = r3.y - image_ROI_position[0].y;
                    }else{
                        grids_position_image[i][j][1].y = r4.y - image_ROI_position[0].y;
                    }
                }
            }
        }else{
            // Set grids points to calculate the grid position
            for (int i = 0; i <= grid_x; i++){
                grids_points_top_video[i].x = screen_position[1] + (screen_position[3] - screen_position[1]) / grid_x * i;
                grids_points_top_video[i].y = screen_position[2] + (screen_position[4] - screen_position[2]) / grid_x * i;
                grids_points_bottom_video[i].x = screen_position[7] + (screen_position[5] - screen_position[7]) / grid_x * i;
                grids_points_bottom_video[i].y = screen_position[8] + (screen_position[6] - screen_position[8]) / grid_x * i;
            }
            
            for (int i = 0; i <= grid_y; i++){
                grids_points_left_video[i].x = screen_position[1] + (screen_position[7] - screen_position[1]) / grid_y * i;
                grids_points_left_video[i].y = screen_position[2] + (screen_position[8] - screen_position[2]) / grid_y * i;
                grids_points_right_video[i].x = screen_position[3] + (screen_position[5] - screen_position[3]) / grid_y * i;
                grids_points_right_video[i].y = screen_position[4] + (screen_position[6] - screen_position[4]) / grid_y * i;
            }
            
            for (int i = 0; i < grid_x; i++){
                for (int j = 0; j < grid_y; j++){
                    cv::Point2f r1;
                    cv::Point2f r2;
                    cv::Point2f r3;
                    cv::Point2f r4;
                    intersection(grids_points_top_video[i], grids_points_bottom_video[i], grids_points_left_video[j], grids_points_right_video[j], r1);//top left
                    intersection(grids_points_top_video[i+1], grids_points_bottom_video[i+1], grids_points_left_video[j], grids_points_right_video[j], r2);//top right
                    intersection(grids_points_top_video[i+1], grids_points_bottom_video[i+1], grids_points_left_video[j+1], grids_points_right_video[j+1], r3);// bottom right
                    intersection(grids_points_top_video[i], grids_points_bottom_video[i], grids_points_left_video[j+1], grids_points_right_video[j+1], r4);//bottom left
                    
                    //refine grid_position
                    if (r1.x <= r4.x){
                        grids_position_video[i][j][0].x = r4.x - image_ROI_position[0].x;
                    }else{
                        grids_position_video[i][j][0].x = r1.x - image_ROI_position[0].x;
                    }
                    
                    if (r1.y <= r2.y){
                        grids_position_video[i][j][0].y = r2.y - image_ROI_position[0].y;
                    }else{
                        grids_position_video[i][j][0].y = r1.y - image_ROI_position[0].y;
                    }
                    
                    if (r3.x <= r2.x){
                        grids_position_video[i][j][1].x = r3.x - image_ROI_position[0].x;
                    }else{
                        grids_position_video[i][j][1].x = r2.x - image_ROI_position[0].x;
                    }
                    
                    if (r3.y <= r4.y){
                        grids_position_video[i][j][1].y = r3.y - image_ROI_position[0].y;
                    }else{
                        grids_position_video[i][j][1].y = r4.y - image_ROI_position[0].y;
                    }
                }
            }
        }
        
        srcTri[0] = cv::Point2f( screen_position[2],screen_position[1] );
        srcTri[1] = cv::Point2f( screen_position[4],screen_position[3] );
        srcTri[2] = cv::Point2f( screen_position[6],screen_position[5] );
        
        dist_top = sqrt(pow(screen_position[4]-screen_position[2],2.0) + pow(screen_position[3]-screen_position[1],2.0));
        dstTri[0] = cv::Point2f( screen_position[2],screen_position[1] );
        dstTri[1] = cv::Point2f( screen_position[2],screen_position[1] + dist_top );
        dstTri[2] = cv::Point2f( screen_position[2] + (int)(dist_top*transmitter_screen_ratio),screen_position[1] + dist_top);
        warp_mat = getAffineTransform( srcTri, dstTri );
    }else{
        current_time = current_timestamp;
    }
    
    //crop the transmitter screen from the captured image
    cv::Mat image_ROI = inputFrame(cv::Range(image_ROI_position[0].y, image_ROI_position[1].y), cv::Range(image_ROI_position[0].x, image_ROI_position[1].x));
    getGray_HiLight(image_ROI, grayImage);
    image_ROI.release();
    
    for (int c = 0; c < grid_x; c++){
        for (int r = 0; r < grid_y; r++){
            brightness[c][r] = 0;
        }
    }
    if (isImage){
        int brightness_c = 0;
        int brightness_r = 0;
        for (int c = 0; c < grid_x * grid_x_MVDR; c++){
            for (int r = 0; r < grid_y * grid_y_MVDR; r++)
            {
                cv::Mat tile = grayImage(cv::Range(grids_position_image[c][r][0].y + grid_margin, grids_position_image[c][r][1].y - grid_margin)
                                         , cv::Range(grids_position_image[c][r][0].x + grid_margin, grids_position_image[c][r][1].x - grid_margin));
                
                cv::Scalar pixel_scalar = cv::mean(tile);
                tile.release();
                float brightness_tmp =pixel_scalar[0];
                
                if (MVDR){
                    if(!denoise_start && MVDR_index < MVDR_frame){
                        grid_color_intensity_MVDR[c][r][MVDR_index] = brightness_tmp;
                        if ( c == grid_x * grid_x_MVDR -1 && r == grid_y * grid_y_MVDR - 1){
                            MVDR_index++;
                        }
                    }else if(!denoise_start && MVDR_index >= MVDR_frame){
                        denoise_start = true;
                        *denoise_check = true;
                        get_MVDR_weighting(grid_color_intensity_MVDR, MVDR_weighting);
                        
                        //print MVDR matrix
                        if (isDebug){
                            for (int i = 0; i < grid_x * grid_x_MVDR; i++){
                                for (int j = 0; j < grid_y * grid_y_MVDR; j++){
                                    printf("%f\t", MVDR_weighting[i][j]);
                                }
                                printf("\n");
                            }
                        }
                    }else{
                        brightness_c = floor(c*1.0/grid_x_MVDR);
                        brightness_r = floor(r*1.0/grid_y_MVDR);
                        brightness[brightness_c][brightness_r] = brightness[brightness_c][brightness_r] + brightness_tmp * MVDR_weighting[c][r];
                    }
                }else{
                    brightness_c = floor(c*1.0/grid_x_MVDR);
                    brightness_r = floor(r*1.0/grid_y_MVDR);
                    brightness[brightness_c][brightness_r] = brightness[brightness_c][brightness_r] + brightness_tmp;
                }
            }
        }
    }else{
        for (int c = 0; c < grid_x; c++){
            for (int r = 0; r < grid_y; r++)
            {
                cv::Mat tile = grayImage(cv::Range(grids_position_video[c][r][0].y + grid_margin, grids_position_video[c][r][1].y - grid_margin)
                                         , cv::Range(grids_position_video[c][r][0].x + grid_margin, grids_position_video[c][r][1].x - grid_margin));
                
                cv::Scalar pixel_scalar = cv::mean(tile);
                tile.release();
                float brightness_tmp =pixel_scalar[0];
                brightness[c][r] = brightness_tmp;
            }
        }
    }

    if (denoise_start || !MVDR){
        for (int c = 0; c < grid_x; c++){
            for (int r = 0; r < grid_y; r++){
                if (window_index<(N-1)) {
                    grid_color_intensity[c][r][window_index] = brightness[c][r];
                    window_index++;
                }else{
                    int i;
                    float fft_sum_N_over_2 = 0;
                    for (i = 1; i < N; i++){
                        grid_color_intensity[c][r][i-1] = grid_color_intensity[c][r][i];
                    }
                    grid_color_intensity[c][r][N-1] = brightness[c][r];
                    
                    for (i = 0; i < N/2; i++){
                        fft_sum_N_over_2 = fft_sum_N_over_2 + grid_color_intensity[c][r][2*i] - grid_color_intensity[c][r][2*i+1];
                    }

                    // Initialize the input buffer with a sinusoid
                    
                    // We need complex buffers in two different formats!
                    tempSplitComplex.realp = new float[N/2];
                    tempSplitComplex.imagp = new float[N/2];
                    
                    // ----------------------------------------------------------------
                    // Forward FFT
                    
                    // Scramble-pack the real data into complex buffer in just the way that's
                    // required by the real-to-complex FFT function that follows.
                    vDSP_ctoz((DSPComplex*)grid_color_intensity[c][r], 2, &tempSplitComplex, 1, N/2);
                    
                    // Do real->complex forward FFT
                    vDSP_fft_zrip(fftSetup, &tempSplitComplex, 1, LOG_N, kFFTDirection_Forward);
                    
                    int max_pulse_index = object_pulse_threashold;
                    float max_pulse = -100;
                    float object_pulse_power_1 = LinearToDecibel(pow (fft_sum_N_over_2, 2.0)/ N);
                    float object_pulse_power_2;
                    int object_pulse_force;
                    int object_pulse;
                    for (int k = object_pulse_threashold; k < N/2; k++)
                    {
                        float spectrum = LinearToDecibel((pow (tempSplitComplex.realp[k], 2.0) + pow (tempSplitComplex.imagp[k], 2.0))/ 4/ N);
                        if (max_pulse<spectrum){
                            max_pulse = spectrum;
                            max_pulse_index = k;
                        }
                        if(k == object_pulse_2){
                            object_pulse_power_2 = spectrum;
                        }
                    }
                    
                    delete tempSplitComplex.realp;
                    delete tempSplitComplex.imagp;
                    
                    if(max_pulse < object_pulse_power_1 && object_pulse_power_1 > -25){
                        object_pulse = 1;
                    }else if(max_pulse == object_pulse_power_2){
                        object_pulse = 2;
                    }else{
                        object_pulse = 0;
                    }
                    
                    if(object_pulse_power_1 >= object_pulse_power_2){
                        object_pulse_force = 1;
                    }else{
                        object_pulse_force = 2;
                    }
                    
                    //decode data by find the peak frequency power
                    if (first_bit_detected){
                        if (first_bit_1_detected && isCalibrate){
                            hilight_first_bit_stack[c][r][hilight_first_bit_index] = object_pulse_force;
                        }
                        if(current_time - start_time >= N / 60.0 * (bit_counter + 1)){
                            hilight_results[c][r] = get_hilight_results(hilight_results_stack[c][r], hilight_stack_index);
                            hilight_results_stack[c][r][0] = object_pulse_force;
                        }else{
                            hilight_results_stack[c][r][hilight_stack_index] = object_pulse_force;
                        }
                    }else{
                        first_bit_color_window[c][r][first_bit_index] = object_pulse;
                    }
                }
            }
        }

        if (first_bit_1_detected && isCalibrate){
            hilight_first_bit_timestamp[hilight_first_bit_index++] = current_time;
        }if(first_bit_2_detected && isCalibrate){
            first_bit_2_detected = false;
            hilight_first_bit_counter++;
            if (hilight_first_bit_counter == N/2){
                calibrate_first_bit_position();
            }
        }
        if (first_bit_detected){
            if(current_time - start_time >= N / 60.0 * (bit_counter + 1)){
                int counter_for_2 = 0;
                for (int i = 0; i < grid_x; i++){
                    for (int j = 0; j <  grid_y; j++){
                        if (isDebug || start_receiving){
                            printf("%d ",hilight_results[i][j]);
                        }
                        if (hilight_results[i][j] == 2){
                            counter_for_2++;
                        }
                    }
                }
                if (isDebug || start_receiving){
                    printf("\n");
                }
                hilight_stack_index = 1;
                bit_counter++;
                
                if (!start_receiving){
                    counter_after_detect_1++;
                    if (counter_after_detect_1 > 3){
                        reset();
                    }else{
                        if(counter_for_2 >= (double)grid_x * grid_y * first_2_ratio){
                            first_bit_2_detected = true;
                            start_receiving = true;
                            *first_bit_check = true;
                            if (isDebug){
                                printf("\n");
                            }
                        }
                    }
                    return false;
                }else{
                    int results_counter_tmp =0;
                    for (int i = 0; i < grid_x; i++){
                        for (int j = 0; j <  grid_y; j++){
                            output_bit_stack[results_stack_counter++] = hilight_results[i][j];
                            hilight_results_tmp[results_counter_tmp++] = hilight_results[i][j] + '0' - 1;
                        }
                    }
                    hilight_line_index[0] = results_stack_counter/grid_x/grid_y;
                    
                    if (results_stack_counter == output_bit_stck_length){
                        results_stack_counter = 0;
                        get_char_from_bits(output_bit_stack, results);
                        
                        printf("%s\n",results);
                        
                        if (demo){
                            debug_reset();
                            return true;
                        }else{
                            debug_reset();
                            return false;
                        }
                    }else{
                        return false;
                    }
                }
            }else{
                hilight_stack_index++;
            }
        }else{
            if(first_bit_index<first_bit_window-1){
                first_bit_index++;
            }else{
                first_bit_detected = detect_first_bit(first_bit_color_window);
                if (first_bit_detected){
                    if (isCalibrate){
                        first_bit_1_detected = true;
                    }
                    start_time = current_time;
                }
            }
        }
    }
    return false;
}

// detect first bit function
bool HiLight::detect_first_bit(int first_bit_color_window[grid_x][grid_y][first_bit_window]){
    int sum_first_bit_window[grid_x][grid_y];
    int first_bit_counter = 0;
    double first_bit_ratio_judge = grid_x * 1.0 * grid_y * first_bit_ratio;
    double first_bit_voting_judge = first_bit_window * 1.0 * first_bit_voting;
    for (int i = 0; i < grid_x; i++){
        for(int j = 0; j < grid_y; j++){
            sum_first_bit_window[i][j] = 0;
        }
    }
    for (int i = 0; i < grid_x; i++){
        for(int j = 0; j < grid_y; j++){
            for (int k = 0; k < first_bit_window; k++){
                if(first_bit_color_window[i][j][k] == 1){
                    sum_first_bit_window[i][j]++;
                }
            }
        }
    }
    for (int i = 0; i < grid_x; i++){
        for(int j = 0; j < grid_y; j++){
            if(sum_first_bit_window[i][j] >= first_bit_voting_judge){
                first_bit_counter++;
            }
        }
    }
    if(first_bit_counter >= first_bit_ratio_judge){
        return true;
    }else{
        for (int i = 0; i < grid_x; i++){
            for(int j = 0; j < grid_y; j++){
                for (int k = 0; k < first_bit_window-1; k++){
                    first_bit_color_window[i][j][k] = first_bit_color_window[i][j][k+1];
                }
            }
        }
        return false;
    }
}

//get demodulation results
int HiLight::get_hilight_results(int *hilight_results_stack, int hilight_stack_index){
    int counter_1 = 0;
    int counter_2 = 0;
    for (int i = 0; i < hilight_stack_index; i++){
        if (hilight_results_stack[i] == 1){
            if (i < hilight_stack_index/8 || i >= hilight_stack_index * 7 / 8){
                counter_1 = counter_1 + 1;
            }else if (i < hilight_stack_index*3/8 || i >= hilight_stack_index * 5 / 8){
                counter_1 = counter_1 + 1;
            }else{
                counter_1 = counter_1 + 1;
            }
        }else if(hilight_results_stack[i] == 2){
            if (i < hilight_stack_index/8 || i >= hilight_stack_index * 7 / 8){
                counter_2 = counter_2 + 1;
            }else if (i < hilight_stack_index*3/8 || i >= hilight_stack_index * 5 / 8){
                counter_2 = counter_2 + 1;
            }else{
                counter_2 = counter_2 + 1;
            }
        }
    }
    if (counter_1 >= counter_2){
        return 1;
    }else{
        return 2;
    }
}

//get MVDR weighting matrix
void HiLight::get_MVDR_weighting(float grid_color_intensity_MVDR[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR][MVDR_frame], float MVDR_weighting[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR]){
    
    float brightness_mean[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR];
    for (int j = 0; j < grid_x * grid_x_MVDR; j++){
        for (int k = 0; k < grid_y * grid_y_MVDR; k++){
            brightness_mean[j][k] = 0;
        }
    }
    
    for (int j = 0; j < grid_x*grid_x_MVDR; j++){
        for (int k = 0; k < grid_y*grid_y_MVDR; k++){
            for(int i = 0; i < MVDR_frame; i++){
                brightness_mean[j][k] = brightness_mean[j][k] + grid_color_intensity_MVDR[j][k][i];
            }
            brightness_mean[j][k] = brightness_mean[j][k]*1.0 / MVDR_frame;
            for(int i = 0; i < MVDR_frame; i++){
                grid_color_intensity_MVDR[j][k][i] = grid_color_intensity_MVDR[j][k][i] - brightness_mean[j][k];
            }
        }
    }
    
    // Set up a data structure with pre-calculated values for
    // doing a very fast FFT.
    float x[N];
    for (int j = 0; j < grid_x; j++){
        for (int k = 0; k < grid_y; k++){
            float x_MVDR[grid_x_MVDR * grid_y_MVDR][MVDR_frame/N][N];
            float r_MDVR[grid_x_MVDR * grid_y_MVDR][grid_x_MVDR * grid_y_MVDR];
            for (int frame_index = 0; frame_index < MVDR_frame/N;  frame_index++){
                for (int j1 = 0; j1 < grid_x_MVDR; j1++){
                    for (int k1 = 0; k1 < grid_y_MVDR; k1++){
                        for (int i = 0 ; i < N; i++){
                            x[i] = grid_color_intensity_MVDR[j1][k1][frame_index*N + i];
                        }
                        float fft_sum_N_over_2 = 0;
                        for (int i = 0; i < N/2; i++){
                            fft_sum_N_over_2 = fft_sum_N_over_2 + x[2*i] - x[2*i+1];
                        }
                        // We need complex buffers in two different formats!
                        DSPSplitComplex tempSplitComplex;
                        tempSplitComplex.realp = new float[N/2];
                        tempSplitComplex.imagp = new float[N/2];
                        
                        // ----------------------------------------------------------------
                        // Forward FFT
                        
                        // Scramble-pack the real data into complex buffer in just the way that's
                        // required by the real-to-complex FFT function that follows.
                        vDSP_ctoz((DSPComplex*)x, 2, &tempSplitComplex, 1, N/2);
                        
                        // Do real->complex forward FFT
                        vDSP_fft_zrip(fftSetup, &tempSplitComplex, 1, LOG_N, kFFTDirection_Forward);
                        
                        x_MVDR[j1*grid_y_MVDR + k1][frame_index][N/2] = fft_sum_N_over_2;
                        for (int i = 0; i < N/2; i++)
                        {
                            x_MVDR[j1*grid_y_MVDR + k1][frame_index][i] = tempSplitComplex.realp[i];
                            if (i>0){
                                x_MVDR[j1*grid_y_MVDR + k1][frame_index][N-i] = tempSplitComplex.realp[i];
                            }
                        }
                    }
                }
            }
            for (int r_j = 0; r_j < grid_x_MVDR * grid_y_MVDR; r_j++){
                for (int r_k = 0; r_k < grid_x_MVDR * grid_y_MVDR; r_k++){
                    float x_MVDR_sum = 0;
                    for (int row = 0; row != MVDR_frame/N; ++row)
                    {
                        for (int col = 0; col != N; ++col)
                        {
                            x_MVDR_sum = x_MVDR_sum + x_MVDR[r_j][row][col] * x_MVDR[r_k][row][col];
                        }
                    }
                    r_MDVR[r_j][r_k] = x_MVDR_sum;
                }
            }
            
            float w[grid_x_MVDR * grid_y_MVDR];
            float r_MVDR_inv[grid_x_MVDR * grid_y_MVDR][grid_x_MVDR * grid_y_MVDR];
            matrix <double> r_MDVR_tmp(grid_x_MVDR * grid_y_MVDR,grid_x_MVDR * grid_y_MVDR);
            for (int r_i=0; r_i < r_MDVR_tmp.getactualsize(); r_i++){
                for (int r_j=0; r_j < r_MDVR_tmp.getactualsize(); r_j++)
                {
                    r_MDVR_tmp.setvalue(r_i , r_j, r_MDVR[r_i][r_j]);
                }
            }
            r_MDVR_tmp.invert();
            bool invert_check;
            double inv_results;
            float r_MVDR_inv_sum = 0;
            float w_sum = 0;
            for (int r_i=0; r_i < r_MDVR_tmp.getactualsize(); r_i++){
                for (int r_j=0; r_j<r_MDVR_tmp.getactualsize(); r_j++)
                {
                    r_MDVR_tmp.getvalue(r_i, r_j, inv_results, invert_check);
                    r_MVDR_inv[r_i][r_j] = inv_results;
                    r_MVDR_inv_sum = r_MVDR_inv_sum + inv_results;
                }
            }
            for (int r_i = 0; r_i < grid_x_MVDR * grid_y_MVDR; r_i++){
                w[r_i] = 0;
                for (int r_j = 0; r_j < grid_x_MVDR * grid_y_MVDR; r_j++){
                    w[r_i] = w[r_i] + r_MVDR_inv[r_i][r_j];
                }
                w[r_i] = w[r_i]/r_MVDR_inv_sum;
                if (w[r_i] < 0){
                    w[r_i] = 0;
                }
                w_sum = w_sum + w[r_i];
            }
            for (int r_i = 0; r_i < grid_x_MVDR * grid_y_MVDR; r_i++){
                w[r_i] = w[r_i] / w_sum * grid_x_MVDR * grid_y_MVDR;
            }
            
            for(int w_i = 0; w_i < grid_x_MVDR; w_i++){
                for(int w_j = 0; w_j < grid_y_MVDR; w_j++){
                    MVDR_weighting[ j * grid_x_MVDR + w_i][ k * grid_y_MVDR + w_j] = w[w_i * grid_y_MVDR + w_j];
                }
            }
        }
    }
}

//get matrix multiplx results
void HiLight::get_matrix_multiplx (float **matrix1, float **matrix2, float **results, int results_x, int results_y, int INNER){
    for (int row = 0; row != results_x; ++row)
	{
 		for (int col = 0; col != results_y; ++col)
		{
			int sum = 0;
			for (int inner = 0; inner != INNER; ++inner)
			{
				sum += matrix1[row][inner] * matrix2[inner][col];
			}
			results[row][col] = sum;
		}
	}
}

void HiLight::get_matrix_outer_multiplx (float **matrix1, float **matrix2, float **results, int results_x, int results_y){
    for (int row = 0; row != results_x; ++row)
	{
 		for (int col = 0; col != results_y; ++col)
		{
			results[row][col] = matrix1[row][col] * matrix2[row][col];
		}
	}
}

void HiLight::get_matrix_outer_divide (float **matrix1, float **matrix2, float **results, int results_x, int results_y){
    for (int row = 0; row != results_x; ++row)
	{
 		for (int col = 0; col != results_y; ++col)
		{
			results[row][col] = matrix1[row][col] / matrix2[row][col];
		}
	}
}

//get matrix transform
void HiLight::get_matrix_transform (float **matrix, int matrix_x, int matrix_y, float **results){
    for (int i = 0; i < matrix_x; i++){
        for(int j = 0; j < matrix_y; j++){
            results[matrix_y][matrix_x] = matrix[matrix_x][matrix_y];
        }
    }
}

// get char from bits
void HiLight::get_char_from_bits(int *output_bit_stack, char* results){
    int results_voting_stack[repeat_time][encode_length];
    int results_stack[encode_length];
    int stack_counter = 0;
    int voting_counter_1 = 0;
    int voting_counter_2 = 0;
    int result_tmp = 0;
    for (int i = 0; i < output_results_length; i++){
        
        for (int j = 0; j < repeat_time; j++){
            for(int k = 0; k < encode_length; k++){
                results_voting_stack[j][k] = output_bit_stack[stack_counter++];
            }
        }
        
        for (int k = 0; k < encode_length; k++){
            voting_counter_1 = 0;
            voting_counter_2 = 0;
            for(int j = 0; j < repeat_time; j++){
                if (results_voting_stack[j][k] == 1){
                    voting_counter_1++;
                }else{
                    voting_counter_2++;
                }
            }
            if(voting_counter_1 > voting_counter_2){
                results_stack[k] = 0;
            }else{
                results_stack[k] = 1;
            }
        }
        
        result_tmp = 0;
        for (int k = 0; k < encode_length; k++){
            if (results_stack[k] == 1){
                result_tmp = result_tmp + pow(2, encode_length-1-k);
            }
        }
        
        result_tmp = result_tmp + 64;
        if ((result_tmp > 64 && result_tmp < 91)||(result_tmp > 96 && result_tmp < 123)){
            results[i] = result_tmp;
        }else if (result_tmp == 64){
            results[i] = ' ';
        }else if (result_tmp == 91){
            results[i] = '1';
        }
        else if (result_tmp == 92){
            results[i] = '2';
        }
        else if (result_tmp == 93){
            results[i] = '3';
        }
        else if (result_tmp == 94){
            results[i] = '4';
        }
        else if (result_tmp == 95){
            results[i] = '5';
        }
        else if (result_tmp == 96){
            results[i] = '6';
        }
        else if (result_tmp == 123){
            results[i] = '7';
        }
        else if (result_tmp == 124){
            results[i] = '8';
        }
        else if (result_tmp == 125){
            results[i] = '9';
        }
        else if (result_tmp == 126){
            results[i] = '0';
        }
    }
}