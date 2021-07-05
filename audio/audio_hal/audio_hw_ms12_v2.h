/*
 * Copyright (C) 2010 Amlogic Corporation.
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

#ifndef _AUDIO_HW_MS12_H_
#define _AUDIO_HW_MS12_H_

#include <tinyalsa/asoundlib.h>
#include <system/audio.h>
#include <stdbool.h>
#include <aml_audio_ms12.h>

#include "audio_hw.h"

#define DDP_OUTPUT_SAMPLE_RATE (48000)
#define SAMPLE_NUMS_IN_ONE_BLOCK (256)
#define DDP_FRAME_DURATION(sample_nums, sample_rate) ((sample_nums) / (sample_rate))

#define MAIN_INPUT_STREAM 1
#define ASSOC_INPUT_STREAM 0
#define SYSTEM_INPUT_STREAM 0
#define RESERVED_LENGTH 31

#define AUDIO_2MAIN_MIXER_NODE           0xFF000001UL
#define AUDIO_2MAIN_MIXER_NODE_PRIMARY   0xFF000002UL
#define AUDIO_2MAIN_MIXER_NODE_SECONDARY 0xFF000003UL
#define AUDIO_2MAIN_MIXER_NODE_SYSTEM    0xFF000004UL

#define AUDIO_SYSTEM_MIXER_NODE          0xFF000010UL
#define AUDIO_AD_MIXER_NODE              0xFF000020UL


#define MIXER_PRIMARY_INPUT   0
#define MIXER_SECONDARY_INPUT 1
#define MIXER_SYSTEM_INPUT    2


/*
 *@brief get dolby ms12 prepared
 */
int get_the_dolby_ms12_prepared(
    struct aml_stream_out *aml_out
    , audio_format_t input_format
    , audio_channel_mask_t input_channel_mask
    , int input_sample_rate);

/*
 *@brief get the data of direct thread
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_main_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);


/*
 *@brief dolby ms12 system process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_system_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);

enum MS12_PCM_TYPE {
    NORMAL_LPCM = 0,
    DAP_LPCM = 1,
};

typedef struct aml_ms12_dec_info {
    int output_sr ;   /** the decoded data samplerate*/
    int output_ch ;   /** the decoded data channels*/
    int output_bitwidth; /**the decoded sample bit width*/
    int data_type;
    enum MS12_PCM_TYPE pcm_type;
    int reserved[RESERVED_LENGTH];
} aml_ms12_dec_info_t;


/*
 *@brief dolby ms12 app process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     used_size: buffer used size
 */
int dolby_ms12_app_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *used_size);

/*
 *@brief get dolby ms12 cleanup
 * input parameters
 *     set_non_continuous: disable ms12 continuous mode
 */
int get_dolby_ms12_cleanup(struct dolby_ms12_desc *ms12, bool set_non_continuous);

/*
 *@brief set dolby ms12 primary gain
 */
int set_dolby_ms12_primary_input_db_gain(struct dolby_ms12_desc *ms12,
        int db_gain, int duration);

/*
 *@brief set system app mixing status
 * if normal pcm stream status is STANDBY, set mixing off(-xs 0)
 * if normal pcm stream status is active, set mixing on(-xs 1)
 */
int set_system_app_mixing_status(struct aml_stream_out *aml_out, int stream_status);

/*
 *@brief an callback for dolby ms12 output
 */
int ms12_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

/*
 *@brief dolby ms12 open the main decoder
 */
int dolby_ms12_main_open(struct audio_stream_out *stream);

/*
 *@brief dolby ms12 close the main decoder
 */
int dolby_ms12_main_close(struct audio_stream_out *stream);

/*
 *@brief dolby ms12 flush the main related buffer
 */
int dolby_ms12_main_flush(struct audio_stream_out *stream);

/*
 *@brief dolby ms12 flush the app related buffer
 */
void dolby_ms12_app_flush();

/*
 *@brief dolby ms12 enable to debug by the property 'vendor.audio.dolbyms12.debug'
 */
void dolby_ms12_enable_debug();

/*
 *@brief check that dolby ms12 output ddp atmos(5.1.2)/ddp(5.1) is suitable or not.
 */
bool is_ms12_out_ddp_5_1_suitable(bool is_ddp_atmos);

/*
 *@brief bypass iec61937 audio data
 */
int dolby_ms12_bypass_process();

/*
 *@brief set dolby ms12 ad mixing enable
 */
void set_ms12_ad_mixing_enable(struct dolby_ms12_desc *ms12, int ad_mixing_enable);

/*
 *@brief set dolby ms12 mixing level
 */
void set_ms12_ad_mixing_level(struct dolby_ms12_desc *ms12, int mixing_level);
/*
 *@brief set dolby ms12 ad volume
 */
void set_ms12_ad_vol(struct dolby_ms12_desc *ms12, int ad_vol);

/*
 *@brief set dolby ms12 system mixing enable
 */
void set_dolby_ms12_runtime_system_mixing_enable(struct dolby_ms12_desc *ms12, int sys_mixing_enable);
bool is_ms12_passthrough(struct audio_stream_out *stream);

void set_ms12_atmos_lock(struct dolby_ms12_desc *ms12, bool is_atmos_lock_on);
void set_ms12_acmod2ch_lock(struct dolby_ms12_desc *ms12, bool is_lock_on);
bool is_ms12_continous_mode(struct aml_audio_device *adev);
void set_ms12_main_volume(struct dolby_ms12_desc *ms12, float volume);

/*
 *@brief get the platform's capability of DDP-ATMOS.
 */
bool is_platform_supported_ddp_atmos(bool atmos_supported, enum OUT_PORT current_out_port, bool is_tv);

bool is_dolby_ms12_main_stream(struct audio_stream_out *stream);
bool is_support_ms12_reset(struct audio_stream_out *stream);
bool is_bypass_dolbyms12(struct audio_stream_out *stream);
bool is_dolbyms12_dap_enable(struct aml_stream_out *aml_out);
/*
 *@brief init the ms12 hwsync module to save pts info
 */
int dolby_ms12_hwsync_init(void);

/*
 *@brief release the ms12 hwsync module
 */
int dolby_ms12_hwsync_release(void);

/*
 *@brief check in the audio offset with pts
 */
int dolby_ms12_hwsync_checkin_pts(int offset, int apts);

/*
 *@brief dolby ms12 insert one frame, it is 32ms
 */
int dolby_ms12_output_insert_oneframe(struct audio_stream_out *stream);

/*
 *@brief get how many bytes consumed by main decoder
 */
unsigned long long dolby_ms12_get_main_bytes_consumed(struct audio_stream_out *stream);
/*
 *@brief get how many pcm frames generated by main decoder
 */
unsigned long long dolby_ms12_get_main_pcm_generated(struct audio_stream_out *stream);

/*
 *@brief get ms12 continuous reset status
 */
bool is_need_reset_ms12_continuous(struct audio_stream_out *stream);

bool is_ms12_output_compatible(struct audio_stream_out *stream, audio_format_t new_sink_format, audio_format_t new_optical_format);

/*
 *@brief dynamicly set dolby ms12 drc parameters
 */
void dynamic_set_dolby_ms12_drc_parameters(struct dolby_ms12_desc *ms12);

/*
 *@brief get ms12 pipeline latency
 */
int dolby_ms12_main_pipeline_latency_frames(struct audio_stream_out *stream);

/*
 *@brief get audio postprocessing add dolby ms12 dap or not
 * return
 *   true if DAP is in MS12 pipline;
 *   false if DAP is not in MS12 pipline;
 */
bool is_audio_postprocessing_add_dolbyms12_dap(struct aml_audio_device *adev);

int bitstream_output(void *buffer, void *priv_data, size_t size);

int spdif_bitstream_output(void *buffer, void *priv_data, size_t size);

int dap_pcm_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

int stereo_pcm_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info);

int mat_bitstream_output(void *buffer, void *priv_data, size_t size);

/*
 *@brief set ms12 dap postgain
 */
void set_ms12_dap_postgain(struct dolby_ms12_desc *ms12, int postgain);

#endif //end of _AUDIO_HW_MS12_H_
