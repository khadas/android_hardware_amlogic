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



#define LOG_TAG "audio_hw_utils"
//#define LOG_NDEBUG 0
#define __USE_GNU

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <utils/Timers.h>
#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <linux/ioctl.h>
#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>

#define ATRACE_TAG ATRACE_TAG_AUDIO
#include <cutils/trace.h>

#include "audio_hw_utils.h"

#include "audio_hwsync.h"
#include "amlAudioMixer.h"
#include <audio_utils/primitives.h>
#include "alsa_device_parser.h"
#include "a2dp_hal.h"
#include "aml_audio_avsync_table.h"

#ifdef LOG_NDEBUG_FUNCTION
#define LOGFUNC(...) ((void)0)
#else
#define LOGFUNC(...) (ALOGD(__VA_ARGS__))
#endif

//DRC Mode
#define DDPI_UDC_COMP_LINE 2
#define DRC_MODE_BIT  0
#define DRC_HIGH_CUT_BIT 3
#define DRC_LOW_BST_BIT 16
static const char *str_compmode[] = {"custom mode, analog dialnorm","custom mode, digital dialnorm",
                            "line out mode","RF remod mode"};


#define DD_MUTE_FRAME_SIZE 1536
#define DDP_MUTE_FRAME_SIZE 6144
// add array of dd/ddp mute frame for mute function
static const unsigned int muted_frame_dd[DD_MUTE_FRAME_SIZE] = {
    0x4e1ff872, 0x50000001, 0xcdc80b77, 0xe1ff2430, 0x9200fcf4, 0x5578fc02, 0x187f6186, 0x9f3eceaf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf7ff7cf9, 0x7cf93abe, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xdffcf3e7, 0xf3e7eaf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x7ff3cf9f, 0xcf9fabe7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,
    0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xffce3e7d, 0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xff3af9f7, 0x3891e53e, 0x89102244, 0x3fa0fd00, 0xc78f1de3, 0xdddd1ddd,     0xdc00,          0,          0,          0,
             0,          0, 0xbbbb003b, 0x6db6b6db, 0xcd6bdbe7, 0xb5af5ad6, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,     0xe7c0, 0xbc780003, 0xbbbbf1e3, 0x8000bbbb,          0,          0,          0,
             0,          0,  0x7770000, 0xdb6d7776, 0x7cf9b6db, 0x5ad6ad6b, 0xe7cfb5f3, 0x7cf99f3e, 0xcf9ff3e7, 0xfbf03e7c,   0x67013e, 0x3c778f1e, 0x77707777,          0,          0,          0,
             0,          0,          0, 0xeedbeeee, 0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,          0, 0xe3c70ef1, 0xeeee8eee,     0xee00,          0,          0,
             0,          0,          0, 0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,     0xf3e0, 0xde3c0001, 0xdddd78f1, 0xc000dddd,          0,          0,
             0,          0,          0,  0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c009f3e,   0x3b0000,     0xc07e,   0x7f01fa, 0xc78f403b, 0xbbbb1e3b,
        0xbbb8,          0,          0,          0,          0,          0, 0x77770000, 0xb6db776d, 0xcf9a6db7, 0xad6bd6b5, 0x7cf95f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x8000e7cf,  0x7780000, 0xc777f1e3,
    0x77007777,          0,          0,          0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f73e7c, 0x7c00e002, 0x3c78cf1e,
    0xeeeeeeee,     0xe000,          0,          0,          0,          0,          0, 0xdddd01dd, 0x6db6b6db, 0x6b5adf3e, 0xad7cd6b5, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,     0x3e00, 0xe3c7001d,
    0xdddd8f1d,     0xdddc,          0,          0,          0,          0,          0, 0x3bbb0000, 0xdb6dbbb6, 0xe7cdb6db, 0xd6b56b5a, 0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xc000f3e7,  0x3bc0000,
    0xe3bb78f1, 0xbb80bbbb,          0,          0,          0,          0,          0,    0x70000, 0x76db7777, 0xdb7c6db6, 0x6b5af9ad, 0xf3e7d6b5, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf89f3e,          0,
    0xfc007780, 0xf4000003, 0x778ffe80, 0x77771e3c, 0x70007777,          0,          0,          0,          0,          0,   0xee0000, 0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f00e7cf,    0xe0000, 0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,          0,          0,          0, 0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e,
    0xcf9ff3e7, 0xf9f33e7c,  0x4f8efc0, 0x3c78019e, 0xddddf1dd,     0xddc0,          0,          0,          0,          0,          0, 0xbbbb0003, 0xb6dbbb6d, 0x7cd66dbe, 0x6b5ab5ad, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7,     0x3e7c, 0x3bc70000, 0x3bbb8f1e, 0xb800bbbb,          0,          0,          0,          0,          0,   0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f,
    0xe7cff9f3, 0x7cf99f3e, 0xcf80f3e7,    0x70000, 0xe3c778f1, 0x77777777,          0,          0,          0,          0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad,
    0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,     0xf000,  0x1f800ef,  0x8380000, 0xea4402ad, 0xfd8909ba,  0x7559e6c, 0xf4008783, 0x778ffe80, 0x77771e3c, 0x70007777,          0,          0,          0,
             0,          0,   0xee0000, 0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7, 0xf9f33e7c, 0x9f00e7cf,    0xe0000, 0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,
             0,          0,          0, 0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,  0x4f8efc0, 0x3c78019e, 0xddddf1dd,     0xddc0,          0,          0,
             0,          0,          0, 0xbbbb0003, 0xb6dbbb6d, 0x7cd66dbe, 0x6b5ab5ad, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,     0x3e7c, 0x3bc70000, 0x3bbb8f1e, 0xb800bbbb,          0,          0,
             0,          0,          0,   0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f, 0xe7cff9f3, 0x7cf99f3e, 0xcf80f3e7,    0x70000, 0xe3c778f1, 0x77777777,          0,          0,
             0,          0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,     0xf000,  0x1f800ef,  0x9600000, 0x678702c0, 0x7755b8ed,
    0x4cbcd453, 0xab7696aa, 0x4b47bdb4, 0xebe40734, 0x511930e9, 0x3ea40092, 0x684037ec, 0x9db490bc, 0x96bbccc3,  0xf17adba, 0xce80f164, 0x90c75984, 0x1598a7da, 0x20c19a22, 0x7202ee1d, 0xc1106588,
    0xd9bbc22c, 0x77b2c1c6, 0x56c22a12, 0x36a3b0d1, 0xfe80f400, 0x1e3c778f, 0x77777777,     0x7000,          0,          0,          0,          0,          0, 0xeeee00ee, 0xb6dbdb6d, 0x35ad6f9f,
    0xd6be6b5a, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,     0x9f00, 0xf1e3000e, 0xeeeec78e,     0xeeee,          0,          0,          0,          0,          0, 0x1ddd0000, 0x6db6dddb, 0xf3e6db6d,
    0x6b5ab5ad, 0x9f3ed7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xefc0f9f3,  0x19e04f8, 0xf1dd3c78, 0xddc0dddd,          0,          0,          0,          0,          0,    0x30000, 0xbb6dbbbb, 0x6dbeb6db,
    0xb5ad7cd6, 0xf9f36b5a, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,          0, 0x8f1e3bc7, 0xbbbb3bbb,     0xb800,          0,          0,          0,          0,          0, 0x77770077, 0xdb6d6db6,
    0x9ad6b7cf, 0x6b5fb5ad, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,     0xcf80, 0x78f10007, 0x7777e3c7,     0x7777,          0,          0,          0,          0,          0,  0xeee0000, 0xb6dbeeed,
    0xf9f36db6, 0xb5ad5ad6, 0xcf9f6be7, 0xf9f33e7c, 0x9f3ee7cf, 0xf0007cf9,   0xef0000,      0x1f8,   0x1f095c, 0x152cc7df, 0xe0a1af0b, 0xbd3f4b74, 0x71b859e9, 0xb4da9f21, 0x515fd7d9, 0xfe05c0db,
    0x819022dd, 0x96c4b6a1, 0xfc593cef, 0x7d127a7c, 0xcfac240e, 0xb6ec0a66, 0xed96e243, 0x3e5e6c62, 0x5c0a6d81, 0x1158a269, 0x1d0ecbd5, 0x39e9a681, 0x2ea4f735, 0xb2077aac,   0xfef3f4, 0x8f1e8077,
    0x77773c77,     0x7770,          0,          0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f, 0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,     0xcf9f,  0xef10000,
    0x8eeee3c7, 0xee00eeee,          0,          0,          0,          0,          0,   0x1d0000, 0xdb6ddddd, 0x6df3b6db, 0xad6be6b5, 0xcf9f5ad7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3ef7cf9, 0xf801c004,
    0x78f19e3c, 0xdddddddd,     0xc000,          0,          0,          0,          0,          0, 0xbbbb03bb, 0xdb6d6db6, 0xd6b5be7c, 0x5af9ad6b, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,     0x7c00,
    0xc78f003b, 0xbbbb1e3b,     0xbbb8,          0,          0,          0,          0,          0, 0x77770000, 0xb6db776d, 0xcf9a6db7, 0xad6bd6b5, 0x7cf95f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x8000e7cf,
     0x7780000, 0xc777f1e3, 0x77007777,          0,          0,          0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f03e7c,
             0, 0xf9e9ef00,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
};

static const unsigned int muted_frame_ddp[DDP_MUTE_FRAME_SIZE] = {
    0x4e1ff872,  0xbe40715,  0x5f10b77, 0xfffa3f67,   0x484900,    0x40000,    0x80000, 0x6186e100, 0xff3a1861, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0xfceae7df, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0xf3ab9f7f, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,
    0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xceaf7dff, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0x3abef7ff, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0xeb94dffc, 0x137ef8e0, 0xc3c7c280, 0x3bbb8f1e, 0xb800bbbb,          0,          0,          0,
             0,          0,   0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f, 0xe7cff9f3, 0x7cf99f3e, 0xcfb9f3e7, 0x27d33873, 0xe3c778f1, 0x77777777,          0,          0,          0,
             0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xf21bf0d9, 0x1e3c876f, 0xeeee78ee,     0xeee0,          0,          0,
             0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0xe4e19f3e, 0x4de3cc9f, 0x1dddc78f, 0xdc00dddd,          0,          0,
             0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7c3f9f3, 0x6e1d67c8, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,          0,
             0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xffc0f804, 0x90008077, 0x614009bf, 0xc78f61e3, 0xdddd1ddd,
        0xdc00,          0,          0,          0,          0,          0, 0xbbbb003b, 0x6db6b6db, 0xcd6bdbe7, 0xb5af5ad6, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9c39e7dc, 0xbc7893e9, 0xbbbbf1e3,
    0x8000bbbb,          0,          0,          0,          0,          0,  0x7770000, 0xdb6d7776, 0x7cf9b6db, 0x5ad6ad6b, 0xe7cfb5f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf86c3e7c, 0xc3b7f90d, 0x3c778f1e,
    0x77707777,          0,          0,          0,          0,          0,          0, 0xeedbeeee, 0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xe64f7270, 0xe3c7a6f1,
    0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xb3e4f3e1, 0xde3c370e,
    0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c029f3e, 0x403b7fe0,
    0xfa00c800,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,    0x20000, 0x50186fd8, 0xe3c778f1, 0x77777777,          0,          0,
             0,          0,          0,          0, 0xeeed0eee, 0x6db6b6db, 0x5ad6f9f3, 0x6be7b5ad, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,  0xe64f727, 0x1e3cfa6f, 0xeeee78ee,     0xeee0,          0,
             0,          0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x1b3e9f3e, 0xede34370, 0x1dddc78f, 0xdc00dddd,          0,
             0,          0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7dcf9f3, 0x93e99c39, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,
             0,          0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xf90df86c, 0x8f1ec3b7, 0x77773c77,     0x7770,
             0,          0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f, 0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x9fcf9f,  0xef2f810, 0x37ec0001, 0x3c78280c,
    0xbbbbf1e3, 0x8000bbbb,          0,          0,          0,          0,          0,  0x7770000, 0xdb6d7776, 0x7cf9b6db, 0x5ad6ad6b, 0xe7cfb5f3, 0x7cf99f3e, 0xcf9ff3e7, 0xfb933e7c, 0x7d378732,
    0x3c778f1e, 0x77707777,          0,          0,          0,          0,          0,          0, 0xeedbeeee, 0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x21b80d9f,
    0xe3c776f1, 0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0x4e1cf3ee,
    0xde3cc9f4, 0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c369f3e,
    0xe1db7c86, 0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,          0, 0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0xfc08804f,      0x779, 0x14069bf6, 0x78f11e3c, 0xdddddddd,     0xc000,          0,          0,          0,          0,          0, 0xbbbb03bb, 0xdb6d6db6, 0xd6b5be7c, 0x5af9ad6b, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xc3997dc9, 0xc78f3e9b, 0xbbbb1e3b,     0xbbb8,          0,          0,          0,          0,          0, 0x77770000, 0xb6db776d, 0xcf9a6db7, 0xad6bd6b5, 0x7cf95f3e,
    0xcf9ff3e7, 0xf9f33e7c, 0x86cfe7cf, 0x3b7890dc, 0xc777f1e3, 0x77007777,          0,          0,          0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f73e7c, 0x64fa270e, 0x3c786f1e, 0xeeeeeeee,     0xe000,          0,          0,          0,          0,          0, 0xdddd01dd, 0x6db6b6db, 0x6b5adf3e, 0xad7cd6b5,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x3e433e1b, 0xe3c770ed, 0xdddd8f1d,     0xdddc,          0,          0,          0,          0,          0, 0x3bbb0000, 0xdb6dbbb6, 0xe7cdb6db, 0xd6b56b5a,
    0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xc027f3e7,  0x3bcfe04, 0x4dfb8000,  0xf1e0a03, 0xeeee3c78, 0xe000eeee,          0,          0,          0,          0,          0,  0x1dd0000, 0xb6dbdddd,
    0xdf3e6db6, 0xd6b56b5a, 0xf9f3ad7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3ee4cf9f, 0x9f4de1cc, 0x8f1de3c7, 0xdddcdddd,          0,          0,          0,          0,          0,          0, 0xbbb63bbb,
    0xb6dbdb6d, 0x6b5ae7cd, 0xaf9fd6b5, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0xc86ec367, 0x78f11dbc, 0xbbbbe3bb,     0xbb80,          0,          0,          0,          0,          0, 0x77770007,
    0x6db676db, 0xf9addb7c, 0xd6b56b5a, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0x93877cfb, 0x378f327d, 0x77771e3c, 0x70007777,          0,          0,          0,          0,          0,   0xee0000,
    0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7, 0xf9f33e7c, 0x9f0de7cf, 0xb8769f21, 0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,          0,          0,          0,
    0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0xff02e013,      0x1de,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,    0x30000,  0x8a1e6a4,          0,          0,          0,          0,          0,

};

static const unsigned int muted_frame_atmos[DDP_MUTE_FRAME_SIZE] = {
    0x4e1ff872,  0xbe40715,  0x5f10b77, 0xfffa3f67, 0x41014900, 0x20000f01, 0x10000000, 0x23840000, 0x18610186, 0xeaf987fc, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xabe77ff3, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,
    0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7dcf9f, 0xaf9fffce, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,
    0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f73e7c, 0xbe7cff3a, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7dff9f3, 0xf9f3fcea, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,
    0xf9f33e7c, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c, 0x9f7fe7cf, 0x53e3f3ae,   0x7f87fe, 0x1e3c80ef, 0xeeee78ee,     0xeee0,          0,          0,
             0,          0,          0, 0xdddd0001, 0xdb6dddb6, 0x3e6bb6df, 0xb5ad5ad6, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,  0x23f9f3e, 0x1de3e040, 0x1dddc78f, 0xdc00dddd,          0,          0,
             0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7c0f9f3,    0x30400, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,          0,
             0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x8000f801, 0x8f1e4077, 0x77773c77,     0x7770,          0,
             0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f, 0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x30cf9f,  0xef10008, 0x8eeee3c7, 0xee00eeee,          0,
             0,          0,          0,          0,   0x1d0000, 0xdb6ddddd, 0x6df3b6db, 0xad6be6b5, 0xcf9f5ad7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e07cf9,  0xb0657fb,  0x3ff1e40, 0xc077003f, 0x3c778f1e,
    0x77707777,          0,          0,          0,          0,          0,          0, 0xeedbeeee, 0xdb6f6db6, 0xad6b9f35, 0xbe7c5ad6, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0xf020011f, 0xe3c70ef1,
    0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e,  0x200f3e0, 0xde3c0001,
    0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c009f3e, 0x203bc000,
    0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,          0, 0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,    0x48018,
    0xf1e30778, 0x7777c777,     0x7700,          0,          0,          0,          0,          0, 0xeeee000e, 0xdb6dedb6, 0xf35ab6f9, 0xad6bd6b5, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x2bfdf9f0,
     0xf208583, 0x583803e8,  0x86900ce,     0x81c0, 0x5e000c04, 0x2000b83f, 0x8443481f, 0xa27fe000, 0x40ffbe80, 0x81df3f00,  0x3fc0201,  0xa100402, 0x41501f80,  0x20a81f4, 0x20108400, 0x1f900074,
     0x384008f, 0xa9a90161, 0xa9a9a9a9, 0xa9a9a9a9, 0xa9a9a9a9,     0xa800, 0x60180001,     0x5954,    0x90000, 0x88dad018,    0xc0000,     0x1600,  0x3050000,     0x8000,          0,          0,
             0,          0,          0, 0x4492000c,     0x8000,          0,          0,          0,          0,          0,          0, 0xb2d00030,          0,          0,          0,          0,
             0,          0,     0x182c,          0, 0x61750002, 0x21802d12, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd,
    0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xcdcdcdcd, 0xffc0cd00, 0x1de30ff0, 0x1dddc78f, 0xdc00dddd,          0,
             0,          0,          0,          0,   0x3b0000, 0xb6dbbbbb, 0xdbe76db6, 0x5ad6cd6b, 0x9f3eb5af, 0xf3e77cf9, 0x3e7ccf9f, 0xe7c0f9f3,  0x80347fc, 0xf1e3bc78, 0xbbbbbbbb,     0x8000,
             0,          0,          0,          0,          0, 0x77760777, 0xb6dbdb6d, 0xad6b7cf9, 0xb5f35ad6, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f, 0x8000f800, 0x8f1e0077, 0x77773c77,     0x7770,
             0,          0,          0,          0,          0, 0xeeee0000, 0x6db6eedb, 0x9f35db6f, 0x5ad6ad6b, 0xf9f3be7c, 0x9f3ee7cf, 0xf3e77cf9,   0x30cf9f,  0xef10008, 0x8eeee3c7, 0xee00eeee,
             0,          0,          0,          0,          0,   0x1d0000, 0xdb6ddddd, 0x6df3b6db, 0xad6be6b5, 0xcf9f5ad7, 0xf9f33e7c, 0x9f3ee7cf, 0xf3e07cf9,  0x1010600, 0x78f1de3c, 0xdddddddd,
        0xc000,          0,          0,          0,          0,          0, 0xbbbb03bb, 0xdb6d6db6, 0xd6b5be7c, 0x5af9ad6b, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf, 0xff617c0a, 0xc80060c3,  0x7f87fe0,
    0xe3c70ef1, 0xeeee8eee,     0xee00,          0,          0,          0,          0,          0, 0xdddd001d, 0xb6dbdb6d, 0xe6b56df3, 0x5ad7ad6b, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf99f3e, 0x23fef3e0,
    0xde3c0401, 0xdddd78f1, 0xc000dddd,          0,          0,          0,          0,          0,  0x3bb0000, 0x6db6bbbb, 0xbe7cdb6d, 0xad6bd6b5, 0xf3e75af9, 0x3e7ccf9f, 0xe7cff9f3, 0x7c009f3e,
      0x3b4000, 0x1e3bc78f, 0xbbb8bbbb,          0,          0,          0,          0,          0,          0, 0x776d7777, 0x6db7b6db, 0xd6b5cf9a, 0x5f3ead6b, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3,
       0x48018, 0xf1e30778, 0x7777c777,     0x7700,          0,          0,          0,          0,          0, 0xeeee000e, 0xdb6dedb6, 0xf35ab6f9, 0xad6bd6b5, 0x9f3ee7cf, 0xf3e77cf9, 0x3e7ccf9f,
     0x300f9f0, 0xef1e0080, 0xeeee3c78, 0xe000eeee,          0,          0,          0,          0,          0,  0x1dd0000, 0xb6dbdddd, 0xdf3e6db6, 0xd6b56b5a, 0xf9f3ad7c, 0x9f3ee7cf, 0xf3e77cf9,
    0x3e05cf9f, 0xb0617fb0, 0x3ff0e400,  0x77803fc, 0xc777f1e3, 0x77007777,          0,          0,          0,          0,          0,    0xe0000, 0xedb6eeee, 0xb6f9db6d, 0xd6b5f35a, 0xe7cfad6b,
    0x7cf99f3e, 0xcf9ff3e7, 0xf9f03e7c,  0x20011ff, 0x3c78ef1e, 0xeeeeeeee,     0xe000,          0,          0,          0,          0,          0, 0xdddd01dd, 0x6db6b6db, 0x6b5adf3e, 0xad7cd6b5,
    0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7, 0x20003e00, 0xe3c7001d, 0xdddd8f1d,     0xdddc,          0,          0,          0,          0,          0, 0x3bbb0000, 0xdb6dbbb6, 0xe7cdb6db, 0xd6b56b5a,
    0x3e7caf9f, 0xe7cff9f3, 0x7cf99f3e, 0xc00cf3e7,  0x3bc0002, 0xe3bb78f1, 0xbb80bbbb,          0,          0,          0,          0,          0,    0x70000, 0x76db7777, 0xdb7c6db6, 0x6b5af9ad,
    0xf3e7d6b5, 0x3e7ccf9f, 0xe7cff9f3, 0x7cf89f3e,   0x400180, 0x1e3c778f, 0x77777777,     0x7000,          0,          0,          0,          0,          0, 0xeeee00ee, 0xb6dbdb6d, 0x35ad6f9f,
    0xd6be6b5a, 0xf3e77cf9, 0x3e7ccf9f, 0xe7cff9f3, 0xbfd89f02, 0xf2005830,  0x1fe1ff8, 0x78f103bc, 0xbbbbe3bb,     0xbb80,          0,          0,          0,          0,          0, 0x77770007,
    0x6db676db, 0xf9addb7c, 0xd6b56b5a, 0xcf9ff3e7, 0xf9f33e7c, 0x9f3ee7cf,  0x8ff7cf8, 0x778f8100, 0x77771e3c, 0x70007777,          0,          0,          0,          0,          0,   0xee0000,
    0xdb6deeee, 0x6f9fb6db, 0x6b5a35ad, 0x7cf9d6be, 0xcf9ff3e7, 0xf9f33e7c, 0x9f00e7cf,    0xe1000, 0xc78ef1e3, 0xeeeeeeee,          0,          0,          0,          0,          0,          0,
    0xdddb1ddd, 0xdb6d6db6, 0xb5adf3e6, 0xd7cf6b5a, 0x7cf99f3e, 0xcf9ff3e7, 0xf9f33e7c,    0x1e006, 0x3c7801de, 0xddddf1dd,     0xddc0,          0,          0,          0,          0,          0,
    0xbbbb0003, 0xb6dbbb6d, 0x7cd66dbe, 0x6b5ab5ad, 0xe7cff9f3, 0x7cf99f3e, 0xcf9ff3e7,   0xc03e7c, 0x3bc70020, 0x3bbb8f1e, 0xb800bbbb,          0,          0,          0,          0,          0,
      0x770000, 0x6db67777, 0xb7cfdb6d, 0xb5ad9ad6, 0x3e7c6b5f, 0xe7cff9f3, 0x7cf99f3e, 0xcf81f3e7, 0x2c185fec,     0x7800,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
             0,          0,          0,          0,          0,          0,          0,          0,          0,    0x30000, 0x19b76198,          0,          0,          0,          0,          0,
};

int64_t aml_gettime(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((int64_t)(tv.tv_sec) * 1000000 + (int64_t)(tv.tv_usec));
}
int get_sysfs_uint(const char *path, uint *value)
{
    int fd;
    char valstr[64];
    uint val = 0;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        memset(valstr, 0, 64);
        read(fd, valstr, 64 - 1);
        valstr[strlen(valstr)] = '\0';
        close(fd);
    } else {
        ALOGE("unable to open file %s\n", path);
        return -1;
    }
    if (sscanf(valstr, "0x%x", &val) < 1) {
        if (sscanf(valstr, "%u", &val) < 1) {
            ALOGE("unable to get pts from: %s", valstr);
            return -1;
        }
    }
    *value = val;
    return 0;
}

int sysfs_set_sysfs_str(const char *path, const char *val)
{
    int fd;
    int bytes;
    fd = open(path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd >= 0) {
        bytes = write(fd, val, strlen(val));
        close(fd);
        return 0;
    } else {
        ALOGE("unable to open file %s,err: %s", path, strerror(errno));
    }
    return -1;
}

int sysfs_get_sysfs_str(const char *path, char *valstr, int size)
{
    int fd;
    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        memset(valstr,0,size);
        read(fd, valstr, size - 1);
        valstr[strlen(valstr)] = '\0';
        close(fd);
    } else {
        ALOGE("unable to open file %s,err: %s", path, strerror(errno));
        sprintf(valstr, "%s", "fail");
        return -1;
    };
    return 0;
}


int get_sysfs_int(const char *path)
{
    int val = 0;
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        char bcmd[16];
        read(fd, bcmd, sizeof(bcmd));
        val = strtol(bcmd, NULL, 10);
        close(fd);
    } else {
        ALOGD("[%s]open %s node failed! return 0\n", path, __FUNCTION__);
    }
    return val;
}

int set_sysfs_int(const char *path, int value)
{
    char buf[16];
    int fd = open(path, O_WRONLY);
    if (fd >= 0) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d", value);
        write(fd, buf, sizeof(buf));
        close(fd);
    } else {
        ALOGI("[%s]open %s node failed! return 0\n", __FUNCTION__, path);
        return -1;
    }
    return 0;
}

int mystrstr(char *mystr, char *substr)
{
    int i = 0;
    int j = 0;
    int score = 0;
    int substrlen = strlen(substr);
    int ok = 0;
    for (i = 0; i < 1024 - substrlen; i++) {
        for (j = 0; j < substrlen; j++) {
            score += (substr[j] == mystr[i + j]) ? 1 : 0;
        }
        if (score == substrlen) {
            ok = 1;
            break;
        }
        score = 0;
    }
    return ok;
}

int find_offset_in_file_strstr(char *mystr, char *substr)
{
    int i = 0;
    int j = 0;
    int score = 0;
    int substrlen = strlen(substr);
    int ok = 0;
    for (i = 0; i < 1024 - substrlen; i++) {
        for (j = 0; j < substrlen; j++) {
            score += (substr[j] == mystr[i + j]) ? 1 : 0;
        }
        if (score == substrlen) {
            ok = 1;
            break;
        }
        score = 0;
    }
    if (ok) {
        return (i + j - substrlen);
    }
    else {
        return -1;
    }
}

void set_codec_type(int type)
{
    char buf[16];
    int fd = open("/sys/class/audiodsp/digital_codec", O_WRONLY);

    if (fd >= 0) {
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d", type);

        write(fd, buf, sizeof(buf));
        close(fd);
    }
}
unsigned char codec_type_is_raw_data(int type)
{
    switch (type) {
    case TYPE_AC3:
    case TYPE_EAC3:
    case TYPE_TRUE_HD:
    case TYPE_DTS:
    case TYPE_DTS_HD:
    case TYPE_DTS_HD_MA:
    case TYPE_AC4:
        return 1;
    default:
        return 0;
    }
}

int get_codec_type(int format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
        return TYPE_AC3;
    case AUDIO_FORMAT_E_AC3:
        return TYPE_EAC3;
    case AUDIO_FORMAT_DTS:
        return TYPE_DTS;
    case AUDIO_FORMAT_DTS_HD:
        return TYPE_DTS_HD;
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        return TYPE_TRUE_HD;
    case AUDIO_FORMAT_AC4:
        return TYPE_AC4;
    case AUDIO_FORMAT_MAT:
        return TYPE_MAT;
    case AUDIO_FORMAT_PCM:
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
        return TYPE_PCM;
    default:
        return TYPE_PCM;
    }
}
int getprop_bool(const char *path)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get(path, buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "true") == 0 || strcmp(buf, "1") == 0) {
            return 1;
        }
    }
    return 0;
}

int check_chip_name(char *name, unsigned int length)
{
    char buf[PROPERTY_VALUE_MAX];
    char *chip_name = name;
    int ret = -1;

    ret = property_get("ro.board.platform", buf, NULL);
    if (ret > 0) {
        if (strncasecmp(buf, chip_name, length) == 0) {
            return true;
        }
    }
    return false;
}

int is_multi_demux()
{
    if (access("/sys/module/dvb_demux/",F_OK) == 0) {
        ALOGI("use AmHwMultiDemux mode\n");
        return 1;
    }
    ALOGI("use AmHwDemux mode\n");
    return 0;
}

/*
convert audio formats to supported audio format
8 ch goes to 32 bit
2 ch can be 16 bit or 32 bit
@return input buffer used by alsa drivers to do the data write
*/
void *convert_audio_sample_for_output(int input_frames, int input_format, int input_ch, void *input_buf, int *out_size)
{
    float lvol = 1.0;
    int *out_buf = NULL;
    short *p16 = (short*)input_buf;
    int *p32 = (int*)input_buf;
    int max_ch =  input_ch;
    int i;
    //ALOGV("intput frame %d,input ch %d,buf ptr %p,vol %f\n", input_frames, input_ch, input_buf, lvol);
    ALOG_ASSERT(input_buf);
    if (input_ch > 2) {
        max_ch = 8;
    }
    //our HW need round the frames to 8 channels
    out_buf = aml_audio_malloc(sizeof(int) * max_ch * input_frames);
    if (out_buf == NULL) {
        ALOGE("malloc buffer failed\n");
        return NULL;
    }
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT:
        break;
    case AUDIO_FORMAT_PCM_32_BIT:
        break;
    case AUDIO_FORMAT_PCM_8_24_BIT:
        for (i = 0; i < input_frames * input_ch; i++) {
            p32[i] = p32[i] << 8;
        }
        break;
    case AUDIO_FORMAT_PCM_FLOAT:
        memcpy_to_i16_from_float((short*)out_buf, input_buf, input_frames * input_ch);
        memcpy(input_buf, out_buf, sizeof(short)*input_frames * input_ch);
        break;
    }
    //current all the data are in the input buffer
    if (input_ch == 8) {
        short *p16_temp;
        int i, NumSamps;
        int *p32_temp = out_buf;
        float m_vol = lvol;
        NumSamps = input_frames * input_ch;
        //here to swap the channnl data here
        //actual now:L,missing,R,RS,RRS,,LS,LRS,missing
        //expect L,C,R,RS,RRS,LRS,LS,LFE (LFE comes from to center)
        //actual  audio data layout  L,R,C,none/LFE,LRS,RRS,LS,RS
        if (input_format == AUDIO_FORMAT_PCM_16_BIT) {
            p16_temp = (short*)out_buf;
            for (i = 0; i < NumSamps; i = i + 8) {
                p16_temp[0 + i]/*L*/ = m_vol * p16[0 + i];
                p16_temp[1 + i]/*R*/ = m_vol * p16[1 + i];
                p16_temp[2 + i] /*LFE*/ = m_vol * p16[3 + i];
                p16_temp[3 + i] /*C*/ = m_vol * p16[2 + i];
                p16_temp[4 + i] /*LS*/ = m_vol * p16[6 + i];
                p16_temp[5 + i] /*RS*/ = m_vol * p16[7 + i];
                p16_temp[6 + i] /*LRS*/ = m_vol * p16[4 + i];
                p16_temp[7 + i]/*RRS*/ = m_vol * p16[5 + i];
            }
            memcpy(p16, p16_temp, NumSamps * sizeof(short));
            for (i = 0; i < NumSamps; i++) { //suppose 16bit/8ch PCM
                p32_temp[i] = p16[i] << 16;
            }
        } else {
            p32_temp = out_buf;
            for (i = 0; i < NumSamps; i = i + 8) {
                p32_temp[0 + i]/*L*/ = m_vol * p32[0 + i];
                p32_temp[1 + i]/*R*/ = m_vol * p32[1 + i];
                p32_temp[2 + i] /*LFE*/ = m_vol * p32[3 + i];
                p32_temp[3 + i] /*C*/ = m_vol * p32[2 + i];
                p32_temp[4 + i] /*LS*/ = m_vol * p32[6 + i];
                p32_temp[5 + i] /*RS*/ = m_vol * p32[7 + i];
                p32_temp[6 + i] /*LRS*/ = m_vol * p32[4 + i];
                p32_temp[7 + i]/*RRS*/ = m_vol * p32[5 + i];
            }

        }
        *out_size = NumSamps * sizeof(int);

    } else if (input_ch == 6) {
        int j, NumSamps, real_samples;
        short *p16_temp;
        int *p32_temp = out_buf;
        float m_vol = lvol;
        NumSamps = input_frames * input_ch;
        real_samples = NumSamps;
        NumSamps = real_samples * 8 / 6;
        //ALOGI("6ch to 8 ch real %d, to %d\n",real_samples,NumSamps);
        if (input_format == AUDIO_FORMAT_PCM_16_BIT) {
            p16_temp = (short*)out_buf;
            for (i = 0; i < real_samples; i = i + 6) {
                p16_temp[0 + i]/*L*/ = m_vol * p16[0 + i];
                p16_temp[1 + i]/*R*/ = m_vol * p16[1 + i];
                p16_temp[2 + i] /*LFE*/ = m_vol * p16[3 + i];
                p16_temp[3 + i] /*C*/ = m_vol * p16[2 + i];
                p16_temp[4 + i] /*LS*/ = m_vol * p16[4 + i];
                p16_temp[5 + i] /*RS*/ = m_vol * p16[5 + i];
            }
            memcpy(p16, p16_temp, real_samples * sizeof(short));
            memset(p32_temp, 0, NumSamps * sizeof(int));
            for (i = 0, j = 0; j < NumSamps; i = i + 6, j = j + 8) { //suppose 16bit/8ch PCM
                p32_temp[j + 0] = p16[i] << 16;
                p32_temp[j + 1] = p16[i + 1] << 16;
                p32_temp[j + 2] = p16[i + 2] << 16;
                p32_temp[j + 3] = p16[i + 3] << 16;
                p32_temp[j + 4] = p16[i + 4] << 16;
                p32_temp[j + 5] = p16[i + 5] << 16;
            }
        } else {
            p32_temp = out_buf;
            memset(p32_temp, 0, NumSamps * sizeof(int));
            for (i = 0, j = 0; j < NumSamps; i = i + 6, j = j + 8) { //suppose 16bit/8ch PCM
                p32_temp[j + 0] = m_vol * p32[i + 0];
                p32_temp[j + 1] = m_vol * p32[i + 1] ;
                p32_temp[j + 2] = m_vol * p32[i + 2] ;
                p32_temp[j + 3] = m_vol * p32[i + 3] ;
                p32_temp[j + 4] = m_vol * p32[i + 4] ;
                p32_temp[j + 5] = m_vol * p32[i + 5] ;
            }
        }
        *out_size = NumSamps * sizeof(int);
    } else {
        //2ch with 24 bit/32/float audio
        int *p32_temp = out_buf;
        short *p16_temp = (short*)out_buf;
        for (i = 0; i < input_frames; i++) {
            p16_temp[2 * i + 0] =  lvol * p16[2 * i + 0];
            p16_temp[2 * i + 1] =  lvol * p16[2 * i + 1];
        }
        *out_size = sizeof(short) * input_frames * input_ch;
    }
    return out_buf;

}

int aml_audio_start_trigger(void *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    char tempbuf[128];
    ALOGI("reset alsa to set the audio start\n");
    pcm_stop(aml_out->pcm);
    sprintf(tempbuf, "AUDIO_START:0x%x", adev->first_apts);
    ALOGI("audio start set tsync -> %s", tempbuf);

    aml_hwsync_set_tsync_init(aml_out->hwsync);


    if (aml_hwsync_set_tsync_start_pts(aml_out->hwsync, adev->first_apts) == -1) {
        ALOGE("set AUDIO_START failed \n");
        return -1;
    }
    return 0;
}

int aml_audio_get_debug_flag()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int debug_flag = 0;
    ret = property_get("vendor.media.audio.hal.debug", buf, NULL);
    if (ret > 0) {
        debug_flag = atoi(buf);
    }
    return debug_flag;
}

int aml_audio_get_default_alsa_output_ch()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    /* default 8 channels for TV product */
    int chan_num =  8;
    ret = property_get("ro.vendor.platform.alsa.spk.ch", buf, NULL);
    if (ret > 0) {
        chan_num = atoi(buf);
    }
    return chan_num;
}

/*
this prop judgement is borrowed from CEC framework, which is used
to detect TV/SBR product, audio HAL can also use that.
*/
bool aml_audio_check_sbr_product()
{
    char buf[PROPERTY_VALUE_MAX] ={0};
    int ret = -1;
    char *sbr_str = NULL;
    ret = property_get("ro.vendor.platform.hdmi.device_type", buf, NULL);
    if (ret > 0) {
        sbr_str = strstr(buf,"5");
        if (sbr_str)
            return true;
    }
    return false;
}

int aml_audio_debug_set_optical_format()
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get("vendor.media.audio.hal.optical", buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "pcm") == 0 || strcmp(buf, "0") == 0) {
            return TYPE_PCM;
        }
        if (strcasecmp(buf, "dd") == 0 || strcmp(buf, "1") == 0) {
            return TYPE_AC3;
        }
        if (strcasecmp(buf, "ddp") == 0 || strcmp(buf, "2") == 0) {
            return TYPE_EAC3;
        }
    }
    return -1;
}

int aml_audio_dump_audio_bitstreams(const char *path, const void *buf, size_t bytes)
{
    if (!path) {
        return 0;
    }

    FILE *fp = fopen(path, "a+");
    if (fp) {
        int flen = fwrite((char *)buf, 1, bytes, fp);
        fclose(fp);
    }

    return 0;
}
int aml_audio_get_arc_latency_offset(int aformat)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    (void)aformat;
    prop_name = "vendor.media.audio.hal.arc_latency.ddp";
    latency_ms = 0;
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}

int aml_audio_get_ddp_latency_offset(int aformat,  bool dual_spdif)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    (void)aformat;
    prop_name = "vendor.media.audio.hal.latency.ddp";
    /* the difference between dual_spdif and no dual_spdif is:
     * dcv decode need 2 frame and output 1 frame, spdif enc only
     * need one frame and can output the frame, so there is one frame diff
     */
    if (dual_spdif) {
        latency_ms = -80;
    } else {
        latency_ms = -48;
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}

int aml_audio_get_pcm_latency_offset(int aformat, bool is_netflix, stream_usecase_t usecase)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    (void)aformat;
    prop_name = "vendor.media.audio.hal.latency.pcm";
    if (!is_netflix) {
        //32ms Omx decoder delay
        //16ms video delay
        //16ms Sub Mix delay
        /* 384Bytes*8 = 16ms*48kHz(newAmlAudioMixer tmp_buffer size is MIXER_FRAME_COUNT * MIXER_OUT_FRAME_SIZE) */
        latency_ms = 64;
    } else {
        switch (usecase) {
            case STREAM_PCM_NORMAL:
                latency_ms = 5;
                break;
            default:
               latency_ms = -50;
               break;
        };
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}


int aml_audio_get_hwsync_latency_offset(bool b_raw)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    if (!b_raw) {
        prop_name = "vendor.media.audio.hal.hwsync_latency.pcm";
        latency_ms = -22; // left offset 0 --> -15 --> -27 --> -22
    } else {
        prop_name = "vendor.media.audio.hal.hwsync_latency.ddp";
        latency_ms = -45; // left offset -30 --> -50 --> -45
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}


int aml_audio_get_ddp_frame_size()
{
    int frame_size = DDP_FRAME_SIZE;
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    char *prop_name = "vendor.media.audio.hal.frame_size";
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        frame_size = atoi(buf);
    }
    return frame_size;
}

bool is_stream_using_mixer(struct aml_stream_out *out)
{
    return is_inport_valid(out->inputPortID);
}

uint32_t out_get_outport_latency(const struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    struct subMixing *sm = adev->sm;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    int frames = 0, latency_ms = 0;

    if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(adev);
    }

    if (is_stream_using_mixer(out)) {
        int outport_latency_frames = mixer_get_outport_latency_frames(audio_mixer);

        if (outport_latency_frames <= 0)
            outport_latency_frames = out->config.period_size * out->config.period_count / 2;

        frames = outport_latency_frames;
        ALOGV("%s(), total frames %d", __func__, frames);
        latency_ms = (frames * 1000) / out->config.rate;
        ALOGV("%s(), latencyMs %d, rate %d", __func__, latency_ms,out->config.rate);
    }
    return latency_ms;
}

static bool is_4x_rate_fmt(audio_format_t afmt)
{
    return (afmt == AUDIO_FORMAT_E_AC3) ||
        (afmt == AUDIO_FORMAT_DTS_HD) ||
        (afmt == AUDIO_FORMAT_DOLBY_TRUEHD);
}

uint32_t out_get_latency_frames(const struct audio_stream_out *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    audio_format_t afmt = get_output_format((struct audio_stream_out *)stream);
    snd_pcm_sframes_t frames = 0;
    uint32_t whole_latency_frames;
    int ret = 0;
    int mul = 1;

    if (is_4x_rate_fmt(afmt))
        mul = 4;

    if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(adev) * out->hal_rate / 1000;
    }

    whole_latency_frames = out->config.period_size * out->config.period_count;
    if (!out->pcm || !pcm_is_ready(out->pcm)) {
        return whole_latency_frames / mul;
    }
    ret = pcm_ioctl(out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    return frames / mul;
}

uint32_t out_get_alsa_latency_frames(const struct audio_stream_out *stream)
{
    const struct aml_stream_out *out = (const struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    audio_format_t afmt = get_output_format((struct audio_stream_out *)stream);
    snd_pcm_sframes_t frames = 0;
    uint32_t whole_latency_frames;
    int ret = 0;
    int mul = 1;

    if (is_4x_rate_fmt(afmt))
        mul = 4;

    if (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(adev) * out->hal_rate / 1000;
    }

    whole_latency_frames = out->config.period_size * out->config.period_count / 2;
    if (!out->pcm || !pcm_is_ready(out->pcm)) {
        return whole_latency_frames / mul;
    }
    ret = pcm_ioctl(out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    return frames / mul;
}


int aml_audio_get_spdif_tuning_latency(void)
{
    char *prop_name = "persist.vendor.audio.hal.spdif_ltcy_ms";
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}

int aml_audio_get_arc_tuning_latency(audio_format_t arc_fmt)
{
    char *prop_name = NULL;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    switch (arc_fmt) {
    case AUDIO_FORMAT_PCM_16_BIT:
        prop_name = "persist.vendor.audio.arc_ltcy.pcm";
        break;
    case AUDIO_FORMAT_AC3:
        prop_name = "persist.vendor.audio.arc_ltcy.dd";
        break;
    case AUDIO_FORMAT_E_AC3:
        prop_name = "persist.vendor.audio.arc_ltcy.ddp";
        break;
    default:
        ALOGE("%s(), unsupported audio arc_fmt: %#x", __func__, arc_fmt);
        return 0;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}

int aml_audio_get_src_tune_latency(enum patch_src_assortion patch_src) {
    char *prop_name = NULL;
    char buf[PROPERTY_VALUE_MAX];
    int latency_ms = 0;
    int ret = -1;

    switch (patch_src)
    {
    case SRC_HDMIIN:
        prop_name = "persist.vendor.audio.tune_ms.hdmiin";
        break;
    case SRC_ATV:
        prop_name = "persist.vendor.audio.tune_ms.atv";
        break;
    case SRC_LINEIN:
        prop_name = "persist.vendor.audio.tune_ms.linein";
        break;
    default:
        ALOGE("%s(), unsupported audio patch source: %s", __func__, patchSrc2Str(patch_src));
        return 0;
    }

    ret = property_get(prop_name, buf, NULL);
    if (ret > 0)
    {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}

void audio_fade_func(void *buf,int fade_size,int is_fadein) {
    float fade_vol = is_fadein ? 0.0 : 1.0;
    int i = 0;
    float fade_step = is_fadein ? 1.0/(fade_size/4):-1.0/(fade_size/4);
    int16_t *sample = (int16_t *)buf;
    for (i = 0; i < fade_size/2; i += 2) {
        sample[i] = sample[i]*fade_vol;
        sample[i+1] = sample[i+1]*fade_vol;
        fade_vol += fade_step;
    }
    ALOGI("do fade %s done,size %d",is_fadein?"in":"out",fade_size);

}

void ts_wait_time_us(struct timespec *ts, uint32_t time_us)
{
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += (time_us / 1000000);
    ts->tv_nsec += (time_us * 1000);
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
}

int cpy_16bit_data_with_gain(int16_t *dst, int16_t *src, int size_in_bytes, float vol)
{
    int size_in_short = size_in_bytes / 2;
    int i = 0;

    if (size_in_bytes % 2) {
        ALOGE("%s(), size inval %d", __func__, size_in_bytes);
        return -EINVAL;
    }

    if (vol > 1.0 || vol < 0) {
        ALOGE("%s(), inval vol %f, should in [0,1]", __func__, vol);
        return -EINVAL;
    }

    for (i = 0; i < size_in_short; i++) {
        dst[i] = src[i] * vol;
    }

    return 0;
}

static inline uint64_t timespec_ns(struct timespec tspec)
{
    return (tspec.tv_sec * 1000000000 + tspec.tv_nsec);
}

uint64_t get_systime_ns(void)
{
    struct timespec tval;

    clock_gettime(CLOCK_MONOTONIC, &tval);

    return timespec_ns(tval);
}

int aml_audio_get_hdmi_latency_offset(audio_format_t source_format,
                                      audio_format_t sink_format,int ms12_enable)
{
    char buf[PROPERTY_VALUE_MAX];
    char *prop_name = NULL;
    int ret = -1;
    int latency_ms = 0;

    if (source_format == AUDIO_FORMAT_PCM_16_BIT || source_format == AUDIO_FORMAT_PCM_32_BIT) {
        prop_name = "vendor.media.audio.hal.hdmi_latency.pcm";
        if (ms12_enable) {
           if (sink_format == AUDIO_FORMAT_E_AC3) {
              latency_ms = -10;
           } else if (sink_format == AUDIO_FORMAT_AC3) {
              latency_ms = -10;
           } else {
              latency_ms = 0;
           }
        } else
           latency_ms = 0;
    } else {
        prop_name = "vendor.media.audio.hal.hdmi_latency.raw";
        if (source_format == AUDIO_FORMAT_E_AC3) {
             if (ms12_enable) {
                 if (sink_format == AUDIO_FORMAT_E_AC3) {
                        latency_ms = -80;
                 } else if (sink_format == AUDIO_FORMAT_PCM_16_BIT) {
                        latency_ms = -60;
                 }
             } else {
                 latency_ms = -25; //left offset -50 --> -35.-25
             }
        } else  if(source_format == AUDIO_FORMAT_AC3) {
            if (ms12_enable)
                latency_ms = 0;
             else
                latency_ms = -95;
        }
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0)
    {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}

int aml_audio_get_speaker_latency_offset(int aformat ,int ms12_enable)
{
    char buf[PROPERTY_VALUE_MAX];
    char *prop_name = NULL;
    int ret = -1;
    int latency_ms = 0;

    (void)aformat;
    // PCM latency
    if (aformat == AUDIO_FORMAT_PCM_16_BIT || aformat == AUDIO_FORMAT_PCM_32_BIT) {
        prop_name = "vendor.media.audio.hal.speaker_latency.pcm";
        if (ms12_enable)
           latency_ms = 105;
        else
           latency_ms = 0;
    } else {
        prop_name = "vendor.media.audio.hal.speaker_latency.raw";
        latency_ms = 80;
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0)
    {
        latency_ms = atoi(buf);
    }

    return latency_ms;
}
int aml_audio_get_latency_offset( enum OUT_PORT port,audio_format_t source_format,
                                      audio_format_t sink_format,int ms12_enable)
{
    int latency_ms = 0;
    switch (port)  {
        case OUTPORT_HDMI_ARC:
            latency_ms = aml_audio_get_arc_latency_offset(source_format);
            break;
        case OUTPORT_HDMI:
            latency_ms = aml_audio_get_hdmi_latency_offset(source_format,sink_format,ms12_enable);
            break;
        case OUTPORT_SPEAKER:
        case OUTPORT_AUX_LINE:
            latency_ms = aml_audio_get_speaker_latency_offset(source_format,ms12_enable);
            break;
        default :
            break;
    }
    return latency_ms;
}

// tval_new *must* later than tval_old
uint32_t tspec_diff_to_us(struct timespec tval_old,
        struct timespec tval_new)
{
    return (tval_new.tv_sec - tval_old.tv_sec) * 1000000
            + (tval_new.tv_nsec - tval_old.tv_nsec) / 1000;
}


int aml_audio_get_dolby_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost)
{
    char cEndpoint[PROPERTY_VALUE_MAX];
    int ret = 0;
    unsigned ac3_drc_control = (DDPI_UDC_COMP_LINE<<DRC_MODE_BIT)|(100<<DRC_HIGH_CUT_BIT)|(100<<DRC_LOW_BST_BIT);
    ac3_drc_control = get_sysfs_int("/sys/class/audiodsp/ac3_drc_control");

    if (!drc_mode || !drc_cut || !drc_boost)
        return -1;
    *drc_mode = ac3_drc_control&3;
    ALOGI("drc mode from sysfs %s\n",str_compmode[*drc_mode]);
    ret = property_get("ro.vendor.dolby.drcmode",cEndpoint,"");
    if (ret > 0) {
        *drc_mode = atoi(cEndpoint)&3;
        ALOGI("drc mode from prop %s\n",str_compmode[*drc_mode]);
    }
    *drc_cut  = (ac3_drc_control>>DRC_HIGH_CUT_BIT)&0xff;
    *drc_boost  = (ac3_drc_control>>DRC_LOW_BST_BIT)&0xff;
    ALOGI("dd+ drc mode %s,high cut %d pct,low boost %d pct\n",
        str_compmode[*drc_mode],*drc_cut, *drc_boost);
    return 0;
}

int aml_audio_get_dolby_dap_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost)
{
    char cEndpoint[PROPERTY_VALUE_MAX];
    int ret = 0;
    unsigned dap_drc_control = (DDPI_UDC_COMP_LINE<<DRC_MODE_BIT)|(100<<DRC_HIGH_CUT_BIT)|(100<<DRC_LOW_BST_BIT);
    dap_drc_control = get_sysfs_int("/sys/class/audiodsp/dap_drc_control");

    if (!drc_mode || !drc_cut || !drc_boost)
        return -1;
    *drc_mode = dap_drc_control&3;
    ALOGI("drc mode from sysfs %s\n",str_compmode[*drc_mode]);
    ret = property_get("ro.dolby.dapdrcmode",cEndpoint,"");
    if (ret > 0) {
        *drc_mode = atoi(cEndpoint)&3;
        ALOGI("drc mode from prop %s\n",str_compmode[*drc_mode]);
    }
    *drc_cut  = (dap_drc_control>>DRC_HIGH_CUT_BIT)&0xff;
    *drc_boost  = (dap_drc_control>>DRC_LOW_BST_BIT)&0xff;
    ALOGI("dap drc mode %s,high cut %d pct,low boost %d pct\n",
        str_compmode[*drc_mode],*drc_cut, *drc_boost);
    return 0;
}

void aml_audio_set_cpu23_affinity()
{
    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(2, &cpuSet);
    CPU_SET(3, &cpuSet);
    int status = sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet);
    if (status) {
        ALOGW("%s(), failed to set cpu affinity", __FUNCTION__);
    }
}

void * aml_audio_get_muteframe(audio_format_t output_format, int * frame_size, int bAtmos) {
    if (output_format == AUDIO_FORMAT_AC3) {
        *frame_size = sizeof(muted_frame_dd);
        return (void*)muted_frame_dd;
    } else if (output_format == AUDIO_FORMAT_E_AC3) {
        if (bAtmos) {
            *frame_size = sizeof(muted_frame_atmos);
            return (void*)muted_frame_atmos;
        } else {
            *frame_size = sizeof(muted_frame_ddp);
            return (void*)muted_frame_ddp;
        }
    } else {
        *frame_size = 0;
        return NULL;
    }
}

void aml_audio_switch_output_mode(int16_t *buf, size_t bytes, AM_AOUT_OutputMode_t mode)
{
    int16_t tmp;

    for (unsigned int i= 0; i < bytes / 2; i = i + 2) {
        switch (mode) {
            case AM_AOUT_OUTPUT_DUAL_LEFT:
                buf[i + 1] = buf[i];
                break;
            case AM_AOUT_OUTPUT_DUAL_RIGHT:
                buf[i] = buf[i + 1];
                break;
            case AM_AOUT_OUTPUT_SWAP:
                tmp = buf[i];
                buf[i] = buf[i + 1];
                buf[i + 1] = tmp;
                break;
            case AM_AOUT_OUTPUT_LRMIX:
                tmp = (buf[i] / 2)  + (buf[i + 1] / 2);
                buf[i] = tmp;
                buf[i + 1] = tmp;
                break;
            default :
                break;
        }
    }
}

/*****************************************************************************
*   Function Name:  aml_audio_data_detect
*   Description:    accumulate buf in bytes, then compare with detect_value.
*                   the purpose is to detect value of buf that is zero or not.
*   Parameters:     int16_t *: the audio data buffer.
*                   size_t bytes: the buffer length.
*                   int: for compared value.
*   Return value:   true if buf value is zero, or false.
******************************************************************************/
bool aml_audio_data_detect(int16_t *buf, size_t bytes, int detect_value)
{
    int ret = false;
    uint64_t buf_value = 0;
    uint32_t i = 0;

    while (i <= bytes/2) {
        buf_value +=  abs(buf[i++]);
    };

    ALOGV("%s  i:%u  buf_value:%lld (%#llx),  sizeof(uint64_t):%u sizeof(size_t):%u", __func__,
                i, buf_value, buf_value,  sizeof(uint64_t), sizeof(size_t));
    if (buf_value <= detect_value) {
        ret = true;
    } else {
        ret = false;
    }

    return ret;
}

static int mixer_aux_start_ease_in(struct aml_stream_out *aml_out) {
    /*start ease in the audio*/
    ease_setting_t ease_setting;
    aml_out->audio_stream_ease->data_format.format = AUDIO_FORMAT_PCM_16_BIT;
    aml_out->audio_stream_ease->data_format.ch = 2;
    aml_out->audio_stream_ease->data_format.sr = 48000;
    aml_out->audio_stream_ease->ease_type = EaseLinear;
    ease_setting.duration = 40;
    ease_setting.start_volume = 0.0;
    ease_setting.target_volume = 1.0;
    aml_audio_ease_config(aml_out->audio_stream_ease, &ease_setting);

    ALOGV("%s ", __func__);
    return 0;
}

/*****************************************************************************
*   Function Name:  aml_audio_data_handle
*   Description:    handle audio data before send to driver or decoder.
*                   the purpose is to detect and fade in.
*   Parameters:     struct audio_stream_out: audio output stream pointer.
*                   const void *: the buffer pointer.
*                   size_t: the buffer length.
*   Return value:   true if buf value is zero, or false.
******************************************************************************/
int aml_audio_data_handle(struct audio_stream_out *stream, const void* buffer, size_t bytes)
{
// 8ms audio data
#define DETECT_AUDIO_TIME_UNIT (8)
#define DETECT_AUDIO_DATA_UNIT (DETECT_AUDIO_TIME_UNIT * out->hal_frame_size * out->hal_rate / 1000)
/* value 2000 for filter noise data,
** this value is confirmed according logs.
*/
#define AML_DETECT_VALUE (2000)

    struct aml_stream_out *out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = out->dev;
    bool ret = false;
    int unit_size = 0;
    int detected_size = 0;
    size_t remaining_size = bytes;

    ALOGV("%s out_stream usecase:%d-->%s, hal_format:%#x hal_ch:%u --> hal_frame_size:%u, hal_rate:%u, DETECT_AUDIO_DATA_UNIT:%zu, bytes:%zu", __func__,
          out->usecase, usecase2Str(out->usecase), out->hal_format, out->hal_ch, out->hal_frame_size, out->hal_rate, DETECT_AUDIO_DATA_UNIT, bytes);

    while (out->audio_data_handle_state < AUDIO_DATA_HANDLE_FINISHED && remaining_size) {
        ALOGD("%s remaing_size:%zu,  out->audio_data_handle_status:%u", __func__, remaining_size, out->audio_data_handle_state);
        switch (out->audio_data_handle_state) {
            case AUDIO_DATA_HANDLE_START:
                FALLTHROUGH_INTENDED; /* [[fallthrough]] */
            case AUDIO_DATA_HANDLE_DETECT:
                out->audio_data_handle_state = AUDIO_DATA_HANDLE_DETECT;
                while (remaining_size > 0) {
                    if (remaining_size > DETECT_AUDIO_DATA_UNIT) {
                        unit_size = DETECT_AUDIO_DATA_UNIT;
                    } else {
                        unit_size = remaining_size;
                    }

                    ret = aml_audio_data_detect((int16_t *)((int8_t *)buffer + detected_size), unit_size , AML_DETECT_VALUE);
                    if (false == ret) {
                        out->audio_data_handle_state = AUDIO_DATA_HANDLE_DETECTED;
                        ALOGD("%s  detected the nonzero data, remaing_size:%zu  detected_size:%zu", __func__, remaining_size, detected_size);
                        break;
                    }

                    remaining_size -= unit_size;
                    detected_size += unit_size;
                }
                break;
            // detect finished, then do fade in.
            case AUDIO_DATA_HANDLE_DETECTED:
                out->audio_data_handle_state = AUDIO_DATA_HANDLE_EASE_CONFIG;
                break;
            case AUDIO_DATA_HANDLE_EASE_CONFIG:
                mixer_aux_start_ease_in(out);
                out->easing_time = 0;
                out->audio_data_handle_state = AUDIO_DATA_HANDLE_EASING;
                break;
            case AUDIO_DATA_HANDLE_EASING:
                aml_audio_ease_process(out->audio_stream_ease, (void *)((uint8_t *)buffer + detected_size), remaining_size);
                out->easing_time += remaining_size/(out->hal_frame_size * out->hal_rate / 1000);
                ALOGD("%s  easing_time:%u, audio_stream_ease->ease_time:%u", __func__, out->easing_time, out->audio_stream_ease->ease_time);
                remaining_size = 0;
                if (out->easing_time >=  out->audio_stream_ease->ease_time) {
                    out->audio_data_handle_state = AUDIO_DATA_HANDLE_FINISHED;
                }
                break;
            case AUDIO_DATA_HANDLE_FINISHED:
                out->audio_data_handle_state = AUDIO_DATA_HANDLE_FINISHED;
                ALOGD("%s  handle finished", __func__);
                break;
            default :
                break;
        };
    }

    return 0;
}


int aml_audio_compensate_video_delay( int enable) {
    int video_delay = 0;
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    char *prop_name = NULL;

    if (enable) {
        /*alsa delay is about 80, the MS12 tunning delay is about 70*/
        video_delay = 150;
        prop_name = "vendor.media.audio.hal.video_delay_time";
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            video_delay = atoi(buf);
        }
    }
    ALOGI("set video delay=%d", video_delay);
    set_sysfs_int("/sys/class/video/video_delay_time", video_delay);
    return 0;
}


int aml_audio_get_ms12_timestamp_offset(void)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    char *prop_name = NULL;
    int delay_time_ms = 0;
    delay_time_ms = 100;
    prop_name = "vendor.media.audio.hal.delay_timestamp";
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        delay_time_ms = atoi(buf);
    }

    return delay_time_ms;
}


int aml_audio_delay_timestamp(struct timespec *timestamp, int delay_time_ms) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    char *prop_name = NULL;
    uint64_t new_time_ns;


    new_time_ns = timestamp->tv_sec * 1000000000LL + timestamp->tv_nsec + delay_time_ms * 1000000LL;
    timestamp->tv_sec = new_time_ns/1000000000LL;
    timestamp->tv_nsec = new_time_ns - timestamp->tv_sec*1000000000LL;

    return 0;
}

int halformat_convert_to_spdif(audio_format_t format, int ch_mask) {
    int aml_spdif_format = AML_STEREO_PCM;
    switch (format) {
        case AUDIO_FORMAT_PCM_16_BIT:
            aml_spdif_format = AML_STEREO_PCM;
            if (audio_channel_count_from_out_mask(ch_mask) > 2) {
                aml_spdif_format = AML_MULTI_CH_LPCM;
            }
            break;
        case AUDIO_FORMAT_AC3:
            aml_spdif_format = AML_DOLBY_DIGITAL;
            break;
        case AUDIO_FORMAT_E_AC3:
            aml_spdif_format = AML_DOLBY_DIGITAL_PLUS;
            break;
        case AUDIO_FORMAT_DTS:
            aml_spdif_format = AML_DTS;
            break;
        case AUDIO_FORMAT_DTS_HD:
            aml_spdif_format = AML_DTS_HD;
            break;
        case AUDIO_FORMAT_MAT:
            aml_spdif_format = AML_TRUE_HD;
            break;
        default:
            aml_spdif_format = AML_STEREO_PCM;
            break;
    }
    return aml_spdif_format;
}

/*
 * convert alsa_device_t to PORT***
 */
int alsa_device_get_port_index(alsa_device_t alsa_device)
{
    int alsa_port = -1;
    if (alsa_device == I2S_DEVICE) {
        alsa_port = PORT_I2S;
    } else if (alsa_device == DIGITAL_DEVICE) {
        alsa_port = PORT_SPDIF;
    } else if (alsa_device == DIGITAL_DEVICE2) {
        alsa_port = PORT_SPDIFB;
    } else if (alsa_device == TDM_DEVICE) {
        alsa_port = PORT_I2S2HDMI;
    }
    return alsa_port;
}

int aml_set_thread_priority(char *pName, pthread_t threadId)
{
    struct sched_param  params = {0};
    int                 ret = 0;
    int                 policy = SCHED_FIFO; /* value:1 [pthread.h] */
    params.sched_priority = 5;
    ret = pthread_setschedparam(threadId, SCHED_FIFO, &params);
    if (ret != 0) {
        ALOGW("[%s:%d] set scheduled param error, ret:%#x", __func__, __LINE__, ret);
    }
    ret = pthread_getschedparam(threadId, &policy, &params);
    ALOGD("[%s:%d] thread:%s set priority, ret:%d policy:%d priority:%d",
        __func__, __LINE__, pName, ret, policy, params.sched_priority);
    return ret;
}

bool is_multi_channel_pcm(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    return (audio_is_linear_pcm(aml_out->hal_internal_format) &&
           (aml_out->hal_ch > 2));
}

bool is_high_rate_pcm(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    return (audio_is_linear_pcm(aml_out->hal_internal_format) &&
           (aml_out->hal_rate > MM_FULL_POWER_SAMPLING_RATE));
}

bool is_disable_ms12_continuous(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;

    if ((aml_out->hal_internal_format == AUDIO_FORMAT_DTS)
        || (aml_out->hal_internal_format == AUDIO_FORMAT_DTS_HD)) {
        /*dts case, we need disable ms12 continuous mode*/
        return true;
    } else if (is_high_rate_pcm(stream) || is_multi_channel_pcm(stream)) {
        /*high bit rate pcm case, we need disable ms12 continuous mode*/
        return true;
    } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3 \
               || aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
        /*only support 48kz ddp/dd*/
        if (aml_out->hal_rate == 48000 || aml_out->hal_rate == 192000) {
            return false;
        } else {
            return true;
        }
    } else if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        return true;
    }
    return false;
}

float aml_audio_get_s_gain_by_src(struct aml_audio_device *adev, enum patch_src_assortion type)
{
    switch(type) {
        case SRC_ATV:
            return adev->eq_data.s_gain.atv;
        case SRC_DTV:
            return adev->eq_data.s_gain.dtv;
        case SRC_HDMIIN:
            return adev->eq_data.s_gain.hdmi;
        case SRC_LINEIN:
            return adev->eq_data.s_gain.av;
        default:
            return adev->eq_data.s_gain.media;
    }
}

int android_dev_convert_to_hal_dev(audio_devices_t android_dev, int *hal_dev_port)
{
    switch (android_dev) {
    /* audio hal output device port */
    case AUDIO_DEVICE_OUT_HDMI_ARC:
        *hal_dev_port = OUTPORT_HDMI_ARC;
        break;
    case AUDIO_DEVICE_OUT_HDMI:
        *hal_dev_port = OUTPORT_HDMI;
        break;
    case AUDIO_DEVICE_OUT_SPDIF:
        *hal_dev_port = OUTPORT_SPDIF;
        break;
    case AUDIO_DEVICE_OUT_AUX_LINE:
        *hal_dev_port = OUTPORT_AUX_LINE;
        break;
    case AUDIO_DEVICE_OUT_SPEAKER:
        *hal_dev_port = OUTPORT_SPEAKER;
        break;
    case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
        *hal_dev_port = OUTPORT_HEADPHONE;
        break;
    case AUDIO_DEVICE_OUT_REMOTE_SUBMIX:
        *hal_dev_port = OUTPORT_REMOTE_SUBMIX;
        break;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        *hal_dev_port = OUTPORT_BT_SCO;
        break;
    case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        *hal_dev_port = OUTPORT_BT_SCO_HEADSET;
        break;
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES:
    case AUDIO_DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER:
        *hal_dev_port = OUTPORT_A2DP;
        break;
    /* audio hal input device port */
    case AUDIO_DEVICE_IN_HDMI:
        *hal_dev_port = INPORT_HDMIIN;
        break;
    case AUDIO_DEVICE_IN_HDMI_ARC:
        *hal_dev_port = INPORT_ARCIN;
        break;
    case AUDIO_DEVICE_IN_LINE:
        *hal_dev_port = INPORT_LINEIN;
        break;
    case AUDIO_DEVICE_IN_TV_TUNER:
        *hal_dev_port = INPORT_TUNER;
        break;
    case AUDIO_DEVICE_IN_SPDIF:
        *hal_dev_port = INPORT_SPDIF;
        break;
    case AUDIO_DEVICE_IN_REMOTE_SUBMIX:
        *hal_dev_port = INPORT_REMOTE_SUBMIXIN;
        break;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
        *hal_dev_port = INPORT_WIRED_HEADSETIN;
        break;

    case AUDIO_DEVICE_IN_BUILTIN_MIC:/* fallthrough */
    case AUDIO_DEVICE_IN_BACK_MIC:
        *hal_dev_port = INPORT_BUILTIN_MIC;
        break;
    case AUDIO_DEVICE_IN_ECHO_REFERENCE:
        *hal_dev_port = INPORT_ECHO_REFERENCE;
        break;
/*
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        *hal_dev_port = INPORT_BT_SCO_HEADSET_MIC;
        break;
    case AUDIO_DEVICE_IN_USB_DEVICE:
        *hal_dev_port = INPORT_USB;
        break;
    case AUDIO_DEVICE_IN_BUS:
        *hal_dev_port = INPORT_LOOPBACK;
        break;
*/
    default:
        if (AUDIO_DEVICE_BIT_IN & android_dev) {
            *hal_dev_port = INPORT_HDMIIN;
            ALOGW("[%s:%d] unsupport input dev:%#x, return default HDMIN.", __func__, __LINE__, android_dev);
        } else {
            *hal_dev_port = OUTPORT_SPEAKER;
            ALOGW("[%s:%d] unsupport output dev:%#x, return default SPEAKER.", __func__, __LINE__, android_dev);
        }
        return -1;
    }
    return 0;
}

enum patch_src_assortion android_input_dev_convert_to_hal_patch_src(audio_devices_t android_dev)
{
    enum patch_src_assortion patch_src = SRC_INVAL;
    switch (android_dev) {
    case AUDIO_DEVICE_IN_HDMI:
        patch_src = SRC_HDMIIN;
        break;
    case AUDIO_DEVICE_IN_HDMI_ARC:
        patch_src = SRC_ARCIN;
        break;
    case AUDIO_DEVICE_IN_LINE:
        patch_src = SRC_LINEIN;
        break;
    case AUDIO_DEVICE_IN_SPDIF:
        patch_src = SRC_SPDIFIN;
        break;
    case AUDIO_DEVICE_IN_TV_TUNER:
        patch_src = SRC_ATV;
        break;
    case AUDIO_DEVICE_IN_REMOTE_SUBMIX:
        patch_src = SRC_REMOTE_SUBMIXIN;
        break;
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
        patch_src = SRC_WIRED_HEADSETIN;
        break;
    case AUDIO_DEVICE_IN_BUILTIN_MIC: /* fallthrough */
    case AUDIO_DEVICE_IN_BACK_MIC:
        patch_src = SRC_BUILTIN_MIC;
        break;
    case AUDIO_DEVICE_IN_ECHO_REFERENCE:
        patch_src = SRC_ECHO_REFERENCE;
        break;
/*
    case AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET:
        patch_src = SRC_BT_SCO_HEADSET_MIC;
        break;
    case AUDIO_DEVICE_IN_USB_DEVICE:
        patch_src = SRC_USB;
        break;
    case AUDIO_DEVICE_IN_BUS:
        patch_src = SRC_LOOPBACK;
        break;
*/
    default:
        ALOGW("[%s:%d] unsupport input dev:%#x, return SRC_INVAL.", __func__, __LINE__, android_dev);
    }
    return patch_src;
}

enum input_source android_input_dev_convert_to_hal_input_src(audio_devices_t android_dev)
{
    enum input_source input_src = SRC_NA;
    switch (android_dev) {
    case AUDIO_DEVICE_IN_HDMI:
        input_src = HDMIIN;
        break;
    case AUDIO_DEVICE_IN_HDMI_ARC:
        input_src = ARCIN;
        break;
    case AUDIO_DEVICE_IN_LINE:
        input_src = LINEIN;
        break;
    case AUDIO_DEVICE_IN_SPDIF:
        input_src = SPDIFIN;
        break;
    case AUDIO_DEVICE_IN_TV_TUNER:
        input_src = ATV;
        break;
    case AUDIO_DEVICE_IN_REMOTE_SUBMIX:
    case AUDIO_DEVICE_IN_WIRED_HEADSET:
    case AUDIO_DEVICE_IN_BUILTIN_MIC: /* fallthrough */
    case AUDIO_DEVICE_IN_BACK_MIC:
    case AUDIO_DEVICE_IN_ECHO_REFERENCE:
        input_src = SRC_NA;
        break;

    default:
        input_src = SRC_NA;
        break;
    }
    return input_src;
}

const char* patchSrc2Str(enum patch_src_assortion type)
{
    ENUM_TYPE_TO_STR_START("SRC_");
    ENUM_TYPE_TO_STR(SRC_DTV)
    ENUM_TYPE_TO_STR(SRC_ATV)
    ENUM_TYPE_TO_STR(SRC_LINEIN)
    ENUM_TYPE_TO_STR(SRC_HDMIIN)
    ENUM_TYPE_TO_STR(SRC_SPDIFIN)
    ENUM_TYPE_TO_STR(SRC_REMOTE_SUBMIXIN)
    ENUM_TYPE_TO_STR(SRC_WIRED_HEADSETIN)
    ENUM_TYPE_TO_STR(SRC_BUILTIN_MIC)
    ENUM_TYPE_TO_STR(SRC_ECHO_REFERENCE)
    ENUM_TYPE_TO_STR(SRC_ARCIN)
    ENUM_TYPE_TO_STR(SRC_OTHER)
    ENUM_TYPE_TO_STR(SRC_INVAL)
    ENUM_TYPE_TO_STR_END
}

const char* usecase2Str(stream_usecase_t type)
{
    ENUM_TYPE_TO_STR_START("STREAM_");
    ENUM_TYPE_TO_STR(STREAM_PCM_NORMAL)
    ENUM_TYPE_TO_STR(STREAM_PCM_DIRECT)
    ENUM_TYPE_TO_STR(STREAM_PCM_HWSYNC)
    ENUM_TYPE_TO_STR(STREAM_RAW_DIRECT)
    ENUM_TYPE_TO_STR(STREAM_RAW_HWSYNC)
    ENUM_TYPE_TO_STR(STREAM_PCM_PATCH)
    ENUM_TYPE_TO_STR(STREAM_RAW_PATCH)
    ENUM_TYPE_TO_STR(STREAM_PCM_MMAP)
    ENUM_TYPE_TO_STR(STREAM_USECASE_MAX)
    ENUM_TYPE_TO_STR_END
}

const char* outputPort2Str(enum OUT_PORT type)
{
    ENUM_TYPE_TO_STR_START("OUTPORT_");
    ENUM_TYPE_TO_STR(OUTPORT_SPEAKER)
    ENUM_TYPE_TO_STR(OUTPORT_HDMI_ARC)
    ENUM_TYPE_TO_STR(OUTPORT_HDMI)
    ENUM_TYPE_TO_STR(OUTPORT_SPDIF)
    ENUM_TYPE_TO_STR(OUTPORT_AUX_LINE)
    ENUM_TYPE_TO_STR(OUTPORT_HEADPHONE)
    ENUM_TYPE_TO_STR(OUTPORT_REMOTE_SUBMIX)
    ENUM_TYPE_TO_STR(OUTPORT_A2DP)
    ENUM_TYPE_TO_STR(OUTPORT_BT_SCO)
    ENUM_TYPE_TO_STR(OUTPORT_BT_SCO_HEADSET)
    ENUM_TYPE_TO_STR(OUTPORT_MAX)
    ENUM_TYPE_TO_STR_END
}

const char* inputPort2Str(enum IN_PORT type)
{
    ENUM_TYPE_TO_STR_START("INPORT_");
    ENUM_TYPE_TO_STR(INPORT_TUNER)
    ENUM_TYPE_TO_STR(INPORT_HDMIIN)
    ENUM_TYPE_TO_STR(INPORT_SPDIF)
    ENUM_TYPE_TO_STR(INPORT_LINEIN)
    ENUM_TYPE_TO_STR(INPORT_REMOTE_SUBMIXIN)
    ENUM_TYPE_TO_STR(INPORT_WIRED_HEADSETIN)
    ENUM_TYPE_TO_STR(INPORT_BUILTIN_MIC)
    ENUM_TYPE_TO_STR(INPORT_ECHO_REFERENCE)
    ENUM_TYPE_TO_STR(INPORT_ARCIN)
    ENUM_TYPE_TO_STR(INPORT_MAX)
    ENUM_TYPE_TO_STR_END
}

const char* mixerInputType2Str(aml_mixer_input_port_type_e type)
{
    ENUM_TYPE_TO_STR_START("AML_MIXER_INPUT_PORT_");
    ENUM_TYPE_TO_STR(AML_MIXER_INPUT_PORT_INVAL)
    ENUM_TYPE_TO_STR(AML_MIXER_INPUT_PORT_PCM_SYSTEM)
    ENUM_TYPE_TO_STR(AML_MIXER_INPUT_PORT_PCM_DIRECT)
    ENUM_TYPE_TO_STR(AML_MIXER_INPUT_PORT_PCM_MMAP)
    ENUM_TYPE_TO_STR(AML_MIXER_INPUT_PORT_BUTT)
    ENUM_TYPE_TO_STR_END
}
const char* mediasyncAudiopolicyType2Str(audio_policy type)
{
    ENUM_TYPE_TO_STR_START("MEDIASYNC_AUDIO_");
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_NORMAL_OUTPUT)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_DROP_PCM)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_INSERT)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_HOLD)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_MUTE)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_RESAMPLE)
    ENUM_TYPE_TO_STR(MEDIASYNC_AUDIO_ADJUST_CLOCK)
    ENUM_TYPE_TO_STR_END
}

const char* dtvAudioPatchCmd2Str(AUDIO_DTV_PATCH_CMD_TYPE type)
{
    ENUM_TYPE_TO_STR_START("AUDIO_DTV_PATCH_");
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_NULL)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_START)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_PAUSE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_RESUME)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_STOP)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_SUPPORT)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_VOLUME)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_MUTE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_OUTPUT_MODE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_PRE_GAIN)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_PRE_MUTE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_OPEN)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_CLOSE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_DEMUX_INFO)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_SECURITY_MEM_LEVEL)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_HAS_VIDEO)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_CONTROL)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_PID)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_FMT)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_PID)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_FMT)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_ENABLE)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_MIX_LEVEL)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_AD_VOL_LEVEL)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_SET_MEDIA_SYNC_ID)
    ENUM_TYPE_TO_STR(AUDIO_DTV_PATCH_CMD_NUM)
    ENUM_TYPE_TO_STR_END
}

const char* hdmiFormat2Str(AML_HDMI_FORMAT_E type)
{
    ENUM_TYPE_TO_STR_START("AML_HDMI_FORMAT_");
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_LPCM)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_AC3)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_MPEG1)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_MP3)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_MPEG2MC)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_AAC)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_DTS)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_ATRAC)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_OBA)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_DDP)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_DTSHD)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_MAT)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_DST)
    ENUM_TYPE_TO_STR(AML_HDMI_FORMAT_WMAPRO)
    ENUM_TYPE_TO_STR_END
}

int convert_audio_format_2_period_mul(audio_format_t format)
{
    int period_mul = 1;

    switch (format) {
    case AUDIO_FORMAT_E_AC3:
        period_mul = EAC3_MULTIPLIER;
        break;
    case AUDIO_FORMAT_DTS_HD:
        // 192Khz
    case AUDIO_FORMAT_MAT:
        period_mul = HBR_MULTIPLIER;
        break;
    case AUDIO_FORMAT_PCM_32_BIT:
        period_mul = 2;
    default:
        period_mul = 1;
        break;
    }

    return period_mul;
}

/*****************************************************************************
*   Function Name:  aml_audio_trace_debug_level
*   Description:    detect audio trace debug level.
*   Parameters:     void.
*   Return value:   0: debug is closed, or else debug is valid.
******************************************************************************/
int aml_audio_trace_debug_level(void)
{
    char buf[PROPERTY_VALUE_MAX] = {'\0'};
    int ret = -1;
    int debug_level = 0;
    ret = property_get("vendor.audio.hal.trace.debug", buf, NULL);
    if (ret > 0) {
        debug_level = atoi(buf);
    }

    //ALOGV("%s:  debug_level:%d", __func__, debug_level);
    return debug_level;
}

/*****************************************************************************
*   Function Name:  aml_audio_trace_int
*   Description:    trace's feature implement in audio hal.
*   Parameters:     char *: trace name.
*                   int: trace value.
*   Return value:   0: just a return value, no real meaning.
******************************************************************************/
int aml_audio_trace_int(char *name, int value)
{
    int debug_level = aml_audio_trace_debug_level();

    if (debug_level > 0) {
        ATRACE_INT(name, value);
    } else {
        // do nothing.
    }

    return 0;
}
