# Copyright (C) 2012 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

TA_UUID := 2088528d-102a-4716-b940-23fd9be04adf
TA_SUFFIX := .ta

#####################################################
#    TA Library
#####################################################
ifneq ($(PLATFORM_TDK_VERSION), 24)
	ifeq ($(BOARD_AML_SOC_TYPE),)
		LOCAL_TA := ta/v3/signed/$(TA_UUID)$(TA_SUFFIX)
	else
		LOCAL_TA := ta/v3/dev/$(BOARD_AML_SOC_TYPE)/$(TA_UUID)$(TA_SUFFIX)
	endif
else
	LOCAL_TA := ta/v2/signed/$(TA_UUID)$(TA_SUFFIX)
endif

ifneq ($(USE_PRESIGNED_TA),true)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(LOCAL_TA)
LOCAL_MODULE := $(TA_UUID)
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_MODULE_SUFFIX := $(TA_SUFFIX)
LOCAL_STRIP_MODULE := false
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/teetz
include $(BUILD_PREBUILT)
endif  # USE_PRESIGNED_TA != true


include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.gatekeeper-service.amlogic
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
TRUSTY_SRC_FILES := ../../../system/core/trusty/gatekeeper/trusty_gatekeeper_ipc.c
TRUSTY_SHARED_LIBRARIES := libtrusty
TRUSTY_INCLUDES = system/core/trusty/libtrusty/include \
                  system/core/trusty/gatekeeper \
                  system/core/libutils/include/
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := service.cpp \
                    amlogic_gatekeeper_ipc.c \
                    amlogic_gatekeeper.cpp

LOCAL_C_INCLUDES := \
                    $(LOCAL_PATH)/include \
                    $(PLATFORM_TDK_PATH)/ca_export_arm/include

LOCAL_SHARED_LIBRARIES := \
                    android.hardware.gatekeeper-V1-ndk \
                    libbase \
                    libbinder_ndk \
                    libgatekeeper \
                    libhardware \
                    libutils \
                    liblog \
                    libcutils \
                    libtrusty \
                    libteec

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_SRC_FILES += $(TRUSTY_SRC_FILES)
LOCAL_SHARED_LIBRARIES += $(TRUSTY_SHARED_LIBRARIES)
LOCAL_C_INCLUDES += $(TRUSTY_INCLUDES)

LOCAL_CFLAGS += -Wall \
                -Werror \
                -fvisibility=hidden

LOCAL_REQUIRED_MODULES := $(TA_UUID)
LOCAL_VINTF_FRAGMENTS := android.hardware.gatekeeper-service.amlogic.xml
LOCAL_INIT_RC := android.hardware.gatekeeper-service.amlogic.rc
include $(BUILD_EXECUTABLE)
