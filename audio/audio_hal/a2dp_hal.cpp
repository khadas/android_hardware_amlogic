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

#include "BluetoothAudioSession.h"

#include "a2dp_hal.h"
#include "a2dp_hw.h"
#include "aml_audio_resampler.h"


extern "C" {
#include "audio_hw_utils.h"
#include "aml_audio_stream.h"
#include "aml_audio_timer.h"
}

using ::android::bluetooth::audio::BluetoothAudioPortOut;
using ::android::bluetooth::audio::BluetoothAudioSession;
using ::android::bluetooth::audio::BluetoothAudioSessionInstance;
using ::android::hardware::bluetooth::audio::V2_0::SessionType;

#define DEBUG_LOG_MASK_A2DP                         (0x1000)
#define A2DP_RING_BUFFER_DELAY_TIME_MS              (64)
#define A2DP_WAIT_STATE_DELAY_TIME_US               (8000)
#define DEFAULT_A2DP_LATENCY_NS                     (100 * NSEC_PER_MSEC) // Default delay to use when BT device does not report a delay
#define A2DP_STATIC_DELAY_MS                        (100) // Additional device-specific delay
#define AUDIO_HAL_FIXED_CFG_CHANNEL                 (AUDIO_CHANNEL_OUT_STEREO)
#define AUDIO_HAL_FIXED_CFG_FORMAT                  (AUDIO_FORMAT_PCM_16_BIT)


struct aml_a2dp_hal {
    BluetoothAudioPortOut a2dphw;
    audio_config config;
    aml_audio_resample_t *resample;
    int64_t last_write_time;
    uint64_t mute_time;
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
            //AM_LOGD("Invalid audio parameter: ", segment.char());
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
                AM_LOGD("a2dp wait for %d ms, state:%s",
                    retry * A2DP_WAIT_STATE_DELAY_TIME_US / 1000, a2dpStatus2String(state));
            }
            return true;
        }
        usleep(A2DP_WAIT_STATE_DELAY_TIME_US);
        retry++;
        state = a2dphw->GetState();
    }
    AM_LOGW("a2dp wait timeout for %d ms, state:%s",
        retry * A2DP_WAIT_STATE_DELAY_TIME_US / 1000, a2dpStatus2String(state));
    return false;
}

static void dump_a2dp_output_data(aml_a2dp_hal *hal, const void *buffer, uint32_t size)
{
    if (getprop_bool("vendor.media.audiohal.a2dpdump")) {
        char acFilePathStr[ENUM_TYPE_STR_MAX_LEN];
        size_t out_per_sample_byte = audio_bytes_per_sample(hal->config.format);
        size_t out_channel_byte = audio_channel_count_from_out_mask(hal->config.channel_mask);
        sprintf(acFilePathStr, "/data/audio/a2dp_%0.1fK_%dC_%dB.pcm", hal->config.sample_rate/1000.0, out_channel_byte, out_per_sample_byte);
        aml_audio_dump_audio_bitstreams(acFilePathStr, buffer, size);
    }
}

int a2dp_out_open(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = NULL;
    struct timespec ts = {.tv_sec = 0, .tv_nsec = 0};
    pthread_mutex_lock(&adev->a2dp_lock);

    if (adev->a2dp_hal != NULL) {
        AM_LOGW("already open");
        pthread_mutex_unlock(&adev->a2dp_lock);
        return 0;
    }
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        AM_LOGD("");
    }

    hal = new aml_a2dp_hal;
    if (hal == NULL) {
        AM_LOGE("new BluetoothAudioPortOut fail");
        pthread_mutex_unlock(&adev->a2dp_lock);
        return -1;
    }
    hal->resample = NULL;
    hal->buff_conv_format = NULL;
    hal->buff_size_conv_format = 0;
    hal->state = BluetoothStreamState::UNKNOWN;
    if (!hal->a2dphw.SetUp(AUDIO_DEVICE_OUT_BLUETOOTH_A2DP)) {
        AM_LOGE("BluetoothAudioPortOut setup fail");
        pthread_mutex_unlock(&adev->a2dp_lock);
        return -1;
    }
    if (!hal->a2dphw.LoadAudioConfig(&hal->config)) {
        AM_LOGE("LoadAudioConfig fail");
    }
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO)
        hal->a2dphw.ForcePcmStereoToMono(true);
    clock_gettime(CLOCK_MONOTONIC, &ts);
    hal->mute_time = ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
    hal->mute_time += 1000000LL; // mute for 1s
    adev->a2dp_hal = (void*)hal;
    pthread_mutex_unlock(&adev->a2dp_lock);
    AM_LOGI("Rx param rate:%d, bytes_per_sample:%d, ch:%d", hal->config.sample_rate,
        audio_bytes_per_sample(hal->config.format), audio_channel_count_from_out_mask(hal->config.channel_mask));
    return 0;
}
int a2dp_out_close(struct aml_audio_device *adev) {
    pthread_mutex_lock(&adev->a2dp_lock);
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;

    R_CHECK_POINTER_LEGAL(-1, hal, "a2dp hw is released");
    adev->a2dp_hal = NULL;
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        AM_LOGD("");
    }
    a2dp_wait_status(&hal->a2dphw);
    hal->a2dphw.Stop();
    hal->a2dphw.TearDown();
    if (hal->resample) {
        aml_audio_resample_close(hal->resample);
        hal->resample = NULL;
    }
    if (hal->buff_conv_format)
        aml_audio_free(hal->buff_conv_format);
    pthread_mutex_unlock(&adev->a2dp_lock);
    delete hal;
    return 0;
}

static int a2dp_out_resume_l(aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    a2dp_wait_status(&hal->a2dphw);
    state = hal->a2dphw.GetState();
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        AM_LOGD("a2dp cur status:%s", a2dpStatus2String(state));
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
    pthread_mutex_lock(&adev->a2dp_lock);
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    R_CHECK_POINTER_LEGAL(-1, hal, "a2dp hw is released");
    int32_t ret = a2dp_out_resume_l(adev);
    pthread_mutex_unlock(&adev->a2dp_lock);
    return ret;
}

static int a2dp_out_standby_l(struct aml_audio_device *adev) {
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    BluetoothStreamState state;
    a2dp_wait_status(&hal->a2dphw);
    state = hal->a2dphw.GetState();
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        AM_LOGD("a2dp cur status:%s", a2dpStatus2String(state));
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
    pthread_mutex_lock(&adev->a2dp_lock);
    struct aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    R_CHECK_POINTER_LEGAL(-1, hal, "a2dp hw is released");
    int32_t ret = a2dp_out_standby_l(adev);
    pthread_mutex_unlock(&adev->a2dp_lock);
    return ret;
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
    int64_t data_delta_time_us = cur_frames * USEC_PER_SEC / config->sample_rate - write_delta_time_us;
    hal->last_write_time = cur_write_time_us;
    if (hal->state != cur_state) {
        AM_LOGI("a2dp state changed: %s -> %s",  a2dpStatus2String(hal->state), a2dpStatus2String(cur_state));
    }
    if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
        AM_LOGD("cur_state:%s, frames:%d, gap:%lld ms", a2dpStatus2String(cur_state), cur_frames, write_delta_time_us / 1000);
    }

    if (cur_state == BluetoothStreamState::STARTING) {
        if (data_delta_time_us > 0) {
            if (adev->debug_flag & DEBUG_LOG_MASK_A2DP) {
                AM_LOGD("write too fast, need sleep:%lld ms", data_delta_time_us / 1000);
            }
            usleep(data_delta_time_us);
        }
    } else if (cur_state == BluetoothStreamState::STARTED) {
         if (adev->audio_patch) {
            /* tv_mute for atv switch channel */
            if (write_delta_time_us > 128000 || adev->tv_mute) {
                AM_LOGI("tv_mute:%d, gap:%lld ms, start standby", adev->tv_mute, write_delta_time_us / 1000);
                a2dp_out_standby_l(adev);
            } else {
                /* The first startup needs to be filled. BT stack is 40ms buffer.*/
                if (first_start) {
                    first_start = false;
                    size_t empty_data_40ms_bytes = 40 * hal->config.sample_rate * 4 / 1000;
                    char *pTempBuffer = (char *)aml_audio_calloc(1, empty_data_40ms_bytes);
                    hal->a2dphw.WriteData(pTempBuffer, empty_data_40ms_bytes);
                    aml_audio_free(pTempBuffer);
                    AM_LOGI("Insert empty data ensures that underrun does not occur.");
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
        AM_LOGW("not support param, channel_cnt:%d, format:%#x",
            audio_channel_count_from_out_mask(config->channel_mask), config->format);
        return -1;
    }

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

static ssize_t a2dp_data_resample_process(aml_a2dp_hal *hal, audio_config_base_t *input_cfg,
    const void *buffer, size_t in_frames, const void **output_buffer) {
    int out_frames = in_frames;
    *output_buffer = buffer;
    if (input_cfg->sample_rate != hal->config.sample_rate) {
        size_t in_frame_size = audio_channel_count_from_out_mask(AUDIO_HAL_FIXED_CFG_CHANNEL) *
            audio_bytes_per_sample(AUDIO_HAL_FIXED_CFG_FORMAT);
        /* The resampled frames may be large than the theoretical value.
         * So, there is an extra 32 bytes allocated to prevent overflows.
         */
        int resample_out_buffer_size = in_frames * hal->config.sample_rate * in_frame_size / input_cfg->sample_rate + 32;
        if (hal->resample == NULL || hal->resample->resample_config.input_sr != input_cfg->sample_rate) {
            audio_resample_config_t resample_cfg;
            resample_cfg.aformat   = AUDIO_HAL_FIXED_CFG_FORMAT;
            resample_cfg.channels  = audio_channel_count_from_out_mask(AUDIO_HAL_FIXED_CFG_CHANNEL);
            resample_cfg.input_sr  = input_cfg->sample_rate;
            resample_cfg.output_sr = hal->config.sample_rate;
            if (hal->resample == NULL) {
                AM_LOGI("resample init format:%#x, ch:%d", resample_cfg.aformat, resample_cfg.channels);
                int ret = aml_audio_resample_init(&hal->resample, AML_AUDIO_SIMPLE_RESAMPLE, &resample_cfg);
                R_CHECK_RET(ret, "Resampler is failed initialization !!!");
            } else {
                AM_LOGI("resample reconfig, input_sr changed %d -> %d", hal->resample->resample_config.input_sr, input_cfg->sample_rate);
                memcpy(&hal->resample->resample_config, &resample_cfg, sizeof(audio_resample_config_t));
                aml_audio_resample_reset(hal->resample);
            }
        }
        aml_audio_resample_process(hal->resample, (void *)buffer, in_frames * in_frame_size);
        out_frames = hal->resample->resample_size / in_frame_size;
        *output_buffer = hal->resample->resample_buffer;
    }
    return out_frames;
}

static ssize_t a2dp_out_data_process(aml_a2dp_hal *hal, audio_config_base_t *config __unused,
    const void *buffer, size_t in_frames, const void **output_buffer) {
    size_t in_frame_size = audio_channel_count_from_out_mask(AUDIO_HAL_FIXED_CFG_CHANNEL) *
        audio_bytes_per_sample(AUDIO_HAL_FIXED_CFG_FORMAT);
    ssize_t out_size = in_frames * in_frame_size;
    if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_MONO) {
        int16_t *tmp_buffer = (int16_t *)buffer;
        for (int i=0; i<in_frames; i++) {
            tmp_buffer[i] = tmp_buffer[2 * i];
        }
        out_size = in_frames * 1 * audio_bytes_per_sample(AUDIO_HAL_FIXED_CFG_FORMAT);
    } else if (hal->config.channel_mask == AUDIO_CHANNEL_OUT_STEREO) {
        /* 2channel do nothing*/
    } else {
        AM_LOGW("not support a2dp output channel_cnt:%#x",
            audio_channel_count_from_out_mask(AUDIO_HAL_FIXED_CFG_CHANNEL));
        return 0;
    }

    size_t out_per_sample_byte = audio_bytes_per_sample(hal->config.format);
    size_t out_channel_byte = audio_channel_count_from_out_mask(hal->config.channel_mask);
    out_size = out_per_sample_byte * out_channel_byte * in_frames;
    if (hal->config.format != AUDIO_FORMAT_PCM_16_BIT) {
        aml_audio_check_and_realloc((void **)&hal->buff_conv_format, &hal->buff_size_conv_format, out_size);
        R_CHECK_RET(0, "realloc buff_conv_format size:%d fail", out_size);
        if (hal->config.format == AUDIO_FORMAT_PCM_32_BIT) {
            memcpy_to_i32_from_i16((int32_t *)hal->buff_conv_format, (int16_t *)buffer, in_frames * out_channel_byte);
        } else if (hal->config.format == AUDIO_FORMAT_PCM_24_BIT_PACKED) {
            memcpy_to_p24_from_i16((uint8_t *)hal->buff_conv_format, (int16_t *)buffer, in_frames * out_channel_byte);
        } else {
            AM_LOGW("not support a2dp output format:%#x", hal->config.format);
            return 0;
        }
        *output_buffer = hal->buff_conv_format;
    }
    return out_size;
}

static ssize_t a2dp_out_write_l(struct aml_audio_device *adev, audio_config_base_t *config, const void* buffer, size_t bytes) {
    pthread_mutex_lock(&adev->a2dp_lock);
    aml_a2dp_hal *hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    int wr_size = 0;
    const void *wr_buff = NULL;
    size_t cur_frames = 0;
    size_t resample_frames = 0;
    uint32_t bytes_written = 0;
    size_t sent = 0;

    if (adev->a2dp_hal == NULL) {
        if (adev->debug_flag) {
            AM_LOGW("a2dp_hal is null pointer");
        }
        goto exit;
    }

    cur_frames = a2dp_in_data_process(hal, config, buffer, bytes);
    if (cur_frames < 0) {
        goto exit;
    }

    if (!a2dp_state_process(adev, config, cur_frames)) {
        goto exit;
    }

    resample_frames = a2dp_data_resample_process(hal, config, buffer, cur_frames, &wr_buff);
    if (resample_frames < 0) {
        goto exit;
    }

    wr_size = a2dp_out_data_process(hal, config, wr_buff, resample_frames, &wr_buff);
    if (wr_size == 0) {
        goto exit;
    }

    if (adev->patch_src == SRC_DTV && adev->parental_control_av_mute) {
        memset((void*)wr_buff, 0, wr_size);
    }
    dump_a2dp_output_data(hal, wr_buff, wr_size);
    while (bytes_written < wr_size) {
        sent = hal->a2dphw.WriteData((char *)wr_buff + bytes_written, wr_size - bytes_written);
        AM_LOGV("need_write:%d, actual sent:%d, wr_size:%d", (wr_size - bytes_written), sent, wr_size);
        bytes_written += sent;
    }

exit:
    pthread_mutex_unlock(&adev->a2dp_lock);
    return bytes;
}

ssize_t a2dp_out_write(struct aml_audio_device *adev, audio_config_base_t *config, const void* buffer, size_t bytes) {
    size_t in_frame_size = audio_channel_count_from_out_mask(config->channel_mask) * audio_bytes_per_sample(config->format);
    uint32_t one_ms_data = in_frame_size * config->sample_rate / 1000;
    uint32_t date_len_ms = bytes / one_ms_data;
    const uint32_t period_time_ms = 32;
    const uint32_t period_time_size = one_ms_data * period_time_ms;

    if (bytes == 0) {
        AM_LOGW("bytes is 0");
        return -1;
    }
    R_CHECK_POINTER_LEGAL(-1, config, "");
    R_CHECK_POINTER_LEGAL(-1, buffer, "");

    uint32_t written_size = 0;
    while (bytes > written_size) {
        uint32_t remain_size = bytes - written_size;
        size_t sent = remain_size;
        if (remain_size > period_time_ms * one_ms_data) {
            sent = period_time_size;
        }
        a2dp_out_write_l(adev, config, (char *)buffer + written_size, sent);
        AM_LOGV("written_size:%d, remain_size:%d, sent:%d", written_size, remain_size, sent);
        written_size += sent;
    }
    return written_size;
}

uint32_t a2dp_out_get_latency(struct aml_audio_device *adev __unused) {
    uint64_t remote_delay_report_ns;
    std::shared_ptr<BluetoothAudioSession> session_ptr =
        BluetoothAudioSessionInstance::GetSessionInstance(SessionType::A2DP_SOFTWARE_ENCODING_DATAPATH);
    bool success = session_ptr->GetPresentationPosition(&remote_delay_report_ns, nullptr, nullptr);
    if (!success || remote_delay_report_ns == 0) {
        remote_delay_report_ns = DEFAULT_A2DP_LATENCY_NS;
    }
    return static_cast<uint32_t>(remote_delay_report_ns / NSEC_PER_MSEC + A2DP_STATIC_DELAY_MS);
}

int a2dp_out_set_parameters(struct aml_audio_device *adev, const char *kvpairs) {
    struct aml_a2dp_hal * hal = (struct aml_a2dp_hal *)adev->a2dp_hal;
    R_CHECK_POINTER_LEGAL(-1, hal, "a2dp hw is released");

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
        aml_audio_resample_t *resample = hal->resample;
        if (resample) {
            audio_resample_config_t *config = &resample->resample_config;
            dprintf(fd, "-[AML_HAL] resample in_sr     : %10d     | out_sr    :%10d\n", config->input_sr, config->output_sr);
            dprintf(fd, "-[AML_HAL] resample ch        : %10d     | type      :%10d\n", config->channels, resample->resample_type);
        }
    }
    return 0;
}


