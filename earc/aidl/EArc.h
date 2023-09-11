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

#include <aidl/android/hardware/tv/hdmi/earc/BnEArc.h>
#include <aidl/android/hardware/tv/hdmi/earc/Result.h>
#include <algorithm>
#include <vector>

#include <aml_alsa_mixer.h>
#include "EArcObserver.h"
#include "earc_utils.h"

#define PROPERTY_EARC_SUPPORTED         "ro.vendor.media.support_earc"
#define PROPERTY_EARC_Enabled           "persist.vendor.sys.earc_enabled"
#define PROPERTY_ARC_PORT               "persist.vendor.sys.arc_port"
#define PROPERTY_EARC_PORT              "ro.vendor.hdmi.arc_port"

#define EARC_PORT_DEFAULT               2
#define EARC_PORT_STR_DEFAULT           "2"

#define EARC_CDS_CHAR_MAX_LEN           256

using namespace std;
using namespace android;

namespace android {
namespace hardware {
namespace tv {
namespace hdmi {
namespace earc {
namespace implementation {

using ::aidl::android::hardware::tv::hdmi::earc::BnEArc;
using ::aidl::android::hardware::tv::hdmi::earc::IEArc;
using ::aidl::android::hardware::tv::hdmi::earc::IEArcCallback;
using ::aidl::android::hardware::tv::hdmi::earc::IEArcStatus;
using ::aidl::android::hardware::tv::hdmi::earc::Result;

class EArc : public BnEArc {
public:
    EArc();
    virtual ~EArc();

    ::ndk::ScopedAStatus setEArcEnabled(bool in_enabled) override;
    ::ndk::ScopedAStatus isEArcEnabled(bool* _aidl_return) override;
    ::ndk::ScopedAStatus setCallback(const std::shared_ptr<IEArcCallback>& in_callback) override;
    ::ndk::ScopedAStatus getState(int32_t in_portId, IEArcStatus* _aidl_return) override;
    ::ndk::ScopedAStatus getLastReportedAudioCapabilities(
            int32_t in_portId, std::vector<uint8_t>* _aidl_return) override;
    ::ndk::ScopedAStatus reportCapabilities(const std::vector<uint8_t>& capabilities,
                                            int32_t portId);
    ::ndk::ScopedAStatus changeState(const IEArcStatus status, int32_t portId);

protected:
    class EArcStateListener: public EArcListener {
    public:
        EArcStateListener(EArc *eArc);
        virtual ~EArcStateListener();
        virtual void onEArcEvent(int eArcState);

    private:
        EArc *mEArc;
    };

private:
    IEArcStatus toEArcStatus(int state);
    void handleEarcState(IEArcStatus status);
    int getPropertyInt(const char * key, int def, const char* defaultValue);

private:
    static void serviceDied(void* cookie);
    shared_ptr<IEArcCallback> mCallback;
    sp<EArcObserver> mEArcObserver;
    shared_ptr<EArcStateListener> mEArcStateListener;

    std::vector<uint8_t> mCapabilities;

    IEArcStatus mPortStatus;
    bool mEArcEnabled = true;
    bool mEArcSupported = false;

    int mEArcPort = EARC_PORT_DEFAULT;

    ::ndk::ScopedAIBinder_DeathRecipient mDeathRecipient;

    struct aml_mixer_handle mAlsaMixer;
    /* The HDMI ARC capability info currently set. */
    struct aml_arc_hdmi_desc mArcHdmiDesc;

};
}  // namespace implementation
}  // namespace earc
}  // Namespace hdmi
}  // Namespace tv
}  // namespace hardware
}  // namespace android
