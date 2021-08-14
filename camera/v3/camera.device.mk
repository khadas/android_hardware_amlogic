# Copyright (C) 2020 Arm Limited.
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Configuration that should be included by BoardConfig.mk to configure necessary Soong namespaces.

ifeq ($(TARGET_BUILD_KERNEL_4_9), true)
BUILD_KERNEL_4_9 ?= true
endif

# Setup configuration in Soong namespace
SOONG_CONFIG_NAMESPACES += arm_camera
SOONG_CONFIG_arm_camera := \
    build_kernel_4_9_x

SOONG_CONFIG_arm_camera_build_kernel_4_9_x := $(BUILD_KERNEL_4_9)
