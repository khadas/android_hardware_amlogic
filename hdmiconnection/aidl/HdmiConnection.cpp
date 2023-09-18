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

#define LOG_TAG "hdmiconnection"
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
    ALOGE("HdmiConnection client died %p", cookie);
    HdmiConnection* hdmi = static_cast<HdmiConnection*>(cookie);
    if (hdmi == nullptr) {
        return;
    }
    hdmi->mCallback = nullptr;
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
        ALOGD("port id:%d physical:%2x", mHdmiPorts[i].port_id, mHdmiPorts[i].physical_address);
    }

    *_aidl_return = mPortInfos;
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::isConnected(int32_t portId, bool* _aidl_return) {
    if (portId < 0 || portId > mTotalPorts) {
        ALOGE("isConnected but port:%d is invalid!", portId);
        *_aidl_return = false;
        return ScopedAStatus::ok();

    }
    // Maintain port connection status and update on hotplug event

    *_aidl_return = mHdmiCecControl->isConnected(portId); //mPortConnectionStatus.at(portId - 1);
    ALOGD("isConnected:%d", *_aidl_return);

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
        AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(), this);
    }
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::setHpdSignal(HpdSignal signal, int32_t portId) {
    ALOGD("%s signal:%d portId:%d", __FUNCTION__, static_cast<int>(signal), portId);
    if (portId == 0) {
        // todo support hdmi tx.
        mTxHpdSignal = signal;
        return ScopedAStatus::ok();
    }
    struct HdmiHpdInfo hpdInfo = {signal, portId - 1};

    if (portId > mTotalPorts) {
        ALOGD("%s, invalid port id:%d port size:%d", __FUNCTION__, portId, mTotalPorts);
        // binder_auto_utils.h
        return ScopedAStatus::ok();
    }
    if (mHdmiFd < 0) {
        mHdmiFd = open(HDMIRX_DEV_PATH, O_RDWR);
    }

    if (mHdmiFd < 0) {
          ALOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, HDMIRX_DEV_PATH, strerror(errno));
          return ScopedAStatus::ok();
    }
    if (ioctl(mHdmiFd, HDMI_IOC_SET_HPD, &hpdInfo) < 0)
         LOGE("%s, port:%d, error: (%s)!\n", __FUNCTION__, portId, strerror(errno));
    mHpdSignal.at(portId - 1) = signal;
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiConnection::getHpdSignal(int32_t portId, HpdSignal* _aidl_return) {
    ALOGD("%s portId:%d", __FUNCTION__, portId);
    if (portId == 0) {
        // todo support hdmi tx.
        *_aidl_return = mTxHpdSignal;
        return ScopedAStatus::ok();
    }

    struct HdmiHpdInfo hpdInfo = {HpdSignal::HDMI_HPD_PHYSICAL, portId - 1};

    if (portId > mTotalPorts) {
        ALOGD("%s, invalid port id:%d port size:%d", __FUNCTION__, portId, mTotalPorts);
        return ScopedAStatus::ok();
    }
    if (mHdmiFd < 0) {
        mHdmiFd = open(HDMIRX_DEV_PATH, O_RDWR);
    }
    if (mHdmiFd < 0) {
          ALOGE("%s, Open file %s error: (%s)!\n", __FUNCTION__, HDMIRX_DEV_PATH, strerror(errno));
          return ScopedAStatus::ok();
    }
    if (ioctl(mHdmiFd, HDMI_IOC_GET_HPD, &hpdInfo) < 0)
        LOGE("%s, port:%d, error: (%s)!\n", __FUNCTION__, portId, strerror(errno));
    mHpdSignal[portId - 1] = hpdInfo.signal;
    *_aidl_return = mHpdSignal.at(portId - 1);
    return ScopedAStatus::ok();
}


HdmiConnection::HdmiConnection() {
    ALOGI("Opening IHdmi Connection HAL");
    mCallback = nullptr;
    mHdmiCecControl = std::make_shared<HdmiCecControl>(HDMI_EVENT_HOT_PLUG);

    getPortInfo(&mPortInfos);
    mTotalPorts = mPortInfos.size();
    ALOGI("%s port size:%d", __FUNCTION__, mTotalPorts);
    mPortConnectionStatus.resize(mTotalPorts, false);
    mHpdSignal.resize(mTotalPorts, HpdSignal::HDMI_HPD_PHYSICAL);
    mTxHpdSignal = HpdSignal::HDMI_HPD_PHYSICAL;

    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient_new(serviceDied));

    mEarcSupported = android::getPropertyBoolean(PROPERTY_EARC_SUPPORTED, false);
    mHdmiPorts = new hdmi_port_info[mTotalPorts];
}

HdmiConnection::~HdmiConnection() {
    ALOGD("quit HdmiConnection fd:%d", mHdmiFd);
    if (mHdmiFd > 0) {
        close(mHdmiFd);
    }
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
