/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_primary"

#include <media/audiohal/DeviceHalInterface.h>
#include <media/audiohal/DevicesFactoryHalInterface.h>
#include <media/audiohal/FactoryHalHidl.h>
#include "aml_android_hidl_utils.h"

namespace android {

class DevicesFactoryHalInterface;
class DeviceHalInterface;

sp<DevicesFactoryHalInterface> mDevicesFactoryHal;
sp<DeviceHalInterface> hwDevice;

static void getHwDevice() {
    if (mDevicesFactoryHal == nullptr) {
        mDevicesFactoryHal = DevicesFactoryHalInterface::create();
        if (mDevicesFactoryHal == nullptr) {
            ALOGI("get DevicesFactoryHal fail");
            return;
        } else
            ALOGI("get DevicesFactoryHal sucess");

        int rc = mDevicesFactoryHal->openDevice("primary", &hwDevice);
        if (rc == NO_ERROR) {
            ALOGI("get hwDevice success ");
        } else {
            ALOGI("get hwDevice fail");
        }

        rc = hwDevice->initCheck();
     }
}

status_t setParameters(const String8& keyValuePairs) {
    status_t err = NO_ERROR;

    if (hwDevice == NULL)
        getHwDevice();

    if (hwDevice) {
        err = hwDevice->setParameters(keyValuePairs);
        ALOGI("setParameters:%s, err=%d", keyValuePairs.string(), err);
    }

    return err;
}

String8  getParameters(const String8& keys) {
    status_t err = NO_ERROR;
    String8 mString = String8("");

    if (hwDevice == NULL)
        getHwDevice();

    if (hwDevice) {
        err = hwDevice->getParameters(keys, &mString);
        if (err != NO_ERROR) {
            ALOGI("getParameters err: err=%d", err);
        }

        ALOGI("getParameters:keys:%s, return value:%s", keys.string(), mString.string());
    }

    return mString;
}

}
