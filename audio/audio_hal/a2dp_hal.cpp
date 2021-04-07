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
#include <cutils/properties.h>
#include <android-base/strings.h>
#include <audio_utils/primitives.h>

#include "a2dp_hal.h"
#include "a2dp_hw.h"
#include "audio_hw.h"
#include "aml_audio_stream.h"
extern "C" {
#include "aml_audio_timer.h"
}

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
    uint64_t mute_time;
    mutable std::mutex mutex_;
    char * buff_conv_format;
    size_t buff_size_conv_format;
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

bool a2dp_wait_status(BluetoothAudioPortOut * a2dphw) {
    BluetoothStreamState state = a2dphw->GetState();
    int retry = 0;
    while (retry < 100) {
        if ((state != BluetoothStreamState::STARTING) && (state != BluetoothStreamState::SUSPENDING)) {
            if (retry > 0)
                ALOGD("%s: wait for %d ms, state=%d", __func__, retry*10, (uint8_t)state);
            return true;
        }
        usleep(10000);
        retry++;
        state = a2dphw->GetState();
    }
    ALOGD("%s: wait for %d ms, state=%d", __func__, retry*10, (uint8_t)state);
    return false;
}

int a2dp_out_open(struct audio_hw_device* dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_a2dp_hal * hal = NULL;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};

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
    hal->buff_conv_format = NULL;
    hal->buff_size_conv_format = 0;
    if (!hal->a2dphw.SetUp(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP)) {
        ALOGE("BluetoothAudioPortOut setup fail");
        return -1;
    }
    if (!hal->a2dphw.LoadAudioConfig(&hal->config)) {
        ALOGE("LoadAudioConfig fail");
    }
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO)
        hal->a2dphw.ForcePcmStereoToMono(true);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    hal->mute_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    hal->mute_time += 1000000LL; // mute for 1s
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
    adev->a2dp_hal = NULL;
    hal->mutex_.lock();
    ALOGD("%s: close", __func__);
    a2dp_wait_status(&hal->a2dphw);
    hal->a2dphw.Stop();
    hal->a2dphw.TearDown();
    if (hal->resample)
        delete hal->resample;
    if (hal->buff)
        delete hal->buff;
    if (hal->buff_conv_format)
        delete [] hal->buff_conv_format;
    hal->mutex_.unlock();
    delete hal;
    adev->a2dp_active = 0;
    return 0;
}

int a2dp_out_resume(struct audio_stream_out* stream) {
    struct aml_stream_out* out = (struct aml_stream_out*)stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    std::unique_lock<std::mutex> lock(hal->mutex_);
    state = hal->a2dphw.GetState();
    ALOGD("%s: state=%d", __func__, (uint8_t)state);
    a2dp_wait_status(&hal->a2dphw);
    if (state == BluetoothStreamState::STANDBY) {
        if (hal->a2dphw.Start()) {
            if ((out->flags & (AUDIO_OUTPUT_FLAG_PRIMARY|AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) == 0)
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
    BluetoothStreamState state;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }

    std::unique_lock<std::mutex> lock(hal->mutex_);
    state = hal->a2dphw.GetState();
    ALOGD("%s: state=%d", __func__, (uint8_t)state);
    a2dp_wait_status(&hal->a2dphw);
    if (state == BluetoothStreamState::STARTED) {
        if (hal->a2dphw.Suspend()) {
            if ((out->flags & (AUDIO_OUTPUT_FLAG_PRIMARY|AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) == 0)
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
    BluetoothStreamState state = BluetoothStreamState::UNKNOWN;
    int frame_size = 4; //2ch 16bits
    size_t frames = bytes / frame_size;
    int wr_size = bytes;
    const void * wr_buff = buffer;
    unsigned int rate = out->hal_rate;

    if (hal == NULL) {
        ALOGE("%s: a2dp hw is release", __func__);
        return -1;
    }
    std::unique_lock<std::mutex> lock(hal->mutex_);
    if (out->pause_status)
        return bytes;
    const int64_t cur_write = aml_audio_get_systime();
    state = hal->a2dphw.GetState();
    if (adev->debug_flag)
        ALOGD("%s:%p bytes=%zu, state=%d, format=0x%x, hwsync=%d, continuous=%d",
                __func__, out, bytes, (uint8_t)state, out->hal_internal_format,
                out->hw_sync_mode, adev->continuous_audio_mode);

    if (out->is_tv_platform == 1) {
        int16_t *tmp_buffer = (int16_t *)buffer;
        int32_t *tmp_buffer_8ch = (int32_t *)buffer;
        frames = bytes/32; // 8ch 32bit
        for (int i=0; i<(int)frames; i++) {
            tmp_buffer[2*i] = (tmp_buffer_8ch[8*i]>>16);
            tmp_buffer[2*i+1] = (tmp_buffer_8ch[8*i+1]>>16);
        }
        wr_size = frames * frame_size;
    }

    if (state == BluetoothStreamState::STARTING) {
        const int64_t gap = cur_write - hal->last_write_time;
        int64_t sleep_time = frames * 1000000LL / rate - gap;
        hal->last_write_time = cur_write;
        if (sleep_time > 0) {
            hal->last_write_time += sleep_time;
            lock.unlock();
            usleep(sleep_time);
        }
        return bytes;
    } else if (state == BluetoothStreamState::STARTED) {
        if (adev->audio_patch) {
            int64_t write_delta_time_us = cur_write - hal->last_write_time;
            if (write_delta_time_us > 128000) {
                ALOGD("%s:%d, for DTV/HDMIIN, input may be has gap: %lld", __func__, __LINE__, cur_write - hal->last_write_time);
                lock.unlock();
                a2dp_out_standby(&stream->common);
                return bytes;
            }
            int64_t data_delta_time_us = frames * 1000000LL / rate - write_delta_time_us;
            /* Prevent consuming data too quickly. */
            if (data_delta_time_us > 0) {
                usleep(data_delta_time_us / 2);
            }
        }
    } else {
        lock.unlock();
        struct aml_audio_patch *patch = adev->audio_patch;
        /* 1.We need sleep 64ms when playing Dolby in HDMI_IN, decoder need 2 frames to decoded a frame, need waste of a frame.
         * 2.Look for the sync bytes head in the worst case, need waste of a frame.
         * 3.So, we need delay at least 2 frames(32ms * 2) to accumulate data, to prevent a2dp UNDERFLOW.
         */
        if (patch && patch->input_src == AUDIO_DEVICE_IN_HDMI && is_dolby_format(patch->aformat)) {
            usleep(64000);
        }

        if (a2dp_out_resume(stream)) {
            usleep(8000);
        }
        // a2dp_out_resume maybe cause over 100ms, so set last_write_time after resume,
        // otherwise, the gap woud always over 64ms, and always standby in dtv
        hal->last_write_time = aml_audio_get_systime();
        return 0;
    }
    if (hal->mute_time > 0) {
        if (hal->mute_time > cur_write) {
            memset((void*)buffer, 0, bytes);
        } else {
            hal->mute_time = 0;
        }
    }

    if (rate != hal->config.sample_rate) {
        int out_frames = 0;
        int out_size = frames*hal->config.sample_rate*frame_size/rate+32;
        if ((hal->resample != NULL) && (hal->resample->input_sr != rate)) {
            delete hal->resample;
            hal->resample = NULL;
        }
        if (hal->resample == NULL) {
            hal->resample = new aml_resample;
            if (hal->resample == NULL) {
                ALOGD("%s: new resample_para error", __func__);
                return bytes;
            }
            hal->resample->input_sr = rate;
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
        frames = out_frames;
        wr_buff = hal->buff;
        wr_size = out_frames * frame_size;
    }

    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO) {
        int16_t *tmp_buffer = (int16_t *)wr_buff;
        for (int i=0; i<(int)frames; i++) {
            tmp_buffer[i] = tmp_buffer[2*i];
        }
        wr_size = frames * 2;
    }

    if (hal->config.format == AUDIO_FORMAT_PCM_32_BIT) {
        int out_size = wr_size*2;
        if ((hal->buff_size_conv_format < out_size) || (hal->buff_conv_format == NULL)) {
            if (hal->buff_conv_format)
                delete [] hal->buff_conv_format;
            hal->buff_conv_format = new char[out_size];
            if (hal->buff_conv_format == NULL) {
                ALOGE("realloc hal->buff fail: %d", out_size);
                return bytes;
            }
            hal->buff_size_conv_format = out_size;
        }
        memcpy_to_i32_from_i16((int32_t *)hal->buff_conv_format, (int16_t *)wr_buff, wr_size/sizeof(int16_t));
        wr_buff = hal->buff_conv_format;
        wr_size = out_size;
    } else if (hal->config.format == AUDIO_FORMAT_PCM_24_BIT_PACKED) {
        int out_size = (wr_size*3+1)/2;
        if ((hal->buff_size_conv_format < out_size) || (hal->buff_conv_format == NULL)) {
            if (hal->buff_conv_format)
                delete [] hal->buff_conv_format;
            hal->buff_conv_format = new char[out_size];
            if (hal->buff_conv_format == NULL) {
                ALOGE("realloc hal->buff fail: %d", out_size);
                return bytes;
            }
            hal->buff_size_conv_format = out_size;
        }
        memcpy_to_p24_from_i16((uint8_t *)hal->buff_conv_format, (int16_t *)wr_buff, wr_size/sizeof(int16_t));
        wr_buff = hal->buff_conv_format;
        wr_size = out_size;
    }
    if (adev->patch_src == SRC_DTV && adev->parental_control_av_mute) {
        memset((void*)wr_buff,0x0,wr_size);
    }

    if (property_get_int32("vendor.media.audiohal.a2dpdump", 0) > 0) {
        FILE *fp = fopen("/data/audio/a2dp.pcm", "a+");
        if (fp) {
            int flen = fwrite((char *)wr_buff, 1, wr_size, fp);
            fclose(fp);
        }
    }

    int writed_bytes = 0;
    int sent = 0;
    while (writed_bytes < wr_size && adev->a2dp_hal) {
        sent = hal->a2dphw.WriteData((char *)wr_buff+writed_bytes, wr_size-writed_bytes);
        writed_bytes += sent;
    }
    hal->last_write_time = cur_write;
    #if 0
    if (totalWritten) {
        hal->last_write_time = cur_write;
    } else {
        const int64_t gap = cur_write - hal->last_write_time;
        int64_t sleep_time = frames * 1000000LL / hal->config.sample_rate - gap;
        hal->last_write_time = cur_write;
        if (sleep_time > 0) {
            hal->last_write_time += sleep_time;
            lock.unlock();
            usleep(sleep_time);
        }
    }
    #endif
    return bytes;
}

uint32_t a2dp_out_get_latency(const struct audio_stream_out* stream) {
    (void *)stream;
    return property_get_int32("vendor.media.a2dp.latency", 200);
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

