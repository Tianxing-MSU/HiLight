//
//  HiLight.h
//  HiLightReceiver
//
//  Created by Tianxing Li on 7/29/14.
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//

#ifndef OpenCV_Tutorial_HiLight_h
#define OpenCV_Tutorial_HiLight_h

class HiLight
{
public:
  HiLight();
    
    const static int LOG_N = 4;
    const static int N = 1 << LOG_N;
    const static int first_bit_window = N;
    const static int grid_x = 6;
    const static int grid_y = 6;
    const static int grid_x_MVDR = 3;
    const static int grid_y_MVDR = 3;
    const static int MVDR_frame = 96;
    const static int output_results_length = 12;
    
  //! Processes a frame and returns output image
  virtual void reset();
  virtual bool processFrame(const cv::Mat& inputFrame, int* screen_position, double current_timestamp, char *results, bool* denoise_check, bool* first_bit_check, int* hilight_line_index, char* hilight_results_tmp);
  virtual bool detect_first_bit(int first_bit_color_window[grid_x][grid_y][first_bit_window]);
  virtual int get_hilight_results(int *hilight_results_stack , int hilight_stack_index);
  virtual void get_MVDR_weighting(float grid_color_intensity_MVDR[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR][MVDR_frame], float MVDR_weighting[grid_x*grid_x_MVDR][grid_y*grid_y_MVDR]);
  virtual void get_matrix_outer_multiplx (float **matrix1, float **matrix2, float **results, int results_x, int results_y);
  virtual void get_matrix_outer_divide (float **matrix1, float **matrix2, float **results, int results_x, int results_y);
  virtual void get_matrix_multiplx (float **matrix1, float **matrix2, float **results, int results_x, int results_y, int INNER);
  virtual void get_matrix_transform (float **matrix, int matrix_x, int matrix_y, float **results);
  virtual void get_char_from_bits(int *output_bit_stack, char* results);
    
private:
  cv::Mat grayImage;
  cv::Mat edges;

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
    
  // Harris detector options:
  int m_harrisBlockSize;
  int m_harrisapertureSize;
  double m_harrisK;
  int m_harrisThreshold;
};

#endif
