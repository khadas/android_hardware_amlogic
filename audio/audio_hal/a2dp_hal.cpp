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
#include "aml_audio_resampler.h"
#include "aml_audio_stream.h"
#include "audio_hw_utils.h"
extern "C" {
#include "aml_audio_timer.h"
}

using ::android::bluetooth::audio::BluetoothAudioPortOut;
#define DEBUG_LOG_MASK_A2DP                         (0x1000)
#define A2DP_RING_BUFFER_DELAY_TIME_MS              (64)
#define A2DP_WAIT_STATE_DELAY_TIME_US               (8000)

struct aml_a2dp_hal {
    BluetoothAudioPortOut a2dphw;
    audio_config config;
    struct resample_para *resample;
    char* buff;
    size_t buffsize;
    int64_t last_write_time;
    uint64_t mute_time;
    mutable std::mutex mutex_;
    char * buff_conv_format;
    size_t buff_size_conv_format;
    BluetoothStreamState state;
};

const char* a2dpStatus2String(BluetoothStreamState type)
{
    ENUM_TYPE_TO_STR_START("BluetoothStreamState::");
    ENUM_TYPE_TO_STR(BluetoothStreamState::DISABLED)
    ENUM_TYPE_TO_STR(BluetoothStreamState::STANDBY)
    ENUM_TYPE_TO_STR(BluetoothStreamState::STARTING)
    ENUM_TYPE_TO_STR(BluetoothStreamState::STARTED)
    ENUM_TYPE_TO_STR(BluetoothStreamState::SUSPENDING)
    ENUM_TYPE_TO_STR(BluetoothStreamState::UNKNOWN)
    ENUM_TYPE_TO_STR_END
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

static bool a2dp_wait_status(BluetoothAudioPortOut *a2dphw) {
    BluetoothStreamState state = a2dphw->GetState();
    int retry = 0;
    while (retry < 100) {
        if ((state != BluetoothStreamState::STARTING) && (state != BluetoothStreamState::SUSPENDING)) {
            if (retry > 0) {
                ALOGD("[%s:%d] a2dp wait for %d ms, state:%s", __func__, __LINE__,
                    retry * A2DP_WAIT_STATE_DELAY_TIME_US / 1000, a2dpStatus2String(state));
            }
            return true;
        }
        usleep(A2DP_WAIT_STATE_DELAY_TIME_US);
        retry++;
        state = a2dphw->GetState();
    }
    ALOGW("[%s:%d] a2dp wait timeout for %d ms, state:%s", __func__, __LINE__,
        retry * A2DP_WAIT_STATE_DELAY_TIME_US / 1000, a2dpStatus2String(state));
    return false;
}

int a2dp_out_open(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = NULL;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};

    if (adev->a2dp_hal != NULL) {
        ALOGW("[%s:%d] already open", __func__, __LINE__);
        return 0;
    }
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        ALOGD("[%s:%d]", __func__, __LINE__);
    }

    hal = new aml_a2dp_hal;
    if (hal == NULL) {
        ALOGE("[%s:%d] new BluetoothAudioPortOut fail", __func__, __LINE__);
        return -1;
    }
    hal->resample = NULL;
    hal->buff = NULL;
    hal->buffsize = 0;
    hal->buff_conv_format = NULL;
    hal->buff_size_conv_format = 0;
    hal->state = BluetoothStreamState::UNKNOWN;
    if (!hal->a2dphw.SetUp(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP)) {
        ALOGE("[%s:%d] BluetoothAudioPortOut setup fail", __func__, __LINE__);
        return -1;
    }
    if (!hal->a2dphw.LoadAudioConfig(&hal->config)) {
        ALOGE("[%s:%d] LoadAudioConfig fail", __func__, __LINE__);
    }
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO)
        hal->a2dphw.ForcePcmStereoToMono(true);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    hal->mute_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    hal->mute_time += 1000000LL; // mute for 1s
    adev->a2dp_hal = (void*)hal;
    ALOGI("[%s:%d] Rx param rate:%d, bytes_per_sample:%d, ch:%d", __func__, __LINE__, hal->config.sample_rate,
        audio_bytes_per_sample(hal->config.format), audio_channel_count_from_out_mask(hal->config.channel_mask));
    return 0;
}
int a2dp_out_close(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;

    if (hal == NULL) {
        ALOGW("[%s:%d] a2dp hw already release", __func__, __LINE__);
        return -1;
    }
    adev->a2dp_hal = NULL;
    hal->mutex_.lock();
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        ALOGD("[%s:%d]", __func__, __LINE__);
    }
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
    return 0;
}

static int a2dp_out_resume_l(aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    a2dp_wait_status(&hal->a2dphw);
    state = hal->a2dphw.GetState();
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        ALOGD("[%s:%d] a2dp cur status:%s", __func__, __LINE__, a2dpStatus2String(state));
    }
    if (state == BluetoothStreamState::STARTED) {
        return 0;
    } else if (state == BluetoothStreamState::STANDBY) {
        if (hal->a2dphw.Start()) {
            return 0;
        }
    }
    return -1;
}

int a2dp_out_resume(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    std::unique_lock<std::mutex> lock(hal->mutex_);
    if (hal == NULL) {
        ALOGW("[%s:%d] a2dp hw is released", __func__, __LINE__);
        return -1;
    }
    return a2dp_out_resume_l(adev);
}

static int a2dp_out_standby_l(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    a2dp_wait_status(&hal->a2dphw);
    state = hal->a2dphw.GetState();
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        ALOGD("[%s:%d] a2dp cur status:%s", __func__, __LINE__, a2dpStatus2String(state));
    }
    if (state == BluetoothStreamState::STANDBY) {
        return 0;
    } else if (state == BluetoothStreamState::STARTED) {
        if (hal->a2dphw.Suspend()) {
            return 0;
        }
    }
    return -1;
}

int a2dp_out_standby(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    std::unique_lock<std::mutex> lock(hal->mutex_);
    if (hal == NULL) {
        ALOGW("[%s:%d] a2dp hw is released", __func__, __LINE__);
        return -1;
    }
    return a2dp_out_standby_l(adev);
}

static bool a2dp_state_process(struct aml_audio_device *adev, audio_config_base_t *config, size_t cur_frames) {
    static bool             first_start = true;
    aml_a2dp_hal            *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState    cur_state = hal->a2dphw.GetState();
    const int64_t           cur_write_time_us = aml_audio_get_systime();
    bool                    prepared = false;

    if (adev->need_reset_a2dp) {
        adev->need_reset_a2dp = false;
        a2dp_out_standby_l(adev);
        return false;
    }

    const int64_t write_delta_time_us = cur_write_time_us - hal->last_write_time;
    int64_t data_delta_time_us = cur_frames * 1000000LL / config->sample_rate - write_delta_time_us;
    hal->last_write_time = cur_write_time_us;
    if (hal->state != cur_state) {
        ALOGI("[%s:%d] a2dp state changed: %s -> %s", __func__, __LINE__,
            a2dpStatus2String(hal->state), a2dpStatus2String(cur_state));
    }
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        ALOGD("[%s:%d] frames:%d, gap:%lld ms", __func__, __LINE__, cur_frames, write_delta_time_us / 1000);
    }

    if (cur_state == BluetoothStreamState::STARTING) {
        if (data_delta_time_us > 0) {
            if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
                ALOGD("[%s:%d] write too fast, need sleep:%lld ms", __func__, __LINE__, data_delta_time_us / 1000);
            }
            usleep(data_delta_time_us);
        }
    } else if (cur_state == BluetoothStreamState::STARTED) {
         if (adev->audio_patch) {
            /* tv_mute for atv switch channel */
            if (write_delta_time_us > 128000 || adev->tv_mute) {
                ALOGI("[%s:%d] tv_mute:%d, gap:%lld ms, start standby", __func__, __LINE__, adev->tv_mute, write_delta_time_us / 1000);
                a2dp_out_standby_l(adev);
            } else {
                /* The first startup needs to be filled. BT stack is 40ms buffer.*/
                if (first_start) {
                    first_start = false;
                    size_t empty_data_40ms_bytes = 40 * hal->config.sample_rate * 4 / 1000;
                    char *pTempBuffer = (char *)aml_audio_calloc(1, empty_data_40ms_bytes);
                    hal->a2dphw.WriteData(pTempBuffer, empty_data_40ms_bytes);
                }
                prepared = true;
            }
        } else {
            prepared = true;
        }
    } else {
        struct aml_audio_patch *patch = adev->audio_patch;
        if (!(adev->tv_mute && patch)) {
            if (a2dp_out_resume_l(adev)) {
                usleep(8000);
            }
            first_start = true;
        }
        // a2dp_out_resume maybe cause over 100ms, so set last_write_time after resume,
        // otherwise, the gap woud always over 64ms, and always standby in dtv
        hal->last_write_time = aml_audio_get_systime();
    }
    hal->state = cur_state;
    return prepared;
}

static ssize_t a2dp_in_data_process(aml_a2dp_hal *hal, audio_config_base_t *config, const void *buffer, size_t bytes) {
    size_t frames = 0;
    if (config->channel_mask == AUDIO_CHANNEL_OUT_7POINT1 && config->format == AUDIO_FORMAT_PCM_32_BIT) {
        int16_t *tmp_buffer = (int16_t *)buffer;
        int32_t *tmp_buffer_8ch = (int32_t *)buffer;
        frames = bytes / (4 * 8);
        for (int i=0; i<frames; i++) {
            tmp_buffer[2 * i]       = (tmp_buffer_8ch[8 *  i] >> 16);
            tmp_buffer[2 * i + 1]   = (tmp_buffer_8ch[8 * i + 1] >> 16);
        }
    } else if (config->channel_mask == AUDIO_CHANNEL_OUT_STEREO && config->format == AUDIO_FORMAT_PCM_16_BIT) {
        frames = bytes / (2 * 2);
    } else {
        ALOGW("[%s:%d] not support param, channel_cnt:%d, format:%#x", __func__, __LINE__,
            audio_channel_count_from_out_mask(config->channel_mask), config->format);
        return -1;
    }
    config->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    config->format = AUDIO_FORMAT_PCM_16_BIT;

    const int64_t cur_write_time_us = aml_audio_get_systime();
    if (hal->mute_time > 0) {
        if (hal->mute_time > cur_write_time_us) {
            memset((void*)buffer, 0, bytes);
        } else {
            hal->mute_time = 0;
        }
    }
    return frames;
}

static ssize_t a2dp_data_resample_process(aml_a2dp_hal *hal, audio_config_base_t *config,
    const void *buffer, size_t in_frames, const void **output_buffer) {
    int out_frames = in_frames;
    if (config->sample_rate != hal->config.sample_rate) {
        size_t in_frame_size = audio_channel_count_from_out_mask(config->channel_mask) * audio_bytes_per_sample(config->format);
        /* The resampled frames may be large than the theoretical value.
         * So, there is an extra 32 bytes allocated to prevent overflows.
         */
        int resample_out_buffer_size = in_frames * hal->config.sample_rate * in_frame_size / config->sample_rate + 32;
        if ((hal->resample != NULL) && (hal->resample->input_sr != config->sample_rate)) {
            delete hal->resample;
            hal->resample = NULL;
        }
        if (hal->resample == NULL) {
            hal->resample = new struct resample_para;
            if (hal->resample == NULL) {
                ALOGW("[%s:%d] new resample_para error", __func__, __LINE__);
                return -1;
            }
            hal->resample->input_sr = config->sample_rate;
            hal->resample->output_sr = hal->config.sample_rate;
            hal->resample->channels = 2;
            resampler_init(hal->resample);
        }
        if (hal->buffsize < resample_out_buffer_size) {
            if (hal->buff)
                delete[] hal->buff;
            hal->buff = new char[resample_out_buffer_size];
            if (hal->buff == NULL) {
                ALOGW("[%s:%d new buff error, size:%d", __func__, __LINE__, resample_out_buffer_size);
                return -1;
            }
            hal->buffsize = resample_out_buffer_size;
        }
        out_frames = resample_process(hal->resample, in_frames, (int16_t*) buffer, (int16_t*) hal->buff);
        *output_buffer = hal->buff;
    }
    return out_frames;
}

static ssize_t a2dp_out_data_process(aml_a2dp_hal *hal, audio_config_base_t *config,
    const void *buffer, size_t in_frames, const void **output_buffer) {
    size_t in_frame_size = audio_channel_count_from_out_mask(config->channel_mask) * audio_bytes_per_sample(config->format);
    ssize_t out_size = in_frames * in_frame_size;
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO) {
        int16_t *tmp_buffer = (int16_t *)buffer;
        for (int i=0; i<in_frames; i++) {
            tmp_buffer[i] = tmp_buffer[2 * i];
        }
        out_size = in_frames * 1 * audio_bytes_per_sample(config->format);
    } else if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_STEREO) {
        /* 2channel do nothing*/
    } else {
        ALOGW("[%s:%d] not support a2dp output channel_cnt:%#x", __func__, __LINE__,
            audio_channel_count_from_out_mask(config->channel_mask));
        return 0;
    }

    size_t out_per_sample_byte = audio_bytes_per_sample(hal->config.format);
    out_size = (out_size * out_per_sample_byte + 1) / 2;
    if (hal->config.format != AUDIO_FORMAT_PCM_16_BIT) {
        if ((hal->buff_size_conv_format < out_size) || (hal->buff_conv_format == NULL)) {
            if (hal->buff_conv_format)
                delete [] hal->buff_conv_format;
            hal->buff_conv_format = new char[out_size];
            if (hal->buff_conv_format == NULL) {
                ALOGE("[%s:%d] realloc hal->buff fail, out_size:%d", __func__, __LINE__, out_size);
                return 0;
            }
            hal->buff_size_conv_format = out_size;
        }
        if (hal->config.format == AUDIO_FORMAT_PCM_32_BIT) {
            memcpy_to_i32_from_i16((int32_t *)hal->buff_conv_format, (int16_t *)buffer, in_frames);
        } else if (hal->config.format == AUDIO_FORMAT_PCM_24_BIT_PACKED) {
            memcpy_to_p24_from_i16((uint8_t *)hal->buff_conv_format, (int16_t *)buffer, in_frames);
        } else {
            ALOGW("[%s:%d] not support a2dp output format:%#x", __func__, __LINE__, hal->config.format);
            return 0;
        }
        *output_buffer = hal->buff_conv_format;
    }
    return out_size;
}

ssize_t a2dp_out_write(struct aml_audio_device *adev, audio_config_base_t *config, const void* buffer, size_t bytes) {
    aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    int wr_size = 0;
    const void *wr_buff = NULL;
    ssize_t cur_frames = 0;

    if (hal == NULL) {
        ALOGW("[%s:%d] a2dp hw is released", __func__, __LINE__);
        return -1;
    }

    std::unique_lock<std::mutex> lock(hal->mutex_);
    cur_frames = a2dp_in_data_process(hal, config, buffer, bytes);
    if (cur_frames < 0) {
        return bytes;
    }

    bool prepared = a2dp_state_process(adev, config, cur_frames);
    if (!prepared) {
        return bytes;
    }

    cur_frames = a2dp_data_resample_process(hal, config, buffer, cur_frames, &wr_buff);
    if (cur_frames < 0) {
        return bytes;
    }

    wr_size = a2dp_out_data_process(hal, config, wr_buff, cur_frames, &wr_buff);
    if (wr_size == 0) {
        return bytes;
    }

    if (adev->patch_src == SRC_DTV && adev->parental_control_av_mute) {
        memset((void*)wr_buff, 0, wr_size);
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
        sent = hal->a2dphw.WriteData((char *)wr_buff + writed_bytes, wr_size - writed_bytes);
        ALOGV("[%s:%d] need_write:%d, actual sent:%d, wr_size:%d", __func__, __LINE__, (wr_size - writed_bytes), sent, wr_size);
        writed_bytes += sent;
    }
    return bytes;
}

uint32_t a2dp_out_get_latency(struct aml_audio_device *adev) {
    (void *)adev;
    return property_get_int32("vendor.media.a2dp.latency", 200);
}

int a2dp_out_set_parameters(struct aml_audio_device *adev, const char *kvpairs) {
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

int a2dp_hal_dump(struct aml_audio_device *adev, int fd) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    if (hal) {
        dprintf(fd, "-------------[AM_HAL][A2DP]-------------\n");
        dprintf(fd, "-[AML_HAL]      out_rate      : %10d     | out_ch    :%10d\n", hal->config.sample_rate, audio_channel_count_from_out_mask(hal->config.channel_mask));
        dprintf(fd, "-[AML_HAL]      out_format    : %#10x     | cur_state :%10s\n", hal->config.format, a2dpStatus2String(hal->a2dphw.GetState()));
        struct resample_para *resample = hal->resample;
        if (resample) {
            dprintf(fd, "-[AML_HAL] resample in_sr     : %10d     | out_sr    :%10d\n", resample->input_sr, resample->output_sr);
        }
    }
    return 0;
}


