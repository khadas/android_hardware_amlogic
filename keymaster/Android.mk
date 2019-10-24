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

KEYMASTER_TA_BINARY := 8efb1e1c-37e5-4326-a5d68c33726c7d57

#####################################################
#	TA Library
#####################################################
include $(CLEAR_VARS)
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := $(KEYMASTER_TA_BINARY)
LOCAL_MODULE_SUFFIX := .ta
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/lib/teetz
ifeq ($(TARGET_ENABLE_TA_SIGN), true)
$(info $(shell mkdir -p $(shell pwd)/$(PRODUCT_OUT)/signed/keymaster))
$(info $(shell $(shell pwd)/$(BOARD_AML_VENDOR_PATH)/tdk/ta_export/scripts/sign_ta_auto.py \
		--in=$(shell pwd)/$(LOCAL_PATH)/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX) \
		--out=$(shell pwd)/$(PRODUCT_OUT)/signed/keymaster/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX) \
		--keydir=$(shell pwd)/$(BOARD_AML_TDK_KEY_PATH)))
LOCAL_SRC_FILES := ../../../$(PRODUCT_OUT)/signed/keymaster/$(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
else
LOCAL_SRC_FILES := $(LOCAL_MODULE)$(LOCAL_MODULE_SUFFIX)
endif
include $(BUILD_PREBUILT)


include $(CLEAR_VARS)
LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := 4.0/service.cpp \
				   4.0/AmlogicKeymaster4Device.cpp \
				   ipc/amlogic_keymaster_ipc.cpp \
				   AmlogicKeymaster.cpp

LOCAL_CFLAGS += -DAMLOGIC_MODIFY=1
LOCAL_C_INCLUDES := \
			$(LOCAL_PATH)/include \
			$(BOARD_AML_VENDOR_PATH)/tdk/ca_export_arm/include

LOCAL_SHARED_LIBRARIES := \
        		liblog \
				libcutils \
				libdl \
				libbase \
				libutils \
				libhardware \
				libhidlbase \
				libteec \
				libkeymaster_messages \
				libkeymaster4 \
				android.hardware.keymaster@4.0 

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_REQUIRED_MODULES := $(KEYMASTER_TA_BINARY)
LOCAL_MODULE := android.hardware.keymaster@4.0-service.amlogic
LOCAL_INIT_RC := 4.0/android.hardware.keymaster@4.0-service.amlogic.rc
include $(BUILD_EXECUTABLE)
