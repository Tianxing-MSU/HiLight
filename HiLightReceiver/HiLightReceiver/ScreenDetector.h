//
//  ScreenDetector.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 8/4/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#ifndef __HiLightReceiver__ScreenDetector__
#define __HiLightReceiver__ScreenDetector__

using namespace cv;

class ScreenDetector{
public:
    ScreenDetector();
    
    //! Processes a frame and returns output image
    virtual bool processFrame(cv::Mat& inputFrame, cv::Point& Top_left, cv::Point& Top_right, cv::Point& Bottom_left, cv::Point& Bottom_right);
    // Add rect to image 
    virtual void Addrect(cv::Mat& inputFrame, cv::Point Top_left, cv::Point Top_right, cv::Point Bottom_left, cv::Point Bottom_right);
    
private:
    cv::Mat grayImage;
    cv::Mat edges;
    cv::vector<cv::Vec4i> hougnLines;
    
    cv::Mat grad_x, grad_y;
    cv::Mat abs_grad_x, abs_grad_y;
    
    cv::Mat dst;
    cv::Mat dst_norm, dst_norm_scaled;
    
    bool m_showOnlyEdges;
    std::string m_algorithmName;
    
    // Canny detector options:
    int m_cannyLoThreshold;
    int m_cannyHiThreshold;
    int m_cannyAperture;
    
    // HoughLinesP detector options:
    double m_rho;
    double m_theta;
    int m_threshold;
    double m_minLineLength;
    double m_maxLineGap;
};

#endif /* defined(__HiLightReceiver__ScreenDetector__) */
