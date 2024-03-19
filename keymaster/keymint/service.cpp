/*
 * Copyright 2021, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "android.hardware.security.keymint-service.amlogic"
#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

#include <amlogic_keymaster/AmlogicKeyMintDevice.h>
#ifndef NO_RKP
#include <amlogic_keymaster/AmlogicRemotelyProvisionedComponentDevice.h>
#endif
#include <amlogic_keymaster/AmlogicSecureClock.h>
#include <amlogic_keymaster/AmlogicSharedSecret.h>

using aidl::android::hardware::security::keymint::AmlogicKeyMintDevice;
#ifndef NO_RKP
using aidl::android::hardware::security::keymint::AmlogicRemotelyProvisionedComponentDevice;
#endif
using aidl::android::hardware::security::secureclock::AmlogicSecureClock;
using aidl::android::hardware::security::sharedsecret::AmlogicSharedSecret;

#ifndef KEYMASTER_TEMP_FAILURE_RETRY
#define KEYMASTER_TEMP_FAILURE_RETRY(exp, retry)            \
      ({                                       \
           __typeof__(exp) _rc;                   \
           int count = 0;                         \
           do {                                   \
             usleep(10000);                        \
             _rc = (exp);                         \
             count ++;                            \
           } while (_rc != 0 && count < retry); \
           _rc;                                   \
         })
#endif

template <typename T, class... Args>
std::shared_ptr<T> addService(Args&&... args) {
    std::shared_ptr<T> service = ndk::SharedRefBase::make<T>(std::forward<Args>(args)...);
    auto instanceName = std::string(T::descriptor) + "/default";
    LOG(ERROR) << "Adding service instance: " << instanceName;
    auto status = AServiceManager_addService(service->asBinder().get(), instanceName.c_str());
    CHECK(status == STATUS_OK) << "Failed to add service " << instanceName;
    return service;
}

static auto amlKeymaster = std::make_shared<keymaster::AmlogicKeymaster>();

int init_service_later() {
    int err = KEYMASTER_TEMP_FAILURE_RETRY(amlKeymaster->Initialize(keymaster::KmVersion::KEYMINT_3), 10000);
    if (err != 0) {
        LOG(FATAL) << "Could not initialize AmlogicKeymaster for KeyMint (" << err << ")";
        return -1;
    }
    return 0;
}

int main() {
    // Zero threads seems like a useless pool but below we'll join this thread to it, increasing
    // the pool size to 1.
    ABinderProcess_setThreadPoolMaxThreadCount(0);

    auto keyMint = addService<AmlogicKeyMintDevice>(amlKeymaster);
    auto secureClock = addService<AmlogicSecureClock>(amlKeymaster);
    auto sharedSecret = addService<AmlogicSharedSecret>(amlKeymaster);
#ifndef NO_RKP
    auto remotelyProvisionedComponent =
            addService<AmlogicRemotelyProvisionedComponentDevice>(amlKeymaster);
#endif
    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach

}

