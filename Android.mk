LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES := jshon.c
LOCAL_C_INCLUDES := external/jansson/src/ external/jansson/android/
LOCAL_MODULE := jshon

LOCAL_SHARED_LIBRARIES := libjansson

include $(BUILD_EXECUTABLE)
