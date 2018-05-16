LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_PREBUILT_LIBS := libgoogle_aec.so
include $(BUILD_MULTI_PREBUILT)
