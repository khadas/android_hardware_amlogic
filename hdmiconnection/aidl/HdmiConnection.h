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

#include <aidl/android/hardware/tv/hdmi/connection/BnHdmiConnection.h>
#include <aidl/android/hardware/tv/hdmi/connection/Result.h>
#include <algorithm>
#include <vector>

#include "HdmiCecControl.h"

using namespace std;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace connection {
namespace implementation {

using ::aidl::android::hardware::tv::hdmi::connection::BnHdmiConnection;
using ::aidl::android::hardware::tv::hdmi::connection::HdmiPortInfo;
using ::aidl::android::hardware::tv::hdmi::connection::HdmiPortType;
using ::aidl::android::hardware::tv::hdmi::connection::HpdSignal;
using ::aidl::android::hardware::tv::hdmi::connection::IHdmiConnection;
using ::aidl::android::hardware::tv::hdmi::connection::IHdmiConnectionCallback;
using ::aidl::android::hardware::tv::hdmi::connection::Result;

#define HDMI_MSG_IN_FIFO "/dev/hdmi_in_pipe"
#define MESSAGE_BODY_MAX_LENGTH 4
#define HDMIRX_DEV_PATH "/dev/hdmirx0"

// struct used in hdmi driver
struct hdmirx_hpd_info {
    int signal;
    int port;
};

#define HDMI_IOC_MAGIC 'H'
#define HDMI_IOC_SET_HPD   _IOW(HDMI_IOC_MAGIC, 0x15, struct hdmirx_hpd_info)
#define HDMI_IOC_GET_HPD   _IOR(HDMI_IOC_MAGIC, 0x16, struct hdmirx_hpd_info)

struct HdmiConnection : public BnHdmiConnection {
    HdmiConnection();
    virtual ~HdmiConnection();

    ::ndk::ScopedAStatus getPortInfo(std::vector<HdmiPortInfo>* _aidl_return) override;
    ::ndk::ScopedAStatus isConnected(int32_t portId, bool* _aidl_return) override;
    ::ndk::ScopedAStatus setCallback(
            const std::shared_ptr<IHdmiConnectionCallback>& callback) override;
    ::ndk::ScopedAStatus setHpdSignal(HpdSignal signal, int32_t portId) override;
    ::ndk::ScopedAStatus getHpdSignal(int32_t portId, HpdSignal* _aidl_return) override;

    void printEventBuf(const char* msg_buf, int len);

  protected:
    class HdmiConnectionCallback : public HdmiCecEventListener {
    public:
        HdmiConnectionCallback(HdmiConnection* hdmiConnection);
        ~HdmiConnectionCallback(){}
        virtual void onEventUpdate(const hdmi_cec_event_t* event);

    private:
        HdmiConnection* mHdmiConnection;
    };

  private:
    static void serviceDied(void* cookie);
    std::shared_ptr<IHdmiConnectionCallback> mCallback;

    // Variables for the virtual HDMI hal impl
    std::vector<HdmiPortInfo> mPortInfos;
    std::vector<bool> mPortConnectionStatus;

    // Port configuration
    uint16_t mPhysicalAddress = 0xFFFF;
    int mTotalPorts = 1;

    int mHdmiFd = -1;

    // HPD Signal being used
    std::vector<HpdSignal> mHpdSignal;

    HpdSignal mTxHpdSignal;

    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    std::shared_ptr<android::HdmiCecControl> mHdmiCecControl;
    bool mEarcSupported = false;
    hdmi_port_info_t* mHdmiPorts;
};
}  // namespace implementation
}  // namespace connection
}  // namespace hdmi
}  // Namespace tv
}  // namespace hardware
}  // namespace android
