/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#ifndef _AML_DCV_DEC_API_H_
#define _AML_DCV_DEC_API_H_

#include <hardware/audio.h>
#include "aml_ringbuffer.h"
#include "aml_audio_types_def.h"
#include "aml_dec_api.h"

#define MAX_BUFF_LEN 36
typedef enum  {
    DDP_CONFIG_MIXER_LEVEL, //runtime param
    DDP_CONFIG_OUT_BITDEPTH, //static param
    DDP_CONFIG_OUT_CH, //static param
    DDP_CONFIG_AD_PCMSCALE, //runtime param
    DDP_CONFIG_MAIN_PCMSCALE,//runtime param
} ddp_config_type_t;

typedef enum  {
    DDP_DECODE_MODE_SINGLE = 1,
    DDP_DECODE_MODE_AD_DUAL = 2,
    DDP_DECODE_MODE_AD_SUBSTREAM = 3,
} ddp_decoding_mode_t;

typedef union ddp_config {
    int  mixer_level;
} ddp_config_t;

struct dolby_ddp_dec {
    aml_dec_t  aml_dec;
    unsigned char *inbuf;
    size_t dcv_pcm_writed;
    size_t dcv_decoded_samples;
    int dcv_decoded_errcount;
    char sysfs_buf[MAX_BUFF_LEN];
    int status;
    int inbuf_size;
    int remain_size;
    int outlen_pcm;
    int outlen_raw;
    int nIsEc3;
    aml_dec_control_type_t digital_raw;
    int decoding_mode;
    int  mixer_level;
    bool is_iec61937;
    int curFrmSize;
    int (*get_parameters)(void *, int *, int *, int *,int *,int *);
    int (*decoder_process)(unsigned char*, int, unsigned char *, int *, char *, int *, int, struct pcm_info *);
    struct pcm_info pcm_out_info;
    //struct resample_para aml_resample;
    //unsigned char *resample_outbuf;
    //ring_buffer_t output_ring_buf;
    void *spdifout_handle;
    int ad_substream_supported;
    int mainvol_level;
    int advol_level;
    int is_dolby_atmos;
    unsigned int sourcesr;
    unsigned int sourcechnum;
};

int dcv_decoder_init_patch(aml_dec_t ** ppaml_dec, aml_dec_config_t * dec_config);
int dcv_decoder_release_patch(aml_dec_t * aml_dec);
int dcv_decoder_process_patch(aml_dec_t * aml_dec, unsigned char*buffer, int bytes);
int dcv_decoder_get_framesize(unsigned char*buffer, int bytes, int* p_head_offset);
int is_ad_substream_supported(unsigned char *buffer,int bytes);
int dcv_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config);


extern aml_dec_func_t aml_dcv_func;

#endif
