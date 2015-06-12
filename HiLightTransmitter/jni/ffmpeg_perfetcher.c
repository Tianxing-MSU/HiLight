//
//  Created by Tianxing Li 
//  Copyright 2015 The Trustees Dartmouth College, Permission to use for Non-commercial Research Purposes Only. All Other Rights Reserved.
//


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>

#include <stdio.h>
#include <wchar.h>

#include <jni.h>

/*for android logs*/
#include <android/log.h>

#define LOG_TAG "ffmpeg_perfetcher"
#define LOGI(...) __android_log_print(4, LOG_TAG, __VA_ARGS__);
#define LOGE(...) __android_log_print(6, LOG_TAG, __VA_ARGS__);

int finish = 0;

void SaveFrame(JNIEnv *pEnv, jobject pObj, jobject pBitmap, int width, int height, int iFrame, int64_t pts) {
	char szFilename[200];
	jmethodID sSaveFrameMID;
	jclass mainActCls;
	sprintf(szFilename, "/sdcard/HiLight/frame%d.jpg", iFrame);
	mainActCls = (*pEnv)->GetObjectClass(pEnv, pObj);
	sSaveFrameMID = (*pEnv)->GetMethodID(pEnv, mainActCls, "video_perfetch_thread", "(Landroid/graphics/Bitmap;Ljava/lang/String;I)V");
	LOGI("call java method to save frame %d", iFrame);
	jstring filePath = (*pEnv)->NewStringUTF(pEnv, szFilename);
	int timestamp = (int)pts;
	(*pEnv)->CallVoidMethod(pEnv, pObj, sSaveFrameMID, pBitmap, filePath, (jint)timestamp);
	LOGI("call java method to save frame %d done", iFrame);
}

jobject createBitmap(JNIEnv *pEnv, int pWidth, int pHeight) {
	int i;
	//get Bitmap class and createBitmap method ID
	jclass javaBitmapClass = (jclass)(*pEnv)->FindClass(pEnv, "android/graphics/Bitmap");
	jmethodID mid = (*pEnv)->GetStaticMethodID(pEnv, javaBitmapClass, "createBitmap", "(IILandroid/graphics/Bitmap$Config;)Landroid/graphics/Bitmap;");
	//create Bitmap.Config
	const wchar_t* configName = L"ARGB_8888";
	int len = wcslen(configName);
	jstring jConfigName;
	if (sizeof(wchar_t) != sizeof(jchar)) {
		//wchar_t is defined as different length than jchar(2 bytes)
		jchar* str = (jchar*)malloc((len+1)*sizeof(jchar));
		for (i = 0; i < len; ++i) {
			str[i] = (jchar)configName[i];
		}
		str[len] = 0;
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)str, len);
	} else {
		//wchar_t is defined same length as jchar(2 bytes)
		jConfigName = (*pEnv)->NewString(pEnv, (const jchar*)configName, len);
	}
	jclass bitmapConfigClass = (*pEnv)->FindClass(pEnv, "android/graphics/Bitmap$Config");
	jobject javaBitmapConfig = (*pEnv)->CallStaticObjectMethod(pEnv, bitmapConfigClass,
			(*pEnv)->GetStaticMethodID(pEnv, bitmapConfigClass, "valueOf", "(Ljava/lang/String;)Landroid/graphics/Bitmap$Config;"), jConfigName);
	//create the bitmap
	return (*pEnv)->CallStaticObjectMethod(pEnv, javaBitmapClass, mid, pWidth, pHeight, javaBitmapConfig);
}

jint perfetch_start(JNIEnv *pEnv, jobject pObj, jobject pMainAct, jstring pFileName, jint video_fps) {
	AVFormatContext *pFormatCtx = NULL;
	int             i, videoStream;
	AVCodecContext  *pCodecCtx = NULL;
	AVCodec         *pCodec = NULL;
	AVFrame         *pFrame = NULL;
	AVFrame         *pFrameRGBA = NULL;
	AVPacket        packet;
	int             frameFinished;
	jobject			bitmap;
	void* 			buffer;

	AVDictionary    *optionsDict = NULL;
	struct SwsContext      *sws_ctx = NULL;
	char *videoFileName;

	// Register all formats and codecs
	av_register_all();

	//get C string from JNI jstring
	videoFileName = (char *)(*pEnv)->GetStringUTFChars(pEnv, pFileName, NULL);

	// Open video file
	if(avformat_open_input(&pFormatCtx, videoFileName, NULL, NULL)!=0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0)
		return -1; // Couldn't find stream information

	// Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, videoFileName, 0);

	// Find the first video stream
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++) {
		if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	}
	if(videoStream==-1)
		return -1; // Didn't find a video stream

	// Get a pointer to the codec context for the video stream
	pCodecCtx=pFormatCtx->streams[videoStream]->codec;

	// Find the decoder for the video stream
	pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
	if(pCodec==NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1; // Codec not found
	}
	// Open codec
	if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
		return -1; // Could not open codec

	// Allocate video frame
	pFrame=avcodec_alloc_frame();

	// Allocate an AVFrame structure
	pFrameRGBA=avcodec_alloc_frame();
	if(pFrameRGBA==NULL)
		return -1;

	//create a bitmap as the buffer for pFrameRGBA
	bitmap = createBitmap(pEnv, pCodecCtx->width, pCodecCtx->height);
	if (AndroidBitmap_lockPixels(pEnv, bitmap, &buffer) < 0)
		return -1;
	//get the scaling context
	sws_ctx = sws_getContext
    (
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

	// Assign appropriate parts of bitmap to image planes in pFrameRGBA
	// Note that pFrameRGBA is an AVFrame, but AVFrame is a superset
	// of AVPicture
	avpicture_fill((AVPicture *)pFrameRGBA, buffer, AV_PIX_FMT_RGBA,
		 pCodecCtx->width, pCodecCtx->height);

	// Read frames and save first five frames to disk
	i=0;
	int ret;
	int fps = 0;
	int previous_pts = 0;
	int current_pts = 0;
	int prefetch_frame_index = 100;
	finish = 0;
	while(finish == 0) {
//		LOGI("av_read_frame start");
		ret = av_read_frame(pFormatCtx, &packet);
//		LOGI("av_read_frame end");
		if (ret <0){
			av_free_packet(&packet);
			break;
		}
		// Is this a packet from the video stream?
		if(packet.stream_index==videoStream) {

			// Decode video frame
			LOGI("avcodec_decode_video2 start");
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,
					&packet);
			LOGI("avcodec_decode_video2 end");
			// Did we get a video frame?
			if(frameFinished) {
				if (fps == 0){
					if (i == 0){
						previous_pts = av_frame_get_best_effort_timestamp ( pFrame );
					}else if (i == 8){
						current_pts = av_frame_get_best_effort_timestamp ( pFrame );
						//fps = 800000/(current_pts - previous_pts);
						fps = video_fps;
						LOGI("video fps %d", fps);
						prefetch_frame_index = fps*16/60;
						LOGI("prefetch_frame_index %d", prefetch_frame_index);
					}
				}


				if (i++%prefetch_frame_index == 0 && i < 1500){
					// Convert the image from its native format to RGBA
					sws_scale
					(
							sws_ctx,
							(uint8_t const * const *)pFrame->data,
							pFrame->linesize,
							0,
							pCodecCtx->height,
							pFrameRGBA->data,
							pFrameRGBA->linesize
					);

					// return the frame to java layer
					int64_t pts = av_frame_get_best_effort_timestamp ( pFrame );
					pts = av_rescale_q ( pts,  pFormatCtx->streams[videoStream]->time_base, AV_TIME_BASE_Q );
					LOGI("save frame %d, pts: %d", i, (int)pts);
					SaveFrame(pEnv, pMainAct, bitmap, pCodecCtx->width, pCodecCtx->height, i, pts);
					int got_packet_ptr;
					AVCodecContext *c= NULL;
					c = avcodec_alloc_context3(avcodec_find_encoder(pCodecCtx->codec_id));
					if (!c) {
							LOGI("Could not allocate video codec context\n");
					        return 0;
					}
					av_free_packet(&packet);
				}
			}
		}
		// Free the packet that was allocated by av_read_frame
		av_free_packet(&packet);
	}
	LOGI("final frame %d", i);
	//unlock the bitmap
	AndroidBitmap_unlockPixels(pEnv, bitmap);

	// Free the RGB image
	av_free(pFrameRGBA);

	// Free the YUV frame
	av_free(pFrame);

	// Close the codec
	avcodec_close(pCodecCtx);

	// Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;
}

jint perfetch_finish(){
	finish = 1;
	return 0;
}

jint JNI_OnLoad(JavaVM* pVm, void* reserved) {
	JNIEnv* env;
	if ((*pVm)->GetEnv(pVm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		 return -1;
	}
	JNINativeMethod nm[1];
	nm[0].name = "perfetch_start";
	nm[0].signature = "(Ledu/dartnets/hilight/HiLightTransmitter;Ljava/lang/String;I)I";
	nm[0].fnPtr = (void*)perfetch_start;
	jclass cls = (*env)->FindClass(env, "edu/dartnets/hilight/HiLightTransmitter");
	//Register methods with env->RegisterNatives.
	(*env)->RegisterNatives(env, cls, nm, 1);
	JNINativeMethod nm1[1];
	nm1[0].name = "perfetch_finish";
	nm1[0].signature = "()I";
	nm1[0].fnPtr = (void*)perfetch_finish;
	(*env)->RegisterNatives(env, cls, nm1, 1);
	return JNI_VERSION_1_6;
}
