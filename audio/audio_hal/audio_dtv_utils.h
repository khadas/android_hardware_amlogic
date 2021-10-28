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

#ifndef _AUDIO_DTV_UTILS_H_
#define _AUDIO_DTV_UTILS_H_

#include "dmx_audio_es.h"
#include "aml_dec_api.h"

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

#define DTV_PTS_CORRECTION_THRESHOLD (90000 * 30 / 1000)
#define DTV_PCR_DIS_DIFF_THRESHOLD (90000 * 150 / 1000)
#define AUDIO_PTS_DISCONTINUE_THRESHOLD (90000 * 5)
#define AC3_IEC61937_FRAME_SIZE 6144
#define EAC3_IEC61937_FRAME_SIZE 24576
#define DECODER_PTS_DEFAULT_LATENCY (200 * 90)
#define DEMUX_PCR_APTS_LATENCY (300 * 90)

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
#define AUDIO_FADEOUT_TV_DURATION_US 100 * 1000
#define AUDIO_FADEOUT_STB_DURATION_US 40 * 1000
#define MAX_BUFF_LEN 36
#define MAX(a, b) ((a) > (b)) ? (a) : (b)

#define INPUT_PACKAGE_MAXCOUNT 40


#define AD_PACK_STATUS_UNNORMAL_THRESHOLD_MS 4000

#define AD_PACK_STATUS_DROP_THRESHOLD_MS 600
#define AD_PACK_STATUS_DROP_START_THRESHOLD_MS 60

#define AD_PACK_STATUS_HOLD_THRESHOLD_MS 400
#define AD_PACK_STATUS_HOLD_START_THRESHOLD_MS 100


#define NON_DOLBY_AD_PACK_STATUS_DROP_THRESHOLD_MS 3000
#define NON_DOLBY_AD_PACK_STATUS_DROP_START_THRESHOLD_MS 2000

#define NON_DOLBY_AD_PACK_STATUS_HOLD_THRESHOLD_MS 600
#define NON_DOLBY_AD_PACK_STATUS_HOLD_START_THRESHOLD_MS 100

typedef enum  {
    AD_PACK_STATUS_NORMAL,
    AD_PACK_STATUS_DROP,
    AD_PACK_STATUS_HOLD,
} AD_PACK_STATUS_T;

struct cmd_list {
    struct cmd_list *next;
    int cmd;
    int cmd_num;
    int used;
    int initd;
};

struct cmd_node {
    struct cmd_node *next;
    int cmd;
    int cmd_num;
    int used;
    int initd;
    int path_id;
    pthread_mutex_t dtv_cmd_mutex;
};

struct package {
    char *data;//buf ptr
    int size;  //package size
    char *ad_data;//ad buf ptr
    int  ad_size;//apackage size
    struct package * next;//next ptr
    uint64_t pts;
    uint64_t ad_pts;
};

typedef struct {
    struct package *first;
    int pack_num;
    struct package *current;
    pthread_mutex_t tslock;
} package_list;

int dtv_package_list_flush(package_list *list);


int dtv_package_list_init(package_list *list);
int dtv_package_add(package_list *list, struct package *p);

struct package * dtv_package_get(package_list *list);

void init_cmd_list(struct cmd_node *dtv_cmd_list);

void deinit_cmd_list(struct cmd_node *dtv_cmd_list);
int dtv_patch_add_cmd(struct cmd_node *dtv_cmd_list,int cmd, int path_id);


int dtv_patch_get_cmd(struct cmd_node *dtv_cmd_list,int *cmd, int *path_id);
int dtv_patch_cmd_is_empty(struct cmd_node *dtv_cmd_list);

AD_PACK_STATUS_T check_ad_package_status(int64_t main_pts, int64_t ad_pts,  aml_demux_audiopara_t *demux_info);


#endif
