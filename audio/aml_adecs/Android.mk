LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    aml_dts_dec_api.c    \
    aml_ddp_dec_api.c    \
    aml_dec_api.c        \
    aml_aac_dec_api.c   \
    aml_mpeg_dec_api.c    \
    aml_pcm_dec_api.c

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libamaudioutils \
    libamlparser

LOCAL_C_INCLUDES := \
   external/tinyalsa/include \
   system/media/audio_utils/include \
   system/media/audio/include \
   $(LOCAL_PATH)/../utils/include \
   system/core/libion/include \
   system/core/include \
   hardware/libhardware/include \
   $(LOCAL_PATH)/include \
   $(LOCAL_PATH)/../aml_resampler/include \
   $(LOCAL_PATH)/../aml_speed/include \
   $(LOCAL_PATH)/../audio_hal \
   $(LOCAL_PATH)/../aml_parser/include

LOCAL_CFLAGS += -Werror -Wno-unused-label -Wno-unused-parameter
LOCAL_MODULE := libamladecs
LOCAL_MODULE_TAGS := optional
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
    LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)
