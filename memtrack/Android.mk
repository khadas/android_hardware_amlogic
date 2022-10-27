#Copyright (C) 2014 The Android Open Source Project
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

# HAL module implemenation stored in
# hw/<POWERS_HARDWARE_MODULE_ID>.default.so
include $(CLEAR_VARS)

ifeq ($(TARGET_BUILD_KERNEL_4_9), true)
BUILD_KERNEL_4_9 := true
else
BUILD_KERNEL_4_9 := false
endif

LOCAL_CFLAGS += -DBUILD_KERNEL_4_9=$(BUILD_KERNEL_4_9)
$(warning "the value of BUILD_KERNEL_4_9 is $(BUILD_KERNEL_4_9)")

PLATFORM_SDK_GREATER_THAN_29 := $(shell expr $(PLATFORM_SDK_VERSION) \> 29)
ifeq ($(PLATFORM_SDK_GREATER_THAN_29), 1)
SKIP_COUNT_ION := true
else
SKIP_COUNT_ION := false
endif

LOCAL_CFLAGS += -DSKIP_COUNT_ION=$(SKIP_COUNT_ION)
$(warning "the valaue of SKIP_COUNT_ION is $(SKIP_COUNT_ION)")

LOCAL_CFLAGS += -Wno-unused-parameter
LOCAL_CFLAGS += -Wno-unused-variable
LOCAL_CFLAGS += -Wno-format

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_C_INCLUDES += \
	hardware/libhardware/include \
	system/core/libcutils/include \
	system/core/libsystem/include

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_SHARED_LIBRARIES += libcutils
LOCAL_SRC_FILES := memtrack_aml.c
#LOCAL_MODULE := memtrack.$(TARGET_BOARD_PLATFORM)
LOCAL_MODULE := memtrack.amlogic
ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif
include $(BUILD_SHARED_LIBRARY)
