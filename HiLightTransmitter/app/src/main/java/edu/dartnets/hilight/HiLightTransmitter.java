/*
 * Created by Tianxing Li
 *
 * Copyright 2015 The Trustees Dartmouth College,
 * Permission to use for Non-commercial Research Purposes Only.
 * All Other Rights Reserved.
 */

package edu.dartnets.hilight;

import android.animation.TimeAnimator;
import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Point;
import android.graphics.SurfaceTexture;
import android.media.MediaCodec;
import android.media.MediaExtractor;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;

import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.Core;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.MatOfFloat;
import org.opencv.core.MatOfInt;
import org.opencv.core.Scalar;
import org.opencv.imgproc.Imgproc;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

public class HiLightTransmitter extends Activity implements TextureView.SurfaceTextureListener {

    static String filePath;
    String encode_info;
    Bitmap bmFrame;
    Mat current_Frame;
    Mat perfetch_Frame;
    Timer timer;
    TimerTask timerTask;
    final Handler handler = new Handler();
    int counter = 0;
    ImageView[][] mImageViews;
    ImageView background_image;

    int grid_size_x;
    int grid_size_y;
    int frame_x;
    int frame_y;
    int frame_size_x;
    int frame_size_y;
    boolean debug = false;
    int frame_length;
    int[][][] alpha_matrix;
    boolean isImage;
    TextureView mPlaybackView;
    TimeAnimator mTimeAnimator;
    MediaCodecWrapper mCodecWrapper;
    MediaExtractor mExtractor;
    Activity activity;
    boolean video_start;
    long prefetch_video_milis;
    int video_prefetch_counter;
    Point size;

    // Setup variables.
    double[][] current_color_intensity;
    double[][] perfetch_color_intensity;
    double[][] alpha_level_image;
    double[][][] alpha_level_video;
    double[] alpha_level_video_timestamp;
    int alpha_level_video_index;
    double[][] gauss2d_matrix;
    float current_hist_buffer[];
    float perfetch_hist_buffer[];
    Mat current_Frame_tmp;
    Mat perfetch_Frame_tmp;
    List<Mat> bgr_planes;
    Mat hist;
    Mat mat_tmp;
    long current_video_timestamp;
    static int video_fps;
    int previous_k;
    Bitmap icon_white;
    long previous = 0;
    long now;

    static {
        if (!OpenCVLoader.initDebug()) {
            // Handle initialization error
            System.out.println("BAD");
        }
    }

    //init parameters for image file
    private void init_image() {
        alpha_level_image = new double[Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_Y];
        for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                alpha_level_image[i][j] = 1;
            }
        }

        current_color_intensity = new double[Globals.GRID_X][Globals.GRID_Y];
        perfetch_color_intensity = new double[Globals.GRID_X][Globals.GRID_Y];
        gauss2d_matrix = new double[Globals.SUB_GRID_X][Globals.SUB_GRID_X];
        video_start = false;
        current_video_timestamp = 0;
    }

    //init parameters for video file
    private void init_video() {
        alpha_level_video_timestamp = new double[Globals.video_length];
        current_color_intensity = new double[Globals.GRID_X][Globals.GRID_Y];
        perfetch_color_intensity = new double[Globals.GRID_X][Globals.GRID_Y];
        alpha_level_video = new double[Globals.video_length][Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_X];
        alpha_level_video_index = 0;
        gauss2d_matrix = new double[Globals.SUB_GRID_X][Globals.SUB_GRID_X];
        current_hist_buffer = new float[Globals.mHistSizeNum];
        perfetch_hist_buffer = new float[Globals.mHistSizeNum];
        video_start = false;
        current_video_timestamp = 0;
        previous_k = 0;
        video_prefetch_counter = 0;
    }

    ;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        activity = this;

        Bundle extras = getIntent().getExtras();
        int demoType = 0;

        //extract intent message from Main Activity
        if (extras != null) {
            filePath = extras.getString("filepath");
            encode_info = extras.getString("encode_info");
            demoType = extras.getInt("input_type");
        }
        video_fps = 30; //default video frame rate

        if (demoType == HiLight_MainActivity.DEMO_IMAGE) {
            isImage = true;
            init_image();
        } else if (demoType == HiLight_MainActivity.DEMO_VIDEO) {
            isImage = false;
            init_video();
        } else {
            System.out.println("Something went wrong reading demo type.");
            System.exit(0);
        }

        if (encode_info.equals("")) {
            encode_info = "MobiSys 2015";
        }

        String alpha_vector = getAlphaMatrix(encode_info);
        frame_length = alpha_vector.length() / Globals.GRID_X / Globals.GRID_Y;

        alpha_matrix = new int[frame_length * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES][Globals.GRID_X][Globals.GRID_Y];
        for (int i = 0; i < Globals.BLANK_FRAMES; i++) {
            for (int j = 0; j < Globals.GRID_X; j++) {
                for (int k = 0; k < Globals.GRID_Y; k++) {
                    alpha_matrix[i][j][k] = 1;
                }
            }
        }

        getGaussian2d();

        for (int j = 0; j < Globals.GRID_X; j++) {
            for (int k = 0; k < Globals.GRID_Y; k++) {
                for (int i = 0; i < Globals.MODULATION_LENGTH; i++) {
                    if (i % 2 == 0) {
                        alpha_matrix[i + Globals.BLANK_FRAMES - 2 * Globals.MODULATION_LENGTH][j][k] = 0;
                    } else {
                        alpha_matrix[i + Globals.BLANK_FRAMES - 2 * Globals.MODULATION_LENGTH][j][k] = 1;
                    }
                    if (i % 2 == 0) {
                        alpha_matrix[i + Globals.BLANK_FRAMES - 3 * Globals.MODULATION_LENGTH][j][k] = 0;
                    } else {
                        alpha_matrix[i + Globals.BLANK_FRAMES - 3 * Globals.MODULATION_LENGTH][j][k] = 1;
                    }

                    if (i % 3 == 0) {
                        alpha_matrix[i + Globals.BLANK_FRAMES - Globals.MODULATION_LENGTH][j][k] = 1;
                    } else {
                        alpha_matrix[i + Globals.BLANK_FRAMES - Globals.MODULATION_LENGTH][j][k] = 0;
                    }
                }
            }
        }

        for (int i = 0; i < frame_length; i++) {
            for (int j = 0; j < Globals.GRID_X; j++) {
                for (int k = 0; k < Globals.GRID_Y; k++) {
                    String tmp_string = String.valueOf(alpha_vector.charAt(i * Globals.GRID_X * Globals.GRID_Y + j * Globals.GRID_Y + k));
                    for (int p = 0; p < Globals.MODULATION_LENGTH; p++) {
                        if (tmp_string.equals("0")) {
                            if (p % 2 == 0) {
                                alpha_matrix[i * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES + p][j][k] = 0;
                            } else {
                                alpha_matrix[i * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES + p][j][k] = 1;
                            }
                        } else {
                            if (p % 3 == 0) {
                                alpha_matrix[i * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES + p][j][k] = 1;
                            } else {
                                alpha_matrix[i * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES + p][j][k] = 0;
                            }
                        }
                    }
                }
            }
        }

        setContentView(R.layout.media_metadata);
        RelativeLayout layout = (RelativeLayout) findViewById(R.id.mainLayout);

        Display display = getWindowManager().getDefaultDisplay();
        size = new Point();
        display.getSize(size);
        int width = size.x;
        int height = size.y;
        grid_size_x = width / Globals.GRID_X;
        grid_size_y = height / Globals.GRID_Y;

        mImageViews = new ImageView[Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_Y];

        //Get black background for the foreground imageview
        Bitmap icon = decodeSampledBitmapFromResource(this.getResources(), R.drawable.single_color_img, grid_size_x, grid_size_y);
        icon = Bitmap.createScaledBitmap(icon, grid_size_x, grid_size_y, true);
        icon_white = decodeSampledBitmapFromResource(this.getResources(), R.drawable.single_color_img_white, grid_size_x, grid_size_y);
        icon_white = Bitmap.createScaledBitmap(icon_white, grid_size_x, grid_size_y, true);

        LayoutParams params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        if (isImage) {
            bmFrame = BitmapFactory.decodeFile(filePath);
            background_image = new ImageView(this);
            background_image.setImageBitmap(bmFrame);
            background_image.setLayoutParams(params);
            layout.addView(background_image);
        } else {
            video_start = false;
            alpha_level_video_index = 0;
            current_video_timestamp = 0;
            mPlaybackView = new TextureView(this);
            mPlaybackView.setLayoutParams(params);
            mPlaybackView.setSurfaceTextureListener(this);
            mTimeAnimator = new TimeAnimator();
            mExtractor = new MediaExtractor();
            layout.addView(mPlaybackView);
        }

        for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                mImageViews[i][j] = new ImageView(this);
                mImageViews[i][j].setId(1 + i * Globals.GRID_Y * Globals.SUB_GRID_Y + j);
                mImageViews[i][j].setImageAlpha(0);
                params = new LayoutParams(width / Globals.GRID_X, height / Globals.GRID_Y);
                if (j == 0) {
                    if (i == 0) {
                        params.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
                        params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
                        mImageViews[i][j].setLayoutParams(params);

                    } else {
                        params.addRule(RelativeLayout.ALIGN_PARENT_TOP);
                        params.addRule(RelativeLayout.RIGHT_OF, mImageViews[i - 1][j].getId());
                        mImageViews[i][j].setLayoutParams(params);
                    }
                } else {
                    params.addRule(RelativeLayout.BELOW, mImageViews[i][j - 1].getId());
                    if (i == 0) {
                        params.addRule(RelativeLayout.ALIGN_PARENT_LEFT);
                    } else {
                        params.addRule(RelativeLayout.RIGHT_OF, mImageViews[i - 1][j].getId());
                    }
                    mImageViews[i][j].setLayoutParams(params);
                }

                mImageViews[i][j].setImageBitmap(icon);
                mImageViews[i][j].bringToFront();
                layout.addView(mImageViews[i][j]);
            }
        }
    }

    public static Bitmap decodeSampledBitmapFromResource(Resources res, int resId,
                                                         int reqWidth, int reqHeight) {

        // First decode with inJustDecodeBounds=true to check dimensions
        final BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeResource(res, resId, options);

        // Calculate inSampleSize
        options.inSampleSize = calculateInSampleSize(options, reqWidth, reqHeight);

        // Decode bitmap with inSampleSize set
        options.inJustDecodeBounds = false;
        return BitmapFactory.decodeResource(res, resId, options);
    }

    public static int calculateInSampleSize(
            BitmapFactory.Options options, int reqWidth, int reqHeight) {
        // Raw height and width of image
        final int height = options.outHeight;
        final int width = options.outWidth;
        int inSampleSize = 1;

        if (height > reqHeight || width > reqWidth) {

            // Calculate ratios of height and width to requested height and width
            final int heightRatio = Math.round((float) height / (float) reqHeight);
            final int widthRatio = Math.round((float) width / (float) reqWidth);

            // Choose the smallest ratio as inSampleSize value, this will guarantee
            // a final image with both dimensions larger than or equal to the
            // requested height and width.
            inSampleSize = heightRatio < widthRatio ? heightRatio : widthRatio;
        }

        return inSampleSize;
    }

    @Override
    protected void onPause() {
        super.onPause();
    }

    //Gaussian filter
    public void getGaussian2d() {
        int centre_x = (int) Math.ceil(Globals.SUB_GRID_X * 1.0 / 2);
        int centre_y = (int) Math.ceil(Globals.SUB_GRID_Y * 1.0 / 2);
        int normalized_index = 3;
        double exponent;
        int i, j;
        for (i = 0; i < Globals.SUB_GRID_X; i++) {
            for (j = 0; j < Globals.SUB_GRID_Y; j++) {
                exponent = (Math.pow((i + 1 - centre_x) * 1.0 / Globals.SUB_GRID_X * normalized_index, 2.0) + Math.pow((j + 1 - centre_y) * 1.0 / Globals.SUB_GRID_Y * normalized_index, 2.0)) / 2;
                gauss2d_matrix[i][j] = Math.exp(-exponent);
            }
        }
    }

    public void getBitmapAlphaLevel(Bitmap picture, Point screen_size) {
        Bitmap scaledBitmap = Bitmap.createScaledBitmap(picture, screen_size.x, screen_size.y, true);
        perfetch_Frame = new Mat(scaledBitmap.getHeight(), scaledBitmap.getWidth(), CvType.CV_8UC1);
        perfetch_Frame_tmp = new Mat(scaledBitmap.getHeight(), scaledBitmap.getWidth(), CvType.CV_8UC3);
        Utils.bitmapToMat(scaledBitmap, perfetch_Frame_tmp);
        Imgproc.cvtColor(perfetch_Frame_tmp, perfetch_Frame, Imgproc.COLOR_RGB2GRAY);
        mat_tmp = new Mat(grid_size_y, grid_size_x, CvType.CV_8UC1);
        int rowStart;
        int rowEnd;
        int colStart;
        int colEnd;
        Scalar scalar_tmp;
        double grid_intensity_tmp;
        double alpha_level_tmp;
        for (int i = 0; i < Globals.GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y; j++) {
                rowStart = j * grid_size_y;
                rowEnd = (j + 1) * grid_size_y - 1;
                colStart = i * grid_size_x;
                colEnd = (i + 1) * grid_size_x - 1;
                mat_tmp = perfetch_Frame.submat(rowStart, rowEnd, colStart, colEnd);
                scalar_tmp = Core.mean(mat_tmp);
                grid_intensity_tmp = scalar_tmp.val[0];
                if (grid_intensity_tmp < 75) {
                    if (grid_intensity_tmp < 50) {
                        if (grid_intensity_tmp < 25) {
                            alpha_level_tmp = 4;
                        } else {
                            alpha_level_tmp = 3;
                        }
                    } else {
                        alpha_level_tmp = 1.5;
                    }
                } else {
                    alpha_level_tmp = 1;
                }
                for (int i1 = 0; i1 < Globals.SUB_GRID_X; i1++) {
                    for (int j1 = 0; j1 < Globals.SUB_GRID_Y; j1++) {
                        alpha_level_image[i * Globals.SUB_GRID_X + i1][j * Globals.SUB_GRID_Y + j1] = alpha_level_tmp * gauss2d_matrix[i1][j1];
                    }
                }
            }
        }
    }

    protected void onResume() {
        super.onResume();

        //onResume we start our timer so it can start when the app comes from the background
        startTimer();
    }

    public void startTimer() {
        //set a new Timer
        timer = new Timer();

        //initialize the TimerTask's job
        initializeTimerTask();

        //schedule the timer, after the first 1000ms the TimerTask will run every 16ms
        timer.schedule(timerTask, Globals.start_time_ms, 16); //
        if (!isImage) {
            DumpFrameTask task = new DumpFrameTask(HiLightTransmitter.this);
            task.execute();
        }
    }

    public void stoptimertask(View v) {
        //stop the timer, if it's not already null
        if (timer != null) {
            timer.cancel();
            timer = null;
        }
    }

    public void initializeTimerTask() {
        timerTask = new TimerTask() {
            public void run() {
                //use a handler to run a toast that shows the current timestamp
                handler.post(new Runnable() {
                    public void run() {
                        now = System.currentTimeMillis();

                        if(now-previous > 10){
                            Log.e("HiLight_refresh", "produced images at rate: " + (now-previous) + " ms");
                            previous = now;
                            if (debug) {
                                if (counter % 100 == 0) {
                                    Log.e("Media_test", "Media_test");
                                }
                            }
                            if (counter % (frame_length * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES) == 0) {
                                counter = 0;
                            }
                            if (!video_start && isImage) {
                                video_start = true;
                                getBitmapAlphaLevel(bmFrame, size);
                            }

                            int k;
                            for (k = 0; k < alpha_level_video_index - 1; k++) {
                                if (current_video_timestamp < alpha_level_video_timestamp[k + 1] && current_video_timestamp > alpha_level_video_timestamp[k]) {
                                    break;
                                }
                            }
                            if (k == alpha_level_video_index - 1) {
                                k = previous_k;
                            } else {
                                previous_k = k;
                            }

                            for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
                                for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                                    int row = i / Globals.SUB_GRID_X;
                                    int column = j / Globals.SUB_GRID_Y;
                                    int alpha = alpha_matrix[counter][row][column];
                                    if (alpha == 0) {
                                        if (isImage) {
                                            mImageViews[i][j].setImageAlpha((int) (2 * alpha_level_image[i][j]));
                                        } else {
                                            mImageViews[i][j].setImageAlpha((int) (2 * alpha_level_video[k][i][j]));
                                        }
                                    } else {
                                        mImageViews[i][j].setImageAlpha(0);
                                    }
                                }
                            }
                            counter++;
                        }
                    }
                });
            }
        };
    }

    private double detect_cut_scene_hist(float[] current_hist_buffer, float[] perfetch_hist_buffer) {
        double diff_histogram = 0;
        for (int i = 0; i < Globals.mHistSizeNum; i++) {
            if (current_hist_buffer[i] != 0 && perfetch_hist_buffer[i] != 0) {
                diff_histogram = diff_histogram + Math.pow((current_hist_buffer[i] - perfetch_hist_buffer[i]), 2.0) / Math.max(current_hist_buffer[i], perfetch_hist_buffer[i]);
            }
        }
        return diff_histogram;
    }

    private double detect_cut_scene_pixel(double[][] current_color_intensity, double[][] perfetch_color_intensity) {
        double diff_pixel = 0;
        for (int i1 = 0; i1 < Globals.GRID_X; i1++) {
            for (int j1 = 0; j1 < Globals.GRID_Y; j1++) {
                for (int i2 = 0; i2 < Globals.GRID_X; i2++) {
                    for (int j2 = 0; j2 < Globals.GRID_Y; j2++) {
                        diff_pixel = diff_pixel + Math.abs(current_color_intensity[i1][j1] - perfetch_color_intensity[i2][j2]);
                    }
                }
            }
        }
        return diff_pixel;
    }

    private void get_alpha_level_video(int frame_timestamp) {
        alpha_level_video_timestamp[alpha_level_video_index] = frame_timestamp;
        for (int i = 0; i < Globals.GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y; j++) {
                double color_intensity_diff = Math.abs(perfetch_color_intensity[i][j] - current_color_intensity[i][j]);
                double color_intensity_mean = Math.abs(perfetch_color_intensity[i][j] + current_color_intensity[i][j]) / 2;
                double color_intensity_changes_ratio = color_intensity_diff / color_intensity_mean * 200;
                if (color_intensity_changes_ratio < 4) {
                    if (color_intensity_mean < 75) {
                        if (color_intensity_mean < 50) {
                            if (color_intensity_mean < 25) {
                                alpha_level_video[alpha_level_video_index][i][j] = 4;
                            } else {
                                alpha_level_video[alpha_level_video_index][i][j] = 3;
                            }
                        } else {
                            alpha_level_video[alpha_level_video_index][i][j] = 1.5;
                        }
                    } else {
                        alpha_level_video[alpha_level_video_index][i][j] = 1;
                    }
                } else {
                    if (color_intensity_mean < 75) {
                        if (color_intensity_mean < 50) {
                            if (color_intensity_mean < 25) {
                                alpha_level_video[alpha_level_video_index][i][j] = 8;
                            } else {
                                alpha_level_video[alpha_level_video_index][i][j] = 6;
                            }
                        } else {
                            alpha_level_video[alpha_level_video_index][i][j] = 3;
                        }
                    } else {
                        alpha_level_video[alpha_level_video_index][i][j] = 1.6;
                    }
                }
            }
        }
        alpha_level_video_index++;
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
        perfetch_finish();
        if (mTimeAnimator != null && mTimeAnimator.isRunning()) {
            mTimeAnimator.end();
        }
        if (mCodecWrapper != null) {
            mCodecWrapper.stopAndRelease();
            mExtractor.release();
        }
        if (timer != null) {
            timer.cancel();
            timer = null;
        }
        if (bmFrame != null) {
            bmFrame.recycle();
        }
        if (current_Frame != null) {
            current_Frame.release();
        }
        if (perfetch_Frame != null) {
            perfetch_Frame.release();
        }
        handler.removeCallbacksAndMessages(null);
    }

    public String getAlphaMatrix(String input_string) {
        String repeat_string = "";

        for (int i = 0; i < input_string.length(); i++) {
            String tmp = input_string.substring(i, i + 1);
            for (int j = 0; j < Globals.REPEAT_TIME; j++) {
                repeat_string = repeat_string + tmp;
            }
        }

        String alpha_vector = "";
        for (int i = 0; i < repeat_string.length(); i++) {
            char character = repeat_string.charAt(i);
            int ascii = (int) character;
            if (String.valueOf(character).equals("0")) {
                ascii = 126;
            } else if (String.valueOf(character).equals("1")) {
                ascii = 91;
            } else if (String.valueOf(character).equals("2")) {
                ascii = 92;
            } else if (String.valueOf(character).equals("3")) {
                ascii = 93;
            } else if (String.valueOf(character).equals("4")) {
                ascii = 94;
            } else if (String.valueOf(character).equals("5")) {
                ascii = 95;
            } else if (String.valueOf(character).equals("6")) {
                ascii = 96;
            } else if (String.valueOf(character).equals("7")) {
                ascii = 123;
            } else if (String.valueOf(character).equals("8")) {
                ascii = 124;
            } else if (String.valueOf(character).equals("9")) {
                ascii = 125;
            } else if (String.valueOf(character).equals(" ")) {
                ascii = 64;
            }
            String tmp_string = Integer.toBinaryString(ascii);
            while (tmp_string.length() < 6) {
                tmp_string = "0" + tmp_string;
            }
            alpha_vector = alpha_vector + tmp_string.substring(tmp_string.length() - 6, tmp_string.length());
        }
        return alpha_vector;
    }

    @Override
    public void onSurfaceTextureAvailable(SurfaceTexture surface, int width,
                                          int height) {

        try {
            Thread.sleep(1000);
        } catch (InterruptedException e1) {
            e1.printStackTrace();
        }

        try {
            // BEGIN_INCLUDE(initialize_extractor)
            mExtractor.setDataSource(filePath);
            int nTracks = mExtractor.getTrackCount();
            // Begin by unselecting all of the tracks in the extractor, so we won't see
            // any tracks that we haven't explicitly selected.
            for (int i = 0; i < nTracks; ++i) {
                mExtractor.unselectTrack(i);
            }

            // Find the first video track in the stream. In a real-world application
            // it's possible that the stream would contain multiple tracks, but this
            // sample assumes that we just want to play the first one.
            for (int i = 0; i < nTracks; ++i) {
                // Try to create a video codec for this track. This call will return null if the
                // track is not a video track, or not a recognized video format. Once it returns
                // a valid MediaCodecWrapper, we can break out of the loop.
                mCodecWrapper = MediaCodecWrapper.fromVideoFormat(mExtractor.getTrackFormat(i),
                        new Surface(mPlaybackView.getSurfaceTexture()));
                if (mCodecWrapper != null) {
                    mExtractor.selectTrack(i);
                    break;
                }
            }

            // By using a {@link TimeAnimator}, we can sync our media rendering commands with
            // the system display frame rendering. The animator ticks as the {@link Choreographer}
            // recieves VSYNC events.
            mTimeAnimator.setTimeListener(new TimeAnimator.TimeListener() {
                @Override
                public void onTimeUpdate(final TimeAnimator animation,
                                         final long totalTime,
                                         final long deltaTime) {

                    boolean isEos = ((mExtractor.getSampleFlags() & MediaCodec
                            .BUFFER_FLAG_END_OF_STREAM) == MediaCodec.BUFFER_FLAG_END_OF_STREAM);

                    // BEGIN_INCLUDE(write_sample)
                    if (!isEos) {
                        // Try to submit the sample to the codec and if successful advance the
                        // extractor to the next available sample to read.
                        boolean result = mCodecWrapper.writeSample(mExtractor, false,
                                mExtractor.getSampleTime(), mExtractor.getSampleFlags());

                        if (result) {
                            // Advancing the extractor is a blocking operation and it MUST be
                            // executed outside the main thread in real applications.
                            mExtractor.advance();
                        }
                    }
                    //Log.e("HiLight_alpha_level_video_index1", "HiLight_alpha_level_video_index 1="+String.valueOf(current_video_timestamp));
                    // END_INCLUDE(write_sample)
                    current_video_timestamp = mExtractor.getSampleTime();

                    // Examine the sample at the head of the queue to see if its ready to be
                    // rendered and is not zero sized End-of-Stream record.
                    MediaCodec.BufferInfo out_bufferInfo = new MediaCodec.BufferInfo();
                    mCodecWrapper.peekSample(out_bufferInfo);

                    // BEGIN_INCLUDE(render_sample)
                    if (out_bufferInfo.size <= 0 && isEos) {
                        mTimeAnimator.end();
                    } else if (out_bufferInfo.presentationTimeUs / 1000 < totalTime) {
                        // Pop the sample off the queue and send it to {@link Surface}
                        mCodecWrapper.popSample(true);
                    }
                    // END_INCLUDE(render_sample)

                }
            });

            // We're all set. Kick off the animator to process buffers and render video frames as
            // they become available
            mTimeAnimator.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private static class DumpFrameTask extends AsyncTask<Void, Integer, Void> {
        HiLightTransmitter mlOuterAct;

        DumpFrameTask(HiLightTransmitter pContext) {
            mlOuterAct = pContext;
        }

        @Override
        protected void onPreExecute() {
        }

        @Override
        protected Void doInBackground(Void... params) {
            perfetch_start(mlOuterAct, filePath, video_fps);
            return null;
        }

        @Override
        protected void onPostExecute(Void param) {
        }
    }

    private void video_perfetch_thread(Bitmap bitmap, String pPath, int frame_timestamp) {
        if (!video_start) {
            video_start = true;
            prefetch_video_milis = Globals.start_time_ms * 1000;

            //init parameters
            current_Frame = new Mat(bitmap.getHeight(), bitmap.getWidth(), CvType.CV_8UC1);
            perfetch_Frame = new Mat(bitmap.getHeight(), bitmap.getWidth(), CvType.CV_8UC1);
            current_Frame_tmp = new Mat(bitmap.getHeight(), bitmap.getWidth(), CvType.CV_8UC3);
            perfetch_Frame_tmp = new Mat(bitmap.getHeight(), bitmap.getWidth(), CvType.CV_8UC3);
            frame_x = bitmap.getWidth();
            frame_y = bitmap.getHeight();
            frame_size_x = frame_x / Globals.GRID_X;
            frame_size_y = frame_y / Globals.GRID_Y;

            Log.e("HiLight_start:", String.valueOf(frame_timestamp));
            Utils.bitmapToMat(bitmap, current_Frame_tmp);
            Imgproc.cvtColor(current_Frame_tmp, current_Frame, Imgproc.COLOR_RGB2GRAY);

            /// Separate the image in 3 places ( B, G and R )
            bgr_planes = new ArrayList<Mat>();
            Core.split(current_Frame, bgr_planes);

            /// Establish the number of bins
            MatOfInt histSize = new MatOfInt(Globals.mHistSizeNum);

            /// Set the ranges ( for B,G,R) )
            MatOfFloat range = new MatOfFloat(0f, Float.valueOf(Globals.mHistSizeNum));
            MatOfInt channels = new MatOfInt(0);

            hist = new Mat();
            /// Compute the histograms:
            Imgproc.calcHist(bgr_planes, channels, new Mat(), hist, histSize, range);
            hist.get(0, 0, current_hist_buffer);

            float current_hist_buffer_sum = 0;
            for (int i = 0; i < Globals.mHistSizeNum; i++) {
                current_hist_buffer_sum = current_hist_buffer_sum + current_hist_buffer[i];
            }
            for (int i = 0; i < Globals.mHistSizeNum; i++) {
                current_hist_buffer[i] = current_hist_buffer[i] / current_hist_buffer_sum;
            }

            mat_tmp = new Mat(frame_size_y, frame_size_x, CvType.CV_8UC1);
            Scalar scalar_tmp;

            int rowStart;
            int rowEnd;
            int colStart;
            int colEnd;
            for (int i = 0; i < Globals.GRID_X; i++) {
                for (int j = 0; j < Globals.GRID_Y; j++) {
                    rowStart = j * frame_size_y;
                    rowEnd = (j + 1) * frame_size_y - 1;
                    colStart = i * frame_size_x;
                    colEnd = (i + 1) * frame_size_x - 1;
                    mat_tmp = current_Frame.submat(rowStart, rowEnd, colStart, colEnd);
                    scalar_tmp = Core.mean(mat_tmp);
                    current_color_intensity[i][j] = scalar_tmp.val[0];
                }
            }
            video_prefetch_counter++;
        } else {
            prefetch_video_milis = (video_prefetch_counter * 16 * Globals.MODULATION_LENGTH + Globals.start_time_ms) * 1000;
            Log.e("HiLight_start", "HiLight_start " + String.valueOf(frame_timestamp));
            Utils.bitmapToMat(bitmap, perfetch_Frame_tmp);
            perfetch_Frame = new Mat(bitmap.getHeight(), bitmap.getWidth(), CvType.CV_8UC1);
            Imgproc.cvtColor(perfetch_Frame_tmp, perfetch_Frame, Imgproc.COLOR_RGB2GRAY);
            perfetch_Frame_tmp.release();

//			/// Separate the image in 3 places ( B, G and R )
//			bgr_planes = new ArrayList<Mat>();
//			Core.split( perfetch_Frame, bgr_planes );
//
//			/// Establish the number of bins
//			MatOfInt histSize = new MatOfInt(Globals.mHistSizeNum);
//
//			/// Set the ranges ( for B,G,R) )
//			MatOfFloat range = new MatOfFloat(0f, Float.valueOf(Globals.mHistSizeNum));
//			MatOfInt channels = new MatOfInt(0);
//
//			/// Compute the histograms:
//			Imgproc.calcHist(bgr_planes, channels, new Mat(), hist, histSize, range);
//			hist.get(0, 0, perfetch_hist_buffer);
//
//			float perfetch_hist_buffer_sum = 0;
//			for (int i = 0; i < Globals.mHistSizeNum; i++){
//				perfetch_hist_buffer_sum = perfetch_hist_buffer_sum + perfetch_hist_buffer[i];
//			}
//			for (int i = 0; i < Globals.mHistSizeNum; i++){
//				perfetch_hist_buffer[i] = perfetch_hist_buffer[i]/perfetch_hist_buffer_sum;
//			}

            //Compute pixel based feature
            Scalar scalar_tmp;
            int rowStart;
            int rowEnd;
            int colStart;
            int colEnd;
            for (int i = 0; i < Globals.GRID_X; i++) {
                for (int j = 0; j < Globals.GRID_Y; j++) {
                    rowStart = j * frame_size_y;
                    rowEnd = (j + 1) * frame_size_y - 1;
                    colStart = i * frame_size_x;
                    colEnd = (i + 1) * frame_size_x - 1;
                    mat_tmp = perfetch_Frame.submat(rowStart, rowEnd, colStart, colEnd);
                    scalar_tmp = Core.mean(mat_tmp);
                    perfetch_color_intensity[i][j] = scalar_tmp.val[0];
                }
            }

            //double cut_scene_detector_hist = detect_cut_scene_hist(current_hist_buffer, perfetch_hist_buffer);
            //Log.e("HiLight_hist_index:", String.valueOf(cut_scene_detector_hist));
            //double cut_scene_detector_pixel = detect_cut_scene_pixel(current_color_intensity, perfetch_color_intensity);
            //Log.e("HiLight_pixel_index:", String.valueOf(cut_scene_detector_pixel));
            get_alpha_level_video(frame_timestamp);

            video_prefetch_counter++;
            current_color_intensity = perfetch_color_intensity.clone();
            current_hist_buffer = perfetch_hist_buffer.clone();
        }
    }

    @Override
    public void onSurfaceTextureSizeChanged(SurfaceTexture surface, int width,
                                            int height) {
    }

    @Override
    public boolean onSurfaceTextureDestroyed(SurfaceTexture surface) {
        return false;
    }

    @Override
    public void onSurfaceTextureUpdated(SurfaceTexture surface) {
    }

    private static native int perfetch_start(HiLightTransmitter pObject, String pVideoFileName, int video_fps);

    private static native int perfetch_finish();

    static {
        System.loadLibrary("ffmpeg_perfetcher");
    }
}
