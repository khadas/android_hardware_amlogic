/*
 * Copyright (C) 2021 Amlogic Corporation.
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

#ifndef _AML_DEC_API_H_
#define _AML_DEC_API_H_

#include <hardware/audio.h>
#include <system/audio.h>
#include "aml_ringbuffer.h"
#include "aml_volume_utils.h"
#include "aml_malloc_debug.h"

#define ACODEC_FMT_NULL -1
#define ACODEC_FMT_MPEG 0
#define ACODEC_FMT_PCM_S16LE 1
#define ACODEC_FMT_AAC 2
#define ACODEC_FMT_AC3 3
#define ACODEC_FMT_ALAW 4
#define ACODEC_FMT_MULAW 5
#define ACODEC_FMT_DTS 6
#define ACODEC_FMT_PCM_S16BE 7
#define ACODEC_FMT_FLAC 8
#define ACODEC_FMT_COOK 9
#define ACODEC_FMT_PCM_U8 10
#define ACODEC_FMT_ADPCM 11
#define ACODEC_FMT_AMR 12
#define ACODEC_FMT_RAAC 13
#define ACODEC_FMT_WMA 14
#define ACODEC_FMT_WMAPRO 15
#define ACODEC_FMT_PCM_BLURAY 16
#define ACODEC_FMT_ALAC 17
#define ACODEC_FMT_VORBIS 18
#define ACODEC_FMT_AAC_LATM 19
#define ACODEC_FMT_APE 20
#define ACODEC_FMT_EAC3 21
#define ACODEC_FMT_WIFIDISPLAY 22
#define ACODEC_FMT_DRA 23
#define ACODEC_FMT_TRUEHD 25
#define ACODEC_FMT_MPEG1                                                       \
  26 // AFORMAT_MPEG-->mp3,AFORMAT_MPEG1-->mp1,AFROMAT_MPEG2-->mp2
#define ACODEC_FMT_MPEG2 27
#define ACODEC_FMT_WMAVOI 28
#define ACODEC_FMT_AC4    29


typedef enum {
    AML_DEC_CONFIG_MIXING_ENABLE,
    AML_DEC_CONFIG_AD_VOL,
    AML_DEC_CONFIG_MIXER_LEVEL, //runtime param
    AML_DEC_CONFIG_OUTPUT_CHANNEL,  //runtime/static param
} aml_dec_config_type_t;

typedef enum {
    AML_DEC_REMAIN_SIZE, //runtime param
    AML_DEC_STREMAM_INFO,
    AML_DEC_OUTPUT_INFO,
} aml_dec_info_type_t;

typedef enum {
    AML_DEC_CONTROL_DECODING            = 0,
    AML_DEC_CONTROL_CONVERT             = 1,
    AML_DEC_CONTROL_RAW                 = 2,
} aml_dec_control_type_t;

typedef enum {
    AML_DEC_RETURN_TYPE_FAIL            = -1,
    AML_DEC_RETURN_TYPE_OK              = 0,
    AML_DEC_RETURN_TYPE_CACHE_DATA      = -2,    /* Not enough decoded data. */
    AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN  = -3,    /* Cache a lot of data, needs to be decoded multiple times. */
} aml_dec_return_type_t;

typedef struct aml_dec_stream_info {
    int stream_sr;    /** the sample rate in stream*/
    int stream_ch;    /** the original channels in stream*/
    int output_bLFE;

} aml_dec_stream_info_t;

typedef struct aml_dec_output_info {
    int output_sr;
    int output_ch;
    int output_bitwidth;

} aml_dec_output_info_t;

typedef struct dec_data_info {
    audio_format_t data_format;
    audio_format_t sub_format;
    unsigned char *buf;
    int buf_size;
    int data_len;
    int data_ch;
    int data_sr;
    //int data_bitwidth;
} dec_data_info_t;

typedef struct aml_dec {
    audio_format_t format;
    dec_data_info_t dec_pcm_data;
    dec_data_info_t ad_dec_pcm_data;
    dec_data_info_t dec_raw_data;
    dec_data_info_t raw_in_data;
    char *ad_data;
    int ad_size;
    int64_t in_frame_pts;
    int64_t out_frame_pts;
    int status;
    int frame_cnt;
    int fragment_left_size;
    void *dev;
} aml_dec_t;

typedef struct aml_dcv_config {
    audio_format_t format;
    aml_dec_control_type_t digital_raw;
    bool is_iec61937;
    int decoding_mode;
    int nIsEc3;
} aml_dcv_config_t;

typedef struct aml_dca_config {
    audio_format_t format;
    aml_dec_control_type_t digital_raw;
    bool is_dtscd;
    bool is_iec61937;
    int output_ch;
    void *dev;
} aml_dca_config_t;

typedef struct aml_pcm_config {
    audio_format_t pcm_format;
    int samplerate;
    int channel;
    int max_out_channels;
} aml_pcm_config_t;


typedef struct aml_faad_config {
    audio_format_t aac_format;
    int samplerate;
    int channel;
} aml_faad_config_t;

typedef struct aml_mad_config {
    audio_format_t mpeg_format;
    int samplerate;
    int channel;
} aml_mad_config_t;

typedef struct aml_dec_config {
    /*config for decoder init*/
    aml_dcv_config_t dcv_config;
    aml_dca_config_t dca_config;
    aml_mad_config_t mad_config;
    aml_faad_config_t faad_config;
    aml_pcm_config_t pcm_config;

    /*config for runtime*/
    bool ad_decoder_supported;
    bool ad_mixing_enable;
    int advol_level;
    int  mixer_level;   /* AML_DEC_CONFIG_MIXER_LEVEL */
} aml_dec_config_t;


typedef union aml_dec_info {
    int remain_size;                         /* AML_DEC_REMAIN_SIZE */
    aml_dec_stream_info_t dec_info;          /* AML_DEC_STREMAM_INFO*/
    aml_dec_output_info_t dec_output_info;   /* AML_DEC_OUTPUT_INFO */
} aml_dec_info_t;



typedef int (*F_Init)(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config);
typedef int (*F_Release)(aml_dec_t *aml_dec);
typedef int (*F_Process)(aml_dec_t *aml_dec, unsigned char* buffer, int bytes);
typedef int (*F_Config)(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config);
typedef int (*F_Info)(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info);

typedef struct aml_dec_func {
    F_Init                  f_init;
    F_Release               f_release;
    F_Process               f_process;
    F_Config                f_config;
    F_Info                  f_info;
} aml_dec_func_t;

int aml_decoder_init(aml_dec_t **aml_dec, audio_format_t format, aml_dec_config_t * dec_config);
int aml_decoder_release(aml_dec_t *aml_dec);
int aml_decoder_info(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info);
int aml_decoder_process(aml_dec_t *aml_dec, unsigned char*buffer, int bytes, int * used_bytes);
int aml_decoder_set_config(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config);



#endif
