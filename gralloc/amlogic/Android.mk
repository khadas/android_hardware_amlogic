# Copyright (C) 2008 The Android Open Source Project
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

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := liblog libcutils android.hardware.graphics.common@1.0
LOCAL_C_INCLUDES := \
	system/core/libutils/include \
	hardware/libhardware/include \
	hardware/amlogic/gralloc
LOCAL_SRC_FILES := \
	amlogic/am_gralloc_ext.cpp
LOCAL_CFLAGS := -DLOG_TAG=\"gralloc_ext\"
LOCAL_MODULE := libamgralloc_ext
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := liblog libcutils android.hardware.graphics.common@1.0
LOCAL_C_INCLUDES := \
	system/core/libutils/include \
	hardware/libhardware/include \
	hardware/amlogic/gralloc
LOCAL_SRC_FILES := \
	amlogic/am_gralloc_ext.cpp
LOCAL_MODULE := libamgralloc_ext_static
include $(BUILD_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_PROPRIETARY_MODULE := true
LOCAL_SHARED_LIBRARIES := liblog libcutils
LOCAL_C_INCLUDES := \
	hardware/libhardware/include \
	system/core/libutils/include \
	system/core/libsystem/include \
	hardware/amlogic/gralloc
LOCAL_SRC_FILES := \
	amlogic/am_gralloc_internal.cpp
LOCAL_MODULE := libamgralloc_internal_static
include $(BUILD_STATIC_LIBRARY)

