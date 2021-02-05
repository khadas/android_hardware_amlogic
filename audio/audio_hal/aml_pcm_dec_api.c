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
#include "audio_hw.h"
#include "aml_dec_api.h"
#include "aml_audio_stream.h"
#include "aml_malloc_debug.h"

#define PCM_MAX_LENGTH (8192*2*2)

struct pcm_dec_t {
    aml_dec_t  aml_dec;
    aml_pcm_config_t pcm_config;
};


static int pcm_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct pcm_dec_t *pcm_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;
    dec_data_info_t * dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("PCM config is NULL\n");
        return -1;
    }
    pcm_config = (aml_pcm_config_t *)dec_config;

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
    ALOGI("PCM format=%d samplerate =%d ch=%d\n", pcm_config->pcm_format,
          pcm_config->samplerate, pcm_config->channel);

    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_pcm_data->buf_size = PCM_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        return -1;
    }

    aml_dec->status = 1;
    *ppaml_dec = (aml_dec_t *)pcm_dec;
    ALOGE("%s success", __func__);
    return 0;

exit:
    if (pcm_dec) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
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

    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        aml_audio_free(aml_dec);
    }
    return 0;
}

static int pcm_decoder_process(aml_dec_t * aml_dec, unsigned char*buffer, int bytes)
{
    struct pcm_dec_t *pcm_dec = NULL;
    aml_pcm_config_t *pcm_config = NULL;

    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return -1;
    }

    pcm_dec = (struct pcm_dec_t *)aml_dec;
    pcm_config = &pcm_dec->pcm_config;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;

    if (dec_pcm_data->buf_size < bytes) {
        ALOGI("realloc outbuf_max_len  from %zu to %zu\n", dec_pcm_data->buf_size, bytes);
        dec_pcm_data->buf = aml_audio_realloc(dec_pcm_data->buf, bytes);
        if (dec_pcm_data->buf == NULL) {
            ALOGE("realloc pcm buffer failed size %zu\n", bytes);
            return -1;
        }
        dec_pcm_data->buf_size = bytes;
        memset(dec_pcm_data->buf, 0, bytes);
    }

    /*now we only support bypass PCM data*/
    memcpy(dec_pcm_data->buf, buffer, bytes);

    dec_pcm_data->data_len = bytes / (audio_bytes_per_sample(pcm_config->pcm_format) * pcm_config->channel);
    dec_pcm_data->data_sr  = pcm_config->samplerate;
    dec_pcm_data->data_ch  = pcm_config->channel;
    dec_pcm_data->data_format  = pcm_config->pcm_format;

    return 0;
}



aml_dec_func_t aml_pcm_func = {
    .f_init                 = pcm_decoder_init,
    .f_release              = pcm_decoder_release,
    .f_process              = pcm_decoder_process,
    .f_config               = NULL,
    .f_info                 = NULL,
};
