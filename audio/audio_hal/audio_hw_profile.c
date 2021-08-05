/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#define LOG_TAG "audio_hw_profile"
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <aml_android_utils.h>

#include "audio_hw_utils.h"
#include "alsa_device_parser.h"
#include "dolby_lib_api.h"

#define SOUND_CARDS_PATH "/proc/asound/cards"
#define SOUND_PCM_PATH  "/proc/asound/pcm"
#define MAT_EDID_MAX_LEN 256

#define SUPPORT_MAX_CHANNEL_2CH     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO"

#define SUPPORT_MAX_CHANNEL_3CH     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK"

#define SUPPORT_MAX_CHANNEL_4CH     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND"

#define SUPPORT_MAX_CHANNEL_5CH     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA"


#define SUPPORT_MAX_CHANNEL_6CH     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1"

#define SUPPORT_MAX_CHANNEL_7CH \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_6POINT1"


#define SUPPORT_MAX_CHANNEL_8CH \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_6POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"


#define AC3_SUPPORT_CHANNEL     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1"

#define EAC3_SUPPORT_CHANNEL    \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define EAC3_JOC_SUPPORT_CHANNEL \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define DOLBY_TRUEHD_SUPPORT_CHANNEL \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"


#define AC4_SUPPORT_CHANNEL     \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_TRI_BACK|\
AUDIO_CHANNEL_OUT_3POINT1|\
AUDIO_CHANNEL_OUT_QUAD|\
AUDIO_CHANNEL_OUT_SURROUND|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define DTS_SUPPORT_CHANNEL      \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_2POINT1|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_QUAD_BACK|\
AUDIO_CHANNEL_OUT_QUAD_SIDE|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_6POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define DTSHD_SUPPORT_CHANNEL    \
"AUDIO_CHANNEL_OUT_MONO|\
AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_2POINT1|\
AUDIO_CHANNEL_OUT_TRI|\
AUDIO_CHANNEL_OUT_QUAD_BACK|\
AUDIO_CHANNEL_OUT_QUAD_SIDE|\
AUDIO_CHANNEL_OUT_PENTA|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_6POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define IEC61937_SUPPORT_CHANNEL  \
"AUDIO_CHANNEL_OUT_STEREO|\
AUDIO_CHANNEL_OUT_5POINT1|\
AUDIO_CHANNEL_OUT_7POINT1"

#define AUIDO_PROFILE_ITEM_NUM               (32)
#define AUDIO_PROFILE_SAMPLERATE_NUM         (10)
#define AUDIO_PROFILE_MAXCH_STRING           " %d ch"
#define AUDIO_PROFILE_DEPVALUE_STRING        " DepVaule %x"

/* Vendor Specific Audio Data Block Begin */
/* Dolby Vendor-Specific Audio Data Block */
/* defined in DolbyAudioandDolbyAtmosOverHDMI.pdf Version 1.3 */

/*
Table 11: This table describes the allocation of bytes 1 to 7 of the Dolby VSADB for sink devices.
|----------------------------------------------------------------------------------------------|
|Byte |                                   Bits                                                 |
|     |----------------------------------------------------------------------------------------|
|  #  |    7     |    6     |    5     |    4     |    3     |    2     |    1     |    0      |
|----------------------------------------------------------------------------------------------|
|  1  |         Tag code(0x7)          |           Length of following data block              |
|     |                                |     (in bytes, in including this byte)(=0x06)         |
|----------------------------------------------------------------------------------------------|
|  2  |                               Extended tag code(0x11)                                  |
|----------------------------------------------------------------------------------------------|
|  3  |            IEEE organizational unique identifier third two hex digits(0x46)            |
|----------------------------------------------------------------------------------------------|
|  4  |            IEEE organizational unique identifier third two hex digits(0xD0)            |
|----------------------------------------------------------------------------------------------|
|  5  |            IEEE organizational unique identifier third two hex digits(0x00)            |
|----------------------------------------------------------------------------------------------|
|  6  |Headphone|        Output configuration     |  F63=0   |      Dolby VSADB version        |
|     | playback|                                 |          |                                 |
|----------------------------------------------------------------------------------------------|
|  7  |                            Sink device capabilities                                    |
|----------------------------------------------------------------------------------------------|

Table 12: Meanings of bit7 of Dolby VSADB byte 6(headphone playback)
|----------------------------------------------------------------------------------------------|
|    Meaning     |                Bit 7 of Dolby VSADB byte 6                                  |
|----------------------------------------------------------------------------------------------|
|   If bite = 0  |      Sink device not configured for headphone playback only                 |
|----------------------------------------------------------------------------------------------|
|   If bite = 1  |       Sink device configured for headphone playback only                    |
|----------------------------------------------------------------------------------------------|

Table 13: Meanings of bits [6:4] of Dolby VSADB byte 6(output configuration)
|----------------------------------------------------------------------------------------------|
|                |               Bit [6:4] of Dolby VSADB byte 6                               |
|    Meaning     | ----------------------------------------------------------------------------|
|                |         Bit 6          |         Bit 5          |         Bit 4             |
|----------------------------------------------------------------------------------------------|
|    If bit = 0  |Height speaker zone not | Surround speaker zone not | Center speaker zone not|
|                |        present         |           present         |         present        |
|----------------------------------------------------------------------------------------------|
|    If bit = 0  |Height speaker zone     | Surround speaker zone     | Center speaker zone    |
|                |        present         |           present         |         present        |
|----------------------------------------------------------------------------------------------|

Table 14: Meanings of bits [2:0] of Dolby VSADB byte 6(Dolby VSADB version)
|---------------------------------------------------------------------|
|                  Byte 6                   |           Version       |
|     Bit2    |     Bit 1     |    Bit 0    |                         |
|---------------------------------------------------------------------|
|      0      |      0        |      0      |            1.0          |
|---------------------------------------------------------------------|
|      0      |      0        |      1      |                         |
|-------------------------------------------|                         |
|                   ...                     |          Reserved       |
|-------------------------------------------|                         |
|      1      |      1        |      1      |                         |
|---------------------------------------------------------------------|

Table 15: Meanings of bit [7:0] of Dolby VSADB byte 7(sink device capabilities)
|----------------------------------------------------------------------------------------------|
|             |               Bit [7:0] of Dolby VSADB byte 7                                  |
|    Meaning  | -------------------------------------------------------------------------------|
|             |Bit 7 |Bit 6 |Bit 5 |Bit 4 |Bit 3 |Bit 2 |Bit 1 |         Bit 0                 |
|----------------------------------------------------------------------------------------------|
|  If bit = 0 |                                                |Refer to Dolby MAT Short Audio |
|             |                                                |          Descriptors          |
|-------------|                                                |-------------------------------|
|  If bit = 0 |             F77 to F71 = 0                     |Sink device supports Dolby MAT |
|             |                                                | PCM decoding at 48 kHz only,  |
|             |                                                | and does not support Dolby    |
|             |                                                |     TrueHD decoding           |
|----------------------------------------------------------------------------------------------|

*/

#define VSADB_LENGTH (7)/* vendor_specific_audio_data_block length is 7bytes*/
#define VSADB_TOTAL_LINE_NUM_IN_AUD_CAP (7) //VSADB defination in the "/sys/class/amhdmitx/amhdmitx0/aud_cap"
#define DOLBY_VSADB_SYNC_WORD "e61146d000"
/*
 *if the VSADB defined in "/sys/class/amhdmitx/amhdmitx0/aud_cap",
 *we suppose it in this format:
 *
 *Dolby Vendor Specific:
 *  headphone_playback_only:0,
 *  center_speaker:1,
 *  surround_speaker:1,
 *  height_speaker:1,
 *  Ver:1.0,
 *  MAT_PCM_48kHz_only:1,
 *  e61146d0007001,
 */

typedef  struct vendor_specific_audio_data_block{
    bool is_dolby_vsad;
    bool is_headphone_playback_only;//Sink device configured for headphone playback only
    bool is_height_speaker_zone_present;
    bool is_surround_speaker_zone_present;
    bool is_center_speaker_zone_present;
    int version;
    bool is_dolby_mat_pcm_48kHz;
    char vsadb_array[VSADB_LENGTH];
}  vsadb_t;


/* Vendor Specific Audio Data Block End */

typedef  struct {
    char              name[32];
    audio_format_t    audio_format;
} audio_format_pair_t;

/*these format name is from hdmi driver, don't change it*/
static audio_format_pair_t hdmi_format_table[] = {
    {"PCM"            , AUDIO_FORMAT_PCM_16_BIT   },
    {"AC-3"           , AUDIO_FORMAT_AC3          },
    {"MPEG1"          , AUDIO_FORMAT_MP2          },
    {"MP3"            , AUDIO_FORMAT_MP3          },
    {"MPEG2"          , AUDIO_FORMAT_MP2          },
    {"AAC"            , AUDIO_FORMAT_AAC          },
    {"DTS-HD"         , AUDIO_FORMAT_DTS_HD       }, /*put dts-hd before dts, we can check dts-hd first*/
    {"DTS"            , AUDIO_FORMAT_DTS          },
    {"Dobly_Digital+" , AUDIO_FORMAT_E_AC3        },
    {"MAT"            , AUDIO_FORMAT_MAT          },
    {"WMA_Pro"        , AUDIO_FORMAT_WMA_PRO      },
};

typedef  struct {
    char     rate_name[16];
    int32_t  sample_rate;
} hdmi_audio_rate_t;


/* The all support sample rate are:
 * 8/16/22.05/24/32/48/88.2/96/176.4/192 kHz
 */
typedef struct {
    audio_format_t audio_format;    /*the support format*/
    int32_t        max_channels;    /*the support max channel*/
    int32_t        samplerate[AUDIO_PROFILE_SAMPLERATE_NUM];  /*the support sample rate*/
    int32_t        bitwidth_mask;   /*the support bitwidth*/
    int32_t        dep_value;       /*the dep value in edid*/
    vsadb_t vsadb; //vendor_specific_audio_data_block
} audio_profile_cap_t;


struct hdmi_audio_profile_t{
    audio_profile_cap_t audio_cap_item[AUIDO_PROFILE_ITEM_NUM];
} ;

static struct hdmi_audio_profile_t hdmi_audio_profile;

static int32_t edid_audio_format_parse(char * audio_info, audio_profile_cap_t * audio_profile) {
    int32_t i = 0;
    int32_t format_num = sizeof(hdmi_format_table) / sizeof(audio_format_pair_t);
    bool    b_found = false;
    for (i = 0; i < format_num; i++) {
        if (strstr(audio_info, hdmi_format_table[i].name)) {
            audio_profile->audio_format = hdmi_format_table[i].audio_format;
            b_found = 1;
            break;
        }
    }
    return b_found ? 0 : -1;
}

static void edid_audio_channel_parse(char * audio_info, audio_profile_cap_t * audio_profile) {
    sscanf(audio_info, AUDIO_PROFILE_MAXCH_STRING, &audio_profile->max_channels);
    return;
}

static void edid_audio_samplerate_parse(char * audio_info, audio_profile_cap_t * audio_profile) {
    char *saveptr = NULL;
    char *string_tok = strtok_r(audio_info, "/", &saveptr);
    int i = 0;
    do {
       if (string_tok) {
            //ALOGI("sample rate =%d", (int)(atof(string_tok) * 1000));
            audio_profile->samplerate[i] = (int)(atof(string_tok) * 1000);
            i++;
            if (i >= AUDIO_PROFILE_SAMPLERATE_NUM) {
                break;
            }
            string_tok = strtok_r(NULL, "/", &saveptr);
       }
    } while (string_tok);

    return;
}

static void edid_audio_depvalue_parse(char * audio_info, audio_profile_cap_t * audio_profile) {
    sscanf(audio_info, AUDIO_PROFILE_DEPVALUE_STRING, &audio_profile->dep_value);
    return;
}

int string2hex(char *str, unsigned char *out, unsigned int *outlen)
{
    char *p = str;
    char high = 0;
    char low = 0;
    int tmplen = strlen(p);
    int cnt = 0;
    tmplen = strlen(p);
    while (cnt < (tmplen / 2))
    {
        high = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;
        low = (*(++p) > '9' && ((*p <= 'F') || (*p <= 'f'))) ? *(p) - 48 - 7 : *(p) - 48;
        out[cnt] = ((high & 0x0f) << 4 | (low & 0x0f));
        p++;
        cnt++;
    }
    if (tmplen % 2 != 0)
        out[cnt] = ((*p > '9') && ((*p <= 'F') || (*p <= 'f'))) ? *p - 48 - 7 : *p - 48;

    if (outlen != NULL)
        *outlen = tmplen / 2 + tmplen % 2;

    return tmplen / 2 + tmplen % 2;
}


static int32_t parse_vendor_specific_audio_data_block(char * audio_info, audio_profile_cap_t * audio_profile)
{
    char *saveptr = NULL;
    char *string_tok = NULL;
    bool b_found = false;

    ALOGV("%s line %d audio_info:%s is_dolby_vsad %d\n", __func__, __LINE__, audio_info, audio_profile->vsadb.is_dolby_vsad);
    if (!audio_profile->vsadb.is_dolby_vsad) {
        string_tok = strtok_r(audio_info, ":", &saveptr);
    }
    else {
        string_tok = strtok_r(audio_info, ",", &saveptr);
    }
    ALOGV("%s line %d string_tok:%s\n", __func__, __LINE__, string_tok);
    {
        if (string_tok) {
            ALOGV("%s line %d string_tok:%s\n", __func__, __LINE__, string_tok);
            if ( (!audio_profile->vsadb.is_dolby_vsad) && strstr(string_tok, "Dolby Vendor Specific") ) {
                audio_profile->vsadb.is_dolby_vsad = true;
                ALOGV("%s line %d is_dolby_vsad:%d\n", __func__, __LINE__, audio_profile->vsadb.is_dolby_vsad);
                b_found = true;
            }
            if (audio_profile->vsadb.is_dolby_vsad) {
                if (strstr(string_tok, "Ver")) {
                    if (strstr(string_tok, "1.0"))
                        audio_profile->vsadb.version = 1;
                    ALOGV("%s line %d version:%d\n", __func__, __LINE__, audio_profile->vsadb.version);
                    b_found = true;
                }
                else if (strstr(string_tok, "headphone_playback_only")) {
                    audio_profile->vsadb.is_headphone_playback_only = !!strstr(string_tok, "headphone_playback_only:1");
                    ALOGV("%s line %d is_headphone_playback_only:%d\n",
                        __func__, __LINE__, audio_profile->vsadb.is_headphone_playback_only);
                    b_found = true;
                }
                else if (strstr(string_tok, "center_speaker")) {
                    audio_profile->vsadb.is_height_speaker_zone_present = !!strstr(string_tok,"center_speaker:1");
                    ALOGV("%s line %d is_height_speaker_zone_present:%d\n",
                        __func__, __LINE__, audio_profile->vsadb.is_height_speaker_zone_present);
                    b_found = true;
                }
                else if (strstr(string_tok, "surround_speaker")) {
                    audio_profile->vsadb.is_surround_speaker_zone_present = !!strstr(string_tok, "surround_speaker:1");
                    ALOGV("%s line %d is_surround_speaker_zone_present:%d\n",
                        __func__, __LINE__, audio_profile->vsadb.is_surround_speaker_zone_present);
                    b_found = true;
                }
                else if (strstr(string_tok, "height_speaker")) {
                    audio_profile->vsadb.is_center_speaker_zone_present = !!strstr(string_tok, "height_speaker:1");
                    ALOGV("%s line %d is_center_speaker_zone_present:%d\n",
                        __func__, __LINE__, audio_profile->vsadb.is_center_speaker_zone_present);
                    b_found = true;
                }
                else if (strstr(string_tok, "MAT_PCM_48kHz_only")) {
                    audio_profile->vsadb.is_dolby_mat_pcm_48kHz = !!strstr(string_tok, "MAT_PCM_48kHz_only:1");
                    ALOGV("%s line %d is_dolby_mat_pcm_48kHz:%d\n",
                        __func__, __LINE__, audio_profile->vsadb.is_dolby_mat_pcm_48kHz);
                    b_found = true;
                }
                else if (strstr(string_tok, DOLBY_VSADB_SYNC_WORD)) {
                    char *vsadb_src = strstr(string_tok, DOLBY_VSADB_SYNC_WORD);
                    char *vsadb_target = (audio_profile->vsadb.vsadb_array);
                    unsigned int target_len = 0;
                    string2hex(vsadb_src, (unsigned char *)vsadb_target, &target_len);
                    ALOGV("%s line %d vsadb_array 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x\n",
                        __func__, __LINE__, vsadb_target[0], vsadb_target[1], vsadb_target[2],
                        vsadb_target[3], vsadb_target[4], vsadb_target[5], vsadb_target[6]);
                    b_found = true;
                }
            }
        }
    }
    return b_found ? 0 : -1;
}

/* the string info format is:
 * CodingType MaxChannels SamplingFreq SampleSize DepValue
 */
static int32_t edid_audio_info_parse(char * audio_info, audio_profile_cap_t * audio_profile) {
    char *saveptr = NULL;
    char *string_tok = strtok_r(audio_info, ",", &saveptr);
    int32_t ret = 0;
    if (string_tok) {
        bool is_dolby_vsadb = strstr(string_tok, "Dolby Vendor");
        bool is_two_space = strstr(string_tok, "  ");
        if (!is_dolby_vsadb && !is_two_space)
            ret = edid_audio_format_parse(string_tok, audio_profile);
        else
            ret = parse_vendor_specific_audio_data_block(string_tok, audio_profile);
        if (ret) {
            ALOGE("%s doesn't support %s\n", __func__, audio_info);
            return -1;
        }
        string_tok = strtok_r(NULL, ",", &saveptr);
        if (string_tok) {
            /*parse channel*/
            edid_audio_channel_parse(string_tok, audio_profile);
            string_tok = strtok_r(NULL, ",", &saveptr);
            if (string_tok) {
                /*parse samplerate*/
                edid_audio_samplerate_parse(string_tok, audio_profile);
                while(string_tok) {
                    string_tok = strtok_r(NULL, ",", &saveptr);
                    if (string_tok) {
                        /*parse depvalue*/
                        if (strstr(string_tok, "DepVaule")) {
                            edid_audio_depvalue_parse(string_tok, audio_profile);
                        }
                    }

                }
            }
        }
    }

    return 0;
}

static audio_profile_cap_t * get_edid_support_audio_format(audio_format_t audio_format) {
    int i = 0;
    for (i = 0; i < AUIDO_PROFILE_ITEM_NUM; i++) {
        if ((hdmi_audio_profile.audio_cap_item[i].audio_format == audio_format) &&
            hdmi_audio_profile.audio_cap_item[i].audio_format != AUDIO_FORMAT_INVALID &&
            hdmi_audio_profile.audio_cap_item[i].audio_format != AUDIO_FORMAT_DEFAULT) {
            return &hdmi_audio_profile.audio_cap_item[i];
        }
    }
    return NULL;
}

int  aml_hdmi_audio_profile_parser() {
    FILE *file = NULL;
    char * infobuf = NULL;
    int32_t buf_len = 256;
    int32_t i = 0, j = 0, k = 0;
    int32_t ret = 0;
    audio_profile_cap_t * audio_cap_item = NULL;
    int32_t vsadb_sub_index = 0;
    int32_t index_of_audio_cap_for_vsadb = 0;//current suppose to analysis the dolby vsadb.

    infobuf = (char *)aml_audio_calloc(1, buf_len * sizeof(char *));
    file = fopen("/sys/class/amhdmitx/amhdmitx0/aud_cap", "r");
    if (!file) {
        goto exit;
    }
    memset(&hdmi_audio_profile, 0, sizeof(struct hdmi_audio_profile_t));
    /*skip the first line*/
    fgets(infobuf, buf_len, file);
    while (NULL != fgets(infobuf, buf_len, file)) {
        bool duplicated = false;
        ALOGV("%s***************************************\n", __func__);
        ALOGV("%s infobuf:%s index %d vsadb_sub_index %d\n", __func__, infobuf, i, vsadb_sub_index);

        audio_cap_item = &hdmi_audio_profile.audio_cap_item[i];
        ret = edid_audio_info_parse(infobuf, audio_cap_item);
        ALOGV("%s edid audio info parse ret %d\n", __func__, ret);

        memset(infobuf, 0, buf_len);
        /*can't parse it, continue parse*/
        if (ret != 0) {
            memset(audio_cap_item, 0, sizeof(audio_profile_cap_t));
            continue;
        }

        /*check whether it is duplicated one*/
        for (k = 0; k < i; k++) {
            if (hdmi_audio_profile.audio_cap_item[k].audio_format == audio_cap_item->audio_format) {
                /*we use the big one*/
                if (audio_cap_item->max_channels > hdmi_audio_profile.audio_cap_item[k].max_channels) {
                    memcpy(&hdmi_audio_profile.audio_cap_item[k], audio_cap_item, sizeof(audio_profile_cap_t));
                }
                memset(audio_cap_item, 0, sizeof(audio_profile_cap_t));
                duplicated = true;
                break;
            }
        }
        if (duplicated) {
            continue;
        }

        ALOGV("%s format=0x%x max channel=%d depvalue=0x%x\n", __func__,
            audio_cap_item->audio_format,
            audio_cap_item->max_channels,
            audio_cap_item->dep_value);
        for (j = 0; j < AUDIO_PROFILE_SAMPLERATE_NUM; j++) {
            if (!audio_cap_item->samplerate[j]) {
                break;
            }
            ALOGV("sample rate =%d\n", audio_cap_item->samplerate[j]);
        }

        /*if not the "Dolby Vendor Specific", it is the normal SAD*/
        if (!audio_cap_item->vsadb.is_dolby_vsad)
           i++;
        else
            //vsadb has headphone & three speakers & version & Sink device capabilities & e61146d000xxxx(7bytes)
            vsadb_sub_index++;

        /* if all the VSADB part is analysised, will to store new EDID infor with new audio_profile_cap_t*/
        if (vsadb_sub_index > VSADB_TOTAL_LINE_NUM_IN_AUD_CAP) {
            index_of_audio_cap_for_vsadb = i;
            i++;
            vsadb_sub_index = 0;
        }

        if (i >= AUIDO_PROFILE_ITEM_NUM) {
            ALOGE("the HDMI EDID item is full\n");
            break;
        }
        memset(infobuf, 0, buf_len);
    }
    fclose(file);

    audio_profile_cap_t *audio_cap_vsadb = &(hdmi_audio_profile.audio_cap_item[index_of_audio_cap_for_vsadb]);
    if (audio_cap_vsadb) {
        if (audio_cap_vsadb->vsadb.is_dolby_vsad && audio_cap_vsadb->vsadb.is_dolby_mat_pcm_48kHz) {
            //Byte3 bit0:1 bit1:0
            //eg: "MAT, 8 ch, 348 kHz, DepVaule 0x1"
            /* According to Byte7 bit1 = 0, at table15
             * the sink device only support MAT PCM 48kHz decoding
             * and does not support Dolby TrueHD decoding
             * so, Dolby MS12 V2.4(or later), can output Dolby MAT.
             * TODO:
             * If Dolby MS12 further version can output DOlby TrueHD,
             * this part should be modified as Dolby VSADB spec.
             */
            audio_cap_vsadb->audio_format = AUDIO_FORMAT_MAT;
            audio_cap_vsadb->max_channels = 8;
            audio_cap_vsadb->samplerate[0] = 48000;
            audio_cap_vsadb->dep_value = 0x1;
        }
    }

exit:
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    return 0;
}

char*  get_hdmi_sink_cap_new(const char *keys, audio_format_t format, struct aml_arc_hdmi_desc *p_hdmi_descs)
{
    int i = 0;
    int fd = -1;
    int size = 0;
    char *aud_cap = NULL;
    audio_profile_cap_t * audio_cap_item = NULL;
    struct aml_audio_device *adev = adev_get_handle();

    ALOGD("%s is running...\n", __func__);
    aud_cap = (char*)aml_audio_malloc(1024);
    if (aud_cap == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    aml_hdmi_audio_profile_parser();
    memset(aud_cap, 0, 1024);

    /*currently we treat mat as truehd*/
    if (format == AUDIO_FORMAT_MAT_1_0 ||
        format == AUDIO_FORMAT_MAT_2_0 ||
        format == AUDIO_FORMAT_MAT_2_1 ||
        format == AUDIO_FORMAT_DOLBY_TRUEHD) {
        format = AUDIO_FORMAT_MAT;
    }

    /*currently we treat eac3_joc as eac3*/
    if (format == AUDIO_FORMAT_E_AC3_JOC) {
        format = AUDIO_FORMAT_E_AC3;
    }

    /* check the format cap */
    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        ALOGD("query hdmi format...\n");
        size += sprintf(aud_cap, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_IEC61937");

        p_hdmi_descs->ddp_fmt.atmos_supported = 0;//default set ddp-joc atmos_supported as false
        p_hdmi_descs->ddp_fmt.is_support = 0;
        p_hdmi_descs->dd_fmt.max_channels = 0;
        p_hdmi_descs->dd_fmt.is_support = 0;
        p_hdmi_descs->dts_fmt.is_support = 0;
        p_hdmi_descs->dtshd_fmt.is_support = 0;
        p_hdmi_descs->mat_fmt.is_support = 0;
        p_hdmi_descs->pcm_fmt.max_channels = 2;

        /*check EAC3*/
        audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_E_AC3);
        if (audio_cap_item) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
            p_hdmi_descs->ddp_fmt.is_support = 1;
            if (audio_cap_item->dep_value & 0x1) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
                p_hdmi_descs->ddp_fmt.atmos_supported = 1;
            }
        }
        ALOGD("%s ddp %s ddp-joc(atmos) %s\n", __func__,
            p_hdmi_descs->ddp_fmt.is_support ? "is supported;" : "is unsupported;",
            p_hdmi_descs->ddp_fmt.atmos_supported ? "is supported;" : "is unsupported;");

        /*check ac3*/
        audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_AC3);
        if (audio_cap_item) {
            p_hdmi_descs->dd_fmt.max_channels = audio_cap_item->max_channels;
            if (audio_cap_item->max_channels <= 2) {
                p_hdmi_descs->dd_fmt.is_support = 0;
            } else {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
                p_hdmi_descs->dd_fmt.is_support = 1;
            }
        }

        /*check dts-hd/dts*/
        audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_DTS_HD);
        if (audio_cap_item) {
            p_hdmi_descs->dts_fmt.is_support = 1;
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
            /*as we don't support dts decoder, then we cant' support dts hd passthrough*/
            if (adev->dts_decode_enable) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS_HD");
            }
            p_hdmi_descs->dtshd_fmt.is_support = 1;
        } else if ((audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_DTS)) != NULL) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
            p_hdmi_descs->dts_fmt.is_support = 1;
        }

        /*check pcm*/
        audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_PCM_16_BIT);
        if (audio_cap_item) {
            p_hdmi_descs->pcm_fmt.max_channels = audio_cap_item->max_channels;
            ALOGI("PCM cap Max channel =%d", p_hdmi_descs->pcm_fmt.max_channels);
        }

        /*check dolby truehd*/
        audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_MAT);
        if (audio_cap_item && (eDolbyMS12Lib == adev->dolby_lib_type)) {
            /*
             * when cat /sys/class/amhdmitx/amhdmitx0/aud_cap,
             * "MAT, 8 ch, 44.1/48/88.2/96/176.4/192 kHz, DepVaule 0x1"
             * would output MAT_1_0/MAT_2_0/MAT_2_1
             * here, call the AVR-TrueHD if AVR only supports MAT_1_0(DOLBY TRUEHD) not MAT_2_0/MAT_2_1,
             * If the AVR-TrueHD is broken or sick, we replace the AVR(Dolby ATMOS) with the property:
             * "vendor.media.audiohal.mat_1_0" with value 1.
             */
            if (audio_cap_item->dep_value == 0x1) {
                int force_truehd = aml_getprop_bool("vendor.media.audiohal.avr.mat_1_0");
                if (force_truehd) {
                    audio_cap_item->dep_value = 0x0;
                    ALOGD("JUST For DEBUG!!! Convert the MAT's dep_value from 1 to 0 for TrueHD!\n");
                }
            }
            //DLB MAT and DLB TrueHD SAD
            if (audio_cap_item->dep_value == 0x1) {
                //Byte3 bit0:1 bit1:0
                //eg: "MAT, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, DepVaule 0x1"
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0");
                p_hdmi_descs->mat_fmt.is_support = 1;

            } else if (audio_cap_item->dep_value == 0x0) {
                //Byte3 bit0:0 bit1:0
                //eg: "MAT, 8 ch, 48/96/192 kHz, DepVaule 0x0"
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0");
                p_hdmi_descs->mat_fmt.is_support = 0;//fixme about the mat_fmt.is_support

            } else if (audio_cap_item->dep_value == 0x3) {
                //Byte3 bit0:0 bit1:1
                //eg: "MAT, 8 ch, 48 kHz, DepVaule 0x3"
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0|AUDIO_FORMAT_MAT_2_1");
                p_hdmi_descs->mat_fmt.is_support = 1;

            } else {
                ALOGE("%s line %d MAT SAD Byte3 bit0&bit1 is invalid!", __func__, __LINE__);
                p_hdmi_descs->mat_fmt.is_support = 0;
            }
        }
    }
    /*check the channel cap */
    else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        ALOGD("query hdmi channels..., format %#x\n", format);
        switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
        case AUDIO_FORMAT_PCM_32_BIT:
            audio_cap_item = get_edid_support_audio_format(AUDIO_FORMAT_PCM_16_BIT);
            if (audio_cap_item) {
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                if (audio_cap_item->max_channels == 8) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1|AUDIO_CHANNEL_OUT_7POINT1");
                } else if (audio_cap_item->max_channels == 6) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1");
                }
            }
            break;
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_E_AC3:
        case AUDIO_FORMAT_E_AC3_JOC:
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
        case AUDIO_FORMAT_MAT:
            audio_cap_item = get_edid_support_audio_format(format);
            if (audio_cap_item) {
                switch (audio_cap_item->max_channels) {
                case 8:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_8CH);
                    break;
                case 7:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_7CH);
                    break;
                case 6:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_6CH);
                    break;
                case 5:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_5CH);
                    break;
                case 4:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_4CH);
                    break;
                case 3:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_3CH);
                    break;
                default:
                    size += sprintf(aud_cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_2CH);
                    break;
                }

            } else {
                ALOGE("%s not found support channel for 0x%x", __func__, format);
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");

            }
            break;
        default:
            /* take the 2ch suppported as default */
            size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
            break;
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        ALOGD("query hdmi sample_rate...format %#x\n", format);
        switch (format) {
            case AUDIO_FORMAT_IEC61937:
                size += sprintf(aud_cap, "sup_sampling_rates=%s",
                "8000|11025|16000|22050|24000|32000|44100|48000|128000|176400|192000");
                break;
            default:
                audio_cap_item = get_edid_support_audio_format(format);
                if (audio_cap_item) {
                    char temp[16] = { 0 };
                    size += sprintf(aud_cap, "sup_sampling_rates=%d", audio_cap_item->samplerate[0]);
                    for (i = 1; i < AUDIO_PROFILE_SAMPLERATE_NUM; i++) {
                        if (audio_cap_item->samplerate[i]) {
                            sprintf(temp, "|%d", audio_cap_item->samplerate[i]);
                            strcat(aud_cap, temp);
                        }
                    }
                    ALOGI("%s format =0x%x support rate =%s", __func__, format , aud_cap);
                } else {
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
                }
                break;
        }
    }

    return aud_cap;
fail:
    if (aud_cap) {
        aml_audio_free(aud_cap);
    }
    return NULL;
}


/*
  type : 0 -> playback, 1 -> capture
*/
#define MAX_CARD_NUM    2
int get_external_card(int type)
{
    int card_num = 1;       // start num, 0 is defualt sound card.
    struct stat card_stat;
    char fpath[256];
    int ret;
    while (card_num <= MAX_CARD_NUM) {
        snprintf(fpath, sizeof(fpath), "/proc/asound/card%d", card_num);
        ret = stat(fpath, &card_stat);
        if (ret < 0) {
            ret = -1;
        } else {
            snprintf(fpath, sizeof(fpath), "/dev/snd/pcmC%uD0%c", card_num,
                     type ? 'c' : 'p');
            ret = stat(fpath, &card_stat);
            if (ret == 0) {
                return card_num;
            }
        }
        card_num++;
    }
    return ret;
}

/*
CodingType MaxChannels SamplingFreq SampleSize
PCM, 2 ch, 32/44.1/48/88.2/96/176.4/192 kHz, 16/20/24 bit
PCM, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, 16/20/24 bit
AC-3, 8 ch, 32/44.1/48 kHz,  bit
DTS, 8 ch, 44.1/48 kHz,  bit
OneBitAudio, 2 ch, 44.1 kHz,  bit
Dobly_Digital+, 8 ch, 44.1/48 kHz, 16 bit
DTS-HD, 8 ch, 44.1/48/88.2/96/176.4/192 kHz, 16 bit
MAT, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, 16 bit
*/
char*  get_hdmi_sink_cap(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs)
{
    int i = 0;
    char * infobuf = NULL;
    int channel = 0;
    int dgraw = 0;
    int fd = -1;
    int size = 0;
    char *aud_cap = NULL;
    ALOGD("%s is running...\n", __func__);
    infobuf = (char *)aml_audio_malloc(1024 * sizeof(char));
    if (infobuf == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    aud_cap = (char*)aml_audio_malloc(1024);
    if (aud_cap == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    memset(aud_cap, 0, 1024);
    memset(infobuf, 0, 1024);
    fd = open("/sys/class/amhdmitx/amhdmitx0/aud_cap", O_RDONLY);
    if (fd >= 0) {
        int nread = read(fd, infobuf, 1024);
        /* check the format cap */
        if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            ALOGD("query hdmi format...\n");
            size += sprintf(aud_cap, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT");
            p_hdmi_descs->pcm_fmt.max_channels = 2;
            p_hdmi_descs->ddp_fmt.is_support = 0;
            if (mystrstr(infobuf, "Dobly_Digital+")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
                p_hdmi_descs->ddp_fmt.is_support = 1;
            }

            p_hdmi_descs->ddp_fmt.atmos_supported = 0;//default set ddp-joc atmos_supported as false
            if (mystrstr(infobuf, "ATMOS")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
                p_hdmi_descs->ddp_fmt.atmos_supported = 1;//set ddp-joc atmos_supported as true
            }
            ALOGD("%s ddp %s ddp-joc(atmos) %s\n", __func__,
                p_hdmi_descs->ddp_fmt.is_support ? "is supported;" : "is unsupported;",
                p_hdmi_descs->ddp_fmt.atmos_supported ? "is supported;" : "is unsupported;");

            if (mystrstr(infobuf, "AC-3")) {
                if (mystrstr(infobuf, "AC-3, 2 ch")) {
                    p_hdmi_descs->dd_fmt.is_support = 0;
                } else {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
                    p_hdmi_descs->dd_fmt.is_support = 1;
                }
            }
            if (mystrstr(infobuf, "DTS-HD")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS|AUDIO_FORMAT_DTS_HD");
                p_hdmi_descs->dts_fmt.is_support = 1;
            } else if (mystrstr(infobuf, "DTS")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
                p_hdmi_descs->dts_fmt.is_support = 1;
            }

            if (mystrstr(infobuf, "PCM, 6 ch")) {
                p_hdmi_descs->pcm_fmt.max_channels = 6;
            }
            if (mystrstr(infobuf, "PCM, 8 ch")) {
                p_hdmi_descs->pcm_fmt.max_channels = 8;
            }

            if (mystrstr(infobuf, "MAT")) {
                //DLB MAT and DLB TrueHD SAD
                int mat_edid_offset = find_offset_in_file_strstr(infobuf, "MAT");

                if (mat_edid_offset >= 0) {
                    char mat_edid_array[MAT_EDID_MAX_LEN] = {};
                    lseek(fd, mat_edid_offset, SEEK_SET);
                    int nread = read(fd, mat_edid_array, MAT_EDID_MAX_LEN);
                    if (nread >= 0) {
                        if (mystrstr(mat_edid_array, "DepVaule 0x1")) {
                            //Byte3 bit0:1 bit1:0
                            //eg: "MAT, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, DepVaule 0x1"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x0")) {
                            //Byte3 bit0:0 bit1:0
                            //eg: "MAT, 8 ch, 48/96/192 kHz, DepVaule 0x0"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0");
                            p_hdmi_descs->mat_fmt.is_support = 0;//fixme about the mat_fmt.is_support
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x3")) {
                            //Byte3 bit0:0 bit1:1
                            //eg: "MAT, 8 ch, 48 kHz, DepVaule 0x3"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0|AUDIO_FORMAT_MAT_2_1");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else {
                            ALOGE("%s line %d MAT SAD Byte3 bit0&bit1 is invalid!", __func__, __LINE__);
                            p_hdmi_descs->mat_fmt.is_support = 0;
                        }
                    }
                }
                else {
                    p_hdmi_descs->mat_fmt.is_support = 0;
                    ALOGE("%s line %d MAT EDID offset is invalid!", __func__, __LINE__);
                }
            }
        }
        /*check the channel cap */
        else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            ALOGD("query hdmi channels..., format %#x\n", format);
            p_hdmi_descs->pcm_fmt.max_channels = 2;
            switch (format) {
            case AUDIO_FORMAT_PCM_16_BIT:
            case AUDIO_FORMAT_PCM_32_BIT:
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                if (mystrstr(infobuf, "PCM, 8 ch")) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1|AUDIO_CHANNEL_OUT_7POINT1");
                    p_hdmi_descs->pcm_fmt.max_channels = 8;
                } else if (mystrstr(infobuf, "PCM, 6 ch")) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1");
                    p_hdmi_descs->pcm_fmt.max_channels = 6;
                }
                break;
            case AUDIO_FORMAT_AC3:
                if (mystrstr(infobuf, "AC-3")) {
                    size += sprintf(aud_cap, "sup_channels=%s", AC3_SUPPORT_CHANNEL);
                    p_hdmi_descs->dd_fmt.max_channels = 6;
                }
                break;
            case AUDIO_FORMAT_E_AC3:
                if (mystrstr(infobuf, "Dobly_Digital+")) {
                    size += sprintf(aud_cap, "sup_channels=%s", EAC3_SUPPORT_CHANNEL);
                    p_hdmi_descs->ddp_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_E_AC3_JOC:
                if (mystrstr(infobuf, "ATMOS") ||
                    mystrstr(infobuf, "Dobly_Digital+")) {
                    size += sprintf(aud_cap, "sup_channels=%s", EAC3_JOC_SUPPORT_CHANNEL);
                    p_hdmi_descs->ddp_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_DOLBY_TRUEHD:
                if (mystrstr(infobuf, "ATMOS") ||
                    mystrstr(infobuf, "Dobly_Digital+")) {
                    size += sprintf(aud_cap, "sup_channels=%s", EAC3_JOC_SUPPORT_CHANNEL);
                    p_hdmi_descs->ddp_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_DTS:
                if (mystrstr(infobuf, "DTS")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTS_SUPPORT_CHANNEL);
                    p_hdmi_descs->dts_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_DTS_HD:
                if (mystrstr(infobuf, "DTS-HD") || mystrstr(infobuf, "DTS")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTSHD_SUPPORT_CHANNEL);
                    p_hdmi_descs->dtshd_fmt.max_channels = 8;
                }
                break;
            default:
                /* take the 2ch suppported as default */
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                break;
            }
        } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            ALOGD("query hdmi sample_rate...\n");
            /* take the 32/44.1/48 khz suppported as default */
            size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");

            if (format != AUDIO_FORMAT_IEC61937) {
                if (mystrstr(infobuf, "88.2")) {
                    size += sprintf(aud_cap + size, "|%s", "88200");
                }
                if (mystrstr(infobuf, "96")) {
                    size += sprintf(aud_cap + size, "|%s", "96000");
                }
                if (mystrstr(infobuf, "176.4")) {
                    size += sprintf(aud_cap + size, "|%s", "176400");
                }
                if (mystrstr(infobuf, "192")) {
                    size += sprintf(aud_cap + size, "|%s", "192000");
                }
            } else {
              if((mystrstr(infobuf, "Dobly_Digital+") || mystrstr(infobuf, "DTS-HD") ||
                  mystrstr(infobuf, "MAT")) && format == AUDIO_FORMAT_IEC61937) {
                  size += sprintf(aud_cap + size, "|%s", "128000|176400|192000");
              }
            }
        }
    } else {
        ALOGE("open /sys/class/amhdmitx/amhdmitx0/aud_cap failed!!\n");
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    if (fd >= 0) {
        close(fd);
    }
    return aud_cap;
fail:
    if (aud_cap) {
        aml_audio_free(aud_cap);
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    return NULL;
}

char*  get_hdmi_sink_cap_dolbylib(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs, int conv_support)
{
    int i = 0;
    char * infobuf = NULL;
    int channel = 0;
    int dgraw = 0;
    int fd = -1;
    int size = 0;
    char *aud_cap = NULL;
    int dolby_decoder_sup = 0;
    ALOGD("%s is running...\n", __func__);
    infobuf = (char *)aml_audio_malloc(1024 * sizeof(char));
    if (infobuf == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    aud_cap = (char*)aml_audio_malloc(1024);
    if (aud_cap == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    memset(aud_cap, 0, 1024);
    memset(infobuf, 0, 1024);
    fd = open("/sys/class/amhdmitx/amhdmitx0/aud_cap", O_RDONLY);
    if (fd >= 0) {
        int nread = read(fd, infobuf, 1024);

        /*hdmi device only support dd, not ddp, then we check whether we can do convert*/
        if ((mystrstr(infobuf, "AC-3")) && !(mystrstr(infobuf, "Dobly_Digital+"))) {
            dolby_decoder_sup = conv_support;
            ALOGI("dolby convet support =%d", dolby_decoder_sup);
        }

        /* check the format cap */
        if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            ALOGD("query hdmi format...\n");
            size += sprintf(aud_cap, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_IEC61937");
            if (mystrstr(infobuf, "Dobly_Digital+") || dolby_decoder_sup) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
                p_hdmi_descs->ddp_fmt.is_support = 1;
            }
            if (mystrstr(infobuf, "ATMOS")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
            }
            if (mystrstr(infobuf, "AC-3")) {
                if (mystrstr(infobuf, "AC-3, 2 ch")) {
                    p_hdmi_descs->dd_fmt.is_support = 0;
                } else {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
                    p_hdmi_descs->dd_fmt.is_support = 1;
                }
            }

            if (mystrstr(infobuf, "DTS-HD")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS|AUDIO_FORMAT_DTS_HD");
                p_hdmi_descs->dtshd_fmt.is_support = 1;
            } else if (mystrstr(infobuf, "DTS")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
                p_hdmi_descs->dts_fmt.is_support = 1;
            }


            if (mystrstr(infobuf, "MAT")) {
                //DLB MAT and DLB TrueHD SAD
                int mat_edid_offset = find_offset_in_file_strstr(infobuf, "MAT");

                if (mat_edid_offset >= 0) {
                    char mat_edid_array[MAT_EDID_MAX_LEN] = {};
                    lseek(fd, mat_edid_offset, SEEK_SET);
                    int nread = read(fd, mat_edid_array, MAT_EDID_MAX_LEN);
                    if (nread >= 0) {
                        if (mystrstr(mat_edid_array, "DepVaule 0x1")) {
                            //Byte3 bit0:1 bit1:0
                            //eg: "MAT, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, DepVaule 0x1"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x0")) {
                            //Byte3 bit0:0 bit1:0
                            //eg: "MAT, 8 ch, 48/96/192 kHz, DepVaule 0x0"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0");
                            p_hdmi_descs->mat_fmt.is_support = 0;//fixme about the mat_fmt.is_support
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x3")) {
                            //Byte3 bit0:0 bit1:1
                            //eg: "MAT, 8 ch, 48 kHz, DepVaule 0x3"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0|AUDIO_FORMAT_MAT_2_1");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else {
                            ALOGE("%s line %d MAT SAD Byte3 bit0&bit1 is invalid!", __func__, __LINE__);
                            p_hdmi_descs->mat_fmt.is_support = 0;
                        }
                    }
                }
                else {
                    p_hdmi_descs->mat_fmt.is_support = 0;
                    ALOGE("%s line %d MAT EDID offset is invalid!", __func__, __LINE__);
                }
            }
        }
        /*check the channel cap */
        else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            ALOGD("query hdmi channels..., format %#x\n", format);
            p_hdmi_descs->pcm_fmt.max_channels = 2;
            switch (format) {
            case AUDIO_FORMAT_PCM_16_BIT:
            case AUDIO_FORMAT_PCM_32_BIT:
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                if (mystrstr(infobuf, "PCM, 8 ch")) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1|AUDIO_CHANNEL_OUT_7POINT1");
                    p_hdmi_descs->pcm_fmt.max_channels = 8;
                } else if (mystrstr(infobuf, "PCM, 6 ch")) {
                    size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1");
                    p_hdmi_descs->pcm_fmt.max_channels = 6;
                }
                break;
            case AUDIO_FORMAT_AC3:
                if (mystrstr(infobuf, "AC-3")) {
                    size += sprintf(aud_cap, "sup_channels=%s", AC3_SUPPORT_CHANNEL);
                    p_hdmi_descs->dd_fmt.max_channels = 6;
                }
                break;
            case AUDIO_FORMAT_E_AC3:
                if (mystrstr(infobuf, "Dobly_Digital+") || dolby_decoder_sup) {
                    size += sprintf(aud_cap, "sup_channels=%s", EAC3_SUPPORT_CHANNEL);
                    p_hdmi_descs->ddp_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_E_AC3_JOC:
                if (mystrstr(infobuf, "ATMOS") ||
                    mystrstr(infobuf, "Dobly_Digital+") ||
                    dolby_decoder_sup) {
                    size += sprintf(aud_cap, "sup_channels=%s", EAC3_JOC_SUPPORT_CHANNEL);
                    p_hdmi_descs->ddp_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_DTS:
                if (mystrstr(infobuf, "DTS")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTS_SUPPORT_CHANNEL);
                    p_hdmi_descs->dts_fmt.max_channels = 8;
                }
                break;
            case AUDIO_FORMAT_DTS_HD:
                if (mystrstr(infobuf, "DTS-HD") || mystrstr(infobuf, "DTS")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTSHD_SUPPORT_CHANNEL);
                    p_hdmi_descs->dtshd_fmt.max_channels = 8;
                }
                break;
            default:
                /* take the 2ch suppported as default */
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                break;
            }
        } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            ALOGD("query hdmi sample_rate...format %#x\n", format);
            switch (format) {
                case AUDIO_FORMAT_AC3:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
                    break;
                case AUDIO_FORMAT_E_AC3:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                    break;
                case AUDIO_FORMAT_E_AC3_JOC:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                    break;
                case AUDIO_FORMAT_AC4:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "44100|48000");
                    break;
                case AUDIO_FORMAT_DTS:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                    break;
                case AUDIO_FORMAT_DTS_HD:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                    break;
                case AUDIO_FORMAT_IEC61937:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s",
                    "8000|11025|16000|22050|24000|32000|44100|48000|128000|176400|192000");
                default:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
            }
        }
    } else {
        ALOGE("open /sys/class/amhdmitx/amhdmitx0/aud_cap failed!!\n");
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    if (fd >= 0) {
        close(fd);
    }
    return aud_cap;
fail:
    if (aud_cap) {
        aml_audio_free(aud_cap);
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    return NULL;
}

/*
 * fake to get sink capability for dolby ms12 logic
 * this only used in out_get_parameters()
 */
char*  get_hdmi_sink_cap_dolby_ms12(const char *keys,audio_format_t format,struct aml_arc_hdmi_desc *p_hdmi_descs)
{
    int i = 0;
    char * infobuf = NULL;
    int channel = 0;
    int dgraw = 0;
    int fd = -1;
    int size = 0;
    char *aud_cap = NULL;
    ALOGD("%s is running...\n", __func__);
    infobuf = (char *)aml_audio_malloc(1024 * sizeof(char));
    if (infobuf == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    aud_cap = (char*)aml_audio_malloc(1024);
    if (aud_cap == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    memset(aud_cap, 0, 1024);
    memset(infobuf, 0, 1024);
    fd = open("/sys/class/amhdmitx/amhdmitx0/aud_cap", O_RDONLY);
    if (fd >= 0) {
        int nread = read(fd, infobuf, 1024);

        /* check the format cap */
        if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
            ALOGD("query hdmi format...\n");
            size += sprintf(aud_cap, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_IEC61937");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC4");

            if (mystrstr(infobuf, "Dobly_Digital+")) {
                p_hdmi_descs->ddp_fmt.is_support = 1;
            }
            if (mystrstr(infobuf, "AC-3")) {
                p_hdmi_descs->dd_fmt.is_support = 1;
                /*if the sink device only support ac3 2ch, we need output pcm*/
                if (mystrstr(infobuf, "AC-3, 2 ch")) {
                    p_hdmi_descs->dd_fmt.is_support = 0;
                }
            }

            if (mystrstr(infobuf, "DTS-HD")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS|AUDIO_FORMAT_DTS_HD");
                p_hdmi_descs->dtshd_fmt.is_support = 1;
            } else if (mystrstr(infobuf, "DTS")) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
                p_hdmi_descs->dts_fmt.is_support = 1;
            }

            if (mystrstr(infobuf, "MAT")) {
                //DLB MAT and DLB TrueHD SAD
                int mat_edid_offset = find_offset_in_file_strstr(infobuf, "MAT");

                if (mat_edid_offset >= 0) {
                    char mat_edid_array[MAT_EDID_MAX_LEN] = {};
                    lseek(fd, mat_edid_offset, SEEK_SET);
                    int nread = read(fd, mat_edid_array, MAT_EDID_MAX_LEN);
                    if (nread >= 0) {
                        if (mystrstr(mat_edid_array, "DepVaule 0x1")) {
                            //Byte3 bit0:1 bit1:0
                            //eg: "MAT, 8 ch, 32/44.1/48/88.2/96/176.4/192 kHz, DepVaule 0x1"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x0")) {
                            //Byte3 bit0:0 bit1:0
                            //eg: "MAT, 8 ch, 48/96/192 kHz, DepVaule 0x0"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0");
                            p_hdmi_descs->mat_fmt.is_support = 0;//fixme about the mat_fmt.is_support
                        }
                        else if (mystrstr(mat_edid_array, "DepVaule 0x3")) {
                            //Byte3 bit0:0 bit1:1
                            //eg: "MAT, 8 ch, 48 kHz, DepVaule 0x3"
                            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD|AUDIO_FORMAT_MAT_1_0|AUDIO_FORMAT_MAT_2_0|AUDIO_FORMAT_MAT_2_1");
                            p_hdmi_descs->mat_fmt.is_support = 1;
                        }
                        else {
                            ALOGE("%s line %d MAT SAD Byte3 bit0&bit1 is invalid!", __func__, __LINE__);
                            p_hdmi_descs->mat_fmt.is_support = 0;
                        }
                    }
                }
                else {
                    p_hdmi_descs->mat_fmt.is_support = 0;
                    ALOGE("%s line %d MAT EDID offset is invalid!", __func__, __LINE__);
                }
            }
        }
        /*check the channel cap */
        else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
            ALOGD("query hdmi channels..., format %#x\n", format);
            if (format == AUDIO_FORMAT_DTS || format == AUDIO_FORMAT_DTS_HD) {
                if (mystrstr(infobuf, "DTS-HD")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTSHD_SUPPORT_CHANNEL);
                } else if (mystrstr(infobuf, "DTS")) {
                    size += sprintf(aud_cap, "sup_channels=%s", DTSHD_SUPPORT_CHANNEL);
                }
            } else {
                switch (format) {
                    case AUDIO_FORMAT_AC3:
                        size += sprintf(aud_cap, "sup_channels=%s", AC3_SUPPORT_CHANNEL);
                        break;
                    case AUDIO_FORMAT_E_AC3:
                        size += sprintf(aud_cap, "sup_channels=%s", EAC3_SUPPORT_CHANNEL);
                        break;
                    case AUDIO_FORMAT_E_AC3_JOC:
                        size += sprintf(aud_cap, "sup_channels=%s", EAC3_JOC_SUPPORT_CHANNEL);
                        break;
                    case AUDIO_FORMAT_AC4:
                        size += sprintf(aud_cap, "sup_channels=%s", AC4_SUPPORT_CHANNEL);
                        break;
                    case AUDIO_FORMAT_IEC61937:
                        size += sprintf(aud_cap, "sup_channels=%s", IEC61937_SUPPORT_CHANNEL);
                        break;
                    default:
                        size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
                }
            }
        } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            ALOGD("query hdmi sample_rate...format %#x\n", format);
            switch (format) {
                case AUDIO_FORMAT_AC3:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
                    break;
                case AUDIO_FORMAT_E_AC3:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                    break;
                case AUDIO_FORMAT_E_AC3_JOC:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                    break;
                case AUDIO_FORMAT_AC4:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "44100|48000");
                    break;
                case AUDIO_FORMAT_DTS:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                    break;
                case AUDIO_FORMAT_DTS_HD:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                    break;
                case AUDIO_FORMAT_IEC61937:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s",
                    "8000|11025|16000|22050|24000|32000|44100|48000|128000|176400|192000");
                    break;
                default:
                    size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
            }
        }
    } else {
        ALOGE("open /sys/class/amhdmitx/amhdmitx0/aud_cap failed!!\n");
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    if (fd >= 0) {
        close(fd);
    }
    return aud_cap;
fail:
    if (aud_cap) {
        aml_audio_free(aud_cap);
    }
    if (infobuf) {
        aml_audio_free(infobuf);
    }
    return NULL;
}

/*
 *It is used to get all the offload cap
 *
 */
char*  get_offload_cap(const char *keys,audio_format_t format)
{
    int i = 0;
    int channel = 0;
    int dgraw = 0;
    int size = 0;
    char *aud_cap = NULL;
    struct aml_audio_device *adev = adev_get_handle();

    ALOGD("%s is running...\n", __func__);

    aud_cap = (char*)aml_audio_malloc(1024);
    if (aud_cap == NULL) {
        ALOGE("malloc buffer failed\n");
        goto fail;
    }
    memset(aud_cap, 0, 1024);


    /* check the format cap */
    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        ALOGD("query hdmi format...\n");
        size += sprintf(aud_cap, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_IEC61937");

        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC4");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DOLBY_TRUEHD");
        } else {
            /*todo if dcv decoder is supprted*/
            //if (adev->dolby_decode_enable)
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
        }
        /*todo if dts decoder is supported*/
        //if (adev->dts_decode_enable)
        size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS|AUDIO_FORMAT_DTS_HD");
        size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AAC_LC");
        size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_MP3");

    }
    /*check the channel cap */
    else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        ALOGD("query hdmi channels..., format %#x\n", format);
        switch (format) {
            case AUDIO_FORMAT_AC3:
                size += sprintf(aud_cap, "sup_channels=%s", AC3_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_E_AC3:
                size += sprintf(aud_cap, "sup_channels=%s", EAC3_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_E_AC3_JOC:
                size += sprintf(aud_cap, "sup_channels=%s", EAC3_JOC_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_AC4:
                size += sprintf(aud_cap, "sup_channels=%s", AC4_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_DOLBY_TRUEHD:
                size += sprintf(aud_cap, "sup_channels=%s", DOLBY_TRUEHD_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_DTS:
                size += sprintf(aud_cap, "sup_channels=%s", DTS_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_DTS_HD:
                size += sprintf(aud_cap, "sup_channels=%s", DTSHD_SUPPORT_CHANNEL);
                break;
            case AUDIO_FORMAT_IEC61937:
                size += sprintf(aud_cap, "sup_channels=%s", IEC61937_SUPPORT_CHANNEL);
                break;
            default:
                size += sprintf(aud_cap, "sup_channels=%s", "AUDIO_CHANNEL_OUT_MONO|AUDIO_CHANNEL_OUT_STEREO");
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        ALOGD("query hdmi sample_rate...format %#x\n", format);
        switch (format) {
            case AUDIO_FORMAT_AC3:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
                break;
            case AUDIO_FORMAT_E_AC3:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                break;
            case AUDIO_FORMAT_E_AC3_JOC:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "16000|22050|24000|32000|44100|48000");
                break;
            case AUDIO_FORMAT_AC4:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "44100|48000");
                break;
            case AUDIO_FORMAT_DOLBY_TRUEHD:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "44100|48000|88200|96000|192000");
                break;
            case AUDIO_FORMAT_DTS:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                break;
            case AUDIO_FORMAT_DTS_HD:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "22050|24000|32000|44100|48000|88200|96000|192000");
                break;
            case AUDIO_FORMAT_IEC61937:
                size += sprintf(aud_cap, "sup_sampling_rates=%s",
                "8000|11025|16000|22050|24000|32000|44100|48000|128000|176400|192000");
                break;
            default:
                size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
        }
    }
    return aud_cap;
fail:
    if (aud_cap) {
        aml_audio_free(aud_cap);
    }
    return NULL;
}


inline static int hdmi_arc_process_sample_rate_str(struct format_desc *format_desc, char *aud_cap)
{
    int size = 0;
    if (format_desc->max_bit_rate & AML_HDMI_ARC_RATE_MASK_88200) {
        size += sprintf(aud_cap + size,  "|%s", "88200");
    }
    if (format_desc->max_bit_rate & AML_HDMI_ARC_RATE_MASK_96000) {
        size += sprintf(aud_cap + size,  "|%s", "96000");
    }
    if (format_desc->max_bit_rate & AML_HDMI_ARC_RATE_MASK_176400) {
        size += sprintf(aud_cap + size,  "|%s", "176400");
    }
    if (format_desc->max_bit_rate & AML_HDMI_ARC_RATE_MASK_192000) {
        size += sprintf(aud_cap + size,  "|%s", "192000");
    }
    return size;
}

inline static int hdmi_arc_process_channel_str(struct format_desc *format_desc, char *aud_cap)
{
    int size = 0;
    if (format_desc->max_channels >= 8) {
        size += sprintf(aud_cap + size,  "|%s", "AUDIO_CHANNEL_OUT_5POINT1|AUDIO_CHANNEL_OUT_7POINT1");
    } else if (format_desc->max_channels >= 6) {
        size += sprintf(aud_cap + size,  "|%s", "AUDIO_CHANNEL_OUT_5POINT1");
    }
    return size;
}

char *get_hdmi_arc_cap(struct audio_hw_device *dev, const char *keys, audio_format_t format)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;
    char *aud_cap = (char*)aml_audio_malloc(1024);
    int size = 0;
    if (aud_cap == NULL) {
        ALOGE("[%s:%d] aud_cap malloc buffer 1024 failed", __func__, __LINE__);
        return NULL;
    }
    memset(aud_cap, 0, 1024);
    if (adev->debug_flag) {
        ALOGD("[%s:%d] keys:%s, format:%#x", __func__, __LINE__, keys, format);
    }
    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        size += sprintf(aud_cap + size, "sup_formats=%s", "AUDIO_FORMAT_PCM_16_BIT|AUDIO_FORMAT_IEC61937");
        if (hdmi_desc->dd_fmt.is_support) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_AC3");
        }
        if (hdmi_desc->ddp_fmt.is_support) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3");
            if (hdmi_desc->ddp_fmt.atmos_supported) {
                size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_E_AC3_JOC");
            }
        }
        if (hdmi_desc->dts_fmt.is_support) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS");
        }
        if (hdmi_desc->dtshd_fmt.is_support) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_FORMAT_DTS_HD");
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        size += sprintf(aud_cap + size, "sup_channels=%s", "AUDIO_CHANNEL_OUT_STEREO");
        if (AUDIO_FORMAT_IEC61937 == format) {
            size += sprintf(aud_cap + size, "|%s", "AUDIO_CHANNEL_OUT_5POINT1|AUDIO_CHANNEL_OUT_7POINT1");
        } else if (AUDIO_FORMAT_AC3 == format) {
            size += hdmi_arc_process_channel_str(&hdmi_desc->dd_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_E_AC3 == format || AUDIO_FORMAT_E_AC3_JOC == format) {
            size += hdmi_arc_process_channel_str(&hdmi_desc->ddp_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_DTS == format) {
            size += hdmi_arc_process_channel_str(&hdmi_desc->dts_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_DTS_HD == format) {
            size += hdmi_arc_process_channel_str(&hdmi_desc->dtshd_fmt, aud_cap + size);
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        size += sprintf(aud_cap, "sup_sampling_rates=%s", "32000|44100|48000");
        if (AUDIO_FORMAT_IEC61937 == format) {
            size += sprintf(aud_cap + size,  "|%s", "88200|96000|176400|192000");
        } else if (AUDIO_FORMAT_AC3 == format) {
            size += hdmi_arc_process_sample_rate_str(&hdmi_desc->dd_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_E_AC3 == format || AUDIO_FORMAT_E_AC3_JOC == format) {
            size += hdmi_arc_process_sample_rate_str(&hdmi_desc->ddp_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_DTS == format) {
            size += hdmi_arc_process_sample_rate_str(&hdmi_desc->dts_fmt, aud_cap + size);
        } else if (AUDIO_FORMAT_DTS_HD == format) {
            size += hdmi_arc_process_sample_rate_str(&hdmi_desc->dtshd_fmt, aud_cap + size);
        }
    } else {
        ALOGW("[%s:%d] not supported key:%s", __func__, __LINE__, keys);
    }
    return aud_cap;
}

char *strdup_a2dp_cap_default(struct aml_audio_device *adev, const char *keys, audio_format_t format)
{
    char fmt[512] = "sup_formats=AUDIO_FORMAT_PCM_16_BIT";
    char ch_mask[128] = "sup_channels=AUDIO_CHANNEL_OUT_STEREO";
    char sr[64] = "sup_sampling_rates=48000|44100";
    char *cap = NULL;

    /* check the format cap */
    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        if (eDolbyMS12Lib == adev->dolby_lib_type) {
            strcat(fmt, "|AUDIO_FORMAT_AC4");
        }
        cap = strdup(fmt);
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        /* take the 2ch suppported as default */
        switch (format) {
        case AUDIO_FORMAT_AC4:
        case AUDIO_FORMAT_E_AC3:
            strcat(ch_mask, "|AUDIO_CHANNEL_OUT_7POINT1");
        case AUDIO_FORMAT_AC3:
            strcat(ch_mask, "|AUDIO_CHANNEL_OUT_5POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_PCM_16_BIT:
            cap = strdup(ch_mask);
            break;
        default:
            ALOGE("%s, unsupport format: %#x", __FUNCTION__, format);
            break;
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        /* take the 48 khz suppported as default */
        switch (format) {
        case AUDIO_FORMAT_AC4:
        case AUDIO_FORMAT_E_AC3:
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_PCM_16_BIT:
        case AUDIO_FORMAT_AC3:
            strcat(sr, "|32000");
            cap = strdup(sr);
            break;
        default:
            ALOGE("%s, unsupport format: %#x", __FUNCTION__, format);
            break;
        }
    } else {
        ALOGE("NOT support yet");
    }

    if (!cap) {
        cap = strdup("");
    }

    return cap;
}

char *strdup_tv_platform_cap_default(const char *keys, audio_format_t format)
{
    char fmt[64] = "sup_formats=";
    char ch_mask[512] = "sup_channels=";
    char sr[256] = "sup_sampling_rates=";
    char *cap = NULL;

    /* check the format cap */
    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
        switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            strcat(fmt, "AUDIO_FORMAT_PCM_16_BIT");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_PCM_32_BIT:
            strcat(fmt, "AUDIO_FORMAT_PCM_32_BIT");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_AC3:
            strcat(fmt, "AUDIO_FORMAT_AC3");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_E_AC3:
            strcat(fmt, "AUDIO_FORMAT_E_AC3");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_E_AC3_JOC:
            strcat(fmt, "AUDIO_FORMAT_E_AC3_JOC");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_AC4:
            strcat(fmt, "AUDIO_FORMAT_AC4");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_DTS:
            strcat(fmt, "AUDIO_FORMAT_DTS");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_DTS_HD:
            strcat(fmt, "AUDIO_FORMAT_DTS_HD");
            cap = strdup(fmt);
            break;
        case AUDIO_FORMAT_IEC61937:
            strcat(fmt, "AUDIO_FORMAT_IEC61937");
            cap = strdup(fmt);
            break;
        default:
            ALOGE("%s, unsupport format: %#x", __FUNCTION__, format);
            break;
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
        /* take the 2ch suppported as default */
        switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_STEREO");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_AC3:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_TRI,AUDIO_CHANNEL_OUT_TRI_BACK,AUDIO_CHANNEL_OUT_3POINT1,AUDIO_CHANNEL_OUT_QUAD,AUDIO_CHANNEL_OUT_SURROUND,AUDIO_CHANNEL_OUT_PENTA,AUDIO_CHANNEL_OUT_5POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_E_AC3:
        case AUDIO_FORMAT_E_AC3_JOC:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_TRI,AUDIO_CHANNEL_OUT_TRI_BACK,AUDIO_CHANNEL_OUT_3POINT1,AUDIO_CHANNEL_OUT_QUAD,AUDIO_CHANNEL_OUT_SURROUND,AUDIO_CHANNEL_OUT_PENTA,AUDIO_CHANNEL_OUT_5POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_AC4:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_TRI,AUDIO_CHANNEL_OUT_TRI_BACK,AUDIO_CHANNEL_OUT_3POINT1,AUDIO_CHANNEL_OUT_QUAD,AUDIO_CHANNEL_OUT_SURROUND,AUDIO_CHANNEL_OUT_PENTA,AUDIO_CHANNEL_OUT_5POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_DTS:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_2POINT1,AUDIO_CHANNEL_OUT_TRI,AUDIO_CHANNEL_OUT_QUAD_BACK,AUDIO_CHANNEL_OUT_QUAD_SIDE,AUDIO_CHANNEL_OUT_PENTA,AUDIO_CHANNEL_OUT_5POINT1,AUDIO_CHANNEL_OUT_6POINT1,AUDIO_CHANNEL_OUT_7POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_DTS_HD:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_MONO,AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_2POINT1,AUDIO_CHANNEL_OUT_TRI,AUDIO_CHANNEL_OUT_QUAD_BACK,AUDIO_CHANNEL_OUT_QUAD_SIDE,AUDIO_CHANNEL_OUT_PENTA,AUDIO_CHANNEL_OUT_5POINT1,AUDIO_CHANNEL_OUT_6POINT1,AUDIO_CHANNEL_OUT_7POINT1");
            cap = strdup(ch_mask);
            break;
        case AUDIO_FORMAT_IEC61937:
            strcat(ch_mask, "AUDIO_CHANNEL_OUT_STEREO,AUDIO_CHANNEL_OUT_5POINT1,AUDIO_CHANNEL_OUT_7POINT1");
            cap = strdup(ch_mask);
            break;
        default:
            ALOGE("%s, unsupport format: %#x", __FUNCTION__, format);
            break;
        }
    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
        /* take the 48 khz suppported as default */
        switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            strcat(sr, "32000|44100|48000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_AC3:
            strcat(sr, "32000|44100|48000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_E_AC3:
        case AUDIO_FORMAT_E_AC3_JOC:
            strcat(sr, "16000|22050|24000|32000|44100|48000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_AC4:
            strcat(sr, "44100|48000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_DTS:
            strcat(sr, "22050|24000|32000|44100|48000|88200|96000|192000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_DTS_HD:
            strcat(sr, "22050|24000|32000|44100|48000|88200|96000|192000");
            cap = strdup(sr);
            break;
        case AUDIO_FORMAT_IEC61937:
            strcat(sr, "8000|11025|16000|22050|24000|32000|44100|48000|128000|176400|192000");
            cap = strdup(sr);
            break;
        default:
            ALOGE("%s, unsupport format: %#x", __FUNCTION__, format);
            break;
        }
    } else {
        ALOGE("NOT support yet");
    }

    if (!cap) {
        cap = strdup("");
    }

    return cap;
}

char *out_get_parameters_wrapper_about_sup_sampling_rates__channels__formats(const struct audio_stream *stream, const char *keys)
{
    char *cap = NULL;
    char *para = NULL;
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    struct str_parms *parms;
    audio_format_t format;
    int ret = 0, val_int = 0;
    bool dd_only_support = false;
    bool conv_support = false;
    if ((eDolbyMS12Lib == adev->dolby_lib_type) || adev->dolby_decode_enable) {
        conv_support = true;
    }

    parms = str_parms_create_str (keys);
    ret = str_parms_get_int(parms, AUDIO_PARAMETER_STREAM_FORMAT, &val_int);

    format = (audio_format_t) val_int;
    if (format == 0) {
        format = out->hal_format;
    }
    ALOGI ("out_get_parameters %s,out %p format:%#x hal_format:%#x", keys, out, format, out->hal_format);
    if ((out->flags & AUDIO_OUTPUT_FLAG_PRIMARY) &&
        (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES) || strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS))) {
        ALOGV ("Amlogic - return hard coded channel_mask list or sample_rate for primary output stream.");
        if (strstr (keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
            cap = strdup ("sup_sampling_rates=8000|11025|16000|22050|24000|32000|44100|48000");
        } else {
            cap = strdup ("sup_channels=AUDIO_CHANNEL_OUT_MONO|AUDIO_CHANNEL_OUT_STEREO");
        }
    } else {
        if (out->out_device & AUDIO_DEVICE_OUT_HDMI_ARC) {
            cap = (char *) get_hdmi_arc_cap(&adev->hw_device, keys, format);
        } else if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
            cap = (char *) strdup_a2dp_cap_default(adev, keys, format);
        } else {
            if (out->is_tv_platform == 1) {
                cap = (char *)strdup_tv_platform_cap_default(keys, format);
            } else {
                if (out->flags & AUDIO_OUTPUT_FLAG_COMPRESS_OFFLOAD) {
                        cap = (char *) get_offload_cap(keys,format);
                } else {
                    cap = (char *)get_hdmi_sink_cap_new(keys,format,&(adev->hdmi_descs));

                    /* below patch is for dd only sink device.
                     * When connect dd only device, if we support ms12 or ddp convert,
                     * we should also reply we support ddp
                     */
                    dd_only_support  = adev->hdmi_descs.dd_fmt.is_support && !adev->hdmi_descs.ddp_fmt.is_support;
                    if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_FORMATS)) {
                        if (dd_only_support && conv_support) {
                            strcat(cap, "|AUDIO_FORMAT_E_AC3");
                        }
                    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_CHANNELS)) {
                        if (format == AUDIO_FORMAT_E_AC3) {
                            if (dd_only_support && conv_support) {
                                int dd_max_channels = adev->hdmi_descs.dd_fmt.max_channels;
                                if (dd_max_channels == 8) {
                                    sprintf(cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_8CH);
                                } else if (dd_max_channels == 6){
                                    sprintf(cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_6CH);
                                } else {
                                    sprintf(cap, "sup_channels=%s", SUPPORT_MAX_CHANNEL_2CH);
                                }
                            }
                        }
                    } else if (strstr(keys, AUDIO_PARAMETER_STREAM_SUP_SAMPLING_RATES)) {
                        if (format == AUDIO_FORMAT_E_AC3) {
                            if (dd_only_support && conv_support) {
                                sprintf(cap, "sup_sampling_rates=%s", "32000|44100|48000");
                            }
                        }

                    }
                }
            }
        }
    }
    if (cap) {
        para = strdup (cap);
        aml_audio_free (cap);
    } else {
        para = strdup ("");
    }
    str_parms_destroy(parms);
    ALOGI ("%s\n", para);
    return para;
}

