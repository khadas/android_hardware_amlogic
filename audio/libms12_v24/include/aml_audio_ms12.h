/*
 * hardware/amlogictv/t962x3/audio/TvAudio/aml_audio_ms12.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 */
#ifndef __AML_AUDIO_MS12_H__
#define __AML_AUDIO_MS12_H__


#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <system/audio.h>
#include <cutils/list.h>
#include "dolby_ms12.h"
#include "dolby_ms12_config_params.h"
#include "dolby_ms12_status.h"

#include "aml_ringbuffer.h"



#define DOLBY_SAMPLE_SIZE 4//2ch x 2bytes(16bits) = 4 bytes

enum {
    BITSTREAM_OUTPUT_A,
    BITSTREAM_OUTPUT_B,
    BITSTREAM_OUTPUT_CNT
};

struct bitstream_out_desc {
    audio_format_t audio_format;
    void *spdifout_handle;
    int  need_drop_frame;
    bool is_bypass_ms12;
};

struct dolby_ms12_desc {
    bool dolby_ms12_enable;
    bool dolby_ms12_init_flags;
    audio_format_t input_config_format;
    audio_channel_mask_t config_channel_mask;
    int config_sample_rate;
    int output_config;
    int output_samplerate;
    audio_channel_mask_t output_channelmask;
    int ms12_out_bytes;
    //audio_policy_forced_cfg_t force_use;
    int dolby_ms12_init_argc;
    char **dolby_ms12_init_argv;
    void *dolby_ms12_ptr;
#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK

#else
    char *dolby_ms12_out_data;
#endif
    int dolby_ms12_out_max_size;
    /*
    there are some risk when aux write thread and direct thread
    access the ms12 module at the same time.
    1) aux thread is writing. direct thread is on standby and clear up the ms12 module.
    2) aux thread is writing. direct thread is preparing the ms12 module.
    */
    pthread_mutex_t lock;
    /*
    for higher effiency we dot use the the lock for main write
    function,as ms clear up may called by binder  thread
    we need protect the risk situation
    */
    pthread_mutex_t main_lock;
    pthread_t dolby_ms12_threadID;
    bool dolby_ms12_thread_exit;
    bool is_continuous_paused;
    int device;//alsa_device_t
    struct timespec timestamp;
    uint64_t last_frames_postion;
    uint64_t last_ms12_pcm_out_position;
    bool     ms12_position_update;
    /*
    latency frame is maintained by the whole device output.
    whatever what bistream is outputed we need use this latency frames.
    */
    int latency_frame;
    int sys_avail;
    int curDBGain;

    // for DDP stream, the input frame is 768/1537/1792(each 32ms)
    // May change through playback.
    // here to caculate average frame size;
    int avgDdpFramesize;
    // the input signal atmos info
    int is_dolby_atmos;
    int input_total_ms;
    int bitsteam_cnt;
    void * system_virtual_buf_handle;
    ring_buffer_t spdif_ring_buffer;
    unsigned char *lpcm_temp_buffer;

    /*
     *-ac4_de             * <int> [ac4] Dialogue Enhancement gain that will be applied in the decoder
     *                      Range: 0 to 12 dB (in 1 dB steps, default is 0 dB)
     */
    int ac4_de;
    int nbytes_of_dmx_output_pcm_frame;
    void * ac3_parser_handle;
    int hdmi_format;
    audio_format_t  optical_format;
    audio_format_t sink_format;
    struct timespec  sys_audio_timestamp;
    uint64_t  sys_audio_frame_pos;
    uint64_t  sys_audio_base_pos;
    uint64_t  sys_audio_skip;
    uint64_t  last_sys_audio_cost_pos;
    /*ms12 main input information */
    audio_format_t main_input_fmt;
    unsigned int   main_input_sr;
    void * ms12_bypass_handle;
    bool   is_bypass_ms12;
    int    atmos_info_change_cnt;
    int need_resume;
    int need_resync; /*handle from pause to resume sync*/
    bool dual_bitstream_support;
    struct bitstream_out_desc bitstream_out[BITSTREAM_OUTPUT_CNT];
    void * spdif_dec_handle;
    bool dual_decoder_support;
    uint64_t main_input_start_offset_ns;
    uint64_t main_input_ns;
    uint64_t main_output_ns;
    uint32_t main_input_rate;  /*it is used to calculate the buffer duration*/
    uint32_t main_buffer_min_level;
    uint32_t main_buffer_max_level;
    int   dap_bypass_enable;
    float dap_bypassgain;
    /*
     * these variables are used for ms12 message thread.
     */
    pthread_t ms12_mesg_threadID;
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    bool CommThread_ExitFlag;
    struct listnode mesg_list;
    struct aml_stream_out *ms12_main_stream_out;
    struct aml_stream_out *ms12_app_stream_out; /*Reserve for extension*/
    uint64_t dap_pcm_frames;
    uint64_t stereo_pcm_frames;
    uint64_t master_pcm_frames;
    uint64_t ms12_main_input_size;
    bool     b_legacy_ddpout;
    void *   iec61937_ddp_buf;
    float    main_volume;
};

/*
 *@brief this function is get the ms12 suitalbe output format
 *       1.input format
 *       2.EDID pcm/dd/dd+
 *       3.system settting
 * TODO, get the suitable format
 */
audio_format_t get_dolby_ms12_suitable_output_format(void);

/*
 *@brief get the ms12 output details, samplerate/formate/channelnum
 */
int get_dolby_ms12_output_details(struct dolby_ms12_desc *ms12_desc);

/*
 *@brief init the dolby ms12
 */
int get_dolby_ms12_init(struct dolby_ms12_desc *ms12_desc, char *dolby_ms12_path);

/*
 *@brief get the dolby ms12 config parameters
 * ms12_desc: ms12 handle
 * ms12_config_format: AUDIO_FORMAT_PCM_16_BIT/AUDIO_FORMAT_PCM_32_BIT/AUDIO_FORMAT_AC3/AUDIO_FORMAT_E_AC3/AUDIO_FORMAT_MAT
 * config_channel_mask: AUDIO_CHANNEL_OUT_STEREO/AUDIO_CHANNEL_OUT_5POINT1/AUDIO_CHANNEL_OUT_7POINT1
 * config_sample_rate: sample rate.
 * output_config: bit mask | of {MS12_OUTPUT_MASK_DD/DDP/MAT/STEREO/SPEAKER}
 */
int aml_ms12_config(struct dolby_ms12_desc *ms12_desc
                    , audio_format_t config_format
                    , audio_channel_mask_t config_channel_mask
                    , int config_sample_rate
                    , int output_config
                    , char *dolby_ms12_path);
/*
 *@brief cleanup the dolby ms12
 */
int aml_ms12_cleanup(struct dolby_ms12_desc *ms12_desc);

int aml_ms12_update_runtime_params(struct dolby_ms12_desc *ms12_desc, char *cmd);

int aml_ms12_update_runtime_params_direct(struct dolby_ms12_desc *ms12_desc
                    , int argc
                    , char **argv);

#if 0
int aml_ms12_update_runtime_params_lite(struct dolby_ms12_desc *ms12_desc);
#endif

int aml_ms12_lib_preload(char *dolby_ms12_path);

int aml_ms12_lib_release();


#endif //end of __AML_AUDIO_MS12_H__
