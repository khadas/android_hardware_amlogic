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

PLATFORM_SDK_GREATER_THAN_28 := $(shell expr $(PLATFORM_SDK_VERSION) \> 28)

# Set default Gralloc version for the platform, but allow this to be overriden.
ifeq ($(PLATFORM_SDK_GREATER_THAN_28), 1)
    GRALLOC_API_VERSION?=3.x
else
    GRALLOC_API_VERSION?=1.x
endif

ifdef GRALLOC_USE_GRALLOC1_API
    ifeq ($(GRALLOC_USE_GRALLOC1_API), 1)
        GRALLOC_API_VERSION := 1.x
    else
        ifeq ($(GRALLOC_USE_GRALLOC1_API), 0)
            GRALLOC_API_VERSION := 0.x
        endif
    endif
endif

# Fail build if GRALLOC_API_VERSION is not supported on the platform.
ifeq ($(PLATFORM_SDK_GREATER_THAN_28), 1)
    GRALLOC_VALID_VERSIONS := 3.x
else
    GRALLOC_VALID_VERSIONS := 1.x 2.x
endif
ifeq ($(filter $(GRALLOC_API_VERSION),$(GRALLOC_VALID_VERSIONS)),)
    $(error Gralloc version $(GRALLOC_API_VERSION) is not valid on the current platform. Valid versions are $(GRALLOC_VALID_VERSIONS))
endif

# Derive namespaces and associated (internal and scaled) versions for Gralloc 2.x
# Note: On a given SDK platform, versions of Allocator / Mapper / Common can be
#       changed (somewhat) independently of each other. Scaled internal versions, encapsulating
#       their major and minor versions, provide for building specific combinations
ifeq ($(GRALLOC_API_VERSION), 2.x)
    HIDL_IMAPPER_NAMESPACE := V2_1
    HIDL_IALLOCATOR_NAMESPACE := V2_0
    HIDL_COMMON_NAMESPACE := V1_1

    #Allocator = 2.0, Mapper = 2.1 and Common = 1.1
    HIDL_ALLOCATOR_VERSION_SCALED := 200
    HIDL_MAPPER_VERSION_SCALED := 210
    HIDL_COMMON_VERSION_SCALED := 110
else ifeq ($(GRALLOC_API_VERSION), 3.x)
    $(info Building Gralloc 3.x on platform SDK version $(PLATFORM_SDK_VERSION))

    HIDL_IMAPPER_NAMESPACE := V3_0
    HIDL_IALLOCATOR_NAMESPACE := V3_0
    HIDL_COMMON_NAMESPACE := V1_2

    #Allocator = 3.0, Mapper = 3.0 and Common = 1.2
    HIDL_ALLOCATOR_VERSION_SCALED := 300
    HIDL_MAPPER_VERSION_SCALED := 300
    HIDL_COMMON_VERSION_SCALED := 120
endif

GRALLOC_VERSION_MAJOR := $(shell echo $(GRALLOC_API_VERSION) | cut -d. -f1)
