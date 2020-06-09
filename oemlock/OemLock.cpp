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

#include "OemLock.h"

#include <vector>

#include <android-base/logging.h>


namespace android {
namespace hardware {
namespace oemlock {
namespace V1_0 {
namespace implementation {

// Methods from ::android::hardware::oemlock::V1_0::IOemLock follow.
Return<void> OemLock::getName(getName_cb _hidl_cb) {
  _hidl_cb(OemLockStatus::OK, {"droidlogic v1.0"});
  return Void();
}

Return<OemLockSecureStatus> OemLock::setOemUnlockAllowedByCarrier(
        bool allowed, const hidl_vec<uint8_t>& signature __unused) {
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByCarrier: " << allowed;
    return OemLockSecureStatus::OK;
}

Return<void> OemLock::isOemUnlockAllowedByCarrier(isOemUnlockAllowedByCarrier_cb _hidl_cb) {
    LOG(INFO) << "Running OemLock::isOemUnlockAllowedByCarrier";
    _hidl_cb(OemLockStatus::OK, true);
    return Void();
}

Return<OemLockStatus> OemLock::setOemUnlockAllowedByDevice(bool allowed) {
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByDevice: " << allowed;

    setLockAbility(allowed);
    return OemLockStatus::OK;
}

Return<void> OemLock::isOemUnlockAllowedByDevice(isOemUnlockAllowedByDevice_cb _hidl_cb) {
    LOG(INFO) << "Running OemLock::isOemUnlockAllowedByDevice";
    _hidl_cb(OemLockStatus::OK, getLockAbility());
    return Void();
}

//if return true, means we can use "fastboot flashing unlock" to unlock the device
bool OemLock::getLockAbility() {
    int version_major;
    int version_minor;
    int unlock_ability;
    int lock_state;
    int lock_critical_state;
    int lock_bootloader;
    int lock_reserve;

    std::string value;
    mSysCtrl->getBootEnv("ubootenv.var.lock", value);
    sscanf(value.c_str(), "%1d%1d%1d%1d%1d%1d%1d%1d", &version_major, &version_minor,
        &unlock_ability, &lock_reserve, &lock_state, &lock_critical_state, &lock_bootloader, &lock_reserve);

    return (1 == unlock_ability)?true:false;
}

void OemLock::setLockAbility(bool allowed) {
    std::string value;
    mSysCtrl->getBootEnv("ubootenv.var.lock", value);
    value.replace(2, 1, allowed?"1":"0");
    mSysCtrl->setBootEnv("ubootenv.var.lock", value);
}

status_t OemLock::dump(int fd, const std::vector<std::string>& args) {
    Mutex::Autolock lock(mLock);

    dprintf(fd, "get bootloader unlock state: %d (1:can unlock, 0:can not unlock)\n",
        getLockAbility()?1:0);

    int len = args.size();
    for (int i = 0; i < len; i ++) {
        std::string lock("-l");
        std::string help("-h");
        if (args[i] == lock) {
            if ((i + 2 < len) && (args[i + 1] == std::string("set"))) {
                std::string allowStr(args[i+2]);
                int allow = atoi(allowStr.c_str());

                setLockAbility((1 == allow)?true:false);
                dprintf(fd, "set bootloader unlock state to: %d"
                    "(1:can unlock, 0:can not unlock)\n", allow);
                break;
            }
            else {
                dprintf(fd,
                    "dump bootloader oemlock format error!! should use:\n"
                    "lshal debug interface -l set value \n");
            }
        }
        else if (args[i] == help) {
            dprintf(fd,
                "oemlock hwbinder service use to control the bootloader lock and unlock \n"
                "if we set the unlock state to 1, means the device can be unlocked \n"
                "usage: \n"
                "lshal debug android.hardware.oemlock@1.0::IOemLock/default \n"
                "lshal debug android.hardware.oemlock@1.0::IOemLock/default -l set [0|1]\n"
                "-h: help \n");
        }
    }

    return NO_ERROR;
}

Return<void> OemLock::debug(const hidl_handle& handle, const hidl_vec<hidl_string>& options) {
    if (handle != nullptr && handle->numFds >= 1) {
        int fd = handle->data[0];

        std::vector<std::string> args;
        for (size_t i = 0; i < options.size(); i++) {
            args.push_back(options[i]);
        }
        dump(fd, args);
        fsync(fd);
    }
    return Void();
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace oemlock
}  // namespace hardware
}  // namespace android

