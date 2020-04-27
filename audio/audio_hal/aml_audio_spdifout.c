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

#define LOG_TAG "audio_spdif_out"
//#define LOG_NDEBUG 0

#include <stdlib.h>
#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>

#include "audio_hw.h"
#include "alsa_device_parser.h"
#include "aml_audio_spdifout.h"
#include "audio_hw_dtv.h"
#include "spdif_encoder_api.h"
#include "audio_hw_utils.h"
#include "alsa_manager.h"


typedef struct spdifout_handle {
    int device_id; /*used for refer aml_dev->alsa_handle*/
    int spdif_port;
    audio_format_t audio_format;
    bool need_spdif_enc;
    bool spdif_enc_init;
    void *spdif_enc_handle;


} spdifout_handle_t;


int aml_audio_get_spdif_port(eMixerSpdif_Format spdif_format)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int pcm_index = -1;
    int spdif_port = PORT_SPDIF;

    /*
     * there are 3 cases:
       1. Soc deosn't support dual spdif, and its spdif port name is PORT_SPDIF
       2. Soc supports dual spdif, but it doesn't have spdif interface
       3. Soc supports dual spdif, and it has spdif interface
          spdif_a can be connected to spdif & hdmi, its name is PORT_SPDIF
          spdif_b only can be conncted to hdmi, its name is PORT_SPDIFB
     */
    if (aml_dev->dual_spdif_support) {
        /*it means we have spdif_a & spdif_b & spdif out interface*/
        if ((spdif_format == AML_DOLBY_DIGITAL) ||
            (spdif_format == AML_DTS)) {
            /*these data can be transfer to spdif*/
            spdif_port = PORT_SPDIF;
        } else {
            spdif_port = PORT_SPDIFB;
        }
    } else {
        /*we try to get the right spdif pcm alsa port*/
        pcm_index = alsa_device_update_pcm_index(PORT_SPDIF, PLAYBACK);
        if (pcm_index != -1) {
            /*we have spdif*/
            spdif_port = PORT_SPDIF;
        } else {
            spdif_port = -1;
        }
    }
    return spdif_port;
}


int aml_audio_get_spdifa_port(void)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    int pcm_index = -1;
    int spdif_port = PORT_SPDIF;

    pcm_index = alsa_device_update_pcm_index(PORT_SPDIF, PLAYBACK);
    if (pcm_index != -1) {
        /*we have spdif*/
        spdif_port = PORT_SPDIF;
    } else {
        spdif_port = -1;
    }

    return spdif_port;
}


void aml_audio_set_spdif_format(int spdif_port, eMixerSpdif_Format aml_spdif_format, struct aml_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int spdif_format_ctr_id = AML_MIXER_ID_SPDIF_FORMAT;
    int spdif_to_hdmi_select = AML_SPDIF_A_TO_HDMITX;

    if ((spdif_port != PORT_SPDIF) && (spdif_port != PORT_SPDIFB)) {
        return;
    }

    if (spdif_port == PORT_SPDIF) {
        spdif_format_ctr_id = AML_MIXER_ID_SPDIF_FORMAT;
    } else if (spdif_port == PORT_SPDIFB) {
        spdif_format_ctr_id = AML_MIXER_ID_SPDIF_B_FORMAT;
    }

    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, spdif_format_ctr_id, aml_spdif_format);

    /*use same source for normal pcm case*/
    if (aml_spdif_format == AML_STEREO_PCM) {
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_TO_HDMI,  AML_SPDIF_A_TO_HDMITX);
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, aml_spdif_format);
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, aml_spdif_format);
    }

    ALOGI("%s tinymix AML_MIXER_ID_SPDIF_FORMAT %d\n",
          __FUNCTION__, aml_spdif_format);
    return;
}


void aml_audio_select_spdif_to_hdmi(int spdif_select)
{
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    if ((spdif_select != AML_SPDIF_A_TO_HDMITX) && (spdif_select != AML_SPDIF_B_TO_HDMITX)) {
        return;
    }
    aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_TO_HDMI,  spdif_select);

    return;
}

static int spdifout_support_format(audio_format_t audio_format)
{
    switch (audio_format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
    case AUDIO_FORMAT_IEC61937:
        return true;
    default:
        return false;
    }
}

int aml_audio_spdifout_open(void **pphandle, audio_format_t audio_format)
{
    int ret = -1;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void *alsa_handle = NULL;
    struct spdifout_handle *phandle = NULL;
    int device_id = 0;
    int aml_spdif_format = AML_STEREO_PCM;

    if (!spdifout_support_format(audio_format)) {
        ALOGE("%s format not support =0x%x", __FUNCTION__, audio_format);
        return -1;
    }

    phandle = (struct spdifout_handle *) calloc(1, sizeof(struct spdifout_handle));
    if (phandle == NULL) {
        ALOGE("%s malloc failed\n", __FUNCTION__);
        goto error;
    }

    if (audio_format != AUDIO_FORMAT_IEC61937) {
        phandle->need_spdif_enc = 1;
    }
    phandle->audio_format = audio_format;

    if (!phandle->spdif_enc_init) {
        ret = aml_spdif_encoder_open(&phandle->spdif_enc_handle, phandle->audio_format);
        if (ret) {
            ALOGE("%s() aml_spdif_encoder_open failed", __func__);
            goto error;
        }
        phandle->spdif_enc_init = true;
    }

    /*here is some tricky code, we assume below config for dual output*/
    if (aml_dev->dual_spdif_support) {
        if (audio_format == AUDIO_FORMAT_AC3 ||
            audio_format == AUDIO_FORMAT_DTS) {
            device_id = DIGITAL_DEVICE;
        } else {
            device_id = DIGITAL_DEVICE2;
        }
    } else {
        device_id = DIGITAL_DEVICE;
    }

    alsa_handle = aml_dev->alsa_handle[device_id];

    if (!alsa_handle) {
        aml_stream_config_t stream_config;
        aml_device_config_t device_config;
        memset(&stream_config, 0, sizeof(aml_stream_config_t));
        memset(&device_config, 0, sizeof(aml_device_config_t));
        /*config stream info*/
        stream_config.config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        stream_config.config.sample_rate  = MM_FULL_POWER_SAMPLING_RATE;
        stream_config.config.format       = AUDIO_FORMAT_IEC61937;
        stream_config.config.offload_info.format = audio_format;

        device_config.device_port = alsa_device_get_port_index(device_id);
        phandle->spdif_port       = device_config.device_port;

        aml_spdif_format = halformat_convert_to_spdif(audio_format);

        /*set spdif format*/
        if (phandle->spdif_port == PORT_SPDIF) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, aml_spdif_format);
        } else if (phandle->spdif_port == PORT_SPDIFB) {
            aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, aml_spdif_format);
        }

        /*open output alsa*/
        ret = aml_alsa_output_open_new(&alsa_handle, &stream_config, &device_config);

        if (ret != 0) {
            goto error;
        }
        aml_dev->alsa_handle[device_id] = alsa_handle;
        ALOGI("dev alsa handle device id=%d handle=%p", device_id, alsa_handle);

    }

    phandle->device_id = device_id;

    *pphandle = (void *)phandle;
    ALOGI("%s success ret=%d", __func__, ret);
    return ret;

error:
    if (phandle) {
        if (phandle->spdif_enc_handle) {
            aml_spdif_encoder_close(phandle->spdif_enc_handle);
        }
        free(phandle);
    }
    *pphandle = NULL;
    return -1;


}

int aml_audio_spdifout_processs(void *phandle, void *buffer, size_t byte)
{
    int ret = -1;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void * output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int device_id = -1;

    void *alsa_handle = NULL;
    if (phandle == NULL) {
        return -1;
    }

    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];

    if (spdifout_phandle->need_spdif_enc) {
        ret = aml_spdif_encoder_process(spdifout_phandle->spdif_enc_handle, buffer, byte, &output_buffer, &output_buffer_bytes);
        if (ret != 0) {
            ALOGE("%s: spdif encoder process error", __func__);
            return ret;
        }
    } else {
        output_buffer = buffer;
        output_buffer_bytes = byte;
    }

#if 0
    {
        output_info_t output_info = { 0 };
        aml_alsa_output_getinfo(alsa_handle, OUTPUT_INFO_DELAYFRAME, &output_info);
        ALOGI("delay frame =%d\n", output_info.delay_frame);

    }
#endif

    ret = aml_alsa_output_write_new(alsa_handle, output_buffer, output_buffer_bytes);


    return ret;
}


int aml_audio_spdifout_close(void *phandle)
{
    int ret = -1;
    int device_id = -1;
    struct spdifout_handle *spdifout_phandle = (struct spdifout_handle *)phandle;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *)adev_get_handle();
    void *alsa_handle = NULL;
    if (phandle == NULL) {
        return -1;
    }

    device_id = spdifout_phandle->device_id;
    alsa_handle = aml_dev->alsa_handle[device_id];


    if (alsa_handle) {
        ALOGI("%s close dual output bitstream id=%d handle %p", __func__, device_id, alsa_handle);
        aml_alsa_output_close_new(alsa_handle);
        aml_dev->alsa_handle[device_id] = NULL;
    }

    /*it is spdif a output*/
    if (spdifout_phandle->spdif_port == PORT_SPDIF) {
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_STEREO_PCM);

    } else if (spdifout_phandle->spdif_port == PORT_SPDIFB) {
        /*it is spdif b output*/
        aml_mixer_ctrl_set_int(&aml_dev->alsa_mixer, AML_MIXER_ID_SPDIF_B_FORMAT, AML_STEREO_PCM);
    }


    if (spdifout_phandle) {
        if (spdifout_phandle->spdif_enc_handle) {
            ret = aml_spdif_encoder_close(spdifout_phandle->spdif_enc_handle);
        }
        free(spdifout_phandle);
    }
    return ret;
}
