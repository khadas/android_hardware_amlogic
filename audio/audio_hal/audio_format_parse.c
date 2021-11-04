/*
 * hardware/amlogic/audio/TvAudio/audio_format_parse.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 */

#define LOG_TAG "audio_format_parse"
//#define LOG_NDEBUG 0

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <cutils/log.h>

#include "aml_audio_spdifdec.h"
#include "aml_audio_stream.h"
#include "audio_format_parse.h"
#include "aml_dump_debug.h"
#include "ac3_parser_utils.h"

#include "aml_alsa_mixer.h"
#include "audio_hw_utils.h"

#include "alsa_device_parser.h"

/*Find the position of 61937 sync word in the buffer*/
static int seek_61937_sync_word(char *buffer, int size)
{
    int i = -1;
    if (size < 4) {
        return i;
    }

    //DoDumpData(buffer, size, CC_DUMP_SRC_TYPE_INPUT_PARSE);

    for (i = 0; i < (size - 3); i++) {
        //ALOGV("%s() 61937 sync word %x %x %x %x\n", __FUNCTION__, buffer[i + 0], buffer[i + 1], buffer[i + 2], buffer[i + 3]);
        if (buffer[i + 0] == 0x72 && buffer[i + 1] == 0xf8 && buffer[i + 2] == 0x1f && buffer[i + 3] == 0x4e) {
            return i;
        }
        if (buffer[i + 0] == 0x4e && buffer[i + 1] == 0x1f && buffer[i + 2] == 0xf8 && buffer[i + 3] == 0x72) {
            return i;
        }
    }
    return -1;
}

/*DTSCD format is a special dts format without 61937 header*/
static int seek_dts_cd_sync_word(char *buffer, int size)
{
    int i = -1;
    if (size < 6) {
        return i;
    }

    for (i = 0; i < (size - 5); i++) {
        if ((buffer[i + 0] == 0xFF && buffer[i + 1] == 0x1F
             && buffer[i + 2] == 0x00 && buffer[i + 3] == 0xE8
             && buffer[i + 4] == 0xF0 && buffer[i + 5] == 0x07) ||
            (buffer[i + 0] == 0xE8 && buffer[i + 1] == 0x00
             && buffer[i + 2] == 0x1F && buffer[i + 3] == 0xFF
             && buffer[i + 6] == 0x07 && buffer[i + 7] == 0xF1) ||
             (buffer[i + 0] == 0xFF && buffer[i + 1] == 0x1F
             && buffer[i + 2] == 0x00 && buffer[i + 3] == 0xE8
             && buffer[i + 4] == 0xF1 && buffer[i + 5] == 0x07) ||
             (buffer[i + 0] == 0xFE && buffer[i + 1] == 0x7F
             && buffer[i + 2] == 0x01 && buffer[i + 3] == 0x80)) {
            return i;
        }
    }
    return -1;
}

static audio_channel_mask_t get_dolby_channel_mask(const unsigned char *frameBuf
        , int length)
{
    int scan_frame_offset;
    int scan_frame_size;
    int scan_channel_num;
    int scan_numblks;
    int scan_timeslice_61937;
    int scan_framevalid_flag;
    int ret = 0;
    int total_channel_num  = 0;

    if ((frameBuf == NULL) || (length <= 0)) {
        ret = -1;
    } else {
        ret = parse_dolby_frame_header(frameBuf, length, &scan_frame_offset, &scan_frame_size
                                       , &scan_channel_num, &scan_numblks, &scan_timeslice_61937, &scan_framevalid_flag);
        ALOGV("%s A:scan_channel_num %d scan_numblks %d scan_timeslice_61937 %d\n",
              __FUNCTION__, scan_channel_num, scan_numblks, scan_timeslice_61937);
    }

    if (ret) {
        return AUDIO_CHANNEL_OUT_STEREO;
    } else {
        total_channel_num += scan_channel_num;
        if (length - scan_frame_offset - scan_frame_size > 0) {
            ret = parse_dolby_frame_header(frameBuf + scan_frame_offset + scan_frame_size
                                           , length - scan_frame_offset - scan_frame_size, &scan_frame_offset, &scan_frame_size
                                           , &scan_channel_num, &scan_numblks, &scan_timeslice_61937, &scan_framevalid_flag);
            ALOGV("%s B:scan_channel_num %d scan_numblks %d scan_timeslice_61937 %d\n",
                  __FUNCTION__, scan_channel_num, scan_numblks, scan_timeslice_61937);
            if ((ret == 0) && (scan_timeslice_61937 == 3)) {
                /*dolby frame contain that DEPENDENT frame, that means 7.1ch*/
                total_channel_num = 8;
            }
        }
        return audio_channel_out_mask_from_count(total_channel_num);
    }
}

static int hdmiin_audio_format_detection(struct aml_mixer_handle *mixer_handle)
{
    int type = 0;

    if (alsa_device_is_auge())
        type = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_HDMIIN_AUDIO_TYPE);
    else
        type = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_SPDIFIN_AUDIO_TYPE);

    if (type >= LPCM && type <= PAUSE) {
        return type;
    } else {
        return LPCM;
    }
}

static int spdifin_audio_format_detection(struct aml_mixer_handle *mixer_handle)
{
    int type = 0;

    type = aml_mixer_ctrl_get_int(mixer_handle, AML_MIXER_ID_SPDIFIN_AUDIO_TYPE);

    if (type >= LPCM && type <= PAUSE) {
        return type;
    } else {
        return LPCM;
    }

}

/*
 * Extracts the relevant bits of the specified length from the string based on the offset address
 */
static size_t extract_bits(const char* buffer, size_t extract_bits_offset, size_t extract_bits_len) {
    int i = 0;
    size_t ret = 0;
    char mask = 0xFF;
    int offset_divisor = (int)extract_bits_offset / 8;
    int offset_remainder = (int)extract_bits_offset % 8;
    int len_divisor = 0;
    int len_remainder = 0;
    if (buffer == NULL) {
        ALOGE("%s, illegal param buffer, it is null", __FUNCTION__);
        return 0;
    }
    //ALOGD("%s, extract_bits_offset = %d, extract_bits_len = %d, offset_divisor = %d, offset_remainder = %d",
    //    __FUNCTION__, extract_bits_offset, extract_bits_len, offset_divisor, offset_remainder);
    char *temp_pointer = (char *)buffer;
    temp_pointer += offset_divisor;
    if (8 - offset_remainder >= (int)extract_bits_len) {
        ret = (temp_pointer[0] & (mask >> offset_remainder)) >> (8 - offset_remainder - (int)extract_bits_len);
        //ALOGD("%s, ret = %#x", __FUNCTION__, ret);
        return ret;
    }
    len_divisor = (extract_bits_len - (8 - offset_remainder)) / 8;
    len_remainder = (extract_bits_len - (8 - offset_remainder)) % 8;
    //ALOGD("%s, len_divisor = %d, len_remainder = %d", __FUNCTION__,  len_divisor, len_remainder);
    for (i = (len_divisor + 1); i >= 0; i--) {
        if (i == (len_divisor + 1)) {
            ret |= (temp_pointer[i] >> (8 - len_remainder));
        } else if (i == 0) {
            ret |= ((temp_pointer[i] & (mask >> offset_remainder)) << ((len_divisor - i) * 8 + len_remainder));
        } else {
            ret |= (temp_pointer[i] << ((len_divisor - i) * 8 + len_remainder));
        }
    }
    //ALOGD("%s, ret = %#x", __FUNCTION__, ret);
    return ret;
}

int get_dts_stream_channels(const char *buffer, size_t buffer_size) {
    int i = 0;
    int count = 0;
    int pos_iec_header = 0;
    bool little_end = false;
    size_t frame_header_len = 8;
    char *temp_buffer = NULL;
    char temp_ch;
    int channels = 0;
    int lfe = 0;
    size_t amode = 0;
    size_t lfe_value = 0;
    size_t bytes = 0;

    pos_iec_header = seek_61937_sync_word((char *)buffer, (int)buffer_size);
    if (pos_iec_header < 0) {
        return -1;
    }

    bytes = buffer_size - (size_t)pos_iec_header;
    if (buffer_size > 6) {
        //case of DTS type IV, have 12 bytes of IEC_DTS_HD_APPEND
        if (((buffer[pos_iec_header + 4] & 0x1f) == IEC61937_DTSHD)
            || ((buffer[pos_iec_header + 5] & 0x1f) == IEC61937_DTSHD)) {
            frame_header_len += 12;
        }
    }

    //IEC Header + 15 Byte Bitstream Header(120 bits)
    if (bytes < (frame_header_len + 15)) {
        ALOGE("%s, illegal param bytes(%d)", __FUNCTION__, bytes);
        return -1;
    }
    temp_buffer = (char *)aml_audio_malloc(sizeof(char) * bytes);
    if (temp_buffer == NULL) {
        ALOGE("%s, malloc error", __FUNCTION__);
        return -1;
    }
    memset(temp_buffer, '\0', sizeof(char) * bytes);
    memcpy(temp_buffer, buffer + pos_iec_header, bytes);

    if (temp_buffer[0] == 0xf8 && temp_buffer[1] == 0x72 && temp_buffer[2] == 0x4e && temp_buffer[3] == 0x1f) {
        little_end = true;
    }
    //ALOGD("%s, little_end = %d", __FUNCTION__, little_end);
    if (!little_end) {
        if ((bytes - frame_header_len) % 2 == 0) {
            count = bytes - frame_header_len;
        } else {
            count = bytes - frame_header_len - 1;
        }

        //DTS frame header mybe 11 bytes, and align 2 bytes
        count = (count > 12) ? 12 : count;
        for (i = 0; i < count; i+=2) {
            temp_ch = temp_buffer[frame_header_len + i];
            temp_buffer[frame_header_len + i] = temp_buffer[frame_header_len + i + 1];
            temp_buffer[frame_header_len + i + 1] = temp_ch;
        }
    }

    if (!((temp_buffer[frame_header_len + 0] == 0x7F
            && temp_buffer[frame_header_len + 1] == 0xFE
            && temp_buffer[frame_header_len + 2] == 0x80
            && temp_buffer[frame_header_len + 3] == 0x01))) {
        ALOGE("%s, illegal synchronization", __FUNCTION__);
        goto exit;
    } else {
        //ALOGI("%s, right synchronization", __FUNCTION__);
    }
    amode = extract_bits((const char*)(temp_buffer + frame_header_len), 60, 6);
    if (amode == 0x0) {
        channels = 1;
    } else if ((amode == 0x1) || (amode == 0x2) || (amode == 0x3) || (amode == 0x4)) {
        channels = 2;
    } else if ((amode == 0x5) || (amode == 0x6)) {
        channels = 3;
    } else if ((amode == 0x7) || (amode == 0x8)) {
        channels = 4;
    } else if (amode == 0x9) {
        channels = 5;
    } else if ((amode == 0xa) || (amode == 0xb) || (amode == 0xc)) {
        channels = 6;
    } else if (amode == 0xd) {
        channels = 7;
    } else if ((amode == 0xe) || (amode == 0xf)) {
        channels = 8;
    } else {
        ALOGE("%s, amode user defined", __FUNCTION__);
        goto exit;
    }
    lfe_value = extract_bits((const char*)(temp_buffer + frame_header_len), 85, 2);
    if (lfe_value == 0x0) {
        lfe = 0;
    } else if ((lfe_value == 1) || (lfe_value == 2)) {
        lfe = 1;
    } else {
        ALOGE("%s, invalid lfe value", __FUNCTION__);
        goto exit;
    }

    aml_audio_free(temp_buffer);
    //ALOGD("%s, channels = %d, lfe = %d", __FUNCTION__, channels, lfe);
    return (channels + lfe);

exit:
    if (temp_buffer != NULL) {
        aml_audio_free(temp_buffer);
        temp_buffer = NULL;
    }
    return -1;
}

int audio_type_parse(void *buffer, size_t bytes, int *package_size, audio_channel_mask_t *cur_ch_mask)
{
    int pos_sync_word = -1, pos_dtscd_sync_word = -1;
    char *temp_buffer = (char*)buffer;
    int AudioType = LPCM;
    uint32_t *tmp_pc;
    uint32_t pc = 0;
    uint32_t tmp = 0;
    static int dts_cd_sync_count = 0;

    DoDumpData(temp_buffer, bytes, CC_DUMP_SRC_TYPE_INPUT_PARSE);

    pos_sync_word = seek_61937_sync_word((char*)temp_buffer, bytes);

    if (pos_sync_word < 0) {
        pos_dtscd_sync_word = seek_dts_cd_sync_word((char*)temp_buffer, bytes);
        if (pos_dtscd_sync_word >= 0) {
            dts_cd_sync_count++;
            if (dts_cd_sync_count < DTSCD_VALID_COUNT) {
                AudioType = LPCM;
            } else {
                *package_size = DTSHD_PERIOD_SIZE * 2;
                ALOGV("%s() %d data format: %d *package_size %d\n", __FUNCTION__, __LINE__, AudioType, *package_size);
                AudioType = DTSCD;
            }
            return AudioType;
        } else {
            dts_cd_sync_count = 0;
        }
    }

    if (pos_sync_word >= 0) {
        tmp_pc = (uint32_t*)(temp_buffer + pos_sync_word + 4);
        pc = *tmp_pc;
        *cur_ch_mask = AUDIO_CHANNEL_OUT_STEREO;
        /*Value of 0-4bit is data type*/
        switch (pc & 0x1f) {
        case IEC61937_NULL:
            AudioType = MUTE;
            // not defined, suggestion is 4096 samples must have one
            *package_size = 4096 * 4;
            break;
        case IEC61937_AC3:
            AudioType = AC3;
            /* *cur_ch_mask = get_dolby_channel_mask((const unsigned char *)(temp_buffer + pos_sync_word + 8),
                    (bytes - pos_sync_word - 8));
            ALOGV("%d channel mask %#x", __LINE__, *cur_ch_mask);*/
            *package_size = AC3_PERIOD_SIZE;
            break;
        case IEC61937_EAC3:
            AudioType = EAC3;
            /* *cur_ch_mask = get_dolby_channel_mask((const unsigned char *)(temp_buffer + pos_sync_word + 8),
                    (bytes - pos_sync_word - 8));
            ALOGV("%d channel mask %#x", __LINE__, *cur_ch_mask);*/
            *package_size = EAC3_PERIOD_SIZE;
            break;
        case IEC61937_DTS1:
            AudioType = DTS;
            *package_size = DTS1_PERIOD_SIZE;
            break;
        case IEC61937_DTS2:
            AudioType = DTS;
            *package_size = DTS2_PERIOD_SIZE;
            break;
        case IEC61937_DTS3:
            AudioType = DTS;
            *package_size = DTS3_PERIOD_SIZE;
            break;
        case IEC61937_DTSHD:
            /*Value of 8-12bit is framesize*/
            tmp = (pc & 0x7ff) >> 8;
            AudioType = DTSHD;
            /*refer to IEC 61937-5 pdf, table 6*/
            *package_size = DTSHD_PERIOD_SIZE << tmp ;
            break;
        case IEC61937_MAT:
            AudioType = MAT;
            *package_size = MAT_PERIOD_SIZE;
            break;
        case IEC61937_PAUSE:
            AudioType = PAUSE;
            // Not defined, set it as 1024*4
            *package_size = 1024 * 4;
            break;
        default:
            AudioType = LPCM;
            break;
        }
        ALOGV("%s() data format: %d, *package_size %d, input size %zu\n",
              __FUNCTION__, AudioType, *package_size, bytes);
    }
    return AudioType;
}

static int get_config_by_params(struct pcm_config *config_in, bool normal_pcm)
{
    if (normal_pcm) {
        config_in->channels = 2;
    } else {
        /* HBR I2S 8 channel */
        config_in->channels = 8;
    }

    config_in->rate = 48000;
    config_in->format = PCM_FORMAT_S16_LE;
    config_in->period_size = PARSER_DEFAULT_PERIOD_SIZE;
    config_in->period_count = 4;

    return 0;
}

static int audio_type_parse_init(audio_type_parse_t *status)
{
    audio_type_parse_t *audio_type_status = status;
    struct aml_mixer_handle *mixer_handle = audio_type_status->mixer_handle;
    struct pcm_config *config_in = &(audio_type_status->config_in);
    struct pcm *in;
    int ret, bytes;
    int port = 0;

    if (check_chip_name("t3", 2, status->mixer_handle) &&
            audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI &&
            get_hdmiin_audio_mode(mixer_handle) == HDMIIN_MODE_I2S) {
        port = PORT_I2S4PARSER;
        audio_type_status->soft_parser = 1;
    } else {
        port = PORT_I2S;
    }

    audio_type_status->card = (unsigned int)alsa_device_get_card_index();
    audio_type_status->device = (unsigned int)alsa_device_update_pcm_index(port, CAPTURE);
    audio_type_status->flags = PCM_IN;

    get_config_by_params(&audio_type_status->config_in, 1);
    bytes = config_in->period_size * config_in->channels * 2;
    audio_type_status->period_bytes = bytes;

    /*malloc max audio type size, save last 3 byte*/
    audio_type_status->parse_buffer = (char*) aml_audio_malloc(sizeof(char) * (bytes + 16));
    if (NULL == audio_type_status->parse_buffer) {
        ALOGE("%s, no memory\n", __FUNCTION__);
        return -1;
    }

    if (audio_type_status->soft_parser) {
        in = pcm_open(audio_type_status->card, audio_type_status->device,
                      PCM_IN | PCM_NONEBLOCK, &audio_type_status->config_in);
        if (!pcm_is_ready(in)) {
            ALOGE("open device failed: %s\n", pcm_get_error(in));
            pcm_close(in);
            ret = -ENXIO;
            goto error;
        }
        audio_type_status->in = in;
    }
    enable_HW_resample(mixer_handle, HW_RESAMPLE_48K);

    ALOGD("init parser success: (%d), (%d), (%p)",
          audio_type_status->card, audio_type_status->device, audio_type_status->in);
    return 0;
error:
    aml_audio_free(audio_type_status->parse_buffer);
    return -1;
}

static int audio_type_parse_release(audio_type_parse_t *status)
{
    audio_type_parse_t *audio_type_status = status;

    if (audio_type_status->soft_parser && audio_type_status->in) {
        pcm_close(audio_type_status->in);
        audio_type_status->in = NULL;
    }
    aml_audio_free(audio_type_status->parse_buffer);

    return 0;
}

static int audio_transer_samplerate (int hw_sr)
{
    int samplerate;
    switch (hw_sr) {
    case HW_32K:
        samplerate = 32000;
        break;
    case HW_44K:
        samplerate = 44100;
        break;
    case HW_48K:
        samplerate = 48000;
        break;
    case HW_88K:
        samplerate = 88000;
        break;
    case HW_96K:
        samplerate = 96000;
        break;
    case HW_176K:
        samplerate = 176000;
        break;
    case HW_192K:
        samplerate = 192000;
        break;
    default:
        samplerate = 48000;
        break;
    }
    return samplerate;
}

static int update_audio_type(audio_type_parse_t *status, int update_bytes, int sr)
{
    audio_type_parse_t *audio_type_status = status;
    struct aml_mixer_handle *mixer_handle = audio_type_status->mixer_handle;
    struct pcm_config *config_in = &(audio_type_status->config_in);

    if (audio_type_status->audio_type == audio_type_status->cur_audio_type) {
        audio_type_status->read_bytes = 0;
        return 0;
    }
    if (audio_type_status->audio_type != LPCM && audio_type_status->cur_audio_type == LPCM) {
        /* check 2 period size of IEC61937 burst data to find syncword*/
        if (audio_type_status->read_bytes > (audio_type_status->package_size * 2)) {
            audio_type_status->audio_type = audio_type_status->cur_audio_type;
            audio_type_status->read_bytes = 0;
            AM_LOGI("package_size:%d no IEC61937 header found, PCM data!", audio_type_status->package_size);
            enable_HW_resample(mixer_handle, sr);
        }
        audio_type_status->read_bytes += update_bytes;
    } else {
        /* if find 61937 syncword or raw audio type changed,
        immediately update audio type*/
        audio_type_status->audio_type = audio_type_status->cur_audio_type;
        audio_type_status->read_bytes = 0;
        AM_LOGI("Raw data found: type(%d)\n", audio_type_status->audio_type);
        enable_HW_resample(mixer_handle, HW_RESAMPLE_DISABLE);
    }
    return 0;
}

static int is_normal_config(hdmiin_audio_packet_t cur_audio_packet)
{
    return cur_audio_packet == AUDIO_PACKET_NONE ||
        cur_audio_packet == AUDIO_PACKET_AUDS;
}

static int reconfig_pcm_by_packet_type(audio_type_parse_t *audio_type_status,
            hdmiin_audio_packet_t cur_audio_packet)
{
    hdmiin_audio_packet_t last_packet_type = audio_type_status->hdmi_packet;
    bool reopen = false;

    if (cur_audio_packet == AUDIO_PACKET_HBR && is_normal_config(last_packet_type)) {
        get_config_by_params(&audio_type_status->config_in, 0);
        reopen = true;
    } else if (is_normal_config(cur_audio_packet) && last_packet_type == AUDIO_PACKET_HBR) {
        get_config_by_params(&audio_type_status->config_in, 1);
        reopen = true;
    }

    if (reopen) {
        struct pcm *in = NULL;

        if (audio_type_status->in) {
            pcm_close(audio_type_status->in);
            audio_type_status->in = NULL;
        }

        ALOGI("%s(), reopen channels %d",
            __func__, audio_type_status->config_in.channels);
        in = pcm_open(audio_type_status->card, audio_type_status->device,
                      PCM_IN | PCM_NONEBLOCK, &audio_type_status->config_in);
        if (!pcm_is_ready(in)) {
            ALOGE("open device failed: %s\n", pcm_get_error(in));
            pcm_close(in);
            return -EINVAL;
        }
        audio_type_status->in = in;
    }

    return 0;
}

#define WAIT_COUNT_MAX 30
static void* audio_type_parse_threadloop(void *data)
{
    audio_type_parse_t *audio_type_status = (audio_type_parse_t *)data;
    int bytes, ret = -1;
    int cur_samplerate = HW_RESAMPLE_48K;
    int last_cur_samplerate = HW_RESAMPLE_48K;
    hdmiin_audio_packet_t cur_audio_packet = AUDIO_PACKET_NONE;
    int read_bytes = 0, read_back, nodata_count;
    int txlx_chip = check_chip_name("txlx", 4, audio_type_status->mixer_handle);
    int txl_chip = check_chip_name("txl", 3, audio_type_status->mixer_handle);
    int auge_chip = alsa_device_is_auge();

    ret = audio_type_parse_init(audio_type_status);
    if (ret < 0) {
        ALOGE("fail to init parser\n");
        return ((void *) 0);
    }

    prctl(PR_SET_NAME, (unsigned long)"audio_type_parse");

    bytes = audio_type_status->period_bytes;

    ALOGV("Start thread loop for android audio data parse! data = %p, bytes = %d, in = %p\n",
          data, bytes, audio_type_status->in);

    if (audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI) {
        eMixerAudioResampleSource resample_src = RESAMPLE_FROM_FRHDMIRX;
        if (check_chip_name("t3", 2, audio_type_status->mixer_handle) &&
            get_hdmiin_audio_mode(audio_type_status->mixer_handle) == HDMIIN_MODE_I2S) {
            resample_src = RESAMPLE_FROM_TDMIN_B;
        }
        cur_samplerate = set_resample_source(audio_type_status->mixer_handle, resample_src);
    } else if (audio_type_status->input_dev == AUDIO_DEVICE_IN_SPDIF) {
        cur_samplerate = set_resample_source(audio_type_status->mixer_handle, RESAMPLE_FROM_SPDIFIN);
    }

    while (audio_type_status->running_flag) {
        if (audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI) {
            cur_audio_packet = get_hdmiin_audio_packet(audio_type_status->mixer_handle);
            cur_samplerate = get_hdmiin_samplerate(audio_type_status->mixer_handle);
            audio_type_status->audio_samplerate = audio_transer_samplerate(cur_samplerate);
        } else if (audio_type_status->input_dev == AUDIO_DEVICE_IN_SPDIF) {
            cur_samplerate = get_spdifin_samplerate(audio_type_status->mixer_handle);
        } else if (audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI_ARC) {
            cur_samplerate = -1;//temp code
        }

        if (cur_samplerate == -1)
            cur_samplerate = HW_RESAMPLE_48K;

        /*check hdmiin audio input sr and reset hw resample*/
        if (cur_samplerate != HW_RESAMPLE_DISABLE &&
                cur_samplerate != last_cur_samplerate &&
                audio_type_status->audio_type == LPCM) {
            enable_HW_resample(audio_type_status->mixer_handle, cur_samplerate);
            ALOGD("Reset hdmiin/spdifin audio resample sr from %d to %d\n",
                last_cur_samplerate, cur_samplerate);
            last_cur_samplerate = cur_samplerate;
        }

        if (audio_type_status->soft_parser && audio_type_status->in) {
            if (audio_type_status->hdmi_packet != cur_audio_packet) {
                ALOGI("---HDMI Format Switch [audio_packet pre:%d->cur:%d]",
                    audio_type_status->hdmi_packet, cur_audio_packet);
                reconfig_pcm_by_packet_type(audio_type_status, cur_audio_packet);
                audio_type_status->hdmi_packet = cur_audio_packet;
            }

            //sw audio format detection.
            if (cur_samplerate == HW_RESAMPLE_192K) {
                read_bytes = bytes * 4;
                if (read_bytes > audio_type_status->period_bytes) {
                    audio_type_status->parse_buffer = aml_audio_realloc(audio_type_status->parse_buffer, read_bytes + 16);
                    audio_type_status->period_bytes = read_bytes;
                }
            } else {
                read_bytes = bytes;
            }
            /* non-blocking read prevent 10s stuck when switching TV sources */
            nodata_count = 0;
            read_back = 0;
            while (read_back < read_bytes && audio_type_status->running_flag) {
                ret = pcm_read(audio_type_status->in,
                        audio_type_status->parse_buffer + read_back + 3, read_bytes - read_back);
                if (ret >= 0) {
                    nodata_count = 0;
                    if (read_back + ret > audio_type_status->period_bytes) {
                        AM_LOGW("overflow buffer read_cnt:%d ret:%d period_bytes:%d need:%d", read_back, ret, audio_type_status->period_bytes, read_bytes);
                        read_back = audio_type_status->period_bytes;
                    } else {
                        read_back += ret;
                    }
                } else if (ret != -EAGAIN) {
                    ALOGD("%s:%d, pcm_read fail, ret:%#x, error info:%s",
                        __func__, __LINE__, ret, strerror(errno));
                    memset(audio_type_status->parse_buffer + 3, 0, read_bytes);
                    break;
                } else {
                    if (nodata_count >= WAIT_COUNT_MAX) {
                        nodata_count = 0;
                        AM_LOGW("aml_alsa_input_read immediate return");
                        memset(audio_type_status->parse_buffer + 3, 0, bytes);
                        break;
                    }
                    nodata_count++;
                    usleep((read_bytes - read_back) * 1000 / 4 / 48 / 2);
                }
            }

            if (ret >= 0) {
                audio_type_status->cur_audio_type = audio_type_parse(audio_type_status->parse_buffer,
                                                    read_bytes, &(audio_type_status->package_size),
                                                    &(audio_type_status->audio_ch_mask));
                //ALOGD("cur_audio_type=%d\n", audio_type_status->cur_audio_type);
                memcpy(audio_type_status->parse_buffer, audio_type_status->parse_buffer + read_bytes, 3);
                update_audio_type(audio_type_status, read_bytes, cur_samplerate);
            } else {
                usleep(10 * 1000);
            }
        } else {
            if (auge_chip || txlx_chip) {
                // get audio format from hw.
                if (audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI) {
                    audio_type_status->cur_audio_type = hdmiin_audio_format_detection(audio_type_status->mixer_handle);
                } else if (audio_type_status->input_dev == AUDIO_DEVICE_IN_SPDIF) {
                    audio_type_status->cur_audio_type = spdifin_audio_format_detection(audio_type_status->mixer_handle);
                }

                if (audio_type_status->audio_type != LPCM && audio_type_status->cur_audio_type == LPCM) {
                    enable_HW_resample(audio_type_status->mixer_handle, cur_samplerate);
                } else if (audio_type_status->audio_type == LPCM && audio_type_status->cur_audio_type != LPCM){
                    ALOGV("Raw data found: type(%d)\n", audio_type_status->cur_audio_type);
                    enable_HW_resample(audio_type_status->mixer_handle, HW_RESAMPLE_DISABLE);
                }

                audio_type_status->audio_type = audio_type_status->cur_audio_type;
            } else if (audio_type_status->input_dev == AUDIO_DEVICE_IN_HDMI) {
                hdmiin_audio_packet_t audio_packet = get_hdmiin_audio_packet(audio_type_status->mixer_handle);
                if (audio_packet == AUDIO_PACKET_HBR) {
                    enable_HW_resample(audio_type_status->mixer_handle, HW_RESAMPLE_DISABLE);
                }
            }
            usleep(10 * 1000);
            //ALOGE("fail to read bytes = %d\n", bytes);
        }
    }

    audio_type_parse_release(audio_type_status);

    ALOGI("Exit thread loop for audio type parse!\n");
    return ((void *) 0);
}

int audio_parse_get_audio_samplerate(audio_type_parse_t *status)
{
    if (!status) {
        ALOGE("NULL pointer of audio_type_parse_t, return default samperate:48000\n");
        return 48000;
    }
    return status->audio_samplerate;
}

int creat_pthread_for_audio_type_parse(
                     pthread_t *audio_type_parse_ThreadID,
                     void **status,
                     struct aml_mixer_handle *mixer,
                     audio_devices_t input_dev)
 {
    pthread_attr_t attr;
    struct sched_param param;
    audio_type_parse_t *audio_type_status = NULL;
    int ret;

    if (*status) {
        ALOGE("Aml TV audio format check is exist!");
        return -1;
    }

    audio_type_status = (audio_type_parse_t*) aml_audio_malloc(sizeof(audio_type_parse_t));
    if (NULL == audio_type_status) {
        ALOGE("%s, no memory\n", __FUNCTION__);
        return -1;
    }

    memset(audio_type_status, 0, sizeof(audio_type_parse_t));
    audio_type_status->running_flag = 1;
    audio_type_status->audio_type = LPCM;
    audio_type_status->audio_ch_mask = AUDIO_CHANNEL_OUT_STEREO;
    audio_type_status->mixer_handle = mixer;
    audio_type_status->input_dev = input_dev;

    pthread_attr_init(&attr);
    pthread_attr_setschedpolicy(&attr, SCHED_RR);
    param.sched_priority = 50;//sched_get_priority_max(SCHED_RR);
    pthread_attr_setschedparam(&attr, &param);
    ret = pthread_create(audio_type_parse_ThreadID, &attr,
                         &audio_type_parse_threadloop, audio_type_status);
    pthread_attr_destroy(&attr);
    if (ret != 0) {
        ALOGE("%s, Create thread fail!\n", __FUNCTION__);
        aml_audio_free(audio_type_status);
        return -1;
    }

    ALOGI("Creat thread ID: %lu! audio_type_status: %p\n", *audio_type_parse_ThreadID, audio_type_status);
    *status = audio_type_status;
    return 0;
}

void exit_pthread_for_audio_type_parse(
    pthread_t audio_type_parse_ThreadID,
    void **status)
{
    audio_type_parse_t *audio_type_status = (audio_type_parse_t *)(*status);
    audio_type_status->running_flag = 0;
    pthread_join(audio_type_parse_ThreadID, NULL);
    aml_audio_free(audio_type_status);
    *status = NULL;
    ALOGI("Exit parse thread,thread ID: %ld!\n", audio_type_parse_ThreadID);
    return;
}

/*
 *@brief convert the audio type to android audio format
 */
audio_format_t audio_type_convert_to_android_audio_format_t(int codec_type)
{
    switch (codec_type) {
    case AC3:
        return AUDIO_FORMAT_AC3;
    case EAC3:
        return AUDIO_FORMAT_E_AC3;
    case MAT:
        return AUDIO_FORMAT_MAT;
    case DTS:
    case DTSCD:
        return AUDIO_FORMAT_DTS;
    case DTSHD:
        return AUDIO_FORMAT_DTS_HD;
    case TRUEHD:
        return AUDIO_FORMAT_DOLBY_TRUEHD;
    case LPCM:
        return AUDIO_FORMAT_PCM_16_BIT;
    default:
        AM_LOGW("invalid codec_type:%d, return PCM.", codec_type);
        return AUDIO_FORMAT_PCM_16_BIT;
    }
}

/*
 *@brief convert android audio format to the audio type
 */
int android_audio_format_t_convert_to_andio_type(audio_format_t format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
        return AC3;
    case AUDIO_FORMAT_E_AC3:
        return EAC3;
    case AUDIO_FORMAT_MAT:
        return MAT;
    case AUDIO_FORMAT_DTS:
        return  DTS;//DTSCD;
    case AUDIO_FORMAT_DTS_HD:
        return DTSHD;
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        return TRUEHD;
    case AUDIO_FORMAT_PCM:
        return LPCM;
    default:
        return LPCM;
    }
}

char* audio_type_convert_to_string(int s32AudioType)
{
    switch (s32AudioType) {
    case AC3:
        return "AC3";
    case EAC3:
        return "EAC3";
    case DTS:
        return "DTS";
    case DTSCD:
        return "DTSCD";
    case DTSHD:
        return "DTSHD";
    case TRUEHD:
        return "TRUEHD";
    case LPCM:
        return "LPCM";
    case MAT:
        return "MAT";
    default:
        return "UNKNOWN";
    }
}

int audio_parse_get_audio_type_direct(audio_type_parse_t *status)
{
    if (!status) {
        ALOGE("NULL pointer of audio_type_parse_t\n");
        return -1;
    }
    return status->audio_type;
}

audio_format_t audio_parse_get_audio_type(audio_type_parse_t *status)
{
    if (!status) {
        ALOGE("NULL pointer of audio_type_parse_t\n");
        return AUDIO_FORMAT_INVALID;
    }
    return audio_type_convert_to_android_audio_format_t(status->audio_type);
}

audio_channel_mask_t audio_parse_get_audio_channel_mask(audio_type_parse_t *status)
{
    if (!status) {
        ALOGE("NULL pointer of audio_type_parse_t, return AUDIO_CHANNEL_OUT_STEREO\n");
        return AUDIO_CHANNEL_OUT_STEREO;
    }
    return status->audio_ch_mask;
}
