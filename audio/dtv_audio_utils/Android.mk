LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)


LOCAL_C_INCLUDES +=                      \
    hardware/libhardware/include \
    $(TOPDIR)system/core/include        \
    $(LOCAL_PATH)/include       \
    $(TOPDIR)system/media/audio_utils/include \
    $(LOCAL_PATH)/utils \
    $(LOCAL_PATH)/dmxwrap \
    $(LOCAL_PATH)/dmxwrap/HwDemux \
    $(LOCAL_PATH)/dmxwrap/MultiHwDemux \
    $(LOCAL_PATH)/audio_read_api \
    $(LOCAL_PATH)/sync


LOCAL_SRC_FILES  +=               \
    utils/tsp_utils.c \
    utils/tsplinux.c \
    utils/Unicode.cpp \
    utils/TSPHandler.cpp \
    utils/TSPLooper.cpp \
    utils/TSPLooperRoster.cpp \
    utils/TSPMessage.cpp \
    utils/RefBase.cpp \
    utils/SharedBuffer.cpp \
    utils/String16.cpp \
    utils/String8.cpp \
    utils/StrongPointer.cpp \
    utils/Threads.cpp \
    utils/Timers.cpp \
    utils/VectorImpl.cpp \
    dmxwrap/HwDemux/AmHwDemuxWrapper.cpp \
    dmxwrap/MultiHwDemux/AmLinuxDvb.cpp \
    dmxwrap/MultiHwDemux/AmDmx.cpp \
    dmxwrap/MultiHwDemux/AmHwMultiDemuxWrapper.cpp \
    audio_read_api/dmx_audio_es.cpp \
    sync/audio_dtv_sync.c

LOCAL_MODULE := libdvbaudioutils

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
    LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_SHARED_LIBRARIES += \
    libc                  \
    libcutils             \
    libutils              \
    liblog                \
    libaudioutils

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS += -DBUILD_IN_ANDROID -Werror -Wno-deprecated-declarations -Wno-deprecated-register \
                -Wno-unused-parameter -Wall
include $(BUILD_SHARED_LIBRARY)
