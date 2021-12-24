LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_SRC_FILES := mediactl.cpp
LOCAL_SHARED_LIBRARIES := liblog libcutils libmedia.amlogic.hal
LOCAL_MODULE := mediactl
LOCAL_LICENSE_KINDS := legacy_by_exception_only legacy_proprietary
LOCAL_LICENSE_CONDITIONS := by_exception_only proprietary
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../../LICENSE
LOCAL_MODULE_TAGS := optional

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_EXECUTABLE)
