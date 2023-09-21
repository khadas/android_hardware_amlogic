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

#define LOG_TAG "HdmiCec"
#include <android-base/logging.h>
#include <fcntl.h>
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/hdmi_cec.h>
#include "HdmiCec.h"

using ndk::ScopedAStatus;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace cec {
namespace implementation {

void HdmiCec::serviceDied(void* cookie) {
    ALOGE("HdmiCec client died %p", cookie);
    if (cookie == nullptr) {
        return;
    }
    HdmiCec* hdmicec = static_cast<HdmiCec*>(cookie);
    hdmicec->handleBinderDied();
}

ScopedAStatus HdmiCec::addLogicalAddress(CecLogicalAddress addr, Result* _aidl_return) {
    *_aidl_return = getReturnValue(
        mHdmiCecControl->addLogicalAddress((cec_logical_address_t)addr));
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::clearLogicalAddress() {
    mHdmiCecControl->clearLogicaladdress();
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::enableAudioReturnChannel(int32_t portId __unused, bool enable __unused) {
    mHdmiCecControl->setAudioReturnChannel(portId, enable);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::getCecVersion(int32_t* _aidl_return) {
    mHdmiCecControl->getVersion(_aidl_return);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::getPhysicalAddress(int32_t* _aidl_return) {
    mHdmiCecControl->getPhysicalAddress((uint16_t*)_aidl_return);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::getVendorId(int32_t* _aidl_return) {
    mHdmiCecControl->getVendorId((uint32_t*)_aidl_return);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::sendMessage(const CecMessage& message, SendMessageResult* _aidl_return) {
    cec_message_t msg;
    msg.initiator = (cec_logical_address_t)message.initiator;
    msg.destination = (cec_logical_address_t)message.destination;
    msg.length = std::min(static_cast<size_t>(message.body.size()),
                             static_cast<size_t>(CEC_MESSAGE_BODY_MAX_LENGTH));
    for (size_t i = 0; i < msg.length; ++i) {
        msg.body[i] = static_cast<unsigned char>(message.body[i]);
    }
    *_aidl_return = static_cast<SendMessageResult>(mHdmiCecControl->sendMessage(&msg));
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::setCallback(const std::shared_ptr<IHdmiCecCallback>& callback) {
    if (callback == nullptr) {
        ALOGE("%s callback is null!", __FUNCTION__);
        return ScopedAStatus::ok();
    }
    if (mIsVendorCallback) {
        ALOGD("%s set the specific vendor callback", __FUNCTION__);
        mVendorCallback = callback;
        mHdmiCecControl->setVendorEventObserver(new HdmiCecVendorCallback(this));
        mIsVendorCallback = false;
        return ScopedAStatus::ok();
    }
    pid_t pid = AIBinder_getCallingPid();
    ALOGD("%s pid:%d with current callback size:%d", __FUNCTION__, pid, mCallbackSize);

    AIBinder_linkToDeath(callback->asBinder().get(), mDeathRecipient.get(), this);
    mCallbackSize++;
    mCallbackMap[pid] = callback;
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::setLanguage(const std::string& language) {
    if (language.size() != 3) {
        LOG(ERROR) << "Wrong language code: expected 3 letters, but it was " << language.size()
                   << ".";
        return ScopedAStatus::ok();
    }
    // TODO Validate if language is a valid language code
    const char* languageStr = language.c_str();
    int convertedLanguage = ((languageStr[0] & 0xFF) << 16) | ((languageStr[1] & 0xFF) << 8) |
                            (languageStr[2] & 0xFF);
    ALOGI("setLanguage %2x", convertedLanguage);

    if (language == LANG_VENDOR_CALLBACK) {
        ALOGD("%s going to set vendor callback", __FUNCTION__);
        mIsVendorCallback = true;
    }
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::enableWakeupByOtp(bool value) {
    mHdmiCecControl->setOption(HDMI_OPTION_WAKEUP, value ? 1 : 0);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::enableCec(bool value) {
    mHdmiCecControl->setOption(HDMI_OPTION_ENABLE_CEC, value ? 1 : 0);
    return ScopedAStatus::ok();
}

ScopedAStatus HdmiCec::enableSystemCecControl(bool value) {
    mHdmiCecControl->setOption(HDMI_OPTION_SYSTEM_CEC_CONTROL, value ? 1 : 0);
    return ScopedAStatus::ok();
}

HdmiCec::HdmiCec() {
    ALOGI("Initlizing CEC HAL");
    mDeathRecipient = ndk::ScopedAIBinder_DeathRecipient(AIBinder_DeathRecipient_new(serviceDied));
    mHdmiCecControl = std::make_shared<HdmiCecControl>(HDMI_EVENT_CEC_MESSAGE);
    mHdmiCecControl->setEventObserver(new HdmiCecCallback(this));

    mCallbackSize = 0;

    mVendorCallback = nullptr;
    mIsVendorCallback = false;
}

void HdmiCec::handleBinderDied() {
    ALOGD("%s callback size:%d", __FUNCTION__, mCallbackSize);
    for (auto it = mCallbackMap.begin(); it != mCallbackMap.end();) {
        if (!AIBinder_isAlive(it->second->asBinder().get())) {
            // Remove the key-value pair if the condition is met
            it = mCallbackMap.erase(it);
            mCallbackSize--;
        } else {
            // Move to the next element
            ++it;
        }
    }
}

Result HdmiCec::getReturnValue(int result) {
    if (result == 0) {
        return Result::SUCCESS;
    }
    return Result::FAILURE_INVALID_STATE;
}

void HdmiCec::getAidlCecMessage(const hdmi_cec_event_t* cecEvent, CecMessage& message) {
    size_t length = std::min(static_cast<size_t>(cecEvent->cec.length),
                             static_cast<size_t>(CEC_MESSAGE_BODY_MAX_LENGTH));
    message.body.resize(length);

    for (size_t i = 0; i < length; ++i) {
        message.body[i] = static_cast<uint8_t>(cecEvent->cec.body[i]);
    }

    message.initiator = static_cast<CecLogicalAddress>(cecEvent->cec.initiator);
    message.destination = static_cast<CecLogicalAddress>(cecEvent->cec.destination);
}

HdmiCec::HdmiCecCallback::HdmiCecCallback(HdmiCec* hdmiCec) {
    mHdmiCec = hdmiCec;
}

void HdmiCec::HdmiCecCallback::onEventUpdate(const hdmi_cec_event_t* cecEvent)
{
    if (cecEvent == nullptr) return;
    if (mHdmiCec->mCallbackSize == 0) {
        ALOGI("no cec callback exists");
        return;
    }
    //ALOGI("%s event type:%d callback size:%d", __FUNCTION__, cecEvent->eventType,
    //    mHdmiCec->mCallbackSize);

    if (cecEvent->eventType == HDMI_EVENT_CEC_MESSAGE) {
        CecMessage message;
        mHdmiCec->getAidlCecMessage(cecEvent, message);

        for (const auto& pair : mHdmiCec->mCallbackMap) {
            if (pair.second != nullptr) {
                pair.second->onCecMessage(message);
            }
        }
    }
}

HdmiCec::HdmiCecVendorCallback::HdmiCecVendorCallback(HdmiCec* hdmiCec) {
    mHdmiCec = hdmiCec;
}

void HdmiCec::HdmiCecVendorCallback::onEventUpdate(const hdmi_cec_event_t* cecEvent)
{
    if (cecEvent == nullptr) return;
    if (mHdmiCec->mVendorCallback == nullptr) {
        ALOGI("no vendor cec callback exists");
        return;
    }

    if (cecEvent->eventType == HDMI_EVENT_VENDOR_MESSAGE) {
        CecMessage message;
        mHdmiCec->getAidlCecMessage(cecEvent, message);
        mHdmiCec->mVendorCallback->onCecMessage(message);
    }
}

}  // namespace implementation
}  // namespace cec
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
