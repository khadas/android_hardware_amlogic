LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    ../../../../frameworks/av/media/libaudioprocessing/AudioResampler.cpp \
    ../../../../frameworks/av/media/libaudioprocessing/AudioResamplerCubic.cpp \
    ../../../../frameworks/av/media/libaudioprocessing/AudioResamplerSinc.cpp \
    ../../../../frameworks/av/media/libaudioprocessing/AudioResamplerDyn.cpp \
    aml_audio_resample_manager.c \
    aml_resample_wrap.cpp \
    audio_android_resample_api.c \
    aml_audio_resampler.c \
    audio_simple_resample_api.c

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libaudioutils \
    libamaudioutils

LOCAL_C_INCLUDES := \
   $(LOCAL_PATH)/include \
   $(LOCAL_PATH)/../audio_hal \
   $(LOCAL_PATH)/../utils/tinyalsa/include \
   $(TOPDIR)system/media/audio_utils/include \
   $(TOPDIR)system/media/audio/include \
   $(TOPDIR)system/core/libion/include \
   $(TOPDIR)system/core/include \
   $(TOPDIR)frameworks/av/media/libaudioclient/include \
   $(TOPDIR)frameworks/av/media/libaudioprocessing/include \
   $(TOPDIR)hardware/libhardware/include


LOCAL_CFLAGS += -Werror -Wno-unused-label -Wno-unused-parameter
LOCAL_MODULE := libamlresampler
LOCAL_MODULE_TAGS := optional
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
    LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)
