LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    aml_audio_ac3parser.c \
    aml_audio_ac4parser.c \
    aml_audio_bitsparser.c \
    aml_audio_matparser.c \
    ac3_parser_utils.c \
    aml_ac3_parser.c

LOCAL_SHARED_LIBRARIES := \
    libcutils \
    liblog \
    libutils \
    libaudioutils \
    libtinyalsa \
    libamaudioutils

LOCAL_C_INCLUDES := \
   $(LOCAL_PATH)/include \
   $(TOPDIR)system/media/audio_utils/include \
   $(TOPDIR)system/media/audio/include \
   $(TOPDIR)system/core/libion/include \
   $(TOPDIR)system/core/include \
   $(TOPDIR)frameworks/av/media/libaudioclient/include \
   $(TOPDIR)frameworks/av/media/libaudioprocessing/include \
   $(TOPDIR)hardware/libhardware/include \
   $(TOPDIR)external/tinyalsa/include \
   $(LOCAL_PATH)/../utils/include


LOCAL_CFLAGS += -Werror -Wno-unused-label -Wno-unused-parameter
LOCAL_MODULE := libamlparser
LOCAL_MODULE_TAGS := optional
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
    LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)
