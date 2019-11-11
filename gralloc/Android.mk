#
# Copyright (C) 2016-2019 ARM Limited. All rights reserved.
#
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

TOP_LOCAL_PATH := $(call my-dir)
MALI_GRALLOC_API_TESTS?=0

# meson graphics start
GRALLOC_AML_EXTEND:=1

ifeq ($(GRALLOC_AML_EXTEND),1)
# amlogic config
BOARD_RESOLUTION_RATIO ?= 1080
ifneq ($(GPU_ARCH),utgard)
	GRALLOC_INIT_AFBC:=1
	DGPU_FORMAT_LIMIT:=1
endif

#arm gralloc config by amlogic
GRALLOC_USE_ION_DMA_HEAP:=1
GRALLOC_DISABLE_FRAMEBUFFER_HAL:=1
GRALLOC_HWC_FB_DISABLE_AFBC:=0

endif

ifdef GRALLOC_USE_GRALLOC1_API
    ifdef GRALLOC_API_VERSION
        $(warning GRALLOC_API_VERSION flag is not compatible with GRALLOC_USE_GRALLOC1_API)
    endif
endif

include $(TOP_LOCAL_PATH)/gralloc.version.mk

$(warning "GPU_ARCH = $(GPU_ARCH)")
$(warning "GRALLOC_INIT_AFBC = $(GRALLOC_INIT_AFBC)")
$(warning "Gralloc API = $(GRALLOC_API_VERSION)")
$(warning "BOARD_RESOLUTION_RATIO = $(BOARD_RESOLUTION_RATIO)")
$(warning "AFBC_BASIC =  $(MALI_GPU_SUPPORT_AFBC_BASIC)")
$(warning "AFBC_SPLIT = $(MALI_GPU_SUPPORT_AFBC_SPLITBLK)")
$(warning "AFBC_WIDBLK = $(MALI_GPU_SUPPORT_AFBC_WIDEBLK)")


# Amlogic usage & flags api.
#meson graphics start
include $(TOP_LOCAL_PATH)/amlogic/Android.mk
#meson graphics end

# Place and access VPU library from /vendor directory in unit testing as default
# /system is not in the linker permitted paths.
ifeq ($(MALI_GRALLOC_API_TESTS), 1)
MALI_GRALLOC_VPU_LIBRARY_PATH := "\"/vendor/lib/\""
endif

#Build allocator for version >= 2.x and gralloc libhardware HAL for all previous versions.
GRALLOC_MAPPER := 0
ifeq ($(shell expr $(GRALLOC_VERSION_MAJOR) \>= 2), 1)
    $(info Build Gralloc allocator for $(GRALLOC_API_VERSION))
else ifeq ($(shell expr $(GRALLOC_VERSION_MAJOR) \>= 1), 1)
    $(info Build Gralloc 1.x libhardware HAL)
else
    $(info Build Gralloc 0.x libhardware HAL)
endif
include $(TOP_LOCAL_PATH)/src/Android.mk

ifeq ($(shell expr $(GRALLOC_VERSION_MAJOR) \>= 2), 1)
   GRALLOC_MAPPER := 1
   $(info Build Gralloc mapper for $(GRALLOC_API_VERSION))
   include $(TOP_LOCAL_PATH)/src/Android.mk
endif

# Build service for 3.0
ifeq ($(GRALLOC_VERSION_MAJOR), 3)
   $(info Build 3.0 IAllocator service.)
   include $(TOP_LOCAL_PATH)/service/3.x/Android.mk
endif

# Build gralloc api tests.
ifeq ($(MALI_GRALLOC_API_TESTS), 1)
   $(info Build gralloc API tests.)
   include $(TOP_LOCAL_PATH)/api_tests/Android.mk
endif
