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


#define LOG_TAG "audio_hw_hdmi"
//#define LOG_NDEBUG 0

/*
 *This table shows the meaning of bytes 1 to 3 of the Short Audio Descriptor.
 *|-----------------------------------------------------------------------------------------|
 *|            |                                  Bits                                      |
 *|    Byte    |----------------------------------------------------------------------------|
 *|    #       |   7     |    6   |    5   |    4   |    3   |    2    |    1    |    0     |
 *|-----------------------------------------------------------------------------------------|
 *|     1      | F17 = 0 |         Audio format code         |Maximum number of channels – 1|
 *|-----------------------------------------------------------------------------------------|
 *|     2      | F27 = 0 | 192kHz |176.4kHz| 96kHz  | 88.2kHz|   48kHz | 44.1kHz |  32kHz   |
 *|-----------------------------------------------------------------------------------------|
 *|     3      |                       Audio format code–dependent value                    |
 *|-----------------------------------------------------------------------------------------|
 */

/*
 * The audio format code values (bits [6:3] of byte 1) for Dolby technologies are:
 * 0x2 = Dolby Digital
 * 0xA = Dolby Digital Plus
 * 0xC = Dolby TrueHD and Dolby MAT
 */
#define SAD_SIZE 3 /* SAD only has 3bytes */
#define DOLBY_DIGITAL               0x2
#define DOLBY_DIGITAL_PLUS          0xA
#define DOLBY_TRUEHD_AND_DOLBY_MAT  0xC
#define AUDIO_FORMAT_CODE_BYTE1_BIT3 3

#include <errno.h>
#include <cutils/log.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "audio_hdmi_util.h"
#include "aml_alsa_mixer.h"
#include "aml_audio_stream.h"

struct audio_format_code_list {
    AML_HDMI_FORMAT_E  id;
    char audio_format_code_name[32];
};

static struct audio_format_code_list gAudioFormatList[] = {
    {AML_HDMI_FORMAT_RESERVED1, "AML_HDMI_FORMAT_RESERVED1"},
    {AML_HDMI_FORMAT_LPCM, "AUDIO_FORMAT_LPCM"},
    {AML_HDMI_FORMAT_AC3, "AUDIO_FORMAT_AC3"},
    {AML_HDMI_FORMAT_MPEG1, "AUDIO_FORMAT_MPEG1"},
    {AML_HDMI_FORMAT_MP3, "AUDIO_FORMAT_MP3"},
    {AML_HDMI_FORMAT_MPEG2MC, "AUDIO_FORMAT_MPEG2MC"},
    {AML_HDMI_FORMAT_AAC, "AUDIO_FORMAT_AAC"},
    {AML_HDMI_FORMAT_DTS, "AUDIO_FORMAT_DTS"},
    {AML_HDMI_FORMAT_ATRAC, "AUDIO_FORMAT_ATRAC"},
    {AML_HDMI_FORMAT_OBA, "AUDIO_FORMAT_OBA"},
    {AML_HDMI_FORMAT_DDP, "AUDIO_FORMAT_DDP"},
    {AML_HDMI_FORMAT_DTSHD, "AUDIO_FORMAT_DTSHD"},
    {AML_HDMI_FORMAT_MAT, "AUDIO_FORMAT_MAT"},
    {AML_HDMI_FORMAT_DST, "AUDIO_FORMAT_DST"},
    {AML_HDMI_FORMAT_WMAPRO, "AUDIO_FORMAT_WMAPRO"},
    {AML_HDMI_FORMAT_RESERVED2, "AUDIO_FORMAT_RESERVED2"},
};

/* default_edid = 1, restore default edid
 * default_edid = 0, update AVR ARC capability to edid.
 */
void update_edid(struct aml_audio_device *adev, bool default_edid, void *edid_array, int edid_length)
{
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    int suitable_edid_len = edid_length;
    ALOGD("%s() edid_length %d default_edid %d will %s\n", __func__, edid_length ,default_edid
        , default_edid ? "restore default edid" : "update AVR ARC capability to edid");

    char *EDID_audio_array = edid_array;
    for (int n = 0; n < edid_length + TLV_HEADER_SIZE ; n++) {
        ALOGV("%s line %d SAD_array(%d) [%#x]\n", __func__, __LINE__, n, EDID_audio_array[n]);
    }

    if (default_edid == true) {
        aml_mixer_ctrl_set_array(&adev->alsa_mixer, AML_MIXER_ID_HDMIIN_AUDIO_EDID,
            edid_array, TLV_HEADER_SIZE);
    } else {
        suitable_edid_len = (edid_length <= (EDID_ARRAY_MAX_LEN - TLV_HEADER_SIZE)) ? (edid_length) : (EDID_ARRAY_MAX_LEN - TLV_HEADER_SIZE);
        aml_mixer_ctrl_set_array(&adev->alsa_mixer, AML_MIXER_ID_HDMIIN_AUDIO_EDID,
            edid_array, (suitable_edid_len + TLV_HEADER_SIZE));
    }
    hdmi_desc->default_edid = default_edid;
}

/*
 *|-----------------------------------------------------------------------------------------------------------|
 *|            |              Bits [1:0] of Dolby Digital Plus Short Audio Descriptor byte 3                  |
 *|   Meaning  |----------------------------------------------------------------------------------------------|
 *|            |                Bit1                           |                      Bit0                    |
 *|-----------------------------------------------------------------------------------------------------------|
 *|            | Dolby Atmos decoding and rendering of joint   | Dolby Atmos decoding and rendering of joint  |
 *| If bit = 0 | object coding content from Dolby Digital Plus | object coding content is not supported.      |
 *|            | acmod 28 is not supported.                    |                                              |
 *|-----------------------------------------------------------------------------------------------------------|
 *|            | Dolby Atmos decoding and rendering of joint   | Dolby Atmos decoding and rendering of joint  |
 *| If bit = 1 | object coding content from Dolby Digital Plus | object coding content is supported.          |
 *|            | acmod 28 is supported.                        |                                              |
 *|-----------------------------------------------------------------------------------------------------------|
 */
int update_dolby_atmos_decoding_and_rendering_cap_for_ddp_sad(
    void *array
    , int count
    , bool is_acmod_28_supported
    , bool is_joc_supported)
{
    char *ddp_sad = NULL;
    int bit_for_acmod_28 = 1; // bytes3 bit1
    int bit_for_joc = 0; //bytes3 bit0
    int ret = -1;
    if (!array || (count < SAD_SIZE)) {
        ALOGE("%s line %d array %p count %d\n", __func__, __LINE__, array, count);
        return -1;
    }

    ddp_sad = (char *)array;
    //ALOGV("%s line %d ddp sad [%#x %#x %#x]\n", __func__, __LINE__, ddp_sad[0], ddp_sad[1], ddp_sad[2]);

    if (DOLBY_DIGITAL_PLUS == ((ddp_sad[0] >> AUDIO_FORMAT_CODE_BYTE1_BIT3)&0xF)) {
        if (is_acmod_28_supported) {
            ddp_sad[2] |= (0x1<<bit_for_acmod_28);
        }
        if (is_joc_supported) {
            ddp_sad[2] |= (0x1<<bit_for_joc);
        }
        ret = 0;
    }
    else
        ret = -1;

    //ALOGV("%s line %d ddp sad [%#x %#x %#x]\n", __func__, __LINE__, ddp_sad[0], ddp_sad[1], ddp_sad[2]);
    return ret;
}

/*
 *--------------------------------------------------------------------------------------------
 *|            |     Bits [1:0] of Dolby MAT and Dolby TrueHD Short Audio Descriptor byte 3  |
 *|    Meaning |-----------------------------------------------------------------------------|
 *|            |                Bit1                  |              Bit0                    |
 *|------------------------------------------------------------------------------------------|
 *|If bit = 0  |EMDF hash calculation required by     |Only Dolby TrueHD decoding is         |
 *|            |the Dolby MAT decoder for PCM input.  |supported.                            |
 *|------------------------------------------------------------------------------------------|
 *|If bit = 1  |EMDF hash calculation not required    |Dolby Atmos decoding and rendering of |
 *|            |by theDolby MAT decoder for PCM input.|Dolby TrueHD and PCM are supported.   |
 *--------------------------------------------------------------------------------------------
 *Note: If bit 0 of byte 3 is set to 0, then bits 1 through 7 of byte 3 are also set to 0.
 */
int update_dolby_MAT_decoding_cap_for_dolby_MAT_and_dolby_TRUEHD_sad(
    void *array
    , int count
    , bool is_mat_pcm_supported
    , bool is_truehd_supported)
{
    char *mat_sad = NULL;
    int bit_for_mat_pcm = 1; // bytes3 bit1
    int bit_for_truehd = 0; //bytes3 bit0
    int ret = -1;
    if (!array || (count < SAD_SIZE)) {
        ALOGE("%s line %d array %p count %d\n", __func__, __LINE__, array, count);
        return -1;
    }

    mat_sad = (char *)array;
    //ALOGV("%s line %d mat sad [%#x %#x %#x]\n", __func__, __LINE__, mat_sad[0], mat_sad[1], mat_sad[2]);
    if (DOLBY_TRUEHD_AND_DOLBY_MAT == ((mat_sad[0] >> AUDIO_FORMAT_CODE_BYTE1_BIT3)&0xF)) {
        if (is_mat_pcm_supported) {
            mat_sad[2] |= (0x1 << bit_for_mat_pcm);
        }
        if (is_truehd_supported) {
            mat_sad[2] |= (0x1 << bit_for_truehd);
        }
        ret = 0;
    }
    else
        ret = -1;

    //ALOGV("%s line %d mat sad [%#x %#x %#x]\n", __func__, __LINE__, mat_sad[0], mat_sad[1], mat_sad[2]);
    return ret;
}


int get_current_edid(struct aml_audio_device *adev, char *edid_array, int edid_array_len)
{
    char EDID_audio_array[EDID_ARRAY_MAX_LEN] = {0};
    int n = 0;
    int ret = 0;

    if (!adev || !edid_array || (edid_array_len <= 0)) {
        ALOGD("%s line %d adev %p edid_array %p ret %d\n", __func__, __LINE__, adev, edid_array, edid_array_len);
        return -1;
    }

    ret = aml_mixer_ctrl_get_array(&adev->alsa_mixer, AML_MIXER_ID_HDMIIN_AUDIO_EDID,
        EDID_audio_array, EDID_ARRAY_MAX_LEN);

    ALOGV("%s line %d mixer %s ret %d\n", __func__, __LINE__, "HDMIIN AUDIO EDID", ret);

    if (ret == 0) {
        /* got the SAD_array with first TLV_HEADER_SIZE all zero */
        memmove(EDID_audio_array, EDID_audio_array + TLV_HEADER_SIZE, EDID_ARRAY_MAX_LEN - TLV_HEADER_SIZE);
    }

    for (n = 0; n < EDID_ARRAY_MAX_LEN; n++) {
        ALOGV("%s line %d EDID_cur_array(%d) [%#x]\n",  __func__, __LINE__, n, EDID_audio_array[n]);
    }

    if (edid_array_len >= EDID_ARRAY_MAX_LEN)
        memcpy(edid_array, EDID_audio_array, EDID_ARRAY_MAX_LEN);
    else {
        ALOGE("%s line %d edid_array_len %d is less than %d, something is lost!\n",
            __func__, __LINE__, edid_array_len, EDID_ARRAY_MAX_LEN);
        memcpy(edid_array, EDID_audio_array, edid_array_len);
    }

    return 0;
}

int set_arc_hdmi(struct audio_hw_device *dev, char *value, size_t len)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    char *pt = NULL, *tmp = NULL;
    int i = 0;
    unsigned int *ptr = (unsigned int *)(&hdmi_desc->target_EDID_array[0]);

    if (strlen (value) > len) {
        ALOGE ("value array overflow!");
        return -EINVAL;
    }

    memset(hdmi_desc->target_EDID_array, 0, EDID_ARRAY_MAX_LEN);

    pt = strtok_r (value, "[], ", &tmp);
    while (pt != NULL) {

        if (i == 0) //index 0 means avr cec length
            hdmi_desc->EDID_length = atoi (pt);
        else if (i == 1) //index 1 means avr port
            hdmi_desc->avr_port = atoi (pt);
        else
            hdmi_desc->target_EDID_array[TLV_HEADER_SIZE + i - 2] = atoi (pt);

        pt = strtok_r (NULL, "[], ", &tmp);
        i++;
    }

    ptr[0] = 0;
    ptr[1] = (unsigned int)hdmi_desc->EDID_length;

    if (hdmi_desc->EDID_length == 0) {
        ALOGI("ARC is disconnect!, Reset to default EDID.");
        adev->arc_hdmi_updated = 0;
        update_edid(adev, true, (void *)&hdmi_desc->target_EDID_array[0], hdmi_desc->EDID_length);
    } else {
        ALOGI("ARC is connected, EDID_length = [%d], ARC HDMI AVR port = [%d]",
            hdmi_desc->EDID_length, hdmi_desc->avr_port);
        /*for (i = 0; i < hdmi_desc->EDID_length/3; i++) {
            ALOGI ("EDID SAD in hex [%x, %x, %x]",
            hdmi_desc->target_EDID_array[TLV_HEADER_SIZE + 3*i],
            hdmi_desc->target_EDID_array[TLV_HEADER_SIZE + 3*i + 1],
            hdmi_desc->target_EDID_array[TLV_HEADER_SIZE + 3*i + 2]);
        }*/
        adev->arc_hdmi_updated = 1;
    }

    return 0;
}


static char *get_audio_format_code_name_by_id(int fmt_id)
{
    int i;
    int cnt_mixer = sizeof(gAudioFormatList) / sizeof(struct audio_format_code_list);

    for (i = 0; i < cnt_mixer; i++) {
        if (gAudioFormatList[i].id == fmt_id) {
            return gAudioFormatList[i].audio_format_code_name;
        }
    }

    return NULL;
}

int update_edid_after_edited_audio_sad(struct aml_audio_device *adev, struct format_desc *fmt_desc)
{
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    if (fmt_desc) {
        ALOGD("Update [%s] support:%d, ch:%d, sample_mask:%#x, bit_rate:%d, atmos:%d",
            hdmiFormat2Str(fmt_desc->fmt), fmt_desc->is_support, fmt_desc->max_channels,
            fmt_desc->sample_rate_mask, fmt_desc->max_bit_rate, fmt_desc->atmos_supported);
    }

    if (BYPASS == adev->hdmi_format) {
        /* update the AVR's EDID */
        update_edid(adev, false, (void *)&hdmi_desc->target_EDID_array[0], hdmi_desc->EDID_length);
    } else if (AUTO == adev->hdmi_format) {
        if (!fmt_desc->is_support) {
            //if DDP is not in EDID, should update AVR's EDID
            update_edid(adev, false, (void *)&hdmi_desc->target_EDID_array[0], hdmi_desc->EDID_length);
        } else {
            /* get the default EDID audio array */
            char EDID_cur_array[EDID_ARRAY_MAX_LEN] = {0};
            int available_edid_len = 0;
            bool is_mat_pcm_supported = fmt_desc->atmos_supported;
            bool is_truehd_supported = fmt_desc->atmos_supported;

            memcpy(EDID_cur_array, adev->default_EDID_array, EDID_ARRAY_MAX_LEN);

            /* edit the current EDID audio array to add DDP-SAD(byte3-bit0~1) and MAT-SAD(byte3-bit0~1)*/
            for (int n = 0; n < EDID_ARRAY_MAX_LEN / SAD_SIZE; n++) {
                update_dolby_atmos_decoding_and_rendering_cap_for_ddp_sad(
                    (void *)(EDID_cur_array  + SAD_SIZE*n)
                    , (EDID_ARRAY_MAX_LEN - SAD_SIZE * n)
                    , fmt_desc->atmos_supported
                    , fmt_desc->atmos_supported);
                update_dolby_MAT_decoding_cap_for_dolby_MAT_and_dolby_TRUEHD_sad(
                    (void *)(EDID_cur_array  + SAD_SIZE*n)
                    , (EDID_ARRAY_MAX_LEN - SAD_SIZE * n)
                    , is_mat_pcm_supported
                    , is_truehd_supported);

                /* From the SAD table, one invalid SAD is like this [0, 0, 0], here filter the valid SAD */
                if (EDID_cur_array[SAD_SIZE*n]) {
                    available_edid_len += SAD_SIZE;
                }
            }
            /* Skip PCM SAD */
            memmove((EDID_cur_array + TLV_HEADER_SIZE - SAD_SIZE) , EDID_cur_array, available_edid_len);
            available_edid_len -= SAD_SIZE;

            unsigned int *ptr = (unsigned int *)EDID_cur_array;
            ptr[0] = 0;
            ptr[1] = available_edid_len;

            for (int cnt = 0; cnt < EDID_ARRAY_MAX_LEN; cnt++) {
                ALOGV("%s line %d EDID_cur_array(%d) [%#x]\n",  __func__, __LINE__, cnt, EDID_cur_array[cnt]);
            }

            /* update the EDID after editing*/
            update_edid(adev, false, (void *)EDID_cur_array, available_edid_len);
        }
    } else if (hdmi_desc->default_edid == false) {
        /* Reset the audio default EDID */
        update_edid(adev, true, (void *)&hdmi_desc->target_EDID_array[0], hdmi_desc->EDID_length);
    }
    return 0;
}

int set_arc_format(struct audio_hw_device *dev, char *value, size_t len)
{
    struct aml_audio_device *adev = (struct aml_audio_device *) dev;
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    struct format_desc *fmt_desc = NULL;
    char *pt = NULL, *tmp = NULL;
    int i = 0, val = 0;
    AML_HDMI_FORMAT_E format = AML_HDMI_FORMAT_LPCM;
    bool is_dolby_sad = false;
    bool is_dts_sad = false;

    if (strlen (value) > len) {
        ALOGW("[%s:%d] value array len:%d overflow!", __func__, __LINE__, strlen(value));
        return -EINVAL;
    }

    /*
     * ex: adev_set_parameters with (const char *kvpairs = "set_ARC_format=[10, 1, 7, 6, 3]")
     * after the progress: str_parms_get_str with (const char *key = "set_ARC_format")
     * get the (char *value = [10, 1, 7, 6, 3])
     * it means [AML_HDMI_FORMAT_DDP, is_support(1), max_channels(7+1), sample_rate_mask(6), atmos_supported(0x3&0x1)]
     * so, here to analysis the Audio EDID from CEC.
     */
    pt = strtok_r (value, "[], ", &tmp);
    while (pt != NULL) {
        val = atoi (pt);
        switch (i) {
        /* the first step, strtok_r got the "[", found the value of fmt_desc->fmt */
        case 0:
            format = val;
            if (val == AML_HDMI_FORMAT_AC3) {
                fmt_desc = &hdmi_desc->dd_fmt;
            } else if (val == AML_HDMI_FORMAT_DDP) {
                fmt_desc = &hdmi_desc->ddp_fmt;
            } else if (val == AML_HDMI_FORMAT_MAT) {
                fmt_desc = &hdmi_desc->mat_fmt;
            } else if (val == AML_HDMI_FORMAT_LPCM) {
                fmt_desc = &hdmi_desc->pcm_fmt;
            } else if (val == AML_HDMI_FORMAT_DTS) {
                fmt_desc = &hdmi_desc->dts_fmt;
            } else if (val == AML_HDMI_FORMAT_DTSHD) {
                fmt_desc = &hdmi_desc->dtshd_fmt;
            } else {
                ALOGW("[%s:%d] unsupport fmt:%d", __func__, __LINE__, val);
                return -EINVAL;
            }
            fmt_desc->fmt = val;
            break;
        /* the second step, strtok_r got the ",", found the value of fmt_desc->is_support */
        case 1:
            fmt_desc->is_support = val;
            break;
        /* the three step, strtok_r got the ",", found the value of fmt_desc->max_channels */
        case 2:
            fmt_desc->max_channels = val + 1;
            break;
        /* the four step, strtok_r got the ",", found the value of fmt_desc->sample_rate_mask */
        case 3:
            fmt_desc->sample_rate_mask = val;
            break;
        /* the five step, strtok_r got the ",", found the value of fmt_desc->atmos_supported */
        case 4:
            if (format == AML_HDMI_FORMAT_DDP) {
                /* byte 3, bit 0 is atmos bit*/
                fmt_desc->atmos_supported = (val & 0x1) > 0 ? true : false;

                /* when arc is connected update AVR SAD to hdmi edid */
                update_edid_after_edited_audio_sad(adev, fmt_desc);
                /*
                 * if the ARC capbility format is changed, it should not support HBR(MAT/DTS-HD)
                 * for the sequence is LPCM -> DD -> DTS -> DDP -> DTSHD,
                 * which is defined in "private void setAudioFormat()" at file:DroidLogicEarcService.java
                 * so, here we choose the DDP part to update the sink format.
                 */
                update_sink_format_after_hotplug(adev);
            } else {
                fmt_desc->max_bit_rate = val * 80;
            }
            break;
        default:
            break;
        }

        pt = strtok_r (NULL, "[], ", &tmp);
        i++;
    }

    memcpy(&adev->hdmi_arc_capability_desc, hdmi_desc, sizeof(struct aml_arc_hdmi_desc));
    if (fmt_desc) {
        ALOGI("----[%s] support:%d, ch:%d, sample_mask:%#x, bit_rate:%d, atmos:%d",
            hdmiFormat2Str(fmt_desc->fmt),fmt_desc->is_support, fmt_desc->max_channels,
            fmt_desc->sample_rate_mask, fmt_desc->max_bit_rate, fmt_desc->atmos_supported);
    }

    return 0;
}

