LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := esplayer
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	esplayer.c \
	vcodec.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)
