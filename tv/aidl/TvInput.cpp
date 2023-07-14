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

#define LOG_TAG "android.hardware.tv.input-service"

#include <utils/Log.h>
#include <cutils/properties.h>

#include "TvInput.h"
#include "tvcmd.h"
#include <hardware/hardware.h>

namespace aidl {
namespace android {
namespace hardware {
namespace tv {
namespace input {

shared_ptr<ITvInputCallback> TvInput::mCallback = nullptr;


TvInput::TvInput() {
    ALOGI("TvInput Hal Initialization starts");

    mModule.name = "TvInput";

    tv_input_device_open(&mModule, "default", reinterpret_cast<hw_device_t**>(&mDevice));

    mCallbackOps.notify = &TvInput::notify;
}

void TvInput::init() {
    // Set up TvInputDeviceInfo and TvStreamConfig
    mDeviceInfos[0] = shared_ptr<TvInputDeviceInfoWrapper>(
            new TvInputDeviceInfoWrapper(0, TvInputType::TUNER, true));
    mDeviceInfos[1] = shared_ptr<TvInputDeviceInfoWrapper>(
            new TvInputDeviceInfoWrapper(1, TvInputType::HDMI, true));
    mDeviceInfos[3] = shared_ptr<TvInputDeviceInfoWrapper>(
            new TvInputDeviceInfoWrapper(3, TvInputType::DISPLAY_PORT, true));

    mStreamConfigs[0] = {
            {1, shared_ptr<TvStreamConfigWrapper>(new TvStreamConfigWrapper(1, 720, 1080, false))}};
    mStreamConfigs[1] = {{11, shared_ptr<TvStreamConfigWrapper>(
                                      new TvStreamConfigWrapper(11, 360, 480, false))}};
    mStreamConfigs[3] = {{5, shared_ptr<TvStreamConfigWrapper>(
                                     new TvStreamConfigWrapper(5, 1080, 1920, false))}};
}

::ndk::ScopedAStatus TvInput::setCallback(const shared_ptr<ITvInputCallback>& in_callback) {
    ALOGV("%s", __FUNCTION__);

    mCallback = in_callback;

    if (mCallback != nullptr) {
        mDevice->initialize(mDevice, &mCallbackOps, nullptr);
    }

    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus TvInput::setTvMessageEnabled(int32_t deviceId, int32_t streamId,
                                                  TvMessageEventType in_type, bool enabled) {
    ALOGV("%s deviceId:%d streamId:%d enabled:%d", __FUNCTION__, deviceId, streamId, enabled);
    //todo

    if (mStreamConfigs.count(deviceId) == 0) {
        ALOGW("Device with id %d isn't available", deviceId);
        return ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_ARGUMENTS);
    }

    mTvMessageEventEnabled[deviceId][streamId][in_type] = enabled;
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus TvInput::getTvMessageQueueDesc(
        MQDescriptor<int8_t, SynchronizedReadWrite>* out_queue, int32_t in_deviceId,
        int32_t in_streamId) {
    ALOGV("%s deviceId:%d streamId:%d", __FUNCTION__, in_deviceId, in_streamId);
    //todo

    if (mStreamConfigs.count(in_deviceId) == 0) {
        ALOGW("Device with id %d isn't available", in_deviceId);
        return ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_ARGUMENTS);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus TvInput::getStreamConfigurations(int32_t in_deviceId,
                                                      vector<TvStreamConfig>* _aidl_return) {
    ALOGV("%s deviceId:%d", __FUNCTION__, in_deviceId);

    int32_t configCount = 0;
    const tv_stream_config_t* configs = nullptr;
    int ret = mDevice->get_stream_configurations(mDevice, in_deviceId, &configCount, &configs);
    vector<TvStreamConfig> tvStreamConfigs;
    if (ret == 0) {
        tvStreamConfigs.resize(getSupportedConfigCount(configCount, configs));
        int32_t pos = 0;
        for (int32_t i = 0; i < configCount; ++i) {
            if (isSupportedStreamType(configs[i].type)) {
                tvStreamConfigs[pos].streamId = configs[i].stream_id;
                tvStreamConfigs[pos].maxVideoWidth = configs[i].max_video_width;
                tvStreamConfigs[pos].maxVideoHeight = configs[i].max_video_height;
                _aidl_return->push_back(tvStreamConfigs[pos]);
                ALOGD("%s streamId:%d width:%d, height:%d", __FUNCTION__,
                     configs[i].stream_id, configs[i].max_video_width, configs[i].max_video_height);

                ++pos;
            }
        }
        return ::ndk::ScopedAStatus::ok();
    } else if (ret == -EINVAL) {
        return ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_NO_RESOURCE);
    }
    return ::ndk::ScopedAStatus::ok();
}

::ndk::ScopedAStatus TvInput::openStream(int32_t in_deviceId, int32_t in_streamId,
                                         ::aidl::android::hardware::common::NativeHandle* _aidl_return) {
    ALOGV("%s deviceId:%d, streamId:%d", __FUNCTION__, in_deviceId, in_streamId);

    tv_stream_t stream;
    stream.stream_id = in_streamId;
    int ret = mDevice->open_stream(mDevice, in_deviceId, &stream);
    native_handle_t* sidebandStream = nullptr;
    ::ndk::ScopedAStatus res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_UNKNOWN);
    if (ret == 0) {
        if (isSupportedStreamType(stream.type)) {
            sidebandStream = stream.sideband_stream_source_handle;
            *_aidl_return = makeToAidl(sidebandStream);
            res =  ::ndk::ScopedAStatus::ok();
        }
    } else {
        if (ret == -EBUSY) {
            res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_NO_RESOURCE);
        } else if (ret == -EEXIST) {
            res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_STATE);
        } else if (ret == -EINVAL) {
            res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_ARGUMENTS);
        }
    }

    return res;
}

::ndk::ScopedAStatus TvInput::closeStream(int32_t in_deviceId, int32_t in_streamId) {
    ALOGV("%s deviceId:%d, streamId:%d", __FUNCTION__, in_deviceId, in_streamId);

    int ret = mDevice->close_stream(mDevice, in_deviceId, in_streamId);
    ::ndk::ScopedAStatus res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_UNKNOWN);
    if (ret == 0) {
        res = ::ndk::ScopedAStatus::ok();
    } else if (ret == -EBUSY) {
        res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_NO_RESOURCE);
    } else if (ret == -EEXIST) {
        res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_STATE);
    } else if (ret == -EINVAL) {
        res = ::ndk::ScopedAStatus::fromServiceSpecificError(STATUS_INVALID_ARGUMENTS);
    }

    return res;
}

// static
void TvInput::notify(struct tv_input_device* __unused, tv_input_event_t* event,
                     void* optionalStatus) {
    if (mCallback != nullptr && event != nullptr) {
        // Capturing is no longer supported.
        if (event->type >= TV_INPUT_EVENT_CAPTURE_SUCCEEDED) {
            return;
        }
        TvInputEvent tvInputEvent;
        tvInputEvent.type = static_cast<TvInputEventType>(event->type);
        tvInputEvent.deviceInfo.deviceId = event->device_info.device_id;
        tvInputEvent.deviceInfo.type = static_cast<TvInputType>(
                event->device_info.type);
        tvInputEvent.deviceInfo.portId = event->device_info.hdmi.port_id;
        // aidl audio type is of audio_devices_t.
        // TvInputHardwareInfo {id=19, type=2, audio_type=13, audio_addr=, cable_connection_status=0}
        // TvInputHardwareInfo {id=19, type=2, audio_type=-2147467264, audio_addr=, cable_connection_status=0}
        tvInputEvent.deviceInfo.audioDevice.type.type =
                static_cast<AudioDeviceType>(event->device_info.audio_type);
        CableConnectionStatus connectionStatus = CableConnectionStatus::UNKNOWN;
        if (optionalStatus != nullptr &&
            ((event->type == TV_INPUT_EVENT_STREAM_CONFIGURATIONS_CHANGED) ||
             (event->type == TV_INPUT_EVENT_DEVICE_AVAILABLE))) {
            int newStatus = *reinterpret_cast<int*>(optionalStatus);
            if (newStatus <= static_cast<int>(CableConnectionStatus::DISCONNECTED) &&
                newStatus >= static_cast<int>(CableConnectionStatus::UNKNOWN)) {
                connectionStatus = static_cast<CableConnectionStatus>(newStatus);
            }
        }
        tvInputEvent.deviceInfo.cableConnectionStatus = connectionStatus;
        // TODO: Ensure the legacy audio type code is the same once audio HAL default
        // implementation is ready.
        switch (event->device_info.device_id) {
            case SOURCE_TV:
            case SOURCE_DTV:
            case SOURCE_ADTV:
            case SOURCE_DTVKIT:
            case SOURCE_DTVKIT_PIP:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_TV_TUNER;
                break;
            case SOURCE_AV1:
            case SOURCE_AV2:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_DEVICE;
                tvInputEvent.deviceInfo.audioDevice.type.connection = AudioDeviceDescription::CONNECTION_ANALOG;
                break;
            case SOURCE_HDMI1:
            case SOURCE_HDMI2:
            case SOURCE_HDMI3:
            case SOURCE_HDMI4:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_DEVICE;
                tvInputEvent.deviceInfo.audioDevice.type.connection = AudioDeviceDescription::CONNECTION_HDMI;
                break;
            case SOURCE_SPDIF:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_DEVICE;
                tvInputEvent.deviceInfo.audioDevice.type.connection = AudioDeviceDescription::CONNECTION_SPDIF;
                break;
            case SOURCE_AUX:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_DEVICE;
                tvInputEvent.deviceInfo.audioDevice.type.connection = AudioDeviceDescription::CONNECTION_ANALOG;
                break;
            case SOURCE_ARC:
                //tvInputEvent.deviceInfo.audioDevice.type.type = AudioDeviceType::IN_DEVICE;
                tvInputEvent.deviceInfo.audioDevice.type.connection = AudioDeviceDescription::CONNECTION_HDMI_ARC;
                break;
            default:
                break;
        }

        // todo the address from hal is always null now.
        // vInputEvent.deviceInfo.audioDevice.address.id = ?

        mCallback->notify(tvInputEvent);
    }
}

// static
uint32_t TvInput::getSupportedConfigCount(uint32_t configCount,
        const tv_stream_config_t* configs) {
    uint32_t supportedConfigCount = 0;
    for (uint32_t i = 0; i < configCount; ++i) {
        if (isSupportedStreamType(configs[i].type)) {
            supportedConfigCount++;
        }
    }
    return supportedConfigCount;
}

// static
bool TvInput::isSupportedStreamType(int type) {
    // Buffer producer type is no longer supported.
    return type != TV_STREAM_TYPE_BUFFER_PRODUCER;
}

}  // namespace input
}  // namespace tv
}  // namespace hardware
}  // namespace android
}  // namespace aidl
