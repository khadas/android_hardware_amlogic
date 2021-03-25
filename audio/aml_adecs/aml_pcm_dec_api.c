/*
 * Copyright (C) 2020 Amlogic Corporation.
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

#define LOG_TAG "aml_audio_pcm_dec"

#include <cutils/log.h>
#include "aml_dec_api.h"
#include "aml_malloc_debug.h"

#define PCM_MAX_LENGTH (8192*2*2)

struct pcm_dec_t {
    aml_dec_t  aml_dec;
    aml_pcm_config_t pcm_config;
};

static inline short CLIP16(int r)
{
    return (r >  0x7fff) ? 0x7fff :
           (r < -0x8000) ? -0x8000 :
           r;
}

static void downmix_6ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 6;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5     + (int)src[channel*i + 2] * 0.25 + (int)src[channel*i + 3] * 0.25 + (int)src[channel*i + 5] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5 + (int)src[channel*i + 2] * 0.25 + (int)src[channel*i + 4] * 0.25 + (int)src[channel*i + 5] * 0.25);
        }
    }
    return;
}

static void downmix_8ch_to_2ch(void *in_buf, void *out_buf, int bytes, int audio_format) {
    int frames_num = 0;
    int channel = 8;
    int i = 0;
    if (audio_format == AUDIO_FORMAT_PCM_16_BIT) {
        frames_num = bytes / (channel * 2);
        int16_t *src = (int16_t *)in_buf;
        int16_t *dst = (int16_t *)out_buf;
        for (i = 0; i < frames_num; i++) {
            dst[2*i]   = (int16_t)CLIP16((int)src[channel*i] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 4] * 0.25
                       + (int)src[channel*i + 6] * 0.25) ;
            dst[2*i+1] = (int16_t)CLIP16((int)src[channel*i + 1] * 0.5
                       + (int)src[channel*i + 2] * 0.25
                       + (int)src[channel*i + 3] * 0.25
                       + (int)src[channel*i + 5] * 0.25
                       + (int)src[channel*i + 7] * 0.25);
        }
    }
    return;
}


static int pcm_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct pcm_dec_t *pcm_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;

    if (dec_config == NULL) {
        ALOGE("PCM config is NULL\n");
        return -1;
    }
    pcm_config = &dec_config->pcm_config;

    if (pcm_config->channel <= 0 || pcm_config->channel > 8) {
        ALOGE("PCM config channel is invalid=%d\n", pcm_config->channel);
        return -1;
    }

    if (pcm_config->samplerate <= 0 || pcm_config->samplerate > 192000) {
        ALOGE("PCM config samplerate is invalid=%d\n", pcm_config->samplerate);
        return -1;
    }

    if (!audio_is_linear_pcm(pcm_config->pcm_format)) {
        ALOGE("PCM config format is not supported =%d\n", pcm_config->pcm_format);
        return -1;
    }

    pcm_dec = aml_audio_calloc(1, sizeof(struct pcm_dec_t));
    if (pcm_dec == NULL) {
        ALOGE("malloc ddp_dec failed\n");
        return -1;
    }

    aml_dec = &pcm_dec->aml_dec;
    memcpy(&pcm_dec->pcm_config, pcm_config, sizeof(aml_pcm_config_t));

    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_pcm_data->buf_size = PCM_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }

    raw_in_data = &aml_dec->raw_in_data;
    raw_in_data->buf_size = PCM_MAX_LENGTH;
    raw_in_data->buf = (unsigned char*) aml_audio_calloc(1, raw_in_data->buf_size);
    if (!raw_in_data->buf) {
        ALOGE("malloc buffer failed\n");
        return -1;
    }


    aml_dec->status = 1;
    *ppaml_dec = (aml_dec_t *)pcm_dec;
    ALOGI("[%s:%d] success PCM format=%d, samplerate:%d, ch:%d", __func__, __LINE__,
        pcm_config->pcm_format, pcm_config->samplerate, pcm_config->channel);
    return 0;

exit:
    if (pcm_dec) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
        }
        aml_audio_free(pcm_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;
}

static int pcm_decoder_release(aml_dec_t * aml_dec)
{
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * raw_in_data = NULL;

    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        raw_in_data = &aml_dec->raw_in_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
        }

        aml_audio_free(aml_dec);
    }
    return 0;
}

static int pcm_decoder_process(aml_dec_t * aml_dec, unsigned char*buffer, int bytes)
{
    struct pcm_dec_t *pcm_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;
    int src_channel = 0;
    int dst_channel = 0;
    int downmix_conf = 1;
    int downmix_size = 0;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    if (bytes <= 0) {
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    pcm_dec = (struct pcm_dec_t *)aml_dec;
    pcm_config = &pcm_dec->pcm_config;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * raw_in_data = &aml_dec->raw_in_data;

    dec_pcm_data->data_len = 0;
    raw_in_data->data_len = 0;


    src_channel = pcm_config->channel;
    dst_channel = 2;
    downmix_conf = src_channel / dst_channel;
    downmix_size = bytes / downmix_conf;

    if (dec_pcm_data->buf_size < downmix_size) {
        ALOGI("realloc outbuf_max_len  from %zu to %zu\n", dec_pcm_data->buf_size, downmix_size);
        dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, downmix_size);
        if (dec_pcm_data->buf == NULL) {
            ALOGE("realloc pcm buffer failed size %zu\n", downmix_size);
            return AML_DEC_RETURN_TYPE_FAIL;
        }
        dec_pcm_data->buf_size = downmix_size;
        memset(dec_pcm_data->buf, 0, downmix_size);
    }


    if (pcm_config->channel == 2) {
        /*now we only support bypass PCM data*/
        memcpy(dec_pcm_data->buf, buffer, bytes);
    } else if (pcm_config->channel == 6) {
        downmix_6ch_to_2ch(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
    } else if (pcm_config->channel == 8) {
        downmix_8ch_to_2ch(buffer, dec_pcm_data->buf, bytes, pcm_config->pcm_format);
    }else {
        ALOGI("unsupport channel =%d", pcm_config->channel);
        return AML_DEC_RETURN_TYPE_OK;
    }


    dec_pcm_data->data_len = downmix_size;
    dec_pcm_data->data_sr  = pcm_config->samplerate;
    dec_pcm_data->data_ch  = 2;
    dec_pcm_data->data_format  = pcm_config->pcm_format;
    ALOGV("%s data_in=%d ch =%d out=%d ch=%d", __func__, bytes, pcm_config->channel, downmix_size, 2);

    if (pcm_config->max_out_channels >= pcm_config->channel) {
        if (raw_in_data->buf_size < bytes) {
            ALOGI("realloc outbuf_max_len  from %zu to %zu\n", raw_in_data->buf_size, bytes);
            raw_in_data->buf = aml_audio_realloc(raw_in_data->buf, bytes);
            if (raw_in_data->buf == NULL) {
                ALOGE("realloc pcm buffer failed size %zu\n", bytes);
                return AML_DEC_RETURN_TYPE_FAIL;
            }
            raw_in_data->buf_size = bytes;
            memset(raw_in_data->buf, 0, bytes);
        }
        memcpy(raw_in_data->buf, buffer, bytes);
        raw_in_data->data_len = bytes ;
        raw_in_data->data_sr  = pcm_config->samplerate;
        raw_in_data->data_ch  = pcm_config->channel;
        raw_in_data->data_format  = pcm_config->pcm_format;
        ALOGV("%s multi data_in=%d ch =%d out=%d ch=%d", __func__, bytes, pcm_config->channel, downmix_size, pcm_config->channel);
    }
    return bytes;
}



aml_dec_func_t aml_pcm_func = {
    .f_init                 = pcm_decoder_init,
    .f_release              = pcm_decoder_release,
    .f_process              = pcm_decoder_process,
    .f_config               = NULL,
    .f_info                 = NULL,
};
