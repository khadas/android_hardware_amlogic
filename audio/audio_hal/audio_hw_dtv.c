/*
 * Copyright (C) 2018 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_dtv"
//#define LOG_NDEBUG 0

#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/hardware.h>
#include <inttypes.h>
#include <linux/ioctl.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/system_properties.h>
#include <system/audio.h>
#include <time.h>
#include <utils/Timers.h>

#if ANDROID_PLATFORM_SDK_VERSION >= 25 // 8.0
#include <system/audio-base.h>
#endif

#include <hardware/audio.h>

#include <aml_android_utils.h>
#include <aml_data_utils.h>


#include "aml_audio_stream.h"
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "audio_hw_profile.h"
#include "audio_hw_utils.h"
#include "dtv_patch_out.h"
#include "aml_audio_resampler.h"
#include "audio_hw_ms12.h"
#include "dolby_lib_api.h"
#include "audio_dtv_ad.h"
#include "alsa_config_parameters.h"
#include "alsa_device_parser.h"
#include "aml_audio_hal_avsync.h"
#include "aml_audio_spdifout.h"
#include "aml_audio_timer.h"
#include "aml_volume_utils.h"
#include "dmx_audio_es.h"
#include "uio_audio_api.h"
#include "audio_dtv_sync.h"
#include "aml_ddp_dec_api.h"
#include "aml_dts_dec_api.h"
#include "audio_dtv_utils.h"

static struct timespec start_time;
const unsigned int mute_dd_frame[] = {
    0x5d9c770b, 0xf0432014, 0xf3010713, 0x2020dc62, 0x4842020, 0x57100404, 0xf97c3e1f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0xf97c75fe, 0x9fcfe7f3,
    0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0x3e5f9dff, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0x48149ff2, 0x2091,
    0x361e0000, 0x78bc6ddb, 0xbbbbe3f1, 0xb8, 0x0, 0x0, 0x0, 0x77770700, 0x361e8f77, 0x359f6fdb, 0xd65a6bad, 0x5a6badb5, 0x6badb5d6, 0xa0b5d65a, 0x1e000000, 0xbc6ddb36,
    0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35, 0xa6b5d6, 0x0, 0xb66de301, 0x1e8fc7db, 0x80bbbb3b, 0x0, 0x0,
    0x0, 0x0, 0x78777777, 0xb66de3f1, 0xd65af3f9, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x5a6b, 0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0,
    0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0x605a, 0x1e000000, 0xbc6ddb36, 0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35,
    0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0xa0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0, 0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xa6b5d65a, 0x0,
    0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0, 0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0x5a6bad, 0xe3010000, 0xc7dbb66d,
    0xbb3b1e8f, 0x80bb, 0x0, 0x0, 0x0, 0x77770000, 0xe3f17877, 0xf3f9b66d, 0xadb5d65a, 0x605a6b, 0x0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0,
    0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xa0b5, 0xdb361e00, 0xf178bc6d, 0xb8bbbbe3, 0x0, 0x0, 0x0, 0x0,
    0x77777707, 0xdb361e8f, 0xad359f6f, 0xb5d65a6b, 0x10200a6, 0x0, 0xdbb6f100, 0x8fc7e36d, 0xc0dddd1d, 0x0, 0x0, 0x0, 0x0, 0xbcbbbb3b, 0xdbb6f178, 0x6badf97c,
    0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xadb5, 0xb6f10000, 0xc7e36ddb, 0xdddd1d8f, 0xc0, 0x0, 0x0, 0x0, 0xbbbb3b00, 0xb6f178bc, 0xadf97cdb, 0xb5d65a6b, 0x4deb00ad
};

const unsigned int mute_ddp_frame[] = {
    0x7f01770b, 0x20e06734, 0x2004, 0x8084500, 0x404046c, 0x1010104, 0xe7630001, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xce7f9fcf, 0x7c3e9faf,
    0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xf37f9fcf, 0x9fcfe7ab, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x53dee7f3, 0xf0e9,
    0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d,
    0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0,
    0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0,
    0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a,
    0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0,
    0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db,
    0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0,
    0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000,
    0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5,
    0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x40, 0x7f227c55,
};
static int pcr_apts_diff;

static int create_dtv_output_stream_thread(struct aml_audio_patch *patch);
static int release_dtv_output_stream_thread(struct aml_audio_patch *patch);
static int create_dtv_input_stream_thread(struct aml_audio_patch *patch);
static int release_dtv_input_stream_thread(struct aml_audio_patch *patch);

static void ts_wait_time(struct timespec *ts, uint32_t time)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += time / 1000000;
    ts->tv_nsec += (time * 1000) % 1000000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -=1000000000;
    }
}

static void dtv_check_audio_reset()
{
    ALOGI("reset dtv audio port\n");
    aml_sysfs_set_str(AMSTREAM_AUDIO_PORT_RESET, "1");
}

void decoder_set_latency(unsigned int latency)
{
    char tempbuf[128];
    memset(tempbuf, 0, 128);
    sprintf(tempbuf, "%d", latency);
    ALOGI("latency=%u\n", latency);
    if (aml_sysfs_set_str(TSYNC_PCR_LANTCY, tempbuf) == -1) {
        ALOGE("set pcr lantcy failed %s\n", tempbuf);
    }
    return;
}

unsigned int decoder_get_latency(void)
{
    unsigned int latency = 0;
    int ret;
    char buff[64];
    memset(buff, 0, 64);
    ret = aml_sysfs_get_str(TSYNC_PCR_LANTCY, buff, sizeof(buff));
    if (ret > 0) {
        ret = sscanf(buff, "%u\n", &latency);
    }
    //ALOGI("get lantcy %d", latency);
    return (unsigned int)latency;
}

int get_video_delay(void)
{
    char tempbuf[128] = {0};
    int vpts = 0, ret;
    ret = aml_sysfs_get_str(TSYNC_VPTS_ADJ, tempbuf, sizeof(tempbuf));
    if (ret > 0) {
        ret = sscanf(tempbuf, "%d\n", &vpts);
    }
    if (ret > 0) {
        return vpts;
    } else {
        vpts = 0;
    }
    return vpts;
}

static int get_dtv_audio_mode(void)
{
    int ret, mode = 0;
    char buff[64];
    ret = aml_sysfs_get_str(TSYNC_AUDIO_MODE, buff, sizeof(buff));
    if (ret > 0) {
        ret = sscanf(buff, "%d", &mode);
    }
    return mode;
}

int get_dtv_pcr_sync_mode(void)
{
    int ret, mode = 0;
    char buff[64];
    ret = aml_sysfs_get_str(TSYNC_PCR_MODE, buff, sizeof(buff));
    if (ret > 0) {
        ret = sscanf(buff, "%d", &mode);
    }
    return mode;
}

void clean_dtv_patch_pts(struct aml_audio_patch *patch)
{
    if (patch) {
        patch->last_apts = 0;
        patch->last_pcrpts = 0;
    }
}
int get_audio_checkin_underrun(void)
{
    char tempbuf[128];
    int a_checkin_underrun = 0, ret = 0;
    ret = aml_sysfs_get_str(TSYNC_AUDIO_UNDERRUN, tempbuf, sizeof(tempbuf));
    if (ret > 0) {
        ret = sscanf(tempbuf, "%d\n", &a_checkin_underrun);
    } else
        ALOGI("getting failed\n");
    return a_checkin_underrun;
}

int get_audio_discontinue(void)
{
    char tempbuf[128];
    int a_discontinue = 0, ret;
    ret = aml_sysfs_get_str(TSYNC_AUDIO_LEVEL, tempbuf, sizeof(tempbuf));
    if (ret > 0) {
        ret = sscanf(tempbuf, "%d\n", &a_discontinue);
    }
    if (ret > 0 && a_discontinue > 0) {
        a_discontinue = (a_discontinue & 0xff);
    } else {
        a_discontinue = 0;
    }
    return a_discontinue;
}
static int get_video_discontinue(void)
{
    char tempbuf[128];
    int pcr_vdiscontinue = 0, ret;
    ret = aml_sysfs_get_str(TSYNC_VIDEO_DISCONT, tempbuf, sizeof(tempbuf));
    if (ret > 0) {
        ret = sscanf(tempbuf, "%d\n", &pcr_vdiscontinue);
    }
    if (ret > 0 && pcr_vdiscontinue > 0) {
        pcr_vdiscontinue = (pcr_vdiscontinue & 0xff);
    } else {
        pcr_vdiscontinue = 0;
    }
    return pcr_vdiscontinue;
}

void  clean_dtv_demux_info(aml_demux_audiopara_t *demux_info) {
    demux_info->demux_id = -1;
    demux_info->security_mem_level  = -1;
    demux_info->output_mode  = -1;
    demux_info->has_video  = 0;
    demux_info->main_fmt  = -1;
    demux_info->main_pid  = -1;
    demux_info->ad_fmt  = -1;
    demux_info->ad_pid  = -1;
    demux_info->dual_decoder_support = 0;
    demux_info->associate_audio_mixing_enable  = 0;
    demux_info->media_sync_id  = -1;
    demux_info->media_presentation_id  = -1;
    demux_info->ad_package_status  = -1;
}
static int dtv_patch_handle_event(struct audio_hw_device *dev, int cmd, int val) {

    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int has_audio = 1;
    int audio_sync_mode = 0;
    pthread_mutex_lock(&adev->dtv_lock);
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)adev->aml_dtv_audio_instances;
    unsigned int path_id = val >> DVB_DEMUX_ID_BASE;
    ALOGI("%s path_id %d cmd %d",__FUNCTION__,path_id, cmd);
    if (path_id < 0  ||  path_id >= DVB_DEMUX_SUPPORT_MAX_NUM) {
        ALOGI("path_id %d  is invalid ! ",path_id);
        goto exit;
    }

    void *demux_handle = dtv_audio_instances->demux_handle[path_id];
    aml_demux_audiopara_t *demux_info = &dtv_audio_instances->demux_info[path_id];
    aml_dtvsync_t *dtvsync =  &dtv_audio_instances->dtvsync[path_id];
    val = val & ((1 << DVB_DEMUX_ID_BASE) - 1);
    switch (cmd) {
        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID:
            demux_info->media_sync_id = val;
            ALOGI("demux_info->media_sync_id  %d", demux_info->media_sync_id);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE:
            ALOGI("DTV sound mode %d ", val);
            demux_info->output_mode = val;
            if (patch && path_id == dtv_audio_instances->demux_index_working) {
                patch->mode = demux_info->output_mode;
            }
            adev->dtv_sound_mode = demux_info->output_mode;
            break;
        case AUDIO_DTV_PATCH_CMD_SET_MUTE:
            ALOGE ("Amlogic_HAL - %s: TV-Mute:%d.", __FUNCTION__,val);
            adev->tv_mute = val;
            break;
        case AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO:
            demux_info->has_video = val;
            ALOGI("has_video %d",demux_info->has_video);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO:
            demux_info->demux_id = val;
            ALOGI("demux_id %d",demux_info->demux_id);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL:
            demux_info->security_mem_level = val;
            ALOGI("security_mem_level set to %d\n", demux_info->security_mem_level);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_PID:
            demux_info->main_pid = val;
            ALOGI("main_pid %d",demux_info->main_pid);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_FMT:
            demux_info->main_fmt = val;
            ALOGI("main_fmt %d",demux_info->main_fmt);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_FMT:
            demux_info->ad_fmt = val;
            ALOGI("ad_fmt %d",demux_info->ad_fmt);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_PID:
            demux_info->ad_pid = val;
            ALOGI("ad_pid %d",demux_info->ad_pid);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT:

            adev->dual_decoder_support = val;
            demux_info->dual_decoder_support = val;
            ALOGI("dual_decoder_support set to %d\n", adev->dual_decoder_support);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE:

            if (val == 0) {
                dtv_assoc_audio_cache(-1);
            }
            if (adev->ad_switch_enable) {
                adev->associate_audio_mixing_enable = val;
            } else {
                adev->associate_audio_mixing_enable = 0;
           }
            demux_info->associate_audio_mixing_enable = adev->associate_audio_mixing_enable;
            ALOGI("associate_audio_mixing_enable set to %d\n", adev->associate_audio_mixing_enable);
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL:
            if (val < 0) {
                val = 0;
            } else if (val > 100) {
                val = 100;
            }
            adev->advol_level = val;
            ALOGI("madvol_level set to %d\n", adev->advol_level);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last && ms12->dolby_ms12_enable) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ad_vol(ms12, adev->advol_level);
                pthread_mutex_unlock(&ms12->lock);
            }
            break;
        case AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL:

            if (val < 0) {
                val = 0;
            } else if (val > 100) {
                val = 100;
            }
            adev->mixing_level = (val * 64 - 32 * 100) / 100; //[0,100] mapping to [-32,32]
            ALOGI("mixing_level set to %d\n", adev->mixing_level);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                dolby_ms12_set_user_control_value_for_mixing_main_and_associated_audio(adev->mixing_level);
                set_ms12_ad_mixing_level(ms12, adev->mixing_level);
                pthread_mutex_unlock(&ms12->lock);
            }

            break;
        case AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID:
            demux_info->media_presentation_id = val;
            ALOGI("media_presentation_id %d",demux_info->media_presentation_id);
            if (eDolbyMS12Lib == adev->dolby_lib_type_last) {
                pthread_mutex_lock(&ms12->lock);
                set_ms12_ac4_presentation_group_index(ms12, demux_info->media_presentation_id);
                pthread_mutex_unlock(&ms12->lock);
            }
            break;
        case AUDIO_DTV_PATCH_CMD_CONTROL:
            if (patch == NULL) {
                ALOGI("%s()the audio patch is NULL \n", __func__);
            }
            if (val <= AUDIO_DTV_PATCH_CMD_NULL || val > AUDIO_DTV_PATCH_CMD_NUM) {
                ALOGW("[%s:%d] Unsupported dtv patch cmd:%d", __func__, __LINE__, val);
                break;
            }
            ALOGI("[%s:%d] Send dtv patch cmd:%s", __func__, __LINE__, dtvAudioPatchCmd2Str(val));
            if (val == AUDIO_DTV_PATCH_CMD_OPEN) {
                if (adev->is_multi_demux) {
                    Open_Dmx_Audio(&demux_handle,demux_info->demux_id, demux_info->security_mem_level);
                    ALOGI("path_id %d demux_hanle %p ",path_id, demux_handle);
                    dtv_audio_instances->demux_handle[path_id] = demux_handle;
                    Init_Dmx_Main_Audio(demux_handle, demux_info->main_fmt, demux_info->main_pid);
                    if (demux_info->dual_decoder_support)
                        Init_Dmx_AD_Audio(demux_handle, demux_info->ad_fmt, demux_info->ad_pid);

                    Start_Dmx_Main_Audio(demux_handle);
                    if (adev->dual_decoder_support)
                        Start_Dmx_AD_Audio(demux_handle);
                    if (dtvsync->mediasync_new == NULL) {
                        dtvsync->mediasync_new = aml_dtvsync_create();
                        if (dtvsync->mediasync_new == NULL)
                            ALOGI("mediasync create failed\n");
                        else {
                            //Need to initialize pts when start play.
                            //For MS12 will out negative apts at begin, so initialize with big small number
                            dtvsync->cur_outapts = DTVSYNC_INIT_PTS;
                            dtvsync->out_start_apts = DTVSYNC_INIT_PTS;
                            dtvsync->out_end_apts = DTVSYNC_INIT_PTS;
                            dtvsync->mediasync_id = demux_info->media_sync_id;
                            ALOGI("path_id:%d,dtvsync media_sync_id=%d, init cur_outapts: %lld\n", path_id, dtvsync->mediasync_id, dtvsync->cur_outapts);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_ISOMXTUNNELMODE, &audio_sync_mode);
                            mediasync_wrap_bindInstance(dtvsync->mediasync_new, dtvsync->mediasync_id, MEDIA_AUDIO);
                            ALOGI("normal output version CMD open audio bind syncId:%d\n", dtvsync->mediasync_id);
                            mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_HASAUDIO, &has_audio);
                        }
                    }
                    ALOGI("create mediasync:%p\n", dtvsync->mediasync_new);
                } else {
                    /*int ret = uio_init(&patch->uio_fd);
                    if (ret < 0) {
                        ALOGI("uio init error! \n");
                        goto exit;
                    }*/
                }
            } else if (val == AUDIO_DTV_PATCH_CMD_CLOSE) {
                if (adev->is_multi_demux) {
                    if (demux_handle) {
                        Stop_Dmx_Main_Audio(demux_handle);
                        if (adev->dual_decoder_support)
                            Stop_Dmx_AD_Audio(demux_handle);
                        Destroy_Dmx_Main_Audio(demux_handle);
                        if (demux_info->dual_decoder_support)
                            Destroy_Dmx_AD_Audio(demux_handle);
                        Close_Dmx_Audio(demux_handle);
                        demux_handle = NULL;
                        dtv_audio_instances->demux_handle[path_id] = NULL;
                        if (dtvsync->mediasync_new != NULL) {
                            ALOGI("close mediasync_new:%p, mediasync:%p", dtvsync->mediasync_new, dtvsync->mediasync);
                            if (dtvsync->mediasync_new == dtvsync->mediasync) {
                                dtvsync->mediasync = NULL;
                            }
                            mediasync_wrap_destroy(dtvsync->mediasync_new);
                            dtvsync->mediasync_new = NULL;
                        }

                        if (dtvsync->mediasync != NULL) {
                            ALOGI("receive close cmd, release mediasync:%p\n", dtvsync->mediasync);
                            aml_dtvsync_release(dtvsync);
                            dtvsync->mediasync = NULL;
                        }
                    }
                } else {
                     //uio_deinit(&patch->uio_fd);
                }
                clean_dtv_demux_info(demux_info);
            } else {
                if (patch == NULL || patch->dtv_cmd_list == NULL) {
                    ALOGI("%s()the audio patch is NULL or dtv_cmd_list is NULL \n", __func__);
                    break;
                }
                if (val <= AUDIO_DTV_PATCH_CMD_NULL || val > AUDIO_DTV_PATCH_CMD_STOP) {
                    ALOGW("[%s:%d] Unsupported AUDIO_DTV_PATCH_CMD_CONTROL :%d", __func__, __LINE__, val);
                    break;
                }
                pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
                if (val == AUDIO_DTV_PATCH_CMD_START) {
                    dtv_audio_instances->demux_index_working = path_id;
                    patch->mode = adev->dtv_sound_mode;
                    patch->dtv_aformat = demux_info->main_fmt;
                    patch->media_sync_id = demux_info->media_sync_id;
                    patch->pid = demux_info->main_pid;
                    patch->demux_info = demux_info;
                    patch->dtv_has_video = demux_info->has_video;
                    patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
                    ALOGI("dtv_has_video %d",patch->dtv_has_video);
                    ALOGI("demux_index_working %d handle %p",dtv_audio_instances->demux_index_working, dtv_audio_instances->demux_handle[path_id]);
                }
                if (path_id == dtv_audio_instances->demux_index_working) {
                    if (val == AUDIO_DTV_PATCH_CMD_STOP) {
                        tv_do_ease_out(adev);
                    }
                    dtv_patch_add_cmd(patch->dtv_cmd_list, val, path_id);
                    pthread_cond_signal(&patch->dtv_cmd_process_cond);
                } else {
                    ALOGI("path_id %d not work ,cmd %d invalid",path_id, val);
                }
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
            }
            break;
        default:
            ALOGI("invalid cmd %d", cmd);
    }

    pthread_mutex_unlock(&adev->dtv_lock);
    return 0;
exit:
    pthread_mutex_unlock(&adev->dtv_lock);
    ALOGI("dtv_patch_handle_event failed ");
    return -1;
}
static int dtv_patch_status_info(void *args, INFO_TYPE_E info_flag)
{
    int ret = 0;
    struct aml_audio_patch *patch = (struct aml_audio_patch *)args;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    if (info_flag == BUFFER_SPACE)
        ret = get_buffer_write_space(ringbuffer);
    else if (info_flag == BUFFER_LEVEL)
        ret = get_buffer_read_space(ringbuffer);
    else if (info_flag == AD_MIXING_ENABLE) {
        ret = aml_dev->associate_audio_mixing_enable;
    } else if (info_flag == AD_MIXING_LEVLE)
        ret = aml_dev->mixing_level;
    else if (info_flag == AD_MIXING_PCMSCALE)
        ret = aml_dev->advol_level;
    else if (info_flag == SECURITY_MEM_LEVEL) {
        ret = aml_dev->security_mem_level;
    }
    return ret;
}

int dtv_patch_get_latency(struct aml_audio_device *aml_dev)
{
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    if (patch == NULL ) {
        ALOGI("dtv patch == NULL");
        return -1;
    }
    int latencyms = 0;
    int64_t last_queue_es_apts = 0;
    if (aml_dev->is_multi_demux) {
        if (Get_Audio_LastES_Apts(patch->demux_handle, &last_queue_es_apts) == 0) {
             ALOGI("last_queue_es_apts %lld",last_queue_es_apts);
             patch->last_chenkin_apts = last_queue_es_apts;
        }
        ALOGI("lastcheckinapts %d patch->cur_outapts %d ", patch->last_chenkin_apts, patch->cur_outapts);
        if (patch->last_chenkin_apts != 0xffffffff) {
            if (patch->skip_amadec_flag) {
                if (patch->dtvsync->cur_outapts > 0 && patch->last_chenkin_apts - patch->dtvsync->cur_outapts)
                    latencyms = (patch->last_chenkin_apts - patch->dtvsync->cur_outapts) / 90;
            } else {
                if (patch->cur_outapts > 0 && patch->last_chenkin_apts > patch->cur_outapts)
                    latencyms = (patch->last_chenkin_apts - patch->cur_outapts) / 90;
            }
        }
    } else {
        uint lastcheckinapts = 0;
        get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &lastcheckinapts);
        ALOGI("lastcheckinapts %d patch->cur_outapts %d", lastcheckinapts, patch->cur_outapts);
        patch->last_chenkin_apts = lastcheckinapts;
        if (patch->last_chenkin_apts != 0xffffffff) {
            if (patch->last_chenkin_apts > patch->cur_outapts && patch->cur_outapts > 0)
                latencyms = (patch->last_chenkin_apts - patch->cur_outapts) / 90;
        }

    }
    return latencyms;
}


static int dtv_patch_audio_info(void *args,unsigned char ori_channum,unsigned char lfepresent)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)args;
    patch->dtv_NchOriginal = ori_channum;
    patch->dtv_lfepresent = lfepresent;
    return 1;
}

static bool dtv_firstapts_lookup_over(struct aml_audio_patch *patch, struct aml_audio_device *aml_dev, bool a_discontinue, int *apts_diff)
{
    char buff[32];
    int ret;
    unsigned int first_checkinapts = 0xffffffff;
    unsigned int last_checkinapts = 0xffffffff;
    unsigned int last_checkinvpts = 0xffffffff;
    unsigned int first_checkinvpts = 0xffffffff;
    unsigned int cur_vpts = 0xffffffff;
    unsigned int first_vpts = 0xffffffff;
    unsigned int demux_pcr = 0xffffffff;
    unsigned int pcr_inited = 0;
    int first_checkin_av_diff = 0;
    int first_out_av_diff = 0;

    if (!patch || !aml_dev) {
        return true;
    }

    if (dtv_get_tsync_mode() == TSYNC_MODE_PCRMASTER && get_dtv_pcr_sync_mode() == 0) {
        ret = get_sysfs_uint(TSYNC_PCR_INITED, &pcr_inited);
        if (ret == 0 && pcr_inited != 0) {
            ALOGI("pcr_already inited=0x%x\n", pcr_inited);
        } else {
            ALOGI("ret = %d, pcr_inited=%x\n",ret, pcr_inited);
            return false;
        }
    }
    patch->tsync_mode = dtv_get_tsync_mode();
    get_sysfs_uint(TSYNC_PCRSCR, &demux_pcr);

    if (a_discontinue) {
        get_sysfs_uint(TSYNC_LAST_DISCONTINUE_CHECKIN_APTS, &first_checkinapts);
    } else {
        get_sysfs_uint(TSYNC_FIRSTCHECKIN_APTS, &first_checkinapts);
    }

    if (get_tsync_pcr_debug()) {
        get_sysfs_uint(TSYNC_FIRSTCHECKIN_VPTS, &first_checkinvpts);
        get_sysfs_uint(TSYNC_FIRST_VPTS, &first_vpts);
        get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkinapts);
        get_sysfs_uint(TSYNC_LAST_CHECKIN_VPTS, &last_checkinvpts);
        first_checkin_av_diff = (int)(first_checkinapts - first_checkinvpts) / 90;
        first_out_av_diff = (int)(first_checkinapts - first_vpts) / 90;
        ALOGI("demux_pcr %x first_checkinapts %x,last_checkinapts=%x,first_checkinvpts=%x,first_vpts:0x%x(has_video:%d),"
               " last_checkinvpts=%x, discontinue %d, apts_diff=%d, first_checkin_av_diff: %d ms, first_out_av_diff: %d ms",\
               demux_pcr, first_checkinapts, last_checkinapts, first_checkinvpts, first_vpts, patch->dtv_has_video,\
               last_checkinvpts, a_discontinue, *apts_diff, first_checkin_av_diff, first_out_av_diff);
    }

    if (dtv_get_tsync_mode() == TSYNC_MODE_AMASTER) {
       unsigned int videostarted = 0;
       struct timespec curtime;
       int costtime_ms = 0;
       int timeout = property_get_int32("vendor.media.audio.timecostms", 3000);

       clock_gettime(CLOCK_MONOTONIC, &curtime);
       costtime_ms = calc_time_interval_us(&start_time, &curtime) / 1000;
       get_sysfs_uint(TSYNC_VIDEO_STARTED, &videostarted);
       ALOGI("videostarted:%d , costtime:%d.", videostarted, costtime_ms);

       if (patch->dtv_has_video && videostarted == 0 && costtime_ms < timeout) {
           ALOGI("videostarted is 0.");
           return false;
       } else
           return true;
    }

    if ((first_checkinapts != 0xffffffff) && (demux_pcr != 0xffffffff)) {
        if (demux_pcr == 0 && first_checkinapts != 0 && last_checkinapts != 0) {
            ALOGI("demux pcr not set, wait, tsync_mode=%d, use_tsdemux_pcr=%d\n", dtv_get_tsync_mode(), get_dtv_pcr_sync_mode());
            return false;
        }
        if (first_checkinapts > demux_pcr) {
            unsigned diff = first_checkinapts - demux_pcr;
            if (diff < AUDIO_PTS_DISCONTINUE_THRESHOLD &&
                dtv_get_tsync_mode() == TSYNC_MODE_PCRMASTER) {
                //not return false in AMASTER mode
                return false;
            }
        } else {
            unsigned diff = demux_pcr - first_checkinapts;
            aml_dev->dtv_droppcm_size = diff * 48 * 2 * 2 / 90;
            ALOGI("now must drop size %d\n", aml_dev->dtv_droppcm_size);
        }
    }
    get_sysfs_uint(TSYNC_FIRSTCHECKIN_VPTS, &first_checkinvpts);
    get_sysfs_uint(TSYNC_FIRST_VPTS, &first_vpts);
    get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkinapts);
    get_sysfs_uint(TSYNC_LAST_CHECKIN_VPTS, &last_checkinvpts);
    get_sysfs_uint(TSYNC_VPTS, &cur_vpts);
    first_checkin_av_diff = (int)(first_checkinapts - first_checkinvpts) / 90;
    first_out_av_diff = (int)(first_checkinapts - first_vpts) / 90;
    ALOGI("++ demux_pcr %x first_checkinapts %x,last_checkinapts=%x,first_checkinvpts=%x,first_vpts:0x%x(has_video:%d),"
          " last_checkinvpts=%x,cur_vpts %x,discontinue %d,apts_diff=%d,first_checkin_av_diff: %d ms,first_out_av_diff: %d ms",\
          demux_pcr, first_checkinapts, last_checkinapts, first_checkinvpts, first_vpts, patch->dtv_has_video,\
          last_checkinvpts, cur_vpts, a_discontinue, *apts_diff, first_checkin_av_diff, first_out_av_diff);

    return true;
}

static int dtv_set_audio_latency(int apts_diff)
{
    int ret, diff = 0;
    char buff[32];

    /*[SE][BUG][SWPL-14828][chengshun.wang] add property
     * to set start latency
     */
    int audio_latency = DEMUX_PCR_APTS_LATENCY;
    int delay_ms = property_get_int32("vendor.media.audio.latencyms", 300);
    if (delay_ms * 90 > audio_latency) {
        audio_latency = delay_ms * 90;
    }

    if (apts_diff == 0) {
        ret = aml_sysfs_get_str(TSYNC_APTS_DIFF, buff, sizeof(buff));
        if (ret > 0) {
            ret = sscanf(buff, "%d\n", &diff);
        }
        if (diff > DECODER_PTS_DEFAULT_LATENCY) {
            diff = DECODER_PTS_DEFAULT_LATENCY;
        }
        apts_diff = diff;
    }
    ALOGI("dtv_set_audio_latency: audio_latency=%d, apts_diff=%d", audio_latency, apts_diff);

    if (apts_diff < audio_latency && apts_diff > 0) {
        decoder_set_latency(audio_latency - apts_diff);
    } else {
        decoder_set_latency(audio_latency);
    }
    return apts_diff;
}
static int dtv_write_mute_frame(struct aml_audio_patch *patch,
                                struct audio_stream_out *stream_out)
{
    unsigned char mixbuffer[EAC3_IEC61937_FRAME_SIZE];
    uint16_t *p16_mixbuff = NULL;
    int main_size = 0, mix_size = 0;
    int dd_bsmod = 0;
    int ret = 0, type = 0;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream_out;
    size_t output_buffer_bytes = 0;
    void *output_buffer = NULL;
#if 0
    struct timespec before_read;
    struct timespec after_read;
    int us = 0;
    clock_gettime(CLOCK_MONOTONIC, &before_read);
#endif
    //package iec61937
    memset(mixbuffer, 0, sizeof(mixbuffer));
    //papbpcpd
    p16_mixbuff = (uint16_t*)mixbuffer;
    p16_mixbuff[0] = 0xf872;
    p16_mixbuff[1] = 0x4e1f;
    if (patch->aformat == AUDIO_FORMAT_AC3) {
        dd_bsmod = 6;
        p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 1;
        p16_mixbuff[3] = (sizeof(mute_dd_frame) * 8);
    } else {
        dd_bsmod = 12;
        p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 21;
        p16_mixbuff[3] = sizeof(mute_ddp_frame) * 8;
    }
    mix_size += 8;
    if (patch->aformat == AUDIO_FORMAT_AC3) {
        memcpy(mixbuffer + mix_size, mute_dd_frame, sizeof(mute_dd_frame));
    } else {
        memcpy(mixbuffer + mix_size, mute_ddp_frame, sizeof(mute_ddp_frame));
    }
    if (aml_out->status != STREAM_HW_WRITING ||
        patch->output_thread_exit == 1) {
        ALOGE("dtv_write_mute_frame exit");
        return -1;
    }
    if (aml_dev->sink_format != AUDIO_FORMAT_PCM_16_BIT &&
        (patch->aformat == AUDIO_FORMAT_E_AC3 ||
          patch->aformat == AUDIO_FORMAT_AC4)) {
        memcpy(mixbuffer + mix_size, mute_ddp_frame, sizeof(mute_ddp_frame));
        type = 2;
    } else if (aml_dev->sink_format != AUDIO_FORMAT_PCM_16_BIT && patch->aformat == AUDIO_FORMAT_AC3) {
        memcpy(mixbuffer + mix_size, mute_dd_frame, sizeof(mute_dd_frame));
        type = 1;
    } else {
        type = 0;
    }
    /*for ms12 case, we always use pcm output do av sync*/
    if (eDolbyMS12Lib == aml_dev->dolby_lib_type_last) {
        type = 0;
    }
    if (type == 2) {
        audio_format_t output_format = AUDIO_FORMAT_IEC61937;
        size_t write_bytes = EAC3_IEC61937_FRAME_SIZE;
        //ALOGI("++aml_alsa_output_write E_AC3");
        if (audio_hal_data_processing(stream_out, (void*)mixbuffer, write_bytes, &output_buffer, &output_buffer_bytes, output_format) == 0) {
            output_format = AUDIO_FORMAT_E_AC3;
            hw_write(stream_out, output_buffer, output_buffer_bytes, output_format);
        }
        //ALOGI("--aml_alsa_output_write E_AC3");
    } else if (type == 1) {
        audio_format_t output_format = AUDIO_FORMAT_IEC61937;
        size_t write_bytes = AC3_IEC61937_FRAME_SIZE;
        //ALOGI("++aml_alsa_output_write AC3");
        if (audio_hal_data_processing(stream_out, (void*)mixbuffer, write_bytes, &output_buffer, &output_buffer_bytes, output_format) == 0) {
            output_format = AUDIO_FORMAT_AC3;
            hw_write(stream_out, output_buffer, output_buffer_bytes, output_format);
        }
        //ALOGI("--aml_alsa_output_write AC3");
    } else {
        audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
        size_t write_bytes = AC3_IEC61937_FRAME_SIZE;
        //ALOGI("++aml_alsa_output_write pcm");
        memset(mixbuffer, 0, sizeof(mixbuffer));
        if (audio_hal_data_processing(stream_out, (void*)mixbuffer, write_bytes, &output_buffer, &output_buffer_bytes, output_format) == 0) {
            hw_write(stream_out, output_buffer, output_buffer_bytes, output_format);
        }
        //ALOGI("--aml_alsa_output_write pcm");
    }
#if 0
    clock_gettime(CLOCK_MONOTONIC, &after_read);
    us = calc_time_interval_us(&before_read, &after_read);
    ALOGI("function gap =%d,sink %x,optional %x\n", us,aml_dev->sink_format,aml_dev->optical_format);
#endif
    return 0;
}

void dtv_audio_gap_monitor(struct aml_audio_patch *patch)
{
    char buff[32];
    unsigned int first_checkinapts = 0;
    int cur_pts_diff = 0;
    int audio_discontinue = 0;
    int ret;
    unsigned int cur_vpts;
    unsigned int tmp_pcrpts;
    unsigned int demux_apts;
    if (!patch) {
        return;
    }
    /*[SE][BUG][OTT-7302][zhizhong.zhang] detect audio discontinue by pts-diff*/
    if (patch->dtv_has_video &&
        (patch->last_apts != 0  && patch->last_apts != (unsigned long) - 1) &&
        (patch->last_pcrpts != 0  && patch->last_pcrpts != (unsigned long) - 1)) {
        cur_pts_diff = patch->last_pcrpts - patch->last_apts;
        if (audio_discontinue == 0 &&
            abs(cur_pts_diff) > DTV_PTS_CORRECTION_THRESHOLD * 5 &&
            get_video_discontinue() != 1 && !dtv_avsync_audio_freerun(patch)) {
            audio_discontinue = 1;
            get_sysfs_uint(TSYNC_CHECKIN_APTS, &demux_apts);
            ALOGI("cur_pts_diff=%d, diff=%d ms, apts=0x%x, pcrpts=0x%x, demux_apts=0x%x\n",
                cur_pts_diff, cur_pts_diff/90, patch->last_apts,
                patch->last_pcrpts, demux_apts);
        } else
            audio_discontinue = 0;
    }
    if ((audio_discontinue || get_audio_discontinue()) &&
        patch->dtv_audio_tune == AUDIO_RUNNING) {
        //ALOGI("%s size %d", __FUNCTION__, get_buffer_read_space(&(patch->aml_ringbuffer)));
        ret = aml_sysfs_get_str(TSYNC_LAST_DISCONTINUE_CHECKIN_APTS, buff, sizeof(buff));
        if (ret > 0) {
            ret = sscanf(buff, "0x%x\n", &first_checkinapts);
        }
        if (first_checkinapts) {
            patch->dtv_audio_tune = AUDIO_BREAK;
            ALOGI("audio discontinue:%d, first_checkinapts:0x%x tune -> AUDIO_BREAK",
                get_audio_discontinue(), first_checkinapts);
        } else if (audio_discontinue == 1) {
            patch->dtv_audio_tune = AUDIO_BREAK;
            get_sysfs_uint(TSYNC_VPTS, &cur_vpts);
            if (get_dtv_pcr_sync_mode() == 0 &&
                (patch->last_pcrpts > cur_vpts + DTV_PCR_DIS_DIFF_THRESHOLD) &&
                (patch->last_pcrpts > patch->last_apts + DTV_PCR_DIS_DIFF_THRESHOLD)) {
                tmp_pcrpts = MAX(cur_vpts, patch->last_apts);
                decoder_set_pcrsrc(tmp_pcrpts);
                ALOGI("cur_vpts=0x%x, last_apts=0x%x, cur_pcr=0x%x, tmp_pcr=0x%x\n",
                    cur_vpts, patch->last_apts, patch->last_pcrpts, tmp_pcrpts);
            }
            ALOGI("audio_discontinue set 1, tune -> AUDIO_BREAK\n");
        }
    }
}

/*+[SE][BUG][SWPL-14811][zhizhong] add ac3/e-ac3 pcm drop function*/
static int dtv_do_drop_ac3_pcm(struct aml_audio_patch *patch,
            struct audio_stream_out *stream_out)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    struct aml_audio_device *adev = aml_out->dev;
    size_t frame_size = 0;
    switch (adev->sink_format) {
    case AUDIO_FORMAT_E_AC3:
        if (eDolbyDcvLib == adev->dolby_lib_type)
            frame_size = AUDIO_AC3_FRAME_SIZE;
        else
            frame_size = AUDIO_EAC3_FRAME_SIZE;
        break;
    case AUDIO_FORMAT_AC3:
        frame_size = AUDIO_AC3_FRAME_SIZE;
        break;
    default:
        frame_size = (aml_out->is_tv_platform == true) ? AUDIO_TV_PCM_FRAME_SIZE : AUDIO_DEFAULT_PCM_FRAME_SIZE;
        break;
    }
    if (patch->dtv_apts_lookup > AUDIO_PTS_DISCONTINUE_THRESHOLD) {
        ALOGI("dtv_apts_looup = 0x%x > 5s,force set 5s \n", patch->dtv_apts_lookup);
        patch->dtv_apts_lookup = AUDIO_PTS_DISCONTINUE_THRESHOLD;
    }
    aml_out->need_drop_size = (patch->dtv_apts_lookup / 90) * 48 * frame_size;
    aml_out->need_drop_size &= ~(frame_size - 1);
    ALOGI("dtv_do_drop need_drop_size=%d,frame_size=%zu\n",
        aml_out->need_drop_size, frame_size);
    return 0;
}

static void dtv_do_drop_insert_ac3(struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_stream_out *out = (struct aml_stream_out *)stream_out;
    int fm_size;
    int drop_size, t1, t2;
    int write_used_ms = 0;
    unsigned int cur_pts = 0;
    unsigned int cur_pcr = 0;
    int ap_diff = 0;
    int write_times = 0;
    struct timespec before_write;
    struct timespec after_write;
    if (!patch || !patch->dev || !stream_out || aml_dev->dev2mix_patch == 1) {
        return;
    }
    if (patch->dtv_apts_lookup > 0 && patch->ac3_pcm_dropping != 1) {
        patch->ac3_pcm_dropping = 1;
        dtv_do_drop_ac3_pcm(patch, stream_out);
    } else if (patch->dtv_apts_lookup < 0) {
        if (abs(patch->dtv_apts_lookup) / 90 > 1000) {
            t1 = 1000;
        } else {
            t1 =  abs(patch->dtv_apts_lookup) / 90;
        }
        t2 = t1 / 32;
        ALOGI("dtv_do_insert:++insert lookup %d,diff(t1) %d ms\n", patch->dtv_apts_lookup, t1);
        t1 = 0;
        clock_gettime(CLOCK_MONOTONIC, &before_write);
        while (t1 == 0 && t2 > 0) {
            write_times++;
            t1 = dtv_write_mute_frame(patch, stream_out);
            usleep(5000);
            cur_pts = patch->last_apts;
            get_sysfs_uint(TSYNC_PCRSCR, (unsigned int *) & (cur_pcr));
            ap_diff = cur_pts - cur_pcr;
            ALOGI("cur_pts=0x%x, cur_pcr=0x%x,ap_diff=%d\n", cur_pts, cur_pcr, ap_diff);
            if (ap_diff < 90*10) {
                ALOGI("write mute pcm enough, write_times=%d break\n", write_times);
                break;
            }
            t2--;
            clock_gettime(CLOCK_MONOTONIC, &after_write);
            write_used_ms = calc_time_interval_us(&before_write, &after_write)/1000;
            ALOGI("write_used_ms = %d, t1 = %d, t2 = %d\n", write_used_ms, t1, t2);
            if (write_used_ms > 1000) {
                ALOGI("Warning write cost over 1s, break\n");
                break;
            }
        }
    }
    ALOGI("dtv_do_drop_insert done\n");
}

int dtv_get_tsync_mode(void)
{
    char tsync_mode_str[PROP_VALUE_MAX / 3];
    char buf[PROP_VALUE_MAX];
    int tsync_mode;
    if (sysfs_get_sysfs_str(DTV_DECODER_TSYNC_MODE, buf, sizeof(buf)) == -1) {
        ALOGI("++ dtv_get_tsync_mode fail. ");
        return -1;
    }
    //ALOGI("dtv_get_tsync_mode syncmode buf:%s.", buf);
    if (sscanf(buf, "%d: %s", &tsync_mode, tsync_mode_str) < 1) {
        ALOGI("+- dtv_get_tsync_mode fail. ");
        return -1;
    }

    return tsync_mode;
}

static int do_audio_resample(int* ratio)
{
    int need_resample = 0;

    if (pcr_apts_diff > AUDIO_RESAMPLE_MIN_THRESHOLD) {
        need_resample = 1;
        if (pcr_apts_diff > AUDIO_RESAMPLE_MAX_THRESHOLD)
            *ratio = 120;
        else if (pcr_apts_diff > AUDIO_RESAMPLE_MIDDLE_THRESHOLD)
            *ratio = 110;
        else
            *ratio = 105;
    } else if (pcr_apts_diff < -AUDIO_RESAMPLE_MIN_THRESHOLD) {
        need_resample = 1;
        if (pcr_apts_diff < -AUDIO_RESAMPLE_MAX_THRESHOLD)
            *ratio = 80;
        else if (pcr_apts_diff < -AUDIO_RESAMPLE_MIDDLE_THRESHOLD)
            *ratio = 90;
        else
            *ratio = 95;
   }
   if (!(dtv_get_tsync_mode() == TSYNC_MODE_PCRMASTER && get_dtv_pcr_sync_mode()))
       need_resample = 0;

   return need_resample;
}

static int dtv_patch_pcm_write(unsigned char *pcm_data, int size,
                               int symbolrate, int channel, int data_width, void *args)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)args;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    int left, need_resample;
    int write_size, return_size;
    unsigned char *write_buf;
    int16_t tmpbuf[OUTPUT_BUFFER_SIZE];
    char info_buf[MAX_BUFF_LEN] = {0};
    int valid_paramters = 1;
    int ratio = 100;
    int enable_audio_resample = property_get_int32(PROPERTY_ENABLE_AUDIO_RESAMPLE, 0);

    write_buf = pcm_data;
    if (pcm_data == NULL || size == 0) {
        return 0;
    }
    /*[SE][BUG][SWPL-22109][zhizhong] for only inserting case, no need to write*/
    if (patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_INIT ||
        (patch->dtv_audio_tune == AUDIO_DROP && patch->pcm_inserting)) {
        return 0;
    }
    patch->sample_rate = symbolrate;
    // In the case of fast switching channels such as mpeg/dra/..., there may be an
    // error "symbolrate" and "channel" paramters, so add the check to avoid it.
    if (symbolrate > 96000 || symbolrate < 8000) {
        valid_paramters = 0;
    }
    if (channel > 8 || channel < 1) {
        valid_paramters = 0;
    }
    patch->chanmask = channel;
    if (patch->sample_rate != 48000) {
        need_resample = 1;
    } else if (enable_audio_resample){
        need_resample = do_audio_resample(&ratio);
    } else
        need_resample = 0;
    ALOGV("output pcr_apts_diff:%d , ratio:%d, need_resample:%d.", pcr_apts_diff,  ratio, need_resample);
    left = get_buffer_write_space(ringbuffer);

    if (left <= 0) {
        return 0;
    }
    if (need_resample == 0 && patch->chanmask == 1) {
        if (left >= 2 * size) {
            write_size = size;
        } else {
            write_size = left / 2;
        }
    } else if (need_resample == 1 && patch->chanmask == 2) {
        if (left >= size * 48000 / patch->sample_rate) {
            write_size = size;
        } else {
            return 0;
        }

    } else if (need_resample == 1 && patch->chanmask == 1) {
        if (left >= 2 * size * 48000 / patch->sample_rate) {
            write_size = size;
        } else {
            return 0;
        }
    } else {
        if (left >= size) {
            write_size = size;
        } else {
            write_size = left;
        }
    }

    return_size = write_size;
    if ((patch->aformat != AUDIO_FORMAT_E_AC3 &&
         patch->aformat != AUDIO_FORMAT_AC3 &&
         patch->aformat != AUDIO_FORMAT_DTS) && valid_paramters) {
        if (patch->chanmask == 1) {
            int16_t *buf = (int16_t *)write_buf;
            int i = 0, samples_num;
            samples_num = write_size / (patch->chanmask * sizeof(int16_t));
            for (; i < samples_num; i++) {
                tmpbuf[2 * (samples_num - i) - 1] = buf[samples_num - i - 1];
                tmpbuf[2 * (samples_num - i) - 2] = buf[samples_num - i - 1];
            }
            write_size = write_size * 2;
            write_buf = (unsigned char *)tmpbuf;
            if (write_size > left || write_size > (OUTPUT_BUFFER_SIZE * 2)) {
                ALOGI("resample, channel, write_size %d, left %d", write_size, left);
                write_size = ((left) < (OUTPUT_BUFFER_SIZE * 2)) ? (left) : (OUTPUT_BUFFER_SIZE * 2);
            }
        }
        if (need_resample == 1) {
            if (patch->dtv_resample.input_sr != (unsigned int)patch->sample_rate) {
                patch->dtv_resample.input_sr = patch->sample_rate;
                patch->dtv_resample.output_sr = 48000;
                patch->dtv_resample.channels = 2;
                resampler_init(&patch->dtv_resample);
            } else if (enable_audio_resample) {
                patch->dtv_resample.output_sr = 48000 * 100 / ratio;
                patch->dtv_resample.channels = 2;
                resampler_init(&patch->dtv_resample);
                ALOGI("output sr:%u.", patch->dtv_resample.output_sr);
            }
            if (!patch->resample_outbuf) {
                patch->resample_outbuf =
                    (unsigned char *)aml_audio_malloc(OUTPUT_BUFFER_SIZE * 3);
                if (!patch->resample_outbuf) {
                    ALOGE("malloc buffer failed\n");
                    return -1;
                }
                memset(patch->resample_outbuf, 0, OUTPUT_BUFFER_SIZE * 3);
            }
            int out_frame = write_size >> 2;
            out_frame = resample_process(&patch->dtv_resample, out_frame,
                                         (int16_t *)write_buf,
                                         (int16_t *)patch->resample_outbuf);
            write_size = out_frame << 2;
            write_buf = patch->resample_outbuf;
            if (write_size > left || write_size > (OUTPUT_BUFFER_SIZE * 3)) {
                ALOGI("resample, process, write_size %d, left %d", write_size, left);
                write_size = ((left) < (OUTPUT_BUFFER_SIZE * 3)) ? (left) : (OUTPUT_BUFFER_SIZE * 3);
            }
        }
    }
    pthread_mutex_lock(&patch->apts_cal_mutex);
    ring_buffer_write(ringbuffer, (unsigned char *)write_buf, write_size,
                      UNCOVER_WRITE);
    pthread_mutex_unlock(&patch->apts_cal_mutex);

    // if ((patch->aformat != AUDIO_FORMAT_E_AC3)
    //     && (patch->aformat != AUDIO_FORMAT_AC3) &&
    //     (patch->aformat != AUDIO_FORMAT_DTS)) {
    //     int abuf_level = get_buffer_read_space(ringbuffer);
    //     process_pts_sync(0, patch, 0);
    // }

    if (aml_getprop_bool("vendor.media.audiohal.outdump")) {
        aml_audio_dump_audio_bitstreams("/data/audio/audio_dtv.pcm",
            write_buf, write_size);
    }
    patch->dtv_pcm_writed += return_size;
    //ALOGI("[%s]ring_buffer_write now wirte %d to ringbuffer\
    //now, total:%d.\n", __FUNCTION__, write_size);
    if (patch->aformat != AUDIO_FORMAT_E_AC3 &&
         patch->aformat != AUDIO_FORMAT_AC3 &&
         patch->aformat != AUDIO_FORMAT_DTS && (channel != 0) && (data_width != 0)) {
            patch->numDecodedSamples = patch->dtv_pcm_writed * 8 / (channel * data_width);
            sprintf(info_buf, "decoded_frames %d", patch->numDecodedSamples);
            //sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
    }
    pthread_cond_signal(&patch->cond);
    return return_size;
}

static int dtv_patch_raw_wirte(unsigned char *raw_data, int size, void *args)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)args;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    int left;
    int write_size;
    if (raw_data == NULL) {
        return 0;
    }

    if (size == 0) {
        return 0;
    }

    left = get_buffer_write_space(ringbuffer);
    if (left > size) {
        write_size = size;
    } else {
        write_size = left;
    }

    ring_buffer_write(ringbuffer, (unsigned char *)raw_data, write_size,
                      UNCOVER_WRITE);
    return write_size;
}

extern int do_output_standby_l(struct audio_stream *stream);
extern void adev_close_output_stream_new(struct audio_hw_device *dev,
        struct audio_stream_out *stream);
extern int adev_open_output_stream_new(struct audio_hw_device *dev,
                                       audio_io_handle_t handle __unused,
                                       audio_devices_t devices,
                                       audio_output_flags_t flags,
                                       struct audio_config *config,
                                       struct audio_stream_out **stream_out,
                                       const char *address __unused);
ssize_t out_write_new(struct audio_stream_out *stream, const void *buffer,
                      size_t bytes);
void audio_dtv_underrun_loop_mute_check(struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out)
{
    struct snd_pcm_status status;
    int audio_loopback_mute = 0;
    bool underrun_flag = false;
    int mutetime_ms = 0;
    int underrun_ms = 0;
    unsigned int checkin_discontinue_apts = 0xffffffff;
    unsigned int last_checkinapts = 0xffffffff;
    struct timespec cur_time;
    struct audio_hw_device *dev = patch->dev;
    struct avsync_para *dtv_sync_para = &(patch->sync_para);
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    if (patch->dtv_first_apts_flag == 1 && aml_out->pcm)
        pcm_ioctl(aml_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
    else
        return;

    if (status.state == PCM_STATE_XRUN)
        underrun_flag = true;
    else
        underrun_flag = false;
    get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkinapts);
    dtv_sync_para->checkin_underrun_flag = get_audio_checkin_underrun();
    if (aml_dev->debug_flag && dtv_sync_para->checkin_underrun_flag)
        ALOGI("checkin_underrun_flag=%d, underrun_flag=%d\n",
            dtv_sync_para->checkin_underrun_flag, underrun_flag);

    if (dtv_sync_para->checkin_underrun_flag && underrun_flag &&
        aml_dev->sink_format == AUDIO_FORMAT_PCM_16_BIT &&
        dtv_sync_para->in_out_underrun_flag == 0)  {
        dtv_sync_para->in_out_underrun_flag = 1;
        dtv_sync_para->underrun_checkinpts = last_checkinapts;
        clock_gettime(CLOCK_MONOTONIC, &(dtv_sync_para->underrun_starttime));
        return;
    }

    if (dtv_sync_para->in_out_underrun_flag == 1) {
        if (aml_dev->underrun_mute_flag == 0 &&
            last_checkinapts != dtv_sync_para->underrun_checkinpts) {
            aml_dev->underrun_mute_flag = 1;
            ALOGI("set underrun_mute_flag 1\n");
            clock_gettime(CLOCK_MONOTONIC, &(dtv_sync_para->underrun_mute_starttime));
            return;
        }
        clock_gettime(CLOCK_MONOTONIC, &cur_time);

        if (aml_dev->underrun_mute_flag) {

            mutetime_ms = calc_time_interval_us(&(dtv_sync_para->underrun_mute_starttime),
                                                &cur_time) / 1000;
            if (dtv_patch_get_audio_loop()) {
                checkin_discontinue_apts = dtv_patch_get_checkin_dicontinue_apts();
                if ((patch->cur_outapts >= checkin_discontinue_apts &&
                    (patch->cur_outapts - checkin_discontinue_apts) < 3000*90) ||
                    mutetime_ms > dtv_sync_para->underrun_mute_time_max) {
                    ALOGI("clear underrun_mute_flag, mute_time:%d\n", mutetime_ms);
                    aml_dev->underrun_mute_flag = 0;
                    dtv_sync_para->in_out_underrun_flag = 0;
                    dtv_patch_clear_audio_loop();
                }
            } else {

                if (mutetime_ms > dtv_sync_para->underrun_mute_time_min) {
                    ALOGI("no loop happen clear underrun_mute_flag\n");
                    aml_dev->underrun_mute_flag = 0;
                    dtv_sync_para->in_out_underrun_flag = 0;
                }
            }
        } else {
            underrun_ms = calc_time_interval_us(&(dtv_sync_para->underrun_starttime),
                                &cur_time) / 1000;
            if (underrun_ms > dtv_sync_para->underrun_max_time) {
                aml_dev->underrun_mute_flag = 0;
                dtv_sync_para->in_out_underrun_flag = 0;
                ALOGI("underrun_ms=%d,but not discontinue,clear\n", underrun_ms);
            }
        }
    }
}


int audio_dtv_patch_output_default(struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out, int *apts_diff)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    //int apts_diff = 0;
    int ret = 0;

    char buff[32];
    int write_len;
    struct aml_stream_out *aml_out;
    aml_out = (struct aml_stream_out *)stream_out;
    int avail = get_buffer_read_space(ringbuffer);
    if (avail >= (int)patch->out_buf_size) {
        write_len = (int)patch->out_buf_size;
        if (!patch->first_apts_lookup_over) {
            *apts_diff = dtv_set_audio_latency(0);
            if (!dtv_firstapts_lookup_over(patch, aml_dev, false, apts_diff) || avail < 48 * 4 * 50) {
                ALOGI("[%d]hold the aduio for cache data, avail %d", __LINE__, avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            patch->first_apts_lookup_over = 1;
            ALOGI("[audiohal_kpi][%s,%d] dtv_audio_tune %d-> AUDIO_LOOKUP\n",
                     __FUNCTION__, __LINE__, patch->dtv_audio_tune);
            patch->dtv_audio_tune = AUDIO_LOOKUP;
            //ALOGI("dtv_audio_tune audio_lookup\n");
            clean_dtv_patch_pts(patch);
            patch->out_buf_size = aml_out->config.period_size * audio_stream_out_frame_size(&aml_out->stream);
        } else if (patch->dtv_audio_tune == AUDIO_BREAK) {
            int a_discontinue = get_audio_discontinue();
            //dtv_set_audio_latency(*apts_diff);
            if (!dtv_firstapts_lookup_over(patch, aml_dev, true, apts_diff) && !a_discontinue) {
                ALOGI("[%d]hold the aduio for cache data, avail %d", __LINE__, avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            if (a_discontinue  == 0) {
                ALOGI("[%s,%d] dtv_audio_tune AUDIO_BREAK-> AUDIO_LOOKUP\n",
                     __FUNCTION__, __LINE__);
                patch->dtv_audio_tune = AUDIO_LOOKUP;
                //ALOGI("dtv_audio_tune audio_lookup\n");
                clean_dtv_patch_pts(patch);
            }
        } else if (patch->dtv_audio_tune == AUDIO_DROP) {
            dtv_do_process_pcm(avail, patch, stream_out);
        }
        ret = ring_buffer_read(ringbuffer, (unsigned char *)patch->out_buf, write_len);
        if (ret == 0) {
            pthread_mutex_unlock(&(patch->dtv_output_mutex));
            usleep(1000);
            /*ALOGE("%s(), live ring_buffer read 0 data!", __func__);*/
            return -EAGAIN;
        }

        if (aml_out->hal_internal_format != patch->aformat) {
            aml_out->hal_format = aml_out->hal_internal_format = patch->aformat;
            get_sink_format(stream_out);
        }
        ret = out_write_new(stream_out, patch->out_buf, ret);
        patch->dtv_pcm_readed += ret;
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    } else {
        dtv_audio_gap_monitor(patch);
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
        usleep(1000);
    }

    return ret;
}

bool is_need_check_ad_substream(struct aml_audio_patch *patch) {
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    bool    is_need_check_ad_substream =  (eDolbyDcvLib == aml_dev->dolby_lib_type &&
                                           ( patch->aformat == AUDIO_FORMAT_E_AC3 ||
                                             patch->aformat == AUDIO_FORMAT_AC3 ) &&
                                           !patch->ad_substream_checked_flag);
    return is_need_check_ad_substream;

}
int audio_dtv_patch_output_dolby(struct aml_audio_patch *patch,
                        struct audio_stream_out *stream_out, int *apts_diff)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    int ret = 0;

    int consume_size = 0,remain_size = 0,ms12_thredhold_size = 256;
    char buff[32];
    int write_len, cur_frame_size = 0;
    unsigned long long all_pcm_len1 = 0;
    unsigned long long all_pcm_len2 = 0;
    unsigned long long all_zero_len = 0;
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_out->aml_dec;
    int avail = get_buffer_read_space(ringbuffer);
    if (aml_dev->is_multi_demux)
        audio_dtv_underrun_loop_mute_check(patch, stream_out);

    if (avail > 0) {
        if (avail > (int)patch->out_buf_size) {
            write_len = (int)patch->out_buf_size;
            if (write_len > 512) {
                write_len = 512;
            }
        } else {
            write_len = 512;
        }
        if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
            if (patch->aformat == AUDIO_FORMAT_AC4) {
                write_len = 512;
            } else {
                if (aml_out->ddp_frame_size != 0) {
                    write_len = aml_out->ddp_frame_size;
                }
            }

        } else if (eDolbyDcvLib == aml_dev->dolby_lib_type) {
            if(ddp_dec) {
                if (ddp_dec->curFrmSize != 0) {
                    write_len = ddp_dec->curFrmSize;
                }
            }
        }

        if (is_need_check_ad_substream (patch)) {
            write_len = ADUIO_DOLBY_ADSUBFRAME_CHECKED_SIZE;
        }
        if (!patch->first_apts_lookup_over) {
            *apts_diff = dtv_set_audio_latency(0);
            if (!dtv_firstapts_lookup_over(patch, aml_dev, false, apts_diff) || avail < 512 * 2) {
                ALOGI("hold the aduio for cache data, avail %d", avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            patch->first_apts_lookup_over = 1;
            ALOGI("[audiohal_kpi][%s,%d] dtv_audio_tune %d-> AUDIO_LOOKUP\n",
                    __FUNCTION__, __LINE__, patch->dtv_audio_tune);
            patch->dtv_audio_tune = AUDIO_LOOKUP;
            clean_dtv_patch_pts(patch);
            //ALOGI("dtv_audio_tune audio_lookup\n");
        } else if (patch->dtv_audio_tune == AUDIO_BREAK) {
            int a_discontinue = get_audio_discontinue();
            int cur_diff = 0;
            /*[SE][BUG][SWPL-26555][chengshun] ms12 happen underrun,if ms12 lib fix it, remove it*/
            if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
                cur_diff = *apts_diff / 2;
            } else {
                cur_diff = *apts_diff;
            }
            dtv_set_audio_latency(cur_diff);
            if (!dtv_firstapts_lookup_over(patch, aml_dev, true, &cur_diff) && !a_discontinue) {
                ALOGI("hold the aduio for cache data, avail %d", avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            if (a_discontinue  == 0) {
                ALOGI("[%s,%d] dtv_audio_tune AUDIO_BREAK-> AUDIO_LOOKUP\n",
                     __FUNCTION__, __LINE__);
                patch->dtv_audio_tune = AUDIO_LOOKUP;
                clean_dtv_patch_pts(patch);
            } else {
                ALOGI("audio still discontinue, not change lookup\n");
            }
            //ALOGI("dtv_audio_tune audio_lookup\n");
        } else if (patch->dtv_audio_tune == AUDIO_DROP) {
            dtv_do_drop_insert_ac3(patch, stream_out);
            if (patch->dtv_apts_lookup < 0 ||
                (patch->dtv_apts_lookup > 0 &&
                aml_out->need_drop_size == 0)) {
                clean_dtv_patch_pts(patch);
                patch->dtv_apts_lookup = 0;
                patch->ac3_pcm_dropping = 0;
                ALOGI("[%s,%d] dtv_audio_tune AUDIO_DROP-> AUDIO_LATENCY\n", __FUNCTION__, __LINE__);
                patch->dtv_audio_tune = AUDIO_LATENCY;
                ALOGI("dtv_audio_tune ac3 audio_latency\n");
            }
        }

        if (is_need_check_ad_substream (patch)) {
               while (get_buffer_read_space(ringbuffer) < write_len) {
                   usleep(20000);
               }
               ret = ring_buffer_read(ringbuffer, (unsigned char *)patch->out_buf, write_len);
               aml_out->ad_substream_supported = is_ad_substream_supported(patch->out_buf,write_len);
               ALOGI("ad_substream_supported %d",aml_out->ad_substream_supported);
               patch->ad_substream_checked_flag = true;
        } else {
             ret = ring_buffer_read(ringbuffer, (unsigned char *)patch->out_buf, write_len);
        }

        if (ret == 0) {
            pthread_mutex_unlock(&(patch->dtv_output_mutex));
            /*ALOGE("%s(), ring_buffer read 0 data!", __func__);*/
            usleep(1000);
            return -EAGAIN;
        }
        {
            if (aml_out->hal_internal_format != patch->aformat) {
                aml_out->hal_format = aml_out->hal_internal_format = patch->aformat;
                get_sink_format(stream_out);
            }
        }
        if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
            consume_size = dolby_ms12_get_main_bytes_consumed(stream_out);
            consume_size  = consume_size > ms12_thredhold_size ? consume_size - ms12_thredhold_size : 0;
            if (is_bypass_dolbyms12(stream_out))
                all_pcm_len1 = aml_out->frame_write_sum * AUDIO_IEC61937_FRAME_SIZE;
            else
                dolby_ms12_get_pcm_output_size(&all_pcm_len1, &all_zero_len);
        }

        /* +[SE] [BUG][SWPL-22893][yinli.xia]
              add: reset decode data when replay video*/
        if (patch->dtv_replay_flag) {
            remain_size = 0;
            patch->dtv_replay_flag = false;
        }
        ret = out_write_new(stream_out, patch->out_buf, ret);

        if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
            int size = dolby_ms12_get_main_bytes_consumed(stream_out);
            size  = size > ms12_thredhold_size ? size - ms12_thredhold_size : 0;
            dolby_ms12_get_pcm_output_size(&all_pcm_len2, &all_zero_len);
            if (is_bypass_dolbyms12(stream_out)) {
                patch->decoder_offset += ret;
                all_pcm_len2 = aml_out->frame_write_sum * AUDIO_IEC61937_FRAME_SIZE;
            } else
                patch->decoder_offset += size - consume_size;
            patch->outlen_after_last_validpts += (unsigned int)(all_pcm_len2 - all_pcm_len1);
            ALOGV("consume_size %d,size %d,ret %d,validpts %d patch->decoder_offset %d",consume_size,size,ret,patch->outlen_after_last_validpts,patch->decoder_offset);
            patch->dtv_pcm_readed += ret;
        }

        if (aml_dev->debug_flag) {
            if (ddp_dec)
                ALOGI("after decode: decode_offset: %d, ddp.remain_size=%d\n",
                   patch->decoder_offset, ddp_dec->remain_size);
        }
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    } else {
        dtv_audio_gap_monitor(patch);
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
        usleep(1000);
    }

    return ret;
}


int audio_dtv_patch_output_dts(struct aml_audio_patch *patch, struct audio_stream_out *stream_out)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    struct dca_dts_dec *dtshd = (struct dca_dts_dec *)aml_out->aml_dec;
    int apts_diff = 0;
    int ret = 0;

    int remain_size = 0;
    int avail = get_buffer_read_space(ringbuffer);
    if (avail > 0) {
        if (avail > (int)patch->out_buf_size) {
            avail = (int)patch->out_buf_size;
            if (avail > 1024) {
                avail = 1024;
            }
        } else {
            avail = 1024;
        }
        if (!patch->first_apts_lookup_over) {
            apts_diff = dtv_set_audio_latency(0);

            if (!dtv_firstapts_lookup_over(patch, aml_dev, false, &apts_diff) || avail < 512 * 2) {
                ALOGI("hold the aduio for cache data, avail %d", avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            patch->first_apts_lookup_over = 1;
            ALOGI("[audiohal_kpi][%s,%d] dtv_audio_tune %d-> AUDIO_LOOKUP\n",
                     __FUNCTION__, __LINE__, patch->dtv_audio_tune);
        }
        ret = ring_buffer_read(ringbuffer, (unsigned char *)patch->out_buf,
                               avail);
        if (ret == 0) {
            pthread_mutex_unlock(&(patch->dtv_output_mutex));
            usleep(1000);
            /*ALOGE("%s(), live ring_buffer read 0 data!", __func__);*/
            return -EAGAIN;
        }
        if (dtshd)
            remain_size = dtshd->remain_size;

        /* +[SE] [BUG][SWPL-22893][yinli.xia]
              add: reset decode data when replay video*/
        if (patch->dtv_replay_flag) {
            remain_size = 0;
            patch->dtv_replay_flag = false;
        }

        ret = out_write_new(stream_out, patch->out_buf, ret);
        if (dtshd) {
            patch->outlen_after_last_validpts += dtshd->outlen_pcm;

            patch->decoder_offset +=
                remain_size + ret - dtshd->remain_size;
            patch->dtv_pcm_readed += ret;

        }

        if (aml_dev->debug_flag) {
            ALOGI("after decode: dtshd->=%d\n", dtshd->remain_size);
        }
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    } else {
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
        usleep(5000);
    }

    return ret;
}


int audio_dtv_patch_output_dolby_dual_decoder(struct aml_audio_patch *patch,
                                         struct audio_stream_out *stream_out, int *apts_diff)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    struct mAudioEsDataInfo *mEsData = patch->mADEsData;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_out->aml_dec;
    //int apts_diff = 0;

    unsigned char main_head[32];
    unsigned char ad_head[32];
    int main_frame_size = 0, last_main_frame_size = 0, main_head_offset = 0, main_head_left = 0;
    int las_ad_frame_size = 0;
    int ad_frame_size = 0, ad_head_offset = 0, ad_head_left = 0;
    unsigned char mixbuffer[EAC3_IEC61937_FRAME_SIZE];
    unsigned char ad_buffer[EAC3_IEC61937_FRAME_SIZE];
    uint16_t *p16_mixbuff = NULL;
    uint32_t *p32_mixbuff = NULL;
    int main_size = 0, ad_size = 0, mix_size = 0;
    int dd_bsmod = 0, remain_size = 0;
    unsigned long long all_pcm_len1 = 0;
    unsigned long long all_pcm_len2 = 0;
    unsigned long long all_zero_len = 0;
    int main_avail = get_buffer_read_space(ringbuffer);
    if (aml_dev->is_multi_demux)
        audio_dtv_underrun_loop_mute_check(patch, stream_out);
    int ad_avail = dtv_assoc_get_avail();
    dtv_assoc_get_main_frame_size(&last_main_frame_size);
    dtv_assoc_get_ad_frame_size(&las_ad_frame_size);
    char buff[32];
    int ret = 0;
    ALOGV("AD main_avail=%d ad_avail=%d last_main_frame_size = %d",
    main_avail, ad_avail, last_main_frame_size);
    if ((last_main_frame_size == 0 && main_avail >= 6144)
        || (last_main_frame_size != 0 && main_avail >= last_main_frame_size)) {
        if (!patch->first_apts_lookup_over) {
            *apts_diff = dtv_set_audio_latency(0);
            if (!dtv_firstapts_lookup_over(patch, aml_dev, false, apts_diff) || main_avail < 512 * 2) {
                ALOGI("hold the aduio for cache data, avail %d", main_avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            patch->first_apts_lookup_over = 1;
            ALOGI("[audiohal_kpi][%s,%d] dtv_audio_tune %d-> AUDIO_LOOKUP",
                    __FUNCTION__, __LINE__, patch->dtv_audio_tune);
            patch->dtv_audio_tune = AUDIO_LOOKUP;
            //ALOGI("dtv_audio_tune audio_lookup\n");
            clean_dtv_patch_pts(patch);
        } else if (patch->dtv_audio_tune == AUDIO_BREAK) {
            int a_discontinue = get_audio_discontinue();
            dtv_set_audio_latency(*apts_diff);
            if (a_discontinue == 0) {
                ALOGI("audio is resumed\n");
            } else if (!dtv_firstapts_lookup_over(patch, aml_dev, true, apts_diff)) {
                ALOGI("hold the aduio for cache data, avail %d", main_avail);
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                usleep(5000);
                return -EAGAIN;
            }
            if (a_discontinue  == 0) {
                ALOGI("[%s,%d] dtv_audio_tune AUDIO_BREAK-> AUDIO_LOOKUP\n",
                    __FUNCTION__, __LINE__);
                patch->dtv_audio_tune = AUDIO_LOOKUP;
                //ALOGI("dtv_audio_tune audio_lookup\n");
                clean_dtv_patch_pts(patch);
            }
        } else if (patch->dtv_audio_tune == AUDIO_DROP) {
            dtv_do_drop_insert_ac3(patch, stream_out);
            if (patch->dtv_apts_lookup < 0 ||
                (patch->dtv_apts_lookup > 0 &&
                aml_out->need_drop_size == 0)) {
                clean_dtv_patch_pts(patch);
                patch->dtv_apts_lookup = 0;
                patch->ac3_pcm_dropping = 0;
                ALOGI("[%s,%d] dtv_audio_tune AUDIO_DROP-> AUDIO_LATENCY\n", __FUNCTION__, __LINE__);
                patch->dtv_audio_tune = AUDIO_LATENCY;
            }
        }

        //dtv_assoc_get_main_frame_size(&main_frame_size);
        //main_frame_size = 0, get from data
        while (main_frame_size == 0 && main_avail >= (int)sizeof(main_head)) {
            memset(main_head, 0, sizeof(main_head));
            ret = ring_buffer_read(ringbuffer, main_head, sizeof(main_head));
            if ( patch->output_thread_exit == 1) {
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                return 0;
            }
            main_frame_size = dcv_decoder_get_framesize(main_head,
                              ret, &main_head_offset);
            main_avail -= ret;
            if (main_frame_size != 0) {
                main_head_left = ret - main_head_offset;
                ALOGV("AD main_frame_size=%d  ", main_frame_size);
            }
        }

        dtv_assoc_set_main_frame_size(main_frame_size);

        if (main_frame_size > 0 && (main_avail >= main_frame_size - main_head_left)) {
            //dtv_assoc_set_main_frame_size(main_frame_size);
            //dtv_assoc_set_ad_frame_size(ad_frame_size);
            //read left of frame;
            if (main_head_left > 0) {
                memcpy(patch->out_buf, main_head + main_head_offset, main_head_left);
            }
            ret = ring_buffer_read(ringbuffer, (unsigned char *)patch->out_buf + main_head_left ,
                                   main_frame_size - main_head_left);
            if (ret == 0) {
                pthread_mutex_unlock(&(patch->dtv_output_mutex));
                /*ALOGE("%s(), ring_buffer read 0 data!", __func__);*/
                usleep(1000);
                return -EAGAIN;

            }
            dtv_assoc_audio_cache(1);
            main_size = ret + main_head_left;
        } else {
            dtv_audio_gap_monitor(patch);
            pthread_mutex_unlock(&(patch->dtv_output_mutex));
            usleep(1000);
            return -EAGAIN;
        }
        memset(ad_buffer, 0, sizeof(ad_buffer));
        if (aml_dev->is_multi_demux) {

            if (patch->demux_handle == NULL) {
                ALOGI("demux_handle %p", patch->demux_handle);
            }
            while (patch->output_thread_exit != 1) {
                if (mEsData == NULL) {
                    if (Get_ADAudio_Es(patch->demux_handle, &mEsData) != 0) {
                        ALOGV("do not get ad es data from dmx ");
                        break;
                    }
                }
                if (mEsData){
                    if (patch->cur_outapts > 0) {
                        demux_info->ad_package_status = check_ad_package_status(patch->cur_outapts, mEsData->pts, demux_info);
                        if (demux_info->ad_package_status == AD_PACK_STATUS_DROP) {
                            if (mEsData->data) {
                                aml_audio_free(mEsData->data);
                                mEsData->data = NULL;
                            }
                            aml_audio_free(mEsData);
                            mEsData = NULL;
                            continue;
                        } else if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                            ALOGV("normally it is impossible");
                        }
                    } else {
                        demux_info->ad_package_status = AD_PACK_STATUS_HOLD;
                    }
                    break;
                }
            }

            if (mEsData == NULL) {
                ad_size = 0;
            } else {
                ALOGV("ad mEsData->size %d mEsData->pts %0llx",mEsData->size, mEsData->pts);
                patch->mADEsData = mEsData;
                if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                    ALOGI("AD_PACK_STATUS_HOLD");
                    mEsData = NULL;
                    ad_size = 0;
                } else {
                   ad_size = mEsData->size;
                }

            }

            if (mEsData) {
                ad_frame_size = dcv_decoder_get_framesize(mEsData->data + mEsData->used_size,
                        mEsData->size, &ad_head_offset);
                ALOGV("ad_frame_size %d ad_head_offset %d used size =%d total=%d", ad_frame_size ,ad_head_offset, mEsData->used_size, mEsData->size);
                if ((mEsData->size - mEsData->used_size  >= ad_frame_size + ad_head_offset) &&
                    ad_frame_size != 0) {
                     memcpy(ad_buffer,mEsData->data + mEsData->used_size + ad_head_offset, ad_frame_size);
                     ad_size = ad_frame_size;
                     mEsData->used_size += (ad_frame_size + ad_head_offset);
                } else {
                    ALOGI("ad_frame_size %d mEsData->used_size %d mEsData->size %d",
                        ad_frame_size, mEsData->used_size, mEsData->size);
                    mEsData->used_size = mEsData->size;
                }
                if (mEsData->size  == mEsData->used_size) {
                   aml_audio_free(mEsData->data);
                   aml_audio_free(mEsData);
                   patch->mADEsData = NULL;
                }

            }

        } else {
            if (ad_avail > 0) {
                //dtv_assoc_get_ad_frame_size(&ad_frame_size);
                //ad_frame_size = 0, get from data
                while (ad_frame_size == 0 && ad_avail >= (int)sizeof(ad_head)) {
                    if ( patch->output_thread_exit == 1) {
                        pthread_mutex_unlock(&(patch->dtv_output_mutex));
                        return 0;
                    }
                    memset(ad_head, 0, sizeof(ad_head));
                    ret = dtv_assoc_read(ad_head, sizeof(ad_head));
                    ad_frame_size = dcv_decoder_get_framesize(ad_head,
                                    ret, &ad_head_offset);
                    ad_avail -= ret;
                    if (ad_frame_size != 0) {
                        ad_head_left = ret - ad_head_offset;
                        //ALOGI("AD ad_frame_size=%d  ", ad_frame_size);
                    }
                }
            }
            if (ad_frame_size > 0 && (ad_avail >= ad_frame_size - ad_head_left)) {
                if (ad_head_left > 0) {
                    memcpy(ad_buffer, ad_head + ad_head_offset, ad_head_left);
                }
                ret = dtv_assoc_read(ad_buffer + ad_head_left, ad_frame_size - ad_head_left);
                if (ret == 0) {
                    ad_size = 0;
                } else {
                    ad_size = ret + ad_head_left;
                }
            } else {
                ad_size = 0;
            }
        }
        dtv_assoc_set_ad_frame_size(ad_size);
        /*guess it is not necessary,left to do */
        if (aml_dev->associate_audio_mixing_enable == 0) {
            ad_size = 0;
        }

        if (aml_out->hal_internal_format != patch->aformat) {
            aml_out->hal_format = aml_out->hal_internal_format = patch->aformat;
            get_sink_format(stream_out);
        }

        if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
            remain_size = dolby_ms12_get_main_buffer_avail(NULL);
            dolby_ms12_get_pcm_output_size(&all_pcm_len1, &all_zero_len);
        }

        //package iec61937
        memset(mixbuffer, 0, sizeof(mixbuffer));
        //papbpcpd
        p16_mixbuff = (uint16_t*)mixbuffer;
        p16_mixbuff[0] = 0xf872;
        p16_mixbuff[1] = 0x4e1f;
        if (patch->aformat == AUDIO_FORMAT_AC3) {
            dd_bsmod = 6;
            p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 1;
            if (ad_size == 0) {
                p16_mixbuff[3] = (main_size + sizeof(mute_dd_frame)) * 8;
            } else {
                p16_mixbuff[3] = (main_size + ad_size) * 8;
            }
        } else {
            dd_bsmod = 12;
            p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 21;
            if (ad_size == 0) {
                p16_mixbuff[3] = main_size + sizeof(mute_ddp_frame);
            } else {
                p16_mixbuff[3] = main_size + ad_size;
            }
        }
        mix_size += 8;
        //main
        memcpy(mixbuffer + mix_size, patch->out_buf, main_size);
        mix_size += main_size;
        //ad
        if (ad_size == 0) {
            ALOGV("ad data not enough,filled with mute frame ");
            if (patch->aformat == AUDIO_FORMAT_AC3) {
                memcpy(mixbuffer + mix_size, mute_dd_frame, sizeof(mute_dd_frame));
            } else {
                memcpy(mixbuffer + mix_size, mute_ddp_frame, sizeof(mute_ddp_frame));
            }
        } else {
            memcpy(mixbuffer + mix_size, ad_buffer, ad_size);
        }

        if (patch->aformat == AUDIO_FORMAT_AC3) {//ac3 iec61937 package size 6144
            ret = out_write_new(stream_out, mixbuffer, AC3_IEC61937_FRAME_SIZE);
        } else {//eac3 iec61937 package size 6144*4
            ret = out_write_new(stream_out, mixbuffer, EAC3_IEC61937_FRAME_SIZE);
        }

        if ((mixbuffer[8] != 0xb && mixbuffer[8] != 0x77)
            || (mixbuffer[9] != 0xb && mixbuffer[9] != 0x77)
            || (mixbuffer[mix_size] != 0xb && mixbuffer[mix_size] != 0x77)
            || (mixbuffer[mix_size + 1] != 0xb && mixbuffer[mix_size + 1] != 0x77)) {
            ALOGD("AD mix main_size=%d ad_size=%d wirte_size=%d 0x%x 0x%x 0x%x 0x%x", main_size, ad_size, ret,
                  mixbuffer[8], mixbuffer[9], mixbuffer[mix_size], mixbuffer[mix_size + 1]);
        }

        if (eDolbyMS12Lib == aml_dev->dolby_lib_type) {
            int size = dolby_ms12_get_main_buffer_avail(NULL);
            dolby_ms12_get_pcm_output_size(&all_pcm_len2, &all_zero_len);
            patch->decoder_offset += remain_size + main_size - size;
            patch->outlen_after_last_validpts += (unsigned int)(all_pcm_len2 - all_pcm_len1);
            //ALOGD("remain_size %d,size %d,main_size %d,validpts %d",remain_size,size,main_size,patch->outlen_after_last_validpts);
            patch->dtv_pcm_readed += main_size;

        } else {
            patch->decoder_offset += main_frame_size;
            patch->dtv_pcm_readed += main_size;
        }
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    } else {
        dtv_audio_gap_monitor(patch);
        pthread_mutex_unlock(&(patch->dtv_output_mutex));
        usleep(1000);
    }

    return ret;
}


void *audio_dtv_patch_output_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct audio_stream_out *stream_out = NULL;
    struct aml_stream_out *aml_out = NULL;
    struct audio_config stream_config;
    int write_bytes = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    int ret;
    int apts_diff = 0;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    ALOGI("[audiohal_kpi]++%s created.", __FUNCTION__);
    // FIXME: get actual configs
    stream_config.sample_rate = 48000;
    stream_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    stream_config.format = AUDIO_FORMAT_PCM_16_BIT;
    /*
    may we just exit from a direct active stream playback
    still here.we need remove to standby to new playback
    */
    pthread_mutex_lock(&aml_dev->lock);
    aml_out = direct_active(aml_dev);
    if (aml_out) {
        ALOGI("%s live stream %p active,need standby aml_out->usecase:%d ",
              __func__, aml_out, aml_out->usecase);
        pthread_mutex_unlock(&aml_dev->lock);
        aml_dev->continuous_audio_mode = 0;
        /*
        there are several output cases. if there are no ms12 or submixing modules.
        we will output raw/lpcm directly.we need close device directly.
        we need call standy function to release the direct stream
        */
        out_standby_new((struct audio_stream *)aml_out);
        pthread_mutex_lock(&aml_dev->lock);
        if (aml_dev->need_remove_conti_mode == true) {
            ALOGI("%s,conntinous mode still there,release ms12 here", __func__);
            aml_dev->need_remove_conti_mode = false;
            aml_dev->continuous_audio_mode = 0;
        }
    } else {
        ALOGI("++%s live cant get the aml_out now!!!\n ", __FUNCTION__);
    }
    aml_dev->mix_init_flag = false;
    aml_dev->mute_start = true;
    pthread_mutex_unlock(&aml_dev->lock);
#ifdef TV_AUDIO_OUTPUT
    patch->output_src = AUDIO_DEVICE_OUT_SPEAKER;
#else
    patch->output_src = AUDIO_DEVICE_OUT_AUX_DIGITAL;
#endif
    if (aml_dev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
        patch->output_src = aml_dev->out_device;

    ret = adev_open_output_stream_new(patch->dev, 0,
                                      patch->output_src,        // devices_t
                                      AUDIO_OUTPUT_FLAG_DIRECT, // flags
                                      &stream_config, &stream_out, "AML_DTV_SOURCE");
    if (ret < 0) {
        ALOGE("live open output stream fail, ret = %d", ret);
        goto exit_open;
    }
    aml_out = (struct aml_stream_out *)stream_out;
    aml_out->dtvsync_enable =  property_get_int32("vendor.media.dtvsync.enable", 1);
    ALOGI("++%s live create a output stream success now!!!\n ", __FUNCTION__);

    patch->out_buf_size = write_bytes * EAC3_MULTIPLIER;
    patch->out_buf = aml_audio_calloc(1, patch->out_buf_size);
    if (!patch->out_buf) {
        ret = -ENOMEM;
        goto exit_outbuf;
    }
    patch->mADEsData = NULL;
    patch->dtv_audio_mode = get_dtv_audio_mode();
    patch->dtv_audio_tune = AUDIO_FREE;
    patch->first_apts_lookup_over = 0;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    ALOGI("[audiohal_kpi]++%s live start output pcm now patch->output_thread_exit %d!!!\n ",
          __FUNCTION__, patch->output_thread_exit);

    prctl(PR_SET_NAME, (unsigned long)"audio_output_patch");

    while (!patch->output_thread_exit) {
        if (patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_PAUSE) {
            usleep(1000);
            continue;
        }

        pthread_mutex_lock(&(patch->dtv_output_mutex));
        //int period_mul =
        //    (patch->aformat == AUDIO_FORMAT_E_AC3) ? EAC3_MULTIPLIER : 1;
        aml_out->codec_type = get_codec_type(patch->aformat);
        if ((patch->aformat == AUDIO_FORMAT_AC3) ||
            (patch->aformat == AUDIO_FORMAT_E_AC3) ||
            (patch->aformat == AUDIO_FORMAT_AC4)) {
            ALOGV("AD %d %d %d", aml_dev->dolby_lib_type, aml_dev->dual_decoder_support, demux_info->ad_pid);
            if (aml_dev->dual_decoder_support && VALID_PID(demux_info->ad_pid)) {
                if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
                    if (aml_dev->disable_pcm_mixing == false || aml_dev->hdmi_format == PCM ||
                        aml_dev->sink_capability == AUDIO_FORMAT_PCM_16_BIT || aml_dev->sink_capability == AUDIO_FORMAT_PCM_32_BIT) {
                        ret = audio_dtv_patch_output_dolby_dual_decoder(patch, stream_out, &apts_diff);
                    } else {
                        ret = audio_dtv_patch_output_dolby(patch, stream_out, &apts_diff);
                    }
                } else {
                    audio_format_t output_format = get_non_ms12_output_format(patch->aformat, aml_dev);
                    if (output_format != AUDIO_FORMAT_AC3 && output_format != AUDIO_FORMAT_E_AC3) {
                        ret = audio_dtv_patch_output_dolby_dual_decoder(patch, stream_out, &apts_diff);
                    } else {
                        ret = audio_dtv_patch_output_dolby(patch, stream_out, &apts_diff);
                    }
                }
            } else {
                ret = audio_dtv_patch_output_dolby(patch, stream_out, &apts_diff);
            }
        } else if (patch->aformat == AUDIO_FORMAT_DTS) {
            ret = audio_dtv_patch_output_dts(patch, stream_out);
        } else {
            ret = audio_dtv_patch_output_default(patch, stream_out, &apts_diff);
        }

        aml_dec_t *aml_dec = aml_out->aml_dec;
        if (aml_dev->mixing_level != aml_out->dec_config.mixer_level ) {
            aml_out->dec_config.mixer_level = aml_dev->mixing_level;
            aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_MIXER_LEVEL, &aml_out->dec_config);
        }
        if (aml_dev->associate_audio_mixing_enable != aml_out->dec_config.ad_mixing_enable) {
           aml_out->dec_config.ad_mixing_enable = aml_dev->associate_audio_mixing_enable;
           aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_MIXING_ENABLE, &aml_out->dec_config);
        }
        if (aml_dev->advol_level != aml_out->dec_config.advol_level) {
           aml_out->dec_config.advol_level = aml_dev->advol_level;
           aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_AD_VOL, &aml_out->dec_config);
        }
    }
    aml_audio_free(patch->out_buf);
    patch->out_buf = NULL;
exit_outbuf:
    do_output_standby_l((struct audio_stream *)aml_out);
    adev_close_output_stream_new(dev, stream_out);
exit_open:
    if (aml_dev->audio_ease) {
        aml_dev->patch_start = false;
    }
    if (get_video_delay() != 0) {
        set_video_delay(0);
    }
    ALOGI("--%s live ", __FUNCTION__);
    return ((void *)0);
}

int patch_thread_get_cmd(struct aml_audio_patch *patch, int *cmd, int *path_id)
{
    if (patch->dtv_cmd_list->initd == 0) {
        return -1;
    }
    if (patch == NULL) {
        return -1;
    }

    if (dtv_patch_cmd_is_empty(patch->dtv_cmd_list) == 1) {
        return -1;
    } else {
        return dtv_patch_get_cmd(patch->dtv_cmd_list,cmd, path_id);
    }
}

static void *audio_dtv_patch_process_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct audio_stream_in *stream_in = NULL;
    struct audio_config stream_config;
    // FIXME: add calc for read_bytes;
    int read_bytes = DEFAULT_CAPTURE_PERIOD_SIZE * CAPTURE_PERIOD_COUNT;
    int ret = 0, retry = 0;
    audio_format_t cur_aformat;
    unsigned int adec_handle;
    int cmd = AUDIO_DTV_PATCH_CMD_NUM;
    int path_id  = 0;
    patch->sample_rate = stream_config.sample_rate = 48000;
    patch->chanmask = stream_config.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    patch->aformat = stream_config.format = AUDIO_FORMAT_PCM_16_BIT;

    int switch_flag = property_get_int32("vendor.media.audio.strategy.switch", 0);
    int show_first_nosync = property_get_int32("vendor.media.video.show_first_frame_nosync", 1);
    patch->pre_latency = property_get_int32(PROPERTY_PRESET_AC3_PASSTHROUGH_LATENCY, 30);
    patch->a_discontinue_threshold = property_get_int32(
                                        PROPERTY_AUDIO_DISCONTINUE_THRESHOLD, 30 * 90000);
    patch->sync_para.cur_pts_diff = 0;
    patch->sync_para.in_out_underrun_flag = 0;
    patch->sync_para.pcr_adjust_max = property_get_int32(
                                        PROPERTY_AUDIO_ADJUST_PCR_MAX, 1 * 90000);
    patch->sync_para.underrun_mute_time_min = property_get_int32(
                                        PROPERTY_UNDERRUN_MUTE_MINTIME, 200);
    patch->sync_para.underrun_mute_time_max = property_get_int32(
                                        PROPERTY_UNDERRUN_MUTE_MAXTIME, 1000);
    patch->sync_para.underrun_max_time =  property_get_int32(
                                        PROPERTY_UNDERRUN_MAX_TIME, 5000);

    ALOGI("switch_flag=%d, show_first_nosync=%d, pre_latency=%d,discontinue:%d\n",
        switch_flag, show_first_nosync, patch->pre_latency,
        patch->a_discontinue_threshold);
    ALOGI("sync:pcr_adjust_max=%d\n", patch->sync_para.pcr_adjust_max);
    ALOGI("[audiohal_kpi]++%s Enter.\n", __FUNCTION__);
    patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
    aml_demux_audiopara_t *demux_info = NULL;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    while (!patch->cmd_process_thread_exit ) {

        pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
        switch (patch->dtv_decoder_state) {
        case AUDIO_DTV_PATCH_DECODER_STATE_INIT: {
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            ALOGI("[audiohal_kpi]++%s live now  open the audio decoder now !\n", __FUNCTION__);
            dtv_patch_input_open(&adec_handle, dtv_patch_pcm_write,
                                 dtv_patch_status_info,
                                 dtv_patch_audio_info,patch);
            patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_START;
            /* +[SE] [BUG][SWPL-21070][yinli.xia] added: turn channel mute audio*/
            if (switch_flag) {
                if (patch->dtv_has_video == 1)
                    aml_dev->start_mute_flag = 1;
            }
            aml_dev->underrun_mute_flag = 0;
            if (show_first_nosync) {
                sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
                ALOGI("show_first_frame_nosync set 1\n");
            }
        }
        break;
        case AUDIO_DTV_PATCH_DECODER_STATE_START:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            ring_buffer_reset(&patch->aml_ringbuffer);
            if (cmd == AUDIO_DTV_PATCH_CMD_START) {
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
                aml_dev->patch_start = false;
                memset(&patch->dtv_resample, 0, sizeof(struct resample_para));
                if (patch->resample_outbuf) {
                    memset(patch->resample_outbuf, 0, OUTPUT_BUFFER_SIZE * 3);
                }

                if (patch->dtv_aformat == ACODEC_FMT_AC3) {
                    patch->aformat = AUDIO_FORMAT_AC3;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_EAC3) {
                    patch->aformat = AUDIO_FORMAT_E_AC3;

                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_DTS) {
                    patch->aformat = AUDIO_FORMAT_DTS;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC ) {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC_LATM) {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG ) {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG1) {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;/* audio-base.h no define mpeg1 format */
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG2) {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AC4) {
                   patch->aformat = AUDIO_FORMAT_AC4;
                   patch->decoder_offset = 0;
                } else {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                }

                demux_info = (aml_demux_audiopara_t *)patch->demux_info;
                bool associate_mix = aml_dev->associate_audio_mixing_enable;
                bool dual_decoder = aml_dev->dual_decoder_support;
                int demux_id  = demux_info->demux_id;
                ALOGI("patch->demux_handle %p patch->aformat %0x", patch->demux_handle, patch->aformat);
                if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
                    if (aml_dev->disable_pcm_mixing == true && aml_dev->hdmi_format != PCM &&
                        (aml_dev->sink_capability == AUDIO_FORMAT_AC3 || aml_dev->sink_capability == AUDIO_FORMAT_E_AC3)) {
                        associate_mix = 0;
                        dual_decoder = 0;
                    }
                } else {
                    audio_format_t output_format = get_non_ms12_output_format(patch->aformat, aml_dev);
                    if (output_format != AUDIO_FORMAT_PCM_16_BIT && output_format != AUDIO_FORMAT_PCM_32_BIT) {
                        associate_mix = 0;
                        dual_decoder = 0;
                    }
                }
                ALOGI("[%s:%d] dtv_aformat:%#x, sink_capability:%#x, associate:%d, dual_decoder_support:%d", __func__, __LINE__,
                    patch->dtv_aformat, aml_dev->sink_capability, associate_mix, dual_decoder);

                dtv_patch_input_start(adec_handle,
                                      demux_info->demux_id,
                                      demux_info->main_pid,
                                      demux_info->main_fmt,
                                      demux_info->has_video,
                                      dual_decoder,
                                      associate_mix,
                                      aml_dev->mixing_level,
                                      patch->demux_handle);
                ALOGI("[audiohal_kpi]++%s live now  start the audio decoder now !\n",
                      __FUNCTION__);
                patch->dtv_first_apts_flag = 0;
                patch->outlen_after_last_validpts = 0;
                patch->last_valid_pts = 0;
                patch->cur_outapts  = 0;
                patch->first_apts_lookup_over = 0;
                patch->ac3_pcm_dropping = 0;
                patch->last_audio_delay = 0;
                patch->pcm_inserting = false;
                patch->tsync_pcr_debug = get_tsync_pcr_debug();
                patch->startplay_firstvpts = 0;
                patch->startplay_first_checkinapts = 0;
                patch->startplay_pcrpts = 0;
                patch->startplay_apts_lookup = 0;
                patch->startplay_vpts = 0;

                patch->dtv_pcm_readed = patch->dtv_pcm_writed = 0;
                patch->numDecodedSamples = patch->numOutputSamples = 0;
                create_dtv_output_stream_thread(patch);
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        case AUDIO_DTV_PATCH_DECODER_STATE_RUNING:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            /*[SE][BUG][SWPL-17416][chengshun] maybe sometimes subafmt and subapid not set before dtv patch start*/
            if (aml_dev->ad_start_enable == 0 && VALID_PID(demux_info->ad_pid) && VALID_AD_FMT(demux_info->ad_fmt)) {
                if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
                    if (aml_dev->disable_pcm_mixing == false || aml_dev->hdmi_format == PCM ||
                        aml_dev->sink_capability == AUDIO_FORMAT_PCM_16_BIT || aml_dev->sink_capability == AUDIO_FORMAT_PCM_32_BIT) {
                        int ad_start_flag;
                        if (aml_dev->is_multi_demux) {
                            ad_start_flag = Start_Dmx_AD_Audio(patch->demux_handle);
                        } else {
                            ad_start_flag = dtv_assoc_audio_start(1, demux_info->ad_pid, demux_info->ad_fmt, demux_info->demux_id);
                        }
                        if (ad_start_flag == 0) {
                            aml_dev->ad_start_enable = 1;
                        }
                    } else {
                        // bypass ms12, not support 2 stream.
                    }
                } else {
                    audio_format_t output_format = get_non_ms12_output_format(patch->aformat, aml_dev);
                    if (output_format != AUDIO_FORMAT_PCM_16_BIT && output_format != AUDIO_FORMAT_PCM_32_BIT) {
                        // non ms12, bypass Dolby, not support 2 stream.
                    } else {
                        int ad_start_flag;

                        if (aml_dev->is_multi_demux) {
                            ad_start_flag = 0;
                        } else {
                            ad_start_flag = dtv_assoc_audio_start(1, demux_info->ad_pid, demux_info->ad_fmt, demux_info->demux_id);
                        }
                        if (ad_start_flag == 0) {
                            aml_dev->ad_start_enable = 1;
                        }
                    }
                }
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_PAUSE) {
                ALOGI("++%s live now start  pause  the audio decoder now \n",
                      __FUNCTION__);
                dtv_patch_input_pause(adec_handle);
                if (aml_dev->is_multi_demux) {
                //to do
                } else {
                    dtv_assoc_audio_pause(1);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_PAUSE;
                ALOGI("++%s live now end  pause  the audio decoder now \n",
                      __FUNCTION__);
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                      __FUNCTION__);
                dtv_patch_input_stop_dmx(adec_handle);
                //tv_do_ease_out(aml_dev);
                release_dtv_output_stream_thread(patch);
                dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                dtv_patch_input_stop(adec_handle);
                dtv_assoc_audio_stop(1);
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            break;

        case AUDIO_DTV_PATCH_DECODER_STATE_PAUSE:
            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                pthread_cond_wait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (aml_dev->is_multi_demux) {
                path_id = dtv_audio_instances->demux_index_working;
                patch->demux_handle = dtv_audio_instances->demux_handle[path_id];
            }
            if (cmd == AUDIO_DTV_PATCH_CMD_RESUME) {
                dtv_patch_input_resume(adec_handle);
                if (aml_dev->is_multi_demux) {
                    //to do
                } else {
                    dtv_assoc_audio_resume(1,demux_info->ad_pid);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                     __FUNCTION__);
                dtv_patch_input_stop(adec_handle);
                dtv_assoc_audio_stop(1);
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
           } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        default:
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }
            break;
        }
        pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
    }

exit:
    ALOGI("[audiohal_kpi]++%s now  live  release  the audio decoder", __FUNCTION__);
    dtv_patch_input_stop_dmx(adec_handle);
    release_dtv_output_stream_thread(patch);
    dtv_patch_input_stop(adec_handle);
    aml_dev->ad_start_enable = 0;
    dtv_assoc_audio_stop(1);
    dtv_check_audio_reset();
    ALOGI("[audiohal_kpi]++%s Exit", __FUNCTION__);
    pthread_exit(NULL);
}

int audio_set_spdif_clock(struct aml_stream_out *stream, int type)
{
    struct aml_audio_device *dev = stream->dev;
    bool is_dual_spdif = is_dual_output_stream((struct audio_stream_out *)stream);

    if (!dev || !dev->audio_patch) {
        return 0;
    }
    if (dev->patch_src != SRC_DTV || !dev->audio_patch->is_dtv_src) {
        return 0;
    }
    if (!(dev->usecase_masks > 1)) {
        return 0;
    }

    switch (type) {
    case AML_DOLBY_DIGITAL:
    case AML_DOLBY_DIGITAL_PLUS:
    case AML_DTS:
    case AML_DTS_HD:
        dev->audio_patch->spdif_format_set = 1;
        break;
    case AML_STEREO_PCM:
    default:
        dev->audio_patch->spdif_format_set = 0;
        break;
    }

    if (alsa_device_is_auge()) {
        if (dev->audio_patch->spdif_format_set) {
            if (stream->hal_internal_format == AUDIO_FORMAT_E_AC3 &&
                dev->bHDMIARCon && !is_dual_output_stream((struct audio_stream_out *)stream)) {
                dev->audio_patch->dtv_default_spdif_clock =
                    stream->config.rate * 128 * 4;
            } else {
                dev->audio_patch->dtv_default_spdif_clock =
                    stream->config.rate * 128;
            }
        } else {
            dev->audio_patch->dtv_default_spdif_clock =
                DEFAULT_I2S_OUTPUT_CLOCK / 2;
        }
    } else {
        if (dev->audio_patch->spdif_format_set) {
            dev->audio_patch->dtv_default_spdif_clock =
                stream->config.rate * 128 * 4;
        } else {
            dev->audio_patch->dtv_default_spdif_clock =
                DEFAULT_I2S_OUTPUT_CLOCK;
        }
    }
    dev->audio_patch->spdif_step_clk =
        dev->audio_patch->dtv_default_spdif_clock / (property_get_int32(
                                        PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS, DEFAULT_TUNING_PCR_CLOCK_STEPS));
    dev->audio_patch->i2s_step_clk =
        DEFAULT_I2S_OUTPUT_CLOCK / (property_get_int32(
                                        PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS, DEFAULT_TUNING_PCR_CLOCK_STEPS));
    ALOGI("[%s] type=%d,spdif %d,dual %d, arc %d", __FUNCTION__, type, dev->audio_patch->spdif_step_clk,
          is_dual_spdif, dev->bHDMIARCon);
    dtv_adjust_output_clock(dev->audio_patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, is_dual_spdif);
    return 0;
}


int audio_dtv_patch_output_single_decoder(struct aml_audio_patch *patch,
                        struct audio_stream_out *stream_out)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    package_list *list = patch->dtv_package_list;
    struct package *cur_package = patch->cur_package;
    int ret = 0;
    if (!cur_package ) {
        ALOGI("cur_package NULL");
        return ret;
    }
    if (cur_package->data)  {
        if (cur_package->size == 0) {
            ALOGI("cur_package->size  %d", cur_package->size );
            return ret;
        }
    } else {
         ALOGI("cur_package->data NULL !!!");
         return ret;
    }
    if (!aml_out->aml_dec && patch->aformat == AUDIO_FORMAT_E_AC3) {
        aml_out->ad_substream_supported = is_ad_substream_supported((unsigned char *)cur_package->data, cur_package->size);
    }
    ALOGV("p_package->data %p p_package->ad_data %p", cur_package->data, cur_package->ad_data);

    ret = out_write_new(stream_out, cur_package->data, cur_package->size);

    if (cur_package->data)
        aml_audio_free(cur_package->data);
    aml_audio_free(cur_package);
    cur_package = NULL;
    return ret;
}

int audio_dtv_patch_output_dual_decoder(struct aml_audio_patch *patch,
                                         struct audio_stream_out *stream_out)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream_out;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    package_list *list = patch->dtv_package_list;
    //int apts_diff = 0;

    unsigned char mixbuffer[EAC3_IEC61937_FRAME_SIZE];
    unsigned char ad_buffer[EAC3_IEC61937_FRAME_SIZE];
    uint16_t *p16_mixbuff = NULL;
    uint32_t *p32_mixbuff = NULL;
    int main_size = 0, ad_size = 0, mix_size = 0 , dd_bsmod = 0;
    int ret = 0;

    struct package *p_package = NULL;
    p_package = patch->cur_package;

    if (patch->aformat == AUDIO_FORMAT_AC3 ||
        patch->aformat == AUDIO_FORMAT_E_AC3) {
       //package iec61937
        memset(mixbuffer, 0, sizeof(mixbuffer));
        //papbpcpd
        p16_mixbuff = (uint16_t*)mixbuffer;
        p16_mixbuff[0] = 0xf872;
        p16_mixbuff[1] = 0x4e1f;

        mix_size += 8;
        main_size = p_package->size;
        ALOGV("main mEsData->size %d",p_package->size);
        if (p_package->size + p_package->ad_size + mix_size > EAC3_IEC61937_FRAME_SIZE) {
            ALOGE("pakage size too large, main_size %d ad_size %d", main_size, p_package->ad_size);
            return ret;
        }
        if (p_package->data)
            memcpy(mixbuffer + mix_size, p_package->data, p_package->size);
        ad_size = p_package->ad_size;
        ALOGV("ad mEsData->size %d",p_package->ad_size);
        if (p_package->ad_data)
            memcpy(mixbuffer + mix_size + main_size,p_package->ad_data, p_package->ad_size);

        if (patch->aformat == AUDIO_FORMAT_AC3) {
            dd_bsmod = 6;
            p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 1;
            if (ad_size == 0) {
                p16_mixbuff[3] = (main_size + sizeof(mute_dd_frame)) * 8;
                memcpy(mixbuffer + mix_size + main_size,mute_dd_frame, sizeof(mute_dd_frame));
            } else {
                p16_mixbuff[3] = (main_size + ad_size) * 8;
            }
        } else {
            dd_bsmod = 12;
            p16_mixbuff[2] = ((dd_bsmod & 7) << 8) | 21;
            if (ad_size == 0) {
                p16_mixbuff[3] = main_size + sizeof(mute_ddp_frame);
                memcpy(mixbuffer + mix_size + main_size,mute_ddp_frame, sizeof(mute_ddp_frame));
            } else {
                p16_mixbuff[3] = main_size + ad_size;
            }
        }
    } else {
         if (aml_dec) {
             aml_dec->ad_data = p_package->ad_data;
             aml_dec->ad_size = p_package->ad_size;
         }
    }

    if (patch->aformat == AUDIO_FORMAT_AC3) {//ac3 iec61937 package size 6144
        ret = out_write_new(stream_out, mixbuffer, AC3_IEC61937_FRAME_SIZE);
    } else if (patch->aformat == AUDIO_FORMAT_E_AC3) {//eac3 iec61937 package size 6144*4
        ret = out_write_new(stream_out, mixbuffer, EAC3_IEC61937_FRAME_SIZE);
    } else {
        ret = out_write_new(stream_out, p_package->data, p_package->size);
    }

    if (p_package) {

        if (p_package->data) {
            aml_audio_free(p_package->data);
            p_package->data = NULL;
        }

        if (p_package->ad_data) {
            aml_audio_free(p_package->ad_data);
            p_package->ad_data = NULL;
        }

        aml_audio_free(p_package);
        p_package = NULL;
    }

    return ret;
}

static bool need_enable_dual_decoder(struct aml_audio_patch *patch)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    if (demux_info->dual_decoder_support && VALID_PID(demux_info->ad_pid)) {
        if (aml_dev->dolby_lib_type == eDolbyDcvLib) {
             if (patch->aformat == AUDIO_FORMAT_AC3 || patch->aformat == AUDIO_FORMAT_E_AC3 )  {
                audio_format_t output_format = get_non_ms12_output_format(patch->aformat, aml_dev);
                if (output_format == AUDIO_FORMAT_AC3 || output_format == AUDIO_FORMAT_E_AC3) {
                    return false;
                }
            }
        } else if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
            if (aml_dev->hdmi_format == BYPASS) {
                return false;
            }
        }
        return true;
    } else {
       return false;
    }

}

void *audio_dtv_patch_input_threadloop(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    package_list *list = patch->dtv_package_list;
    void *demux_handle = patch->demux_handle;
    int read_bytes = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    int ret = 0;
    ALOGI("++%s create a input stream success now!!!\n ", __FUNCTION__);
    int nNextFrameSize = 0; //next read frame size
    int inlen = 0;//real data size in in_buf
    int nInBufferSize = read_bytes; //full buffer size
    char *inbuf = NULL;//real buffer
    char *ad_buffer = NULL;
    struct package *dtv_pacakge = NULL;
    struct mAudioEsDataInfo *mEsData = NULL ,*mAdEsData = NULL;
    int trycount = 0;
    int max_trycount = 2;
    int rlen = 0;//read buffer ret size
    struct mediasync_audio_queue_info audio_queue_info;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    aml_dtvsync_t *Dtvsync = NULL ;
    aml_demux_audiopara_t *demux_info = NULL;
    int path_index = 0, working_path_index = 0;
    ALOGI("[audiohal_kpi]++%s start input now patch->input_thread_exit %d!!!\n ",
          __FUNCTION__, patch->input_thread_exit);

    prctl(PR_SET_NAME, (unsigned long)"audio_input_patch");
    dtv_package_list_init(patch->dtv_package_list);

    while (!patch->input_thread_exit) {

        int nRet = 0;
        if (!aml_dev->is_multi_demux) {
            pthread_mutex_lock(&patch->mutex);
            if (dtv_pacakge == NULL) {
                dtv_pacakge = aml_audio_malloc(sizeof(struct package));
                if (!dtv_pacakge) {
                    ALOGI("dtv_pacakge malloc failed ");
                    pthread_mutex_unlock(&patch->mutex);
                    goto exit;
                }
            }
            if (inbuf != NULL) {
                aml_audio_free(inbuf);
                inbuf = NULL;
            }
            inbuf = aml_audio_malloc(nInBufferSize);
            if (!inbuf) {
                ALOGE("inbuf malloc failed");
                pthread_mutex_unlock(&patch->mutex);
                goto exit;
            }

            int nNextReadSize = nInBufferSize;
            rlen = 0;
            while (nNextReadSize > 0 && !patch->input_thread_exit) {
                nRet = uio_read_buffer((unsigned char *)(inbuf + rlen), nNextReadSize);
                //ALOGI("nRet:%d",nRet);
                if (nRet <= 0) {
                    trycount++;
                    if (trycount >= max_trycount) {
                        break;
                    } else {
                       usleep(20000);
                       continue;
                    }
                }
                rlen += nRet;
                nNextReadSize -= nRet;
            }

            trycount = 0;
            dtv_pacakge->size = nInBufferSize;
            dtv_pacakge->data = (char *)inbuf;

            if (demux_info->dual_decoder_support && VALID_PID(demux_info->ad_pid)) {
               if (ad_buffer != NULL) {
                  aml_audio_free(ad_buffer);
                  ad_buffer = NULL;
               }
               ad_buffer = aml_audio_malloc(nInBufferSize);
               if (!ad_buffer) {
                   ALOGE("ad_buffer malloc failed");
                   pthread_mutex_unlock(&patch->mutex);
                   goto exit;
               }
               ret = dtv_assoc_read((unsigned char *)ad_buffer , nInBufferSize);
               if (ret == 0) {
                    dtv_pacakge->ad_size = 0;
                    dtv_pacakge->ad_data  = NULL;
               } else {
                    dtv_pacakge->ad_size = ret;
                    dtv_pacakge->ad_data = ad_buffer;
               }
               if (dtv_pacakge->ad_size == 0) {
                   ALOGV("ad data not enough,filled with mute frame ");
                   if (patch->aformat == AUDIO_FORMAT_AC3) {
                       memcpy(ad_buffer, mute_dd_frame, sizeof(mute_dd_frame));
                       dtv_pacakge->ad_size = sizeof(mute_dd_frame);
                       dtv_pacakge->ad_data = ad_buffer;
                   } else if (patch->aformat == AUDIO_FORMAT_E_AC3) {
                       memcpy(ad_buffer, mute_ddp_frame, sizeof(mute_ddp_frame));
                       dtv_pacakge->ad_size = sizeof(mute_dd_frame);
                       dtv_pacakge->ad_data = ad_buffer;
                   }
               }
            }

            ret = dtv_package_add(list, dtv_pacakge);
            if (ret == 0) {
                inbuf = NULL;
            }
            ALOGV("pthread_cond_signal");
            pthread_cond_signal(&patch->cond);
            pthread_mutex_unlock(&patch->mutex);

        } else {
            path_index = 0;
            for (; path_index < DVB_DEMUX_SUPPORT_MAX_NUM; path_index++ ) {

                if (patch->input_thread_exit) {
                    break;
                }
                pthread_mutex_lock(&aml_dev->dtv_lock);
                demux_handle = dtv_audio_instances->demux_handle[path_index];
                demux_info = &dtv_audio_instances->demux_info[path_index];
                Dtvsync = &dtv_audio_instances->dtvsync[path_index];
                if (Dtvsync->mediasync_new != NULL && Dtvsync->mediasync != Dtvsync->mediasync_new) {
                    Dtvsync->mediasync = Dtvsync->mediasync_new;
                }

                dtv_pacakge = demux_info->dtv_pacakge;
                mEsData = demux_info->mEsData;
                mAdEsData = demux_info->mADEsData;

                if (demux_handle == NULL) {
                    if (dtv_pacakge) {
                       if (dtv_pacakge->data) {
                           aml_audio_free(dtv_pacakge->data);
                           dtv_pacakge->data = NULL;
                       }
                       if (dtv_pacakge->ad_data) {
                           aml_audio_free(dtv_pacakge->ad_data);
                           dtv_pacakge->ad_data = NULL;
                       }
                       aml_audio_free(dtv_pacakge);
                       dtv_pacakge = NULL;
                       demux_info->dtv_pacakge = NULL;
                    }
                    if (demux_info->mEsData) {
                       aml_audio_free(demux_info->mEsData);
                       demux_info->mEsData =  NULL;
                    }

                    if (demux_info->mADEsData) {
                       aml_audio_free(demux_info->mADEsData);
                       demux_info->mADEsData =  NULL;
                    }

                    pthread_mutex_unlock(&aml_dev->dtv_lock);
                    continue;
                } else {
                    if (dtv_pacakge == NULL) {
                        dtv_pacakge = aml_audio_calloc(1, sizeof(struct package));
                        if (!dtv_pacakge) {
                            ALOGI("dtv_pacakge malloc failed ");
                            goto exit;
                        }

                        /* get dtv main and ad pakcage */
                        while (!patch->input_thread_exit) {
                            /* get main data */
                            if (mEsData == NULL) {
                                nRet = Get_MainAudio_Es(demux_handle,&mEsData);
                                if (nRet != AM_AUDIO_Dmx_SUCCESS) {
                                    if (path_index == dtv_audio_instances->demux_index_working) {
                                        trycount++;
                                        ALOGV("trycount %d", trycount);
                                        if (trycount > 4 ) {
                                            trycount = 0;
                                            break;
                                        } else {
                                           usleep(5000);
                                           continue;
                                        }
                                    } else {
                                        trycount = 0;
                                        break;
                                    }
                                } else {
                                   trycount = 0;
                                   if (aml_dev->debug_flag)
                                       ALOGI("mEsData->size %d",mEsData->size);
                                }
                            }

                            /* get ad data */
                            if (mAdEsData == NULL) {
                                if (demux_info->dual_decoder_support && VALID_PID(demux_info->ad_pid)) {
                                    nRet = Get_ADAudio_Es(demux_handle, &mAdEsData);
                                    if (nRet != AM_AUDIO_Dmx_SUCCESS) {
                                         /*trycount++;
                                        ALOGV("ad trycount %d", trycount);
                                        if (trycount < max_trycount) {
                                            usleep(10000);
                                            continue;
                                        }*/
                                    }

                                    if (mAdEsData == NULL) {
                                        ALOGI("do not get ad es data trycount %d", trycount);
                                        demux_info->ad_package_status = AD_PACK_STATUS_HOLD;
                                        break;
                                    } else {
                                        ALOGV("ad trycount %d", trycount);

                                        if (aml_dev->debug_flag)
                                           ALOGI("ad mAdEsData->size %d mAdEsData->pts %0llx",mAdEsData->size, mAdEsData->pts);
                                        /* align ad and main data by pts compare */
                                        demux_info->ad_package_status = check_ad_package_status(mEsData->pts, mAdEsData->pts, demux_info);
                                        if (demux_info->ad_package_status == AD_PACK_STATUS_DROP) {
                                            if (mAdEsData->data) {
                                                aml_audio_free(mAdEsData->data);
                                                mAdEsData->data = NULL;
                                            }
                                            aml_audio_free(mAdEsData);
                                            mAdEsData = NULL;
                                            trycount = 0;
                                            demux_info->mADEsData = NULL;
                                            continue;
                                        } else if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                                            demux_info->mADEsData = mAdEsData;
                                            ALOGI("hold ad data to wait main data ");
                                        }
                                        break;
                                    }
                                } else {
                                    break;
                                }

                            } else {
                                break;
                            }
                        }

                         /* dtv pack main ad and data data */
                        {
                            if (mEsData) {
                                dtv_pacakge->size = mEsData->size;
                                dtv_pacakge->data = (char *)mEsData->data;
                                dtv_pacakge->pts = mEsData->pts;
                                aml_audio_free(mEsData);
                                mEsData = NULL;
                                demux_info->mEsData = NULL;
                                demux_info->dtv_pacakge = dtv_pacakge;
                            } else {
                                if (aml_dev->debug_flag > 0)
                                    ALOGI(" do not get mEsData");
                                if (dtv_pacakge) {
                                    aml_audio_free(dtv_pacakge);
                                    dtv_pacakge = NULL;
                                }
                                demux_info->dtv_pacakge = NULL;
                                demux_info->mEsData = NULL;
                                pthread_mutex_unlock(&aml_dev->dtv_lock);
                                usleep(10000);
                                continue;
                            }

                            if (mAdEsData) {
                                demux_info->ad_package_status = check_ad_package_status(dtv_pacakge->pts, mAdEsData->pts, demux_info);
                                if (demux_info->ad_package_status == AD_PACK_STATUS_HOLD) {
                                    dtv_pacakge->ad_size = 0;
                                    dtv_pacakge->ad_data = NULL;
                                } else {
                                    dtv_pacakge->ad_size = mAdEsData->size;
                                    dtv_pacakge->ad_data = (char *)mAdEsData->data;
                                    aml_audio_free(mAdEsData);
                                    mAdEsData = NULL;
                                }
                                demux_info->mADEsData = mAdEsData;
                            } else {
                                dtv_pacakge->ad_size = 0;
                                dtv_pacakge->ad_data = NULL;
                                demux_info->mADEsData = NULL;
                            }
                        }

                    }

                     /* mediasyn check dmx package */
                     {
                        audio_queue_info.apts = dtv_pacakge->pts;
                        audio_queue_info.duration = Dtvsync->duration;
                        audio_queue_info.size = dtv_pacakge->size;
                        audio_queue_info.isneedupdate = false;

                        if (path_index == dtv_audio_instances->demux_index_working)
                            audio_queue_info.isworkingchannel = true;
                        else
                            audio_queue_info.isworkingchannel = false;
                        audio_queue_info.tunit = MEDIASYNC_UNIT_PTS;
                        aml_dtvsync_queue_audio_frame(Dtvsync, &audio_queue_info);
                        if (aml_dev->debug_flag > 0)
                             ALOGI("queue pts:%llx, size:%d, dur:%d isneedupdate %d\n",
                                 dtv_pacakge->pts, dtv_pacakge->size, Dtvsync->duration,audio_queue_info.isneedupdate);
                        if (path_index != dtv_audio_instances->demux_index_working ) {
                            if (aml_dev->debug_flag > 0)
                                ALOGI("path_index  %d dtv_audio_instances->demux_index_working %d",
                                path_index, dtv_audio_instances->demux_index_working);
                            if (audio_queue_info.isneedupdate) {
                                if (dtv_pacakge) {
                                   if (dtv_pacakge->data) {
                                       aml_audio_free(dtv_pacakge->data);
                                       dtv_pacakge->data = NULL;
                                   }
                                   if (dtv_pacakge->ad_data) {
                                       aml_audio_free(dtv_pacakge->ad_data);
                                       dtv_pacakge->ad_data = NULL;
                                   }
                                   aml_audio_free(dtv_pacakge);
                                   dtv_pacakge = NULL;
                                }
                            }

                            demux_info->dtv_pacakge = dtv_pacakge;
                            pthread_mutex_unlock(&aml_dev->dtv_lock);
                            if (!audio_queue_info.isneedupdate &&
                                dtv_audio_instances->demux_index_working == -1) {
                                usleep(5000);
                            }
                            continue;
                        }
                    }

                    /* add dtv package to package list */
                    while (!patch->input_thread_exit &&
                        path_index == dtv_audio_instances->demux_index_working) {
                        pthread_mutex_lock(&patch->mutex);
                        if (dtv_pacakge) {
                            ret = dtv_package_add(list, dtv_pacakge);
                            if (ret == 0) {
                                if (aml_dev->debug_flag > 0)
                                    ALOGI("pthread_cond_signal dtv_pacakge %p ", dtv_pacakge);
                                pthread_cond_signal(&patch->cond);
                                pthread_mutex_unlock(&patch->mutex);
                                dtv_pacakge = NULL;
                                demux_info->dtv_pacakge = NULL;
                                break;
                            } else {
                                ALOGI("list->pack_num %d full !!!", list->pack_num);
                                pthread_mutex_unlock(&patch->mutex);
                                pthread_mutex_unlock(&aml_dev->dtv_lock);
                                usleep(150000);
                                pthread_mutex_lock(&aml_dev->dtv_lock);
                                continue;
                            }
                        }
                    }
                }
                pthread_mutex_unlock(&aml_dev->dtv_lock);
                usleep(5000);
            }
        }
    }
exit:

    if (inbuf) {
        aml_audio_free(inbuf);
        inbuf = NULL;
    }
    if (ad_buffer != NULL) {
        aml_audio_free(ad_buffer);
        ad_buffer = NULL;
    }
    if (!aml_dev->is_multi_demux ) {
        if (dtv_pacakge) {
           if (dtv_pacakge->data) {
               aml_audio_free(dtv_pacakge->data);
               dtv_pacakge->data = NULL;
           }
           if (dtv_pacakge->ad_data) {
               aml_audio_free(dtv_pacakge->ad_data);
               dtv_pacakge->ad_data = NULL;
           }
           aml_audio_free(dtv_pacakge);
           dtv_pacakge = NULL;
        }
    } else {
        for (path_index = 0; path_index < DVB_DEMUX_SUPPORT_MAX_NUM; path_index++ ) {

            pthread_mutex_lock(&aml_dev->dtv_lock);
            demux_info = &dtv_audio_instances->demux_info[path_index];
            dtv_pacakge = demux_info->dtv_pacakge;

            if (dtv_pacakge) {
               if (dtv_pacakge->data) {
                   aml_audio_free(dtv_pacakge->data);
                   dtv_pacakge->data = NULL;
               }
               if (dtv_pacakge->ad_data) {
                   aml_audio_free(dtv_pacakge->ad_data);
                   dtv_pacakge->ad_data = NULL;
               }
               aml_audio_free(dtv_pacakge);
               dtv_pacakge = NULL;
            }
            demux_info->dtv_pacakge = NULL;
            pthread_mutex_unlock(&aml_dev->dtv_lock);
        }
    }

    dtv_package_list_flush(list);

    ALOGI("--%s leave patch->input_thread_exit %d ", __FUNCTION__, patch->input_thread_exit);
    return ((void *)0);
}
float aml_audio_get_output_speed(struct aml_stream_out *aml_out)
{
    char buf[127];
    int ret = -1;
    float speed = aml_out->output_speed;
    ret = property_get("vendor.media.audio.output.speed", buf, NULL);
    if (ret > 0) {
        speed = atof(buf);
        if (fabs(speed - aml_out->output_speed) > 1e-6)
            ALOGI("prop set speed change from %f to %f\n",
                    aml_out->output_speed, speed);
    }
    return speed;
}

void *audio_dtv_patch_output_threadloop_v2(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    package_list *list = patch->dtv_package_list;
    struct audio_stream_out *stream_out = NULL;
    struct aml_stream_out *aml_out = NULL;
    struct audio_config stream_config;
    int write_bytes = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    int ret;
    float last_out_speed = 1.0f;
    int apts_diff = 0;
    struct timespec ts;

    ALOGI("[audiohal_kpi]++%s created.", __FUNCTION__);
    // FIXME: get actual configs
    stream_config.sample_rate = 48000;
    stream_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    stream_config.format = AUDIO_FORMAT_PCM_16_BIT;
    /*
    may we just exit from a direct active stream playback
    still here.we need remove to standby to new playback
    */
    pthread_mutex_lock(&aml_dev->lock);
    aml_out = direct_active(aml_dev);
    if (aml_out) {
        ALOGI("%s live stream %p active,need standby aml_out->usecase:%d ",
              __func__, aml_out, aml_out->usecase);
        pthread_mutex_unlock(&aml_dev->lock);
        aml_dev->continuous_audio_mode = 0;
        /*
        there are several output cases. if there are no ms12 or submixing modules.
        we will output raw/lpcm directly.we need close device directly.
        we need call standy function to release the direct stream
        */
        out_standby_new((struct audio_stream *)aml_out);
        pthread_mutex_lock(&aml_dev->lock);
        if (aml_dev->need_remove_conti_mode == true) {
            ALOGI("%s,conntinous mode still there,release ms12 here", __func__);
            aml_dev->need_remove_conti_mode = false;
            aml_dev->continuous_audio_mode = 0;
        }
    } else {
        ALOGI("++%s live cant get the aml_out now!!!\n ", __FUNCTION__);
    }
    aml_dev->mix_init_flag = false;
    aml_dev->mute_start = true;
    pthread_mutex_unlock(&aml_dev->lock);
#ifdef TV_AUDIO_OUTPUT
    patch->output_src = AUDIO_DEVICE_OUT_SPEAKER;
#else
    patch->output_src = AUDIO_DEVICE_OUT_AUX_DIGITAL;
#endif
    if (aml_dev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
        patch->output_src = aml_dev->out_device;

    ret = adev_open_output_stream_new(patch->dev, 0,
                                      patch->output_src,        // devices_t
                                      AUDIO_OUTPUT_FLAG_DIRECT, // flags
                                      &stream_config, &stream_out, "AML_DTV_SOURCE");
    if (ret < 0) {
        ALOGE("live open output stream fail, ret = %d", ret);
        goto exit_open;
    }
    aml_out = (struct aml_stream_out *)stream_out;
    ALOGI("++%s live create a output stream success now!!!\n ", __FUNCTION__);
    patch->out_buf_size = write_bytes * EAC3_MULTIPLIER;
    patch->out_buf = aml_audio_calloc(1, patch->out_buf_size);
    if (!patch->out_buf) {
        ret = -ENOMEM;
        goto exit_outbuf;
    }
    ALOGI("patch->out_buf_size=%d\n", patch->out_buf_size);
    //patch->dtv_audio_mode = get_dtv_audio_mode();
    patch->dtv_audio_tune = AUDIO_FREE;
    patch->first_apts_lookup_over = 0;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    ALOGI("[audiohal_kpi]++%s live start output pcm now patch->output_thread_exit %d!!!\n ",
          __FUNCTION__, patch->output_thread_exit);

    prctl(PR_SET_NAME, (unsigned long)"audio_output_patch");
    aml_out->output_speed = 1.0f;
    aml_out->dtvsync_enable =  property_get_int32("vendor.media.dtvsync.enable", 1);
    ALOGI("output_speed=%f,dtvsync_enable=%d\n", aml_out->output_speed, aml_out->dtvsync_enable);
    while (!patch->output_thread_exit) {

        if (patch->dtv_decoder_state == AUDIO_DTV_PATCH_DECODER_STATE_PAUSE) {
            usleep(1000);
            continue;
        }

        pthread_mutex_lock(&patch->mutex);

        struct package *p_package = NULL;
        p_package = dtv_package_get(list);
        if (!p_package) {
            ts_wait_time(&ts, 100000);
            pthread_cond_timedwait(&patch->cond, &patch->mutex, &ts);
            //pthread_cond_wait(&patch->cond, &patch->mutex);
            pthread_mutex_unlock(&patch->mutex);
            continue;
        } else {
          patch->cur_package = p_package;
          ALOGV("p_package->size %d",p_package->size);
        }

        if (last_out_speed != aml_out->output_speed) {
            ALOGI("[%s-%d] speed change from %f to %f get_sink_format again", __func__, __LINE__,
                last_out_speed, aml_out->output_speed);
            get_sink_format(stream_out);
        }
        last_out_speed = aml_audio_get_output_speed(aml_out);
        aml_out->output_speed = last_out_speed;
        pthread_mutex_unlock(&patch->mutex);
        pthread_mutex_lock(&(patch->dtv_output_mutex));

        aml_out->codec_type = get_codec_type(patch->aformat);
        if (aml_out->hal_internal_format != patch->aformat) {
            aml_out->hal_format = aml_out->hal_internal_format = patch->aformat;
            get_sink_format(stream_out);
        }

        if (patch->dtvsync) {
            if (aml_dev->bHDMIConnected_update || aml_dev->a2dp_updated) {
                ALOGI("reset_dtvsync (mediasync:%p)", patch->dtvsync->mediasync);
                aml_dtvsync_reset(patch->dtvsync);
            }
        }

        ALOGV("AD %d %d %d", aml_dev->dolby_lib_type, demux_info->dual_decoder_support, demux_info->ad_pid);

        if (need_enable_dual_decoder(patch)) {
            ret = audio_dtv_patch_output_dual_decoder(patch, stream_out);
        } else {
            ret = audio_dtv_patch_output_single_decoder(patch, stream_out);
        }

        aml_dec_t *aml_dec = aml_out->aml_dec;
        if (aml_dev->mixing_level != aml_out->dec_config.mixer_level ) {
            aml_out->dec_config.mixer_level = aml_dev->mixing_level;
            aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_MIXER_LEVEL, &aml_out->dec_config);
        }
        if (demux_info->associate_audio_mixing_enable != aml_out->dec_config.ad_mixing_enable) {
           aml_out->dec_config.ad_mixing_enable = demux_info->associate_audio_mixing_enable;
           aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_MIXING_ENABLE, &aml_out->dec_config);
        }
        if (aml_dev->advol_level != aml_out->dec_config.advol_level) {
           aml_out->dec_config.advol_level = aml_dev->advol_level;
           aml_decoder_set_config(aml_dec, AML_DEC_CONFIG_AD_VOL, &aml_out->dec_config);
        }

        pthread_mutex_unlock(&(patch->dtv_output_mutex));
    }
    aml_audio_free(patch->out_buf);
exit_outbuf:
    ALOGI("patch->output_thread_exit %d", patch->output_thread_exit);
    do_output_standby_l((struct audio_stream *)aml_out);
    adev_close_output_stream_new(dev, stream_out);
exit_open:
    if (aml_dev->audio_ease) {
        aml_dev->patch_start = false;
    }
    if (get_video_delay() != 0) {
        set_video_delay(0);
    }

    if (patch->dtvsync) {
        ALOGI("reset_dtvsync (mediasync:%p)", patch->dtvsync->mediasync);
        aml_dtvsync_reset(patch->dtvsync);
    }
    ALOGI("--%s ", __FUNCTION__);
    return ((void *)0);
}


static void *audio_dtv_patch_process_threadloop_v2(void *data)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)data;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct audio_stream_in *stream_in = NULL;
    struct audio_config stream_config;
    // FIXME: add calc for read_bytes;
    int read_bytes = DEFAULT_CAPTURE_PERIOD_SIZE * CAPTURE_PERIOD_COUNT;
    int ret = 0, retry = 0;
    audio_format_t cur_aformat;
    int cmd = AUDIO_DTV_PATCH_CMD_NUM;
    int path_id  = 0;
    aml_dtvsync_t *dtvsync;
    struct mediasync_audio_format audio_format;
    patch->sample_rate = stream_config.sample_rate = 48000;
    patch->chanmask = stream_config.channel_mask = AUDIO_CHANNEL_IN_STEREO;
    patch->aformat = stream_config.format = AUDIO_FORMAT_PCM_16_BIT;

    int switch_flag = property_get_int32("vendor.media.audio.strategy.switch", 0);
    int show_first_nosync = property_get_int32("vendor.media.video.show_first_frame_nosync", 1);
    patch->pre_latency = property_get_int32(PROPERTY_PRESET_AC3_PASSTHROUGH_LATENCY, 30);
    patch->a_discontinue_threshold = property_get_int32(
                                        PROPERTY_AUDIO_DISCONTINUE_THRESHOLD, 30 * 90000);
    patch->sync_para.cur_pts_diff = 0;
    patch->sync_para.in_out_underrun_flag = 0;
    patch->sync_para.pcr_adjust_max = property_get_int32(
                                        PROPERTY_AUDIO_ADJUST_PCR_MAX, 1 * 90000);
    patch->sync_para.underrun_mute_time_min = property_get_int32(
                                        PROPERTY_UNDERRUN_MUTE_MINTIME, 200);
    patch->sync_para.underrun_mute_time_max = property_get_int32(
                                        PROPERTY_UNDERRUN_MUTE_MAXTIME, 1000);
    patch->sync_para.underrun_max_time =  property_get_int32(
                                        PROPERTY_UNDERRUN_MAX_TIME, 5000);

    ALOGI("switch_flag=%d, show_first_nosync=%d, pre_latency=%d,discontinue:%d\n",
        switch_flag, show_first_nosync, patch->pre_latency,
        patch->a_discontinue_threshold);
    ALOGI("sync:pcr_adjust_max=%d\n", patch->sync_para.pcr_adjust_max);
    ALOGI("[audiohal_kpi]++%s Enter.\n", __FUNCTION__);
    patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
    aml_demux_audiopara_t *demux_info = NULL;
    aml_dtv_audio_instances_t *dtv_audio_instances =  (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    while (!patch->cmd_process_thread_exit ) {

        pthread_mutex_lock(&patch->dtv_cmd_process_mutex);
        switch (patch->dtv_decoder_state) {
        case AUDIO_DTV_PATCH_DECODER_STATE_INIT: {
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }
            patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_START;
            /* +[SE] [BUG][SWPL-21070][yinli.xia] added: turn channel mute audio*/
            if (switch_flag) {
                if (patch->dtv_has_video == 1)
                    aml_dev->start_mute_flag = 1;
            }
            aml_dev->underrun_mute_flag = 0;
            if (show_first_nosync) {
                sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
                ALOGI("show_first_frame_nosync set 1\n");
            }
        }
        break;
        case AUDIO_DTV_PATCH_DECODER_STATE_START:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                // 3s timeout after not get new signal to
                // avoid some rare case which blocked in release_dtv_patch_l() pthread_join
                struct timespec tv;
                clock_gettime(CLOCK_MONOTONIC, &tv);
                tv.tv_sec += 3;
                pthread_cond_timedwait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex, &tv);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            ring_buffer_reset(&patch->aml_ringbuffer);
            if (cmd == AUDIO_DTV_PATCH_CMD_START) {
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
                aml_dev->patch_start = false;
                memset(&patch->dtv_resample, 0, sizeof(struct resample_para));
                if (patch->resample_outbuf) {
                    memset(patch->resample_outbuf, 0, OUTPUT_BUFFER_SIZE * 3);
                }

                if (patch->dtv_aformat == ACODEC_FMT_AC3) {
                    patch->aformat = AUDIO_FORMAT_AC3;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_EAC3) {
                    patch->aformat = AUDIO_FORMAT_E_AC3;

                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_DTS) {
                    patch->aformat = AUDIO_FORMAT_DTS;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC ) {
                    patch->aformat = AUDIO_FORMAT_AAC;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AAC_LATM) {
                    patch->aformat = AUDIO_FORMAT_AAC_LATM;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG ) {
                    patch->aformat = AUDIO_FORMAT_MP3;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG1) {
                    patch->aformat = AUDIO_FORMAT_MP2;/* audio-base.h no define mpeg1 format */
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_MPEG2) {
                    patch->aformat = AUDIO_FORMAT_MP2;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_AC4) {
                   patch->aformat = AUDIO_FORMAT_AC4;
                   patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_DRA) {
                   patch->aformat = AUDIO_FORMAT_DRA;
                   patch->decoder_offset = 0;
                } else {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                }

                ALOGI("patch->demux_handle %p patch->aformat %0x", patch->demux_handle, patch->aformat);
                dtvsync = &dtv_audio_instances->dtvsync[path_id];
                patch->dtvsync = dtvsync;
                if (dtvsync->mediasync_new != NULL) {
                    audio_format.format = patch->dtv_aformat;
                    mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_AUDIOFORMAT, &audio_format);
                    mediasync_wrap_setParameter(dtvsync->mediasync_new, MEDIASYNC_KEY_HASVIDEO, &patch->dtv_has_video);
                    patch->dtvsync->mediasync = dtvsync->mediasync_new;
                    dtvsync->mediasync_new = NULL;
                }
                patch->dtv_first_apts_flag = 0;
                patch->outlen_after_last_validpts = 0;
                patch->last_valid_pts = 0;
                patch->first_apts_lookup_over = 0;
                patch->ac3_pcm_dropping = 0;
                patch->last_audio_delay = 0;
                patch->pcm_inserting = false;
                patch->tsync_pcr_debug = get_tsync_pcr_debug();
                patch->startplay_firstvpts = 0;
                patch->startplay_first_checkinapts = 0;
                patch->startplay_pcrpts = 0;
                patch->startplay_apts_lookup = 0;
                patch->startplay_vpts = 0;

                patch->dtv_pcm_readed = patch->dtv_pcm_writed = 0;
                patch->numDecodedSamples = patch->numOutputSamples = 0;
                create_dtv_input_stream_thread(patch);
                create_dtv_output_stream_thread(patch);
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        case AUDIO_DTV_PATCH_DECODER_STATE_RUNING:

            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }

            if (aml_dev->ad_start_enable == 0 && need_enable_dual_decoder(patch)) {
                int ad_start_flag;

                if (aml_dev->is_multi_demux) {
                    ad_start_flag = 0;
                } else {
                    ad_start_flag = dtv_assoc_audio_start(1, demux_info->ad_pid, demux_info->ad_fmt, demux_info->demux_id);
                }
                if (ad_start_flag == 0) {
                    aml_dev->ad_start_enable = 1;
                }
            }

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                // 3s timeout after not get new signal to
                // avoid some rare case which blocked in release_dtv_patch_l() pthread_join
                struct timespec tv;
                clock_gettime(CLOCK_MONOTONIC, &tv);
                tv.tv_sec += 3;
                pthread_cond_timedwait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex, &tv);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_PAUSE) {
                ALOGI("++%s live now start  pause  the audio decoder now \n",
                      __FUNCTION__);
                if (aml_dev->is_multi_demux) {
                    path_id = dtv_audio_instances->demux_index_working;
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[path_id];
                    if (dtvsync->mediasync) {
                        aml_dtvsync_setPause(dtvsync, true);
                    }
                }  else {
                    dtv_assoc_audio_pause(1);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_PAUSE;
                ALOGI("++%s live now end  pause  the audio decoder now \n",
                      __FUNCTION__);
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                      __FUNCTION__);
               // tv_do_ease_out(aml_dev);
                release_dtv_output_stream_thread(patch);
                dtv_package_list_flush(patch->dtv_package_list);
                dtv_audio_instances->demux_index_working = -1;
                dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                dtv_assoc_audio_stop(1);
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
           } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;

        case AUDIO_DTV_PATCH_DECODER_STATE_PAUSE:

            if (patch_thread_get_cmd(patch, &cmd, &path_id) != 0) {
                // 3s timeout after not get new signal to
                // avoid some rare case which blocked in release_dtv_patch_l() pthread_join
                struct timespec tv;
                clock_gettime(CLOCK_MONOTONIC, &tv);
                tv.tv_sec += 3;
                pthread_cond_timedwait(&patch->dtv_cmd_process_cond, &patch->dtv_cmd_process_mutex, &tv);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }

            if (cmd == AUDIO_DTV_PATCH_CMD_RESUME) {
                if (aml_dev->is_multi_demux) {
                    aml_dtvsync_t *dtvsync = &dtv_audio_instances->dtvsync[path_id];
                    if (dtvsync->mediasync) {
                        aml_dtvsync_setPause(dtvsync, false);
                    }
                } else {
                    dtv_assoc_audio_resume(1,demux_info->ad_pid);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                     __FUNCTION__);
                /*
                 * When stop after pause, need release output thread.
                 * Or it will lead next channel no sound which has diff format.
                 * And can't add flush action, it maybe will lead freeze.
                 * */
                release_dtv_output_stream_thread(patch);
                dtv_audio_instances->demux_index_working = -1;
                dtv_assoc_audio_stop(1);
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                continue;
            }
            break;
        default:
            if (patch->cmd_process_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
                goto exit;
            }
            break;
        }
        pthread_mutex_unlock(&patch->dtv_cmd_process_mutex);
    }

exit:
    ALOGI("[audiohal_kpi]++%s now  live  release  the audio decoder", __FUNCTION__);
    release_dtv_input_stream_thread(patch);
    release_dtv_output_stream_thread(patch);
    aml_dev->ad_start_enable = 0;
    dtv_assoc_audio_stop(1);
    dtv_check_audio_reset();
    ALOGI("[audiohal_kpi]++%s Exit", __FUNCTION__);
    pthread_exit(NULL);
}

static int create_dtv_output_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    struct aml_audio_device *adev = (struct aml_audio_device *)patch->dev;
    ALOGI("++%s   ---- %d\n", __FUNCTION__, patch->ouput_thread_created);

    if (patch->ouput_thread_created == 0) {
        patch->output_thread_exit = 0;
        pthread_mutex_init(&patch->dtv_output_mutex, NULL);
        patch->dtv_replay_flag = true;
        start_ease_in(adev->audio_ease);
        if (patch->skip_amadec_flag) {
            ret = pthread_create(&(patch->audio_output_threadID), NULL,
                                 audio_dtv_patch_output_threadloop_v2, patch);
            if (ret != 0) {
                ALOGE("%s, Create output thread fail!\n", __FUNCTION__);
                pthread_mutex_destroy(&patch->dtv_output_mutex);
                return -1;
            }

        } else {
            ret = pthread_create(&(patch->audio_output_threadID), NULL,
                                 audio_dtv_patch_output_threadloop, patch);
            if (ret != 0) {
                ALOGE("%s, Create output thread fail!\n", __FUNCTION__);
                pthread_mutex_destroy(&patch->dtv_output_mutex);
                return -1;
            }
        }

        patch->ouput_thread_created = 1;
    }
    ALOGI("--%s", __FUNCTION__);
    return 0;
}

static int release_dtv_output_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    ALOGI("++%s   ---- %d\n", __FUNCTION__, patch->ouput_thread_created);
    if (patch->ouput_thread_created == 1) {
        patch->output_thread_exit = 1;
        pthread_join(patch->audio_output_threadID, NULL);
        pthread_mutex_destroy(&patch->dtv_output_mutex);
        patch->ouput_thread_created = 0;
    }
    ALOGI("--%s", __FUNCTION__);
    return 0;
}

static int create_dtv_input_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    ALOGI("++%s   ---- %d\n", __FUNCTION__, patch->input_thread_created);

    if (patch->input_thread_created == 0) {
        patch->input_thread_exit = 0;
        pthread_mutex_init(&patch->dtv_input_mutex, NULL);
        patch->dtv_replay_flag = true;
        ret = pthread_create(&(patch->audio_input_threadID), NULL,
                             audio_dtv_patch_input_threadloop, patch);
        if (ret != 0) {
            ALOGE("%s, Create output thread fail!\n", __FUNCTION__);
            pthread_mutex_destroy(&patch->dtv_input_mutex);
            return -1;
        }

        patch->input_thread_created = 1;
    }
    ALOGI("--%s", __FUNCTION__);
    return 0;
}

static int release_dtv_input_stream_thread(struct aml_audio_patch * patch)
{
    int ret = 0;
    ALOGI("++%s   ---- %d\n", __FUNCTION__, patch->input_thread_created);
    if (patch->input_thread_created == 1) {
        patch->input_thread_exit = 1;
        pthread_join(patch->audio_input_threadID, NULL);
        pthread_mutex_destroy(&patch->dtv_input_mutex);
        patch->input_thread_created = 0;
    }
    ALOGI("--%s", __FUNCTION__);
    return 0;
}

int create_dtv_patch_l(struct audio_hw_device *dev, audio_devices_t input,
                       audio_devices_t output __unused)
{
    struct aml_audio_patch *patch;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_demux_audiopara_t *demux_audiopara;
    int period_size = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    pthread_attr_t attr;
    struct sched_param param;
    aml_demux_audiopara_t *demux_info = NULL;
    int ret = 0;
    // ALOGI("++%s live period_size %d\n", __func__, period_size);
    //pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->audio_patch) {
        ALOGD("%s: patch exists, first release it", __func__);
        if (aml_dev->audio_patch->is_dtv_src) {
            return ret;
            //release_dtv_patch_l(aml_dev);
        } else {
            release_patch_l(aml_dev);
        }
    }
    patch = aml_audio_calloc(1, sizeof(*patch));
    if (!patch) {
        ret = -1;
        goto err;
    }

    if (aml_dev->dtv_i2s_clock == 0) {
        aml_dev->dtv_i2s_clock = aml_mixer_ctrl_get_int(&(aml_dev->alsa_mixer), AML_MIXER_ID_CHANGE_I2S_PLL);
        aml_dev->dtv_spidif_clock = aml_mixer_ctrl_get_int(&(aml_dev->alsa_mixer), AML_MIXER_ID_CHANGE_SPIDIF_PLL);
    }


    // save dev to patch
    patch->dev = dev;
    patch->input_src = input;
    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
    patch->is_dtv_src = true;
    patch->startplay_avsync_flag = 1;
    patch->ad_substream_checked_flag = false;

    patch->output_thread_exit = 0;
    patch->cmd_process_thread_exit = 0;
    memset(&patch->sync_para, 0, sizeof(struct avsync_para));

    patch->i2s_div_factor = property_get_int32(PROPERTY_AUDIO_TUNING_CLOCK_FACTOR, DEFAULT_TUNING_CLOCK_FACTOR);
    if (patch->i2s_div_factor == 0)
        patch->i2s_div_factor = DEFAULT_TUNING_CLOCK_FACTOR;

    aml_dev->audio_patch = patch;
    pthread_mutex_init(&patch->mutex, NULL);
    pthread_cond_init(&patch->cond, NULL);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);
    pthread_cond_init(&patch->dtv_cmd_process_cond, &cattr);

    pthread_mutex_init(&patch->dtv_cmd_process_mutex, NULL);
    pthread_mutex_init(&patch->apts_cal_mutex, NULL);

    patch->dtv_cmd_list = aml_audio_calloc(1, sizeof(struct cmd_node));
    if (!patch->dtv_cmd_list) {
        ret = -1;
        goto err;
    }

    init_cmd_list(patch->dtv_cmd_list);
    ret = ring_buffer_init(&(patch->aml_ringbuffer),
                           4 * period_size * PATCH_PERIOD_COUNT * 10);
    if (ret < 0) {
        ALOGE("Fail to init audio ringbuffer!");
        goto err_ring_buf;
    }
    patch->dtv_package_list = aml_audio_calloc(1, sizeof(package_list));
    if (!patch->dtv_package_list) {
        ret = -1;
        goto err;
    }

    if (aml_dev->useSubMix) {
        // switch normal stream to old tv mode writing
        switchNormalStream(aml_dev->active_outputs[STREAM_PCM_NORMAL], 0);
    }

    /* now  only sc2 can use new dtv path */
    if (property_get_bool("vendor.dtv.audio.skipamadec",true) && aml_dev->is_multi_demux) {
        patch->skip_amadec_flag = true;
    } else {
        patch->skip_amadec_flag = false;
    }

    if (patch->skip_amadec_flag) {
        ret = pthread_create(&(patch->audio_cmd_process_threadID), NULL,
                             audio_dtv_patch_process_threadloop_v2, patch);
        if (ret != 0) {
            ALOGE("%s, Create process thread fail!\n", __FUNCTION__);
            goto err_in_thread;
        }
    } else {
        ret = pthread_create(&(patch->audio_cmd_process_threadID), NULL,
                             audio_dtv_patch_process_threadloop, patch);
        if (ret != 0) {
            ALOGE("%s, Create process thread fail!\n", __FUNCTION__);
            goto err_in_thread;
        }
    }

    if (aml_dev->dev2mix_patch) {
        create_tvin_buffer(patch);
    }
    dtv_assoc_init();
    patch->dtv_aformat = aml_dev->dtv_aformat;
    patch->mode = aml_dev->dtv_sound_mode;
    patch->dtv_output_clock = 0;
    patch->dtv_default_i2s_clock = aml_dev->dtv_i2s_clock;
    patch->dtv_default_spdif_clock = aml_dev->dtv_spidif_clock;
    patch->spdif_format_set = 0;
    patch->avsync_callback = dtv_avsync_process;
    patch->spdif_step_clk = 0;
    patch->i2s_step_clk = 0;
    patch->pid = -1;
    patch->debug_para.debug_last_checkin_apts = 0;
    patch->debug_para.debug_last_checkin_vpts = 0;
    patch->debug_para.debug_last_out_apts = 0;
    patch->debug_para.debug_last_out_vpts = 0;
    patch->debug_para.debug_last_demux_pcr = 0;
    patch->debug_para.debug_time_interval = property_get_int32(PROPERTY_DEBUG_TIME_INTERVAL, DEFULT_DEBUG_TIME_INTERVAL);

    ALOGI("--%s", __FUNCTION__);
    return 0;
err_parse_thread:
err_out_thread:
    patch->cmd_process_thread_exit = 1;
    pthread_join(patch->audio_cmd_process_threadID, NULL);
err_in_thread:
    ring_buffer_release(&(patch->aml_ringbuffer));
err_ring_buf:
    aml_audio_free(patch);
err:
    return ret;
}

int release_dtv_patch_l(struct aml_audio_device *aml_dev)
{
    if (aml_dev == NULL) {
        ALOGI("[%s]release the dtv patch aml_dev == NULL\n", __FUNCTION__);
        return 0;
    }
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    struct audio_hw_device *dev   = (struct audio_hw_device *)aml_dev;
    ALOGI("[audiohal_kpi]++%s Enter\n", __FUNCTION__);
    if (patch == NULL) {
        ALOGI("release the dtv patch patch == NULL\n");
        return 0;
    }

    patch->cmd_process_thread_exit = 1;
    pthread_cond_signal(&patch->dtv_cmd_process_cond);
    pthread_join(patch->audio_cmd_process_threadID, NULL);
    pthread_mutex_destroy(&patch->dtv_cmd_process_mutex);
    pthread_cond_destroy(&patch->dtv_cmd_process_cond);
    if (patch->resample_outbuf) {
        aml_audio_free(patch->resample_outbuf);
        patch->resample_outbuf = NULL;
    }
    patch->pid = -1;
    release_tvin_buffer(patch);
    if (patch->dtv_package_list)
        aml_audio_free(patch->dtv_package_list);
    deinit_cmd_list(patch->dtv_cmd_list);
    patch->dtv_cmd_list = NULL;
    dtv_assoc_deinit();
    ring_buffer_release(&(patch->aml_ringbuffer));

    aml_audio_free(patch);

    if (aml_dev->start_mute_flag != 0)
        aml_dev->start_mute_flag = 0;
    aml_dev->underrun_mute_flag = 0;
    aml_dev->audio_patch = NULL;
    ALOGI("[audiohal_kpi]--%s Exit", __FUNCTION__);

    if (aml_dev->useSubMix) {
        switchNormalStream(aml_dev->active_outputs[STREAM_PCM_NORMAL], 1);
    }
    return 0;
}

int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input,
                     audio_devices_t output)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    aml_dtv_audio_instances_t *dtv_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;
    int ret = 0;
    pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->patch_src == SRC_DTV)
        dtv_instances->dvb_path_count++;
    ALOGI("dtv_instances->dvb_path_count %d",dtv_instances->dvb_path_count);
    ret = create_dtv_patch_l(dev, input, output);
    pthread_mutex_unlock(&aml_dev->patch_lock);

    return ret;
}

int release_dtv_patch(struct aml_audio_device *aml_dev)
{
    int ret = 0;
    aml_dtv_audio_instances_t *dtv_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;

    pthread_mutex_lock(&aml_dev->patch_lock);
    if (dtv_instances->dvb_path_count) {
        if (aml_dev->patch_src == SRC_DTV )
            dtv_instances->dvb_path_count--;
        ALOGI("dtv_instances->dvb_path_count %d",dtv_instances->dvb_path_count);
        if (dtv_instances->dvb_path_count == 0) {
            tv_do_ease_out(aml_dev);
            ret = release_dtv_patch_l(aml_dev);
        }
    }

    aml_dev->patch_start = false;
    pthread_mutex_unlock(&aml_dev->patch_lock);
    return ret;
}

#if ANDROID_PLATFORM_SDK_VERSION > 29
int enable_dtv_patch_for_tuner_framework(struct audio_config *config, struct audio_hw_device *dev)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int ret = 0, val=0;

    /*1.only when config has valid content id and sync id*/
    if (config->offload_info.content_id != 0 && config->offload_info.sync_id != 0)
    {
        if ((adev->patch_src == SRC_DTV) && adev->audio_patching) {
            /*2.check if old dtv patch exists*/
            ALOGI("[audiohal_kpi] %s, now release the dtv patch now\n ", __func__);
            ret = release_dtv_patch(adev);
            if (!ret) {
                adev->audio_patching = 0;
            }
        }
        adev->patch_src = SRC_DTV;
        adev->out_device = 0x400;
        /*3.create audio dtv patch*/
        ret = create_dtv_patch(dev, AUDIO_DEVICE_IN_TV_TUNER, AUDIO_DEVICE_OUT_SPEAKER);
        if (ret == 0) {
            adev->audio_patching = 1;
        }

        /*4.parser demux id from offload_info, then set it. tuner/filter.cpp for reference.*/
        val = config->offload_info.content_id >> 16;//demux id
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO, val);

        /*parser pid from offload_info, then set it. tuner/filter.cpp for reference.*/
        val = config->offload_info.content_id & 0x0000FFFF;//pid
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);

        /*parser pid from offload_info, then set it.*/
        val = config->offload_info.sync_id;//sync id
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID, val);

        /*parser format from offload_info, then set it.*/
        val = android_fmt_convert_to_dmx_fmt(config->offload_info.format);//fmt
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);

        /*set security_mem_level. 0 for tunerframework.*/
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL, 0);

        /*5.make dtv patch work via cmds.*/
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_OPEN);
        ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_START);

        ALOGD("%s[%d]:the audio_patching is %d, ret:%d", __func__, __LINE__,adev->audio_patching, ret);
    }
    return ret;
}

int disable_dtv_patch_for_tuner_framework(struct audio_hw_device *dev)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int ret = 0;

    /*1.make dtv patch stop via cmds*/
    ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_STOP);
    ret = dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, AUDIO_DTV_PATCH_CMD_CLOSE);

    /*2.release dtv patch*/
    ret = release_dtv_patch(adev);
    ALOGD("%s[%d]:the audio_patching is %d, ret:%d", __func__, __LINE__,adev->audio_patching, ret);
    return ret;
}
#endif

bool is_dtv_patch_alive(struct aml_audio_device *aml_dev)
{
    int ret = false;
    aml_dtv_audio_instances_t *dtv_instances = (aml_dtv_audio_instances_t *)aml_dev->aml_dtv_audio_instances;

    if (dtv_instances) {
        pthread_mutex_lock(&aml_dev->patch_lock);
        if (dtv_instances->dvb_path_count == 0) {
            ret = false;
        }
        else {
            ret = true;
        }
        pthread_mutex_unlock(&aml_dev->patch_lock);
    }
    else {
        ret = false;
    }

    return ret;
}

int audio_decoder_status(unsigned int *perror_count)
{
    int ret = 0;

    if (perror_count == NULL) {
        return -1;
    }

    ret = dtv_patch_get_decoder_status(perror_count);

    return ret;
}


int set_dtv_parameters(struct audio_hw_device *dev, struct str_parms *parms)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int ret = -1, val = 0;

    /* dvb cmd deal with start */
    ret = str_parms_get_int(parms, "hal_param_dtv_patch_cmd", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_CONTROL, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dual_dec_support", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT ,val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_ad_mix_enable", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE ,val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_media_sync_id", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID ,val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "ad_switch_enable", &val);
    if (ret >= 0) {
        adev->ad_switch_enable = val;
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE, val);
        ALOGI("ad_switch_enable set to %d\n", adev->associate_audio_mixing_enable);
        goto exit;
    }

    ret = str_parms_get_int(parms, "dual_decoder_advol_level", &val);
    if (ret >= 0) {

        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dual_dec_mix_level", &val);
    if (ret >= 0) {

        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL, val);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hal_param_security_mem_level", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL, val);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hal_param_audio_output_mode", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE, val);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hal_param_dtv_demux_id", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO, val);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hal_param_dtv_pid", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dtv_fmt", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);
        goto exit;
    }
    ret = str_parms_get_int(parms, "hal_param_dtv_audio_fmt", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_FMT, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dtv_audio_id", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_PID, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dtv_sub_audio_fmt", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_FMT, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dtv_sub_audio_pid", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_AD_PID, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_has_dtv_video", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_tv_mute", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MUTE, val);
        goto exit;
    }

    ret = str_parms_get_int(parms, "hal_param_dtv_media_presentation_id", &val);
    if (ret >= 0) {
        dtv_patch_handle_event(dev, AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID, val);
        goto exit;
    }

    /* dvb cmd deal with end */
exit:
    return ret;
}

