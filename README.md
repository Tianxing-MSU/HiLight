HiLight: Real-Time Screen-Camera Communication Behind the Scene
================================================================


HiLight is a new form of real-time screen-camera communication system without showing any coded images (e.g., barcodes) on smart devices. HiLight encodes data into pixel translucency change atop any screen content, so that camera-equipped devices can fetch the data by turning their cameras to the screen. Both the transmitter and receiver run at the application layer on off-the-shelf mobile platforms and there is no low-level modification or hacking. To run the code, you will need the following:

* Transmitter: An Android device with 5.0+ OS. We suggest Samsung Tab S tablet for a better performance.

* Receiver: An iPhone 5s or a later iPhone. You may also need an iOS developer account to run the code.

For the design detail of HiLight, please refer to our project website: [HiLight project website](http://dartnets.cs.dartmouth.edu/hilight)


## Transmitter

The HiLight transmitter runs on the Android 5.0+ platform. We suggest Samsung Tab S tablet for a better performance.

We have compiled a customized FFmpeg-based video prefetcher (libffmpeg_prefetcher.so), based on [a ffmpeg tutorial](http://dranger.com/ffmpeg/tutorial07.html). The source code is under the HiLightTransmitter/jni folder. We have also leveraged [a media codec wrapper](https://developer.android.com/samples/BasicMediaDecoder/src/com.example.android.common.media/MediaCodecWrapper.html) from an Android Open Source Project in AndroidStudio IDE.

To transmit data from the TX app:

1. Select either the "Image"/"Video" button or "Universal" bottom within the radio button group in the middle of the app. If you select the former, you need to further specify the image/video file you want to present on the screen.

2. Type the words you want to transmit on the screen in the textbox. For the current setting, the transmission package must contain exactly 12 characters, including 52 English letters (w and w/o capitalization), 10 numbers from 0 – 9, the blank space, and the underline symbol. You can customize the number of characters to transmit in the corresponding header file (HiLight_Receiver.h). You can send multiple packages to transmit more characters.

3. Click the "Start transmission" button to start the data transmission.

4. If you select either the "Image"/"Video", click the Android Back button on your phone to stop the data transmission. If you select "Universal", click the "Stop transmission" button to stop the data transmission.

## Receiver

The HiLight receiver runs on the iOS 7.0+ platform, because our current system runs at 60 FPS and only iPhone 5s or later devices can support it. The current code setting is for iPhones 5s. For newer phones, you will need to change parameters such as the number of pixels on the display in the code. 

Our 60-FPS camera capturing is built on the [SlowMotionVideoRecorder project on GitHub](https://github.com/shu223/SlowMotionVideoRecorder).

To receive data from the RX app:

1. Click the "60 FPS" button to switch the camera frame rate to 60 FPS. For the current HiLight RX, you need to set the camera frame rate to 60 FPS. We leave the 30 and 120 FPS interfaces open for your further development and extension.

2. Detect the transmitter screen by clicking the "screen detector" button. For a better performance, we suggest that you turn off the transmitter screen and make sure that the border of the Android device is not black. Make sure that the green box (the detection result) exactly matches the border of the transmitter screen.

3. Click the red button on the right to start capturing frames and decoding data.

4. Click the red button to stop capturing frames and decoding data.


## Notes and Tips

### Energy cost 
The energy consumption of the HiLight transmitter is low, since HiLight's encoding leverages alpha blending that runs instantaneously on GPU and entails low computation overhead. HiLight receiver consumes more energy because the camera keeps capturing frames at 60 FPS and the decoding runs on CPU. From our experiments, a fully-charged iPhone 5s can last roughly 3 hours when it continuously decodes data. 

### Phone temperature
We also observe that the phone's temperature rises if it is kept running for hours. As a result, the phone's CPU frequency will be locked down to a lower level because of the CPU protection mechanism on the iOS platform. The lower CPU frequency can cause decoding errors on the receiver. So we recommend that when the receiver phone feels hot, you give the phone a rest and cool it down before further tests.


### Screen detection
The current implementation of HiLight leverages Edge detection and Hough lines detection in the OpenCV library to detect the transmitter screen on the receiver side. For a better detection accuracy, we suggest the following:

1. turn off the transmitter screen during the detection;

2. have the transmitter's screen frame color in high contrast to the black color, e.g., our transmitter (Samsung Tab S) has a white frame on the front side;

3. run the screen detection in a good lighting condition -- good ambient light helps! It is just for the screen detection though, the actual transmission is robust in diverse ambient light conditions.

Once the transmitter screen is detected correctly, you can fix both the transmitter and the receiver on phone holders. Supporting constant movement is an interesting future work we plan to explore.

