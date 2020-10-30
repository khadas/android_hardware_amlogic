LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_CFLAGS += -Werror -Wformat -Wimplicit-function-declaration
LOCAL_SRC_LISTS := \
	$(wildcard $(LOCAL_PATH)/*.c) \
	$(wildcard $(LOCAL_PATH)/*.cpp) \
	$(wildcard $(LOCAL_PATH)/mediaconfig/*.cpp) \
	$(wildcard $(LOCAL_PATH)/mediactl/*.cpp)

LOCAL_SRC_FILES := $(LOCAL_SRC_LISTS:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE) \
	$(TOP)/frameworks/native/services \
	$(TOP)/frameworks/native/include \
	$(TOP)/$(BOARD_AML_VENDOR_PATH)/frameworks/services \
	$(LOCAL_PATH)/../mediaconfig \
	$(TOP)/frameworks/native/libs/nativewindow/include \
	frameworks/native/opengl/include \
	system/libhidl/transport/token/1.0/utils/include

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libcutils \
	libc \
	libdl \
	libbinder \
	liblog \
	libui \
	libamavutils

#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE := libamavutils_alsa
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)
LOCAL_CFLAGS+=-DNO_USE_SYSWRITE

ifneq ($(NUM_FRAMEBUFFER_SURFACE_BUFFERS),)
  LOCAL_CFLAGS += -DFB_BUFFER_NUM=$(NUM_FRAMEBUFFER_SURFACE_BUFFERS)
endif

ifeq ($(TARGET_EXTERNAL_DISPLAY),true)
ifeq ($(TARGET_SINGLE_EXTERNAL_DISPLAY_USE_FB1),true)
LOCAL_CFLAGS += -DSINGLE_EXTERNAL_DISPLAY_USE_FB1
endif
endif

LOCAL_CFLAGS += -DANDROID_PLATFORM_SDK_VERSION=$(PLATFORM_SDK_VERSION)
LOCAL_CFLAGS += -Werror -Wformat -Wimplicit-function-declaration
LOCAL_SRC_LISTS := \
	$(wildcard $(LOCAL_PATH)/*.c) \
	$(wildcard $(LOCAL_PATH)/*.cpp) \
	$(wildcard $(LOCAL_PATH)/mediaconfig/*.cpp) \
	$(wildcard $(LOCAL_PATH)/mediactl/*.cpp)

LOCAL_SRC_FILES := $(LOCAL_SRC_LISTS:$(LOCAL_PATH)/%=%)

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	$(call include-path-for, graphics corecg)    \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE) \
	$(TOP)/frameworks/native/services \
	$(TOP)/frameworks/native/include \
	$(TOP)/$(BOARD_AML_VENDOR_PATH)/frameworks/services \
	$(LOCAL_PATH)/../mediaconfig \
	$(TOP)/frameworks/native/libs/nativewindow/include \
	frameworks/native/opengl/include \
	system/libhidl/transport/token/1.0/utils/include

LOCAL_SHARED_LIBRARIES := \
	libutils \
	libcutils \
	libc \
	libbinder \
	liblog \
	libui

#LOCAL_SHARED_LIBRARIES += libandroid_runtime   libnativehelper
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
LOCAL_MODULE := libamavutils_alsa
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
include $(BUILD_STATIC_LIBRARY)

