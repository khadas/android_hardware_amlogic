/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#define LOG_TAG  "a2dp_hal"

#include <system/audio.h>
#include <cutils/log.h>
#include <android-base/strings.h>

#include "a2dp_hal.h"
#include "a2dp_hw.h"
#include "audio_hw.h"

using ::android::bluetooth::audio::BluetoothAudioPortOut;

#define MAX_RESAMPLE_CHANNEL 8
struct aml_resample {
    unsigned int FractionStep;
    unsigned int SampleFraction;
    unsigned int input_sr;
    unsigned int output_sr;
    unsigned int channels;
    int16_t lastsample[MAX_RESAMPLE_CHANNEL];
};

struct aml_a2dp_hal {
    BluetoothAudioPortOut a2dphw;
    audio_config config;
    struct aml_resample * resample;
    char* buff;
    size_t buffsize;
    int64_t last_write_time;
    mutable std::mutex mutex_;
};

inline static short clip(int x) {
    if (x < -32768) {
        return -32768;
    } else if (x > 32767) {
        return 32767;
    } else {
        return x;
    }
}

int resampler_init(struct aml_resample *resample) {

    ALOGD("%s, Init Resampler: input_sr = %d, output_sr = %d \n",
        __FUNCTION__,resample->input_sr,resample->output_sr);

    static const double kPhaseMultiplier = 1L << 28;
    unsigned int i;

    if (resample->channels > MAX_RESAMPLE_CHANNEL) {
        ALOGE("Error: %s, max support channels: %d\n",
        __FUNCTION__, MAX_RESAMPLE_CHANNEL);
        return -1;
    }

    resample->FractionStep = (unsigned int) (resample->input_sr * kPhaseMultiplier
                            / resample->output_sr);
    resample->SampleFraction = 0;
    for (i = 0; i < resample->channels; i++)
        resample->lastsample[i] = 0;

    return 0;
}

int resample_process(struct aml_resample *resample, unsigned int in_frame,
        int16_t* input, int16_t* output) {
    unsigned int inputIndex = 0;
    unsigned int outputIndex = 0;
    unsigned int FractionStep = resample->FractionStep;
    int16_t last_sample[MAX_RESAMPLE_CHANNEL];
    unsigned int i;
    unsigned int channels = resample->channels;

    static const unsigned int kPhaseMask = (1LU << 28) - 1;
    unsigned int frac = resample->SampleFraction;

    for (i = 0; i < channels; i++)
        last_sample[i] = resample->lastsample[i];


    while (inputIndex == 0) {
        for (i = 0; i < channels; i++) {
            *output++ = clip((int) last_sample[i] +
                ((((int) input[i] - (int) last_sample[i]) * ((int) frac >> 13)) >> 15));
        }

        frac += FractionStep;
        inputIndex += (frac >> 28);
        frac = (frac & kPhaseMask);
        outputIndex++;
    }

    while (inputIndex < in_frame) {
        for (i = 0; i < channels; i++) {
            *output++ = clip((int) input[channels * (inputIndex - 1) + i] +
                ((((int) input[channels * inputIndex + i]
                - (int) input[channels * (inputIndex - 1) + i]) * ((int) frac >> 13)) >> 15));
        }

        frac += FractionStep;
        inputIndex += (frac >> 28);
        frac = (frac & kPhaseMask);
        outputIndex++;
    }

    resample->SampleFraction = frac;

    for (i = 0; i < channels; i++)
        resample->lastsample[i] = input[channels * (in_frame - 1) + i];

    return outputIndex;
}

std::unordered_map<std::string, std::string> ParseAudioParams(const std::string& params) {
    std::vector<std::string> segments = android::base::Split(params, ";");
    std::unordered_map<std::string, std::string> params_map;
    for (const auto& segment : segments) {
        if (segment.length() == 0) {
            continue;
        }
        std::vector<std::string> kv = android::base::Split(segment, "=");
        if (kv[0].empty()) {
            //ALOGD("%s: Invalid audio parameter: ", __func__, segment.char());
            continue;
        }
        params_map[kv[0]] = (kv.size() > 1 ? kv[1] : "");
    }
    return params_map;
}

int a2dp_out_open(struct audio_hw_device* dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_a2dp_hal * hal = NULL;

    if (adev->a2dp_hal != NULL) {
        ALOGE("BluetoothAudioPortOut already exist");
        return 0;
    }
    ALOGD("%s: open", __func__);

    hal = new aml_a2dp_hal;
    if (hal == NULL) {
        ALOGE("new BluetoothAudioPortOut fail");
        return -1;
    }
    hal->resample = NULL;
    hal->buff = NULL;
    hal->buffsize = 0;
    if (!hal->a2dphw.SetUp(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP)) {
        ALOGE("BluetoothAudioPortOut setup fail");
        return -1;
    }
    if (!hal->a2dphw.LoadAudioConfig(&hal->config)) {
        ALOGE("LoadAudioConfig fail");
    }
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO)
        hal->a2dphw.ForcePcmStereoToMono(true);

    adev->a2dp_hal = (void*)hal;
    ALOGD("LoadAudioConfig: rate=%d, format=%x, ch=%d",
        hal->config.sample_rate, hal->config.format, hal->config.channel_mask);
    return 0;
}
int a2dp_out_close(struct audio_hw_device* dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;

    if (hal == NULL) {
        ALOGE("a2dp hw already release");
        return -1;
    }
    ALOGD("%s: close", __func__);
    hal->a2dphw.Stop();
    hal->a2dphw.TearDown();
    if (hal->resample)
        delete hal->resample;
    if (hal->buff)
        delete hal->buff;
    delete hal;
    adev->a2dp_hal = NULL;
    adev->a2dp_active = 0;
    return 0;
}

int a2dp_out_resume(struct audio_stream_out* stream) {
    struct aml_stream_out* out = (struct aml_stream_out*)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    std::unique_lock<std::mutex> lock(hal->mutex_);
    BluetoothStreamState state;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    state = hal->a2dphw.GetState();
    ALOGD("%s: state=%d", __func__, (uint8_t)state);
    if (state == BluetoothStreamState::STANDBY) {
        if (hal->a2dphw.Start()) {
            if ((out->flags & AUDIO_OUTPUT_FLAG_PRIMARY) == 0)
                adev->a2dp_active = 1;
            return 0;
        }
    }
    return -1;
}

int a2dp_out_standby(struct audio_stream* stream) {
    struct aml_stream_out* out = (struct aml_stream_out*)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    std::unique_lock<std::mutex> lock(hal->mutex_);
    BluetoothStreamState state;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    state = hal->a2dphw.GetState();
    ALOGD("%s: state=%d", __func__, (uint8_t)state);
    if (state == BluetoothStreamState::STARTED) {
        if (hal->a2dphw.Suspend()) {
            if ((out->flags & AUDIO_OUTPUT_FLAG_PRIMARY) == 0)
                adev->a2dp_active = 0;
            return 0;
        }
    }
    return -1;
}

ssize_t a2dp_out_write(struct audio_stream_out* stream, const void* buffer, size_t bytes) {
    struct aml_stream_out* out = (struct aml_stream_out*)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    std::unique_lock<std::mutex> lock(hal->mutex_);
    BluetoothStreamState state;
    size_t totalWritten = 0;
    int frame_size = 4; //2ch 16bits
    size_t frames = bytes / frame_size;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    state = hal->a2dphw.GetState();
    ALOGD("%s: state=%d", __func__, (uint8_t)state);
    lock.unlock();
    if (state != BluetoothStreamState::STARTED) {
        if (a2dp_out_resume(stream)) {
            usleep(10 * 1000);
            return totalWritten;
        }
    }
    if (out->is_tv_platform == 1) {
        int16_t *tmp_buffer = (int16_t *)buffer;
        int32_t *tmp_buffer_8ch = (int32_t *)buffer;
        frames = bytes/32; // 8ch 32bit
        for (int i=0; i<(int)frames; i++) {
            tmp_buffer[2*i] = (tmp_buffer_8ch[8*i]>>16);
            tmp_buffer[2*i+1] = (tmp_buffer_8ch[8*i+1]>>16);
        }
    }
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
    clock_gettime(CLOCK_MONOTONIC, &ts);

    if (out->hal_rate != hal->config.sample_rate) {
        int out_frames = 0;
        int out_size = frames*hal->config.sample_rate*frame_size/out->hal_rate+32;
        if (hal->resample == NULL) {
            hal->resample = new aml_resample;
            if (hal->resample == NULL) {
                ALOGD("%s: new resample_para error", __func__);
                return bytes;
            }
            hal->resample->input_sr = out->hal_rate;
            hal->resample->output_sr = hal->config.sample_rate;
            hal->resample->channels = 2;
            resampler_init(hal->resample);
        }
        if (hal->buffsize < out_size) {
            if (hal->buff)
                delete[] hal->buff;
            hal->buff = new char[out_size];
            if (hal->buff == NULL) {
                ALOGD("%s: new buff error", __func__);
                return bytes;
            }
            hal->buffsize = out_size;
        }
        out_frames = resample_process(hal->resample, frames, (int16_t*) buffer, (int16_t*) hal->buff);
        if (out_frames == 0) {
            return bytes;
        }
        out_size = out_frames * frame_size;
        frames = out_frames;
        totalWritten = hal->a2dphw.WriteData(hal->buff, out_size);
    } else {
        totalWritten = hal->a2dphw.WriteData(buffer, bytes);
    }
    lock.lock();
    if (totalWritten) {
        hal->last_write_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    } else {
        const int64_t now = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
        const int64_t gap = now - hal->last_write_time;
        int64_t sleep_time = frames * 1000000LL / hal->config.sample_rate - gap;
        if (sleep_time > 0) {
            lock.unlock();
            usleep(sleep_time);
            lock.lock();
        } else {
            sleep_time = 0;
        }
        hal->last_write_time = now + sleep_time;
    }
    return totalWritten;
}

uint32_t a2dp_out_get_latency(const struct audio_stream_out* stream) {
    (void *)stream;
    return 200;
}

int a2dp_out_set_parameters (struct audio_stream *stream, const char *kvpairs) {
    struct aml_stream_out* out = (struct aml_stream_out*)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    std::unordered_map<std::string, std::string> params = ParseAudioParams(kvpairs);
    if (params.empty())
        return 0;

    if (params.find("A2dpSuspended") != params.end()) {
        if (params["A2dpSuspended"] == "true") {
            if (hal->a2dphw.GetState() != BluetoothStreamState::DISABLED)
                hal->a2dphw.Stop();
        } else {
            if (hal->a2dphw.GetState() == BluetoothStreamState::DISABLED)
                hal->a2dphw.SetState(BluetoothStreamState::STANDBY);
        }
    }
    if (params["closing"] == "true") {
        if (hal->a2dphw.GetState() != BluetoothStreamState::DISABLED)
            hal->a2dphw.Stop();
    }
    return 0;
}

