//
//  ScreenDetector.cpp
//  HiLightReceiver
//
//  Created by Tianxing Li on 8/4/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#include "ScreenDetector.h"
#include "Matrix.h"
#include <algorithm>

using namespace cv;

ScreenDetector::ScreenDetector()
: m_showOnlyEdges(true)
, m_algorithmName("Canny")
, m_cannyLoThreshold(250)
, m_cannyHiThreshold(250)
, m_cannyAperture(1)
, m_rho(1)
, m_theta(CV_PI/180)
, m_threshold(30)
, m_minLineLength(100)
, m_maxLineGap(100)
{}

void getGray_screenDetector(const cv::Mat& input, cv::Mat& gray)
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

static Point GetIntersection(Point line1_p1, Point line1_p2, Point line2_p1, Point line2_p2)
{
    Point intersection;
    if (line1_p1.x != line1_p2.x && line2_p1.x != line2_p2.x){
        double a1 = (line1_p1.y - line1_p2.y) / (double)(line1_p1.x - line1_p2.x);
        double b1 = line1_p1.y - a1 * line1_p1.x;
        
        double a2 = (line2_p1.y - line2_p2.y) / (double)(line2_p1.x - line2_p2.x);
        double b2 = line2_p1.y - a2 * line2_p1.x;
        
        intersection.x = (b2 - b1) / (a1 - a2);
        intersection.y = a1 * intersection.x + b1;
    }else if(line1_p1.x == line1_p2.x){
        intersection.x = line1_p1.x;
        
        double a2 = (line2_p1.y - line2_p2.y) / (double)(line2_p1.x - line2_p2.x);
        double b2 = line2_p1.y - a2 * line2_p1.x;
        intersection.y = a2 * intersection.x + b2;
    }else if (line2_p1.x == line2_p2.x){
        intersection.x = line2_p1.x;
        
        double a1 = (line1_p1.y - line1_p2.y) / (double)(line1_p1.x - line1_p2.x);
        double b1 = line1_p1.y - a1 * line1_p1.x;
        intersection.y = a1 * intersection.x + b1;
    }
    
    if (intersection.x>1280){
        intersection.x = 0;
    }
    
    if (intersection.y>720){
        intersection.y = 0;
    }
        
    return intersection;
}

const int threshold_x = 200;
const int threshold_y = 100;
const double left_threshold = 3.0/7;
const double right_threshold = 4.0/7;
const double top_threshold = 1.0/5;
const double botto_threshold = 4.0/5;
const double slope_veritical = CV_PI/48*5;
const double slope_horizontal = CV_PI/48*19;

// Processes a frame and returns output image
bool ScreenDetector::processFrame(cv::Mat& inputFrame, Point& Top_left, Point& Top_right, Point& Bottom_left, Point& Bottom_right)
{
    getGray_screenDetector(inputFrame, grayImage);
    
    int screen_length = grayImage.cols;
    int screen_width  = grayImage.rows;
    
    Point lines_top_1;
    Point lines_bottom_1;
    Point lines_left_1;
    Point lines_right_1;
    Point lines_top_2;
    Point lines_bottom_2;
    Point lines_left_2;
    Point lines_right_2;
    int lines_top_dist = 600;
    int lines_bottom_dist = 600;
    int lines_left_dist = 600;
    int lines_right_dist = 600;
    
    lines_top_1 = Point(0, 0);
    lines_bottom_1 = Point(0, 100000);
    lines_left_1 = Point(0, 0);
    lines_right_1 = Point(100000, 0);
    
    if (m_algorithmName == "Canny")
    {
        //edge detection
        cv::Canny(grayImage, edges, m_cannyLoThreshold, m_cannyHiThreshold);
        
        //line detection
        cv::HoughLinesP(edges, hougnLines, m_rho, m_theta, m_threshold, m_minLineLength, m_maxLineGap);
        
        //detect the screen by finding the closed form of quadrilateral
        for( size_t i = 0; i < hougnLines.size(); i++ )
        {
            double x_1 = hougnLines[i][0];
            double y_1 = hougnLines[i][1];
            double x_2 = hougnLines[i][2];
            double y_2 = hougnLines[i][3];
            bool check = true;
            if ((x_1 < screen_length * left_threshold || x_1 >screen_length*right_threshold)&&(x_2 < screen_length * left_threshold || x_2 >screen_length*right_threshold)){
                if ((y_1 < screen_width * top_threshold || y_1 >screen_width*botto_threshold)&&(y_2 < screen_width * top_threshold || y_2 >screen_width*botto_threshold)){
                    check = true;
                }
            }
            if (x_1 == 0 || x_2 == 0 || y_1 == 0 || y_2 == 0){
                continue;
            }
            double dist = sqrt(pow(y_2-y_1, 2)+pow(x_2-x_1, 2));
            double tan_value = (y_2-y_1)/(x_2-x_1);
            double atan_value = atan(tan_value);
            double slope = std::abs(atan_value);
            if (check){
                if (slope < slope_veritical && dist > threshold_x){
                    if (y_1 < screen_width * top_threshold){
                        if (lines_top_1.y < y_1){
                            lines_top_dist = std::abs(y_1-screen_width * top_threshold);
                            lines_top_1 = Point(x_1, y_1);
                            lines_top_2 = Point(x_2, y_2);
                        }
                    }
                    if (y_1 > screen_width*botto_threshold){
                        if (lines_bottom_1.y > y_1){
                            lines_bottom_dist = std::abs(y_1 - screen_width*botto_threshold);
                            lines_bottom_1 = Point(x_1, y_1);
                            lines_bottom_2 = Point(x_2, y_2);
                        }
                    }
                }else if (slope > slope_horizontal && dist > threshold_y){
                    if (x_1 < screen_length * left_threshold){
                        if (lines_left_1.x < x_1){
                            lines_left_dist = std::abs(x_1-screen_length * left_threshold);
                            lines_left_1 = Point(x_1, y_1);
                            lines_left_2 = Point(x_2, y_2);
                        }
                    }
                    if (x_1 > screen_length*right_threshold){
                        if (lines_right_1.x > x_1){
                            lines_right_dist = std::abs(x_1 - screen_length*right_threshold);
                            lines_right_1 = Point(x_1, y_1);
                            lines_right_2 = Point(x_2, y_2);
                        }
                    }
                }
            }
        }
    }
    
    if (lines_top_1.x == 0){
        lines_top_1.x = lines_left_1.x;
        if (lines_left_1.y < screen_width * left_threshold){
            lines_top_1.y = lines_left_1.y;
        }else{
            lines_top_1.y = lines_left_2.y;
        }
        
        lines_top_2.x = lines_right_1.x;
        if (lines_right_2.y > screen_width*right_threshold){
            lines_top_2.y = lines_right_1.y;
        }else{
            lines_top_2.y = lines_right_2.y;
        }
    }
    if (lines_bottom_1.x == 0){
        lines_bottom_1.x = lines_left_1.x;
        if (lines_left_1.y > screen_width*right_threshold){
            lines_bottom_1.y = lines_left_1.y;
        }else{
            lines_bottom_1.y = lines_left_2.y;
        }
        
        lines_bottom_2.x = lines_right_1.x;
        if (lines_right_2.y < screen_width * left_threshold){
            lines_bottom_2.y = lines_right_1.y;
        }else{
            lines_bottom_2.y = lines_right_2.y;
        }
    }
    if (lines_left_1.x == 0){
        lines_left_1.x = lines_top_1.x;
        if (lines_top_1.y < screen_length * left_threshold){
            lines_left_1.y = lines_top_1.y;
        }else{
            lines_left_1.y = lines_top_2.y;
        }
        
        lines_left_2.x = lines_bottom_1.x;
        if (lines_bottom_2.y > screen_length*right_threshold){
            lines_left_2.y = lines_bottom_1.y;
        }else{
            lines_left_2.y = lines_bottom_2.y;
        }
    }
    if (lines_right_1.x == 0){
        lines_right_1.x = lines_top_1.x;
        if (lines_top_1.y > screen_length*right_threshold){
            lines_right_1.y = lines_top_1.y;
        }else{
            lines_right_1.y = lines_top_2.y;
        }
        
        lines_right_2.x = lines_bottom_1.x;
        if (lines_bottom_2.y < screen_length * left_threshold){
            lines_right_2.y = lines_bottom_1.y;
        }else{
            lines_right_2.y = lines_bottom_2.y;
        }
    }
    
    Top_left = GetIntersection(lines_top_1, lines_top_2, lines_left_1, lines_left_2);
    Top_right = GetIntersection(lines_top_1, lines_top_2, lines_right_1, lines_right_2);
    Bottom_left = GetIntersection(lines_bottom_1, lines_bottom_2, lines_left_1, lines_left_2);
    Bottom_right = GetIntersection(lines_bottom_1, lines_bottom_2, lines_right_1, lines_right_2);
    if (Top_left.x == 0 || Top_left.y == 0 || Top_right.x == 0 || Top_right.y == 0|| Bottom_left.x == 0 || Bottom_left.y == 0 || Bottom_right.x == 0 || Bottom_right.y == 0){
        return false;
    }else{
        return true;
    }
}

//Draw the box for the transmitter screen
void ScreenDetector::Addrect(cv::Mat& inputFrame, cv::Point Top_left, cv::Point Top_right, cv::Point Bottom_left, cv::Point Bottom_right){
    line( inputFrame, Top_left, Top_right, Scalar(0,255,0), 3, 8 );
    line( inputFrame, Top_left, Bottom_left, Scalar(0,255,0), 3, 8 );
    line( inputFrame, Bottom_left, Bottom_right, Scalar(0,255,0), 3, 8 );
    line( inputFrame, Top_right, Bottom_right, Scalar(0,255,0), 3, 8 );
}