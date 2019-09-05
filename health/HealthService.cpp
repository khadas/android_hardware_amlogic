/*
 * Copyright (C) 2019 The Android Open Source Project
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
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2019/09/04
 *  @par function description:
 *  - 1 amlogic health@2.0 hal
 */

#define LOG_TAG "health2"

#include <android-base/logging.h>

#include <healthd/healthd.h>
#include <health2/Health.h>
#include <health2/service.h>
#include <hidl/HidlTransportSupport.h>

#include <android-base/file.h>
#include <android-base/strings.h>

#include <vector>
#include <string>

#include <sys/stat.h>

#include "HealthService.h"

using android::hardware::health::V2_0::StorageInfo;
using android::hardware::health::V2_0::DiskStats;

static constexpr size_t kDiskStatsSize = 11;
static constexpr char kDiskName[] = "/proc/diskstats";
static constexpr char kDiskStatsFile[] = "/sys/block/sda/stat";

static constexpr char kStorageEolInfoFile[] = "/sys/class/mmc_host/emmc/emmc:0001/pre_eol_info";
static constexpr char kStorageLietimeFile[] = "/sys/class/mmc_host/emmc/emmc:0001/life_time";
static constexpr char kStorageRevFile[] = "/sys/class/mmc_host/emmc/emmc:0001/rev";
static constexpr char kStorageNameFile[] = "/sys/class/mmc_host/emmc/emmc:0001/name";

//https://source.android.com/devices/tech/health/
// See : hardware/interfaces/health/2.0/README

void healthd_board_init(struct healthd_config*)
{
    // Implementation-defined init logic goes here.
    // 1. config->periodic_chores_interval_* variables
    // 2. config->battery*Path variables
    // 3. config->energyCounter. In this implementation, energyCounter is not defined.

    // use defaults
}

int healthd_board_battery_update(struct android::BatteryProperties *props)
{
    props->chargerAcOnline = true;
    props->chargerUsbOnline = false;
    props->chargerWirelessOnline = false;
    props->maxChargingCurrent = 0;
    props->maxChargingVoltage = 0;
    props->batteryStatus = android::BATTERY_STATUS_UNKNOWN;
    props->batteryHealth = android::BATTERY_HEALTH_UNKNOWN;
    props->batteryPresent = false;
    props->batteryLevel = 50;
    props->batteryVoltage = 4;
    props->batteryTemperature = 40;
    props->batteryCurrent = 0;
    props->batteryCycleCount = 0;
    props->batteryFullCharge = 0;
    props->batteryChargeCounter = 10;
    props->batteryTechnology = "Li-ion";

    return 0;
}

void get_storage_info(std::vector<StorageInfo>& vec_storage_info) {
    StorageInfo storage_info = {};
    std::string lifeTime, version, name, preEol;

    //read version
    if (!android::base::ReadFileToString(std::string(kStorageRevFile), &version)) {
        LOG(ERROR) << kStorageRevFile << ": ReadFileToString failed.";
        return;
    }
    LOG(INFO) << kStorageRevFile << ":" << version;
    storage_info.version = version;

    //read name
    if (!android::base::ReadFileToString(std::string(kStorageNameFile), &name)) {
        LOG(ERROR) << kStorageNameFile << ": ReadFileToString failed.";
        return;
    }
    LOG(INFO) << kStorageNameFile << ":" << name;

    storage_info.attr.name = name;
    storage_info.attr.isInternal = true;
    storage_info.attr.isBootDevice = true;

    //read life time, normally the value is "0x01 0x01"
    if (!android::base::ReadFileToString(std::string(kStorageLietimeFile), &lifeTime)) {
        LOG(ERROR) << kStorageLietimeFile << ": ReadFileToString failed.";
        return;
    }
    LOG(INFO) << kStorageLietimeFile << ":" << lifeTime;

    std::vector<std::string> lines = android::base::Split(lifeTime, " ");
    if (lines.size() < 2) {
        return;
    }

    sscanf(lines[0].c_str(), "%hx", &storage_info.lifetimeA);
    sscanf(lines[1].c_str(), "%hx", &storage_info.lifetimeB);

    //read pre eol info
    if (!android::base::ReadFileToString(std::string(kStorageEolInfoFile), &preEol)) {
        LOG(ERROR) << kStorageEolInfoFile << ": ReadFileToString failed.";
        return;
    }

    LOG(INFO) << kStorageEolInfoFile << ":" << preEol;
    sscanf(preEol.c_str(), "%hx", &storage_info.eol);

    vec_storage_info.resize(1);
    vec_storage_info[0] = storage_info;
}

void get_disk_stats(std::vector<DiskStats>& vec_stats) {
    DiskStats stats = {};

    stats.attr.isInternal = true;
    stats.attr.isBootDevice = true;
    stats.attr.name = std::string(kDiskName);

    std::string buffer;
    if (!android::base::ReadFileToString(std::string(kDiskStatsFile), &buffer)) {
        LOG(ERROR) << kDiskStatsFile << ": ReadFileToString failed.";
        return;
    }

    // Regular diskstats entries
    std::stringstream ss(buffer);
    for (uint i = 0; i < kDiskStatsSize; i++) {
        ss >> *(reinterpret_cast<uint64_t*>(&stats) + i);
    }
    vec_stats.resize(1);
    vec_stats[0] = stats;
}

int main(void) {
    return health_service_main();
}
