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

#define LOG_TAG "EArc"

#include <stdlib.h>
#include <android-base/logging.h>
#include <fcntl.h>
#include <utils/Log.h>
#include <android-base/properties.h>

#include "EArc.h"

using ndk::ScopedAStatus;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace earc {
namespace implementation {

void EArc::serviceDied(void* cookie) {
    ALOGE("EArc died");
    EArc* eArc = static_cast<EArc*>(cookie);
    if (eArc == nullptr) {
        return;
    }
    eArc->mEArcEnabled = false;
}

ScopedAStatus EArc::setEArcEnabled(bool in_enabled) {
    if (in_enabled == mEArcEnabled) {
        ALOGI("%s in_enabled:%d but it's unchanged", __FUNCTION__, in_enabled);
        return ScopedAStatus::ok();
    }
    mEArcEnabled = in_enabled;
    int state = aml_mixer_ctrl_set_int(&mAlsaMixer, AML_MIXER_ID_EARC_TX_EARC_MODE, mEArcEnabled);
    if (state != 0) {
        ALOGW("%s failed with state:%d", __FUNCTION__, state);
        // As the caller may does not catch ServiceSpecificException, it may cause a crash of
        // system_server which is absolutely unnecessary.
        //return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
        return ScopedAStatus::ok();
    }

    return ScopedAStatus::ok();
}

ScopedAStatus EArc::isEArcEnabled(bool* _aidl_return) {
    *_aidl_return = mEArcEnabled;
    return ScopedAStatus::ok();
}

ScopedAStatus EArc::getState(int32_t portId, IEArcStatus* _aidl_return) {
    // Maintain port connection status and update on hotplug event
    if (portId == mEArcPort) {
        *_aidl_return = mPortStatus;
    } else {
        ALOGW("%s with invalid port:%d", __FUNCTION__, portId);
        //return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        return ScopedAStatus::ok();
    }

    return ScopedAStatus::ok();
}

ScopedAStatus EArc::getLastReportedAudioCapabilities(int32_t portId,
                                                         std::vector<uint8_t>* _aidl_return) {
    if (portId == mEArcPort) {
        *_aidl_return = mCapabilities;
    } else {
        ALOGW("%s with invalid port:%d", __FUNCTION__, portId);
        return ScopedAStatus::ok();
    }

    return ScopedAStatus::ok();
}

ScopedAStatus EArc::setCallback(const std::shared_ptr<IEArcCallback>& callback) {
    if (mCallback != nullptr) {
        mCallback = nullptr;
    }

    if (callback != nullptr) {
        mCallback = callback;
        AIBinder_linkToDeath(mCallback->asBinder().get(), mDeathRecipient.get(), this /* cookie */);
    }
    int attend_type = aml_mixer_ctrl_get_int(&mAlsaMixer, AML_MIXER_ID_EARC_TX_ATTENDED_TYPE);
    ALOGD("%s with attend type:%d", __FUNCTION__, attend_type);
    // report the status after callback is set
    changeState(toEArcStatus(attend_type), mEArcPort);

    return ScopedAStatus::ok();
}

ScopedAStatus EArc::reportCapabilities(const std::vector<uint8_t>& capabilities,
                                           int32_t portId) {
    if (mCallback != nullptr) {
        mCallback->onCapabilitiesReported(capabilities, portId);
        return ScopedAStatus::ok();
    }
    ALOGW("%s no callback is set yet!", __FUNCTION__);
    return ScopedAStatus::ok();
}

ScopedAStatus EArc::changeState(const IEArcStatus status, int32_t portId) {
    ALOGD("%s status:%hhd, portId:%d", __FUNCTION__, status, portId);
    if (portId != mEArcPort) {
        ALOGW("%s with invalid port:%d", __FUNCTION__, portId);
        //return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
        return ScopedAStatus::ok();

    }
    mPortStatus = status;

    if (mCallback != nullptr) {
        mCallback->onStateChange(status, portId);
        return ScopedAStatus::ok();
    }
    ALOGW("%s no callback set yet!", __FUNCTION__);
    return ScopedAStatus::ok();
}

EArc::EArc() {
    ALOGI("Opening eARC HAL.");
    mCallback = nullptr;
    mPortStatus = IEArcStatus::IDLE;
    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient_new(serviceDied));
    // Get alsa mixer
    open_mixer_handle(&mAlsaMixer);

    mEArcSupported = android::base::GetProperty(PROPERTY_EARC_SUPPORTED, "false") == "true";
    mEArcPort =  getPropertyInt(PROPERTY_EARC_PORT, EARC_PORT_DEFAULT, EARC_PORT_STR_DEFAULT);

    ALOGI("EArc is supported?:%d arc port:%d", mEArcSupported, mEArcPort);

    if (!mEArcSupported) {
        return;
    }

    mEArcStateListener = std::make_shared<EArcStateListener>(this);
    mEArcObserver = sp<EArcObserver>::make(mEArcStateListener);
    // Start the earc state observer thread
    mEArcObserver->run("EArcObserver");
}

EArc::~EArc() {
    ALOGI("Releasing eARC HAL.");
    close_mixer_handle(&mAlsaMixer);
}

IEArcStatus EArc::toEArcStatus(int state) {
    IEArcStatus eArcStatus = IEArcStatus::IDLE;
    switch (state) {
        case EARC_STATE_UNKNOWN:
            eArcStatus = IEArcStatus::IDLE;
            break;
        case EARC_STATE_ARC:
            eArcStatus = IEArcStatus::ARC_PENDING;
            break;
        case EARC_STATE_EARC:
            eArcStatus = IEArcStatus::EARC_CONNECTED;
            break;
        default:
            break;
    }
    return eArcStatus;
}

void EArc::handleEarcState(IEArcStatus eArcStatus) {
    // notify the changed earc state to listeners
    changeState(eArcStatus, mEArcPort);

    // If it's earc connected, then notify the earc sads.
    if (eArcStatus == IEArcStatus::EARC_CONNECTED) {
        // get the earc sads
        char cds[CDS_MAX] = {0};
        earctx_fetch_cds(&mAlsaMixer, cds, 0, &mArcHdmiDesc);
        ALOGD("earc sads:%s", cds);
        mCapabilities.assign(cds, cds + strlen(cds));
        reportCapabilities(mCapabilities, mEArcPort);
    }
}

int EArc::getPropertyInt(const char* key, int def, const char* defValue) {
    std::string str = android::base::GetProperty(key, defValue);

    char* endptr;
    long intValue = std::strtol(str.c_str(), &endptr, 10);
    ALOGD("%s %s %ld", __FUNCTION__, str.c_str(), intValue);

    if (*endptr == '\0') {
        return (int)intValue;
    } else {
        ALOGE("Conversion failed. Non-numeric characters found: %s\n", endptr);
    }
    return def;
}

EArc::EArcStateListener::EArcStateListener(EArc *eArc) {
    mEArc = eArc;
}

EArc::EArcStateListener::~EArcStateListener() {
}

void EArc::EArcStateListener::onEArcEvent(int eArcState) {
    IEArcStatus eArcStatus = mEArc->toEArcStatus(eArcState);
    ALOGD("%s state:%hhd", __FUNCTION__, eArcStatus);

    mEArc->handleEarcState(eArcStatus);
}

}  // namespace implementation
}  // namespace earc
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
