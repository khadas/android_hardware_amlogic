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

#ifndef _AUDIO_HW_DTV_H_
#define _AUDIO_HW_DTV_H_

#include <cutils/str_parms.h>

enum {
    AUDIO_DTV_PATCH_DECODER_STATE_INIT,
    AUDIO_DTV_PATCH_DECODER_STATE_START,
    AUDIO_DTV_PATCH_DECODER_STATE_RUNING,
    AUDIO_DTV_PATCH_DECODER_STATE_PAUSE,
    AUDIO_DTV_PATCH_DECODER_STATE_RESUME,
    AUDIO_DTV_PATCH_DECODER_STATE_RELEASE,
};

/* refer to AudioSystemCmdManager */
typedef enum {
    AUDIO_DTV_PATCH_CMD_NULL        = 0,
    AUDIO_DTV_PATCH_CMD_START       = 1,    /* AUDIO_SERVICE_CMD_START_DECODE */
    AUDIO_DTV_PATCH_CMD_PAUSE       = 2,    /* AUDIO_SERVICE_CMD_PAUSE_DECODE */
    AUDIO_DTV_PATCH_CMD_RESUME      = 3,    /* AUDIO_SERVICE_CMD_RESUME_DECODE */
    AUDIO_DTV_PATCH_CMD_STOP        = 4,    /* AUDIO_SERVICE_CMD_STOP_DECODE */
    AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT  = 5,    /* AUDIO_SERVICE_CMD_SET_DECODE_AD */
    AUDIO_DTV_PATCH_CMD_SET_VOLUME  = 6,    /*AUDIO_SERVICE_CMD_SET_VOLUME*/
    AUDIO_DTV_PATCH_CMD_SET_MUTE    = 7,    /*AUDIO_SERVICE_CMD_SET_MUTE*/
    AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE = 8,/*AUDIO_SERVICE_CMD_SET_OUTPUT_MODE */
    AUDIO_DTV_PATCH_CMD_SET_PRE_GAIN  = 9,    /*AUDIO_SERVICE_CMD_SET_PRE_GAIN */
    AUDIO_DTV_PATCH_CMD_SET_PRE_MUTE  = 10,  /*AUDIO_SERVICE_CMD_SET_PRE_MUTE */
    AUDIO_DTV_PATCH_CMD_OPEN        = 12,   /*AUDIO_SERVICE_CMD_OPEN_DECODER */
    AUDIO_DTV_PATCH_CMD_CLOSE       = 13,   /*AUDIO_SERVICE_CMD_CLOSE_DECODER */
    AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO = 14, /*AUDIO_SERVICE_CMD_SET_DEMUX_INFO ;*/
    AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL = 15,/*AUDIO_SERVICE_CMD_SET_SECURITY_MEM_LEVEL*/
    AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO   = 16,/*AUDIO_SERVICE_CMD_SET_HAS_VIDEO */
    AUDIO_DTV_PATCH_CMD_CONTROL       = 17,
    AUDIO_DTV_PATCH_CMD_SET_PID       = 18,
    AUDIO_DTV_PATCH_CMD_SET_FMT        = 19,
    AUDIO_DTV_PATCH_CMD_SET_AD_PID      = 20,
    AUDIO_DTV_PATCH_CMD_SET_AD_FMT      = 21,
    AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE      = 22,
    AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL   = 23,
    AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL   = 24,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID   = 25,
    AUDIO_DTV_PATCH_CMD_SET_MEDIA_PRESENTATION_ID   = 26,
    AUDIO_DTV_PATCH_CMD_NUM             = 27,
} AUDIO_DTV_PATCH_CMD_TYPE;

enum {
    AVSYNC_ACTION_NORMAL,
    AVSYNC_ACTION_DROP,
    AVSYNC_ACTION_HOLD,
};
enum {
    DIRECT_SPEED = 0, // DERIECT_SPEED
    DIRECT_SLOW,
    DIRECT_NORMAL,
};
enum {
    AUDIO_FREE = 0,
    AUDIO_BREAK,
    AUDIO_LOOKUP,
    AUDIO_DROP,
    AUDIO_RAISE,
    AUDIO_LATENCY,
    AUDIO_RUNNING,
};
enum {
    TSYNC_MODE_VMASTER = 0,
    TSYNC_MODE_AMASTER,
    TSYNC_MODE_PCRMASTER,
};

int create_dtv_patch(struct audio_hw_device *dev, audio_devices_t input, audio_devices_t output __unused);
int release_dtv_patch(struct aml_audio_device *dev);
int release_dtv_patch_l(struct aml_audio_device *dev);
#if ANDROID_PLATFORM_SDK_VERSION > 29
int enable_dtv_patch_for_tuner_framework(struct audio_config *config, struct audio_hw_device *dev);
int disable_dtv_patch_for_tuner_framework(struct audio_hw_device *dev);
#endif
//int dtv_patch_add_cmd(int cmd);
void save_latest_dtv_aformat(int afmt);
int audio_set_spdif_clock(struct aml_stream_out *stream,int type);
int dtv_get_syncmode(void);

void clean_dtv_patch_pts(struct aml_audio_patch *patch);
int audio_decoder_status(unsigned int *perror_count);
extern size_t aml_alsa_output_write(struct audio_stream_out *stream, void *buffer, size_t bytes);

extern int get_tsync_pcr_debug(void);
extern int get_video_delay(void);
extern void set_video_delay(int delay_ms);
extern void dtv_do_process_pcm(int avail, struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out);
extern void dtv_do_insert_zero_pcm(struct aml_audio_patch *patch,
                            struct audio_stream_out *stream_out);
extern void dtv_do_drop_pcm(int avail, struct aml_audio_patch *patch);
extern void dtv_adjust_output_clock(struct aml_audio_patch * patch, int direct, int step, bool is_dual);
extern void dtv_avsync_process(struct aml_audio_patch* patch, struct aml_stream_out* stream_out);

extern void decoder_set_pcrsrc(unsigned int pcrsrc);
int get_audio_checkin_underrun(void);
int set_dtv_parameters(struct audio_hw_device *dev, struct str_parms *parms);
bool is_dtv_patch_alive(struct aml_audio_device *aml_dev);
int dtv_patch_get_latency(struct aml_audio_device *aml_dev);

#endif
