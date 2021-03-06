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

#ifndef __AUDIO_FORMAT_PARSE_H__
#define __AUDIO_FORMAT_PARSE_H__

#include <system/audio.h>
#include <tinyalsa/asoundlib.h>
#include <aml_alsa_mixer.h>
#include "aml_ac3_parser.h"

enum audio_type {
    LPCM = 0,
    AC3,
    EAC3,
    DTS,
    DTSHD,
    MAT,
    PAUSE,
    TRUEHD,
    DTSCD,
    MUTE,
};


enum audio_sample {
    HW_NONE = 0,
    HW_32K,
    HW_44K,
    HW_48K,
    HW_88K,
    HW_96K,
    HW_176K,
    HW_192K,
};

typedef enum hdmiin_audio_packet {
    AUDIO_PACKET_NONE,
    AUDIO_PACKET_AUDS,
    AUDIO_PACKET_OBA,
    AUDIO_PACKET_DST,
    AUDIO_PACKET_HBR,
    AUDIO_PACKET_OBM,
    AUDIO_PACKET_MAS
} hdmiin_audio_packet_t;

#define PARSER_DEFAULT_PERIOD_SIZE  (1024)

/*Period of data burst in IEC60958 frames*/
//#define AC3_PERIOD_SIZE  (6144)
//#define EAC3_PERIOD_SIZE (24576)
//#define MAT_PERIOD_SIZE  (61440)

#define DTS1_PERIOD_SIZE (2048)
#define DTS2_PERIOD_SIZE (4096)
#define DTS3_PERIOD_SIZE (8192)
/*min DTSHD Period 2048; max DTSHD Period 65536*/
#define DTSHD_PERIOD_SIZE   (512*8)
#define DTSHD_PERIOD_SIZE_1 (512*32)
#define DTSHD_PERIOD_SIZE_2 (512*48)
#define DTSCD_VALID_COUNT   (2)

typedef struct audio_type_parse {
    struct pcm_config config_in;
    struct pcm *in;
    struct aml_mixer_handle *mixer_handle;
    unsigned int card;
    unsigned int device;
    unsigned int flags;
    int soft_parser;
    hdmiin_audio_packet_t hdmi_packet;

    int period_bytes;
    char *parse_buffer;

    int audio_type;
    int cur_audio_type;

    audio_channel_mask_t audio_ch_mask;

    int read_bytes;
    int package_size;
    int audio_samplerate;

    int running_flag;
    audio_devices_t input_dev;
} audio_type_parse_t;

int creat_pthread_for_audio_type_parse(
    pthread_t *audio_type_parse_ThreadID,
                     void **status,
                     struct aml_mixer_handle *mixer,
                     audio_devices_t input_dev);
void exit_pthread_for_audio_type_parse(
    pthread_t audio_type_parse_ThreadID,
    void **status);

/*
 *@brief convert the audio type to android audio format
 */
audio_format_t audio_type_convert_to_android_audio_format_t(int codec_type);

/*
 *@brief convert the audio type to string format
 */
char* audio_type_convert_to_string(int s32AudioType);

/*
 *@brief convert android audio format to the audio type
 */
int android_audio_format_t_convert_to_andio_type(audio_format_t format);
/*
 *@brief get current android audio fromat from audio parser thread
 */
audio_format_t audio_parse_get_audio_type(audio_type_parse_t *status);
/*
 *@brief get current audio channel mask from audio parser thread
 */
audio_channel_mask_t audio_parse_get_audio_channel_mask(audio_type_parse_t *status);
/*
 *@brief gget current audio fromat from audio parser thread
 */
int audio_parse_get_audio_type_direct(audio_type_parse_t *status);
/*
 *@brief parse the channels in the undecoded DTS stream
 */
int get_dts_stream_channels(const char *buffer, size_t bytes);
/*
 *@brief gget current audio type from buffer data
 */
int audio_type_parse(void *buffer, size_t bytes, int *package_size, audio_channel_mask_t *cur_ch_mask);

int audio_parse_get_audio_samplerate(audio_type_parse_t *status);

#endif
