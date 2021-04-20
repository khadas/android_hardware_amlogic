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

#define LOG_TAG "aml_dec_api"

#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <cutils/log.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "aml_dec_api.h"
#include "aml_dcv_dec_api.h"
#include "aml_dca_dec_api.h"
#include "aml_pcm_dec_api.h"
#include "dolby_lib_api.h"

#define AML_DEC_FRAGMENT_FRAMES     (512)
#define AML_DEC_MAX_FRAMES          (AML_DEC_FRAGMENT_FRAMES * 4)

static aml_dec_func_t * get_decoder_function(audio_format_t format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        return &aml_dcv_func;
    }
    case AUDIO_FORMAT_DOLBY_TRUEHD:
    case AUDIO_FORMAT_MAT:
        return NULL;
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD: {
        return &aml_dca_func;
    }
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT: {
        return &aml_pcm_func;
    }
    default:
        ALOGE("[%s:%d] doesn't support decoder format:%#x", __func__, __LINE__, format);
        return NULL;
    }

    return NULL;
}

static void ddp_decoder_config_prepare(struct audio_stream_out *stream, aml_dcv_config_t * ddp_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    adev->dcvlib_bypass_enable = 0;
    /*for dual output case, we always use convet setting*/
#if 0
    switch (adev->hdmi_format) {
    case PCM:
        ddp_config->digital_raw = AML_DEC_CONTROL_DECODING;
        break;
    case DD:
        ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
        //STB case
        if (!adev->is_TV) {
            aml_out->dual_output_flag = 0;
        } else {
            aml_out->dual_output_flag = true ;
        }
        break;
    case AUTO:
    case BYPASS:
        //STB case
        if (!adev->is_TV) {
            char *cap = NULL;
            cap = (char *) get_hdmi_sink_cap(AUDIO_PARAMETER_STREAM_SUP_FORMATS, 0, &(adev->hdmi_descs));
            if (cap && mystrstr(cap, "AUDIO_FORMAT_E_AC3")) {
                ddp_config->digital_raw = AML_DEC_CONTROL_RAW;
            } else if (cap && mystrstr(cap, "AUDIO_FORMAT_AC3")) {
                if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
                    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
                } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
                    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
                } else {
                    ddp_config->digital_raw = AML_DEC_CONTROL_DECODING;
                }
            } else {
                ddp_config->digital_raw = AML_DEC_CONTROL_DECODING;
            }
            if (cap) {
                aml_audio_free(cap);
            }
        } else {
            if (adev->hdmi_descs.ddp_fmt.is_support) {
                ddp_config->digital_raw = AML_DEC_CONTROL_RAW;
            //} else if (adev->hdmi_descs.dd_fmt.is_support) {
            //    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
            } else {
                ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
            }
        }

        if (adev->patch_src == SRC_DTV) {
            if (adev->dual_spdif_support && adev->optical_format == AUDIO_FORMAT_E_AC3 && adev->dolby_decode_enable == 1) {
                ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
            }
        }

        break;
    default:
        ddp_config->digital_raw = AML_DEC_CONTROL_DECODING;
        break;
    }
    if (aml_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        ALOGI("disable raw output when a2dp device\n");
        ddp_config->digital_raw = AML_DEC_CONTROL_DECODING;
    }
#endif
    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;
    if (/*adev->dcvlib_bypass_enable != 1*/1) {
        if (adev->dual_decoder_support) {
            ddp_config->decoding_mode = DDP_DECODE_MODE_AD_DUAL;
            audio_format_t output_format = get_non_ms12_output_format(aml_out->hal_internal_format, adev);
            if (output_format != AUDIO_FORMAT_PCM_16_BIT && output_format != AUDIO_FORMAT_PCM_32_BIT) {
                ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
            }
        } else if (aml_out->ad_substream_supported) {
            ddp_config->decoding_mode = DDP_DECODE_MODE_AD_SUBSTREAM;
        } else {
            ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
        }
        if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
            ddp_config->nIsEc3 = 1;
        } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
            ddp_config->nIsEc3 = 0;
        }
        /*check if the input format is contained with 61937 format*/
        if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
            ddp_config->is_iec61937 = true;
        } else {
            ddp_config->is_iec61937 = false;
        }
    }
    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_61937:%d, IsEc3:%d"
        , __func__, ddp_config->digital_raw, aml_out->dual_output_flag, ddp_config->is_iec61937, ddp_config->nIsEc3);
    return;
}

static void dts_decoder_config_prepare(struct audio_stream_out *stream, aml_dca_config_t * dts_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    adev->dtslib_bypass_enable = 0;
#if 0
    switch (adev->hdmi_format) {
    case PCM:
        dts_config->digital_raw = AML_DEC_CONTROL_DECODING;
        break;
    case DD:
    case AUTO:
    case BYPASS:
        dts_config->digital_raw = AML_DEC_CONTROL_CONVERT;
        //STB case
        if (!adev->is_TV) {
            aml_out->dual_output_flag = 0;
        } else {
            aml_out->dual_output_flag = true;
        }
        adev->optical_format = AUDIO_FORMAT_DTS;
        break;
    default:
        dts_config->digital_raw = AML_DEC_CONTROL_DECODING;
        break;
    }
#endif
    dts_config->digital_raw = AML_DEC_CONTROL_CONVERT;
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        dts_config->is_iec61937 = true;
    } else {
        dts_config->is_iec61937 = false;
    }

    dts_config->dev = (void *)adev;
    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_iec61937:%d, is_dtscd:%d"
        , __func__, dts_config->digital_raw, aml_out->dual_output_flag, dts_config->is_iec61937, dts_config->is_dtscd);
    return;
}


static void pcm_decoder_config_prepare(struct audio_stream_out *stream, aml_pcm_config_t * pcm_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    pcm_config->channel    = aml_out->hal_ch;
    pcm_config->samplerate = aml_out->hal_rate;
    pcm_config->pcm_format = aml_out->hal_format;
    pcm_config->max_out_channels = adev->hdmi_descs.pcm_fmt.max_channels;

    return;
}


int aml_decoder_config_prepare(struct audio_stream_out *stream, audio_format_t format, aml_dec_config_t * dec_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        ddp_decoder_config_prepare(stream, &dec_config->dcv_config);
        return 0;
    }
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD: {
        dts_decoder_config_prepare(stream, &dec_config->dca_config);
        return 0;
    }
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT: {
        pcm_decoder_config_prepare(stream, &dec_config->pcm_config);
        return 0;
    }
    default:
        break;
        return 0;

    }
    return 0;
}

bool aml_decoder_output_compatible(struct audio_stream_out *stream, audio_format_t sink_format __unused, audio_format_t optical_format) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    bool is_compatible = true;

    if (aml_out->hal_internal_format != aml_out->aml_dec->format) {
        ALOGI("[%s:%d] not compatible. dec format:%#x -> cur format:%#x", __func__, __LINE__,
            aml_out->aml_dec->format, aml_out->hal_internal_format);
        return false;
    }

    if ((aml_out->aml_dec->format == AUDIO_FORMAT_AC3)
        || (aml_out->aml_dec->format == AUDIO_FORMAT_E_AC3)) {
        is_compatible = true;
    } else if ((aml_out->aml_dec->format == AUDIO_FORMAT_DTS)
                || (aml_out->aml_dec->format == AUDIO_FORMAT_DTS_HD)) {
        aml_dca_config_t* dca_config = (aml_dca_config_t *)(&aml_out->dec_config);
        if ((optical_format == AUDIO_FORMAT_PCM_16_BIT) && (dca_config->digital_raw > AML_DEC_CONTROL_DECODING)) {
            is_compatible = false;
        }
    }

    return is_compatible;
}

int aml_decoder_init(aml_dec_t **ppaml_dec, audio_format_t format, aml_dec_config_t * dec_config)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    dec_fun = get_decoder_function(format);
    aml_dec_t *aml_dec_handel = NULL;
    if (dec_fun == NULL) {
        ALOGE("%s got dec_fun as NULL!\n", __func__);
        return -1;
    }

    ALOGD("[%s:%d] dec_fun->f_init=%p, format:%#x", __func__, __LINE__, dec_fun->f_init, format);
    if (dec_fun->f_init) {
        ret = dec_fun->f_init(ppaml_dec, dec_config);
        if (ret < 0) {
            return -1;
        }
    } else {
        return -1;
    }

    aml_dec_handel = *ppaml_dec;
    aml_dec_handel->frame_cnt = 0;
    aml_dec_handel->format = format;
    aml_dec_handel->fragment_left_size = 0;
    dec_config->advol_level = 100;
    dec_config->mixer_level = 0;

    return ret;

ERROR:
    if (dec_fun->f_release && aml_dec_handel) {
        dec_fun->f_release(aml_dec_handel);
    }

    return -1;

}
int aml_decoder_release(aml_dec_t *aml_dec)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }

    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_release) {
        dec_fun->f_release(aml_dec);
    } else {
        return -1;
    }

    return ret;


}

int aml_decoder_config(aml_dec_t *aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }
    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_config) {
        ret = dec_fun->f_config(aml_dec, config_type, dec_config);
    }

    return ret;
}

int aml_decoder_info(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL\n", __func__);
        return -1;
    }
    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        return -1;
    }

    if (dec_fun->f_info) {
        ret = dec_fun->f_info(aml_dec, info_type, dec_info);
    }

    return ret;
}


int aml_decoder_process(aml_dec_t *aml_dec, unsigned char*buffer, int bytes, int *used_bytes)
{
    int ret = -1;
    aml_dec_func_t *dec_fun = NULL;
    int fill_bytes = 0;
    int parser_raw = 0;
    int offset = 0;
    int n_bytes_spdifdec_consumed = 0;
    void *payload_addr = NULL;
    int32_t n_bytes_payload = 0;
    unsigned char *spdif_src = NULL;
    int spdif_offset = 0;
    int frame_size = 0;
    int fragment_size = 0;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;

    *used_bytes = 0;
    if (aml_dec == NULL) {
        ALOGE("[%s:%d] aml_dec is null", __func__, __LINE__);
        return -1;
    }

    dec_fun = get_decoder_function(aml_dec->format);
    if (dec_fun == NULL) {
        ALOGW("[%s:%d] get_decoder_function format:%#x is null", __func__, __LINE__, aml_dec->format);
        return -1;
    }
    /*if we have fragment size output*/
    if (aml_dec->fragment_left_size > 0) {
        ALOGV("[%s:%d] fragment_left_size=%d ", __func__, __LINE__, aml_dec->fragment_left_size);
        frame_size = audio_bytes_per_sample(dec_pcm_data->data_format) * dec_pcm_data->data_ch;
        fragment_size = AML_DEC_FRAGMENT_FRAMES * frame_size;
        memmove(dec_pcm_data->buf, (unsigned char *)dec_pcm_data->buf + fragment_size, aml_dec->fragment_left_size);
        memmove(dec_raw_data->buf, (unsigned char *)dec_raw_data->buf + fragment_size, aml_dec->fragment_left_size);

        if (aml_dec->fragment_left_size >= fragment_size) {
            dec_pcm_data->data_len = fragment_size;
            dec_raw_data->data_len = fragment_size;
            aml_dec->fragment_left_size -= fragment_size;
        } else {
            dec_pcm_data->data_len = aml_dec->fragment_left_size;
            dec_raw_data->data_len = aml_dec->fragment_left_size;
            aml_dec->fragment_left_size = 0;
        }
        *used_bytes = 0;
        return 0;
    }

    dec_pcm_data->data_len = 0;
    dec_raw_data->data_len = 0;

    if (dec_fun->f_process) {
        ret = dec_fun->f_process(aml_dec, buffer, bytes);
    } else {
        ALOGE("[%s:%d] f_process is null", __func__, __LINE__);
        return -1;
    }

    frame_size = audio_bytes_per_sample(dec_pcm_data->data_format) * dec_pcm_data->data_ch;
    /*one decoded frame length is too big, we need seprate it*/
    if ((dec_pcm_data->data_len >= AML_DEC_MAX_FRAMES * frame_size) &&
        (dec_raw_data->data_format == AUDIO_FORMAT_IEC61937) &&
        (dec_raw_data->data_len == dec_pcm_data->data_len)) {
        fragment_size = AML_DEC_FRAGMENT_FRAMES * frame_size;
        aml_dec->fragment_left_size = dec_pcm_data->data_len - fragment_size;
        dec_pcm_data->data_len = fragment_size;
        dec_raw_data->data_len = fragment_size;
    }

    // todo, we should consider the used_bytes
    *used_bytes = bytes;
    return ret;
}
