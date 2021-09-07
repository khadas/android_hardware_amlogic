/*
 * Copyright (C) 2019 Amlogic Corporation.
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
 *
 * Author:
 * yinli.xia@amlogic.com
 *
 * Function:
 * this file is created for starting play avsync
 */

#define LOG_TAG "aml_audio_hal_avsync"

#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
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
#include "aml_audio_timer.h"
#include "aml_data_utils.h"
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "audio_hw_profile.h"
#include "audio_hw_utils.h"
#include "dtv_patch_out.h"
#include "aml_audio_resampler.h"
#include "audio_hw_ms12.h"
#include "dolby_lib_api.h"
#include "audio_dtv_ad.h"
#include "alsa_device_parser.h"
#include "aml_audio_hal_avsync.h"
#include <audio_dtv_sync.h>

unsigned long decoder_apts_lookup(unsigned int offset)
{
    unsigned int pts = 0;
    int ret;
    char buff[32] = {0};

    snprintf(buff, 32, "%d", offset);
    aml_sysfs_set_str(DTV_DECODER_PTS_LOOKUP_PATH, buff);
    ret = aml_sysfs_get_str(DTV_DECODER_PTS_LOOKUP_PATH, buff, sizeof(buff));

    if (ret > 0) {
        ret = sscanf(buff, "0x%x\n", &pts);
    }
    if (pts == (unsigned int) - 1) {
        pts = 0;
    }
    if (aml_audio_get_debug_flag()) {
        ALOGI("adec_apts_lookup get the pts is %x\n", pts);
    }
    return (unsigned long)pts;
}

void decoder_set_pcrsrc(unsigned int pcrsrc)
{
    char tempbuf[128] = {0};
    /*[SE][BUG][SWPL-21122][chengshun] need add 0x, avoid driver get error*/
    sprintf(tempbuf, "0x%x", pcrsrc);
    if (aml_sysfs_set_str(TSYNC_PCRSCR, tempbuf) == -1) {
        ALOGE("set pcr lantcy failed %s\n", tempbuf);
    }
    return;
}

int get_tsync_pcr_debug(void)
{
    char tempbuf[128] = {0};
    int debug = 0, ret;
    ret = aml_sysfs_get_str(TSYNC_PCR_DEBUG, tempbuf, sizeof(tempbuf));
    if (ret > 0) {
        ret = sscanf(tempbuf, "%d\n", &debug);
    }
    if (ret > 0 && debug > 0) {
        return debug;
    } else {
        debug = 0;
    }
    return debug;
}

void set_video_delay(int delay_ms)
{
    char tempbuf[128] = {0};

    if (delay_ms < -100 || delay_ms > 500) {
        ALOGE("set_video_delay out of range[-100 - 500] %d\n", delay_ms);
        return;
    }
    sprintf(tempbuf, "%d", delay_ms);
    if (aml_sysfs_set_str(TSYNC_VPTS_ADJ, tempbuf) == -1) {
        ALOGE("set_video_delay %s\n", tempbuf);
    }
    return;
}

unsigned long dtv_hal_get_pts(struct aml_audio_patch *patch,
                              unsigned int lantcy)
{
    unsigned long val;
    unsigned long pts;
    unsigned long long frame_nums;
    unsigned long delay_pts;
    unsigned int checkin_firstapts;
    char value[PROPERTY_VALUE_MAX];
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device * aml_dev = (struct aml_audio_device*)adev;

    get_sysfs_uint(DTV_DECODER_CHECKIN_FIRSTAPTS_PATH, &checkin_firstapts);
    if (aml_dev->is_multi_demux && !property_get_bool("vendor.dtv.use_tsync_check",false)) {
        if (aml_audio_swcheck_lookup_apts(0,patch->decoder_offset,&pts) == -1) {
            pts = 0;
        }
    } else {
        pts = decoder_apts_lookup(patch->decoder_offset);
    }
    if (patch->dtv_first_apts_flag == 0) {
        pts = checkin_firstapts;
        ALOGI("pts = 0,so get checkin_firstapts:0x%lx", pts);
        patch->last_valid_pts = pts;
        patch->outlen_after_last_validpts = 0;
        ALOGI("first apts looked=0x%lx\n", pts);
        return pts;
    }

    if (pts == 0 || pts == patch->last_valid_pts) {
        if (patch->last_valid_pts) {
            pts = patch->last_valid_pts;
        }
        frame_nums = (patch->outlen_after_last_validpts / (DEFAULT_DATA_WIDTH * DEFAULT_CHANNELS));
        pts += (frame_nums * 90 / DEFAULT_SAMPLERATE);
        if (aml_audio_get_debug_flag()) {
            ALOGI("decode_offset:%d out_pcm:%d   pts:%lx,audec->last_valid_pts %lx\n",
                   patch->decoder_offset, patch->outlen_after_last_validpts, pts, patch->last_valid_pts);
        }
        patch->cur_outapts = pts;
        return 0;
    }
    val = pts - lantcy * 90;
    /*+[SE][BUG][SWPL-14811][zhizhong] set the real apts to last_valid_pts for sum cal*/
    patch->last_valid_pts = val;
    patch->outlen_after_last_validpts = 0;
    if (aml_audio_get_debug_flag()) {
        ALOGI("====get pts:%lx offset:%d lan %d, origin:apts:%lx \n",
               val, patch->decoder_offset, lantcy, pts);
    }
    patch->cur_outapts = val;
    return val;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    snd_pcm_sframes_t frames = out_get_latency_frames(stream);
    return (frames * 1000) / out->config.rate;
}

static unsigned int compare_clock(unsigned int clock1, unsigned int clock2)
{
    if (clock1 == clock2) {
        return true;
    }
    if (clock1 > clock2) {
        if (clock1 < clock2 + 60) {
            return true;
        }
    }
    if (clock1 < clock2) {
        if (clock2 < clock1 + 60) {
            return true;
        }
    }
    return false;
}

void dtv_adjust_i2s_output_clock(struct aml_audio_patch* patch, int direct, int step)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device * aml_dev = (struct aml_audio_device*)adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int output_clock = 0;
    unsigned int i2s_current_clock = 0;
    i2s_current_clock = aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL);
    if (i2s_current_clock > DEFAULT_I2S_OUTPUT_CLOCK * 4 ||
        i2s_current_clock == 0 || step <= 0 || step > DEFAULT_DTV_OUTPUT_CLOCK) {
        return;
    }
    if (get_tsync_pcr_debug())
        ALOGI("current:%d, default:%d\n", i2s_current_clock, patch->dtv_default_i2s_clock);
    if (direct == DIRECT_SPEED) {
        if (i2s_current_clock >= patch->dtv_default_i2s_clock) {
            if (i2s_current_clock - patch->dtv_default_i2s_clock >=
                DEFAULT_DTV_OUTPUT_CLOCK) {
                ALOGI("already > i2s_step_clk 1M,no need speed adjust\n");
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else {
            int value = patch->dtv_default_i2s_clock - i2s_current_clock;
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    } else if (direct == DIRECT_SLOW) {
        if (i2s_current_clock <= patch->dtv_default_i2s_clock) {
            if (patch->dtv_default_i2s_clock - i2s_current_clock >
                DEFAULT_DTV_OUTPUT_CLOCK) {
                ALOGI("alread < 1M no need adjust slow, return\n");
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else {
            int value = i2s_current_clock - patch->dtv_default_i2s_clock;
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    } else {
        if (compare_clock(i2s_current_clock, patch->dtv_default_i2s_clock)) {
            return ;
        }
        if (i2s_current_clock > patch->dtv_default_i2s_clock) {
            int value = i2s_current_clock - patch->dtv_default_i2s_clock;
            if (value < 60) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        } else if (i2s_current_clock < patch->dtv_default_i2s_clock) {
            int value = patch->dtv_default_i2s_clock - i2s_current_clock;
            if (value < 60) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value;
            aml_mixer_ctrl_set_int(handle, AML_MIXER_ID_CHANGE_I2S_PLL, output_clock);
        }
    }
    return;
}

void dtv_adjust_spdif_output_clock(struct aml_audio_patch* patch, int direct, int step, bool spdifb)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int output_clock, i;
    unsigned int spidif_current_clock = 0;
    eMixerCtrlID mixerID = spdifb ? AML_MIXER_ID_CHANGE_SPIDIFB_PLL : AML_MIXER_ID_CHANGE_SPIDIF_PLL;

    spidif_current_clock = aml_mixer_ctrl_get_int(handle, mixerID);
    if (spidif_current_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
        spidif_current_clock == 0 || step <= 0 || step > DEFAULT_DTV_OUTPUT_CLOCK) {
        return;
    }
    if (direct == DIRECT_SPEED) {
        if (compare_clock(spidif_current_clock, patch->dtv_default_spdif_clock)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 1 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));
        } else if (spidif_current_clock < patch->dtv_default_spdif_clock) {
            int value = patch->dtv_default_spdif_clock - spidif_current_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 2 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));

        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_SPEED clk %d,default %d",spidif_current_clock,patch->dtv_default_spdif_clock);
            return ;
        }
    } else if (direct == DIRECT_SLOW) {
        if (compare_clock(spidif_current_clock, patch->dtv_default_spdif_clock)) {
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 3 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));
        } else if (spidif_current_clock > patch->dtv_default_spdif_clock) {
            int value = spidif_current_clock - patch->dtv_default_spdif_clock;
            if (value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - step / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 4 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));
        } else {
            if (aml_audio_get_debug_flag())
                ALOGI("spdif_SLOW clk %d,default %d",spidif_current_clock,patch->dtv_default_spdif_clock);
            return ;
        }
    } else {
        if (compare_clock(spidif_current_clock, patch->dtv_default_spdif_clock)) {
            return ;
        }
        if (spidif_current_clock > patch->dtv_default_spdif_clock) {
            int value = spidif_current_clock - patch->dtv_default_spdif_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK - value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 5 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));
        } else if (spidif_current_clock < patch->dtv_default_spdif_clock) {
            int value = patch->dtv_default_spdif_clock - spidif_current_clock;
            if (value < 60 || value > DEFAULT_DTV_OUTPUT_CLOCK) {
                return;
            }
            output_clock = DEFAULT_DTV_OUTPUT_CLOCK + value / DEFAULT_SPDIF_ADJUST_TIMES;
            for (i = 0; i < DEFAULT_SPDIF_ADJUST_TIMES; i++) {
                aml_mixer_ctrl_set_int(handle, mixerID, output_clock);
            }
            if (aml_audio_get_debug_flag())
                ALOGI("spidif_clock 6 set %d to %d",spidif_current_clock,aml_mixer_ctrl_get_int(handle, AML_MIXER_ID_CHANGE_SPIDIF_PLL));
        } else {
            return ;
        }
    }
}

void dtv_adjust_output_clock(struct aml_audio_patch * patch, int direct, int step, bool dual)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    bool spdif_b = dual;
    if (aml_audio_get_debug_flag())
        ALOGI("dtv_adjust_output_clock not set,%x,%x",patch->decoder_offset,patch->dtv_pcm_readed);
    if (!aml_dev || step <= 0) {
        return;
    }
    if (patch->decoder_offset < 512 * 2 * 10 &&
        ((patch->aformat == AUDIO_FORMAT_AC3) ||
         (patch->aformat == AUDIO_FORMAT_E_AC3))) {
        return;
    }
    if (patch->dtv_default_spdif_clock > DEFAULT_I2S_OUTPUT_CLOCK * 4 ||
        patch->dtv_default_spdif_clock == 0) {
        return;
    }
    patch->pll_state = direct;
    if (direct == DIRECT_SPEED) {
        clock_gettime(CLOCK_MONOTONIC, &(patch->speed_time));
    } else if (direct == DIRECT_SLOW) {
        clock_gettime(CLOCK_MONOTONIC, &(patch->slow_time));
    }
    if (patch->spdif_format_set == 0) {
        if (patch->dtv_default_i2s_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
            patch->dtv_default_i2s_clock == 0) {
            return;
        }
        ALOGI("i2s_step_clk:%d, i2s_div_factor:%d.", patch->i2s_step_clk, patch->i2s_div_factor);
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
    } else if (!aml_dev->bHDMIARCon) {
        if (patch->dtv_default_i2s_clock > DEFAULT_SPDIF_PLL_DDP_CLOCK * 4 ||
            patch->dtv_default_i2s_clock == 0) {
            return;
        }
        ALOGI("i2s_step_clk:%d, spdif_step_clk:%d, i2s_div_factor:%d.", patch->i2s_step_clk, patch->spdif_step_clk, patch->i2s_div_factor);
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
        dtv_adjust_spdif_output_clock(patch, direct, patch->spdif_step_clk / patch->i2s_div_factor, dual);
    } else {
        dtv_adjust_spdif_output_clock(patch, direct, patch->spdif_step_clk / 4, dual);
    }
}

static void dtv_adjust_output_clock_continue(struct aml_audio_patch * patch, int direct)
{
    struct timespec current_time;
    int time_cost = 0;
    static int last_div = 0;
    int adjust_interval = 0;
    patch->i2s_div_factor = property_get_int32(PROPERTY_AUDIO_TUNING_CLOCK_FACTOR, DEFAULT_TUNING_CLOCK_FACTOR);
    adjust_interval = property_get_int32("vendor.media.audio_hal.adjtime", 1000);
    if (last_div != patch->i2s_div_factor) {
        ALOGI("new_div=%d, adjust_interval=%d ms,spdif_format_set=%d\n",
            patch->i2s_div_factor, adjust_interval, patch->spdif_format_set);
        last_div = patch->i2s_div_factor;
    }

    if (patch->pll_state == DIRECT_NORMAL || patch->pll_state != direct) {
        ALOGI("pll_state=%d, direct=%d no need continue\n", patch->pll_state, direct);
        return;
    }
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    if (direct == DIRECT_SPEED) {
        time_cost = calc_time_interval_us(&patch->speed_time, &current_time)/1000;
    } else if (direct == DIRECT_SLOW) {
        time_cost = calc_time_interval_us(&patch->slow_time, &current_time)/1000;
    }
    if (time_cost > adjust_interval && patch->spdif_format_set == 0) {
        ALOGI("over %d ms continue to adjust the clock\n", time_cost);
        dtv_adjust_output_clock(patch, direct, DEFAULT_DTV_ADJUST_CLOCK, false);
    }
    return;
}

static unsigned int dtv_calc_pcrpts_latency(struct aml_audio_patch *patch, unsigned int pcrpts)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    if (aml_dev->bHDMIARCon == 0 || aml_dev->hdmi_format == PCM) {
        return pcrpts;
    } else {
        return pcrpts + DEFAULT_ARC_DELAY_MS * 90;
    }
    if (aml_dev->bHDMIARCon && aml_dev->hdmi_format == PCM) {
        if (patch->aformat == AUDIO_FORMAT_E_AC3) {
            pcrpts += 2 * DTV_PTS_CORRECTION_THRESHOLD;
        } else {
            pcrpts + DTV_PTS_CORRECTION_THRESHOLD;
        }
    } else if (eDolbyMS12Lib == aml_dev->dolby_lib_type && aml_dev->bHDMIARCon) {
        if (patch->aformat == AUDIO_FORMAT_E_AC3 && !aml_dev->disable_pcm_mixing) {
            pcrpts += 8 * DTV_PTS_CORRECTION_THRESHOLD;
        } else if (patch->aformat == AUDIO_FORMAT_E_AC3 && aml_dev->disable_pcm_mixing) {
            pcrpts += 6 * DTV_PTS_CORRECTION_THRESHOLD;
        } else {
            pcrpts += 3 * DTV_PTS_CORRECTION_THRESHOLD;
        }
    } else if (eDolbyDcvLib == aml_dev->dolby_lib_type && aml_dev->bHDMIARCon) {
        if (patch->aformat == AUDIO_FORMAT_E_AC3) {
            pcrpts += 4 * DTV_PTS_CORRECTION_THRESHOLD;
        } else {
            pcrpts += DTV_PTS_CORRECTION_THRESHOLD;
        }
    } else {
        pcrpts += DTV_PTS_CORRECTION_THRESHOLD;
    }
    return pcrpts;
}

static void dtv_av_pts_info(struct aml_audio_patch *patch, unsigned int apts, unsigned int pcrpts)
{
    unsigned int cur_vpts = 0;
    unsigned int demux_vpts = 0;
    unsigned int demux_apts = 0;
    unsigned int demux_pcr = 0;
    unsigned int firstvpts;
    char buf[4096] = {0};
    int video_display_frame_count = 0;
    int video_receive_frame_count = 0;
    int64_t av_diff_ms = 0;
    int64_t ap_diff_ms = 0;
    struct timespec now_time;
    int time_cost_ms = 0;
    unsigned int mode = 0;

    struct timespec debug_current_time;
    int debug_time_costms = 0;
    int checkin_apts_interval = 0;
    int checkin_vpts_interval = 0;
    int demux_pcr_interval = 0;
    int out_apts_interval = 0;
    int out_vpts_interval = 0;

    get_sysfs_uint(TSYNC_VPTS, &cur_vpts);
    get_sysfs_uint(TSYNC_CHECKIN_APTS, &demux_apts);
    get_sysfs_uint(TSYNC_CHECKIN_VPTS, &demux_vpts);
    get_sysfs_uint(TSYNC_DEMUX_PCR, &demux_pcr);
    get_sysfs_uint(TSYNC_FIRST_VPTS, &firstvpts);
    get_sysfs_uint(TSYNC_PCR_MODE, &mode);
    video_display_frame_count = get_sysfs_int(VIDEO_DISPLAY_FRAME_CNT);
    video_receive_frame_count = get_sysfs_int(VIDEO_RECEIVE_FRAME_CNT);
    av_diff_ms = (apts - cur_vpts) / 90;
    ap_diff_ms = (pcrpts - apts) / 90;
    ALOGI("dtv_av_info: pa-pv-av-diff:[%d, %d, %d], apts:%x(%x,cache:%dms), vpts:%x(%x,cache:%dms), pcrpts:%x,"
          " checkin-pa-pv-av-diff:[%d, %d, %d], size:%d, latency:%d, mode:%d, firstvpts:%d, v_show_cnt:%d, v_rev_cnt:%d\n",\
           (int)(pcrpts - apts) / 90, (int)(pcrpts - cur_vpts) / 90, (int)(apts - cur_vpts)/90, apts, demux_apts,
           (int)(demux_apts - apts) / 90,cur_vpts, demux_vpts, (int)(demux_vpts - cur_vpts) / 90, pcrpts,
           (int)(pcrpts - demux_apts)/90, (int)(pcrpts - demux_vpts)/90, (int)(demux_apts - demux_vpts)/90,
           get_buffer_read_space(&(patch->aml_ringbuffer)), (int)decoder_get_latency() / 90, mode, firstvpts,
           video_display_frame_count, video_receive_frame_count);

    if (patch->debug_para.debug_last_checkin_apts == 0 && patch->debug_para.debug_last_out_apts == 0
        && patch->debug_para.debug_last_checkin_vpts == 0 && patch->debug_para.debug_last_out_vpts == 0
        && patch->debug_para.debug_last_demux_pcr == 0) {
          patch->debug_para.debug_last_checkin_apts = demux_apts;
          patch->debug_para.debug_last_out_apts = apts;
          patch->debug_para.debug_last_checkin_vpts = demux_vpts;
          patch->debug_para.debug_last_out_vpts = cur_vpts;
          patch->debug_para.debug_last_demux_pcr = demux_pcr;
    }

    clock_gettime(CLOCK_MONOTONIC, &debug_current_time);
    debug_time_costms = calc_time_interval_us(&patch->debug_para.debug_system_time, &debug_current_time) / 1000;
    if (debug_time_costms >= patch->debug_para.debug_time_interval) {
        checkin_apts_interval = (int)(demux_apts - patch->debug_para.debug_last_checkin_apts) / 90;
        checkin_vpts_interval = (int)(demux_vpts - patch->debug_para.debug_last_checkin_vpts) / 90;
        demux_pcr_interval = (int)(demux_pcr - patch->debug_para.debug_last_demux_pcr) / 90;
        out_apts_interval = (int)(apts - patch->debug_para.debug_last_out_apts) / 90;
        out_vpts_interval = (int)(cur_vpts - patch->debug_para.debug_last_out_vpts) / 90;
        ALOGI("audio_hal_debug system_time_interval: %d ms, demux_pcr_interval: %d ms,"
              " apts interval:[in:%d, out:%d], vpts interval:[in:%d, out:%d].",\
               debug_time_costms, demux_pcr_interval, checkin_apts_interval,
               out_apts_interval, checkin_vpts_interval, out_vpts_interval);
        patch->debug_para.debug_last_checkin_apts = demux_apts;
        patch->debug_para.debug_last_out_apts = apts;
        patch->debug_para.debug_last_checkin_vpts = demux_vpts;
        patch->debug_para.debug_last_out_vpts = cur_vpts;
        patch->debug_para.debug_last_demux_pcr = demux_pcr;
        clock_gettime(CLOCK_MONOTONIC, &patch->debug_para.debug_system_time);
    }

    if (av_diff_ms > 150 && ap_diff_ms > 0 && ap_diff_ms < 100 && patch->dtv_log_retry_cnt++ > 4) {
        patch->dtv_log_retry_cnt = 0;

        clock_gettime(CLOCK_MONOTONIC, &now_time);
        time_cost_ms = calc_time_interval_us(&patch->last_debug_record, &now_time) / 1000;
        clock_gettime(CLOCK_MONOTONIC, &patch->last_debug_record);
        // add calc to judge audio,video or pcr error.
        if (patch->last_apts_record != 0) {
            ALOGI("dtv_av_info, apts:0x%x, last_record=0x%x,apts diff:%lld ms, time_const=%d ms\n",
                apts, patch->last_apts_record, (int64_t)(apts - patch->last_apts_record) / 90, time_cost_ms);
        }
        if (patch->last_pcrpts_record != 0) {
            ALOGI("dtv_av_info, pcrpts:0x%x, last_record=0x%x,pcr diff:%lld ms, time_const=%d ms\n",
                pcrpts, patch->last_pcrpts_record, (int64_t)(pcrpts - patch->last_pcrpts_record) / 90, time_cost_ms);
        }
        if (patch->last_vpts_record != 0) {
            ALOGI("dtv_av_info, cur_vpts:0x%x, last_record=0x%x,vpts diff:%lld ms, time_const=%d ms\n",
                cur_vpts, patch->last_vpts_record, (int64_t)(cur_vpts - patch->last_vpts_record) / 90, time_cost_ms);
        }
        patch->last_apts_record = apts;
        patch->last_pcrpts_record = pcrpts;
        patch->last_vpts_record = cur_vpts;

        sysfs_get_sysfs_str("/sys/class/amstream/bufs", buf, sizeof(buf));
        ALOGI("dtv_av_info, amstream bufs=%s\n", buf);
        memset(buf, 0, sizeof(buf));
        sysfs_get_sysfs_str("/sys/class/ppmgr/ppmgr_vframe_states", buf, sizeof(buf));
        ALOGI("dtv_av_info, ppmgr states=%s\n", buf);
        memset(buf, 0, sizeof(buf));
        sysfs_get_sysfs_str("/sys/class/deinterlace/di0/provider_vframe_status", buf, sizeof(buf));
        ALOGI("dtv_av_info, di states=%s\n", buf);
        memset(buf, 0, sizeof(buf));
        sysfs_get_sysfs_str("/sys/class/video/vframe_states", buf, sizeof(buf));
        ALOGI("dtv_av_info, video states=%s\n", buf);
    }

}

static int dtv_calc_abuf_level(struct aml_audio_patch *patch, struct aml_stream_out *stream_out)
{
    if (!patch) {
        return 0;
    }
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    int main_avail = 0, min_buf_size;
    main_avail = get_buffer_read_space(ringbuffer);
    if ((patch->aformat == AUDIO_FORMAT_AC3) ||
        (patch->aformat == AUDIO_FORMAT_E_AC3)) {
        min_buf_size = stream_out->ddp_frame_size;
    } else if (patch->aformat == AUDIO_FORMAT_DTS) {
        min_buf_size = 1024;
    } else {
        min_buf_size = 32 * 48 * 4 * 2;
    }
    if (main_avail > min_buf_size) {
        return 1;
    }
    return 0;
}

void dtv_do_drop_pcm(int avail, struct aml_audio_patch *patch)
{
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    int drop_threshold = property_get_int32(PROPERTY_AUDIO_DROP_THRESHOLD, DEFAULT_AUDIO_DROP_THRESHOLD_MS);
    int least_cachetime = property_get_int32(PROPERTY_AUDIO_LEAST_CACHE, DEFAULT_AUDIO_LEAST_CACHE_MS);
    int least_size;
    int ap_diff_ms = patch->dtv_apts_lookup / 90;
    int drop_size = 48 * 4 * ap_diff_ms;
    int pts_latency = decoder_get_latency();
    int t1, t2;
    struct timespec before_write;
    struct timespec after_write;
    int wait_ms;
    least_size = least_cachetime * 48 * 4;
    ALOGI("AUDIO_DROP avail:%d,,dropsize:%d, pts_latency:%d.",  avail, drop_size, pts_latency);

    if (ap_diff_ms > drop_threshold) {
        int real_drop_size = (avail >= drop_size + least_size)?drop_size:avail - least_size;
        if (real_drop_size < 0)
            real_drop_size = 0;
        ALOGI("Drop data size: %d, avail: %d, need drop size: %d\n", real_drop_size, avail, drop_size);
        t1 = real_drop_size / patch->out_buf_size;
        for (t2 = 0; t2 < t1; t2++) {
            ring_buffer_read(&(patch->aml_ringbuffer), (unsigned char *)patch->out_buf, patch->out_buf_size);
        }
        patch->dtv_apts_lookup = 0;
    }
    avail = get_buffer_read_space(ringbuffer);

    clock_gettime(CLOCK_MONOTONIC, &before_write);
    while (avail < least_size) {
        usleep(5 * 1000);
        avail = get_buffer_read_space(ringbuffer);
        clock_gettime(CLOCK_MONOTONIC, &after_write);
        wait_ms = calc_time_interval_us(&before_write, &after_write)/1000;
        if (wait_ms > 1000) {
            ALOGI("Warning waite_ms over 1s, break\n");
            break;
        }
    }

    if (patch->dtv_apts_lookup == 0) {
        patch->last_apts = 0;
        patch->last_pcrpts = 0;
    }
    patch->dtv_audio_tune = AUDIO_LATENCY;
    ALOGI("[%s,%d] dtv_audio_tune AUDIO_DROP-> AUDIO_LATENCY\n", __FUNCTION__, __LINE__);
}

void dtv_do_insert_zero_pcm(struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out)
{
    int t1, t2;
    int insert_size = 0;
    memset(patch->out_buf, 0, patch->out_buf_size);
    if (abs(patch->dtv_apts_lookup) / 90 > 1000) {
        t1 = 1000 * 192;
    } else {
        t1 =  192 * abs(patch->dtv_apts_lookup) / 90;
    }
    t2 = t1 / patch->out_buf_size;
    t1 = t1 & ~3;
    insert_size = t1;
    ALOGI("insert_zero_pcm: ++drop %d,lookup %d,diff %d ms,t2=%d,patch->out_buf_size=%zu\n",
         t1, patch->dtv_apts_lookup, t1 / 192, t2, patch->out_buf_size);
    /*[SE][BUG][SWPL-21122][chengshun] when insert 0, need check write len,
         * and avoid dtv patch write together*/
    unsigned int cur_pcr = 0;
    struct timespec before_write;
    struct timespec after_write;
    clock_gettime(CLOCK_MONOTONIC, &before_write);
    patch->pcm_inserting = true;
    do {
        unsigned int cur_pts = patch->last_apts;
        get_sysfs_uint(TSYNC_PCRSCR, (unsigned int *) & (cur_pcr));
        int ap_diff = cur_pts - cur_pcr;
        ALOGI("insert_zero_pcm: cur_pts=0x%x, cur_pcr=0x%x,ap_diff=%d\n", cur_pts, cur_pcr, ap_diff);
        if (ap_diff < 90*10) {
            ALOGI("insert_zero_pcm: write mute pcm enough,break\n");
            patch->dtv_apts_lookup = 0;
            break;
        }
        memset(patch->out_buf, 0, patch->out_buf_size);
        int ret = ring_buffer_write(&(patch->aml_ringbuffer), (unsigned char *)patch->out_buf, patch->out_buf_size, 0);
        t1 -= ret;
        int buff_len = ring_buffer_read(&(patch->aml_ringbuffer), (unsigned char *)patch->out_buf, patch->out_buf_size);
        int write_len = out_write_new(stream_out, patch->out_buf, buff_len);
        patch->dtv_pcm_readed += write_len;
        clock_gettime(CLOCK_MONOTONIC, &after_write);
        int write_used_ms = calc_time_interval_us(&before_write, &after_write)/1000;
        ALOGI("insert_zero_pcm: write_used_ms = %d\n", write_used_ms);
        if (write_used_ms > 1000) {
            ALOGI("Warning write cost over 1s, break\n");
            break;
        }
        ALOGI("insert_zero_pcm: ++drop t1=%d, ret = %d", t1, ret);
    } while (t1 > 0);
    patch->pcm_inserting = false;
    patch->dtv_apts_lookup += ((insert_size - t1) * 90) / 192;
    ALOGI("after insert size:%d, dtv_apts_lookup=%d\n",
            insert_size - t1, patch->dtv_apts_lookup);
    if (-DTV_PTS_CORRECTION_THRESHOLD <= patch->dtv_apts_lookup) {
        ALOGI("only need insert 30ms, break\n");
        patch->dtv_apts_lookup = 0;
    }
}

void dtv_do_process_pcm(int avail, struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out)
{
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_stream_out *out = (struct aml_stream_out *)stream_out;
    if (!patch || !patch->dev || !stream_out) {
        return;
    }
    if (patch->dtv_apts_lookup > 0) {
        dtv_do_drop_pcm(avail, patch);
    } else if (patch->dtv_apts_lookup < 0) {
        dtv_do_insert_zero_pcm(patch, stream_out);

        if (patch->dtv_apts_lookup == 0) {
            patch->last_apts = 0;
            patch->last_pcrpts = 0;
            ALOGI("[%s,%d] dtv_audio_tune AUDIO_DROP-> AUDIO_LATENCY\n",
                  __FUNCTION__, __LINE__);
            patch->dtv_audio_tune = AUDIO_LATENCY;
        } else {
            patch->dtv_audio_tune = AUDIO_LOOKUP;
        }
    }
}

static int dtv_audio_tune_check(struct aml_audio_patch *patch, int cur_pts_diff, int last_pts_diff, unsigned int apts)
{
    char tempbuf[128] = {0};
    int origin_pts_diff = 0;
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    if (!patch || !patch->dev || aml_dev->dev2mix_patch == 1) {
        patch->dtv_audio_tune = AUDIO_RUNNING;
        return 1;
    }
    if (get_audio_discontinue() || patch->dtv_has_video == 0 ||
        patch->dtv_audio_mode == 1) {
        dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        patch->last_apts = 0;
        patch->last_pcrpts = 0;
        return 1;
    }

    if (abs(cur_pts_diff) >= patch->a_discontinue_threshold) {
        sprintf(tempbuf, "AUDIO_TSTAMP_DISCONTINUITY:0x%lx",
                (unsigned long)apts);
        dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        if (sysfs_set_sysfs_str(TSYNC_EVENT, tempbuf) == -1) {
            ALOGI("unable to open file %s,err: %s", TSYNC_EVENT, strerror(errno));
        }
        return 1;
    }
    if (patch->dtv_audio_tune != AUDIO_RUNNING) {
        if (dtv_avsync_audio_freerun(patch)) {
            patch->dtv_audio_tune = AUDIO_RUNNING;
            return 1;
        }
    }
    if (patch->dtv_audio_tune == AUDIO_LOOKUP) {
        if (abs(last_pts_diff - cur_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD) {
            patch->dtv_apts_lookup = (last_pts_diff + cur_pts_diff) / 2;
            patch->dtv_audio_tune = AUDIO_DROP;
            ALOGI("dtv_audio_tune audio_lookup %d", patch->dtv_apts_lookup);
        }
        return 1;
    } else if (patch->dtv_audio_tune == AUDIO_LATENCY) {
        if (abs(last_pts_diff - cur_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD) {
            int pts_diff = (last_pts_diff + cur_pts_diff) / 2;
            int pts_latency = decoder_get_latency();

            if (get_dtv_pcr_sync_mode() == 1) {
                if (pts_diff > 60 * 90) {
                    pts_diff = 60 * 90;
                } else if (pts_diff < -60 * 90) {
                    pts_diff = -60 * 90;
                }
                ALOGI("dtv_audio_tune audio_latency %d,pts_diff %d", (int)pts_latency / 90, pts_diff / 90);
                pts_latency += pts_diff;
                if (pts_latency >= AUDIO_PCR_LATENCY_MAX * 90) {
                    pts_latency = AUDIO_PCR_LATENCY_MAX * 90;
                } else if (pts_latency < 0) {
                    if (abs(pts_diff) < DTV_PTS_CORRECTION_THRESHOLD) {
                        pts_latency += abs(pts_diff);
                    } else {
                        pts_latency = 0;
                    }
                }
                decoder_set_latency(pts_latency);
            } else {
                uint pcrpts = 0;
                origin_pts_diff = pts_diff;
                get_sysfs_uint(TSYNC_PCRSCR, &pcrpts);
                ALOGI("dtv_audio_tune audio_latency pts_diff %d, pcrsrc %x", pts_diff / 90, pcrpts);
                if (pts_diff > patch->sync_para.pcr_adjust_max) {
                    pts_diff = patch->sync_para.pcr_adjust_max;
                } else if (pts_diff < -patch->sync_para.pcr_adjust_max) {
                    pts_diff = -patch->sync_para.pcr_adjust_max;
                }
                pcrpts -= pts_diff;
                decoder_set_pcrsrc(pcrpts);
                ALOGI("dtv_audio_tune audio_latency end, pcrsrc %x, diff:%d, origin:%d",
                        pcrpts, pts_diff, origin_pts_diff);
            }
            ALOGI("dtv_audio_tune AUDIO_LATENCY -> AUDIO_RUNNING,cur_diff:%d\n",
                    patch->sync_para.cur_pts_diff);
            patch->dtv_audio_tune = AUDIO_RUNNING;
            clean_dtv_patch_pts(patch);
        }
        return 1;
    } else if (patch->dtv_audio_tune != AUDIO_RUNNING) {
        return 1;
    }

    return 0;
}

static void do_pll1_by_pts(unsigned int pcrpts, struct aml_audio_patch *patch,
                           unsigned int apts, struct aml_stream_out *stream_out)
{
    unsigned int last_pcrpts, last_apts;
    int pcrpts_diff, last_pts_diff, cur_pts_diff;
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);

    if (get_tsync_pcr_debug()) {
        dtv_av_pts_info(patch, apts, pcrpts);
    }

    last_apts = patch->last_apts;
    last_pcrpts = patch->last_pcrpts;
    patch->last_pcrpts = pcrpts;
    patch->last_apts = apts;
    last_pts_diff = last_pcrpts - last_apts;
    cur_pts_diff = pcrpts - apts;
    patch->sync_para.cur_pts_diff = cur_pts_diff;
    pcrpts_diff = pcrpts - last_pcrpts;
    if (last_apts == 0 && last_pcrpts == 0) {
        return;
    }
    if (dtv_audio_tune_check(patch, cur_pts_diff, last_pts_diff, apts)) {
        return;
    }
    if (patch->pll_state == DIRECT_NORMAL) {
        if (abs(cur_pts_diff) <= DTV_PTS_CORRECTION_THRESHOLD * 3 ||
            abs(last_pts_diff + cur_pts_diff) / 2 <= DTV_PTS_CORRECTION_THRESHOLD * 3
            || abs(last_pts_diff) <= DTV_PTS_CORRECTION_THRESHOLD * 3) {
            return;
        } else {
            if (pcrpts > apts) {
                if (dtv_calc_abuf_level(patch, stream_out)) {
                    dtv_adjust_output_clock(patch, DIRECT_SPEED, DEFAULT_DTV_ADJUST_CLOCK, false);
                } else {
                    dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                }
            } else {
                dtv_adjust_output_clock(patch, DIRECT_SLOW, DEFAULT_DTV_ADJUST_CLOCK, false);
            }
            return;
        }
    } else if (patch->pll_state == DIRECT_SPEED) {
        if (!dtv_calc_abuf_level(patch, stream_out)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
        if (cur_pts_diff < 0 && ((last_pts_diff + cur_pts_diff) < 0 ||
                                 abs(last_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
	if (cur_pts_diff > 0)
            dtv_adjust_output_clock_continue(patch, DIRECT_SPEED);
    } else if (patch->pll_state == DIRECT_SLOW) {
        if (cur_pts_diff > 0 && ((last_pts_diff + cur_pts_diff) > 0 || abs(last_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
	if (cur_pts_diff < 0)
            dtv_adjust_output_clock_continue(patch, DIRECT_SLOW);
    }
}

static void do_pll2_by_pts(unsigned int pcrpts, struct aml_audio_patch *patch,
                           unsigned int apts, struct aml_stream_out *stream_out)
{
    unsigned int last_pcrpts, last_apts;
    unsigned int cur_vpts = 0;
    int pcrpts_diff, last_pts_diff, cur_pts_diff;
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) adev;
    struct aml_mixer_handle * handle = &(aml_dev->alsa_mixer);
    int ret = 0;

    if (get_tsync_pcr_debug()) {
        dtv_av_pts_info(patch, apts, pcrpts);
    }

    last_apts = patch->last_apts;
    last_pcrpts = patch->last_pcrpts;
    patch->last_pcrpts = pcrpts;
    patch->last_apts = apts;
    last_pts_diff = last_pcrpts - last_apts;
    cur_pts_diff = pcrpts - apts;
    patch->sync_para.cur_pts_diff = cur_pts_diff;
    pcrpts_diff = pcrpts - last_pcrpts;
    if (last_apts == 0 && last_pcrpts == 0) {
        return;
    }
    if (dtv_audio_tune_check(patch, cur_pts_diff, last_pts_diff, apts)) {
        return;
    }
    if (aml_dev->bHDMIARCon) {
        int arc_delay = aml_getprop_int(PROPERTY_LOCAL_ARC_LATENCY) / 1000;
        if (get_video_delay() != -arc_delay) {
            ALOGI("arc:video_delay moved from %d ms to %d ms", get_video_delay(), -arc_delay);
            set_video_delay(-arc_delay);
            return;
        }
    }
    if (patch->pll_state == DIRECT_NORMAL) {
        if (abs(cur_pts_diff) <= DTV_PTS_CORRECTION_THRESHOLD ||
            abs(last_pts_diff + cur_pts_diff) / 2 <= DTV_PTS_CORRECTION_THRESHOLD
            || abs(last_pts_diff) <= DTV_PTS_CORRECTION_THRESHOLD) {
            return;
        } else {
            if (pcrpts > apts) {
                if (dtv_calc_abuf_level(patch, stream_out)) {
                    dtv_adjust_output_clock(patch, DIRECT_SPEED, DEFAULT_DTV_ADJUST_CLOCK, false);
                } else {
                    dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                }
            } else {
                dtv_adjust_output_clock(patch, DIRECT_SLOW, DEFAULT_DTV_ADJUST_CLOCK, false);
            }
            return;
        }
    } else if (patch->pll_state == DIRECT_SPEED) {
        if (!dtv_calc_abuf_level(patch, stream_out)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
        if (cur_pts_diff < 0 && ((last_pts_diff + cur_pts_diff) < 0 ||
                                 abs(last_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
	if (cur_pts_diff > 0)
            dtv_adjust_output_clock_continue(patch, DIRECT_SPEED);
    } else if (patch->pll_state == DIRECT_SLOW) {
        if (cur_pts_diff > 0 && ((last_pts_diff + cur_pts_diff) > 0 || abs(last_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD)) {
            dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
        }
	if (cur_pts_diff < 0)
            dtv_adjust_output_clock_continue(patch, DIRECT_SLOW);
    }
}

void process_ac3_sync(struct aml_audio_patch *patch, unsigned long pts, struct aml_stream_out *stream_out)
{

    int channel_count = 2;
    int bytewidth = 2;
    int symbol = 48;
    char tempbuf[128] = {0};
    unsigned int pcrpts;
    unsigned int pts_diff,last_checkin_apts;
    unsigned long cur_out_pts;
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device * aml_dev = (struct aml_audio_device*)adev;

    get_sysfs_uint(TSYNC_PCRSCR, &pcrpts);
    if (patch->dtv_first_apts_flag == 0) {
        get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkin_apts);
        sprintf(tempbuf, "AUDIO_START:0x%x", (unsigned int)pts);
        ALOGI("[audiohal_kpi] dtv set tsync -> %s, cache audio:%dms,pcr:0x%x",
                tempbuf, (int)(last_checkin_apts - pts)/90, pcrpts);
        if (sysfs_set_sysfs_str(TSYNC_EVENT, tempbuf) == -1) {
            ALOGE("set AUDIO_START failed \n");
        }
        patch->dtv_first_apts_flag = 1;

        if (patch->dtv_has_video) {
            aml_dev->start_mute_flag = 1;
            aml_dev->start_mute_count = 0;
            ALOGI("set start_mute_flag 1.");
        }
        clock_gettime(CLOCK_MONOTONIC, &patch->debug_para.debug_system_time);
    } else {
        cur_out_pts = pts;
        if (!patch || !patch->dev || !stream_out) {
            return;
        }
        if (pts == 0) {
            return;
        }
        pcrpts = dtv_calc_pcrpts_latency(patch, pcrpts);
        do_pll2_by_pts(pcrpts, patch, cur_out_pts, stream_out);
    }
}

void process_pts_sync(unsigned int pcm_lancty, struct aml_audio_patch *patch,
                      unsigned int rbuf_level, struct aml_stream_out *stream_out)
{
    int channel_count = 2;
    int bytewidth = 2;
    int sysmbol = 48;
    char tempbuf[128] = {0};
    unsigned int pcrpts, apts, last_checkin_apts;
    unsigned int calc_len = 0;
    unsigned long pts = 0, lookpts;
    unsigned long cache_pts = 0;
    unsigned long cur_out_pts = 0;
    unsigned int checkin_firstapts = 0;
    struct audio_hw_device *adev = patch->dev;
    struct aml_audio_device * aml_dev = (struct aml_audio_device*)adev;


    get_sysfs_uint(DTV_DECODER_CHECKIN_FIRSTAPTS_PATH, &checkin_firstapts);
    pts = lookpts = dtv_patch_get_pts();
    if (pts == patch->last_valid_pts) {
        ALOGI("dtv_patch_get_pts pts  -> %lx", pts);
    }
    if (patch->dtv_first_apts_flag == 0) {
        if (pts == 0) {
            pts = checkin_firstapts;
            ALOGI("pts = 0,so get checkin_firstapts:0x%lx", pts);
        }

        get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkin_apts);
        sprintf(tempbuf, "AUDIO_START:0x%x", (unsigned int)pts);
        ALOGI("[audiohal_kpi]dtv set tsync -> %s, audio cache:%dms",
                tempbuf, (int)(last_checkin_apts - pts)/90);
        if (sysfs_set_sysfs_str(TSYNC_EVENT, tempbuf) == -1) {
            ALOGE("set AUDIO_START failed \n");
        }
        if (patch->dtv_has_video) {
            aml_dev->start_mute_flag = 1;
            aml_dev->start_mute_count = 0;
            ALOGI("set start_mute_flag 1.");
        }

        patch->dtv_pcm_readed = 0;
        patch->dtv_first_apts_flag = 1;
        patch->last_valid_pts = pts;
        patch->cur_outapts = pts;
        clock_gettime(CLOCK_MONOTONIC, &patch->debug_para.debug_system_time);

    } else {
        unsigned int pts_diff;
        if (pts != (unsigned long) - 1) {
            calc_len = (unsigned int)rbuf_level;
            cache_pts = (calc_len * 90) / (sysmbol * channel_count * bytewidth);
            if (pts > cache_pts) {
                cur_out_pts = pts - cache_pts;
            } else {
                return;
            }
            if (cur_out_pts > pcm_lancty * 90) {
                cur_out_pts = cur_out_pts - pcm_lancty * 90;
            } else {
                return;
            }
            patch->last_valid_pts = cur_out_pts;
            patch->dtv_pcm_readed = 0;
        } else {
            pts = patch->last_valid_pts;
            calc_len = patch->dtv_pcm_readed;
            cache_pts = (calc_len * 90) / (sysmbol * channel_count * bytewidth);
            cur_out_pts = pts + cache_pts;
            patch->cur_outapts = cur_out_pts;
            return;
        }
        if (!patch || !patch->dev || !stream_out) {
            return;
        }
        patch->cur_outapts = cur_out_pts;
        get_sysfs_uint(TSYNC_PCRSCR, &pcrpts);
        //pcrpts -= DTV_PTS_CORRECTION_THRESHOLD;
        do_pll1_by_pts(pcrpts, patch, cur_out_pts, stream_out);
    }
}

bool dtv_avsync_audio_freerun(struct aml_audio_patch* patch)
{
    unsigned int demux_apts = 0, cur_pcr = 0;

    get_sysfs_uint(TSYNC_CHECKIN_APTS, &demux_apts);
    if (patch->last_pcrpts) {
        cur_pcr = patch->last_pcrpts;
    } else {
        get_sysfs_uint(TSYNC_PCRSCR, &cur_pcr);
    }
    if (demux_apts && cur_pcr && (int)demux_apts != -1 && (int)cur_pcr != -1) {
        if (demux_apts < cur_pcr - DTV_PTS_CORRECTION_THRESHOLD * 5 ||
            demux_apts > cur_pcr + patch->a_discontinue_threshold) {
            return true;
        }
    }
    if (patch->dtv_audio_mode == 1) {
        return true;
    }
    return false;
}

/* +[SE] [BUG][SWPL-21070][yinli.xia] startplay strategy choose*/
void dtv_avsync_get_ptsinfo(struct aml_audio_patch* patch)
{
    unsigned int cur_vpts, cur_pcr;
    unsigned int firstvpts, checkin_firstapts;
    get_sysfs_uint(TSYNC_VPTS, &cur_vpts);
    get_sysfs_uint(TSYNC_FIRST_VPTS, &firstvpts);
    get_sysfs_uint(TSYNC_PCRSCR, &cur_pcr);
    get_sysfs_uint(DTV_DECODER_CHECKIN_FIRSTAPTS_PATH, &checkin_firstapts);

    patch->startplay_vpts = cur_vpts;
    patch->startplay_pcrpts = cur_pcr;
    patch->startplay_apts_lookup = patch->last_valid_pts;
    patch->startplay_firstvpts = firstvpts;
    patch->startplay_first_checkinapts = checkin_firstapts;
}

/* +[SE] [BUG][SWPL-21070][yinli.xia] startplay strategy choose*/
void dtv_out_avpts_equal(struct aml_audio_patch* patch)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    if (patch->startplay_pcrpts >= patch->startplay_firstvpts) {
            aml_dev->start_mute_flag = 0;
            patch->startplay_avsync_flag = 0;
            ALOGI("%s avsync startplay strategy mode 0 --\n", __FUNCTION__);
    }
}

/* +[SE] [BUG][SWPL-21070][yinli.xia] startplay strategy choose*/
void dtv_out_apts_biggerthan_vpts(struct aml_audio_patch* patch)
{
    unsigned int demux_pcr = 0;
    get_sysfs_uint(TSYNC_DEMUX_PCR, &demux_pcr);

    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    int strategy_mode = property_get_int32("vendor.media.audio.strategy.aptsbigger", 1);

    if (get_tsync_pcr_debug()) {
        ALOGI("avsync startplay pcrpts = 0x%lx, demux_pcr = 0x%x --\n",
                        patch->startplay_pcrpts, demux_pcr);
    }

    if (patch->startplay_avsync_flag) {
        if (strategy_mode == STRATEGY_A_ZERO_V_NOSHOW) {
            if (patch->startplay_pcrpts >= patch->startplay_firstvpts) {
                //when pcr bigger than vpts,output normal
            }
        } else if (strategy_mode == STRATEGY_A_NORMAL_V_SHOW_QUICK) {
                sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
        }
        if (patch->startplay_pcrpts >= patch->startplay_first_checkinapts) {
            aml_dev->start_mute_flag = 0;
            patch->startplay_avsync_flag = 0;
            ALOGI("%s avsync startplay strategy mode = %d --\n", __FUNCTION__, strategy_mode);
        }
    }
}

/* +[SE] [BUG][SWPL-21070][yinli.xia] startplay strategy choose*/
void dtv_out_vpts_biggerthan_apts(struct aml_audio_patch* patch)
{
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    int strategy_mode = property_get_int32("vendor.media.audio.strategy.vptsbigger", 4);

    if (patch->startplay_avsync_flag) {
        if ((strategy_mode >= STRATEGY_A_NORMAL_V_SHOW_BLOCK) &&
            (strategy_mode <= STRATEGY_A_MUTE_V_SHOW_BLOCK)) {
            if (strategy_mode == STRATEGY_A_NORMAL_V_SHOW_BLOCK) {
                sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
                aml_dev->start_mute_flag = 0;
            } else if (strategy_mode == STRATEGY_A_NORMAL_V_NOSHOW) {
                //if (patch->startplay_pcrpts >= patch->startplay_first_checkinapts) {
                    //aml_dev->start_mute_flag = 1;
                //}
            } else if (strategy_mode == STRATEGY_A_MUTE_V_SHOW_BLOCK) {
                if (patch->startplay_pcrpts >= patch->startplay_first_checkinapts) {
                    aml_dev->start_mute_flag = 1;
                    sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
                }
            }
            if (patch->startplay_pcrpts >= patch->startplay_firstvpts) {
                aml_dev->start_mute_flag = 0;
                patch->startplay_avsync_flag = 0;
                ALOGI("%s avsync startplay strategy mode = %d --\n", __FUNCTION__, strategy_mode);
            }
        }
        if ((strategy_mode == STRATEGY_A_DROP_V_SHOW_BLOCK) ||
            (strategy_mode == STRATEGY_A_DROP_V_NOSHOW)) {
            if (strategy_mode == STRATEGY_A_DROP_V_SHOW_BLOCK)
                sysfs_set_sysfs_str(VIDEO_SHOW_FIRST_FRAME, "1");
            if (patch->startplay_apts_lookup >= patch->startplay_firstvpts) {
                decoder_set_latency(DEMUX_PCR_APTS_LATENCY);
                aml_dev->start_mute_flag = 0;
                patch->startplay_avsync_flag = 0;
                ALOGI("%s avsync startplay strategy mode = %d --\n", __FUNCTION__, strategy_mode);
            }
        }
    }
}

/* +[SE] [BUG][SWPL-21070][yinli.xia] startplay strategy choose*/
void dtv_avsync_startplay_strategy(struct aml_audio_patch *patch)
{
    int strategy_mode = 0;
    unsigned long avdiff;
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    int avail = get_buffer_read_space(ringbuffer);

    if ((dtv_get_tsync_mode() == 2) && (patch->startplay_avsync_flag)) {
        ALOGI(" start play under pcrmaster \n");
    } else {
        return;
    }

    int default_time= DEFAULT_SYSTEM_TIME;
    int avdiff_time_adjust = property_get_int32("vendor.media.audio.avdifftime", 3);
    int startplay_strategy_aptsbigger = property_get_int32("vendor.media.audio.strategy.aptsbigger", 1);
    int startplay_strategy_vptsbigger = property_get_int32("vendor.media.audio.strategy.vptsbigger", 3);

    dtv_avsync_get_ptsinfo(patch);
    if (patch->startplay_firstvpts >patch->startplay_first_checkinapts)
        avdiff = patch->startplay_firstvpts - patch->startplay_first_checkinapts;
    else
        avdiff = patch->startplay_first_checkinapts -patch->startplay_firstvpts;
    if (get_tsync_pcr_debug()) {
        ALOGI("avsync startplay firstvpts = 0x%lx,first_checkinapts = 0x%lx, avdiff = 0x%lx --\n",
                        patch->startplay_firstvpts, patch->startplay_first_checkinapts, avdiff);
        ALOGI("avsync startplay pcrpts = 0x%lx, apts = 0x%lx, vpts = 0x%x --\n",
                        patch->startplay_pcrpts, patch->startplay_apts_lookup, patch->startplay_vpts);
    }
    /*------------------------------------------------------------
    this function is designed for av alternately distributed situation
    startplay have below situation
    -------------------------------------------------------------*/
    if (avdiff < avdiff_time_adjust * default_time) {  //Evenly distributed data
        if (patch->startplay_first_checkinapts == patch->startplay_firstvpts) {  //strategy 0
            strategy_mode = 0;
        } else if (patch->startplay_first_checkinapts > patch->startplay_firstvpts) {  //strategy 1 2
            strategy_mode = startplay_strategy_aptsbigger;
        } else if (patch->startplay_first_checkinapts < patch->startplay_firstvpts) {  //strategy 3 4 5 6 7
            strategy_mode = startplay_strategy_vptsbigger;
        }
        //ALOGI("avsync startplay strategy mode = %d --\n", strategy_mode);
    }
    #if 0
    /*------------------------------------------------------------
    this function is designed for only video situation
    -------------------------------------------------------------*/
    else if () {  //only video

    }
    /*------------------------------------------------------------
    this function is designed for only audio situation
    -------------------------------------------------------------*/
    else if () {  //only audio

    }
     /*------------------------------------------------------------
    this function is designed for av not alternately distributed situation
    -------------------------------------------------------------*/
    else {  //Uneven data distribution

    }
    #endif
    switch (strategy_mode) {
    case STRATEGY_AVOUT_NORMAL:
        dtv_out_avpts_equal(patch);
        break;
    case STRATEGY_A_ZERO_V_NOSHOW:
    case STRATEGY_A_NORMAL_V_SHOW_QUICK:
        dtv_out_apts_biggerthan_vpts(patch);
        break;
    case STRATEGY_A_NORMAL_V_SHOW_BLOCK:
    case STRATEGY_A_NORMAL_V_NOSHOW:
    case STRATEGY_A_MUTE_V_SHOW_BLOCK:
    case STRATEGY_A_DROP_V_SHOW_BLOCK:
    case STRATEGY_A_DROP_V_NOSHOW:
        dtv_out_vpts_biggerthan_apts(patch);
        break;
    default:
        ALOGI(" startplay strategy do not contained\n");
        break;
    }
}

void dtv_avsync_process(struct aml_audio_patch* patch, struct aml_stream_out* stream_out)
{
    unsigned long pts;
    uint firstvpts, pcrpts;
    int ret = 0;
    char tempbuf[128] = {0};
    int audio_output_delay = 0;
    int cache_time = 0;
    unsigned int last_checkin_apts;
    int pcm_lantcy;
    ring_buffer_t *ringbuffer = &(patch->aml_ringbuffer);
    struct audio_hw_device *dev = patch->dev;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    get_sysfs_uint(TSYNC_PCRSCR, &pcrpts);
    get_sysfs_uint(TSYNC_FIRST_VPTS, &firstvpts);

    int switch_flag = property_get_int32("vendor.media.audio.strategy.switch", 0);

    if (patch->dtv_decoder_state != AUDIO_DTV_PATCH_DECODER_STATE_RUNING) {
        return;
    }

    if (patch->dtv_has_video && patch->show_first_frame == 0) {
        patch->show_first_frame = get_sysfs_int(VIDEO_FIRST_FRAME_SHOW) || get_sysfs_int(VIDEO_FIRST_FRAME_SHOW_2);
        ALOGI("dtv_avsync_process: patch->show_first_frame=%d, firstvpts=0x%x, pcrpts=0x%x, cache:%dms",
            patch->show_first_frame, firstvpts, pcrpts, (int)(firstvpts - pcrpts)/90);
    }

    aml_dev->audio_discontinue = get_audio_discontinue();

    audio_output_delay = aml_getprop_int(PROPERTY_LOCAL_PASSTHROUGH_LATENCY);

    if (patch->last_audio_delay != audio_output_delay) {
        patch->last_audio_delay = audio_output_delay;
        patch->dtv_audio_tune = AUDIO_LOOKUP;
        ALOGI("set audio_output_delay = %d\n", audio_output_delay);
    }
    if (patch->aformat == AUDIO_FORMAT_E_AC3 || patch->aformat == AUDIO_FORMAT_AC3 ||
            patch->aformat == AUDIO_FORMAT_AC4) {
        if (stream_out != NULL) {
        /*+[SE][BUG][SWPL-26557][zhizhong] for the passthrough set prelantency default*/
            if (aml_dev->sink_format == AUDIO_FORMAT_E_AC3 ||
                aml_dev->sink_format == AUDIO_FORMAT_AC3)
                audio_output_delay += patch->pre_latency;
            else if (eDolbyMS12Lib == aml_dev->dolby_lib_type && audio_output_delay == 0)
                //for tunning dolby dvb avsync -20 ~30ms
                audio_output_delay = -20;
            pcm_lantcy = out_get_latency(&(stream_out->stream)) + audio_output_delay;
            if (pcm_lantcy < 0)
                pcm_lantcy = 0;
            pts = dtv_hal_get_pts(patch, pcm_lantcy);
            process_ac3_sync(patch, pts, stream_out);
        }
    } else if (patch->aformat ==  AUDIO_FORMAT_DTS || patch->aformat == AUDIO_FORMAT_DTS_HD) {
        if (stream_out != NULL) {
            ringbuffer = &(patch->aml_ringbuffer);
            pcm_lantcy = out_get_latency(&(stream_out->stream)) + audio_output_delay;
            if (pcm_lantcy < 0)
                pcm_lantcy = 0;
            pts = dtv_hal_get_pts(patch, pcm_lantcy);
            process_ac3_sync(patch, pts, stream_out);
        }
    } else {
        if (stream_out != NULL) {
            pcm_lantcy = out_get_latency(&(stream_out->stream)) + audio_output_delay;
            if (pcm_lantcy < 0)
                pcm_lantcy = 0;
            int abuf_level = get_buffer_read_space(ringbuffer);
            process_pts_sync(pcm_lantcy, patch, abuf_level, stream_out);
        }
    }
    /*[SE][BUG][SWPL-21070][yinli.xia] start play strategy choose*/
    if ((firstvpts > 0) && switch_flag) {
        dtv_avsync_startplay_strategy(patch);
    }
    /*[SE][BUG][SWPL-15728][chengshun] if avsync output, when pcr near firstvpts, output sound*/
    if (aml_dev->start_mute_flag && ((firstvpts != 0 && pcrpts + 100*90 > firstvpts) || patch->show_first_frame)) {
        if (patch->dtv_audio_tune ==  AUDIO_RUNNING &&
            abs(patch->sync_para.cur_pts_diff) < DTV_PTS_CORRECTION_THRESHOLD * 5) {
            ALOGI("clear start_mute_flag 0,cur_diff=%d\n",
                patch->sync_para.cur_pts_diff);
            aml_dev->start_mute_flag = 0;
        } else if (aml_dev->start_mute_count++ > aml_dev->start_mute_max) {
            ALOGI("timeout force clear start_mute_flag 0\n");
            aml_dev->start_mute_flag = 0;
        } else if (getprop_bool("vendor.media.audio.syncshow")) {
            ALOGI("need sync show, clear start_mute_flag\n");
            aml_dev->start_mute_flag = 0;
        } else if (dtv_avsync_audio_freerun(patch)) {
            ALOGI("audio is freerun, clear start_mute_flag\n");
            aml_dev->start_mute_flag = 0;
        }
    }
    sprintf(tempbuf, "%u", patch->cur_outapts);
    if (patch->tsync_pcr_debug == 7) {
        get_sysfs_uint(TSYNC_LAST_CHECKIN_APTS, &last_checkin_apts);
        if (last_checkin_apts > patch->cur_outapts) {
            cache_time = last_checkin_apts - patch->cur_outapts;
            ALOGI("cache_time:%d, last_checkin:0x%x, cur_outapts:0x%x\n",
                    cache_time/90, last_checkin_apts, patch->cur_outapts);
        } else {
            ALOGI("pts abnormal last_checkin:0x%x, cur_outapts:0x%x\n",
                    last_checkin_apts, patch->cur_outapts);
        }
    }
    if (patch->tsync_mode == TSYNC_MODE_PCRMASTER) {
        ret = sysfs_set_sysfs_str(TSYNC_APTS, tempbuf);
        if (ret < 0) {
            ALOGI("update apt failed\n");
        }
    }
    dtv_audio_gap_monitor(patch);
}
