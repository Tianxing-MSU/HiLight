/*
 * Created by Tianxing Li
 *
 * Copyright 2015 The Trustees Dartmouth College,
 * Permission to use for Non-commercial Research Purposes Only.
 * All Other Rights Reserved.
 */

package edu.dartnets.hilight;

import android.app.Service;
import android.content.Intent;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PixelFormat;
import android.os.Bundle;
import android.os.Handler;
import android.os.IBinder;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.ImageView;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;

import org.opencv.android.OpenCVLoader;
import java.util.Timer;
import java.util.TimerTask;

public class HiLight_TX_Universal extends Service {
    private WindowManager windowManager;
    private RelativeLayout hilightWindow;
    private String toBeEncoded;

    // Timer variables.
    Timer timer;
    TimerTask timerTask;
    final Handler handler = new Handler();
    int counter = 0;
    int transmission_length_counter = 0;
    long previous = 0;
    long now;

    // Setup variables.
    int frame_length = 0;
    int[][][] alpha_matrix;
    ImageView[][] mImageViews;
    double[][] alpha_level_image;

    static {
        //Init OpenCV libs
        if (!OpenCVLoader.initDebug()) {
            // Handle initialization error
            System.out.println("BAD");
        }
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onCreate() {
        super.onCreate();

        // Get the window manager.
        windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);

        // Create the hilight wrapper.
        hilightWindow = new RelativeLayout(this);

        hilightWindow.setLayoutParams(new RelativeLayout.LayoutParams(
                        RelativeLayout.LayoutParams.MATCH_PARENT,
                        RelativeLayout.LayoutParams.MATCH_PARENT)
        );

        hilightWindow.setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);

        // Add the layout to the window.
        windowManager.addView(hilightWindow, new WindowManager.LayoutParams(
                        WindowManager.LayoutParams.MATCH_PARENT,
                        WindowManager.LayoutParams.MATCH_PARENT,
                        WindowManager.LayoutParams.TYPE_PHONE,
                        WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE,
                        PixelFormat.TRANSLUCENT)
        );
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Get the data to be transmitted.
        Bundle extras = intent.getExtras();
        if (extras != null) {
            toBeEncoded = extras.getString("encode_info");
        }

        //set default encoding message if the user does not specify it.
        if (toBeEncoded.equals("")) {
            toBeEncoded = "MobiSys 2015";
        }

        // Initialize hilight.
        initializeHilight(toBeEncoded);

        // Start the timer.
        initializeHilightTimer();

        return START_NOT_STICKY;
    }

    private void initializeHilight(String toBeEncoded) {
        // Get the alpha vector.
        String alpha_vector = getAlphaMatrix(toBeEncoded);

        // Get the alpha matrix.
        frame_length = alpha_vector.length() / (Globals.GRID_X * Globals.GRID_Y);
        alpha_matrix = new int[frame_length * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES][Globals.GRID_X][Globals.GRID_Y];

        // Initialize the alpha matrix blank frames to 1's
        for (int i = 0; i < Globals.BLANK_FRAMES; i++) {
            for (int j = 0; j < Globals.GRID_X; j++) {
                for (int k = 0; k < Globals.GRID_Y; k++) {
                    alpha_matrix[i][j][k] = 1;
                }
            }
        }

        // Initialize the alpha level image to all 1's
        alpha_level_image = new double[Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_Y];
        for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                alpha_level_image[i][j] = 1;
            }
        }

        // Set preamble data
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
                    } else
                        alpha_matrix[i + Globals.BLANK_FRAMES - Globals.MODULATION_LENGTH][j][k] = 0;
                }
            }
        }

        //BFSK modulation 30Hz for bit '0' and 20Hz for bit '1'
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

        mImageViews = new ImageView[Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_Y];

        //Get black background for the foreground imageview
        Bitmap icon = decodeSampledBitmapFromResource(this.getResources(), R.drawable.single_color_img, Globals.grid_size_x, Globals.grid_size_y);
        icon = Bitmap.createScaledBitmap(icon, Globals.grid_size_x, Globals.grid_size_y, true);

        LayoutParams params;

        //set position for each imageView grid
        for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                mImageViews[i][j] = new ImageView(this);
                mImageViews[i][j].setId(1 + i * Globals.GRID_Y * Globals.SUB_GRID_Y + j);
                mImageViews[i][j].setImageAlpha(0);
                params = new LayoutParams(Globals.grid_size_x, Globals.grid_size_y);
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
                }else {
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
                hilightWindow.addView(mImageViews[i][j]);
            }
        }
    }

    //clear timer and handler
    @Override
    public void onDestroy() {
        super.onDestroy();
        if (hilightWindow != null) windowManager.removeView(hilightWindow);

        if (timer != null) {
            timer.cancel();
            timer = null;
        }
        handler.removeCallbacksAndMessages(null);
    }

    private void initializeHilightTimer() {
        timer = new Timer();

        //initialize the TimerTask's job
        initializeTimerTask();

        //schedule the timer, after the start_time_ms TimerTask will run every 16ms
        timer.schedule(timerTask, Globals.start_time_ms, 16);
    }

    public void initializeTimerTask() {
        timerTask = new TimerTask() {
            public void run() {
                //use a handler to run a toast that shows the current timestamp
                handler.post(new Runnable() {
                    public void run() {
                        now = System.currentTimeMillis();
                        if(now-previous > 10) {
                            Log.e("HiLight_refresh", "produced images at rate: " + (now - previous) + " ms");
                            previous = now;
                            if (counter % (frame_length * Globals.MODULATION_LENGTH + Globals.BLANK_FRAMES) == 0) {
                                counter = 0;
                            }
                            for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++) {
                                for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++) {
                                    int row = i / Globals.SUB_GRID_X;
                                    int column = j / Globals.SUB_GRID_Y;
                                    int alpha = alpha_matrix[counter][row][column];
                                    if (alpha == 0) {
                                        //set alpha value on each ImageView grid
//                                        mImageViews[i][j].setImageAlpha((int) (2 * Globals.alpha_level_image[i][j]));
                                        mImageViews[i][j].setImageAlpha((int) (2 * alpha_level_image[i][j]));
                                    } else {
                                        mImageViews[i][j].setImageAlpha(0);
                                    }
                                }
                            }
                            counter++;
                            transmission_length_counter++;
                        }
                    }
                });
            }
        };
    }

    private String getAlphaMatrix(String input_string) {
        String repeat_string = "";

        for (int i = 0; i < input_string.length(); i++) {
            String tmp = input_string.substring(i, i + 1);
            for (int j = 0; j < Globals.REPEAT_TIME; j++) {
                repeat_string = repeat_string + tmp;
            }
        }

        String alpha_vector = "";
        // convert encoding message to ASCII code
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

    //get image from the source
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
}
