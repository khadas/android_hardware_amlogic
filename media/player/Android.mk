LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := esplayer
LOCAL_LICENSE_KINDS := legacy_by_exception_only legacy_proprietary
LOCAL_LICENSE_CONDITIONS := by_exception_only proprietary
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../LICENSE
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
	esplayer.c \
	vcodec.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE    := DecInfo_test
LOCAL_LICENSE_KINDS := legacy_by_exception_only legacy_proprietary
LOCAL_LICENSE_CONDITIONS := by_exception_only proprietary
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../LICENSE
LOCAL_MODULE_TAGS := optional
LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := \
        DecInfo_test.c

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/../amcodec/include \
	$(JNI_H_INCLUDE)

LOCAL_LDLIBS := -llog
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)
