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

#ifndef _AML_AUDIO_HAL_AVSYNC_H_
#define _AML_AUDIO_HAL_AVSYNC_H_

#define TSYNC_PCRSCR "/sys/class/tsync/pts_pcrscr"
#define TSYNC_EVENT "/sys/class/tsync/event"
#define TSYNC_APTS "/sys/class/tsync/pts_audio"
#define TSYNC_VPTS "/sys/class/tsync/pts_video"
#define TSYNC_CHECKIN_APTS "/sys/class/tsync/last_checkin_apts"
#define TSYNC_CHECKIN_VPTS "/sys/class/tsync/last_checkin_vpts"


#define TSYNC_AUDIO_LEVEL "/sys/class/tsync_pcr/tsync_audio_level"

#define TSYNC_LAST_CHECKIN_APTS "/sys/class/tsync/last_checkin_apts"

#define TSYNC_PCR_DEBUG "/sys/class/tsync_pcr/tsync_pcr_debug"
#define TSYNC_APTS_DIFF "/sys/class/tsync_pcr/tsync_pcr_apts_diff"
#define TSYNC_VPTS_ADJ "/sys/class/tsync_pcr/tsync_vpts_adjust"
#define TSYNC_PCR_MODE "/sys/class/tsync_pcr/tsync_pcr_mode"
#define TSYNC_FIRST_VPTS  "/sys/class/tsync/firstvpts"
#define TSYNC_DEMUX_PCR         "/sys/class/tsync/demux_pcr"
#define TSYNC_LASTCHECKIN_APTS "/sys/class/tsync/last_checkin_apts"
#define VIDEO_FIRST_FRAME_SHOW  "/sys/module/amvideo/parameters/first_frame_toggled"
#define VIDEO_FIRST_FRAME_SHOW_2  "/sys/module/aml_media/parameters/first_frame_toggled"
#define VIDEO_DISPLAY_FRAME_CNT "/sys/module/amvideo/parameters/display_frame_count"
#define VIDEO_RECEIVE_FRAME_CNT "/sys/module/amvideo/parameters/receive_frame_count"
#define VIDEO_SHOW_FIRST_FRAME "/sys/class/video/show_first_frame_nosync"


#define DTV_DECODER_PTS_LOOKUP_PATH "/sys/class/tsync/apts_lookup"
#define DTV_DECODER_CHECKIN_FIRSTAPTS_PATH "/sys/class/tsync/checkin_firstapts"
#define DTV_DECODER_TSYNC_MODE      "/sys/class/tsync/mode"
#define PROPERTY_LOCAL_ARC_LATENCY   "vendor.media.amnuplayer.audio.delayus"
#define PROPERTY_LOCAL_PASSTHROUGH_LATENCY  "vendor.media.dtv.passthrough.latencyms"
#define PROPERTY_PRESET_AC3_PASSTHROUGH_LATENCY  "vendor.media.dtv.passthrough.ac3prelatencyms"
#define PROPERTY_AUDIO_ADJUST_PCR_MAX   "vendor.media.audio.adjust.pcr.max"
#define PROPERTY_UNDERRUN_MUTE_MINTIME     "vendor.media.audio.underrun.mute.mintime"
#define PROPERTY_UNDERRUN_MUTE_MAXTIME     "vendor.media.audio.underrun.mute.maxtime"
#define PROPERTY_UNDERRUN_MAX_TIME      "vendor.media.audio.underruncheck.max.time"
#define PROPERTY_AUDIO_TUNING_PCR_CLOCK_STEPS "vendor.media.audio.tuning.pcr.clocksteps"
#define PROPERTY_AUDIO_TUNING_CLOCK_FACTOR  "vendor.media.audio.tuning.clock.factor"
#define PROPERTY_AUDIO_DROP_THRESHOLD  "vendor.media.audio.drop.thresholdms"
#define PROPERTY_AUDIO_LEAST_CACHE  "vendor.media.audio.leastcachems"
#define PROPERTY_DEBUG_TIME_INTERVAL  "vendor.media.audio.debug.timeinterval"


#define DTV_PTS_CORRECTION_THRESHOLD (90000 * 30 / 1000)
#define AUDIO_PTS_DISCONTINUE_THRESHOLD (90000 * 5)
#define DECODER_PTS_MAX_LATENCY (320 * 90)
#define DEMUX_PCR_APTS_LATENCY (300 * 90)
#define DEFAULT_ARC_DELAY_MS (100)
#define DEFAULT_SYSTEM_TIME (90000)
#define TIMEOUT_WAIT_TIME (1000 * 3)

#define DEFAULT_DTV_OUTPUT_CLOCK    (1000*1000)
#define DEFAULT_DTV_ADJUST_CLOCK    (1000)
#define DEFALUT_DTV_MIN_OUT_CLOCK   (1000*1000-100*1000)
#define DEFAULT_DTV_MAX_OUT_CLOCK   (1000*1000+100*1000)
#define DEFAULT_I2S_OUTPUT_CLOCK    (256*48000)
#define DEFAULT_CLOCK_MUL    (4)
#define DEFAULT_SPDIF_PLL_DDP_CLOCK    (256*48000*2)
#define DEFAULT_SPDIF_ADJUST_TIMES    (4)
#define DEFAULT_STRATEGY_ADJUST_CLOCK    (100)
#define DEFAULT_TUNING_PCR_CLOCK_STEPS (256 * 64)
#define DEFAULT_TUNING_CLOCK_FACTOR (7)
#define DEFAULT_AUDIO_DROP_THRESHOLD_MS (60)
#define DEFAULT_AUDIO_LEAST_CACHE_MS (50)
#define AUDIO_PCR_LATENCY_MAX (3000)
#define DEFULT_DEBUG_TIME_INTERVAL (5000)

//channel define
#define DEFAULT_CHANNELS 2
#define DEFAULT_SAMPLERATE 48
#define DEFAULT_DATA_WIDTH 2

   /*------------------------------------------------------------
    this function is designed for av alternately distributed situation
    startplay have below situation
    1.apts = vpts
    strategy 0: when pcr >= vpts, av pts sync output together
    2.apts > vpts
    strategy 1: audio write 0, video not show first frame
                when pcr >= vpts, video output normal
                when pcr >= apts, audio output normal
    strategy 2: show first frame, video fast play
                when pcr >= vpts, video output normal
                when pcr >= apts, audio output normal
    3.apts < vpts
    strategy 3: video show first frame and block
                when pcr >= apts, audio output normal
                when pcr >= vpts, av pts sync output together
    strategy 4: video not show first frame
                when pcr >= apts, mute audio
                when pcr >= vpts, av pts sync output together
    strategy 5: video show first frame and block
                when pcr >= apts, mute audio
                when pcr >= vpts, av pts sync output together
    strategy 6: drop audio pcm, video show first frame and block
                when apts >= vpts, pcr is set by (vpts - latency)
    strategy 7: drop audio pcm, video not show first frame
                when apts >= vpts, pcr is set by (vpts - latency)
    -------------------------------------------------------------*/
enum {
    STRATEGY_AVOUT_NORMAL = 0,
    STRATEGY_A_ZERO_V_NOSHOW,
    STRATEGY_A_NORMAL_V_SHOW_QUICK,
    STRATEGY_A_NORMAL_V_SHOW_BLOCK,
    STRATEGY_A_NORMAL_V_NOSHOW,
    STRATEGY_A_MUTE_V_SHOW_BLOCK,
    STRATEGY_A_DROP_V_SHOW_BLOCK,
    STRATEGY_A_DROP_V_NOSHOW,
};

struct avsync_para {
    int cur_pts_diff; // pcr-apts
    int pcr_adjust_max; //pcr adjust max value
    int checkin_underrun_flag; //audio checkin underrun
    int in_out_underrun_flag; //input/output both underrun
    unsigned long underrun_checkinpts; //the checkin apts when in underrun
    int underrun_mute_time_min; //not happen loop underrun mute time ms
    int underrun_mute_time_max; //happen loop underrun mute max time ms
    int underrun_max_time;  //max time of underun to force clear
    struct timespec underrun_starttime; //input-output both underrun starttime
    struct timespec underrun_mute_starttime; //underrun mute start time
};

struct audiohal_debug_para {
    int debug_time_interval;
    unsigned int debug_last_checkin_apts;
    unsigned int debug_last_checkin_vpts;
    unsigned int debug_last_out_apts;
    unsigned int debug_last_out_vpts;
    unsigned int debug_last_demux_pcr;
    struct timespec debug_system_time;
};

extern void dtv_audio_gap_monitor(struct aml_audio_patch *patch);
extern void decoder_set_latency(unsigned int latency);
extern unsigned int decoder_get_latency(void);
extern int get_audio_discontinue(void);
extern int dtv_get_tsync_mode(void);
extern int get_dtv_pcr_sync_mode(void);
extern unsigned long decoder_apts_lookup(unsigned int offset);

void dtv_adjust_i2s_output_clock(struct aml_audio_patch* patch, int direct, int step);
void dtv_adjust_spdif_output_clock(struct aml_audio_patch* patch, int direct, int step, bool spdifb);



#endif
