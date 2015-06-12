LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := ffmpeg_perfetcher
LOCAL_SRC_FILES := ffmpeg_perfetcher.c

LOCAL_LDLIBS := -llog -ljnigraphics -lz 
LOCAL_SHARED_LIBRARIES := libavformat libavcodec libswscale libavutil libwsresample libsdl

include $(BUILD_SHARED_LIBRARY)
$(call import-module,ffmpeg-2.4.2/android/arm)
