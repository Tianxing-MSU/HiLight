/*
 * Created by Tianxing Li
 *
 * Copyright 2015 The Trustees Dartmouth College,
 * Permission to use for Non-commercial Research Purposes Only.
 * All Other Rights Reserved.
 */

package edu.dartnets.hilight;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.PixelFormat;
import android.graphics.Point;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.Image;
import android.media.ImageReader;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.MediaStore;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.Display;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.media.ThumbnailUtils;
import android.widget.Toast;

import org.opencv.android.OpenCVLoader;
import org.opencv.android.Utils;
import org.opencv.core.Core;
import org.opencv.core.CvType;
import org.opencv.core.Mat;
import org.opencv.core.Scalar;
import org.opencv.imgproc.Imgproc;

import java.io.FileInputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Timer;
import java.util.TimerTask;

//HiLight main class
public class HiLight_MainActivity extends Activity {
    public static final int REQUEST_CODE_SELECT_FROM_GALLERY = 1;

    private static final String IMAGE_UNSPECIFIED = "image/*";
    private static final String URI_INSTANCE_STATE_KEY = "saved_uri";

    // input type flags
    public static final int DEMO_IMAGE = 0;
    public static final int DEMO_VIDEO = 1;
    public static final int DEMO_UNIVERSAL_SERVICE = 2;

    private ImageView mImageView;
    private RadioGroup mRadioGroup;
    private Button mChange;
    private Uri mImageCaptureUri;
    private String filePath;
    private int service_flag = -1;

    //Screen capture related parameters
    private static final String TAG = HiLight_MainActivity.class.getName();
    private static final int REQUEST_CODE = 100;

    private MediaProjectionManager mProjectionManager;
    private MediaProjection mProjection;
    private ImageReader mImageReader;
    private Handler mHandler;
    private boolean isbmframeInit = false;

    // Timer variables.
    Timer timer;
    TimerTask timerTask;
    private static boolean refresh_screen_content = true;

    static {
        if (!OpenCVLoader.initDebug()) {
            // Handle initialization error
            Log.e(TAG, "BAD OpenCV handler");
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_hilight);

        // Set references to the image/radio/universal button group.
        mImageView = (ImageView) findViewById(R.id.transmissionImage);
        mRadioGroup = (RadioGroup) findViewById(R.id.radioTransmissionMode);
        mChange = (Button) findViewById(R.id.btnChangePhoto);

        mRadioGroup.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener()
        {
            @Override
            public void onCheckedChanged(RadioGroup group, int checgamekedId) {
                // checkedId is the RadioButton selected. Disable Open button when the user select Game radio button
                if (checgamekedId == R.id.radioImage || checgamekedId == R.id.radioVideo) {
                    mChange.setEnabled(true);
                }else{
                    mChange.setEnabled(false);
                }
            }
        });

        if (savedInstanceState != null) {
            mImageCaptureUri = savedInstanceState.getParcelable(URI_INSTANCE_STATE_KEY);
        }

        loadProfile();

        //Init alpha level matrix
        Globals.alpha_level_image = new double[Globals.GRID_X * Globals.SUB_GRID_X][Globals.GRID_Y * Globals.SUB_GRID_Y];
        for (int i = 0; i < Globals.GRID_X * Globals.SUB_GRID_X; i++){
            for (int j = 0; j < Globals.GRID_Y * Globals.SUB_GRID_Y; j++){
                Globals.alpha_level_image[i][j] = 0;
            }
        }

        // call for the projection manager
        mProjectionManager = (MediaProjectionManager) getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        // start capture handling thread
        new Thread() {
            @Override
            public void run() {
                Looper.prepare();
                mHandler = new Handler();
                Looper.loop();
            }
        }.start();

        initializeHilightTimer();

        //Init display size
        WindowManager window = (WindowManager) getSystemService(Context.WINDOW_SERVICE);
        Display display = window.getDefaultDisplay();
        Globals.size = new Point();
        display.getSize(Globals.size);
        Globals.grid_size_x = Globals.size.x / Globals.GRID_X;
        Globals.grid_size_y = Globals.size.y / Globals.GRID_Y;

        View decorView = getWindow().getDecorView();
        // Hide the status bar.
        int uiOptions = View.SYSTEM_UI_FLAG_FULLSCREEN;
        decorView.setSystemUiVisibility(uiOptions);
    }

    @Override
    protected void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        // Save the image capture uri before the activity goes into background
        outState.putParcelable(URI_INSTANCE_STATE_KEY, mImageCaptureUri);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_hi_light, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();

        if (id == R.id.exit_settings) {
            finish();
            return true;
        }

        return super.onOptionsItemSelected(item);
    }

    public void onStartClicked(View v) {
        int selected = mRadioGroup.getCheckedRadioButtonId();
        String toBeEncoded = ((EditText) findViewById(R.id.editTextEncodingInformation)).getText().toString();

        if (selected == R.id.radioImage || selected == R.id.radioVideo) {
            //Check if file directory is null
            if (filePath == null){
                Toast.makeText(getApplicationContext(), "Please choose image/video.", Toast.LENGTH_LONG).show();
                return;
            }

            //send intent message to image/video class
            Intent intent = new Intent(this, HiLightTransmitter.class);
            Bundle b = new Bundle();
            b.putString("filepath", filePath); //input file path
            b.putString("encode_info", toBeEncoded); //encode data
            b.putInt("input_type", selected == R.id.radioImage ? DEMO_IMAGE : DEMO_VIDEO); // input type
            intent.putExtras(b);
            startActivity(intent);

            //Set service flag
            if (selected == R.id.radioImage){
                service_flag = DEMO_IMAGE;
            }else{
                service_flag = DEMO_VIDEO;
            }

        } else if (selected == R.id.radioUniversalService) {

            //send intent message to universal class
            Intent intent = new Intent(this, HiLight_TX_Universal.class);
            Bundle b = new Bundle();
            b.putString("encode_info", toBeEncoded);
            intent.putExtras(b);
            startService(intent);
            service_flag = DEMO_UNIVERSAL_SERVICE;
        }
//        startActivityForResult(mProjectionManager.createScreenCaptureIntent(), REQUEST_CODE);
    }

    //Callback for stop transmission button
    public void onCancelClicked(View v) {
//        if (service_flag >= 0){
//            mHandler.post(new Runnable() {
//                @Override
//                public void run() {
//                    mProjection.stop();G
//                }
//            });
//        }
        if (service_flag == DEMO_UNIVERSAL_SERVICE){
            Intent intent = new Intent(this, HiLight_TX_Universal.class);
            stopService(intent);
        }
        service_flag = -1;
    }

    //Callback for Open buttom
    public void onChangePhotoClicked(View v) {
        // changing the content image
        Intent intent = new Intent();
        intent.setType(IMAGE_UNSPECIFIED);
        intent.setAction(Intent.ACTION_GET_CONTENT);
        // REQUEST_CODE_SELECT_FROM_GALLERY is an integer tag you
        // defined to identify the activity in onActivityResult()
        // when it returns
        startActivityForResult(intent, REQUEST_CODE_SELECT_FROM_GALLERY);
    }

    // Handle date after activity returns.
    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {

        if (requestCode == REQUEST_CODE) {
            mProjection = mProjectionManager.getMediaProjection(resultCode, data);

            if (mProjection != null) {

                DisplayMetrics metrics = getResources().getDisplayMetrics();
                int density = metrics.densityDpi;
                //show only the content
                int flags = DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY | DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC;

                //set display size
                final int width = Globals.size.x;
                final int height = Globals.size.y;

                mImageReader = ImageReader.newInstance(width, height, PixelFormat.RGBA_8888, 2);
                mProjection.createVirtualDisplay("screencap", width, height, density, flags, mImageReader.getSurface(), new VirtualDisplayCallback(), mHandler);
                mImageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {

                    @Override
                    public void onImageAvailable(ImageReader reader) {
                        Image image = null;
                        try {
                            if (refresh_screen_content){
                                image = mImageReader.acquireLatestImage();
                                if (image != null) {
                                    refresh_screen_content = false;
                                    Image.Plane[] planes = image.getPlanes();
                                    ByteBuffer buffer = planes[0].getBuffer();
                                    int pixelStride = planes[0].getPixelStride();
                                    int rowStride = planes[0].getRowStride();
                                    int rowPadding = rowStride - pixelStride * width;

                                    // create bitmap
                                    if (!isbmframeInit){
                                        isbmframeInit = true;
//                                        Globals.bmFrame_ori = Bitmap.createBitmap(width + rowPadding / pixelStride, height, Bitmap.Config.ARGB_8888);

                                        //resize image content
                                        Globals.bmFrame_ori = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
                                        Globals.bmFrame = Bitmap.createBitmap(width/Globals.IMG_RESIZE, height/Globals.IMG_RESIZE, Bitmap.Config.ARGB_8888);
                                        Globals.current_Frame = new Mat(Globals.bmFrame.getHeight(), Globals.bmFrame.getWidth(), CvType.CV_8UC1);
                                        Globals.current_Frame_tmp = new Mat(Globals.bmFrame.getHeight(), Globals.bmFrame.getWidth(), CvType.CV_8UC3);
                                        Globals.mat_tmp = new Mat(Globals.grid_size_y/Globals.IMG_RESIZE, Globals.grid_size_x/Globals.IMG_RESIZE, CvType.CV_8UC1);
                                    }

                                    Globals.bmFrame_ori.copyPixelsFromBuffer(buffer);
                                    Globals.bmFrame = Bitmap.createScaledBitmap(Globals.bmFrame_ori, width/Globals.IMG_RESIZE, height/Globals.IMG_RESIZE, true);

                                    getBitmapAlphaLevel();
                                }
                            }
                        } catch (Exception e) {
                            e.printStackTrace();
                        } finally {
                            if (image != null) {
                                image.close();
                            }
                        }
                    }
                }, mHandler);
            }
        }
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != RESULT_OK)
            return;

        switch (requestCode) {
            case REQUEST_CODE_SELECT_FROM_GALLERY:
                // Send selected image/video from gallery for cropping
                int selected = mRadioGroup.getCheckedRadioButtonId();
                Bitmap yourSelectedImage = null;
                mImageCaptureUri = data.getData();
                String[] filePathColumn = {MediaStore.Images.Media.DATA};

                Cursor cursor = getContentResolver().query(mImageCaptureUri, filePathColumn, null, null, null);
                cursor.moveToFirst();

                int columnIndex = cursor.getColumnIndex(filePathColumn[0]);
                filePath = cursor.getString(columnIndex);
                cursor.close();

                if (selected == R.id.radioImage ) {
                    yourSelectedImage = BitmapFactory.decodeFile(filePath);
                }else if (selected == R.id.radioVideo){
                    //get thumbnail for the video file
                    yourSelectedImage = ThumbnailUtils.createVideoThumbnail(filePath, MediaStore.Video.Thumbnails.MINI_KIND);
                }

                mImageView.setImageBitmap(yourSelectedImage);
                break;
        }
    }

    private void loadProfile() {

        String key, str_val;
        int int_val;

        // Load and update all views
        key = getString(R.string.preference_name);
        SharedPreferences prefs = getSharedPreferences(key, MODE_PRIVATE);

        // Load input test settings
        key = getString(R.string.preference_key_input);
        str_val = prefs.getString(key, "");
        ((EditText) findViewById(R.id.editTextEncodingInformation)).setText(str_val);

        // Load radio group setting and set radio box
        key = getString(R.string.preference_key_input_type);
        int_val = prefs.getInt(key, -1);
        if (int_val >= 0) {
            RadioButton radioBtn = (RadioButton) ((RadioGroup) findViewById(R.id.radioTransmissionMode))
                    .getChildAt(int_val);
            radioBtn.setChecked(true);
        }

        // Load example image from internal storage
        try {
            FileInputStream fis = openFileInput(getString(R.string.input_file_name));
            Bitmap bmap = BitmapFactory.decodeStream(fis);
            mImageView.setImageBitmap(bmap);
            fis.close();
        } catch (IOException e) {
            // Default photo if no photo saved before.
            mImageView.setImageResource(R.drawable.default_image);
        }
    }

    private class VirtualDisplayCallback extends VirtualDisplay.Callback {

        @Override
        public void onPaused() {
            super.onPaused();
            Log.e(TAG, "VirtualDisplayCallback: onPaused");
        }

        @Override
        public void onResumed() {
            super.onResumed();
            Log.e(TAG, "VirtualDisplayCallback: onResumed");
        }

        @Override
        public void onStopped() {
            super.onStopped();
            Log.e(TAG, "VirtualDisplayCallback: onStopped");
        }
    }

    //compute alpha change level
    public void getBitmapAlphaLevel() {
        Utils.bitmapToMat(Globals.bmFrame, Globals.current_Frame_tmp);
        Imgproc.cvtColor(Globals.current_Frame_tmp, Globals.current_Frame, Imgproc.COLOR_RGB2GRAY);
        int rowStart;
        int rowEnd;
        int colStart;
        int colEnd;
        Scalar scalar_tmp;
        double grid_intensity_tmp;
        double alpha_level_tmp;
        for (int i = 0; i < Globals.GRID_X; i++) {
            for (int j = 0; j < Globals.GRID_Y; j++) {
                rowStart = j * Globals.grid_size_y/Globals.IMG_RESIZE;
                rowEnd = (j + 1) * Globals.grid_size_y/Globals.IMG_RESIZE - 1;
                colStart = i * Globals.grid_size_x/Globals.IMG_RESIZE;
                colEnd = (i + 1) * Globals.grid_size_x/Globals.IMG_RESIZE - 1;
                Globals.mat_tmp = Globals.current_Frame.submat(rowStart, rowEnd, colStart, colEnd);
                scalar_tmp = Core.mean(Globals.mat_tmp);
                grid_intensity_tmp = scalar_tmp.val[0];

                // set alpha change level
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
                Globals.alpha_level_image[i][j] = alpha_level_tmp;
            }
        }
    }

    //set transmission timer
    private void initializeHilightTimer() {
        timer = new Timer();

        //initialize the TimerTask's job
        initializeTimerTask();

        //schedule the timer, after the first 1000ms the TimerTask will run every 16ms
        timer.schedule(timerTask, 0, 1000);
    }

    public void initializeTimerTask() {
        timerTask = new TimerTask() {
            public void run() {
                //use a handler to run a toast that shows the current timestamp
                mHandler.post(new Runnable() {
                    public void run() {
                        refresh_screen_content = true;
                    }
                });
            }
        };
    }
}