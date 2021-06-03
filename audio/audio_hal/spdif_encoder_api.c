/*
 * Copyright (C) 2011 The Android Open Source Project
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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>

#include "SPDIFEncoderAD.h"
#include "audio_hw.h"

#define IEC61937_PACKET_SIZE_OF_AC3     0x1800
#define IEC61937_PACKET_SIZE_OF_EAC3    0x6000
#define IEC61937_PACKET_SIZE_MAT       (61440)

struct aml_spdif_encoder {
    void *spdif_encoder_ad;
    audio_format_t format;
    void *temp_buf;
    int temp_buf_size;
    int temp_buf_pos;
    bool bmute;
};

static bool spdif_encoder_support_format(audio_format_t audio_format) {
    switch (audio_format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
    case AUDIO_FORMAT_MAT:
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        return true;
    default:
        return false;
    }
}

/*
 *@brief get the spdif encoder output buffer
 */
static int config_spdif_encoder_output_buffer(struct aml_spdif_encoder *spdif_enc, audio_format_t format)
{
    /*
     *audiofinger write 4096bytes to hal
     *ac3 min frame size is 64bytes, so ac3 coefficient is 4096/64 = 64
     *eac3 min frame size is 128bytes, so ac3 coefficient is 4096/128 = 32
     */
    int ac3_coef = 64;
    int eac3_coef = 32;
    if (format == AUDIO_FORMAT_AC3) {
        spdif_enc->temp_buf_size = IEC61937_PACKET_SIZE_OF_AC3 * ac3_coef;    //0x1800bytes is 6block of ac3
    } else if (format == AUDIO_FORMAT_E_AC3) {
        spdif_enc->temp_buf_size = IEC61937_PACKET_SIZE_OF_EAC3 * eac3_coef;    //0x6000bytes is 6block of eac3
    } else if (format == AUDIO_FORMAT_MAT) {
        spdif_enc->temp_buf_size = IEC61937_PACKET_SIZE_MAT * 2;
    } else {
        spdif_enc->temp_buf_size = IEC61937_PACKET_SIZE_MAT * 2;
    }
    spdif_enc->temp_buf_pos = 0;
    spdif_enc->temp_buf = (void *)aml_audio_malloc(spdif_enc->temp_buf_size);
    ALOGV("-%s() temp_buf_size %x\n", __FUNCTION__, spdif_enc->temp_buf_size);
    if (spdif_enc->temp_buf == NULL) {
        ALOGE("-%s() malloc fail", __FUNCTION__);
        return -1;
    }
    memset(spdif_enc->temp_buf, 0, spdif_enc->temp_buf_size);

    return 0;
}


int aml_spdif_encoder_open(void **spdifenc_handle, audio_format_t format)
{
    int ret = 0;
    struct aml_spdif_encoder *phandle = NULL;
    if (!spdif_encoder_support_format(format)) {
        ALOGE("%s format not support =0x%x", __FUNCTION__, format);
        return -1;
    }

    phandle = (struct aml_spdif_encoder *) aml_audio_calloc(1, sizeof(struct aml_spdif_encoder));
    if (phandle == NULL) {
        ALOGE("%s malloc failed\n", __FUNCTION__);
        *spdifenc_handle = NULL;
        return -1;
    }

    ret = config_spdif_encoder_output_buffer(phandle, format);
    if (ret != 0) {
        ALOGE("-%s() config_spdif_encoder_output_buffer fail", __FUNCTION__);
        goto error;
    }
    ret = spdif_encoder_ad_init(
              &phandle->spdif_encoder_ad
              , format
              , (const void *)phandle->temp_buf
              , phandle->temp_buf_size);
    if (ret != 0) {
        ALOGE("-%s() spdif_encoder_ad_init fail", __FUNCTION__);
        goto error;
    }

    phandle->format = format;
    phandle->bmute  = 0;
    *spdifenc_handle = (void *)phandle;
    ALOGI("%s handle =%p", __FUNCTION__, phandle);
    return 0;

error:
    if (phandle) {
        if (phandle->temp_buf) {
            aml_audio_free(phandle->temp_buf);
        }
        aml_audio_free(phandle);
    }
    *spdifenc_handle = NULL;
    return -1;
}

int aml_spdif_encoder_close(void *phandle)
{
    struct aml_spdif_encoder *spdifenc_handle = (struct aml_spdif_encoder *)phandle;
    if (spdifenc_handle) {
        ALOGI("%s handle=%p", __FUNCTION__, phandle);
        spdif_encoder_ad_deinit(spdifenc_handle->spdif_encoder_ad);
        if (spdifenc_handle->temp_buf) {
            aml_audio_free(spdifenc_handle->temp_buf);
        }
        aml_audio_free(phandle);
        phandle = NULL;
    }
    return 0;
}

int aml_spdif_encoder_process(void *phandle, const void *buffer, size_t numBytes, void **output_buf, size_t *out_size)
{
    struct aml_spdif_encoder *spdifenc_handle = (struct aml_spdif_encoder *)phandle;
    if (phandle == NULL) {
        *output_buf = NULL;
        *out_size   = 0;
        return -1;
    }

    spdif_encoder_ad_write(spdifenc_handle->spdif_encoder_ad, buffer, numBytes);

    spdifenc_handle->temp_buf_pos = spdif_encoder_ad_get_current_position(spdifenc_handle->spdif_encoder_ad);
    if (spdifenc_handle->temp_buf_pos <= 0) {
        spdifenc_handle->temp_buf_pos = 0;
    }
    spdif_encoder_ad_flush_output_current_position(spdifenc_handle->spdif_encoder_ad);

    if (spdifenc_handle->bmute) {
        /*why we just memset it? because it is not always one frame*/
        if (spdifenc_handle->temp_buf_pos > 0) {
            memset(spdifenc_handle->temp_buf, 0, spdifenc_handle->temp_buf_pos);
        }
    }
    *output_buf = spdifenc_handle->temp_buf;
    *out_size   = spdifenc_handle->temp_buf_pos;

    ALOGV("spdif enc format=0x%x size =0x%zx", spdifenc_handle->format, *out_size);
    return 0;
}

int aml_spdif_encoder_mute(void *phandle, bool bmute) {
    struct aml_spdif_encoder *spdifenc_handle = (struct aml_spdif_encoder *)phandle;
    if (phandle == NULL) {
        ALOGE("[%s:%d] invalid param, phandle is null", __func__, __LINE__);
        return -1;
    }

    spdifenc_handle->bmute  = bmute;
    return 0;
}

