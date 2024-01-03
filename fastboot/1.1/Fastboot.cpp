/*
 * Copyright (C) 2020 The Android Open Source Project
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
#define LOG_TAG "android.hardware.fastboot@1.1-impl-aml"

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

namespace android {
namespace hardware {
namespace fastboot {
namespace V1_1 {
namespace implementation {

// Methods from ::android::hardware::fastboot::V1_1::IFastboot follow.
static android::fs_mgr::Fstab fstab;

Return<void> Fastboot::getPartitionType(const hidl_string& partition_name,
                                        getPartitionType_cb _hidl_cb) {
    FstabEntry* mounted_entry;

    LOG(INFO) << "fastboot hal partition_name: " << partition_name;
    if (!android::fs_mgr::ReadDefaultFstab(&fstab)) {
        LOG(ERROR) << "failed to find fstab";
        _hidl_cb(FileSystemType::RAW, {Status::SUCCESS, ""});
        return Void();
    }

    if (partition_name == "userdata") {
        LOG(INFO) << "change partition_name to data";
        mounted_entry = GetEntryForMountPoint(&fstab, "/data");
    } else {
        std::string name = StringPrintf("/%s", partition_name.c_str());
        mounted_entry = GetEntryForMountPoint(&fstab, name);
    }

    if (mounted_entry == nullptr) {
        LOG(ERROR) << "failed to find " << partition_name << " partition";
        _hidl_cb(FileSystemType::RAW, {Status::SUCCESS, ""});
        return Void();
    }

    if (mounted_entry->fs_type == "f2fs") {
        _hidl_cb(FileSystemType::F2FS, {Status::SUCCESS, ""});
        return Void();
    } else if (mounted_entry->fs_type == "EXT4") {
        _hidl_cb(FileSystemType::RAW, {Status::SUCCESS, ""});
        return Void();
    } else {
        _hidl_cb(FileSystemType::RAW, {Status::SUCCESS, ""});
        return Void();
    }
}

Return<void> Fastboot::doOemCommand(const hidl_string& /* oemCmd */, doOemCommand_cb _hidl_cb) {
    _hidl_cb({Status::FAILURE_UNKNOWN, "Command not supported in default implementation"});
    return Void();
}

Return<void> Fastboot::getVariant(getVariant_cb _hidl_cb) {
    std::string device = android::base::GetProperty("ro.product.device", "");
    _hidl_cb(device, {Status::SUCCESS, ""});
    return Void();
}

Return<void> Fastboot::getOffModeChargeState(getOffModeChargeState_cb _hidl_cb) {
    _hidl_cb(false, {Status::SUCCESS, ""});
    return Void();
}

Return<void> Fastboot::getBatteryVoltageFlashingThreshold(
        getBatteryVoltageFlashingThreshold_cb _hidl_cb) {
    _hidl_cb(0, {Status::SUCCESS, ""});
    return Void();
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
    if (!android::fs_mgr::ReadDefaultFstab(&fstab)) {
        return VOL_FSTAB;
    }

    const fs_mgr::FstabEntry *v = android::fs_mgr::GetEntryForPath(&fstab, volume);
    if (v == nullptr) {
        return VOL_UNKNOWN;
    }

    if (android::fs_mgr::EnsurePathUnmounted(&fstab, volume) != true) {
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

Return<void> Fastboot::doOemSpecificErase(doOemSpecificErase_cb _hidl_cb) {
    // Erase metadata partition along with userdata partition.
    auto wipe_status = wipe_volume("/metadata");

    // Return exactly what happened
    if (wipe_status != WIPE_OK)
        _hidl_cb({Status::FAILURE_UNKNOWN, "Fail on wiping metadata and userdata"});
    else
        _hidl_cb({Status::SUCCESS, "wipe metadata/userdata ok"});

    return Void();
}

extern "C" IFastboot* HIDL_FETCH_IFastboot(const char* /* name */) {
    return new Fastboot();
}

}  // namespace implementation
}  // namespace V1_1
}  // namespace fastboot
}  // namespace hardware
}  // namespace android
