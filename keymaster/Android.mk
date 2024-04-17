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

TA_UUID := 8efb1e1c-37e5-4326-a5d6-8c33726c7d57
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
LOCAL_MODULE := android.hardware.hardware_keystore.amlogic.xml
LOCAL_SRC_FILES := android.hardware.hardware_keystore.amlogic.xml
LOCAL_MODULE_CLASS := ETC
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/etc/permissions
LOCAL_MODULE_TAGS := optional
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../LICENSE
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
LOCAL_MODULE := rkp_extract.sh
LOCAL_SRC_FILES := keymint/rkp_extract.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_NOTICE_FILE := $(LOCAL_PATH)/../LICENSE
include $(BUILD_PREBUILT)

include $(CLEAR_VARS)
ifeq ($(TARGET_OLD_DEVICE), true)
LOCAL_CFLAGS += -DNO_RKP
endif

LOCAL_MODULE := provision_devid_demo
LOCAL_SRC_FILES := provision_devid_demo.cpp \
                    keymint/AmlogicKeyMintDevice.cpp \
                    keymint/AmlogicKeyMintOperation.cpp

ifneq ($(TARGET_OLD_DEVICE), true)
LOCAL_SRC_FILES += keymint/AmlogicRemotelyProvisionedComponentDevice.cpp
endif

LOCAL_SRC_FILES += keymint/AmlogicSecureClock.cpp \
                    keymint/AmlogicSharedSecret.cpp \
                    ipc/amlogic_keymaster_ipc.cpp \
                    AmlogicKeymaster.cpp

LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice

TRUSTY_SRC_FILES := ../../../system/core/trusty/keymaster/TrustyKeymaster.cpp \
                    ../../../system/core/trusty/keymaster/ipc/trusty_keymaster_ipc.cpp
TRUSTY_SHARED_LIBRARIES := libtrusty
TRUSTY_INCLUDES = system/core/trusty/libtrusty/include \
                   system/core/trusty/keymaster/include \
                   system/core/libutils/include/ \
                   system/security/provisioner/

LOCAL_C_INCLUDES := \
                    $(LOCAL_PATH)/include \
                    $(PLATFORM_TDK_PATH)/ca_export_arm/include

LOCAL_SHARED_LIBRARIES := \
                    android.hardware.security.keymint-V3-ndk \
                    android.hardware.security.rkp-V3-ndk \
                    lib_android_keymaster_keymint_utils \
                    android.hardware.security.sharedsecret-V1-ndk \
                    android.hardware.security.secureclock-V1-ndk \
                    libbase \
                    libbinder_ndk \
                    libhardware \
                    libkeymaster_messages \
                    libkeymint \
                    liblog \
                    libtrusty \
                    libteec \
                    libtinyxml2 \
                    libcutils \
                    libjsoncpp \
                    libkeymint_remote_prov_support \
                    libcppbor_external \
                    libcppcose_rkp \
                    libcrypto

LOCAL_STATIC_LIBRARIES := librkp_factory_extraction

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_CFLAGS += -Wall \
                -Wextra \
                -Wno-unused-parameter

LOCAL_SRC_FILES += $(TRUSTY_SRC_FILES)
LOCAL_SHARED_LIBRARIES += $(TRUSTY_SHARED_LIBRARIES)
LOCAL_C_INCLUDES += $(TRUSTY_INCLUDES)
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
ifeq ($(TARGET_OLD_DEVICE), true)
LOCAL_CFLAGS += -DNO_RKP
endif

TRUSTY_SRC_FILES := ../../../system/core/trusty/keymaster/TrustyKeymaster.cpp \
                    ../../../system/core/trusty/keymaster/ipc/trusty_keymaster_ipc.cpp
TRUSTY_SHARED_LIBRARIES := libtrusty
TRUSTY_INCLUDES = system/core/trusty/libtrusty/include \
                   system/core/trusty/keymaster/include \
                   system/core/libutils/include/ \
                   system/security/provisioner/

LOCAL_MODULE_RELATIVE_PATH := hw
LOCAL_SRC_FILES := keymint/service.cpp \
                    keymint/AmlogicKeyMintDevice.cpp \
                    keymint/AmlogicKeyMintOperation.cpp

ifneq ($(TARGET_OLD_DEVICE), true)
LOCAL_SRC_FILES += keymint/AmlogicRemotelyProvisionedComponentDevice.cpp
endif

LOCAL_SRC_FILES += keymint/AmlogicSecureClock.cpp \
                    keymint/AmlogicSharedSecret.cpp \
                    ipc/amlogic_keymaster_ipc.cpp \
                    AmlogicKeymaster.cpp

LOCAL_C_INCLUDES := \
                    $(LOCAL_PATH)/include \
                    $(PLATFORM_TDK_PATH)/ca_export_arm/include

LOCAL_SHARED_LIBRARIES := \
                    android.hardware.security.keymint-V3-ndk \
                    android.hardware.security.rkp-V3-ndk \
                    lib_android_keymaster_keymint_utils \
                    android.hardware.security.sharedsecret-V1-ndk \
                    android.hardware.security.secureclock-V1-ndk \
                    libbase \
                    libbinder_ndk \
                    libhardware \
                    libkeymaster_messages \
                    libkeymint \
                    liblog \
                    libtrusty \
                    libteec \
                    libtinyxml2 \
                    libcutils \
                    libjsoncpp \
                    libkeymint_remote_prov_support \
                    libcppbor_external \
                    libcppcose_rkp \
                    libcrypto

ifneq ($(TARGET_OLD_DEVICE), true)
LOCAL_STATIC_LIBRARIES := librkp_factory_extraction
endif

ifeq ($(shell test $(PLATFORM_SDK_VERSION) -ge 26 && echo OK),OK)
LOCAL_PROPRIETARY_MODULE := true
endif

LOCAL_SRC_FILES += $(TRUSTY_SRC_FILES)
LOCAL_SHARED_LIBRARIES += $(TRUSTY_SHARED_LIBRARIES)
LOCAL_C_INCLUDES += $(TRUSTY_INCLUDES)

LOCAL_CFLAGS += -Wall \
                -Wextra

LOCAL_REQUIRED_MODULES := $(TA_UUID)
LOCAL_REQUIRED_MODULES += android.hardware.hardware_keystore.amlogic.xml
LOCAL_REQUIRED_MODULES += provision_devid_demo
ifneq ($(TARGET_OLD_DEVICE), true)
LOCAL_REQUIRED_MODULES += rkp_factory_extraction_tool
LOCAL_VINTF_FRAGMENTS := keymint/android.hardware.security.keymint-service.amlogic.xml
else
LOCAL_VINTF_FRAGMENTS := keymint/android.hardware.security.keymint-service-no-rkp.amlogic.xml
endif
LOCAL_MODULE := android.hardware.security.keymint-service.amlogic
LOCAL_LICENSE_KINDS := SPDX-license-identifier-Apache-2.0
LOCAL_LICENSE_CONDITIONS := notice
LOCAL_INIT_RC := keymint/android.hardware.security.keymint-service.amlogic.rc
include $(BUILD_EXECUTABLE)
