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

#ifndef _AML_DCA_DEC_API_H_
#define _AML_DCA_DEC_API_H_

#include <hardware/audio.h>
#include "aml_ringbuffer.h"
#include "aml_audio_types_def.h"
#include "aml_dec_api.h"

struct dts_syncword_info {
    unsigned int syncword;
    int syncword_pos;
    int check_pos;
};

struct dts_frame_info {
    unsigned int syncword;
    int syncword_pos;
    int check_pos;
    bool is_little_endian;
    int iec61937_data_type;
    int size;
};

///< Keep the members of dca_config_type_e the same as structure in dca_decoder.h
///< static param: need to reset decoder
///< dynamic param: do not neet to reset decoder
typedef enum  {
    DCA_CONFIG_OUT_BITDEPTH, //runtime/static param
    DCA_CONFIG_OUT_CH, //runtime/static param
    DCA_CONFIG_OUTPUT_SR,   // static param
} dca_config_type_e;

///< Keep the members of dca_info_type_e the same as structure in dca_decoder.h
typedef enum  {
    DCA_OUTPUT_INFO,
    DCA_STREAM_INFO,
} dca_info_type_e;

///< Keep the members of dca_config_t the same as structure in dca_decoder.h
typedef union dca_config_s {
    uint32_t output_bitwidth;
    uint32_t output_ch; ///< 0(default): decoder auto config, 2: always 2ch, 6: always 6ch
    uint32_t output_sr;
} dca_config_t;

///< Keep the members of dca_info_t the same as structure in dca_decoder.h
typedef union dca_info_s {
    struct {
        uint32_t output_bitwidth; ///< output channels
        uint32_t output_ch; ///< output channels
        uint32_t output_sr; ///< output sample rate
    } output_info;  ///< DCA_GET_OUTPUT_INFO

    struct {
        uint32_t stream_ch; ///< bitstream origin channels
        uint32_t stream_sr; ///< bitstream origin sample rate
        uint32_t stream_type; ///< bitstream origin stream type
    } stream_info;  ///< DCA_GET_STREAM_INFO
} dca_info_t;

struct dca_dts_dec {
    ///< Control
    aml_dec_t  aml_dec;
    //int (*get_parameters) (void *, int *, int *, int *);
    int (*decoder_process)(unsigned char*, int, unsigned char *, int *, unsigned char *, int *, struct pcm_info *);

    ///< Information
    int status;
    int remain_size;
    int outlen_pcm;
    int outlen_raw;
    int stream_type;    ///< enum audio_hal_format
    bool is_headphone_x;
    struct pcm_info pcm_out_info;
    struct dts_frame_info frame_info;   ///< for frame parsing

    ///< Parameter
    bool is_dtscd;
    bool is_iec61937;
    unsigned char *inbuf;
    aml_dec_control_type_t digital_raw;
    ring_buffer_t input_ring_buf;
};

int dca_decoder_init_patch(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config);
int dca_decoder_release_patch(aml_dec_t *aml_dec);
int dca_decoder_process_patch(aml_dec_t *aml_dec, unsigned char*buffer, int bytes);
/*
    If @aml_dec is NULL, static param will take effect after decoder init.
    If @aml_dec is normal, static/runtime param will take effect after decoder init.
*/
int dca_decoder_config(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t *aml_dec_config);
int dca_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t *aml_dec_info);

///< internal api, for VirtualX.

/**
* @brief Get dca decoder output channel(internal use).
* @param None
* @return [success]: 0 decoder not init.
*         [success]: 1 ~ 8, output channel number
*            [fail]: -1 get output channel fail.
*/
int dca_get_out_ch_internal(void);
/**
* @brief Set dca decoder output channel(internal use).
* @param ch_num: The num of channels you want the decoder to output
*              0: Default setting, decoder configs output channel automatically.
*          1 ~ 8: The decoder outputs the specified number of channels
*                (At present, @ch_num only supports 2-ch and 6-ch).
* @return [success]: 0
*            [fail]: -1 set output channel fail.
*/
int dca_set_out_ch_internal(int ch_num);

extern aml_dec_func_t aml_dca_func;

#endif
