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

#include <aidl/android/hardware/tv/hdmi/cec/BnHdmiCec.h>
#include <algorithm>
#include <vector>

#include "HdmiCecControl.h"

using namespace std;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace cec {
namespace implementation {

using ::aidl::android::hardware::tv::hdmi::cec::BnHdmiCec;
using ::aidl::android::hardware::tv::hdmi::cec::CecLogicalAddress;
using ::aidl::android::hardware::tv::hdmi::cec::CecMessage;
using ::aidl::android::hardware::tv::hdmi::cec::IHdmiCec;
using ::aidl::android::hardware::tv::hdmi::cec::IHdmiCecCallback;
using ::aidl::android::hardware::tv::hdmi::cec::Result;
using ::aidl::android::hardware::tv::hdmi::cec::SendMessageResult;

struct HdmiCec : public BnHdmiCec {
    HdmiCec();
    ::ndk::ScopedAStatus addLogicalAddress(CecLogicalAddress addr, Result* _aidl_return) override;
    ::ndk::ScopedAStatus clearLogicalAddress() override;
    ::ndk::ScopedAStatus enableAudioReturnChannel(int32_t portId, bool enable) override;
    ::ndk::ScopedAStatus getCecVersion(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getPhysicalAddress(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus getVendorId(int32_t* _aidl_return) override;
    ::ndk::ScopedAStatus sendMessage(const CecMessage& message,
                                     SendMessageResult* _aidl_return) override;
    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<IHdmiCecCallback>& callback) override;
    ::ndk::ScopedAStatus setLanguage(const std::string& language) override;
    ::ndk::ScopedAStatus enableWakeupByOtp(bool value) override;
    ::ndk::ScopedAStatus enableCec(bool value) override;
    ::ndk::ScopedAStatus enableSystemCecControl(bool value) override;
    void printCecMsgBuf(const char* msg_buf, int len);

  protected:
    class HdmiCecCallback : public HdmiCecEventListener {
    public:
        HdmiCecCallback(HdmiCec* hdmiCec);
        ~HdmiCecCallback(){}
        virtual void onEventUpdate(const hdmi_cec_event_t* event);

    private:
        HdmiCec* mHdmiCec;
    };

  private:
    static void serviceDied(void* cookie);
    Result getReturnValue(int result);

    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    std::shared_ptr<IHdmiCecCallback> mCallback;
    std::shared_ptr<android::HdmiCecControl> mHdmiCecControl;

};



}  // namespace implementation
}  // namespace cec
}  // namespace hdmi
}  // namespace tv
}  // namespace hardware
}  // namespace android
