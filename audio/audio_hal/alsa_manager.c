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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>
#include <inttypes.h>
#include <cutils/properties.h>

#include "audio_hw.h"
#include "alsa_manager.h"
#include "audio_hw_utils.h"
#include "alsa_device_parser.h"
#include "dolby_lib_api.h"
#include "aml_audio_stream.h"
#include "alsa_config_parameters.h"
#include "audio_hw_dtv.h"
#include "aml_audio_timer.h"


#define AML_ZERO_ADD_MIN_SIZE 1024

#define AUDIO_EAC3_FRAME_SIZE 16
#define AUDIO_AC3_FRAME_SIZE 4
#define AUDIO_TV_PCM_FRAME_SIZE 32
#define AUDIO_DEFAULT_PCM_FRAME_SIZE 4

#define MAX_AVSYNC_GAP (10*90000)
#define MAX_AVSYNC_WAIT_TIME (3*1000*1000)

#define ALSA_DELAY_THRESHOLD_MS    (32)

#define ALSA_OUTPUT_PCM_FILE     "/data/vendor/audiohal/alsa_pcm_write.raw"
#define ALSA_OUTPUT_SPDIF_FILE   "/data/vendor/audiohal/alsa_spdif_write"


/* 0: alsa auge. 1: alsa non auge. */
/* speaker, spdif, headphone, other. refer aml_audio_out_dev_type_e.*/
aml_audio_out_dev_type_e alsa_out_ch_mask[2][AML_AUDIO_OUT_DEV_TYPE_BUTT] = {
    {AML_AUDIO_OUT_DEV_TYPE_SPDIF, AML_AUDIO_OUT_DEV_TYPE_SPEAKER, AML_AUDIO_OUT_DEV_TYPE_HEADPHONE, AML_AUDIO_OUT_DEV_TYPE_OTHER},
    {AML_AUDIO_OUT_DEV_TYPE_SPEAKER, AML_AUDIO_OUT_DEV_TYPE_SPDIF, AML_AUDIO_OUT_DEV_TYPE_HEADPHONE, AML_AUDIO_OUT_DEV_TYPE_OTHER},
};


static int aml_audio_get_alsa_debug() {
    int debug_flag = 0;
    debug_flag = get_debug_value(AML_DEBUG_AUDIOHAL_ALSA);
    return debug_flag;
}

static void alsa_write_rate_control(struct audio_stream_out *stream, size_t bytes  __unused, audio_format_t out_format) {

    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    size_t frame_size = audio_stream_out_frame_size(stream);
    int mutex_lock_status = 0;
    struct snd_pcm_status status;
    int sample_rate = MM_FULL_POWER_SAMPLING_RATE;
    int rate_multiply = 1;
    uint64_t frame_ms = 0;
    snd_pcm_sframes_t frames = 0;
    switch (out_format) {
    case AUDIO_FORMAT_E_AC3:
        frame_size = AUDIO_EAC3_FRAME_SIZE;
        break;
    case AUDIO_FORMAT_AC3:
        frame_size = AUDIO_AC3_FRAME_SIZE;
        break;
    default:
        frame_size = (aml_out->is_tv_platform == true) ? AUDIO_TV_PCM_FRAME_SIZE : AUDIO_DEFAULT_PCM_FRAME_SIZE;
        break;
    }

    if (adev->ms12_config.rate != 0) {
        sample_rate = adev->ms12_config.rate;
    }

    pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);

    if (status.state == PCM_STATE_RUNNING) {
        pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
        if (out_format == AUDIO_FORMAT_E_AC3) {
            rate_multiply = 4;
        }
        frame_ms = (uint64_t)frames * 1000LL/ (sample_rate * rate_multiply);
        if (frame_ms > ALSA_DELAY_THRESHOLD_MS) {
            /*we will go to sleep, unlock mutex*/
            if (pthread_mutex_unlock(&adev->alsa_pcm_lock) == 0) {
                mutex_lock_status = 1;
            }
            aml_audio_sleep((frame_ms - ALSA_DELAY_THRESHOLD_MS) * 1000);
            if (mutex_lock_status) {
                pthread_mutex_lock(&adev->alsa_pcm_lock);
            }
        }
    }
    return;
}

int aml_alsa_output_open(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct pcm_config *config = &aml_out->config;
    struct pcm_config config_raw;
    unsigned int device = aml_out->device;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    ALOGI("\n+%s stream %p,device %d", __func__, stream,device);
    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (adev->ms12.dolby_ms12_enable) {
            config = &(adev->ms12_config);
            device = ms12->device;
            ALOGI("%s indeed choose ms12 [config and device(%d)]", __func__, ms12->device);
            if (aml_out->device != device) {
                ALOGI("%s stream device(%d) differ with current device(%d)!", __func__, aml_out->device, device);
                aml_out->is_device_differ_with_ms12 = true;
            }
        } else {
            audio_format_t output_format = aml_out->alsa_output_format;
            get_hardware_config_parameters(
                config
                , output_format
                , adev->default_alsa_ch/*audio_channel_count_from_out_mask(aml_out->hal_channel_mask)*/
                , aml_out->config.rate
                , aml_out->is_tv_platform
                , continous_mode(adev)
                , adev->game_mode);
            switch (output_format) {
                case AUDIO_FORMAT_E_AC3:
                    device = DIGITAL_DEVICE;
                    break;
                case AUDIO_FORMAT_AC3:
                    device = DIGITAL_DEVICE;
                    break;
                case AUDIO_FORMAT_PCM_16_BIT:
                default:
                    device = I2S_DEVICE;
                    break;
            }
        }
    } else if (eDolbyDcvLib == adev->dolby_lib_type) {
        if (aml_out->dual_output_flag && adev->optical_format != AUDIO_FORMAT_PCM_16_BIT) {
            device = I2S_DEVICE;
            config->rate = MM_FULL_POWER_SAMPLING_RATE;
        } else if (aml_out->alsa_output_format != AUDIO_FORMAT_PCM_16_BIT &&
                   aml_out->hal_format != AUDIO_FORMAT_PCM_16_BIT) {
            memset(&config_raw, 0, sizeof(struct pcm_config));
            int period_mul = (aml_out->alsa_output_format  == AUDIO_FORMAT_E_AC3) ? 4 : 1;
            config_raw.channels = 2;
            config_raw.rate = aml_out->config.rate;//MM_FULL_POWER_SAMPLING_RATE ;
            config_raw.period_size = DEFAULT_PLAYBACK_PERIOD_SIZE * period_mul;
            config_raw.period_count = PLAYBACK_PERIOD_COUNT;
            if ((aml_out->hal_internal_format == AUDIO_FORMAT_DTS) || (aml_out->hal_internal_format == AUDIO_FORMAT_DTS_HD)) {
                config_raw.period_count *= 4;
            }
            config_raw.start_threshold = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
            config_raw.format = PCM_FORMAT_S16_LE;
            config = &config_raw;
            device = DIGITAL_DEVICE;
        }
    }
    int card = aml_out->card;
    struct pcm *pcm = adev->pcm_handle[device];
    ALOGI("%s pcm %p", __func__, pcm);

    // close former and open with configs
    // TODO: check pcm configs and if no changes, do nothing
    if (pcm && device != DIGITAL_DEVICE && device != I2S_DEVICE) {
        ALOGI("pcm device already opened,re-use pcm handle %p", pcm);
    } else {
        /*
        there are some audio format when digital output
        from dd->dd+ or dd+ --> dd,we need reopen the device.
        */
        if (pcm) {
            if (device == I2S_DEVICE)
                ALOGI("pcm device already opened,close the handle %p to reopen", pcm);
            pcm_close(pcm);
            adev->pcm_handle[device] = NULL;
            aml_out->pcm = NULL;
            pcm = NULL;
        }
        int device_index = device;
        // mark: will there wil issue here? conflit with MS12 device?? zz
        if (card == alsa_device_get_card_index()) {
            int alsa_port = alsa_device_get_port_index(device);
            device_index = alsa_device_update_pcm_index(alsa_port, PLAYBACK);
        }

        ALOGI("%s, audio open card(%d), device(%d)", __func__, card, device_index);
        ALOGI("ALSA open configs: channels %d format %d period_count %d period_size %d rate %d",
              config->channels, config->format, config->period_count, config->period_size, config->rate);
        ALOGI("ALSA open configs: threshold start %u stop %u silence %u silence_size %d avail_min %d",
              config->start_threshold, config->stop_threshold, config->silence_threshold, config->silence_size, config->avail_min);
        pcm = pcm_open(card, device_index, PCM_OUT, config);
        if (!pcm || !pcm_is_ready(pcm)) {
            ALOGE("%s, pcm %p open [ready %d] failed", __func__, pcm, pcm_is_ready(pcm));
            return -ENOENT;
        }
    }
    aml_out->pcm = pcm;
    adev->pcm_handle[device] = pcm;
    adev->pcm_refs[device]++;
    aml_out->dropped_size = 0;
    aml_out->device = device;
    aml_out->alsa_write_frames = 0;
    ALOGI("-%s, audio out(%p) device(%d) refs(%d) is_normal_pcm %d, handle %p\n\n",
          __func__, aml_out, device, adev->pcm_refs[device], aml_out->is_normal_pcm, pcm);
    ALOGI("+%s, adev->pcm_handle[%d] %p", __func__, device, adev->pcm_handle[device]);

    return 0;
}

void aml_alsa_output_close(struct audio_stream_out *stream) {
    ALOGI("\n+%s() stream %p\n", __func__, stream);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    unsigned int device = aml_out->device;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    if (eDolbyMS12Lib == adev->dolby_lib_type) {
        if (aml_out->is_device_differ_with_ms12) {
            ALOGI("%s stream out device(%d) truely use device(%d)\n", __func__, aml_out->device, ms12->device);
            device = ms12->device;
            aml_out->is_device_differ_with_ms12 = false;
        }
    }

    struct pcm *pcm = adev->pcm_handle[device];
    ALOGI("+%s, pcm handle %p aml_out->pcm %p", __func__, pcm, aml_out->pcm);
    ALOGI("+%s, adev->pcm_handle[%d] %p", __func__, device, adev->pcm_handle[device]);

    adev->pcm_refs[device]--;
    ALOGI("+%s, audio out(%p) device(%d), refs(%d) is_normal_pcm %d,handle %p",
          __func__, aml_out, device, adev->pcm_refs[device], aml_out->is_normal_pcm, aml_out->pcm);
    if (adev->pcm_refs[device] < 0) {
        adev->pcm_refs[device] = 0;
        ALOGI("%s, device(%d) refs(%d)\n", __func__, device, adev->pcm_refs[device]);
    }
    if (pcm && (adev->pcm_refs[device] == 0)) {
        ALOGI("%s(), pcm_close audio device[%d] pcm handle %p", __func__, device, pcm);
        pcm_close(pcm);
        adev->pcm_handle[device] = NULL;
    }
    aml_out->pcm = NULL;


    /* dual output management */
    /* TODO: only for Dcv, not for MS12 */
    if (is_dual_output_stream(stream) && (eDolbyDcvLib == adev->dolby_lib_type)) {
        device = DIGITAL_DEVICE;
        pcm = adev->pcm_handle[device];
        if (pcm) {
            pcm_close(pcm);
            adev->pcm_handle[device] = NULL;
        }
    }
    ALOGI("-%s()\n\n", __func__);
}

static int aml_alsa_add_zero(struct aml_stream_out *stream, int size) {
    int ret = 0;
    int retry = 10;
    char *buf = NULL;
    int write_size = 0;
    int adjust_bytes = size;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    while (retry--) {
        buf = aml_audio_malloc(AML_ZERO_ADD_MIN_SIZE);
        if (buf != NULL) {
            break;
        }
        usleep(10000);
    }
    if (buf == NULL) {
        return ret;
    }
    memset(buf, 0, AML_ZERO_ADD_MIN_SIZE);

    while (adjust_bytes > 0) {
        write_size = adjust_bytes > AML_ZERO_ADD_MIN_SIZE ? AML_ZERO_ADD_MIN_SIZE : adjust_bytes;
        ret = pcm_write(aml_out->pcm, (void*)buf, write_size);
        if (ret < 0) {
            ALOGE("%s alsa write fail when insert", __func__);
            break;
        }
        adjust_bytes -= write_size;
    }
    aml_audio_free(buf);
    return (size - adjust_bytes);
}

size_t aml_alsa_output_write(struct audio_stream_out *stream,
                             void *buffer,
                             size_t bytes) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    int ret = 0;
    struct pcm_config *config = &aml_out->config;
    size_t frame_size = audio_stream_out_frame_size(stream);
    bool need_trigger = false;
    bool is_dtv = (adev->patch_src == SRC_DTV);
    bool is_dtv_live = 1;
    bool has_video = adev->is_has_video;
    unsigned int first_apts = 0;
    unsigned int first_vpts = 0;
    unsigned int cur_apts = 0;
    unsigned int cur_vpts = 0;
    uint64_t cur_pcr = 0;
    uint64_t cur_pcr2 = 0;
    int av_diff = 0;
    int need_drop_inject = 0;
    int64_t pretime = 0;
    unsigned char*audio_data = (unsigned char*)buffer;
    int debug_enable = aml_audio_get_alsa_debug();
    switch (aml_out->alsa_output_format) {
    case AUDIO_FORMAT_E_AC3:
        frame_size = AUDIO_EAC3_FRAME_SIZE;
        break;
    case AUDIO_FORMAT_AC3:
        frame_size = AUDIO_AC3_FRAME_SIZE;
        break;
    default:
        frame_size = (aml_out->is_tv_platform == true) ? AUDIO_TV_PCM_FRAME_SIZE : AUDIO_DEFAULT_PCM_FRAME_SIZE;
        break;
    }

    // pre-check
    if (!has_video || !is_dtv) {
        goto write;
    }
    if (!adev->first_apts_flag) {
        goto write;
    }

    // video not comming. skip audio
    aml_hwsync_get_tsync_firstvpts(aml_out->hwsync, &first_vpts);
    if (first_vpts == 0) {
        ALOGI("[audio-startup] video not comming - skip this packet. size:%zu\n", bytes);
        aml_out->dropped_size += bytes;
        //memset(audio_data, 0, bytes);
        return bytes;
    }

    // av both comming. check need add zero or skip
    //get_sysfs_uint(TSYNC_FIRSTAPTS, (unsigned int *)&(first_apts));
    first_apts = adev->first_apts;
    aml_hwsync_get_tsync_vpts(aml_out->hwsync, &cur_vpts);
    aml_hwsync_get_tsync_pts(aml_out->hwsync, &cur_pcr);
    if (cur_vpts <= first_vpts) {
        cur_vpts = first_vpts;
    }



    cur_apts = (unsigned int)((int64_t)first_apts + (int64_t)(((int64_t)aml_out->dropped_size * 90) / (48 * frame_size)));
    av_diff = (int)((int64_t)cur_apts - (int64_t)cur_vpts);
    ALOGI("[audio-startup] av both comming.fa:0x%x fv:0x%x ca:0x%x cv:0x%x cp:0x%llx d:%d fs:%zu diff:%d ms\n",
          first_apts, first_vpts, cur_apts, cur_vpts, cur_pcr, aml_out->dropped_size, frame_size, av_diff / 90);

    // Exception
    if (abs(av_diff) > MAX_AVSYNC_GAP) {
        adev->first_apts = cur_apts;
        aml_audio_start_trigger(stream);
        adev->first_apts_flag = false;
        ALOGI("[audio-startup] case-0: ca:0x%x cv:0x%x dr:%d\n",
              cur_apts, cur_vpts, aml_out->dropped_size);
        goto write;
    }

#if 0
    // avsync inside 100ms, start
    if (abs(av_diff) < 100 * 90) {
        adev->first_apts = cur_apts;
        aml_audio_start_trigger(stream);
        adev->first_apts_flag = false;
        ALOGI("[audio-startup] case-1: ca:0x%x cv:0x%x dr:%d\n",
              cur_apts, cur_vpts, aml_out->dropped_size);
        goto write;
    }
#endif
    // drop case
    if (av_diff < 0) {
        need_drop_inject = abs(av_diff) / 90 * 48 * frame_size;
        need_drop_inject &= ~(frame_size - 1);
        if (need_drop_inject >= (int)bytes) {
            aml_out->dropped_size += bytes;
            ALOGI("[audio-startup] av sync drop %d pcm. total dropped:%d need_drop:%d\n",
                  (int)bytes, aml_out->dropped_size, need_drop_inject);
            if (cur_pcr > cur_vpts) {
                // seems can not cache video in time
                adev->first_apts = cur_apts;
                aml_audio_start_trigger(stream);
                adev->first_apts_flag = false;
                ALOGI("[audio-startup] case-2: drop %d pcm. total dropped:%d need_drop:%d\n",
                      (int)bytes, aml_out->dropped_size, need_drop_inject);
            }
            return bytes;
        } else {
            //emset(audio_data, 0, need_drop_inject);
            ret = pcm_write(aml_out->pcm, audio_data + need_drop_inject, bytes - need_drop_inject);
            aml_out->dropped_size += bytes;
            cur_apts = first_apts + (aml_out->dropped_size * 90) / (48 * frame_size);
            adev->first_apts = cur_apts;
            aml_audio_start_trigger(stream);
            adev->first_apts_flag = false;
            ALOGI("[audio-startup] case-3: drop done. ca:0x%x cv:0x%x dropped:%d\n",
                  cur_apts, cur_vpts, aml_out->dropped_size);
            return bytes;
        }
    }

    // wait video sync
    cur_apts = (unsigned int)((int64_t)first_apts + (int64_t)(((int64_t)aml_out->dropped_size * 90) / (48 * frame_size)));
    pretime = aml_gettime();
    while (1) {
        usleep(MIN_WRITE_SLEEP_US);
        aml_hwsync_get_tsync_pts(aml_out->hwsync, &cur_pcr2);
        if (cur_pcr2 > cur_apts - 10 * 90) {
            break;
        }
        if (aml_gettime() - pretime >= MAX_AVSYNC_WAIT_TIME) {
            ALOGI("[audio-startup] add zero exceed %d ms quit.\n", MAX_AVSYNC_WAIT_TIME / 1000);
            break;
        }
    }
    adev->first_apts = cur_apts;
    aml_audio_start_trigger(stream);
    adev->first_apts_flag = false;
    ALOGI("[audio-startup] case-4: ca:0x%x cv:0x%x dropped:%d add zero:%d\n",
          cur_apts, cur_vpts, aml_out->dropped_size, need_drop_inject);

write:

    if (get_debug_value(AML_DUMP_AUDIOHAL_ALSA)) {
        aml_audio_dump_audio_bitstreams(ALSA_OUTPUT_PCM_FILE, buffer, bytes);
    }

    // SWPL-412, when input source is DTV, and UI set "parental_control_av_mute" command to audio hal
    // we need to mute audio output for PCM output here
    if (adev->patch_src == SRC_DTV && adev->parental_control_av_mute) {
        memset(buffer,0x0,bytes);
    }

    if (aml_out->pcm == NULL) {
        ALOGE("%s: pcm is null", __func__);
        return bytes;
    }
    /*+[SE][BUG][SWPL-14811][zhizhong] add drop ac3 pcm function*/
    if (adev->patch_src ==  SRC_DTV && aml_out->need_drop_size > 0 && adev->audio_patch != NULL) {
        if (aml_out->need_drop_size >= (int)bytes) {
            aml_out->need_drop_size -= bytes;
            ALOGI("av sync drop %d pcm, need drop:%d more,apts:0x%x,pcr:0x%x\n",
                (int)bytes, aml_out->need_drop_size, adev->audio_patch->last_apts, adev->audio_patch->last_pcrpts);
            if (adev->audio_patch->last_apts >= adev->audio_patch->last_pcrpts) {
                ALOGI("pts already ok, drop finish\n");
                aml_out->need_drop_size = 0;
            } else
                return bytes;
        } else {
            ALOGI("bytes:%zu, need_drop_size=%d\n", bytes, aml_out->need_drop_size);
            if (adev->discontinue_mute_flag) {
                ALOGI("drop mute discontinue_mute_flag=%d\n",
                adev->discontinue_mute_flag);
                memset(audio_data + aml_out->need_drop_size, 0x0,
                        bytes - aml_out->need_drop_size);
            }
            ret = pcm_write(aml_out->pcm, audio_data + aml_out->need_drop_size,
                    bytes - aml_out->need_drop_size);
            aml_out->need_drop_size = 0;
            ALOGI("drop finish\n");
            return bytes;
        }
    }

    {
        struct snd_pcm_status status;
        bool alsa_status;
        pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
        alsa_status = (status.state == PCM_STATE_RUNNING);
        if (alsa_status != aml_out->alsa_running_status) {
            aml_out->alsa_running_status = alsa_status;
            aml_out->alsa_status_changed = true;
        }
        if (status.state == PCM_STATE_XRUN) {
            ALOGW("[%s:%d] alsa underrun", __func__, __LINE__);
            if (adev->audio_discontinue) {
                adev->discontinue_mute_flag = 1;
                adev->no_underrun_count = 0;
                ALOGD("output_write, audio discontinue, underrun, begin mute");
            }
        } else if (adev->discontinue_mute_flag == 1 && adev->patch_src ==  SRC_DTV ) {
            if (adev->audio_patch != NULL && adev->audio_discontinue == 0 &&
                adev->audio_patch->dtv_audio_tune == AUDIO_RUNNING) {
                ALOGD("[%s:%d] no underrun, not mute, dtv_audio_tune is RUNNING, audio_discontinue=%d",
                    __func__, __LINE__, adev->audio_discontinue);
                adev->discontinue_mute_flag = 0;
                adev->no_underrun_count = 0;
            } else if (adev->no_underrun_count++ >= adev->no_underrun_max) {
                ALOGD("[%s:%d] no underrun, not mute, audio_discontinue:%d >= count:%d", __func__, __LINE__,
                        adev->audio_discontinue, adev->no_underrun_count);
                adev->discontinue_mute_flag = 0;
                adev->no_underrun_count = 0;
            }
        }

        if (adev->patch_src == SRC_DTV && (adev->discontinue_mute_flag ||
            adev->underrun_mute_flag)) {
            memset(buffer, 0x0, bytes);
        }

        if (!adev->continuous_audio_mode && !audio_is_linear_pcm(aml_out->alsa_output_format)) {
            /*to avoid ca noise in Sony TV when audio format switch*/
            if (status.state == PCM_STATE_SETUP ||
                status.state == PCM_STATE_PREPARED ||
                status.state == PCM_STATE_XRUN) {
                ALOGI("mute the first dd+ raw data");
                memset(buffer, 0,bytes);
            }
        }

    }

    if (adev->raw_to_pcm_flag) {
        pcm_stop(aml_out->pcm);
        adev->raw_to_pcm_flag = false;
        ALOGI("raw to lpcm switch %s\n",__func__);
    }

    /*for ms12 case, we control the output buffer level*/
    if ((adev->continuous_audio_mode == 1) && (eDolbyMS12Lib == adev->dolby_lib_type)) {
        alsa_write_rate_control(stream, bytes, aml_out->alsa_output_format);
    }

    aml_out->alsa_write_cnt++;
    aml_out->alsa_write_frames += pcm_bytes_to_frames(aml_out->pcm, bytes);

    if (debug_enable || (aml_out->alsa_write_cnt % 1000) == 0) {
        snd_pcm_sframes_t frames = 0;
        ret = pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
        ALOGI("alsa format =0x%x delay frames =%ld total frames=%lld", aml_out->alsa_output_format, frames, aml_out->alsa_write_frames);
    }

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT) && (aml_out->is_tv_platform == false)) {
        check_audio_level("alsa_write", buffer, bytes);
    }

    ret = pcm_write(aml_out->pcm, buffer, bytes);
    if (ret < 0) {
        ALOGE("%s write failed,pcm handle %p %s, stream %p, %s",
            __func__, aml_out->pcm, pcm_get_error(aml_out->pcm),
            aml_out, usecase2Str(aml_out->usecase));
    }

    return ret;
}

int aml_alsa_output_pause(struct audio_stream_out *stream) {
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    int ret = 0;
    if (out->pcm && pcm_is_ready (out->pcm)) {
        ret = pcm_ioctl (out->pcm, SNDRV_PCM_IOCTL_PAUSE, 1);
        if (ret < 0) {
            ALOGE ("cannot pause channel\n");
        } else {
            ret = 0;
            // set the pcm pause state
            if (out->pcm == adev->pcm)
                adev->pcm_paused = true;
            else
                ALOGE ("out->pcm and adev->pcm are assumed same handle");
        }
    }

    return 0;
}

int aml_alsa_output_resume(struct audio_stream_out *stream) {
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    int ret = 0;
    if (out->pcm && pcm_is_ready (out->pcm) ) {
        ret = pcm_ioctl (out->pcm, SNDRV_PCM_IOCTL_PREPARE, 0);
        if (ret < 0) {
            ALOGE ("%s(), cannot resume channel\n", __func__);
        } else {
            ret = 0;
            // clear the pcm pause state
            if (out->pcm == adev->pcm)
                adev->pcm_paused = false;
        }
    }

    return 0;
}

int aml_alsa_output_get_latency(struct audio_stream_out *stream) {
    const struct aml_stream_out *aml_out = (const struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    snd_pcm_sframes_t frames = 0;
    int start_threshold = aml_out->config.start_threshold;
    if (aml_out->pcm && pcm_is_ready(aml_out->pcm)) {
        pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
        ALOGV("aml_alsa_output_get_latency frames %ld start_threshold %d",frames, start_threshold);
        if ( frames > 0) {
            return (frames * 1000) / aml_out->config.rate;
        } else {
             if (eDolbyMS12Lib == adev->dolby_lib_type) {
                 if (start_threshold == 0) {
                      start_threshold =  2 * DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
                 }
                 if (ms12->master_pcm_frames < start_threshold ) {
                     frames = start_threshold;
                     ALOGI("aml_alsa_output_get_latency frames %ld",frames);
                     return (frames * 1000) / aml_out->config.rate;
                 }
             }
        }
    }

    return 0;
}

void aml_close_continuous_audio_device(struct audio_hw_device *dev) {
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int pcm_index = 0;
    int spdif_index = 1;
    struct pcm *continuous_pcm_device = adev->pcm_handle[pcm_index];
    struct pcm *continuous_spdif_device = adev->pcm_handle[1];
    ALOGI("\n+%s() choose device %d pcm %p\n", __FUNCTION__, pcm_index, continuous_pcm_device);
    ALOGI("%s maybe also choose device %d pcm %p\n", __FUNCTION__, spdif_index, continuous_spdif_device);
    if (continuous_pcm_device) {
        pcm_close(continuous_pcm_device);
        continuous_pcm_device = NULL;
        adev->pcm_handle[pcm_index] = NULL;
        adev->pcm_refs[pcm_index] = 0;
    }
    if (continuous_spdif_device) {
        pcm_close(continuous_spdif_device);
        continuous_spdif_device = NULL;
        adev->pcm_handle[spdif_index] = NULL;
        adev->pcm_refs[spdif_index] = 0;
    }
    ALOGI("-%s(), when continuous is at end, the pcm/spdif devices(single/dual output) are closed!\n\n", __FUNCTION__);
    return ;
}

#define WAIT_COUNT_MAX 30
size_t aml_alsa_input_read(struct audio_stream_in *stream,
                        void *buffer,
                        size_t bytes) {
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *aml_dev = in->dev;
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    char  *read_buf = (char *)buffer;
    int ret = 0;
    size_t  read_bytes = 0;
    int nodata_count = 0;
    struct pcm *pcm_handle = in->pcm;
    size_t frame_size = in->config.channels * pcm_format_to_bits(in->config.format) / 8;
    bool hdmi_raw_in_flag = patch && (patch->input_src == AUDIO_DEVICE_IN_HDMI) && (!audio_is_linear_pcm(patch->aformat));

    while (read_bytes < bytes) {
        if (patch && patch->input_thread_exit) {
            memset((void*)buffer,0,bytes);
            return 0;
        }

        pcm_handle = in->pcm;
        if (pcm_handle == NULL) {
            ALOGE("%s pcm_handle is NULL", __FUNCTION__);
            return -1;
        }

        ret = pcm_read(pcm_handle, (unsigned char *)buffer + read_bytes, bytes - read_bytes);
        if (ret >= 0) {
            nodata_count = 0;
            read_bytes += ret;
            ALOGV("pcm_handle:%p, ret:%d read_bytes:%d, bytes:%d ",
                pcm_handle,ret,read_bytes,bytes);
        } else if (ret != -EAGAIN) {
            ALOGD("%s:%d, pcm_read fail, ret:%#x, error info:%s",
                __func__, __LINE__, ret, strerror(errno));
            memset((void*)buffer,0,bytes);
            return ret;
        } else {
            if (hdmi_raw_in_flag) {
                usleep((bytes - read_bytes) * 1000000 / audio_stream_in_frame_size(stream) /
                    in->config.rate);
            } else {
                usleep((bytes - read_bytes) * 1000000 / audio_stream_in_frame_size(stream) /
                    in->config.rate / 2);

            }

             ALOGV("bytes %d bytes - read_bytes %d nodata_count %d",bytes, bytes - read_bytes, nodata_count);
             nodata_count++;
             if (nodata_count >= WAIT_COUNT_MAX) {
                 nodata_count = 0;
                 AM_LOGW("read timeout, in:%p read_bytes:%d need:%d", in, read_bytes, bytes);
                 memset((void*)buffer, 0, bytes);
                 return 0;
             }
        }
    }
    return 0;
}

int aml_alsa_input_flush(struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct pcm *pcm_handle = in->pcm;
    int ret = 0;

    ret = pcm_ioctl(pcm_handle, SNDRV_PCM_IOCTL_RESET, 0);
    if (ret < 0) {
        ALOGE("cannot reset pcm!");
        return ret;
    }

    return 0;
}

typedef struct alsa_handle {
    unsigned int card;
    unsigned int pcm_index;   /*used for open the alsa device*/
    struct pcm_config config;
    struct pcm *pcm;
    int    block_mode;
    unsigned int alsa_port;  /*refer to PORT_SPDIF, PORT***/
    audio_format_t  format;
    uint32_t write_cnt;
    uint64_t write_frames;
    int pcm2_mute_cnt;
} alsa_handle_t;

static void alsa_write_new_rate_control(void *handle) {

    alsa_handle_t * alsa_handle = (alsa_handle_t *)handle;
    struct aml_audio_device *adev = (struct aml_audio_device *)adev_get_handle();
    snd_pcm_sframes_t frames = 0;
    struct snd_pcm_status status;
    int ret = 0;
    int rate = alsa_handle->config.rate;
    int rate_multiply = 1;
    int frame_ms = 0;

    pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_STATUS, &status);

    if (alsa_handle->format == AUDIO_FORMAT_MAT) {
        rate_multiply = 4;
    } else if (alsa_handle->format == AUDIO_FORMAT_E_AC3) {
        rate_multiply = 4;
    }

    if (status.state == PCM_STATE_RUNNING) {
        pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);

        frame_ms = (uint64_t)frames * 1000LL/ (rate * rate_multiply);
        ALOGI("format %#x frame_ms=%d", alsa_handle->format, frame_ms);
        if (frame_ms > ALSA_DELAY_THRESHOLD_MS) {
            aml_audio_sleep((frame_ms - ALSA_DELAY_THRESHOLD_MS) * 1000);

        }
    }
    return;
}


int aml_alsa_output_open_new(void **handle, aml_stream_config_t * stream_config, aml_device_config_t *device_config)
{
    int ret = -1;
    struct pcm_config *config = NULL;
    int card = 0;
    int alsa_port = 0;
    int pcm_index = -1;
    struct pcm *pcm = NULL;
    alsa_handle_t * alsa_handle = NULL;
    bool platform_is_tv = 0;
    audio_format_t  format = AUDIO_FORMAT_PCM_16_BIT;
    unsigned int channels = 0;
    unsigned int rate = 0;
    struct aml_audio_device *adev = (struct aml_audio_device *)adev_get_handle();


    alsa_handle = (alsa_handle_t *)aml_audio_calloc(1, sizeof(alsa_handle_t));
    if (alsa_handle == NULL) {
        ALOGE("malloc alsa_handle failed\n");
        return -1;
    }

    config = &alsa_handle->config;
    if (stream_config->config.format == AUDIO_FORMAT_IEC61937) {
        format = stream_config->config.offload_info.format;
    }
    channels = audio_channel_count_from_out_mask(stream_config->config.channel_mask);
    rate     = stream_config->config.sample_rate;
    get_hardware_config_parameters(config, format, adev->default_alsa_ch/*channels*/, rate, platform_is_tv,
                continous_mode(adev), adev->game_mode);

    config->channels = channels;
    config->rate     = rate;
    if (config->rate == 0 || config->channels == 0) {

        ALOGE("Invalid sampleate=%d channel=%d\n", config->rate, config->channels);
        goto exit;
    }

    config->format = convert_audio_format_2_alsa_format(format);
    config->avail_min = 0;
    card = alsa_device_get_card_index();
    alsa_port = device_config->device_port;
    if (alsa_port < 0) {
        ALOGE("Wrong alsa_device ID\n");
        return -1;
    }
    pcm_index = alsa_device_update_pcm_index(alsa_port, PLAYBACK);

    ALOGI("In pcm open ch=%d rate=%d\n", config->channels, config->rate);
    ALOGI("%s, audio open card(%d), device(%d) \n", __func__, card, pcm_index);
    ALOGI("ALSA open configs: channels %d format %d period_count %d period_size %d rate %d \n",
          config->channels, config->format, config->period_count, config->period_size, config->rate);
    ALOGI("ALSA open configs: threshold start %u stop %u silence %u silence_size %d avail_min %d \n",
          config->start_threshold, config->stop_threshold, config->silence_threshold, config->silence_size, config->avail_min);

    pcm = pcm_open(card, pcm_index, PCM_OUT, config);
    if (!pcm || !pcm_is_ready(pcm)) {
        ALOGE("%s, pcm %p open [ready %d] failed \n", __func__, pcm, pcm_is_ready(pcm));
        goto exit;
    }

    alsa_handle->card = card;
    alsa_handle->alsa_port = pcm_index;
    alsa_handle->pcm = pcm;
    alsa_handle->alsa_port = alsa_port;
    alsa_handle->format = format;
    alsa_handle->write_cnt = 0;
    alsa_handle->write_frames = 0;

    *handle = (void*)alsa_handle;

    return 0;

exit:
    if (alsa_handle) {
        aml_audio_free(alsa_handle);
    }
    *handle = NULL;
    return -1;

}


void aml_alsa_output_close_new(void *handle) {
    ALOGI("\n+%s() hanlde %p\n", __func__, handle);
    alsa_handle_t * alsa_handle = NULL;
    struct pcm *pcm = NULL;
    struct aml_audio_device *adev = (struct aml_audio_device *)adev_get_handle();

    alsa_handle = (alsa_handle_t *)handle;

    if (alsa_handle == NULL) {
        ALOGE("%s handle is NULL\n", __func__);
        return;
    }

    if (alsa_handle->pcm == NULL) {
        ALOGE("%s PCM is NULL\n", __func__);
        return;
    }
    pcm = alsa_handle->pcm;
    pcm_close(pcm);
    aml_audio_free(alsa_handle);

    ALOGI("-%s()\n\n", __func__);
}

size_t aml_alsa_output_write_new(void *handle, const void *buffer, size_t bytes) {
    int ret = -1;
    int write_frames = bytes / 4;
    int overflow_flag = 0,underrun_flag = 0;
    alsa_handle_t * alsa_handle = NULL;
    struct pcm *dd_pcm = NULL,*ddp_pcm = NULL;
    alsa_handle = (alsa_handle_t *)handle;
    struct aml_audio_device *adev = (struct aml_audio_device *)adev_get_handle();
    char file_name[128] = { 0 };
    char audio_type[32] = { 0 };
    int debug_enable = aml_audio_get_alsa_debug();
    if (alsa_handle == NULL || alsa_handle->pcm == NULL || buffer == NULL || bytes == 0) {
        ALOGW("[%s:%d] invalid param, pcm:%p, alsa_handle:%p, buffer:%p, bytes:%zu",
            __func__, __LINE__, alsa_handle->pcm, alsa_handle, buffer, bytes);
        return -1;
    }
#if 0
    //ALOGD("handle=%p pcm=%p\n",alsa_handle,alsa_handle->pcm);
    /*add for work around ddp dd ouput ,ddp underrun issue */
    dd_pcm = alsa_handle->pcm;
    ddp_pcm = adev->pcm_handle[adev->ms12.device];
    if (is_sc2_chip() && eDolbyMS12Lib == adev->dolby_lib_type && dd_pcm && ddp_pcm && (alsa_handle->format != AUDIO_FORMAT_MAT)) {
        snd_pcm_sframes_t delay_ddp = 0,delay_dd = 0;

        ret = pcm_ioctl(dd_pcm, SNDRV_PCM_IOCTL_DELAY, &delay_dd);
        if (ret < 0) {
             delay_dd = alsa_handle->config.start_threshold;
        }
        ret = pcm_ioctl(ddp_pcm, SNDRV_PCM_IOCTL_DELAY, &delay_ddp);
        if (ret < 0) {
             delay_ddp = adev->ms12_config.start_threshold;
        }

        if (delay_dd + write_frames >= alsa_handle->config.start_threshold * 2)
            overflow_flag = 1;
        /* dd write blocked and ddp delay at a low level ,so skip dd data to avoid ddp underun*/
        if (overflow_flag && delay_ddp <= 2 * 6144)
            underrun_flag = 1;

        if ( underrun_flag ) {
           ALOGI("skip dd data for delay_dd frame =%ld  delay_ddp frame %ld\n",delay_dd, delay_ddp);
           return 0;
        }
    }
#endif
    {
        struct snd_pcm_status status;
        pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
        if (status.state == PCM_STATE_XRUN) {
            ALOGW("[%s:%d] format =0x%x alsa underrun", __func__, __LINE__, alsa_handle->format);
        }
    }
    if (get_debug_value(AML_DUMP_AUDIOHAL_ALSA) ||
        get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        if (alsa_handle->format == AUDIO_FORMAT_AC3) {
            snprintf(audio_type, 32, "%s", "dd");
        } else if (alsa_handle->format == AUDIO_FORMAT_E_AC3) {
            snprintf(audio_type, 32, "%s", "ddp");
        } else if (alsa_handle->format == AUDIO_FORMAT_MAT) {
            snprintf(audio_type, 32, "%s", "mat");
        } else if (alsa_handle->format == AUDIO_FORMAT_DTS) {
            snprintf(audio_type, 32, "%s", "dts");
        } else {
            snprintf(audio_type, 32, "%s", "pcm");
        }
        snprintf(file_name, 128, "%s.%s", ALSA_OUTPUT_SPDIF_FILE, audio_type);
    }

    if (get_debug_value(AML_DUMP_AUDIOHAL_ALSA)) {

        aml_audio_dump_audio_bitstreams(file_name, buffer, bytes);
    }

    alsa_handle->write_cnt++;
    alsa_handle->write_frames += pcm_bytes_to_frames(alsa_handle->pcm, bytes);

    if (debug_enable || (alsa_handle->write_cnt % 1000) == 0) {
        snd_pcm_sframes_t frames = 0;
        ret = pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
        ALOGI("alsa format =0x%x delay frames =%ld total frames=%lld", alsa_handle->format, frames, alsa_handle->write_frames);
    }

#if 0
    /*for ms12 case, we control the output buffer level*/
    if ((adev->continuous_audio_mode == 1) && (eDolbyMS12Lib == adev->dolby_lib_type)) {
        alsa_write_new_rate_control(alsa_handle);
    }
#endif

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level(audio_type, buffer, bytes);
    }


    ret = pcm_write(alsa_handle->pcm, buffer, bytes);
    return ret;
}

int aml_alsa_output_data_handle(void *handle, void *output_buffer, size_t size, int vaule, bool is_mute)
{
    alsa_handle_t *alsa_handle = (alsa_handle_t *)handle;

    if (is_mute) {
        memset(output_buffer, 0, size);
        return 0;
    }

    if (size && alsa_handle->format == AUDIO_FORMAT_E_AC3) {
        struct snd_pcm_status status;
        pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
        if (status.state == PCM_STATE_SETUP ||
            status.state == PCM_STATE_PREPARED ||
            status.state == PCM_STATE_XRUN) {
            /*for sony tv, we need mute first 1 frames to avoid "ca" noise*/
            alsa_handle->pcm2_mute_cnt = 1;
            ALOGI("spdif b mute the data cnt =%d",alsa_handle->pcm2_mute_cnt);
        }
        if (alsa_handle->pcm2_mute_cnt) {
            alsa_handle->pcm2_mute_cnt--;
            memset(output_buffer, vaule, size);
        }
    }

    return 0;
}

int aml_alsa_output_getinfo(void *handle, alsa_info_type_t type, alsa_output_info_t * info) {
    int ret = -1;
    alsa_handle_t * alsa_handle = NULL;
    alsa_handle = (alsa_handle_t *)handle;

    if (handle == NULL) {
        return -1;
    }
    switch (type) {
    case OUTPUT_INFO_DELAYFRAME: {
        snd_pcm_sframes_t delay = 0;
        int rate = alsa_handle->config.rate;
        int rate_multiply = 1;
        ret = pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_DELAY, &delay);
        if (ret < 0) {
            info->delay_ms = 0;
            return -1;
        }
        if (alsa_handle->format == AUDIO_FORMAT_MAT) {
            rate_multiply = 4;
        } else if (alsa_handle->format == AUDIO_FORMAT_E_AC3) {
            rate_multiply = 4;
        }
        info->delay_ms = delay * 1000 / (rate * rate_multiply);
        return 0;
    }
    default:
        return -1;
    }
    return -1;
}

int aml_alsa_output_pause_new(void *handle) {
    int ret = -1;
    alsa_handle_t * alsa_handle = (alsa_handle_t *)handle;

    if (handle == NULL) {
        return -1;
    }
    if (alsa_handle->pcm) {
        ret = pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_PAUSE, 1);
        if (ret < 0) {
            ALOGE("%s error %d", __func__, ret);
        }
    }
    return ret;
}

int aml_alsa_output_resume_new(void *handle) {
    int ret = -1;
    alsa_handle_t * alsa_handle = (alsa_handle_t *)handle;

    if (handle == NULL) {
        return -1;
    }
    if (alsa_handle->pcm) {
        ret = pcm_ioctl(alsa_handle->pcm, SNDRV_PCM_IOCTL_PREPARE);
        if (ret < 0) {
            ALOGE("%s error %d", __func__, ret);
        }
    }
    return ret;
}

void alsa_out_reconfig_params(struct audio_stream_out *stream)
{
    ALOGD("%s()!", __func__);
    aml_alsa_output_close(stream);
    aml_alsa_output_open(stream);
}

enum pcm_format convert_audio_format_2_alsa_format(audio_format_t format)
{
    switch (format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        return PCM_FORMAT_S16_LE;
    case AUDIO_FORMAT_PCM_32_BIT:
        return PCM_FORMAT_S32_LE;
    case AUDIO_FORMAT_PCM_8_BIT:
        return PCM_FORMAT_S8;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        return PCM_FORMAT_S24_LE;
    case AUDIO_FORMAT_PCM_24_BIT_PACKED:
        return PCM_FORMAT_S24_3LE;
    default:
        AM_LOGE("invalid format:%#x, return 16bit format.", format);
        return PCM_FORMAT_S16_LE;
    }
}

