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

#ifndef HUITONG_AUDIO_H
#define HUITONG_AUDIO_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <sys/poll.h>
#include <cutils/sockets.h>

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <hardware/audio_effect.h>
#include <time.h>
#include <utils/Timers.h>

////////////////////////// switch of huitong //////////////////////////////////////////////////
#define ENABLE_HUITONG 0



////////////////////////// bowdlerized huitong_audio_hw.h /////////////////////////////////////
/* ALSA cards for AML */
#define CARD_AMLOGIC_USB 1
/* ALSA ports for AML */
//#define PORT_MM 0    // this macro is different between amlogic and huitong
/* number of frames per period */
#define DEFAULT_WFD_PERIOD_SIZE  256
#define DEFAULT_CAPTURE_PERIOD_SIZE  1024
#define MIXER_XML_PATH "/system/etc/mixer_paths.xml"

extern struct pcm_config pcm_config_vg;


////////////////////////// huitong_audio_hw.c /////////////////////////////////////////////////
#include <linux/input.h>
#include <linux/hidraw.h>
//#include <huitong_audio.h>
#include "def.h"
#include "ti_audio.h"
#include "mainSBC.h"
#include "dvi_adpcm.h"
#include "opus.h"
#include "typedef.h"
#include "bvcommon.h"
#include "bv32cnst.h"
#include "bv32strct.h"
#include "bv32.h"
#include "bitpack.h"

#include "huitong_log.h"

#define BV32_FRAME_LEN      80

#define MAX_HIDRAW_ID 20

#define HUITONG_TI_VID 0x000D
#define HUITONG_TI_PID 0x0001

#define HUITONG_BCM_VID 0x000F
#define HUITONG_BCM_PID_20734 0x0001
#define HUITONG_BCM_PID_20735 0x0002


#define HUITONG_DIALOG_VID 0x2ba5
#define HUITONG_DIALOG_PID 0x8082

#define HUITONG_NORDIC_VID 0x1915
#define HUITONG_NORDIC_PID 0x0001

enum {
    RC_PLATFORM_UNKOWN,
    RC_PLATFORM_TI,
    RC_PLATFORM_BCM,
    RC_PLATFORM_DIALOG,
    RC_PLATFORM_NORDIC
};

#define REPORT_ID 0x05
#define REPORT_ID_NORDIC_BV32			0x01
#define REPORT_ID_NORDIC_ADPCM			0x02
#define REPORT_ID_NORDIC_OPUS			0x03


#define GATT_PDU_LENGTH 20
#define HIDRAW_PDU_LENGTH (1 + GATT_PDU_LENGTH) //the first byte is report id added by stack

static int part_index = 0;
static int total_lenth = 0;
static int receive_index = 0;
#define ADPCM_DATA_PART_NUM 5 //five parts as a frame
static unsigned char ADPCM_Data_Frame[ADPCM_DATA_PART_NUM*GATT_PDU_LENGTH];

static OpusDecoder *st;
static struct BV32_Decoder_State bv32_st;


static short decode_buf[1024];

static int hidraw_fd = -1;

static int huitong_rc_platform = RC_PLATFORM_UNKOWN;

///////////////////// function prototype definition used in huitong_audio_hw.c ////////////////
uint32_t huitong_in_get_sample_rate(const struct audio_stream *stream);
int huitong_in_set_sample_rate(struct audio_stream *stream, uint32_t rate);
size_t huitong_in_get_buffer_size(const struct audio_stream *stream);
audio_channel_mask_t huitong_in_get_channels(const struct audio_stream *stream);
audio_format_t huitong_in_get_format(const struct audio_stream *stream);
int huitong_in_set_format(struct audio_stream *stream, audio_format_t format);
int huitong_in_standby(struct audio_stream *stream);
int huitong_in_dump(const struct audio_stream *stream, int fd);
int huitong_in_set_parameters(struct audio_stream *stream, const char *kvpairs);
char * huitong_in_get_parameters(const struct audio_stream *stream, const char *keys);
int huitong_in_set_gain(struct audio_stream_in *stream, float gain);
ssize_t huitong_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes);
uint32_t huitong_in_get_input_frames_lost(struct audio_stream_in *stream);
int get_hidraw_device_fd();

#endif

