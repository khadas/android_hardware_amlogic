LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

AMADEC_PATH:=$(TOP)/hardware/amlogic/LibAudio/amadec/

LOCAL_SRC_FILES := \
	codec/codec_ctrl.c \
	codec/codec_h_ctrl.c \
	codec/codec_msg.c \
	audio_ctl/audio_ctrl.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/codec \
	$(LOCAL_PATH)/../amavutils/include \
	$(AMADEC_PATH)/include \
	$(LOCAL_PATH)/audio_ctl

LOCAL_ARM_MODE := arm
LOCAL_STATIC_LIBRARIES := libamadec liblog
LOCAL_MODULE:= libamcodec

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	codec/codec_ctrl.c \
	codec/codec_h_ctrl.c \
	codec/codec_msg.c \
	audio_ctl/audio_ctrl.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/codec \
	$(LOCAL_PATH)/../amavutils/include \
	$(LOCAL_PATH)/audio_ctl \
	$(AMADEC_PATH)/include

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libmedia \
	libz \
	libbinder \
	libdl \
	libcutils \
	libc \
	liblog \
	libamavutils \
	libamadec

LOCAL_VENDOR_MODULE := true
LOCAL_ARM_MODE := arm
LOCAL_MODULE:= libamcodec
LOCAL_MODULE_TAGS := optional
LOCAL_PRELINK_MODULE := false

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)
