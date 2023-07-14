/*
 * Copyright 2022 The Android Open Source Project
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

#pragma once

#include <aidl/android/hardware/tv/input/BnTvInput.h>
#include <utils/KeyedVector.h>

#include <aidl/android/hardware/tv/input/TvMessageEventType.h>
#include <aidl/android/media/audio/common/AudioDeviceDescription.h>
#include <fmq/AidlMessageQueue.h>
#include <map>
#include <unordered_map>
#include "TvInputDeviceInfoWrapper.h"
#include "TvStreamConfigWrapper.h"

#include "tv_input.h"
#include "TvInputIntf.h"

using namespace android;
using namespace std;
using ::aidl::android::hardware::common::NativeHandle;
using ::aidl::android::hardware::common::fmq::MQDescriptor;
using ::aidl::android::hardware::common::fmq::SynchronizedReadWrite;
using ::aidl::android::media::audio::common::AudioDeviceDescription;
using ::aidl::android::media::audio::common::AudioDeviceType;

using ::android::AidlMessageQueue;

namespace aidl {
namespace android {
namespace hardware {
namespace tv {
namespace input {

using TvMessageEnabledMap = std::unordered_map<
        int32_t, std::unordered_map<int32_t, std::unordered_map<TvMessageEventType, bool>>>;

class TvInput : public BnTvInput {
  public:
    TvInput();

    ::ndk::ScopedAStatus setCallback(const shared_ptr<ITvInputCallback>& in_callback) override;
    ::ndk::ScopedAStatus setTvMessageEnabled(int32_t deviceId, int32_t streamId,
                                             TvMessageEventType in_type, bool enabled) override;
    ::ndk::ScopedAStatus getTvMessageQueueDesc(
            MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
            int32_t in_streamId) override;
    ::ndk::ScopedAStatus getStreamConfigurations(int32_t in_deviceId,
                                                 vector<TvStreamConfig>* _aidl_return) override;
    ::ndk::ScopedAStatus openStream(int32_t in_deviceId, int32_t in_streamId,
                                    ::aidl::android::hardware::common::NativeHandle* _aidl_return) override;
    ::ndk::ScopedAStatus closeStream(int32_t in_deviceId, int32_t in_streamId) override;
    void init();

  private:
    static void notify(struct tv_input_device* __unused, tv_input_event_t* event,
            void* __unused);
    static uint32_t getSupportedConfigCount(uint32_t configCount,
            const tv_stream_config_t* configs);
    static bool isSupportedStreamType(int type);

    static shared_ptr<ITvInputCallback> mCallback;
    map<int32_t, shared_ptr<TvInputDeviceInfoWrapper>> mDeviceInfos;
    map<int32_t, map<int32_t, shared_ptr<TvStreamConfigWrapper>>> mStreamConfigs;
    TvMessageEnabledMap mTvMessageEventEnabled;

    hw_module_t mModule;

    tv_input_device_t* mDevice;
    tv_input_callback_ops_t mCallbackOps;

    TvInputIntf* mTvInputIntf;
    int mSupportDevices[20];
    int mDeviceCount;
};

}  // namespace input
}  // namespace tv
}  // namespace hardware
}  // namespace android
}  // namespace aidl
