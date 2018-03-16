LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := esplayer
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := esplayer.c
LOCAL_ARM_MODE := arm

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_SHARED_LIBRARIES := \
	libamcodec

ifneq (0, $(shell expr $(PLATFORM_VERSION) \>= 5.0))
LOCAL_SHARED_LIBRARIES += libsystemcontrolservice
else
LOCAL_SHARED_LIBRARIES += libsystemwriteservice
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_EXECUTABLE)
