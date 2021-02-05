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
#include "aml_audio_resampler.h"
#include "aml_audio_parser.h"

typedef enum  {
    AML_DEC_CONFIG_MIXER_LEVEL, //runtime param
} aml_dec_config_type_t;

typedef enum  {
    AML_DEC_REMAIN_SIZE, //runtime param
    AML_DEC_STREMAM_INFO,
} aml_dec_info_type_t;


typedef struct aml_dec_stream_info {
    int stream_sr;    /** the sample rate in stream*/
    int stream_ch;    /** the original channels in stream*/
    int output_bLFE;

} aml_dec_stream_info_t;

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
    dec_data_info_t dec_raw_data;
    int status;
    int frame_cnt;
} aml_dec_t;

typedef struct aml_dcv_config {
    audio_format_t format;
    int digital_raw;
    bool is_iec61937;
    int decoding_mode;
    int nIsEc3;
} aml_dcv_config_t;

typedef struct aml_dca_config {
    audio_format_t format;
    int digital_raw;
    bool is_dtscd;
    bool is_iec61937;
} aml_dca_config_t;

typedef struct aml_pcm_config {
    audio_format_t pcm_format;
    int samplerate;
    int channel;
} aml_pcm_config_t;

typedef union aml_dec_config {
    /*config for decoder init*/
    aml_dcv_config_t dcv_config;
    aml_dca_config_t dca_config;
    aml_pcm_config_t pcm_config;

    /*config for runtime*/
    int  mixer_level;   /* AML_DEC_CONFIG_MIXER_LEVEL */
} aml_dec_config_t;

typedef union aml_dec_info {
    int remain_size;                         /* AML_DEC_REMAIN_SIZE */
    aml_dec_stream_info_t dec_info;          /* AML_DEC_STREMAM_INFO*/
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

int aml_decoder_config_prepare(struct audio_stream_out *stream, audio_format_t format, aml_dec_config_t * dec_config);
int aml_decoder_init(aml_dec_t **aml_dec, audio_format_t format, aml_dec_config_t * dec_config);
int aml_decoder_release(aml_dec_t *aml_dec);
int aml_decoder_config(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config);
int aml_decoder_info(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info);
int aml_decoder_process(aml_dec_t *aml_dec, unsigned char*buffer, int bytes, int * used_bytes);
bool aml_decoder_output_compatible(struct audio_stream_out *stream, audio_format_t sink_format, audio_format_t optical_format);

#endif
