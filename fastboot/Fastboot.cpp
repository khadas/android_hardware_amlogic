/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.fastboot-service.amlogic"

#include <log/log.h>
#include "Fastboot.h"
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <endian.h>
#include <errno.h>

#include <string>
#include <unordered_map>
#include <vector>
#include <map>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>

// FS headers
#include <ext4_utils/wipe.h>
#include <fs_mgr.h>
#include <fs_mgr/roots.h>

#include <fstab/fstab.h>

using android::fs_mgr::Fstab;
using android::fs_mgr::GetEntryForMountPoint;
using android::fs_mgr::ReadDefaultFstab;
using android::fs_mgr::FstabEntry;
using android::fs_mgr::ReadFstabFromFile;
using android::base::StringPrintf;

using ndk::ScopedAStatus;

namespace aidl {
namespace android {
namespace hardware {
namespace fastboot {

static ::android::fs_mgr::Fstab fstab;
ScopedAStatus Fastboot::getPartitionType(const std::string& in_partitionName,
                                         FileSystemType* _aidl_return) {
	FstabEntry* mounted_entry;
	LOG(INFO) << "fastboot hal in_partitionName: " << in_partitionName;

	if (in_partitionName.empty()) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                           "Invalid partition name");
    }

	if (!::android::fs_mgr::ReadDefaultFstab(&fstab)) {
		LOG(ERROR) << "failed to find fstab";
		*_aidl_return = FileSystemType::RAW;
		return ScopedAStatus::ok();
	}
	if (in_partitionName == "userdata") {
		LOG(INFO) << "change in_partitionName to data";
		mounted_entry = GetEntryForMountPoint(&fstab, "/data");
	} else {
        std::string name = StringPrintf("/%s", in_partitionName.c_str());
        mounted_entry = GetEntryForMountPoint(&fstab, name);
    }

	if (mounted_entry == nullptr) {
        LOG(ERROR) << "failed to find " << in_partitionName << " partition";
		*_aidl_return = FileSystemType::RAW;
		return ScopedAStatus::ok();
    }

    if (mounted_entry->fs_type == "f2fs") {
		*_aidl_return = FileSystemType::F2FS;
		return ScopedAStatus::ok();
    } else if (mounted_entry->fs_type == "EXT4") {
		*_aidl_return = FileSystemType::EXT4;
		return ScopedAStatus::ok();
    } else {
		*_aidl_return = FileSystemType::RAW;
		return ScopedAStatus::ok();
    }

}

ScopedAStatus Fastboot::doOemCommand(const std::string& in_oemCmd, std::string* _aidl_return) {
    *_aidl_return = "";
    if (in_oemCmd.empty()) {
        return ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT, "Invalid command");
    }
    return ScopedAStatus::fromExceptionCodeWithMessage(
            EX_UNSUPPORTED_OPERATION, "Command not supported in default implementation");
}

ScopedAStatus Fastboot::getVariant(std::string* _aidl_return) {
    std::string device = ::android::base::GetProperty("ro.product.device", "");
    *_aidl_return = device.c_str();
    return ScopedAStatus::ok();
}

ScopedAStatus Fastboot::getOffModeChargeState(bool* _aidl_return) {
    *_aidl_return = false;
    return ScopedAStatus::ok();
}

ScopedAStatus Fastboot::getBatteryVoltageFlashingThreshold(int32_t* _aidl_return) {
    *_aidl_return = 0;
    return ScopedAStatus::ok();
}

enum WipeVolumeStatus {
    WIPE_OK = 0,
    VOL_FSTAB,
    VOL_UNKNOWN,
    VOL_MOUNTED,
    VOL_BLK_DEV_OPEN,
    WIPE_ERROR_MAX = 0xffffffff,
};
std::map<enum WipeVolumeStatus, std::string> wipe_vol_ret_msg{
        {WIPE_OK, ""},
        {VOL_FSTAB, "Unknown FS table"},
        {VOL_UNKNOWN, "Unknown volume"},
        {VOL_MOUNTED, "Fail to unmount volume"},
        {VOL_BLK_DEV_OPEN, "Fail to open block device"},
        {WIPE_ERROR_MAX, "Unknown wipe error"}};

enum WipeVolumeStatus wipe_volume(const std::string &volume) {
    if (!::android::fs_mgr::ReadDefaultFstab(&fstab)) {
        return VOL_FSTAB;
    }

    const ::android::fs_mgr::FstabEntry *v = ::android::fs_mgr::GetEntryForPath(&fstab, volume);
    if (v == nullptr) {
        return VOL_UNKNOWN;
    }

    if (::android::fs_mgr::EnsurePathUnmounted(&fstab, volume) != true) {
        return VOL_MOUNTED;
    }

    int fd = open(v->blk_device.c_str(), O_WRONLY | O_CREAT, 0644);
    if (fd == -1) {
        return VOL_BLK_DEV_OPEN;
    }

    wipe_block_device(fd, get_block_device_size(fd));
    close(fd);

    return WIPE_OK;
}

ScopedAStatus Fastboot::doOemSpecificErase() {
	// Erase metadata partition along with userdata partition.
	auto wipe_status = wipe_volume("/metadata");
	// Return exactly what happened
	if (wipe_status != WIPE_OK)
		return ScopedAStatus::fromServiceSpecificErrorWithMessage(
            BnFastboot::FAILURE_UNKNOWN, "Fail on wiping metadata and userdata");
	else {
		LOG(INFO) << "wipe metadata/userdata ok! ";
		return ScopedAStatus::ok();
	}
}

}  // namespace fastboot
}  // namespace hardware
}  // namespace android
}  // namespace aidl
