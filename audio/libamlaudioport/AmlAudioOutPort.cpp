/*
 * Copyright (C) 2017 The Android Open Source Project
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

#define LOG_TAG "AmlAudioOutPort"
#include <AmlAudioOutPort.h>
#include <utils/Log.h>

#include <media/audiohal/DeviceHalInterface.h>
#include <media/audiohal/StreamHalInterface.h>
#include <media/audiohal/DevicesFactoryHalInterface.h>

namespace android {
    AmlAudioOutPort::AmlAudioOutPort(audio_stream_type_t streamType,
                        uint32_t sampleRate,
                        audio_format_t format,
                        audio_channel_mask_t channelMask,
                        audio_output_flags_t flags) {
         handle = -1;
         streamType = AUDIO_STREAM_MUSIC;
         config.sample_rate = sampleRate;
         config.format = format;
         config.channel_mask = channelMask;
         if (hwDevice == NULL)
            getHwDevice();
         audio_output_flags_t customFlags = (config.format == AUDIO_FORMAT_IEC61937)
                ? (audio_output_flags_t)(flags | AUDIO_OUTPUT_FLAG_IEC958_NONAUDIO)
                : flags;

        int status = hwDevice->openOutputStream(
                handle,
                devices,
                customFlags,
                &config,
                &address,
                &outStream);
        ALOGI("AudioStreamOut::open(), HAL returned "
                " stream %p, sampleRate %d, Format %#x, "
                "channelMask %#x, status %d",
                outStream.get(),
                config.sample_rate,
                config.format,
                config.channel_mask,
                status);
        if (status == NO_ERROR) {
            ALOGI("get outStream success");
        }
        if (flags == (audio_output_flags_t)(AUDIO_OUTPUT_FLAG_HW_AV_SYNC|AUDIO_OUTPUT_FLAG_DIRECT)) {
            outStream->setParameters(String8("hw_av_sync=12345678"));
        }

    }

    AmlAudioOutPort::AmlAudioOutPort() {
         if (hwDevice == NULL)
            getHwDevice();
    }

    status_t AmlAudioOutPort::standby() {
        return outStream->standby();
    }

    status_t AmlAudioOutPort::start() {
        return outStream->start();
    }
    void    AmlAudioOutPort::stop() {
               outStream->stop();
    }
    void    AmlAudioOutPort::flush() {
            outStream->flush();
    }
    void    AmlAudioOutPort::pause() {
        outStream->pause();
    }
    status_t    AmlAudioOutPort::setVolume(float left, float right) {
        return outStream->setVolume(left,right);
    }
    status_t    AmlAudioOutPort::setVolume(float volume) {
        return setVolume(volume,volume);
    }
    status_t    AmlAudioOutPort::getPosition(uint32_t *position) {
        return outStream->getRenderPosition(position);
    }
    ssize_t     AmlAudioOutPort::write(const void* buffer, size_t size, bool blocking) {
        size_t written = 0;
        ALOGV("blocking:%d  ",blocking);
        return outStream->write(buffer,size,&written);
    }

    status_t AmlAudioOutPort::setParameters(const String8& keyValuePairs) {
        status_t err = NO_ERROR;

        if (hwDevice == NULL)
            getHwDevice();

        err = hwDevice->setParameters(keyValuePairs);
        ALOGI("setParameters:%s, err=%d", keyValuePairs.string(), err);

        return err;
    }

    String8  AmlAudioOutPort::getParameters(const String8& keys) {
        status_t err = NO_ERROR;
        String8 mString = String8("");

        if (hwDevice == NULL)
            getHwDevice();

        err = hwDevice->getParameters(keys, &mString);
        if (err != NO_ERROR) {
            ALOGI("getParameters err: err=%d", err);
        }

        ALOGI("getParameters:keys:%s, return value:%s", keys.string(), mString.string());

        return mString;
    }

    void AmlAudioOutPort::getHwDevice() {
        if (mDevicesFactoryHal == nullptr) {
            mDevicesFactoryHal = DevicesFactoryHalInterface::create();
            if (mDevicesFactoryHal == nullptr)
                ALOGI("get DevicesFactoryHal fail");
            else
                ALOGI("get DevicesFactoryHal sucess ");
            int rc = mDevicesFactoryHal->openDevice("primary", &hwDevice);
            if (rc == NO_ERROR) {
                ALOGI("get hwDevice success ");
            } else {
                ALOGI("get hwDevice fail");
            }

            rc = hwDevice->initCheck();
            if (rc == NO_ERROR) {
                ALOGI("hwDevice init check success ");
            } else {
                ALOGE("hwDevice init check fail");
            }
         }

    }
    status_t AmlAudioOutPort::createAudioPatch() {
        status_t err = NO_ERROR;
        if (hwDevice == NULL)
            getHwDevice();
        const struct audio_port_config sources = { .id = 1,
            .role = AUDIO_PORT_ROLE_SOURCE,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext = {.device =
                   {.type = AUDIO_DEVICE_IN_TV_TUNER}
            }
        };

        const struct audio_port_config sinks = { .id = 2,
            .role = AUDIO_PORT_ROLE_SINK,
            .type = AUDIO_PORT_TYPE_DEVICE,
            .ext = {.device =
                   {.type = AUDIO_DEVICE_OUT_HDMI}
            }
        };

        err = hwDevice->createAudioPatch(1,
                                &sources,
                                1,
                                &sinks,
                                &patch);
        return err;

    }

    status_t AmlAudioOutPort::releaseAudioPatch() {
        status_t err = NO_ERROR;
         if (hwDevice == NULL)
            getHwDevice();
         if (patch != 0)
           hwDevice->releaseAudioPatch(patch);
         return err;
    }


}
