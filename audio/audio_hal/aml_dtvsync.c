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



#define LOG_TAG "aml_dtvsync"
#define LOG_NDEBUG 0
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cutils/log.h>
#include <string.h>
#include <errno.h>
#include <sys/utsname.h>
#include <cutils/properties.h>
#include "audio_hw_utils.h"
#include "aml_audio_spdifout.h"
#include "audio_hw_ms12_v2.h"
#define DD_MUTE_FRAME_SIZE 1536
#define EAC3_IEC61937_FRAME_SIZE 24576
#define MS12_MAT_RAW_LENGTH                 (0x0f7e)

extern int aml_audio_ms12_process_wrapper(struct audio_stream_out *stream, const void *write_buf, size_t write_bytes);

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

const unsigned int m_mute_dd_frame[] = {
    0x5d9c770b, 0xf0432014, 0xf3010713, 0x2020dc62, 0x4842020, 0x57100404, 0xf97c3e1f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0xf97c75fe, 0x9fcfe7f3,
    0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xfb7c3e9f, 0x3e5f9dff, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0x48149ff2, 0x2091,
    0x361e0000, 0x78bc6ddb, 0xbbbbe3f1, 0xb8, 0x0, 0x0, 0x0, 0x77770700, 0x361e8f77, 0x359f6fdb, 0xd65a6bad, 0x5a6badb5, 0x6badb5d6, 0xa0b5d65a, 0x1e000000, 0xbc6ddb36,
    0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35, 0xa6b5d6, 0x0, 0xb66de301, 0x1e8fc7db, 0x80bbbb3b, 0x0, 0x0,
    0x0, 0x0, 0x78777777, 0xb66de3f1, 0xd65af3f9, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x5a6b, 0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0,
    0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0x605a, 0x1e000000, 0xbc6ddb36, 0xbbe3f178, 0xb8bb, 0x0, 0x0, 0x0, 0x77070000, 0x1e8f7777, 0x9f6fdb36, 0x5a6bad35,
    0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0xa0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0, 0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xa6b5d65a, 0x0,
    0x6de30100, 0x8fc7dbb6, 0xbbbb3b1e, 0x80, 0x0, 0x0, 0x0, 0x77777700, 0x6de3f178, 0x5af3f9b6, 0x6badb5d6, 0xadb5d65a, 0xb5d65a6b, 0x5a6bad, 0xe3010000, 0xc7dbb66d,
    0xbb3b1e8f, 0x80bb, 0x0, 0x0, 0x0, 0x77770000, 0xe3f17877, 0xf3f9b66d, 0xadb5d65a, 0x605a6b, 0x0, 0x6ddb361e, 0xe3f178bc, 0xb8bbbb, 0x0, 0x0,
    0x0, 0x7000000, 0x8f777777, 0x6fdb361e, 0x6bad359f, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xa0b5, 0xdb361e00, 0xf178bc6d, 0xb8bbbbe3, 0x0, 0x0, 0x0, 0x0,
    0x77777707, 0xdb361e8f, 0xad359f6f, 0xb5d65a6b, 0x10200a6, 0x0, 0xdbb6f100, 0x8fc7e36d, 0xc0dddd1d, 0x0, 0x0, 0x0, 0x0, 0xbcbbbb3b, 0xdbb6f178, 0x6badf97c,
    0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0xadb5, 0xb6f10000, 0xc7e36ddb, 0xdddd1d8f, 0xc0, 0x0, 0x0, 0x0, 0xbbbb3b00, 0xb6f178bc, 0xadf97cdb, 0xb5d65a6b, 0x4deb00ad
};

const unsigned int m_mute_ddp_frame[] = {
    0x7f01770b, 0x20e06734, 0x2004, 0x8084500, 0x404046c, 0x1010104, 0xe7630001, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xce7f9fcf, 0x7c3e9faf,
    0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xf37f9fcf, 0x9fcfe7ab, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x53dee7f3, 0xf0e9,
    0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d,
    0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0,
    0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0,
    0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0, 0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a,
    0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000, 0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0,
    0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5, 0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db,
    0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x2060, 0x6d3c0000, 0xf178dbb6, 0x7777c7e3, 0x70, 0x0, 0x0,
    0x0, 0xeeee0e00, 0x6d3c1eef, 0x6b3edfb6, 0xadb5d65a, 0xb5d65a6b, 0xd65a6bad, 0x406badb5, 0x3c000000, 0x78dbb66d, 0x77c7e3f1, 0x7077, 0x0, 0x0, 0x0, 0xee0e0000,
    0x3c1eefee, 0x3edfb66d, 0xb5d65a6b, 0x20606bad, 0x0, 0xdbb66d3c, 0xc7e3f178, 0x707777, 0x0, 0x0, 0x0, 0xe000000, 0x1eefeeee, 0xdfb66d3c, 0xd65a6b3e, 0x5a6badb5,
    0x6badb5d6, 0xadb5d65a, 0x406b, 0xb66d3c00, 0xe3f178db, 0x707777c7, 0x0, 0x0, 0x0, 0x0, 0xefeeee0e, 0xb66d3c1e, 0x5a6b3edf, 0x6badb5d6, 0x40, 0x7f227c55,
};

/*this mat mute data is 20ms*/
static const unsigned int ms12_muted_mat_raw[MS12_MAT_RAW_LENGTH / 4 + 1] = {
    0x4009e07,  0x3010184,   0x858085, 0xd903c422, 0x47021181,  0x8030680,  0x1089c11,    0x1091f, 0x85800104, 0x47021183,    0x10c80,  0x8c24341,       0x2f,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0x1183c300,  0xc804702, 0x43410101,   0x2f08c2,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc3000000,
    0x5efc1c5, 0x1182db03,  0x6804702, 0x9c110803,  0x91f0108,  0x1040001, 0x11838580,  0xc804702, 0x43410001,   0x2f08c2,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc3000000, 0x47021183,  0x1010c80,  0x8c24341,       0x2f,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
            0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0, 0xc2c5c300, 0xc4c5c0c5,     0x8cba,
};


void* aml_dtvsync_create()
{
    ALOGI("%s", __func__);
	return mediasync_wrap_create();
}

bool aml_dtvsync_allocinstance(aml_dtvsync_t *p_dtvsync, int32_t* id)
{
    if (p_dtvsync && p_dtvsync->mediasync) {

        return mediasync_wrap_allocInstance(p_dtvsync->mediasync, 0, 0, id);
    }
    return false;
}

bool aml_dtvsync_bindinstance(aml_dtvsync_t *p_dtvsync, uint32_t id)
{

    if (p_dtvsync) {
        return mediasync_wrap_bindInstance(p_dtvsync->mediasync, id, MEDIA_AUDIO);
    }
    return false;
}

bool aml_dtvsync_setParameter(aml_dtvsync_t *p_dtvsync, mediasync_parameter type, void* arg)
{

    if (p_dtvsync) {
        return mediasync_wrap_setParameter(p_dtvsync->mediasync, type, arg);
    }
    return false;
}

bool aml_dtvsync_getParameter(aml_dtvsync_t *p_dtvsync, mediasync_parameter type, void* arg)
{
    if (p_dtvsync) {
         return mediasync_wrap_getParameter(p_dtvsync->mediasync, type, arg);
    }
    return false;
}

bool aml_dtvsync_queue_audio_frame(aml_dtvsync_t *p_dtvsync, struct mediasync_audio_queue_info* info)
{

    if(p_dtvsync) {
        return mediasync_wrap_queueAudioFrame(p_dtvsync->mediasync, info);
    }
    return false;
}

bool aml_dtvsync_audioprocess(aml_dtvsync_t *p_dtvsync, int64_t apts, int64_t cur_apts,
                                                mediasync_time_unit tunit,
                                                struct mediasync_audio_policy* asyncPolicy)
{
    if (p_dtvsync) {
        return mediasync_wrap_AudioProcess(p_dtvsync->mediasync, apts, cur_apts, tunit, asyncPolicy);
    }
    return false;
}

bool aml_dtvsync_insertpcm(struct audio_stream_out *stream, audio_format_t format, int time_ms, bool is_ms12)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    int insert_size = 0, times = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int t1 = 0;
    int ret = 0;
    ALOGI("insert time_ms=%d ms, is_ms12=%d\n", time_ms, is_ms12);
    insert_size =  192 * time_ms;

    memset(patch->out_buf, 0, patch->out_buf_size);
    if (insert_size <= patch->out_buf_size) {
        if (!is_ms12) {
            aml_hw_mixer_mixing(&adev->hw_mixer, patch->out_buf, insert_size, format);
            if (audio_hal_data_processing(stream, patch->out_buf, insert_size, &output_buffer,
                &output_buffer_bytes, format) == 0) {
                hw_write(stream, output_buffer, output_buffer_bytes, format);
            }
        } else {
            ret = aml_audio_ms12_process_wrapper(stream, patch->out_buf, insert_size);
        }
        return true;
    }
    if (patch->out_buf_size != 0)
        t1 = insert_size / patch->out_buf_size;
    else  {
        ALOGI("fatal error out_buf_size is 0\n");
        return false;
    }
    ALOGI("set t1=%d\n", t1);
    for (int i = 0; i < t1; i++) {
        if (!is_ms12) {
            aml_hw_mixer_mixing(&adev->hw_mixer, patch->out_buf, patch->out_buf_size, format);
            if (audio_hal_data_processing(stream, patch->out_buf, insert_size, &output_buffer,
                &output_buffer_bytes, format) == 0) {
                hw_write(stream, output_buffer, output_buffer_bytes, format);
            }
        } else {
            ret = aml_audio_ms12_process_wrapper(stream, patch->out_buf, patch->out_buf_size);
        }
    }
    return true;
}

bool aml_dtvsync_spdif_insertraw(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms, int is_packed) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    unsigned char buffer[EAC3_IEC61937_FRAME_SIZE];
    int t1 = 0;
    int size = 0;
    t1 = time_ms / 32;

    memset(buffer, 0, sizeof(buffer));

    if (is_packed) {
        memcpy(buffer, muted_frame_dd, sizeof(muted_frame_dd));
        size = sizeof(muted_frame_dd);
        ALOGI("packet dd size = %d\n",  size);
    } else {
        memcpy(buffer, m_mute_ddp_frame, sizeof(m_mute_ddp_frame));
        size =  sizeof(m_mute_ddp_frame);
        ALOGI("non-packet ddp size = %d\n", size);
    }
    for (int i = 0; i < t1; i++)
        aml_audio_spdifout_processs(*spdifout_handle, buffer, size);
    return  true;
}

bool aml_audio_spdif_insertpcm(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    int insert_size = 0;
    int t1 = 0;

    insert_size =  192 * time_ms;

    if (insert_size <=  patch->out_buf_size) {
        memset(patch->out_buf, 0, patch->out_buf_size);
        aml_audio_spdifout_processs(*spdifout_handle, patch->out_buf, insert_size);
        return true;
    }

    if (patch->out_buf_size != 0) {
        t1 = insert_size / patch->out_buf_size;
    } else  {
        ALOGI("fatal error out_buf_size is 0\n");
        return false;
    }

    ALOGI("t1=%d\n", t1);

    for (int i = 0; i < t1; i++) {
        memset(patch->out_buf, 0, patch->out_buf_size);
        aml_audio_spdifout_processs(*spdifout_handle, patch->out_buf, patch->out_buf_size);
    }
    return true;
}
bool aml_dtvsync_adjustclock(struct audio_stream_out *stream, struct mediasync_audio_policy *p_policy)
{
    int direct = -1;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_dec_t *aml_dec = aml_out->aml_dec;

    direct = p_policy->param1;
    ALOGI("func:%s, direct =%d\n", __FUNCTION__, direct);
    if (direct >= 0 && direct <= 2) {
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
        if (aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {
            if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                if (adev->dual_spdif_support) {
                    dtv_adjust_spdif_output_clock(patch, direct,
                                                patch->i2s_step_clk / patch->i2s_div_factor, false);
                    dtv_adjust_spdif_output_clock(patch, direct,
                                                4 * patch->i2s_step_clk / patch->i2s_div_factor , true);
                } else {
                    dtv_adjust_spdif_output_clock(patch, direct,
                                                patch->i2s_step_clk / patch->i2s_div_factor, false);
                }
            }
        }

        return true;
    } else {
        ALOGE("adjust abnormal\n");
        return false;
    }
}

bool aml_dtvsync_ms12_adjust_clock(struct audio_stream_out *stream, int direct)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct bitstream_out_desc *bitstream_out;
    int i = 0;

    ALOGI("func:%s, direct = %d\n", __FUNCTION__, direct);
    if (direct >= 0 && direct <= 2) {
        ALOGI("step = %d, patch->i2s_div_factor = %d\n", patch->i2s_step_clk / patch->i2s_div_factor, patch->i2s_div_factor);
        dtv_adjust_i2s_output_clock(patch, direct, patch->i2s_step_clk / patch->i2s_div_factor);
        for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
            bitstream_out = &ms12->bitstream_out[i];
            if (bitstream_out->spdifout_handle != NULL) {
                if (bitstream_out->audio_format == AUDIO_FORMAT_E_AC3) {
                    dtv_adjust_spdif_output_clock(patch, direct,
                            4 * patch->i2s_step_clk / patch->i2s_div_factor , true);

                } else if (bitstream_out->audio_format == AUDIO_FORMAT_AC3) {
                    dtv_adjust_spdif_output_clock(patch, direct,
                            patch->i2s_step_clk / patch->i2s_div_factor, false);

                } else if (bitstream_out->audio_format == AUDIO_FORMAT_MAT) {
                    dtv_adjust_spdif_output_clock(patch, direct,
                            16 * patch->i2s_step_clk / patch->i2s_div_factor , true);
                }
            }
        }
        return true;
    } else {
        ALOGE("adjust abnormal\n");
        return false;
    }
}

int aml_dtvsync_nonms12_process_insert(struct audio_stream_out *stream,
                                    struct mediasync_audio_policy *p_policy)
{
    int insert_time_ms = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    dec_data_info_t * raw_in_data = NULL;
    if (aml_dec) {
        raw_in_data  = &aml_dec->raw_in_data;
    } else {
        ALOGI("aml_dec is null, return -1\n");
        return -1;
    }
    insert_time_ms = p_policy->param1/1000;
    ALOGI("before insert :%d\n", insert_time_ms);
    do {

        if (patch->output_thread_exit == 1) {
            ALOGI("input exit, break now\n");
            break;
        }

        aml_dtvsync_insertpcm(stream, AUDIO_FORMAT_PCM_16_BIT, 32, false);

        if (audio_is_linear_pcm(aml_dec->format) && raw_in_data->data_ch > 2) {
            aml_audio_spdif_insertpcm(stream, &aml_out->spdifout_handle, 32);
        }

        if (aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {

            if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                if (adev->dual_spdif_support) {
                    /*output raw ddp to hdmi*/
                    if (aml_dec->format == AUDIO_FORMAT_E_AC3 &&
                        aml_out->optical_format == AUDIO_FORMAT_E_AC3) {

                        aml_dtvsync_spdif_insertraw(stream,  &aml_out->spdifout_handle,
                                                32, 0);//insert non-IEC packet
                    }

                    /*output dd data to spdif*/
                    aml_dtvsync_spdif_insertraw(stream,  &aml_out->spdifout2_handle,
                                            32, 1);
                } else {

                    aml_dtvsync_spdif_insertraw(stream,  &aml_out->spdifout_handle,
                                            32, 0);//insert non-IEC packet
                }
            }
        }

        insert_time_ms -= 32;

    } while (insert_time_ms  > 0);

    ALOGI("after insert time\n");
    return 0;
}

int aml_dtvsync_ms12_process_insert(void *priv_data, int insert_time_ms,
                                    aml_ms12_dec_info_t *ms12_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct bitstream_out_desc *bitstream_out;
    unsigned char buffer[EAC3_IEC61937_FRAME_SIZE];
    audio_format_t output_format = (ms12_info) ? ms12_info->data_type : AUDIO_FORMAT_PCM_16_BIT;
    int t1 = 0;
    int i = 0;
    int insert_ms = 0;
    int size = 0;
    bool is_mat = false;

    for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
        bitstream_out = &ms12->bitstream_out[i];
        if ((bitstream_out->audio_format == AUDIO_FORMAT_AC3) ||
            (bitstream_out->audio_format == AUDIO_FORMAT_E_AC3)) {
            t1 = insert_time_ms / 32;
            break;
        } else if (bitstream_out->audio_format == AUDIO_FORMAT_MAT) {
            t1 = insert_time_ms / 20;
            is_mat = true;
        }
    }

    if (is_mat)
        insert_ms = 20;
    else
        insert_ms = 32;

    ALOGI("inset_time_ms=%d, insert_ms=%d, t1=%d, is_mat=%d\n",
            insert_time_ms, insert_ms, t1, is_mat);

    memset(patch->out_buf, 0, patch->out_buf_size);

    do {

        if (patch->output_thread_exit == 1) {
                ALOGI("input exit, break now\n");
                break;
        }

        if (audio_is_linear_pcm(output_format)) {

            if (ms12_info->pcm_type == DAP_LPCM) {
                dap_pcm_output(patch->out_buf, priv_data, 192*insert_ms, ms12_info);
            } else {
                stereo_pcm_output(patch->out_buf, priv_data, 192*insert_ms, ms12_info);
            }
        } else {
            if (is_mat) {
                size = sizeof(ms12_muted_mat_raw);
                memcpy(buffer, ms12_muted_mat_raw, size);
                mat_bitstream_output(buffer, priv_data, size);
            } else {
                for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {

                    bitstream_out = &ms12->bitstream_out[i];

                    if (bitstream_out->spdifout_handle != NULL) {
                        if (bitstream_out->audio_format == AUDIO_FORMAT_E_AC3) {
                            size = sizeof(m_mute_ddp_frame);
                            memcpy(buffer,  m_mute_ddp_frame, size);
                            bitstream_output(buffer, priv_data, size);
                        } else if (bitstream_out->audio_format == AUDIO_FORMAT_AC3) {
                            size = sizeof(m_mute_dd_frame);
                            memcpy(buffer,  m_mute_dd_frame, size);
                            spdif_bitstream_output(buffer, priv_data, size);
                        }
                    }
                }
            }
        }
        insert_time_ms -= insert_ms;
    } while(insert_time_ms > 0);

    return 0;
}

int aml_dtvsync_process_resample(struct audio_stream_out *stream,
                                struct mediasync_audio_policy *p_policy,
                                bool *speed_enabled)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    float speed = 0.0f;
    int ret = -1;

    if (p_policy->param2 != 0)
        speed = ((float)(p_policy->param1)) / p_policy->param2;
    else
        ALOGI("Warning speed error\n");

    ALOGI("new speed=%f,  output_speed=%f\n", speed, aml_out->output_speed);

    if (speed != 1.0f) {
        *speed_enabled = true;

        if (speed != aml_out->output_speed) {
            ALOGE("aml_audio_set_output_speed set speed :%f --> %f.\n",
                aml_out->output_speed, speed);
        }

    } else
        *speed_enabled = false;

    aml_out->output_speed = speed;

    return 0;
}

dtvsync_process_res  aml_dtvsync_nonms12_process(struct audio_stream_out *stream, int duration, bool *speed_enabled)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_dec_t *aml_dec = aml_out->aml_dec;
    dec_data_info_t * dec_pcm_data;
    float speed = 0.0f;
    struct mediasync_audio_policy m_audiopolicy;
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));

    if (patch->dtvsync->duration == 0 ||
        (duration > 0 && duration < patch->dtvsync->duration)) {

        ALOGI("set duration from: %d to:%d \n", patch->dtvsync->duration, duration);
        patch->dtvsync->duration = duration;
    }

    do {

        aml_dtvsync_audioprocess(patch->dtvsync, aml_dec->out_frame_pts,
                                patch->dtvsync->cur_outapts,
                                MEDIASYNC_UNIT_PTS, &m_audiopolicy);
        if (m_audiopolicy.audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT)
            ALOGI("do get m_audiopolicy=%d=%s, param1=%u, param2=%u, out_pts=0x%llx,cur=0x%llx,exit=%d\n",
                m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
                m_audiopolicy.param1, m_audiopolicy.param2,
                aml_dec->out_frame_pts, patch->dtvsync->cur_outapts,
                patch->output_thread_exit);

        if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD)
            usleep(15*1000);

        if (patch->output_thread_exit == 1) {
                ALOGI("input exit, break now\n");
                break;
        }

    } while (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD);

    if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {
        patch->dtvsync->cur_outapts = aml_dec->out_frame_pts;
        return DTVSYNC_AUDIO_DROP;
    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_INSERT) {

        aml_dtvsync_nonms12_process_insert(stream,  &m_audiopolicy);

    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {

        aml_dtvsync_adjustclock(stream, &m_audiopolicy);

    } else if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {

        aml_dtvsync_process_resample(stream, &m_audiopolicy, speed_enabled);
    }

    return DTVSYNC_AUDIO_OUTPUT;
}

void aml_dtvsync_ms12_get_policy(struct audio_stream_out *stream)
{

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct mediasync_audio_policy m_audiopolicy;
    memset(&m_audiopolicy, 0, sizeof(m_audiopolicy));

    do {
        aml_dtvsync_audioprocess(patch->dtvsync, patch->cur_package->pts,
                                patch->dtvsync->cur_outapts,
                                MEDIASYNC_UNIT_PTS, &m_audiopolicy);

        if (m_audiopolicy.audiopolicy != MEDIASYNC_AUDIO_NORMAL_OUTPUT)
            ALOGI("do get m_audiopolicy=%d=%s, param1=%u, param2=%u, out_pts=0x%llx,cur=0x%llx\n",
                m_audiopolicy.audiopolicy, mediasyncAudiopolicyType2Str(m_audiopolicy.audiopolicy),
                m_audiopolicy.param1, m_audiopolicy.param2,
                patch->cur_package->pts, patch->dtvsync->cur_outapts);
        if (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD)
            usleep(15*1000);

        if (patch->output_thread_exit == 1) {
            ALOGI("input exit, break now\n");
            break;
        }

    } while (m_audiopolicy.audiopolicy == MEDIASYNC_AUDIO_HOLD);
    patch->dtvsync->apolicy.audiopolicy= (dtvsync_policy)m_audiopolicy.audiopolicy;
    patch->dtvsync->apolicy.param1 = m_audiopolicy.param1;
    patch->dtvsync->apolicy.param2 = m_audiopolicy.param2;
}

dtvsync_process_res aml_dtvsync_ms12_process_policy(void *priv_data, aml_ms12_dec_info_t *ms12_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_dtvsync_t *aml_dtvsync = patch->dtvsync;
    struct dtvsync_audio_policy *async_policy = NULL;

    if (aml_dtvsync != NULL) {
        async_policy = &(aml_dtvsync->apolicy);
        ALOGI("cur policy:%d, prm1:%d, prm2:%d\n", async_policy->audiopolicy,
            async_policy->param1, async_policy->param2);

        if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {

            return DTVSYNC_AUDIO_DROP;

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_INSERT) {

            aml_dtvsync_ms12_process_insert(priv_data, async_policy->param1/1000, ms12_info);

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_ADJUST_CLOCK) {

            aml_dtvsync_ms12_adjust_clock(stream_out, async_policy->param1);

        } else if (async_policy->audiopolicy == MEDIASYNC_AUDIO_RESAMPLE) {

            //aml_dtvsync_ms12_process_resample(stream, async_policy);

        }
    }

    async_policy->audiopolicy = DTVSYNC_AUDIO_UNKNOWN;
    return DTVSYNC_AUDIO_OUTPUT;
}

bool aml_dtvsync_setPause(aml_dtvsync_t *p_dtvsync, bool pause)
{
    if (p_dtvsync) {
        return mediasync_wrap_setPause(p_dtvsync->mediasync, pause);
    }
    return false;
}

bool aml_dtvsync_reset(aml_dtvsync_t *p_dtvsync)
{

    if (p_dtvsync) {
        return mediasync_wrap_reset(p_dtvsync->mediasync);
    }
    return false;
}

void aml_dtvsync_release(aml_dtvsync_t *p_dtvsync)
{

    if (p_dtvsync) {
        mediasync_wrap_destroy(p_dtvsync->mediasync);
    }
}
