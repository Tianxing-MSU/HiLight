/*
 * Created by Tianxing Li
 *
 * Copyright 2015 The Trustees Dartmouth College,
 * Permission to use for Non-commercial Research Purposes Only.
 * All Other Rights Reserved.
 */

package edu.dartnets.hilight;

import android.graphics.Bitmap;
import android.graphics.Point;

import org.opencv.core.Mat;

/**
 * Created by Tianxing on 3/22/15.
 * Global parameters for HiLight transmitter
 */
public class Globals {
    static Bitmap bmFrame_ori;
    static Bitmap bmFrame;
    static int grid_size_x;
    static int grid_size_y;
    static Mat current_Frame;
    static Mat current_Frame_tmp;
    static Mat mat_tmp;
    static double[][] alpha_level_image;
    static Point size;

    static final int REPEAT_TIME = 3;
    static final int MODULATION_LENGTH = 16;
    static final int BLANK_FRAMES = 100;
    static final int GRID_X = 6;
    static final int GRID_Y = 6;
    static final int SUB_GRID_X = 1;
    static final int SUB_GRID_Y = 1;
    static final int IMG_RESIZE = 16;
    static final int mHistSizeNum = 255;
    static final int start_time_ms = 500;
    static final int video_length = 10000;
}
