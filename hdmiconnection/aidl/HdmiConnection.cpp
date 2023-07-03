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

#define LOG_TAG "android.hardware.tv.hdmi.connection"
#include <android-base/logging.h>
#include <fcntl.h>
#include <utils/Log.h>

#include "HdmiConnection.h"

using ndk::ScopedAStatus;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace connection {
namespace implementation {

void HdmiConnection::serviceDied(void* cookie) {
    ALOGE("HdmiConnection died");
    auto hdmi = static_cast<HdmiConnection*>(cookie);
    hdmi->mHdmiThreadRun = false;
}

ScopedAStatus HdmiConnection::getPortInfo(std::vector<HdmiPortInfo>* _aidl_return) {
    if (mHdmiPorts != nullptr) {
        delete[] mHdmiPorts;
    }
    mHdmiPorts = new hdmi_port_info[mTotalPorts];
    mHdmiCecControl->getPortInfos(&mHdmiPorts, &mTotalPorts);

    if (!mHdmiPorts || mTotalPorts < 1) {
        ALOGE("getPortInfo but no port information exists");
        return ScopedAStatus::ok();
    }
    mPortInfos.resize(mTotalPorts);
    for (int i = 0; i < mTotalPorts; i++) {
        mPortInfos[i] = {.type = static_cast<HdmiPortType>(mHdmiPorts[i].type),
                         .portId = mHdmiPorts[i].port_id,
                         .cecSupported = mHdmiPorts[i].cec_supported == 1,
                         .arcSupported = mHdmiPorts[i].arc_supported == 1,
                         .eArcSupported = mHdmiPorts[i].arc_supported == 1 ? mEarcSupported : false,
                         .physicalAddress = mHdmiPorts[i].physical_address};
    }

    *_aidl_return = mPortInfos;
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::isConnected(int32_t portId, bool* _aidl_return) {
    // Maintain port connection status and update on hotplug event
    if (portId <= mTotalPorts && portId >= 1) {
        *_aidl_return = mHdmiCecControl->isConnected(portId); //mPortConnectionStatus.at(portId - 1);
         ALOGD("isConnected:%d", *_aidl_return);
    } else {
        *_aidl_return = false;
    }

    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::setCallback(
        const std::shared_ptr<IHdmiConnectionCallback>& callback) {
    if (mCallback != nullptr) {
        mCallback = nullptr;
    }

    if (callback != nullptr) {
        mCallback = callback;
        mHdmiCecControl->setEventObserver(new HdmiConnectionCallback(this));
        AIBinder_linkToDeath(this->asBinder().get(), mDeathRecipient.get(), 0 /* cookie */);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::setHpdSignal(HpdSignal signal, int32_t portId) {
    //todo
    if (portId > mTotalPorts || portId < 1) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    if (!mHdmiThreadRun) {
        return ScopedAStatus::fromServiceSpecificError(
                static_cast<int32_t>(Result::FAILURE_INVALID_STATE));
    }
    mHpdSignal.at(portId - 1) = signal;
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::getHpdSignal(int32_t portId, HpdSignal* _aidl_return) {
    //todo
    if (portId > mTotalPorts || portId < 1) {
        return ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    *_aidl_return = mHpdSignal.at(portId - 1);
    return ScopedAStatus::ok();
}


HdmiConnection::HdmiConnection() {
    ALOGI("Opening IHdmi Connection HAL");
    mCallback = nullptr;
    mPortInfos.resize(mTotalPorts);
    mPortConnectionStatus.resize(mTotalPorts);
    mHpdSignal.resize(mTotalPorts);
    mPortInfos[0] = {.type = HdmiPortType::OUTPUT,
                     .portId = static_cast<uint32_t>(1),
                     .cecSupported = true,
                     .arcSupported = false,
                     .eArcSupported = false,
                     .physicalAddress = mPhysicalAddress};
    mPortConnectionStatus[0] = false;
    mHpdSignal[0] = HpdSignal::HDMI_HPD_PHYSICAL;
    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient_new(serviceDied));
    mHdmiCecControl = std::make_shared<HdmiCecControl>(HDMI_EVENT_HOT_PLUG);
    mEarcSupported = android::getPropertyBoolean(PROPERTY_EARC_SUPPORTED, false);
    mHdmiPorts = new hdmi_port_info[mTotalPorts];
}

HdmiConnection::HdmiConnectionCallback::HdmiConnectionCallback(HdmiConnection* hdmiConnection) {
    mHdmiConnection = hdmiConnection;
}

void HdmiConnection::HdmiConnectionCallback::onEventUpdate(const hdmi_cec_event_t* cecEvent)
{
    if (cecEvent == nullptr) return;
    if (mHdmiConnection->mCallback == nullptr) {
        ALOGI("no cec callback exists");
        return;
    }

    if ((cecEvent->eventType & HDMI_EVENT_HOT_PLUG) != 0) {
        bool connected = cecEvent->hotplug.connected & 0xf;
        int32_t portId = static_cast<uint32_t>( cecEvent->hotplug.port_id);

        if (portId > static_cast<int32_t>(mHdmiConnection->mPortInfos.size())) {
            ALOGD("ignore hot plug message, id %x does not exist", portId);
            return;
        }

        mHdmiConnection->mPortConnectionStatus.at(portId) = connected;
        ALOGI("hot plug port id %x, is connected %x", portId, connected);

        mHdmiConnection->mCallback->onHotplugEvent(connected, portId);
    }
}


}  // namespace implementation
}  // namespace connection
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
