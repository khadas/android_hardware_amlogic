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
#include <android-base/strings.h>

namespace aidl {
namespace android {
namespace hardware {
namespace oemlock {

using ::std::string;
using ::android::base::EqualsIgnoreCase;


// Methods from ::android::hardware::oemlock::IOemLock follow.

::ndk::ScopedAStatus OemLock::getName(std::string *out_name) {
    *out_name = "droidlogic v1.0" ;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus OemLock::setOemUnlockAllowedByCarrier(bool in_allowed, const std::vector<uint8_t> &in_signature, OemLockSecureStatus *_aidl_return) {
    (void)in_allowed;
    (void)in_signature;
    (void)_aidl_return;
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByCarrier: " << in_allowed;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus OemLock::isOemUnlockAllowedByCarrier(bool *out_allowed) {
    (void)out_allowed;
    *out_allowed = true;
    LOG(INFO) << "Running OemLock::isOemUnlockAllowedByCarrier";
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus OemLock::setOemUnlockAllowedByDevice(bool in_allowed) {
    (void)in_allowed;
    LOG(INFO) << "Running OemLock::setOemUnlockAllowedByDevice: " << in_allowed;

    setLockAbility(in_allowed);
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus OemLock::isOemUnlockAllowedByDevice(bool *out_allowed) {
    (void)out_allowed;
    *out_allowed = getLockAbility();
    return ::ndk::ScopedAStatus::ok();
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
    sscanf(value.c_str(), "%1d%1d%1d%1d%1d%1d%1d%1d",
           &version_major,
           &version_minor,
           &unlock_ability,
           &lock_reserve,
           &lock_state,
           &lock_critical_state,
           &lock_bootloader,
           &lock_reserve);

    return (1 == unlock_ability)?true:false;
}

void OemLock::setLockAbility(bool allowed) {
    std::string value;
    mSysCtrl->getBootEnv("ubootenv.var.lock", value);
    value.replace(2, 1, allowed?"1":"0");
    mSysCtrl->setBootEnv("ubootenv.var.lock", value);
}

binder_status_t OemLock::dump(int fd, const char** args, uint32_t numArgs) {
    if (numArgs == 0) {
        return cmdHelp(fd);
    }
   string option = string(args[0]);
    if (EqualsIgnoreCase(option, "--help")) {
        return cmdHelp(fd);
    } else if (EqualsIgnoreCase(option, "--set")) {
        return cmdSet(fd, args, numArgs);
    } else {
        dprintf(fd, "Invalid option: %s\n", option.c_str());
        cmdHelp(fd);
        return STATUS_BAD_VALUE;
    }
}

binder_status_t OemLock::cmdHelp(int fd) const {
  dprintf(fd,
          "oemlock hwbinder service use to control the bootloader lock and unlock \n"
          "if we set the unlock state to 1, means the device can be unlocked \n"
          "usage: \n"
          "[no args]: dumpsys android.hardware.oemlock.IOemLock/default\n"
          "[--help]: shows this help\n"
          "[--set value]:dumpsys android.hardware.oemlock.IOemLock/default --set value,1:can unlock, 0:can not unlock\n");
  return STATUS_OK;
}

binder_status_t OemLock::cmdSet(int fd, const char** args, uint32_t numArgs) {
    ::android::Mutex::Autolock lock(mLock);
    dprintf(fd, "get bootloader unlock state: %d (1:can unlock, 0:can not unlock)\n",
        getLockAbility()?1:0);
        if (numArgs != 2) {
            dprintf(fd,
            "Invalid number of arguments: dumpsys android.hardware.oemlock.IOemLock/default --set value,1:can unlock, 0:can not unlock\n");
            return STATUS_BAD_VALUE;
        }
        std::string allowStr(args[1]);
        int allow = atoi(allowStr.c_str());
        if (allow == 0 || allow == 1) {
            dprintf(fd, "set bootloader unlock state to: %d"
                    "(1:can unlock, 0:can not unlock)\n", allow);
            setLockAbility((1 == allow)?true:false);

        }else{
            dprintf(fd,"dump bootloader oemlock format error!! should use:\n"
                    "dumpsys android.hardware.oemlock.IOemLock/default --set value,1:can unlock, 0:can not unlock\n");
            return STATUS_BAD_VALUE;
        }
        return STATUS_OK;
}


} // namespace oemlock
} // namespace hardware
} // namespace android
} // aidl
