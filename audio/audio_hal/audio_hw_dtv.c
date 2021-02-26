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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

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
#include "aml_data_utils.h"
#include "aml_dump_debug.h"
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "audio_hw_profile.h"
#include "audio_hw_utils.h"
#include "dtv_patch_out.h"
#include "aml_audio_parser.h"
#include "aml_audio_resampler.h"
#include "audio_hw_ms12.h"
#include "dolby_lib_api.h"
#include "audio_dtv_ad.h"
#include "alsa_device_parser.h"
#include "aml_audio_hal_avsync.h"
#include "aml_audio_spdifout.h"
#include <aml_volume_utils.h>
#include <dmx_audio_es.h>

#define TSYNC_PCRSCR "/sys/class/tsync/pts_pcrscr"
#define TSYNC_EVENT "/sys/class/tsync/event"
#define TSYNC_APTS "/sys/class/tsync/pts_audio"
#define TSYNC_VPTS "/sys/class/tsync/pts_video"

#define TSYNC_AUDIO_MODE "/sys/class/tsync_pcr/tsync_audio_mode"
#define TSYNC_AUDIO_LEVEL "/sys/class/tsync_pcr/tsync_audio_level"
#define TSYNC_AUDIO_UNDERRUN "/sys/class/tsync_pcr/tsync_audio_underrun"
#define TSYNC_VIDEO_DISCONT "/sys/class/tsync_pcr/tsync_vdiscontinue"
#define TSYNC_VIDEO_STARTED "/sys/class/tsync/videostarted"
#define TSYNC_PCR_INITED    "/sys/class/tsync_pcr/tsync_pcr_inited_flag"

#define TSYNC_LAST_DISCONTINUE_CHECKIN_APTS "/sys/class/tsync_pcr/tsync_last_discontinue_checkin_apts"
#define TSYNC_LAST_CHECKIN_APTS "/sys/class/tsync/last_checkin_apts"
#define TSYNC_LAST_CHECKIN_VPTS "/sys/class/tsync/last_checkin_vpts"

#define TSYNC_APTS_DIFF "/sys/class/tsync_pcr/tsync_pcr_apts_diff"
#define TSYNC_FIRSTCHECKIN_APTS "/sys/class/tsync/checkin_firstapts"
#define TSYNC_FIRSTCHECKIN_VPTS "/sys/class/tsync/checkin_firstvpts"
#define TSYNC_FIRST_VPTS  "/sys/class/tsync/firstvpts"
#define TSYNC_DEMUX_PCR         "/sys/class/tsync/demux_pcr"
#define TSYNC_PCR_LANTCY        "/sys/class/tsync/pts_latency"
#define AMSTREAM_AUDIO_PORT_RESET   "/sys/class/amstream/reset_audio_port"
#define PROPERTY_ENABLE_AUDIO_RESAMPLE "vendor.media.audio.resample"
#define PROPERTY_AUDIO_DISCONTINUE_THRESHOLD "vendor.media.audio.discontinue_threshold"

#define PATCH_PERIOD_COUNT 4
#define DTV_PTS_CORRECTION_THRESHOLD (90000 * 30 / 1000)
#define DTV_PCR_DIS_DIFF_THRESHOLD (90000 * 150 / 1000)
#define AUDIO_PTS_DISCONTINUE_THRESHOLD (90000 * 5)
#define AC3_IEC61937_FRAME_SIZE 6144
#define EAC3_IEC61937_FRAME_SIZE 24576
#define DECODER_PTS_DEFAULT_LATENCY (200 * 90)
#define DEMUX_PCR_APTS_LATENCY (300 * 90)
#define PCM 0  /*AUDIO_FORMAT_PCM_16_BIT*/
#define DD 4   /*AUDIO_FORMAT_AC3*/
#define AUTO 5 /*choose by sink capability/source format/Digital format*/

#define DEFAULT_DTV_ADJUST_CLOCK    (1000)
#define DEFALUT_DTV_MIN_OUT_CLOCK   (1000*1000-100*1000)
#define DEFAULT_DTV_MAX_OUT_CLOCK   (1000*1000+100*1000)
#define DEFAULT_I2S_OUTPUT_CLOCK    (256*48000)
#define DEFAULT_CLOCK_MUL    (4)
#define AUDIO_EAC3_FRAME_SIZE 16
#define AUDIO_AC3_FRAME_SIZE 4
#define AUDIO_TV_PCM_FRAME_SIZE 32
#define AUDIO_DEFAULT_PCM_FRAME_SIZE 4
#define AUDIO_IEC61937_FRAME_SIZE 4
#define ADUIO_DOLBY_ADSUBFRAME_CHECKED_SIZE 2048

#define AUDIO_RESAMPLE_MIN_THRESHOLD 50
#define AUDIO_RESAMPLE_MIDDLE_THRESHOLD 100
#define AUDIO_RESAMPLE_MAX_THRESHOLD 150
#define AUDIO_FADEOUT_TV_SLEEP_US 200*1000
#define AUDIO_FADEOUT_STB_SLEEP_US 40*1000
#define MAX_BUFF_LEN 36
#define MAX(a, b) ((a) > (b)) ? (a) : (b)
//pthread_mutex_t dtv_patch_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dtv_cmd_mutex = PTHREAD_MUTEX_INITIALIZER;
struct cmd_list {
    struct cmd_list *next;
    int cmd;
    int cmd_num;
    int used;
    int initd;
};

struct cmd_list dtv_cmd_list = {
    .next = NULL,
    .cmd = -1,
    .cmd_num = 0,
    .used = 1,
};

struct cmd_list cmd_array[16]; // max cache 16 cmd;
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

static void init_cmd_list(void)
{
    dtv_cmd_list.next = NULL;
    dtv_cmd_list.cmd = -1;
    dtv_cmd_list.cmd_num = 0;
    dtv_cmd_list.used = 0;
    dtv_cmd_list.initd = 1;
    memset(cmd_array, 0, sizeof(cmd_array));
}

static void deinit_cmd_list(void)
{
    dtv_cmd_list.next = NULL;
    dtv_cmd_list.cmd = -1;
    dtv_cmd_list.cmd_num = 0;
    dtv_cmd_list.used = 0;
    dtv_cmd_list.initd = 0;
}

static struct cmd_list *cmd_array_get(void)
{
    int index = 0;

    pthread_mutex_lock(&dtv_cmd_mutex);
    for (index = 0; index < 16; index++) {
        if (cmd_array[index].used == 0) {
            break;
        }
    }

    if (index == 16) {
        pthread_mutex_unlock(&dtv_cmd_mutex);
        return NULL;
    }
    pthread_mutex_unlock(&dtv_cmd_mutex);
    return &cmd_array[index];
}
static void cmd_array_put(struct cmd_list *list)
{
    pthread_mutex_lock(&dtv_cmd_mutex);
    list->used = 0;
    pthread_mutex_unlock(&dtv_cmd_mutex);
}

static void _add_cmd_to_tail(struct cmd_list *node)
{
    struct cmd_list *list = &dtv_cmd_list;
    pthread_mutex_lock(&dtv_cmd_mutex);
    while (list->next != NULL) {
        list = list->next;
    }
    list->next = node;
    dtv_cmd_list.cmd_num++;
    pthread_mutex_unlock(&dtv_cmd_mutex);
}

int dtv_patch_add_cmd(int cmd)
{
    struct cmd_list *list = NULL;
    struct cmd_list *cmd_list = NULL;
    int index = 0;
    if (dtv_cmd_list.initd == 0) {
        return 0;
    }
    pthread_mutex_lock(&dtv_cmd_mutex);
    for (index = 0; index < 16; index++) {
        if (cmd_array[index].used == 0) {
            break;
        }
    }
    if (index == 16) {
        pthread_mutex_unlock(&dtv_cmd_mutex);
        ALOGI("list is full, add by live \n");
        return -1;
    }
    cmd_list = &cmd_array[index];
    if (cmd_list == NULL) {
        pthread_mutex_unlock(&dtv_cmd_mutex);
        ALOGI("can't get cmd list, add by live \n");
        return -1;
    }
    cmd_list->cmd = cmd;
    cmd_list->next = NULL;
    cmd_list->used = 1;
    list = &dtv_cmd_list;
    while (list->next != NULL) {
        list = list->next;
    }
    list->next = cmd_list;
    dtv_cmd_list.cmd_num++;
    pthread_mutex_unlock(&dtv_cmd_mutex);
    ALOGI("add by live dtv_patch_add_cmd the cmd is %d \n", cmd);
    return 0;
}

int dtv_patch_get_cmd(void)
{
    int cmd = AUDIO_DTV_PATCH_CMD_NUM;
    struct cmd_list *list = NULL;
    ALOGI("enter dtv_patch_get_cmd funciton now\n");
    pthread_mutex_lock(&dtv_cmd_mutex);
    list = dtv_cmd_list.next;
    if (list != NULL) {
        dtv_cmd_list.next = list->next;
        cmd = list->cmd;
        dtv_cmd_list.cmd_num--;
    } else {
        cmd =  AUDIO_DTV_PATCH_CMD_NULL;
        pthread_mutex_unlock(&dtv_cmd_mutex);
        return cmd;
    }
    list->used = 0;
    pthread_mutex_unlock(&dtv_cmd_mutex);
    ALOGI("leave dtv_patch_get_cmd the cmd is %d \n", cmd);
    return cmd;
}
int dtv_patch_cmd_is_empty(void)
{
    pthread_mutex_lock(&dtv_cmd_mutex);
    if (dtv_cmd_list.next == NULL) {
        pthread_mutex_unlock(&dtv_cmd_mutex);
        return 1;
    }
    pthread_mutex_unlock(&dtv_cmd_mutex);
    return 0;
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

static int dtv_patch_audio_info(void *args,unsigned char ori_channum,unsigned char lfepresent)
{
    struct aml_audio_patch *patch = (struct aml_audio_patch *)args;
    patch->dtv_NchOriginal = ori_channum;
    patch->dtv_lfepresent = lfepresent;
    return 1;
}

static void dtv_do_ease_out(struct aml_audio_device *aml_dev)
{
    if (aml_dev && aml_dev->audio_ease) {
        ALOGI("%s(), do fade out", __func__);
        start_ease_out(aml_dev);
        if (aml_dev->is_TV)
            usleep(AUDIO_FADEOUT_TV_SLEEP_US);
        else
            usleep(AUDIO_FADEOUT_STB_SLEEP_US);
    }
}

static void dtv_check_audio_reset()
{
    ALOGI("reset dtv audio port\n");
    aml_sysfs_set_str(AMSTREAM_AUDIO_PORT_RESET, "1");
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
#if 1
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
    if (aml_dev->sink_format != AUDIO_FORMAT_PCM_16_BIT && patch->aformat == AUDIO_FORMAT_E_AC3) {
        memcpy(mixbuffer + mix_size, mute_ddp_frame, sizeof(mute_ddp_frame));
        type = 2;
    } else if (aml_dev->sink_format != AUDIO_FORMAT_PCM_16_BIT && patch->aformat == AUDIO_FORMAT_AC3) {
        memcpy(mixbuffer + mix_size, mute_dd_frame, sizeof(mute_dd_frame));
        type = 1;
    } else {
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
            get_video_discontinue() != 1) {
            audio_discontinue = 1;
            ALOGI("cur_pts_diff=%d, diff=%d, apts=0x%x, pcrpts=0x%x\n",
                cur_pts_diff, cur_pts_diff/90, patch->last_apts, patch->last_pcrpts);
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
        if (eDolbyDcvLib == adev->dolby_lib_type && adev->ddp.digital_raw == 1)
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
    if (!patch || !patch->dev || !stream_out || aml_dev->tuner2mix_patch == 1) {
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
        ALOGI("dtv_do_insert:++insert lookup %d,diff %d ms\n", patch->dtv_apts_lookup, t1);
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
            ALOGI("write_used_ms = %d\n", write_used_ms);
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
        aml_audio_dump_audio_bitstreams("/data/audio_dtv.pcm",
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
            sysfs_set_sysfs_str(REPORT_DECODED_INFO, info_buf);
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
static int raw_dump_fd = -1;
void dump_raw_buffer(const void *data_buf, int size)
{
    ALOGI("enter the dump_raw_buffer save %d len data now\n", size);
    if (raw_dump_fd < 0) {
        if (access("/data/raw.es", 0) == 0) {
            raw_dump_fd = open("/data/raw.es", O_RDWR);
            if (raw_dump_fd < 0) {
                ALOGE("%s, Open device file \"%s\" error: %s.\n", __FUNCTION__,
                      "/data/raw.es", strerror(errno));
            }
        } else {
            raw_dump_fd = open("/data/raw.es", O_RDWR);
            if (raw_dump_fd < 0) {
                ALOGE("%s, Create device file \"%s\" error: %s.\n", __FUNCTION__,
                      "/data/raw.es", strerror(errno));
            }
        }
    }

    if (raw_dump_fd >= 0) {
        write(raw_dump_fd, data_buf, size);
    }
    return;
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
    int avail = get_buffer_read_space(ringbuffer);
    if (is_sc2_chip())
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
            if (aml_out->ddp_frame_size != 0) {
                write_len = aml_out->ddp_frame_size;
            }
        } else if (eDolbyDcvLib == aml_dev->dolby_lib_type) {
            if (aml_dev->ddp.curFrmSize != 0) {
                write_len = aml_dev->ddp.curFrmSize;
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
               aml_dev->ddp.ad_substream_supported = is_ad_substream_supported(patch->out_buf,write_len);
               ALOGI("ad_substream_supported %d",aml_dev->ddp.ad_substream_supported);
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
        } else {
            remain_size = aml_dev->ddp.remain_size;
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
        } else {
            patch->outlen_after_last_validpts += aml_dev->ddp.outlen_pcm;
            patch->decoder_offset += remain_size + ret - aml_dev->ddp.remain_size;
            patch->dtv_pcm_readed += ret;
        }

        if (eDolbyDcvLib == aml_dev->dolby_lib_type && patch->input_thread_exit != 1 &&
            aml_out->need_drop_size == 0) {
            //write spidfb dd + data
            if (is_use_spdifb(aml_out))
                ret = aml_dtv_spdif_output_new(stream_out, patch->out_buf, ret);
        }
        /* +[SE] [BUG][SWPL-22893][yinli.xia]
              add: reset decode data when replay video*/
        if (aml_dev->debug_flag) {
            ALOGI("after decode: decode_offset: %d, aml_dev->ddp.remain_size=%d\n",
                   patch->decoder_offset, aml_dev->ddp.remain_size);
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

        remain_size = aml_dev->dts_hd.remain_size;

        /* +[SE] [BUG][SWPL-22893][yinli.xia]
              add: reset decode data when replay video*/
        if (patch->dtv_replay_flag) {
            remain_size = 0;
            patch->dtv_replay_flag = false;
        }

        ret = out_write_new(stream_out, patch->out_buf, ret);
        patch->outlen_after_last_validpts += aml_dev->dts_hd.outlen_pcm;

        patch->decoder_offset +=
            remain_size + ret - aml_dev->dts_hd.remain_size;
        patch->dtv_pcm_readed += ret;
        /* +[SE] [BUG][SWPL-22893][yinli.xia]
              add: reset decode data when replay video*/
        if (aml_dev->debug_flag) {
            ALOGI("after decode: aml_dev->ddp.remain_size=%d\n", aml_dev->ddp.remain_size);
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
    //int apts_diff = 0;

    unsigned char main_head[32];
    unsigned char ad_head[32];
    int main_frame_size = 0, last_main_frame_size = 0, main_head_offset = 0, main_head_left = 0;
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
    if (is_sc2_chip())
        audio_dtv_underrun_loop_mute_check(patch, stream_out);
    int ad_avail = dtv_assoc_get_avail();
    dtv_assoc_get_main_frame_size(&last_main_frame_size);
    char buff[32];
    int ret = 0;
    //ALOGI("AD main_avail=%d ad_avail=%d last_main_frame_size = %d",
    //main_avail, ad_avail, last_main_frame_size);
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
                //ALOGI("AD main_frame_size=%d  ", main_frame_size);
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
        if (is_sc2_chip()) {
            struct mAudioEsDataInfo *mEsData = NULL;
            int try_count = 3;
            while (Get_ADAudio_Es(&mEsData) != 0 ) {
                if ( patch->output_thread_exit == 1) {
                    pthread_mutex_unlock(&(patch->dtv_output_mutex));
                    return 0;
                }
                if (try_count-- <= 0)
                    break;
                usleep(1000);
            }
            if (mEsData == NULL) {
                ad_size = 0;
            } else {
                ad_size = mEsData->size;
                ALOGV("ad mEsData->size %d",mEsData->size);
                memcpy(ad_buffer,mEsData->data, mEsData->size);
                free(mEsData);
                mEsData = NULL;
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
        } else {
            remain_size = aml_dev->ddp.remain_size;
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
            patch->outlen_after_last_validpts += aml_dev->ddp.outlen_pcm;
            patch->decoder_offset += remain_size + main_size - aml_dev->ddp.remain_size;
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
        if (out_standby_direct == aml_out->stream.common.standby)
             out_standby_direct((struct audio_stream *)aml_out);
        else
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
    patch->dtv_audio_mode = get_dtv_audio_mode();
    patch->dtv_audio_tune = AUDIO_FREE;
    patch->first_apts_lookup_over = 0;
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
            (patch->aformat == AUDIO_FORMAT_E_AC3)) {
            ALOGV("AD %d %d %d", aml_dev->dolby_lib_type, aml_dev->dual_decoder_support, aml_dev->sub_apid);
            if (aml_dev->dual_decoder_support && VALID_PID(aml_dev->sub_apid)) {
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
    }
    aml_audio_free(patch->out_buf);
exit_outbuf:
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


static void patch_thread_get_cmd(struct aml_audio_patch *patch, int *cmd)
{
    if (dtv_cmd_list.initd == 0) {
        *cmd = AUDIO_DTV_PATCH_CMD_NULL;
        return;
    }
    if (patch == NULL) {
        *cmd = AUDIO_DTV_PATCH_CMD_NULL;
        return;
    }
    if (patch->input_thread_exit == 1) {
        *cmd = AUDIO_DTV_PATCH_CMD_NULL;
        return;
    }
    if (dtv_patch_cmd_is_empty() == 1) {
        *cmd = AUDIO_DTV_PATCH_CMD_NULL;
    } else {
        *cmd = dtv_patch_get_cmd();
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
    struct dolby_ddp_dec *ddp_dec = &(aml_dev->ddp);
    struct dca_dts_dec *dts_dec = &(aml_dev->dts_hd);
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

    while (!patch->input_thread_exit) {
        pthread_mutex_lock(&patch->dtv_input_mutex);

        switch (patch->dtv_decoder_state) {
        case AUDIO_DTV_PATCH_DECODER_STATE_INIT: {
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

            if (patch->input_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                goto exit;
            }
            patch_thread_get_cmd(patch, &cmd);
            if (cmd == AUDIO_DTV_PATCH_CMD_NULL) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
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
                    ddp_dec->is_iec61937 = false;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_EAC3) {
                    patch->aformat = AUDIO_FORMAT_E_AC3;
                    ddp_dec->is_iec61937 = false;
                    patch->decoder_offset = 0;
                } else if (patch->dtv_aformat == ACODEC_FMT_DTS) {
                    patch->aformat = AUDIO_FORMAT_DTS;
                    dts_dec->frame_info.is_iec61937 = false;
                    dca_decoder_init_patch(dts_dec);
                    patch->decoder_offset = 0;
                } else {
                    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
                    patch->decoder_offset = 0;
                }

                bool associate_mix = aml_dev->associate_audio_mixing_enable;
                bool dual_decoder = aml_dev->dual_decoder_support;
                int demux_id  = aml_dev->demux_id;
                patch->pid = aml_dev->pid;
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
				if (is_sc2_chip())
                    Open_Dmx_Audio(demux_id);
                dtv_patch_input_start(adec_handle,
                                      demux_id,
                                      patch->pid,
                                      patch->dtv_aformat,
                                      patch->dtv_has_video,
                                      dual_decoder,
                                      associate_mix,
                                      aml_dev->mixing_level);
                ALOGI("[audiohal_kpi]++%s live now  start the audio decoder now !\n",
                      __FUNCTION__);
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
                create_dtv_output_stream_thread(patch);
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
                continue;
            }
            break;
        case AUDIO_DTV_PATCH_DECODER_STATE_RUNING:

            if (patch->input_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                goto exit;
            }
            /*[SE][BUG][SWPL-17416][chengshun] maybe sometimes subafmt and subapid not set before dtv patch start*/
            if (aml_dev->ad_start_enable == 0 && VALID_PID(aml_dev->sub_apid) && VALID_AD_FMT(aml_dev->sub_afmt)) {
                if (aml_dev->dolby_lib_type == eDolbyMS12Lib) {
                    if (aml_dev->disable_pcm_mixing == false || aml_dev->hdmi_format == PCM ||
                        aml_dev->sink_capability == AUDIO_FORMAT_PCM_16_BIT || aml_dev->sink_capability == AUDIO_FORMAT_PCM_32_BIT) {
                        int ad_start_flag;
                        if (is_sc2_chip()) {
                            ad_start_flag = Init_Dmx_AD_Audio(aml_dev->sub_afmt,aml_dev->sub_apid,aml_dev->security_mem_level);
                            if (ad_start_flag == 0)
                                Start_Dmx_AD_Audio();
                        } else {
                            ad_start_flag = dtv_assoc_audio_start(1, aml_dev->sub_apid, aml_dev->sub_afmt);
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
                        if (is_sc2_chip()) {
                            ad_start_flag = Init_Dmx_AD_Audio(aml_dev->sub_afmt,aml_dev->sub_apid,aml_dev->security_mem_level);
                            if (ad_start_flag == 0)
                                Start_Dmx_AD_Audio();
                        } else {
                            ad_start_flag = dtv_assoc_audio_start(1, aml_dev->sub_apid, aml_dev->sub_afmt);
                        }
                        if (ad_start_flag == 0) {
                            aml_dev->ad_start_enable = 1;
                        }
                    }
                }
            }

            patch_thread_get_cmd(patch, &cmd);
            if (cmd == AUDIO_DTV_PATCH_CMD_NULL) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
                continue;
            }
            if (cmd == AUDIO_DTV_PATCH_CMD_PAUSE) {
                ALOGI("++%s live now start  pause  the audio decoder now \n",
                      __FUNCTION__);
                dtv_patch_input_pause(adec_handle);
                if (is_sc2_chip()) {
                    Stop_Dmx_AD_Audio();
                } else {
                    dtv_assoc_audio_pause(1);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_PAUSE;
                ALOGI("++%s live now end  pause  the audio decoder now \n",
                      __FUNCTION__);
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                      __FUNCTION__);
                dtv_do_ease_out(aml_dev);
                release_dtv_output_stream_thread(patch);
                dtv_adjust_output_clock(patch, DIRECT_NORMAL, DEFAULT_DTV_ADJUST_CLOCK, false);
                dtv_patch_input_stop(adec_handle);
                if (is_sc2_chip()) {
                    Stop_Dmx_AD_Audio();
                } else {
                    dtv_assoc_audio_stop(1);
                }
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
                continue;
            }

            break;

        case AUDIO_DTV_PATCH_DECODER_STATE_PAUSE:
            patch_thread_get_cmd(patch, &cmd);
            if (cmd == AUDIO_DTV_PATCH_CMD_NULL) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
                continue;
            }
            if (cmd == AUDIO_DTV_PATCH_CMD_RESUME) {
                dtv_patch_input_resume(adec_handle);
                if (is_sc2_chip()) {
                    Start_Dmx_AD_Audio();
                } else {
                    dtv_assoc_audio_resume(1,aml_dev->sub_apid);
                }
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_RUNING;
            } else if (cmd == AUDIO_DTV_PATCH_CMD_STOP) {
                ALOGI("[audiohal_kpi]++%s live now  stop  the audio decoder now \n",
                     __FUNCTION__);
                dtv_patch_input_stop(adec_handle);
                if (is_sc2_chip()) {
                    Stop_Dmx_AD_Audio(1);
                } else {
                    dtv_assoc_audio_stop(1);
                }
                aml_dev->ad_start_enable = 0;
                dtv_check_audio_reset();
                patch->dtv_decoder_state = AUDIO_DTV_PATCH_DECODER_STATE_INIT;
            } else {
                ALOGI("++%s line %d  live state unsupport state %d cmd %d !\n",
                      __FUNCTION__, __LINE__, patch->dtv_decoder_state, cmd);
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                usleep(50000);
                continue;
            }
            break;
        default:
            if (patch->input_thread_exit == 1) {
                pthread_mutex_unlock(&patch->dtv_input_mutex);
                goto exit;
            }
            break;
        }
        pthread_mutex_unlock(&patch->dtv_input_mutex);
    }

exit:
    ALOGI("[audiohal_kpi]++%s now  live  release  the audio decoder", __FUNCTION__);
    dtv_patch_input_stop(adec_handle);
    aml_dev->ad_start_enable = 0;
    if (is_sc2_chip()) {
        Close_Dmx_Audio();
    } else {
        dtv_assoc_audio_stop(1);
    }
    dtv_check_audio_reset();
    ALOGI("[audiohal_kpi]++%s Exit", __FUNCTION__);
    pthread_exit(NULL);
}

void release_dtvin_buffer(struct aml_audio_patch *patch)
{
    if (patch->dtvin_buffer_inited == 1) {
        patch->dtvin_buffer_inited = 0;
        ring_buffer_release(&(patch->dtvin_ringbuffer));
    }
}

void dtv_in_write(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    int abuf_level = 0;

    if (stream == NULL || buffer == NULL || bytes == 0) {
        ALOGI("[%s] pls check the input parameters \n", __FUNCTION__);
        return ;
    }
    if ((adev->patch_src == SRC_DTV) && (patch->dtvin_buffer_inited == 1)) {
        abuf_level = get_buffer_write_space(&patch->dtvin_ringbuffer);
        if (abuf_level <= (int)bytes) {
            ALOGI("[%s] dtvin ringbuffer is full", __FUNCTION__);
            return;
        }
        ring_buffer_write(&patch->dtvin_ringbuffer, (unsigned char *)buffer, bytes, UNCOVER_WRITE);
    }
    //ALOGI("[%s] dtvin write ringbuffer successfully,abuf_level=%d", __FUNCTION__,abuf_level);
}
int dtv_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes)
{
    int ret = 0;
    unsigned int es_length = 0;
    struct aml_stream_in *in = (struct aml_stream_in *)stream;
    struct aml_audio_device *adev = in->dev;

    if (stream == NULL || buffer == NULL || bytes == 0) {
        ALOGI("[%s] pls check the input parameters \n", __FUNCTION__);
    }

    struct aml_audio_patch *patch = adev->audio_patch;
    struct dolby_ddp_dec *ddp_dec = & (adev->ddp);
    //ALOGI("[%s] patch->aformat=0x%x patch->dtv_decoder_ready=%d bytes:%d\n", __FUNCTION__,patch->aformat,patch->dtv_decoder_ready,bytes);

    if (patch->dtvin_buffer_inited == 1) {
        int abuf_level = get_buffer_read_space(&patch->dtvin_ringbuffer);
        if (abuf_level <= (int)bytes) {
            memset(buffer, 0, sizeof(unsigned char)* bytes);
            ret = bytes;
        } else {
            ret = ring_buffer_read(&patch->dtvin_ringbuffer, (unsigned char *)buffer, bytes);
            //ALOGI("[%s] abuf_level =%d ret=%d\n", __FUNCTION__,abuf_level,ret);
        }
        return bytes;
    } else {
        memset(buffer, 0, sizeof(unsigned char)* bytes);
        ret = bytes;
        return ret;
    }
    return ret;
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

#define AVSYNC_SAMPLE_INTERVAL (50)
#define AVSYNC_SAMPLE_MAX_CNT (10)

static int create_dtv_output_stream_thread(struct aml_audio_patch *patch)
{
    int ret = 0;
    ALOGI("++%s   ---- %d\n", __FUNCTION__, patch->ouput_thread_created);

    if (patch->ouput_thread_created == 0) {
        patch->output_thread_exit = 0;
        pthread_mutex_init(&patch->dtv_output_mutex, NULL);
        patch->dtv_replay_flag = true;
        ret = pthread_create(&(patch->audio_output_threadID), NULL,
                             audio_dtv_patch_output_threadloop, patch);
        if (ret != 0) {
            ALOGE("%s, Create output thread fail!\n", __FUNCTION__);
            pthread_mutex_destroy(&patch->dtv_output_mutex);
            return -1;
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

int create_dtv_patch_l(struct audio_hw_device *dev, audio_devices_t input,
                       audio_devices_t output __unused)
{
    struct aml_audio_patch *patch;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    int period_size = DEFAULT_PLAYBACK_PERIOD_SIZE * PLAYBACK_PERIOD_COUNT;
    pthread_attr_t attr;
    struct sched_param param;

    int ret = 0;
    // ALOGI("++%s live period_size %d\n", __func__, period_size);
    //pthread_mutex_lock(&aml_dev->patch_lock);
    if (aml_dev->audio_patch) {
        ALOGD("%s: patch exists, first release it", __func__);
        if (aml_dev->audio_patch->is_dtv_src) {
            release_dtv_patch_l(aml_dev);
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

    memset(cmd_array, 0, sizeof(cmd_array));
    // save dev to patch
    patch->dev = dev;
    patch->input_src = input;
    patch->aformat = AUDIO_FORMAT_PCM_16_BIT;
    patch->avsync_sample_max_cnt = AVSYNC_SAMPLE_MAX_CNT;
    patch->is_dtv_src = true;
    patch->startplay_avsync_flag = 1;
    patch->ad_substream_checked_flag = false;

    patch->output_thread_exit = 0;
    patch->input_thread_exit = 0;
    memset(&patch->sync_para, 0, sizeof(struct avsync_para));

    patch->i2s_div_factor = property_get_int32(PROPERTY_AUDIO_TUNING_CLOCK_FACTOR, DEFAULT_TUNING_CLOCK_FACTOR);
    if (patch->i2s_div_factor == 0)
        patch->i2s_div_factor = DEFAULT_TUNING_CLOCK_FACTOR;

    aml_dev->audio_patch = patch;
    pthread_mutex_init(&patch->mutex, NULL);
    pthread_cond_init(&patch->cond, NULL);
    pthread_mutex_init(&patch->dtv_input_mutex, NULL);
    pthread_mutex_init(&patch->apts_cal_mutex, NULL);

    ret = ring_buffer_init(&(patch->aml_ringbuffer),
                           4 * period_size * PATCH_PERIOD_COUNT * 10);
    if (ret < 0) {
        ALOGE("Fail to init audio ringbuffer!");
        goto err_ring_buf;
    }

    if (aml_dev->useSubMix) {
        // switch normal stream to old tv mode writing
        switchNormalStream(aml_dev->active_outputs[STREAM_PCM_NORMAL], 0);
    }

    ret = pthread_create(&(patch->audio_input_threadID), NULL,
                         audio_dtv_patch_process_threadloop, patch);
    if (ret != 0) {
        ALOGE("%s, Create process thread fail!\n", __FUNCTION__);
        goto err_in_thread;
    }
    create_dtv_output_stream_thread(patch);
    init_cmd_list();
    if (aml_dev->tuner2mix_patch) {
        ret = ring_buffer_init(&patch->dtvin_ringbuffer, 4 * 8192 * 2 * 16);
        ALOGI("[%s] aring_buffer_init ret=%d\n", __FUNCTION__, ret);
        if (ret == 0) {
            patch->dtvin_buffer_inited = 1;
        }
    }
    dtv_assoc_init();
    patch->dtv_aformat = aml_dev->dtv_aformat;
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
    patch->input_thread_exit = 1;
    pthread_join(patch->audio_input_threadID, NULL);
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

    ALOGI("[audiohal_kpi]++%s Enter\n", __FUNCTION__);
    if (patch == NULL) {
        ALOGI("release the dtv patch patch == NULL\n");
        return 0;
    }
    deinit_cmd_list();
    patch->input_thread_exit = 1;
    if (aml_dev->ddp.spdifout_handle) {
         aml_audio_spdifout_close(aml_dev->ddp.spdifout_handle);
         aml_dev->ddp.spdifout_handle = NULL;
    }
    pthread_join(patch->audio_input_threadID, NULL);

    pthread_mutex_destroy(&patch->dtv_input_mutex);
    if (patch->resample_outbuf) {
        aml_audio_free(patch->resample_outbuf);
        patch->resample_outbuf = NULL;
    }
    patch->pid = -1;
    release_dtv_output_stream_thread(patch);
    release_dtvin_buffer(patch);
    dtv_assoc_deinit();
    ring_buffer_release(&(patch->aml_ringbuffer));
    pthread_mutex_destroy(&patch->apts_cal_mutex);
    aml_audio_free(patch);
    if (aml_dev->start_mute_flag != 0)
        aml_dev->start_mute_flag = 0;
    aml_dev->underrun_mute_flag = 0;
    aml_dev->audio_patch = NULL;
    aml_dev->sub_apid = -1;
    aml_dev->sub_afmt = ACODEC_FMT_NULL;
    aml_dev->dual_decoder_support = 0;
    aml_dev->associate_audio_mixing_enable = 0;
    //aml_dev->mixing_level = -32;
    ALOGI("[audiohal_kpi]--%s Exit", __FUNCTION__);
    //pthread_mutex_unlock(&aml_dev->patch_lock);
    if (aml_dev->useSubMix) {
        switchNormalStream(aml_dev->active_outputs[STREAM_PCM_NORMAL], 1);
    }
    return 0;
}

int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input,
                     audio_devices_t output)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)dev;
    int ret = 0;
    pthread_mutex_lock(&aml_dev->patch_lock);
    ret = create_dtv_patch_l(dev, input, output);
    pthread_mutex_unlock(&aml_dev->patch_lock);

    return ret;
}

int release_dtv_patch(struct aml_audio_device *aml_dev)
{
    int ret = 0;

    dtv_do_ease_out(aml_dev);
    pthread_mutex_lock(&aml_dev->patch_lock);
    ret = release_dtv_patch_l(aml_dev);
    pthread_mutex_unlock(&aml_dev->patch_lock);

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
