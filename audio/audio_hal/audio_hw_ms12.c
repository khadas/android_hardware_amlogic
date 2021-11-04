/*
 * Copyright (C) 2017 Amlogic Corporation.
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
#define __USE_GNU

#include <cutils/log.h>
#include <dolby_ms12.h>
#include <dolby_ms12_config_params.h>
#include <dolby_ms12_status.h>
#include <aml_android_utils.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <inttypes.h>

#include "audio_hw_ms12.h"
#include "alsa_config_parameters.h"
#include "aml_ac3_parser.h"
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include "audio_hw.h"
#include "alsa_manager.h"
#include "aml_audio_stream.h"
#include "dolby_lib_api.h"
#include "aml_audio_timer.h"
#include "audio_virtual_buf.h"
#include "ac3_parser_utils.h"
#include "aml_audio_ac3parser.h"
#include "audio_hw_utils.h"
#include "aml_audio_ms12_bypass.h"
#include "aml_audio_ac4parser.h"
#include "aml_volume_utils.h"
#include "aml_audio_spdifdec.h"
#include "aml_audio_matparser.h"
#include "aml_audio_spdifout.h"


#define DOLBY_DRC_LINE_MODE 0
#define DOLBY_DRC_RF_MODE   1

#define DDP_MAX_BUFFER_SIZE 2560//dolby ms12 input buffer threshold

#define MS12_MAIN_INPUT_BUF_PCM_NS         (96000000LL)
#define MS12_MAIN_INPUT_BUF_NONEPCM_NS     (160000000LL)
#define MS12_MAIN_INPUT_BUF_NS_UPTHRESHOLD (160000000LL)
#define MS12_MAIN_INPUT_BUF_NS_UPTHRESHOLD_AC4 (256000000LL)

/*if we choose 96ms, it will cause audio filnger underrun,
  if we choose 64ms, it will cause ms12 underrun,
  so we choose 84ms now
*/
#define MS12_SYS_INPUT_BUF_NS  (84000000LL)

#define NANO_SECOND_PER_SECOND 1000000000LL
#define NANO_SECOND_PER_MILLISECOND 1000000LL

#define CONVERT_NS_TO_48K_FRAME_NUM(ns)    (ns * 48 / NANO_SECOND_PER_MILLISECOND)

#define MS12_MAIN_BUF_INCREASE_TIME_MS (0)
#define MS12_SYS_BUF_INCREASE_TIME_MS (1000)
#define DDPI_UDC_COMP_LINE 2

#define MS12_PCM_FRAME_SIZE         (6144)
#define MS12_DD_FRAME_SIZE          (6144)
#define MS12_DDP_FRAME_SIZE         (24576)

#define MS12_DUMP_PROPERTY               "vendor.media.audiohal.ms12dump"

#define DUMP_MS12_OUTPUT_SPEAKER_PCM     0x1
#define DUMP_MS12_OUTPUT_SPDIF_PCM       0x2
#define DUMP_MS12_OUTPUT_BITSTREAN       0x4
#define DUMP_MS12_OUTPUT_BITSTREAN2      0x8
#define DUMP_MS12_OUTPUT_BITSTREAN_MAT   0x10
#define DUMP_MS12_INPUT_MAIN             0x100
#define DUMP_MS12_INPUT_SYS              0x200
#define DUMP_MS12_INPUT_APP              0x400
#define DUMP_MS12_INPUT_ASSOCIATE        0x800


#define MS12_OUTPUT_SPEAKER_PCM_FILE     "/data/vendor/audiohal/ms12_speaker_pcm.raw"
#define MS12_OUTPUT_SPDIF_PCM_FILE       "/data/vendor/audiohal/ms12_spdif_pcm.raw"
#define MS12_OUTPUT_BITSTREAM_FILE       "/data/vendor/audiohal/ms12_bitstream.raw"
#define MS12_OUTPUT_BITSTREAM2_FILE      "/data/vendor/audiohal/ms12_bitstream2.raw"
#define MS12_OUTPUT_BITSTREAM_MAT_FILE   "/data/vendor/audiohal/ms12_bitstream.mat"
#define MS12_INPUT_SYS_PCM_FILE          "/data/vendor/audiohal/ms12_input_sys.pcm"
#define MS12_INPUT_SYS_MAIN_FILE         "/data/vendor/audiohal/ms12_input_main.raw"
#define MS12_INPUT_SYS_ASSOCIATE_FILE    "/data/vendor/audiohal/ms12_input_associate.raw"
#define MS12_INPUT_SYS_APP_FILE          "/data/vendor/audiohal/ms12_input_app.pcm"
#define MS12_INPUT_SYS_MAIN_IEC_FILE     "/data/vendor/audiohal/ms12_input_main_iec.raw"


// Downmix Mode start
#define DOWNMIX_MODE_LtRt (0)
#define DOWNMIX_MODE_LoRo (1)

/* for HE-AAC, currently, it is not used at all. */
#define DOWNMIX_MODE_ARIB (2)

#define MS12_DOWNMIX_MODE_PROPERTY "vendor.media.audio.ms12.downmixmode"
// Downmix Mode end

#define DDP_SYNC_WORD                    0x0B77


#define MS12_MAIN_WRITE_RETIMES             (600)
#define MS12_ATMOS_TRANSITION_THRESHOLD     (3)

#define MS12_BYPASS_DROP_CNT                (5)  /*5 frames is about 150ms*/

#define ms12_to_adev(ms12_ptr)  (struct aml_audio_device *) (((char*) (ms12_ptr)) - offsetof(struct aml_audio_device, ms12))

static const unsigned int ms12_muted_dd_raw[] = {
    0x8f6d770b, 0xffe13024,   0x92f4fc, 0x785502fc, 0x7f188661, 0x3e9fafce, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9,
     0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xfff7f97c, 0xf97cbe3a, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7,
     0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0xfcdfe7f3, 0xe7f3f9ea, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,
     0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0xf37f9fcf, 0x9fcfe7ab, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,
     0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xceff7d3e, 0x7c3e9faf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,
     0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0x3afff7f9, 0x91383ee5, 0x10894422, 0xff9ea0f7, 0x8fc7e3d9, 0xdddddd1d,       0xdc,          0,          0,          0,          0,          0,
     0xbbbb3b00, 0xb66ddbb6, 0x6bcde7db, 0xafb5d65a, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e,     0xc0e7, 0x78bc0300, 0xbbbbe3f1,   0x80bbbb,          0,          0,          0,          0,          0,
     0x77070000, 0x6ddb7677, 0xf97cdbb6, 0xd65a6bad, 0xcfe7f3b5, 0xf97c3e9f, 0x9fcfe7f3, 0xcafb7c3e, 0x577fb903, 0x773c1e8f, 0x70777777,          0,          0,          0,          0,          0,
              0, 0xdbeeeeee, 0x6fdbb66d, 0x6bad359f, 0x7cbed65a, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,          0, 0xc7e3f10e, 0xeeeeee8e,       0xee,          0,          0,          0,          0,
              0, 0xdddd1d00, 0xdbb66ddb, 0xb5e6f36d, 0xd75a6bad, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,     0xe0f3, 0x3cde0100, 0xddddf178,   0xc0dddd,          0,          0,          0,          0,
              0, 0xbb030000, 0xb66dbbbb, 0x7cbe6ddb, 0x6badb5d6, 0xe7f3f95a, 0x7c3e9fcf, 0xcfe7f3f9,   0x7c3e9f, 0x3b000000,     0x7ec0, 0x3d41ef01, 0x8fc7b3ff, 0xbbbb3b1e,     0xb8bb,          0,
              0,          0,          0,          0, 0x77770000, 0xdbb66d77, 0x9acfb76d, 0x6badb5d6, 0xf97c3e5f, 0x9fcfe7f3, 0xf3f97c3e,   0x80cfe7, 0x78070000, 0x77c7e3f1,   0x777777,          0,
              0,          0,          0,          0,  0xe000000, 0xb6edeeee, 0xf9b66ddb, 0xb5d65af3, 0xcfe76bad, 0xf97c3e9f, 0x9fcfe7f3, 0xf7f97c3e, 0xfe720794, 0x783c1eaf, 0xeeeeeeee,       0xe0,
              0,          0,          0,          0,          0, 0xdddddd01, 0xb66ddbb6, 0x5a6b3edf, 0x7cadb5d6, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,       0x3e, 0xc7e31d00, 0xdddd1d8f,     0xdcdd,
              0,          0,          0,          0,          0, 0xbb3b0000, 0x6ddbb6bb, 0xcde7dbb6, 0xb5d65a6b, 0x7c3e9faf, 0xcfe7f3f9, 0xf97c3e9f,   0xc0e7f3, 0xbc030000, 0xbbe3f178, 0x80bbbbbb,
              0,          0,          0,          0,          0,  0x7000000, 0xdb767777, 0x7cdbb66d, 0x5a6badf9, 0xe7f3b5d6, 0x7c3e9fcf, 0xcfe7f3f9, 0xf87c3e9f,          0,   0xfc8077, 0x82de0300,
     0x8f67ff7b, 0x77773c1e,   0x707777,          0,          0,          0,          0,          0, 0xee000000, 0x6ddbeeee, 0x9f6fdbb6, 0x5a6bad35, 0xf97cbed6, 0x9fcfe7f3, 0xf3f97c3e,   0x9fcfe7,
      0xe000000, 0x8ec7e3f1, 0xeeeeeeee,          0,          0,          0,          0,          0,          0, 0xdbdddd1d, 0x6ddbb66d, 0xadb5e6f3, 0xcfd75a6b, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e,
     0xe50e28ef, 0x783c5efd, 0xddddddf1,     0xc0dd,          0,          0,          0,          0,          0, 0xbbbb0300, 0xdbb66dbb, 0xd67cbe6d, 0x5a6badb5, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,
         0x7c3e, 0xc73b0000, 0xbb3b1e8f,   0xb8bbbb,          0,          0,          0,          0,          0, 0x77000000, 0xb66d7777, 0xcfb76ddb, 0xadb5d69a, 0x7c3e5f6b, 0xcfe7f3f9, 0xf97c3e9f,
     0x80cfe7f3,  0x7000000, 0xc7e3f178, 0x77777777,          0,          0,          0,          0,          0,          0, 0xedeeee0e, 0xb66ddbb6, 0xd65af3f9, 0xe76badb5, 0x7c3e9fcf, 0xcfe7f3f9,
     0xf97c3e9f,       0xf0, 0xf801ef00, 0x38080000, 0x1fa03601, 0x2c15dfc7, 0xa1e00baf, 0x82de774b, 0x8f67ff7b, 0x77773c1e,   0x707777,          0,          0,          0,          0,          0,
     0xee000000, 0x6ddbeeee, 0x9f6fdbb6, 0x5a6bad35, 0xf97cbed6, 0x9fcfe7f3, 0xf3f97c3e,   0x9fcfe7,  0xe000000, 0x8ec7e3f1, 0xeeeeeeee,          0,          0,          0,          0,          0,
              0, 0xdbdddd1d, 0x6ddbb66d, 0xadb5e6f3, 0xcfd75a6b, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0xe50e28ef, 0x783c5efd, 0xddddddf1,     0xc0dd,          0,          0,          0,          0,
              0, 0xbbbb0300, 0xdbb66dbb, 0xd67cbe6d, 0x5a6badb5, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,     0x7c3e, 0xc73b0000, 0xbb3b1e8f,   0xb8bbbb,          0,          0,          0,          0,
              0, 0x77000000, 0xb66d7777, 0xcfb76ddb, 0xadb5d69a, 0x7c3e5f6b, 0xcfe7f3f9, 0xf97c3e9f, 0x80cfe7f3,  0x7000000, 0xc7e3f178, 0x77777777,          0,          0,          0,          0,
              0,          0, 0xedeeee0e, 0xb66ddbb6, 0xd65af3f9, 0xe76badb5, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,       0xf0, 0xf801ef00, 0x60090000, 0x593fbd00, 0x9fb871e9, 0xd7dab421, 0xc05f51d9,
     0x2205fedb, 0xb69081dd, 0x3cc496a1, 0x7a59fcef, 0x24127d7c,  0xaaccf0e, 0xe2ecb666, 0x6c96ed43, 0x6d5e3e62, 0xa20a5c81, 0xcb581169, 0xa60e1dd5, 0xf7e93981, 0x7aa42e35, 0xf107b2ac, 0x1cca8ea7,
     0xbdb07be5, 0x937d3f2a, 0xff7b82de, 0x3c1e8f67, 0x77777777,       0x70,          0,          0,          0,          0,          0, 0xeeeeee00, 0xdbb66ddb, 0xad359f6f, 0xbed65a6b, 0xe7f3f97c,
     0x7c3e9fcf, 0xcfe7f3f9,       0x9f, 0xe3f10e00, 0xeeee8ec7,     0xeeee,          0,          0,          0,          0,          0, 0xdd1d0000, 0xb66ddbdd, 0xe6f36ddb, 0x5a6badb5, 0x3e9fcfd7,
     0xe7f3f97c, 0x7c3e9fcf, 0x28eff3f9, 0x5efde50e, 0xddf1783c, 0xc0dddddd,          0,          0,          0,          0,          0,  0x3000000, 0x6dbbbbbb, 0xbe6ddbb6, 0xadb5d67c, 0xf3f95a6b,
     0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf,          0, 0x1e8fc73b, 0xbbbbbb3b,       0xb8,          0,          0,          0,          0,          0, 0x77777700, 0x6ddbb66d, 0xd69acfb7, 0x5f6badb5,
     0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,     0x80cf, 0xf1780700, 0x7777c7e3,     0x7777,          0,          0,          0,          0,          0, 0xee0e0000, 0xdbb6edee, 0xf3f9b66d, 0xadb5d65a,
     0x9fcfe76b, 0xf3f97c3e, 0x3e9fcfe7,   0xf0f97c, 0xef000000,     0xf801, 0x2d035c09, 0xbb5bf290, 0x8ad7c43a, 0x58c3befb, 0xf3e7998a,  0xcfe1bb2, 0x6dca0229, 0xcc0908ba, 0xf77cf51b, 0xa2e4840d,
     0x8d017859, 0x809094b6, 0x3b5690eb, 0x710f31af, 0xf27834c8, 0x765b5cf5, 0xb96f6af9, 0x86761c8f, 0x95303075, 0xa65e6b76, 0x7cc18745, 0xd81947ad, 0x7b82de67, 0x1e8f67ff, 0x7777773c,     0x7077,
              0,          0,          0,          0,          0, 0xeeee0000, 0xb66ddbee, 0x359f6fdb, 0xd65a6bad, 0xf3f97cbe, 0x3e9fcfe7, 0xe7f3f97c,     0x9fcf, 0xf10e0000, 0xee8ec7e3,   0xeeeeee,
              0,          0,          0,          0,          0, 0x1d000000, 0x6ddbdddd, 0xf36ddbb6, 0x6badb5e6, 0x9fcfd75a, 0xf3f97c3e, 0x3e9fcfe7, 0xeff3f97c, 0xfde50e28, 0xf1783c5e, 0xdddddddd,
           0xc0,          0,          0,          0,          0,          0, 0xbbbbbb03, 0x6ddbb66d, 0xb5d67cbe, 0xf95a6bad, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7,       0x7c, 0x8fc73b00, 0xbbbb3b1e,
         0xb8bb,          0,          0,          0,          0,          0, 0x77770000, 0xdbb66d77, 0x9acfb76d, 0x6badb5d6, 0xf97c3e5f, 0x9fcfe7f3, 0xf3f97c3e,   0x80cfe7, 0x78070000, 0x77c7e3f1,
       0x777777,          0,          0,          0,          0,          0,  0xe000000, 0xb6edeeee, 0xf9b66ddb, 0xb5d65af3, 0xcfe76bad, 0xf97c3e9f, 0x9fcfe7f3, 0xf0f97c3e,          0, 0x685c00ef,
};

static const unsigned int ms12_muted_ddp_raw[] = {
    0xff04770b, 0xfaff673f, 0x40000049,  0x4000000,  0x8000000, 0x866100e1, 0x3aff6118, 0xf3f97cbe, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7,
     0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0xeafcdfe7, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,
     0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xabf37f9f, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,
     0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xafceff7d, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,
     0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0xbe3afff7, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf,
     0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c, 0x94ebfcdf, 0x82dee3f8, 0x8f67ff7b, 0x77773c1e,   0x707777,          0,          0,          0,          0,          0,
     0xee000000, 0x6ddbeeee, 0x9f6fdbb6, 0x5a6bad35, 0xf97cbed6, 0x9fcfe7f3, 0xf3f97c3e,   0x9fcfe7,  0xe000000, 0x8ec7e3f1, 0xeeeeeeee,          0,          0,          0,          0,          0,
              0, 0xdbdddd1d, 0x6ddbb66d, 0xadb5e6f3, 0xcfd75a6b, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0xe50e28ef, 0x783c5efd, 0xddddddf1,     0xc0dd,          0,          0,          0,          0,
              0, 0xbbbb0300, 0xdbb66dbb, 0xd67cbe6d, 0x5a6badb5, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,     0x7c3e, 0xc73b0000, 0xbb3b1e8f,   0xb8bbbb,          0,          0,          0,          0,
              0, 0x77000000, 0xb66d7777, 0xcfb76ddb, 0xadb5d69a, 0x7c3e5f6b, 0xcfe7f3f9, 0xf97c3e9f, 0x80cfe7f3,  0x7000000, 0xc7e3f178, 0x77777777,          0,          0,          0,          0,
              0,          0, 0xedeeee0e, 0xb66ddbb6, 0xd65af3f9, 0xe76badb5, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,       0xf0,  0x320ef00, 0xff7b82de, 0x3c1e8f67, 0x77777777,       0x70,          0,
              0,          0,          0,          0, 0xeeeeee00, 0xdbb66ddb, 0xad359f6f, 0xbed65a6b, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9,       0x9f, 0xe3f10e00, 0xeeee8ec7,     0xeeee,          0,
              0,          0,          0,          0, 0xdd1d0000, 0xb66ddbdd, 0xe6f36ddb, 0x5a6badb5, 0x3e9fcfd7, 0xe7f3f97c, 0x7c3e9fcf, 0x28eff3f9, 0x5efde50e, 0xddf1783c, 0xc0dddddd,          0,
              0,          0,          0,          0,  0x3000000, 0x6dbbbbbb, 0xbe6ddbb6, 0xadb5d67c, 0xf3f95a6b, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf,          0, 0x1e8fc73b, 0xbbbbbb3b,       0xb8,
              0,          0,          0,          0,          0, 0x77777700, 0x6ddbb66d, 0xd69acfb7, 0x5f6badb5, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,     0x80cf, 0xf1780700, 0x7777c7e3,     0x7777,
              0,          0,          0,          0,          0, 0xee0e0000, 0xdbb6edee, 0xf3f9b66d, 0xadb5d65a, 0x9fcfe76b, 0xf3f97c3e, 0x3e9fcfe7,   0xf0f97c, 0xef000000, 0x82de0320, 0x8f67ff7b,
     0x77773c1e,   0x707777,          0,          0,          0,          0,          0, 0xee000000, 0x6ddbeeee, 0x9f6fdbb6, 0x5a6bad35, 0xf97cbed6, 0x9fcfe7f3, 0xf3f97c3e,   0x9fcfe7,  0xe000000,
     0x8ec7e3f1, 0xeeeeeeee,          0,          0,          0,          0,          0,          0, 0xdbdddd1d, 0x6ddbb66d, 0xadb5e6f3, 0xcfd75a6b, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0xe50e28ef,
     0x783c5efd, 0xddddddf1,     0xc0dd,          0,          0,          0,          0,          0, 0xbbbb0300, 0xdbb66dbb, 0xd67cbe6d, 0x5a6badb5, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,     0x7c3e,
     0xc73b0000, 0xbb3b1e8f,   0xb8bbbb,          0,          0,          0,          0,          0, 0x77000000, 0xb66d7777, 0xcfb76ddb, 0xadb5d69a, 0x7c3e5f6b, 0xcfe7f3f9, 0xf97c3e9f, 0x80cfe7f3,
      0x7000000, 0xc7e3f178, 0x77777777,          0,          0,          0,          0,          0,          0, 0xedeeee0e, 0xb66ddbb6, 0xd65af3f9, 0xe76badb5, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,
           0xf0,  0x320ef00, 0xff7b82de, 0x3c1e8f67, 0x77777777,       0x70,          0,          0,          0,          0,          0, 0xeeeeee00, 0xdbb66ddb, 0xad359f6f, 0xbed65a6b, 0xe7f3f97c,
     0x7c3e9fcf, 0xcfe7f3f9,       0x9f, 0xe3f10e00, 0xeeee8ec7,     0xeeee,          0,          0,          0,          0,          0, 0xdd1d0000, 0xb66ddbdd, 0xe6f36ddb, 0x5a6badb5, 0x3e9fcfd7,
     0xe7f3f97c, 0x7c3e9fcf, 0x28eff3f9, 0x5efde50e, 0xddf1783c, 0xc0dddddd,          0,          0,          0,          0,          0,  0x3000000, 0x6dbbbbbb, 0xbe6ddbb6, 0xadb5d67c, 0xf3f95a6b,
     0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf,          0, 0x1e8fc73b, 0xbbbbbb3b,       0xb8,          0,          0,          0,          0,          0, 0x77777700, 0x6ddbb66d, 0xd69acfb7, 0x5f6badb5,
     0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,     0x80cf, 0xf1780700, 0x7777c7e3,     0x7777,          0,          0,          0,          0,          0, 0xee0e0000, 0xdbb6edee, 0xf3f9b66d, 0xadb5d65a,
     0x9fcfe76b, 0xf3f97c3e, 0x3e9fcfe7,   0xf0f97c, 0xef000000, 0x82de0320, 0x8f67ff7b, 0x77773c1e,   0x707777,          0,          0,          0,          0,          0, 0xee000000, 0x6ddbeeee,
     0x9f6fdbb6, 0x5a6bad35, 0xf97cbed6, 0x9fcfe7f3, 0xf3f97c3e,   0x9fcfe7,  0xe000000, 0x8ec7e3f1, 0xeeeeeeee,          0,          0,          0,          0,          0,          0, 0xdbdddd1d,
     0x6ddbb66d, 0xadb5e6f3, 0xcfd75a6b, 0xf97c3e9f, 0x9fcfe7f3, 0xf3f97c3e, 0xe50e28ef, 0x783c5efd, 0xddddddf1,     0xc0dd,          0,          0,          0,          0,          0, 0xbbbb0300,
     0xdbb66dbb, 0xd67cbe6d, 0x5a6badb5, 0xcfe7f3f9, 0xf97c3e9f, 0x9fcfe7f3,     0x7c3e, 0xc73b0000, 0xbb3b1e8f,   0xb8bbbb,          0,          0,          0,          0,          0, 0x77000000,
     0xb66d7777, 0xcfb76ddb, 0xadb5d69a, 0x7c3e5f6b, 0xcfe7f3f9, 0xf97c3e9f, 0x80cfe7f3,  0x7000000, 0xc7e3f178, 0x77777777,          0,          0,          0,          0,          0,          0,
     0xedeeee0e, 0xb66ddbb6, 0xd65af3f9, 0xe76badb5, 0x7c3e9fcf, 0xcfe7f3f9, 0xf97c3e9f,       0xf0,  0x320ef00, 0xff7b82de, 0x3c1e8f67, 0x77777777,       0x70,          0,          0,          0,
              0,          0, 0xeeeeee00, 0xdbb66ddb, 0xad359f6f, 0xbed65a6b, 0xe7f3f97c, 0x7c3e9fcf, 0xcfe7f3f9,       0x9f, 0xe3f10e00, 0xeeee8ec7,     0xeeee,          0,          0,          0,
              0,          0, 0xdd1d0000, 0xb66ddbdd, 0xe6f36ddb, 0x5a6badb5, 0x3e9fcfd7, 0xe7f3f97c, 0x7c3e9fcf, 0x28eff3f9, 0x5efde50e, 0xddf1783c, 0xc0dddddd,          0,          0,          0,
              0,          0,  0x3000000, 0x6dbbbbbb, 0xbe6ddbb6, 0xadb5d67c, 0xf3f95a6b, 0x3e9fcfe7, 0xe7f3f97c, 0x7c3e9fcf,          0, 0x1e8fc73b, 0xbbbbbb3b,       0xb8,          0,          0,
              0,          0,          0, 0x77777700, 0x6ddbb66d, 0xd69acfb7, 0x5f6badb5, 0xf3f97c3e, 0x3e9fcfe7, 0xe7f3f97c,     0x80cf, 0xf1780700, 0x7777c7e3,     0x7777,          0,          0,
              0,          0,          0, 0xee0e0000, 0xdbb6edee, 0xf3f9b66d, 0xadb5d65a, 0x9fcfe76b, 0xf3f97c3e, 0x3e9fcfe7,   0xf0f97c, 0xef000000,          0,          0,          0,          0,
              0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
              0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,
              0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,          0,  0x1000000, 0xa2101051,

};

static int nbytes_of_dolby_ms12_downmix_output_pcm_frame();

static int get_ms12_dump_enable(int dump_type) {

    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int value = 0;

    ret = property_get(MS12_DUMP_PROPERTY, buf, NULL);
    if (ret > 0) {
        value = strtol (buf, NULL, 0);
    }
    return (value & dump_type);
}


static void ms12_spdif_encoder(void * in_buf, int in_size, audio_format_t output_format, void *out_buf, int * out_size) {
    uint16_t iec61937_pa = 0xf872;
    uint16_t iec61937_pb = 0x4e1f;
    uint16_t iec61937_pc = 0;
    uint16_t iec61937_pd = 0;
    uint16_t preamble[4] = { 0 };
    int offset  = 0;
    uint8_t * in_data = (uint8_t *)in_buf;

    if (in_size <= 0) {
        *out_size = 0;
        return;
    }

    if (output_format == AUDIO_FORMAT_AC3) {
        iec61937_pc = 0x1;
        *out_size   = MS12_DD_FRAME_SIZE;
        iec61937_pd = in_size << 3;

    } else if (output_format == AUDIO_FORMAT_E_AC3) {
        iec61937_pc = 0x15;
        *out_size   = MS12_DDP_FRAME_SIZE;
        iec61937_pd = in_size;
    } else {
        *out_size = 0;
        return;
    }

    preamble[0] = iec61937_pa;
    preamble[1] = iec61937_pb;
    preamble[2] = iec61937_pc;
    preamble[3] = iec61937_pd;

    memset(out_buf, 0, *out_size);
    memcpy(out_buf, (void *)preamble, sizeof(preamble));
    offset += sizeof(preamble);
    memcpy((char *)out_buf + offset, in_buf, in_size);

    if (in_data[0] == 0x0b && in_data[1] == 0x77) {
        endian16_convert((char *)out_buf + offset, in_size);
        /*if we want lillte endian, we can use below code*/
        //endian16_convert(preamble, sizeof(preamble));
    }


    return;
}


/*
 *@brief dump ms12 output data
 */
static void dump_ms12_output_data(void *buffer, int size, char *file_name)
{
    FILE *fp1 = fopen(file_name, "a+");
    if (fp1) {
        int flen = fwrite((char *)buffer, 1, size, fp1);
        ALOGV("%s buffer %p size %d\n", __FUNCTION__, buffer, size);
        fclose(fp1);
    }
}


static void *dolby_ms12_threadloop(void *data);

int dap_pcm_output_l(void *buffer, void *priv_data, size_t size);
int stereo_pcm_output_l(void *buffer, void *priv_data, size_t size);
int bitstream_output_l(void *buffer, void *priv_data, size_t size);
int spdif_bitstream_output_l(void *buffer, void *priv_data, size_t size);


int dolby_ms12_register_callback(struct aml_stream_out *aml_out)
{
    ALOGI("\n+%s()", __FUNCTION__);
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ret = 0;
    if (ms12->output_config & MS12_OUTPUT_MASK_SPEAKER) {
        ret = dolby_ms12_register_dap_pcm_callback(dap_pcm_output_l, (void *)aml_out);
        ALOGI("%s() dolby_ms12_register_pcm_callback return %d", __FUNCTION__, ret);
    }
    if (ms12->output_config & MS12_OUTPUT_MASK_STEREO) {
        ret = dolby_ms12_register_pcm_callback(stereo_pcm_output_l, (void *)aml_out);
        ALOGI("%s() dolby_ms12_register_pcm_callback return %d", __FUNCTION__, ret);
    }
    if (ms12->output_config & MS12_OUTPUT_MASK_DDP) {
        ret = dolby_ms12_register_bitstream_callback(bitstream_output_l, (void *)aml_out);
        ALOGI("%s() dolby_ms12_register_bitstream_callback return %d", __FUNCTION__, ret);

    }
    if (ms12->output_config & MS12_OUTPUT_MASK_DD) {
        ret = dolby_ms12_register_spdif_bitstream_callback(spdif_bitstream_output_l, (void *)aml_out);
        ALOGI("%s() dolby_ms12_register_spdif_bitstream_callback return %d", __FUNCTION__, ret);
    }
    ALOGI("-%s() ret %d\n\n", __FUNCTION__, ret);

    return ret;

}

bool is_platform_supported_ddp_atmos(bool atmos_supported, enum OUT_PORT current_out_port, bool is_tv)
{
    bool ret = false;
    //ALOGD("%s atmos_supported %d current_out_port %d", __func__, atmos_supported, current_out_port);
    if ((current_out_port == OUTPORT_HDMI_ARC) || (current_out_port == OUTPORT_HDMI)) {
        /*ARC case*/
        ret = atmos_supported;
    }
    else {
        if (is_tv) {
            /*SPEAKER/HEADPHONE case*/
            ret = true;
        }
        else {
            /* OTT CVBS case */
            ret = false;
        }
    }
    //ALOGD("%s Line %d return %s", __func__, __LINE__, ret ? "true": "false");
    return ret;
}

bool is_ms12_out_ddp_5_1_suitable(bool is_ddp_atmos __unused)
{
    return true;
}

/*
 *@brief get ms12 output configure mask
 */
static int get_ms12_output_mask(audio_format_t sink_format,audio_format_t  optical_format,bool is_arc)
{
    int  output_config;
    if (sink_format == AUDIO_FORMAT_E_AC3)
        output_config =  MS12_OUTPUT_MASK_DDP;
    else if (sink_format == AUDIO_FORMAT_AC3)
        output_config = MS12_OUTPUT_MASK_DD;
    else if (sink_format == AUDIO_FORMAT_MAT)
        output_config = MS12_OUTPUT_MASK_MAT;
    else if (sink_format == AUDIO_FORMAT_PCM_16_BIT && optical_format == AUDIO_FORMAT_AC3)
        output_config = MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_SPEAKER;
    else if (is_arc)
        output_config = MS12_OUTPUT_MASK_STEREO;
    else
        output_config = MS12_OUTPUT_MASK_SPEAKER | MS12_OUTPUT_MASK_STEREO;
    return output_config;
}

static audio_format_t ms12_get_audio_hal_format(audio_format_t hal_format)
{
    if (hal_format == AUDIO_FORMAT_E_AC3_JOC) {
        return AUDIO_FORMAT_E_AC3;
    } else if (hal_format == AUDIO_FORMAT_MP2 ||
               hal_format == AUDIO_FORMAT_MP3 ||
               hal_format == AUDIO_FORMAT_AAC ||
               hal_format == AUDIO_FORMAT_AAC_LATM ||
               hal_format == AUDIO_FORMAT_DRA) {
        return AUDIO_FORMAT_PCM_16_BIT;
    } else {
        return hal_format;
    }
}

void set_ms12_ad_mixing_enable(struct dolby_ms12_desc *ms12, int ad_mixing_enable)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xa", ad_mixing_enable);
    if (strlen(parm) > 0)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_ad_mixing_level(struct dolby_ms12_desc *ms12, int mixing_level)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xu", mixing_level);
    if (strlen(parm) > 0)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_ad_vol(struct dolby_ms12_desc *ms12, int ad_vol)
{
    char parm[32] = "";
    /*ad_vol is 0~100*/
    float gain = (float)ad_vol / 100;
    int gain_db = (int)(128 * AmplToDb(gain));
    /*target gain at end of ramp in 1/128 dB (range: -12288..0)*/
    gain_db = gain_db > 0 ? 0 : gain_db;
    gain_db = gain_db < -12288 ? -12288 : gain_db;
    sprintf(parm, "%s %d,%d,%d", "-main2_mixgain", gain_db , 10, 0);
    ALOGI("%s %s", __func__, parm);
    if (strlen(parm) > 0)
        aml_ms12_update_runtime_params(ms12, parm);
}


void set_dolby_ms12_runtime_system_mixing_enable(struct dolby_ms12_desc *ms12, int system_mixing_enable)
{
    char parm[12] = "";
    sprintf(parm, "%s %d", "-xs", system_mixing_enable);
    if ((strlen(parm) > 0) && ms12) {
        aml_ms12_update_runtime_params(ms12, parm);
    }
}
void set_ms12_main1_audio_mute(struct dolby_ms12_desc *ms12, bool b_mute)
{
    char parm[64] = "";
    /*
    - target gain at end of ramp in dB (range: -96 * 128..0), the step is 1/128 db, so the range is (-96,0)
    - duration of ramp in milliseconds (range: 0..60000)
    - shape of the ramp (0: linear, 1: in cube, 2: out cube)
    */
    if (b_mute) {
        sprintf(parm, "%s %d,%d,%d", "-main1_mixgain", -96 * 128, 0, 0);
    } else {
        sprintf(parm, "%s %d,%d,%d", "-main1_mixgain", 0, 0, 0);
    }
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
    ms12->is_muted = b_mute;
}

void set_ms12_atmos_lock(struct dolby_ms12_desc *ms12, bool is_atmos_lock_on)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-atmos_locking", is_atmos_lock_on);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_acmod2ch_lock(struct dolby_ms12_desc *ms12, bool is_lock_on)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-acmod2ch_lock", is_lock_on);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_main_volume(struct dolby_ms12_desc *ms12, float volume) {
    ms12->main_volume = volume;
    dolby_ms12_set_main_volume(volume);
}

void set_ms12_ac4_presentation_group_index(struct dolby_ms12_desc *ms12, int index)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-ac4_pres_group_idx", index);
    (void *)ms12;

}

static inline alsa_device_t usecase_device_adapter_with_ms12(alsa_device_t usecase_device, audio_format_t output_format)
{
    ALOGI("%s usecase_device %d output_format %#x", __func__, usecase_device, output_format);
    switch (usecase_device) {
    case DIGITAL_DEVICE:
    case I2S_DEVICE:
        if ((output_format == AUDIO_FORMAT_AC3) || (output_format == AUDIO_FORMAT_E_AC3)
            || (output_format == AUDIO_FORMAT_MAT) ) {
            return DIGITAL_DEVICE;
        } else {
            return I2S_DEVICE;
        }
    default:
        return I2S_DEVICE;
    }
}



void set_ms12_drc_boost_value_for_2ch_downmixed_output(struct dolby_ms12_desc *ms12, int boost)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-bs", boost);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_drc_cut_value_for_2ch_downmixed_output(struct dolby_ms12_desc *ms12, int cut)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-cs", cut);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}
void set_ms12_drc_mode_for_2ch_downmixed_output(struct dolby_ms12_desc *ms12, bool drc_mode)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-drc", drc_mode);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_drc_boost_value(struct dolby_ms12_desc *ms12, int boost)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-b", boost);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_drc_cut_value(struct dolby_ms12_desc *ms12, int cut)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-c", cut);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_dap_postgain(struct dolby_ms12_desc *ms12 __unused, int postgain __unused)
{
    return;
}

void set_ms12_drc_mode_for_multichannel_and_dap_output(struct dolby_ms12_desc *ms12, bool drc_mode)
{
    char parm[64] = "";
    sprintf(parm, "%s %d", "-dap_drc", drc_mode);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_fade_pan
    (struct dolby_ms12_desc *ms12
    , int fade_byte
    , int gain_byte_center
    , int gain_byte_front
    , int gain_byte_surround
    , int pan_byte
    )
{
    char parm[64] = "";

    sprintf(parm, "%s %d,%d,%d,%d,%d", "-ad_fade_pan",
        fade_byte, gain_byte_center, gain_byte_front, gain_byte_surround, pan_byte);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}


void set_ms12_main_audio_pts(struct dolby_ms12_desc *ms12, uint64_t apts, unsigned int bytes_offset)
{
    char parm[64] = "";
    uint32_t apts_high32b = (uint32_t)(apts>>32);
    uint32_t apts_low32b = (uint32_t)(apts&UINT_MAX);
    sprintf(parm, "%s %u,%u,%u", "-main_audio_pts", apts_high32b, apts_low32b, bytes_offset);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}

void set_ms12_main1_audio_pts(struct dolby_ms12_desc *ms12, uint64_t apts, unsigned int bytes_offset)
{
    char parm[64] = "";
    uint32_t apts_high32b = (uint32_t)(apts>>32);
    uint32_t apts_low32b = (uint32_t)(apts&UINT_MAX);
    sprintf(parm, "%s %u,%u,%u", "-main1_audio_pts", apts_high32b,apts_low32b, bytes_offset);
    if ((strlen(parm)) > 0 && ms12)
        aml_ms12_update_runtime_params(ms12, parm);
}


void dynamic_set_dolby_ms12_drc_parameters(struct dolby_ms12_desc *ms12)
{
    int drc_mode = 0;
    int drc_cut = 0;
    int drc_boost = 0;
    int dolby_ms12_drc_mode = DOLBY_DRC_RF_MODE;
    int dolby_ms12_dap_drc_mode = DOLBY_DRC_RF_MODE;

    if (!ms12) {
        ALOGE("%s() input ms12 is NULL!\n", __FUNCTION__);
        return ;
    }

    if (0 == aml_audio_get_dolby_drc_mode(&drc_mode, &drc_cut, &drc_boost))
        dolby_ms12_drc_mode = (drc_mode == DDPI_UDC_COMP_LINE) ? DOLBY_DRC_LINE_MODE : DOLBY_DRC_RF_MODE;

    /*
     * if main input is hdmi-in/dtv/other-source PCM
     * would not go through the DRC processing
     * DRC LineMode means to bypass DRC processing.
     */
    if (audio_is_linear_pcm(ms12->main_input_fmt)) {
        dolby_ms12_drc_mode = DOLBY_DRC_LINE_MODE;
    }

    set_ms12_drc_boost_value_for_2ch_downmixed_output(ms12, drc_boost);
    set_ms12_drc_cut_value_for_2ch_downmixed_output(ms12, drc_cut);
    set_ms12_drc_mode_for_2ch_downmixed_output(ms12, dolby_ms12_drc_mode);
    ALOGI("%s dynamic set drc %s boost %d cut %d", __FUNCTION__,
        (dolby_ms12_drc_mode == DOLBY_DRC_RF_MODE) ? "RF MODE" : "LINE MODE", drc_boost, drc_cut);

    if (ms12->output_config & MS12_OUTPUT_MASK_DAP) {
        if (0 == aml_audio_get_dolby_dap_drc_mode(&drc_mode, &drc_cut, &drc_boost))
            dolby_ms12_dap_drc_mode = (drc_mode == DDPI_UDC_COMP_LINE) ? DOLBY_DRC_LINE_MODE : DOLBY_DRC_RF_MODE;
        set_ms12_drc_boost_value(ms12, drc_boost);
        set_ms12_drc_cut_value(ms12, drc_cut);
        set_ms12_drc_mode_for_multichannel_and_dap_output(ms12, dolby_ms12_dap_drc_mode);
        ALOGI("%s dynamic set dap_drc %s",
            __FUNCTION__, (dolby_ms12_dap_drc_mode == DOLBY_DRC_RF_MODE) ? "RF MODE" : "LINE MODE");
    }

}

void set_dolby_ms12_drc_parameters(audio_format_t input_format, int output_config_mask)
{
    int dolby_ms12_drc_mode = DOLBY_DRC_RF_MODE;
    int dolby_ms12_dap_drc_mode = DOLBY_DRC_RF_MODE;
    int drc_mode = 0;
    int drc_cut = 0;
    int drc_boost = 0;
/* /
    if (0 == aml_audio_get_dolby_drc_mode(&drc_mode, &drc_cut, &drc_boost))
        dolby_ms12_drc_mode = (drc_mode == DDPI_UDC_COMP_LINE) ? DOLBY_DRC_LINE_MODE : DOLBY_DRC_RF_MODE;
   */
    dolby_ms12_drc_mode = DOLBY_DRC_LINE_MODE;

    //for mul-pcm
    dolby_ms12_set_drc_boost(drc_boost);
    dolby_ms12_set_drc_cut(drc_cut);
    //for 2-channel downmix
    dolby_ms12_set_drc_boost_system(drc_boost);
    dolby_ms12_set_drc_cut_system(drc_cut);

    /*
     * if main input is hdmi-in/dtv/other-source PCM
     * would not go through the DRC processing
     * DRC LineMode means to bypass DRC processing.
     */
    if (audio_is_linear_pcm(input_format)) {
        dolby_ms12_drc_mode = DOLBY_DRC_LINE_MODE;
    }

    dolby_ms12_set_drc_mode(dolby_ms12_drc_mode);
    ALOGI("%s dolby_ms12_set_drc_mode %s", __FUNCTION__, (dolby_ms12_drc_mode == DOLBY_DRC_RF_MODE) ? "RF MODE" : "LINE MODE");

    if (output_config_mask & MS12_OUTPUT_MASK_DAP) {
        if (0 == aml_audio_get_dolby_dap_drc_mode(&drc_mode, &drc_cut, &drc_boost))
            dolby_ms12_dap_drc_mode = (drc_mode == DDPI_UDC_COMP_LINE) ? DOLBY_DRC_LINE_MODE : DOLBY_DRC_RF_MODE;
        dolby_ms12_set_dap_drc_mode(dolby_ms12_dap_drc_mode);
        ALOGI("%s dolby_ms12_set_dap_drc_mode %s",
            __FUNCTION__, (dolby_ms12_dap_drc_mode == DOLBY_DRC_RF_MODE) ? "RF MODE" : "LINE MODE");
    }
}

static void set_dolby_ms12_dap_init_mode(struct aml_audio_device *adev)
{
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int dap_init_mode = 0;

    /* Dolby MS12 V2 uses DAP Tuning file */
    if (adev->is_ms12_tuning_dat) {
        dap_init_mode = get_ms12_dap_init_mode(adev->is_TV);
    }
    else {
        dap_init_mode = 0;
    }
    dolby_ms12_set_dap2_initialisation_mode(dap_init_mode);
}

static void set_dolby_ms12_downmix_mode(struct aml_audio_device *adev)
{
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int downmix_mode = DOWNMIX_MODE_LtRt; // Lt/Rt is default mode
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int value = 0;

    ret = property_get(MS12_DOWNMIX_MODE_PROPERTY, buf, NULL);
    if (ret > 0) {
        if (strcasecmp(buf, "Lt/Rt") == 0) {
            downmix_mode = DOWNMIX_MODE_LtRt;
        }
        else if (strcasecmp(buf, "Lo/Ro") == 0) {
            downmix_mode = DOWNMIX_MODE_LoRo;
        }
        else if (strcasecmp(buf, "ARIB") == 0) {
            /* for HE-AAC, currently, it is not used at all. */
            downmix_mode = DOWNMIX_MODE_ARIB;
        }
    }

    dolby_ms12_set_downmix_modes(downmix_mode);
}
/*
 *@brief get dolby ms12 prepared
 */
int get_the_dolby_ms12_prepared(
    struct aml_stream_out *aml_out
    , audio_format_t input_format
    , audio_channel_mask_t input_channel_mask
    , int input_sample_rate)
{
    ALOGI("+%s()", __FUNCTION__);
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int dolby_ms12_drc_mode = DOLBY_DRC_RF_MODE;
    int system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_OFF;
    struct aml_stream_out *out;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;
    int  output_config;
    if (patch) {
        demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }
    int ret = 0, associate_audio_mixing_enable = 0;

    ALOGI("\n+%s()", __FUNCTION__);
    pthread_mutex_lock(&ms12->lock);
    ALOGI("++%s(), locked", __FUNCTION__);
    ms12->optical_format = adev->optical_format;
    ms12->sink_format    = adev->sink_format;
    set_audio_system_format(AUDIO_FORMAT_PCM_16_BIT);
    input_format = ms12_get_audio_hal_format(input_format);
    /*
    when HDMITX send pause frame,we treated as INVALID format.
    for MS12,we treat it as LPCM and mute the frame
    */
    if (input_format == AUDIO_FORMAT_INVALID) {
        input_format = AUDIO_FORMAT_PCM_16_BIT;
    }
    set_audio_app_format(AUDIO_FORMAT_PCM_16_BIT);
    set_audio_main_format(input_format);

    if (input_format == AUDIO_FORMAT_AC3 || input_format == AUDIO_FORMAT_E_AC3) {
        if (patch && demux_info) {
            ms12->dual_decoder_support = demux_info->dual_decoder_support;
            associate_audio_mixing_enable = adev->associate_audio_mixing_enable;
       } else {
            ms12->dual_decoder_support = 0;
            associate_audio_mixing_enable = 0;
       }
    } else {
        ms12->dual_decoder_support = 0;
        associate_audio_mixing_enable = 0;
    }
    ALOGI("+%s() dual_decoder_support %d optical =0x%x sink =0x%x\n",
        __FUNCTION__, ms12->dual_decoder_support, ms12->optical_format, ms12->sink_format);

    /*set the associate audio format*/
    if (ms12->dual_decoder_support == true) {
        set_audio_associate_format(input_format);
        ALOGI("%s set_audio_associate_format %#x", __FUNCTION__, input_format);
    }
    dolby_ms12_set_asscociated_audio_mixing(associate_audio_mixing_enable);
    dolby_ms12_set_user_control_value_for_mixing_main_and_associated_audio(adev->mixing_level);


    /*set the continous output flag*/
    set_dolby_ms12_continuous_mode((bool)adev->continuous_audio_mode);
    dolby_ms12_set_atmos_lock_flag(adev->atoms_lock_flag);
    /* create  the ms12 output stream here */
    /*************************************/
    if (continous_mode(adev)) {
        // TODO: zz: Might have memory leak, not clear route to release this pointer
        out = (struct aml_stream_out *)aml_audio_calloc(1, sizeof(struct aml_stream_out));
        if (!out) {
            ALOGE("%s malloc stream failed", __func__);
            goto Err;
        }
        /* copy stream information */
        memcpy(out, aml_out, sizeof(struct aml_stream_out));
        ALOGI("%s format %#x in ott\n", __func__, out->hal_format);
        /*
        *for ms12's callback bitstream_output()
        *the audio format should not be same as AUDIO_FORMAT_IEC61937.
        *this will leads some sink device sound abnormal.
        *this format is used in dolby_ms12_threadloop()
        *to determine use the spdifencoder or not.
        */
        if (out->hal_format == AUDIO_FORMAT_IEC61937) {
            out->hal_format = aml_out->hal_internal_format;
            ALOGI("%s format convert to %#x in ott\n", __func__, out->hal_format);
        }
        if (adev->is_TV) {
            out->is_tv_platform  = 1;
            out->config.channels = 8;
            out->config.format = PCM_FORMAT_S32_LE;
            out->tmp_buffer_8ch = aml_audio_malloc(out->config.period_size * 4 * 8);
            if (out->tmp_buffer_8ch == NULL) {
                ALOGE("%s cannot malloc memory for out->tmp_buffer_8ch", __func__);
                goto Err_tmp_buf_8ch;

            }
            out->tmp_buffer_8ch_size = out->config.period_size * 4 * 8;
            out->audioeffect_tmp_buffer = aml_audio_malloc(out->config.period_size * 6);
            if (out->audioeffect_tmp_buffer == NULL) {
                ALOGE("%s cannot malloc memory for audioeffect_tmp_buffer", __func__);
                goto Err_audioeffect_tmp_buf;
            }
        }
        ALOGI("%s create ms12 stream %p,original stream %p", __func__, out, aml_out);
    } else {
        out = aml_out;
    }
    adev->ms12_out = out;
    ALOGI("%s adev->ms12_out =  %p sys mixing =%d", __func__, adev->ms12_out, adev->system_app_mixing_status);
    /************end**************/
    /*set the system app sound mixing enable*/
    if (adev->continuous_audio_mode) {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    } else {
        system_app_mixing_status = adev->system_app_mixing_status;
    }
    dolby_ms12_set_system_app_audio_mixing(system_app_mixing_status);

    /* set DAP init mode */
    dolby_ms12_set_dap2_initialisation_mode(get_ms12_dap_init_mode(adev->is_TV));

    //init the dolby ms12
    ms12->dual_bitstream_support = adev->dual_spdif_support;
    if (adev->aml_dap_v1_enable) {
        output_config = MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_DDP | MS12_OUTPUT_MASK_STEREO | MS12_OUTPUT_MASK_SPEAKER;
    } else {
        output_config = MS12_OUTPUT_MASK_DD | MS12_OUTPUT_MASK_DDP | MS12_OUTPUT_MASK_STEREO;
    }

    set_dolby_ms12_drc_parameters(input_format, output_config);
    aml_ms12_config(ms12, input_format, input_channel_mask, input_sample_rate, output_config, get_ms12_path());

    if (ms12->dolby_ms12_enable) {
        //register Dolby MS12 callback
        dolby_ms12_register_callback(out);
        ms12->device = usecase_device_adapter_with_ms12(out->device, AUDIO_FORMAT_PCM_16_BIT);
        ALOGI("%s out [dual_output_flag %d] adev [format sink %#x optical %#x] ms12 [output-format %#x device %d]",
              __FUNCTION__, out->dual_output_flag, adev->sink_format, adev->optical_format, ms12->output_config, ms12->device);
        memcpy((void *) & (adev->ms12_config), (const void *) & (out->config), sizeof(struct pcm_config));
        get_hardware_config_parameters(
            &(adev->ms12_config)
            , AUDIO_FORMAT_PCM_16_BIT
            , adev->default_alsa_ch
            , ms12->output_samplerate
            , out->is_tv_platform
            , continous_mode(adev)
            , false);

        if (continous_mode(adev)) {
            ms12->dolby_ms12_thread_exit = false;
            ret = pthread_create(&(ms12->dolby_ms12_threadID), NULL, &dolby_ms12_threadloop, out);
            if (ret != 0) {
                ALOGE("%s, Create dolby_ms12_thread fail!\n", __FUNCTION__);
                goto Err_dolby_ms12_thread;
            }
            ALOGI("%s() thread is builded, get dolby_ms12_threadID %ld\n", __FUNCTION__, ms12->dolby_ms12_threadID);
        }
        //n bytes of dowmix output pcm frame, 16bits_per_sample / stereo, it value is 4btes.
        ms12->nbytes_of_dmx_output_pcm_frame = nbytes_of_dolby_ms12_downmix_output_pcm_frame();
        ms12->hdmi_format = adev->hdmi_format;
        //ms12->optical_format = adev->optical_format;
        ms12->main_input_fmt = input_format;
        /*IEC61937 DDP format, the real samplerate need device by 4*/
        if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
            if ((aml_out->hal_internal_format & AUDIO_FORMAT_E_AC3) == AUDIO_FORMAT_E_AC3) {
                input_sample_rate /= 4;
            }
        }
        ms12->main_input_sr = input_sample_rate;
    }
    ms12->sys_audio_base_pos = adev->sys_audio_frame_written;
    ms12->sys_audio_skip     = 0;
    ms12->dap_pcm_frames     = 0;
    ms12->stereo_pcm_frames  = 0;
    ms12->master_pcm_frames  = 0;
    ms12->ms12_main_input_size = 0;
    ms12->main_volume        = 1.0f;
    ALOGI("set ms12 sys pos =%" PRId64 "", ms12->sys_audio_base_pos);

    ms12->iec61937_ddp_buf = aml_audio_calloc(1, MS12_DDP_FRAME_SIZE);
    if (ms12->iec61937_ddp_buf == NULL) {
        goto Err;
    }

    aml_ac3_parser_open(&ms12->ac3_parser_handle);
    aml_spdif_decoder_open(&ms12->spdif_dec_handle);
    aml_ms12_bypass_open(&ms12->ms12_bypass_handle);
    ring_buffer_init(&ms12->spdif_ring_buffer, ms12->dolby_ms12_out_max_size);
    ms12->lpcm_temp_buffer = (unsigned char*)malloc(ms12->dolby_ms12_out_max_size);
    if (!ms12->lpcm_temp_buffer) {
        ALOGE("%s malloc lpcm_temp_buffer failed", __func__);
        if (continous_mode(adev))
            goto Err_dolby_ms12_thread;
        else
            goto Err;
    }
    ms12->dolby_ms12_init_flags = true;
    adev->doing_reinit_ms12     = false;
    ALOGI("--%s(), locked", __FUNCTION__);
    pthread_mutex_unlock(&ms12->lock);
    ALOGI("-%s()\n\n", __FUNCTION__);
    return ret;

Err_dolby_ms12_thread:
    if (continous_mode(adev)) {
        if (ms12->dolby_ms12_enable) {
            ALOGE("%s() %d exit dolby_ms12_thread\n", __FUNCTION__, __LINE__);
            ms12->dolby_ms12_thread_exit = true;
            ms12->dolby_ms12_threadID = 0;
        }
        free(out->audioeffect_tmp_buffer);
    }

Err_audioeffect_tmp_buf:
    free(out->tmp_buffer_8ch);
Err_tmp_buf_8ch:
    free(out);
Err:
    if (ms12->iec61937_ddp_buf) {
        aml_audio_free(ms12->iec61937_ddp_buf);
        ms12->iec61937_ddp_buf = NULL;
    }
    pthread_mutex_unlock(&ms12->lock);
    return ret;
}

static bool is_iec61937_format(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    return (aml_out->hal_format == AUDIO_FORMAT_IEC61937);
}

bool is_ms12_passthrough(struct audio_stream_out *stream) {
    bool bypass_ms12 = false;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    if ((adev->hdmi_format == BYPASS)
        /* when arc output, the optical_format == sink format
         * when speaker output, the optical format != format
         * only the optical_format == hal_internal_format, we can do passthrough,
         * otherwise we need do some convert
         */
        && (ms12->optical_format >= aml_out->hal_internal_format)) {
        if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3 ||
            aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
            /*current we only support 48k ddp/dd bypass*/
            if (aml_out->hal_rate == 48000 || aml_out->hal_rate == 192000 ||
                aml_out->hal_rate == 44100 || aml_out->hal_rate == 176400) {
                bypass_ms12 = true;
            } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3 &&
                (aml_out->hal_rate == 32000)) {
                bypass_ms12 = true;
            }
        }
    }
    ALOGV("bypass_ms12 =%d hdmi format =%d optical format =0x%x 0x%x",
        bypass_ms12, adev->hdmi_format, ms12->optical_format, aml_out->hal_internal_format);
    return bypass_ms12;
}

/*
 *@brief dolby ms12 main process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */

int dolby_ms12_main_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ms12_output_size = 0;
    int dolby_ms12_input_bytes = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;

    void *input_buffer = (void *)buffer;
    size_t input_bytes = bytes;
    int dual_decoder_used_bytes = 0;
    int single_decoder_used_bytes = 0;
    void *main_frame_buffer = input_buffer;/*input_buffer as default*/
    int main_frame_size = input_bytes;/*input_bytes as default*/
    void *associate_frame_buffer = NULL;
    int associate_frame_size = 0;
    int32_t parser_used_size = 0;
    int32_t spdif_dec_used_size = 0;
    int dependent_frame = 0;
    int sample_rate = 48000;
    int ret = 0;
    struct ac4_parser_info ac4_info = { 0 };
    audio_format_t ms12_hal_format = ms12_get_audio_hal_format(aml_out->hal_format);

    if (adev->debug_flag >= 2) {
        ALOGI("\n%s() in continuous %d input ms12 bytes %d input bytes %zu\n",
              __FUNCTION__, adev->continuous_audio_mode, dolby_ms12_input_bytes, input_bytes);
    }

    pthread_mutex_lock(&ms12->main_lock);

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT) && audio_is_linear_pcm(aml_out->hal_internal_format)) {
        check_audio_level("ms12_main", buffer, bytes);
    }

    if (ms12->dolby_ms12_enable) {
        //ms12 input main
        int dual_input_ret = 0;

        if (!aml_out->is_ms12_main_decoder) {
            dolby_ms12_main_open(stream);

        }

        /*this status is only updated in hw_write, continuous mode also need it*/
        if (adev->continuous_audio_mode) {
            if (aml_out->status != STREAM_HW_WRITING) {
                aml_out->status = STREAM_HW_WRITING;
            }
        }

        if (adev->continuous_audio_mode == 1) {
            uint64_t buf_max_threshold = MS12_MAIN_INPUT_BUF_NS_UPTHRESHOLD;
            /*why we add 1ms
             *because the main_input_ns is not acurate enough, it lost the decimal part
             *so we add 1ms to compensate
             */
            uint64_t main_buffer_duration_ns = (ms12->main_input_ns + NANO_SECOND_PER_MILLISECOND - ms12->main_output_ns);
            if ((main_buffer_duration_ns != 0) && main_buffer_duration_ns / NANO_SECOND_PER_MILLISECOND < ms12->main_buffer_min_level) {
                ms12->main_buffer_min_level = main_buffer_duration_ns / NANO_SECOND_PER_MILLISECOND;
            }
            if (main_buffer_duration_ns /NANO_SECOND_PER_MILLISECOND > ms12->main_buffer_max_level) {
                ms12->main_buffer_max_level = main_buffer_duration_ns / NANO_SECOND_PER_MILLISECOND;
            }
            ALOGV("main input_ns =%" PRId64 " output_ns=%" PRId64 " diff_us=%" PRId64 " min =%d max=%d",
                ms12->main_input_ns, ms12->main_output_ns, main_buffer_duration_ns/1000,
                ms12->main_buffer_min_level, ms12->main_buffer_max_level);

            if (main_buffer_duration_ns >= buf_max_threshold) {
                //ALOGI("main_buffer_duration_ns =%lld", main_buffer_duration_ns);
                //audio_virtual_buf_reset(ms12->main_virtual_buf_handle);
                //audio_virtual_buf_process(ms12->main_virtual_buf_handle, MS12_MAIN_INPUT_BUF_NS_UPTHRESHOLD);
            }
        }

        if (ms12->dual_decoder_support == true) {
            dual_input_ret = scan_dolby_main_associate_frame(input_buffer
                             , input_bytes
                             , &dual_decoder_used_bytes
                             , &main_frame_buffer
                             , &main_frame_size
                             , &associate_frame_buffer
                             , &associate_frame_size);
            if (dual_input_ret) {
                ALOGE("%s used size %zu dont find the iec61937 format header, rescan next time!\n", __FUNCTION__, *use_size);
                goto  exit;
            }
        }
        /*
        As the audio payload may cross two write process,we can not skip the
        data when we do not get a complete payload.for ATSC,as we have a
        complete burst align for 6144/24576,so we always can find a valid
        payload in one write process.
        */
        else if (is_iec61937_format(stream)) {
            struct ac3_parser_info ac3_info = { 0 };
            void * dolby_inbuf = NULL;
            int32_t dolby_buf_size = 0;
            int temp_used_size = 0;
            void * temp_main_frame_buffer = NULL;
            int temp_main_frame_size = 0;
            audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
            aml_spdif_decoder_process(ms12->spdif_dec_handle, input_buffer , input_bytes, &spdif_dec_used_size, &main_frame_buffer, &main_frame_size);
            if (main_frame_size && main_frame_buffer) {
                uint16_t * data = (uint16_t *)main_frame_buffer;
                /*if is 0x77 0x0b, we need convert it*/
                if (data[0] == DDP_SYNC_WORD) {
                    endian16_convert(main_frame_buffer, main_frame_size);
                }
            }

            if (main_frame_size == 0) {
                *use_size = spdif_dec_used_size;
                goto exit;
            }
            output_format = aml_spdif_decoder_getformat(ms12->spdif_dec_handle);
            if (output_format == AUDIO_FORMAT_E_AC3
                || output_format == AUDIO_FORMAT_AC3) {
                dolby_inbuf = main_frame_buffer;
                dolby_buf_size = main_frame_size;
                aml_ac3_parser_process(ms12->ac3_parser_handle, dolby_inbuf, dolby_buf_size, &temp_used_size, &temp_main_frame_buffer, &temp_main_frame_size, &ac3_info);
                if (ac3_info.sample_rate != 0) {
                    sample_rate = ac3_info.sample_rate;
                }
                ALOGV("Input size =%d used_size =%d output size=%d rate=%d interl format=0x%x rate=%d",
                    input_bytes, spdif_dec_used_size, main_frame_size, aml_out->hal_rate, aml_out->hal_internal_format, sample_rate);

                if (main_frame_size != 0 && adev->continuous_audio_mode) {
                    struct bypass_frame_info frame_info = { 0 };
                    aml_out->ddp_frame_size    = main_frame_size;
                    frame_info.audio_format    = ms12_hal_format;
                    frame_info.samplerate      = ac3_info.sample_rate;
                    frame_info.dependency_frame = ac3_info.frame_dependent;
                    frame_info.numblks         = ac3_info.numblks;
                    aml_ms12_bypass_checkin_data(ms12->ms12_bypass_handle, main_frame_buffer, main_frame_size, &frame_info);
                }
            }

            if (ms12->input_config_format == AUDIO_FORMAT_MAT) {
                int mat_stream_profile = get_stream_profile_from_dolby_mat_frame((const char *)main_frame_buffer, main_frame_size);
                if (IS_AVAILABLE_MAT_STREAM_PROFILE(mat_stream_profile)) {
                    //dolby_ms12_set_mat_stream_profile(mat_stream_profile);
                }
            }
        }
        /*
         *continuous output with dolby atmos input, the ddp frame size is variable.
         */
        else if (adev->continuous_audio_mode == 1) {
            if ((ms12_hal_format == AUDIO_FORMAT_AC3) ||
                (ms12_hal_format == AUDIO_FORMAT_E_AC3)) {
                struct ac3_parser_info ac3_info = { 0 };
                if (adev->debug_flag) {
                    ALOGI("%s line %d ###### frame size %d #####",
                        __func__, __LINE__, aml_out->ddp_frame_size);
                }
                {
                    aml_ac3_parser_process(ms12->ac3_parser_handle, input_buffer, bytes, &parser_used_size, &main_frame_buffer, &main_frame_size, &ac3_info);
                    aml_out->ddp_frame_size = main_frame_size;
                    aml_out->ddp_frame_nblks = ac3_info.numblks;
                    aml_out->total_ddp_frame_nblks += aml_out->ddp_frame_nblks;
                    dependent_frame = ac3_info.frame_dependent;
                    sample_rate = ac3_info.sample_rate;
                    if (ac3_info.frame_size == 0) {
                        *use_size = parser_used_size;
                        if (parser_used_size == 0) {
                            *use_size = bytes;
                        }
                        goto exit;

                    }
                }
                if (main_frame_size != 0) {
                    struct bypass_frame_info frame_info = { 0 };
                    aml_out->ddp_frame_size    = main_frame_size;
                    frame_info.audio_format    = ms12_hal_format;
                    frame_info.samplerate      = ac3_info.sample_rate;
                    frame_info.dependency_frame = ac3_info.frame_dependent;
                    frame_info.numblks         = ac3_info.numblks;
                    aml_ms12_bypass_checkin_data(ms12->ms12_bypass_handle, main_frame_buffer, main_frame_size, &frame_info);
                }

           } else if (ms12_hal_format == AUDIO_FORMAT_AC4) {
                aml_ac4_parser_process(aml_out->ac4_parser_handle, input_buffer, bytes, &parser_used_size, &main_frame_buffer, &main_frame_size, &ac4_info);
                ALOGV("frame size =%d frame rate=%d sample rate=%d used =%d", ac4_info.frame_size, ac4_info.frame_rate, ac4_info.sample_rate, parser_used_size);
                if (main_frame_size == 0 && parser_used_size == 0) {
                    *use_size = bytes;
                    ALOGE("wrong ac4 frame size");
                    goto exit;
                }

            }
        }

        if (ms12->dual_decoder_support == true) {
            /*if there is associate frame, send it to dolby ms12.*/
            char tmp_array[4096] = {0};
            if (!associate_frame_buffer || (associate_frame_size == 0)) {
                associate_frame_buffer = (void *)&tmp_array[0];
                associate_frame_size = sizeof(tmp_array);
            }
            if (associate_frame_size < main_frame_size) {
                ALOGV("%s() main frame addr %p size %d associate frame addr %p size %d, need a larger ad input size!\n",
                      __FUNCTION__, main_frame_buffer, main_frame_size, associate_frame_buffer, associate_frame_size);
                memcpy(&tmp_array[0], associate_frame_buffer, associate_frame_size);
                associate_frame_size = sizeof(tmp_array);
            }
            dolby_ms12_input_associate(ms12->dolby_ms12_ptr
                                       , (const void *)associate_frame_buffer
                                       , (size_t)associate_frame_size
                                       , ms12->input_config_format
                                       , audio_channel_count_from_out_mask(ms12->config_channel_mask)
                                       , ms12->config_sample_rate
                                      );
            if (adev->debug_flag >= 2)
                ALOGI("%s line %d associate_frame_size %d",
                    __func__, __LINE__, associate_frame_size);
            if (get_ms12_dump_enable(DUMP_MS12_INPUT_ASSOCIATE)) {
                dump_ms12_output_data((void*)associate_frame_buffer, associate_frame_size, MS12_INPUT_SYS_ASSOCIATE_FILE);
            }

        }

MAIN_INPUT:
        if (main_frame_buffer && (main_frame_size > 0)) {
            /*input main frame*/
            int main_format = ms12->input_config_format;
            int main_channel_num = audio_channel_count_from_out_mask(ms12->config_channel_mask);
            int main_sample_rate = ms12->config_sample_rate;
            if ((dolby_ms12_get_dolby_main1_file_is_dummy() == true) && \
                (dolby_ms12_get_ott_sound_input_enable() == true) && \
                (adev->continuous_audio_mode == 1)) {
                //hwsync pcm, 16bits-stereo
                main_format = AUDIO_FORMAT_PCM_16_BIT;
                main_channel_num = aml_out->hal_ch;
                main_sample_rate = 48000;
            }
            /*we check whether there is enough space*/
            if ((adev->continuous_audio_mode == 1)
                && (is_dolby_ms12_support_compression_format(ms12_hal_format) || (ms12_hal_format == AUDIO_FORMAT_IEC61937)))
            {
                int max_size = 0;
                int main_avail = 0;
                int wait_retry = 0;
                do {
                    main_avail = dolby_ms12_get_main_buffer_avail(&max_size);
                    /* after flush, max_size value will be set to 0 and after write first data,
                     * it will be initialized
                     */
                    if (main_avail == 0 && max_size == 0) {
                        break;
                    }
                    if ((max_size - main_avail) >= main_frame_size) {
                        break;
                    }
                    pthread_mutex_unlock(&ms12->main_lock);
                    aml_audio_sleep(5*1000);
                    pthread_mutex_lock(&ms12->main_lock);
                    wait_retry++;
                    /*it cost 3s*/
                    if (wait_retry >= MS12_MAIN_WRITE_RETIMES) {
                        *use_size = parser_used_size;
                        if (parser_used_size == 0) {
                            *use_size = bytes;
                        }
                        ALOGE("write dolby main time out, discard data=%zu main_frame_size=%d", *use_size, main_frame_size);
                        goto exit;
                    }

                } while (aml_out->status != STREAM_STANDBY);
            }

            dolby_ms12_input_bytes = dolby_ms12_input_main(
                                                ms12->dolby_ms12_ptr
                                                , main_frame_buffer
                                                , main_frame_size
                                                , main_format
                                                , main_channel_num
                                                , main_sample_rate);

            if (adev->debug_flag >= 2)
                ALOGI("%s line %d main_frame_size %d ret dolby_ms12 input_bytes %d",
                    __func__, __LINE__, main_frame_size, dolby_ms12_input_bytes);

            if (adev->continuous_audio_mode == 0) {
                dolby_ms12_scheduler_run(ms12->dolby_ms12_ptr);
            }

            if (dolby_ms12_input_bytes > 0) {
                if (ms12->dual_decoder_support == true) {
                    *use_size = dual_decoder_used_bytes;
                } else {
                    if (adev->debug_flag >= 2) {
                        ALOGI("%s() continuous %d input ms12 bytes %d input bytes %zu sr %d main size %d parser size %d\n\n",
                              __FUNCTION__, adev->continuous_audio_mode, dolby_ms12_input_bytes, input_bytes, ms12->config_sample_rate, main_frame_size, single_decoder_used_bytes);
                    }
                    if (adev->continuous_audio_mode == 1) {

                        if (adev->debug_flag) {
                            ALOGI("%s line %d ret dolby_ms12 input_bytes %d",
                                    __func__, __LINE__, dolby_ms12_input_bytes);
                        }

                        //FIXME, if ddp input, the size suppose as CONTINUOUS_OUTPUT_FRAME_SIZE
                        //if pcm input, suppose 2ch/16bits/48kHz
                        //FIXME, that MAT/TrueHD input is TODO!!!
                        uint64_t input_ns = 0;
                        if ((ms12_hal_format == AUDIO_FORMAT_AC3) || \
                            (ms12_hal_format == AUDIO_FORMAT_E_AC3)) {
                            int sample_nums = aml_out->ddp_frame_nblks * SAMPLE_NUMS_IN_ONE_BLOCK;
                            int frame_duration = DDP_FRAME_DURATION(sample_nums*1000, DDP_OUTPUT_SAMPLE_RATE);
                            input_ns = (uint64_t)sample_nums * NANO_SECOND_PER_SECOND / sample_rate;
                            ALOGV("sample_nums=%d input_ns=%" PRId64 "", sample_nums, input_ns);
                            if (dependent_frame) {
                                input_ns = 0;
                            }
                            ms12->main_input_rate = sample_rate;
                        } else if(ms12_hal_format == AUDIO_FORMAT_IEC61937) {
                            input_ns = (uint64_t)1536 * 1000000000LL / sample_rate;
                            ms12->main_input_rate = sample_rate;
                        } else if (ms12_hal_format == AUDIO_FORMAT_AC4) {
                            if (ac4_info.frame_rate) {
                                input_ns = (uint64_t)NANO_SECOND_PER_SECOND * 1000 / ac4_info.frame_rate;
                            } else {
                                input_ns = 0;
                            }
                            ms12->main_input_rate = ac4_info.sample_rate;
                            ALOGV("input ns =%" PRId64 " frame rate=%d frame size=%d", input_ns, ac4_info.frame_rate, ac4_info.frame_size);
                        } else {
                            /*
                            for LPCM audio,we support it is 2 ch 48K audio.
                            */
                            if (main_channel_num == 0) {
                                main_channel_num = 2;
                            }
                            input_ns = (uint64_t)dolby_ms12_input_bytes * NANO_SECOND_PER_SECOND / (2 * main_channel_num) / ms12->config_sample_rate;
                            ms12->main_input_rate = ms12->config_sample_rate;
                        }
                        ms12->main_input_ns += input_ns;
                        aml_out->main_input_ns += input_ns;
                        audio_virtual_buf_process(aml_out->virtual_buf_handle, input_ns);
                    }

                    if (is_iec61937_format(stream)) {
                        *use_size = spdif_dec_used_size;
                    } else {
                        *use_size = dolby_ms12_input_bytes;
                        if (adev->continuous_audio_mode == 1) {
                            if (((ms12_hal_format == AUDIO_FORMAT_AC3)
                               || (ms12_hal_format == AUDIO_FORMAT_E_AC3)
                               || (ms12_hal_format == AUDIO_FORMAT_AC4))) {
                                *use_size = parser_used_size;
                            } else if (ms12_hal_format == AUDIO_FORMAT_IEC61937) {
                                *use_size = spdif_dec_used_size;
                            }
                        }
                    }

                }
            }
        } else {
            if (ms12->dual_decoder_support == true) {
                *use_size = dual_decoder_used_bytes;
            } else {
                *use_size = input_bytes;
            }
        }
        ms12->is_bypass_ms12 = is_ms12_passthrough(stream);
        ms12->is_dolby_atmos = (dolby_ms12_get_input_atmos_info() == 1);
exit:
        if (get_ms12_dump_enable(DUMP_MS12_INPUT_MAIN)) {
            dump_ms12_output_data((void*)buffer, *use_size, MS12_INPUT_SYS_MAIN_FILE);
        }
        ms12->ms12_main_input_size += *use_size;
        ret = 0;
    } else {
        ret = -1;
    }
    pthread_mutex_unlock(&ms12->main_lock);
    return ret;
}


/*
 *@brief dolby ms12 system process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */
int dolby_ms12_system_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    audio_channel_mask_t mixer_default_channelmask = AUDIO_CHANNEL_OUT_STEREO;
    int mixer_default_samplerate = 48000;
    int dolby_ms12_input_bytes = 0;
    int ms12_output_size = 0;
    int ret = -1;

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("ms12_system", buffer, bytes);
    }

    pthread_mutex_lock(&ms12->lock);
    if (ms12->dolby_ms12_enable) {
        //Dual input, here get the system data
        dolby_ms12_input_bytes =
            dolby_ms12_input_system(
                ms12->dolby_ms12_ptr
                , buffer
                , bytes
                , AUDIO_FORMAT_PCM_16_BIT
                , audio_channel_count_from_out_mask(mixer_default_channelmask)
                , mixer_default_samplerate);
        if (dolby_ms12_input_bytes > 0) {
            *use_size = dolby_ms12_input_bytes;
            ret = 0;
        } else {
            *use_size = 0;
            ret = -1;
        }
    }
    if (get_ms12_dump_enable(DUMP_MS12_INPUT_SYS)) {
        dump_ms12_output_data((void*)buffer, *use_size, MS12_INPUT_SYS_PCM_FILE);
    }
    pthread_mutex_unlock(&ms12->lock);

    if (adev->continuous_audio_mode == 1) {
        uint64_t input_ns = 0;
        input_ns = (uint64_t)(*use_size) * NANO_SECOND_PER_SECOND / 4 / mixer_default_samplerate;

        if (ms12->system_virtual_buf_handle == NULL) {
            //aml_audio_sleep(input_ns/1000);
            if (input_ns == 0) {
                input_ns = (uint64_t)(bytes) * NANO_SECOND_PER_SECOND / 4 / mixer_default_samplerate;
            }
            audio_virtual_buf_open(&ms12->system_virtual_buf_handle, "ms12 system input", input_ns/2, MS12_SYS_INPUT_BUF_NS, MS12_SYS_BUF_INCREASE_TIME_MS);
        }
        audio_virtual_buf_process(ms12->system_virtual_buf_handle, input_ns);
    }

    return ret;
}


/*
 *@brief dolby ms12 app process
 *
 * input parameters
 *     stream: audio_stream_out handle
 *     buffer: data buffer address
 *     bytes: data size
 * output parameters
 *     use_size: buffer used size
 */
int dolby_ms12_app_process(
    struct audio_stream_out *stream
    , const void *buffer
    , size_t bytes
    , size_t *use_size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    audio_channel_mask_t mixer_default_channelmask = AUDIO_CHANNEL_OUT_STEREO;
    int mixer_default_samplerate = 48000;
    int dolby_ms12_input_bytes = 0;
    int ms12_output_size = 0;
    int ret = 0;

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("ms12_app", buffer, bytes);
    }

    pthread_mutex_lock(&ms12->lock);
    if (ms12->dolby_ms12_enable) {
        //Dual input, here get the system data
        dolby_ms12_input_bytes =
            dolby_ms12_input_app(
                ms12->dolby_ms12_ptr
                , buffer
                , bytes
                , AUDIO_FORMAT_PCM_16_BIT
                , audio_channel_count_from_out_mask(mixer_default_channelmask)
                , mixer_default_samplerate);
        if (dolby_ms12_input_bytes > 0) {
            *use_size = dolby_ms12_input_bytes;
            ret = 0;
        } else {
            *use_size = 0;
            ret = -1;
        }
    }
    if (get_ms12_dump_enable(DUMP_MS12_INPUT_APP)) {
        dump_ms12_output_data((void*)buffer, *use_size, MS12_INPUT_SYS_APP_FILE);
    }
    pthread_mutex_unlock(&ms12->lock);

    return ret;
}


/*
 *@brief get dolby ms12 cleanup
 */
int get_dolby_ms12_cleanup(struct dolby_ms12_desc *ms12, bool set_non_continuous)
{
    int is_quit = 1;
    int i = 0;
    struct aml_audio_device *adev = NULL;
    ALOGI("+%s()", __FUNCTION__);
    if (!ms12) {
        return -EINVAL;
    }
    adev = ms12_to_adev(ms12);
    pthread_mutex_lock(&ms12->lock);
    pthread_mutex_lock(&ms12->main_lock);

    if (!ms12->dolby_ms12_init_flags) {
        ALOGI("ms12 is not init, don't need cleanup");
        if (set_non_continuous) {
            adev->continuous_audio_mode = 0;
            ALOGI("%s set ms12 to non continuous mode", __func__);
        }
        goto exit;
    }

    adev->doing_cleanup_ms12 = true;

    ALOGI("++%s(), locked", __FUNCTION__);
    ALOGI("%s() dolby_ms12_set_quit_flag %d", __FUNCTION__, is_quit);
    dolby_ms12_set_quit_flag(is_quit);

    if (ms12->dolby_ms12_threadID != 0) {
        ms12->dolby_ms12_thread_exit = true;
        pthread_join(ms12->dolby_ms12_threadID, NULL);
        ms12->dolby_ms12_threadID = 0;
        ALOGI("%s() dolby_ms12_threadID reset to %ld\n", __FUNCTION__, ms12->dolby_ms12_threadID);
    }
    set_audio_system_format(AUDIO_FORMAT_INVALID);
    set_audio_app_format(AUDIO_FORMAT_INVALID);
    set_audio_main_format(AUDIO_FORMAT_INVALID);
    dolby_ms12_flush_main_input_buffer();
    dolby_ms12_config_params_set_system_flag(false);
    dolby_ms12_config_params_set_app_flag(false);
    aml_ms12_cleanup(ms12);
    ms12->output_config = 0;
    ms12->dolby_ms12_enable = false;
    ms12->is_dolby_atmos = false;
    ms12->input_total_ms = 0;
    ms12->bitsteam_cnt = 0;
    ms12->nbytes_of_dmx_output_pcm_frame = 4; //2ch * 16bit, set a default one
    ms12->dual_bitstream_support = 0;
    ms12->is_bypass_ms12 = false;
    ms12->last_frames_postion = 0;
    ms12->main_input_ns = 0;
    ms12->main_output_ns = 0;
    ms12->main_input_rate = DDP_OUTPUT_SAMPLE_RATE;
    ms12->main_buffer_min_level = 0xFFFFFFFF;
    ms12->main_buffer_max_level = 0;
	ms12->dolby_ms12_init_flags = false;
    audio_virtual_buf_close(&ms12->system_virtual_buf_handle);
    aml_ac3_parser_close(ms12->ac3_parser_handle);
    ms12->ac3_parser_handle = NULL;
    aml_spdif_decoder_close(ms12->spdif_dec_handle);
    ms12->spdif_dec_handle = NULL;
    ring_buffer_release(&ms12->spdif_ring_buffer);
    if (ms12->lpcm_temp_buffer) {
        free(ms12->lpcm_temp_buffer);
        ms12->lpcm_temp_buffer = NULL;
    }
    aml_ms12_bypass_close(ms12->ms12_bypass_handle);
    ms12->ms12_bypass_handle = NULL;
    for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
        struct bitstream_out_desc * bitstream_out = &ms12->bitstream_out[i];
        if (bitstream_out->spdifout_handle) {
            aml_audio_spdifout_close(bitstream_out->spdifout_handle);
            bitstream_out->spdifout_handle = NULL;
        }
    }
    if (ms12->iec61937_ddp_buf) {
        aml_audio_free(ms12->iec61937_ddp_buf);
        ms12->iec61937_ddp_buf = NULL;
    }
    /*because we are still in lock, we can set continuous_audio_mode here safely*/
    if (set_non_continuous) {
        adev->continuous_audio_mode = 0;
        ALOGI("%s set ms12 to non continuous mode", __func__);
    }
    adev->doing_cleanup_ms12 = false;
exit:
    ALOGI("--%s(), locked", __FUNCTION__);
    pthread_mutex_unlock(&ms12->main_lock);
    pthread_mutex_unlock(&ms12->lock);
    ALOGI("-%s()", __FUNCTION__);
    return 0;
}

/*
 *@brief set dolby ms12 primary gain
 */
int set_dolby_ms12_primary_input_db_gain(struct dolby_ms12_desc *ms12, int db_gain , int duration)
{
    MixGain gain;
    int ret = 0;

    ALOGI("+%s(): gain %ddb, ms12 enable(%d)",
          __FUNCTION__, db_gain, ms12->dolby_ms12_enable);
    if (!ms12) {
        return -EINVAL;
    }

    //pthread_mutex_lock(&ms12->lock);
    if (!ms12->dolby_ms12_enable) {
        ret = -EINVAL;
        goto exit;
    }

    gain.target = db_gain;
    gain.duration = duration;
    gain.shape = 0;
    dolby_ms12_set_system_sound_mixer_gain_values_for_primary_input(&gain);
    dolby_ms12_set_input_mixer_gain_values_for_main_program_input(&gain);
    //Fixme when tunnel mode is working, the Alexa start and mute the main input!
    //dolby_ms12_set_input_mixer_gain_values_for_ott_sounds_input(&gain);
    // only update very limited parameter with out lock
    //ret = aml_ms12_update_runtime_params_lite(ms12);

exit:
    //pthread_mutex_unlock(&ms12->lock);
    return ret;
}

static ssize_t aml_ms12_spdif_output_new (struct audio_stream_out *stream,
                                struct bitstream_out_desc * bitstream_desc, audio_format_t output_format, audio_format_t sub_format, void *buffer, size_t byte)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    spdif_config_t spdif_config = { 0 };
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    int ret = 0;

    /*some switch happen*/
    if (bitstream_desc->spdifout_handle != NULL && bitstream_desc->audio_format != output_format) {
        ALOGI("spdif output format chamged from =0x%x to 0x%x", bitstream_desc->audio_format, output_format);
        aml_audio_spdifout_close(bitstream_desc->spdifout_handle);
        ALOGI("%s spdif format changed from 0x%x to 0x%x", __FUNCTION__, bitstream_desc->audio_format, output_format);
        bitstream_desc->spdifout_handle = NULL;
    }


    if (bitstream_desc->spdifout_handle == NULL) {
        /*we need update ms12 optical_format in the master pcm output*/
        if (ms12->optical_format != adev->optical_format) {
            ALOGI("wait ms12 optical fomrat update");
            return -1;
        }

        if (output_format == AUDIO_FORMAT_IEC61937) {
            spdif_config.audio_format = AUDIO_FORMAT_IEC61937;
            spdif_config.sub_format   = sub_format;
        } else {
            spdif_config.audio_format = output_format;
            spdif_config.sub_format   = output_format;
        }
        spdif_config.rate = DDP_OUTPUT_SAMPLE_RATE;
        /*for mat output, the rate should be 768 and 2ch, here we set 192, driver will convert to 768*/
        if (output_format == AUDIO_FORMAT_MAT) {
            spdif_config.rate = DDP_OUTPUT_SAMPLE_RATE * 4;
        }
        spdif_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        ret = aml_audio_spdifout_open(&bitstream_desc->spdifout_handle, &spdif_config);
        if (ret != 0) {
            ALOGE("open spdif out failed\n");
            return ret;
        }
        bitstream_desc->is_bypass_ms12 = ms12->is_bypass_ms12;
        /*for bypass case, we drop some data at the beginning to make sure it is stable*/
        ALOGI("is ms12 bypass =%d", ms12->is_bypass_ms12);
        if (bitstream_desc->is_bypass_ms12) {
            bitstream_desc->need_drop_frame = MS12_BYPASS_DROP_CNT;
        }
    }

    bitstream_desc->audio_format = output_format;
    if (bitstream_desc->need_drop_frame-- > 0) {
        ALOGV("drop some bypass frame at the beginning");
        return 0;
    }
    if (bitstream_desc->is_bypass_ms12) {
        if (ms12->main_volume < FLOAT_ZERO) {
            aml_audio_spdifout_mute(bitstream_desc->spdifout_handle, 1);
        } else {
            aml_audio_spdifout_mute(bitstream_desc->spdifout_handle, 0);
        }
    }
    ret = aml_audio_spdifout_processs(bitstream_desc->spdifout_handle, buffer, byte);


    return ret;
}

int dolby_ms12_bypass_process(struct audio_stream_out *stream, void *buffer, size_t bytes) {
    int ret = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct bitstream_out_desc *bitstream_out = &ms12->bitstream_out[BITSTREAM_OUTPUT_A];
    audio_format_t output_format =  ms12_get_audio_hal_format(aml_out->hal_format);
    ALOGV("output_format=0x%x hal_format=0x%#x internal=0x%x", output_format, aml_out->hal_format, aml_out->hal_internal_format);
    bool is_dolby = (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) || (aml_out->hal_internal_format == AUDIO_FORMAT_AC3);
    spdif_config_t spdif_config = { 0 };
    /*for patch mode, the hal_rate is not correct, we should parse it*/
    if (adev->audio_patching && is_dolby) {
        struct ac3_parser_info ac3_info = { 0 };
        void *main_frame_buffer = NULL;
        int32_t main_frame_size = 0;
        int32_t parser_used_size = 0;
        int32_t offset = 0;
        int32_t bytes_left = bytes;
        do {
            aml_ac3_parser_process(ms12->ac3_parser_handle, (char *)buffer + offset, bytes_left, &parser_used_size, &main_frame_buffer, &main_frame_size, &ac3_info);
            offset += parser_used_size;
            if (bytes_left >= parser_used_size) {
                bytes_left -= parser_used_size;
            } else {
                bytes_left = 0;
            }
            if (parser_used_size == 0) {
                ALOGE("%s error", __func__);
                break;
            }
            if (ac3_info.sample_rate != 0 && main_frame_size) {
                aml_out->hal_rate = ac3_info.sample_rate;
            }
        } while(bytes_left != 0);

    }

    ms12->is_bypass_ms12 = is_ms12_passthrough(stream);
    if (ms12->is_bypass_ms12
        && (adev->continuous_audio_mode == 0)
        && is_dolby
        && !ms12->dual_decoder_support) {
        if (bytes != 0 && buffer != NULL) {
            if (bitstream_out->spdifout_handle != NULL && bitstream_out->audio_format != output_format) {
                ALOGI("spdif output format chamged from =0x%x to 0x%x", bitstream_out->audio_format, output_format);
                aml_audio_spdifout_close(bitstream_out->spdifout_handle);
                ALOGI("%s spdif format changed from 0x%x to 0x%x", __FUNCTION__, bitstream_out->audio_format, output_format);
                bitstream_out->spdifout_handle = NULL;
            }

            if (bitstream_out->spdifout_handle == NULL) {
                if (output_format == AUDIO_FORMAT_IEC61937) {
                    spdif_config.audio_format = AUDIO_FORMAT_IEC61937;
                    spdif_config.sub_format   = aml_out->hal_internal_format;
                } else {
                    spdif_config.audio_format = output_format;
                    spdif_config.sub_format   = output_format;
                }
                spdif_config.rate = DDP_OUTPUT_SAMPLE_RATE;
                if (aml_out->hal_rate == 44100 ||
                    aml_out->hal_rate == 176400) {
                    spdif_config.rate = 44100;
                } else if (aml_out->hal_rate == 32000 ||
                           aml_out->hal_rate == 128000) {
                    spdif_config.rate = 32000;
                }
                spdif_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
                ret = aml_audio_spdifout_open(&bitstream_out->spdifout_handle, &spdif_config);
                if (ret != 0) {
                    ALOGE("%s open spdif out failed\n", __func__);
                    return ret;
                }
                bitstream_out->is_bypass_ms12 = ms12->is_bypass_ms12;
            }
        }

        bitstream_out->audio_format = output_format;
        if (ms12->main_volume < FLOAT_ZERO) {
            aml_audio_spdifout_mute(bitstream_out->spdifout_handle, 1);
        } else {
            aml_audio_spdifout_mute(bitstream_out->spdifout_handle, 0);
        }
        ret = aml_audio_spdifout_processs(bitstream_out->spdifout_handle, buffer, bytes);
    }

    return 0;
}

int master_pcm_type(struct aml_stream_out *aml_out) {
    bool dap_enable = is_dolbyms12_dap_enable(aml_out);
    if (dap_enable) {
        return DAP_LPCM;
    }
    return NORMAL_LPCM;
}

/*this function for continuous mode passthrough*/
int ms12_passthrough_output(struct aml_stream_out *aml_out) {
    int ret = 0;
    int i = 0;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    void *output_buf = NULL;
    int32_t out_size = 0;
    struct bypass_frame_info frame_info = { 0 };
    int  passthrough_delay_ms = 0;
    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    uint64_t ms12_dec_out_nframes = dolby_ms12_get_n_bytes_pcmout_of_udc();
    struct bitstream_out_desc *bitstream_out = &ms12->bitstream_out[BITSTREAM_OUTPUT_A];

    if (ms12_dec_out_nframes != 0 &&
        (hal_internal_format == AUDIO_FORMAT_E_AC3 || hal_internal_format == AUDIO_FORMAT_AC3)) {
        uint64_t consume_offset = dolby_ms12_get_consumed_payload();
        aml_ms12_bypass_checkout_data(ms12->ms12_bypass_handle, &output_buf, &out_size, consume_offset, &frame_info);
    }
    if ((adev->hdmi_format != BYPASS)) {
        ms12->is_bypass_ms12 = false;
    }
    if (ms12->is_bypass_ms12 != bitstream_out->is_bypass_ms12) {
        ALOGI("change to bypass mode from =%d to %d", bitstream_out->is_bypass_ms12, ms12->is_bypass_ms12);
        if (bitstream_out->spdifout_handle) {
            aml_audio_spdifout_close(bitstream_out->spdifout_handle);
        }
        memset(bitstream_out, 0, sizeof(struct bitstream_out_desc));
    }


    if (ms12->is_bypass_ms12) {
        ALOGV("bypass ms12 size=%d", out_size);
        output_format = hal_internal_format;
        if (ms12->is_continuous_paused) {
            for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
                struct bitstream_out_desc * bitstream_out = &ms12->bitstream_out[i];
                if (bitstream_out->spdifout_handle) {
                    aml_audio_spdifout_close(bitstream_out->spdifout_handle);
                    bitstream_out->spdifout_handle = NULL;
                }
            }
            out_size = 0;
        }

        if (out_size != 0 && output_buf != NULL) {
            struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
            ret = aml_ms12_spdif_output_new(stream_out, bitstream_out, output_format, aml_out->hal_internal_format, output_buf, out_size);
        }
        passthrough_delay_ms = aml_audio_spdifout_get_delay(bitstream_out->spdifout_handle);
        ALOGV("passthrough_delay_ms =%d", passthrough_delay_ms);
    }

    return ret;
}

/*this is the master output, it will do position calculate and av sync*/
static int ms12_output_master(void *buffer, void *priv_data, size_t size, audio_format_t output_format) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;

    int ret = 0;
    int i;

    /*we update the optical format in pcm, because it is always output*/
    if (ms12->optical_format != adev->optical_format) {
         ALOGI("ms12 optical format change from 0x%x to  0x%x\n",adev->ms12.optical_format,adev->optical_format);
         ms12->optical_format= adev->optical_format;
         for (i = 0; i < BITSTREAM_OUTPUT_CNT; i++) {
             struct bitstream_out_desc * bitstream_out = &ms12->bitstream_out[i];
             if (bitstream_out->spdifout_handle) {
                 aml_audio_spdifout_close(bitstream_out->spdifout_handle);
                 bitstream_out->spdifout_handle = NULL;
             }
         }
    }

    if (adev->continuous_audio_mode) {
        uint32_t sample_rate = ms12->main_input_rate ? ms12->main_input_rate : DDP_OUTPUT_SAMPLE_RATE;
        uint64_t ms12_dec_out_nframes = 0;
        if (!audio_is_linear_pcm(aml_out->hal_internal_format)) {
            if (adev->ms12.nbytes_of_dmx_output_pcm_frame) {
                ms12_dec_out_nframes = dolby_ms12_get_n_bytes_pcmout_of_udc() / adev->ms12.nbytes_of_dmx_output_pcm_frame;
            }
        } else if (aml_out->hw_sync_mode) {
            ms12_dec_out_nframes = dolby_ms12_get_consumed_payload() / 4;
        } else {
            ms12_dec_out_nframes = 0;
        }
        ms12->main_output_ns = ms12_dec_out_nframes * NANO_SECOND_PER_SECOND / sample_rate;
        ALOGV("format = 0x%x ms12_dec_out_nframes=%" PRId64 "", aml_out->hal_internal_format, ms12_dec_out_nframes);
    }

    ms12->is_dolby_atmos = (dolby_ms12_get_input_atmos_info() == 1);

    if (audio_hal_data_processing((struct audio_stream_out *)aml_out, buffer, size, &output_buffer, &output_buffer_bytes, output_format) == 0) {
        ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, output_format);
    }

    /*we put passthrough ms12 data here*/
    ms12_passthrough_output(aml_out);
    return ret;

}

int dap_pcm_output_l(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    int ret = 0;
    int i;

    ms12->dap_pcm_frames += size / (2 * 2);
    /*dump ms12 pcm output*/
    if (get_ms12_dump_enable(DUMP_MS12_OUTPUT_SPEAKER_PCM)) {
        dump_ms12_output_data(buffer, size, MS12_OUTPUT_SPEAKER_PCM_FILE);
    }
    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("ms12_dap_pcm", buffer, size);
    }

    if (is_dolbyms12_dap_enable(aml_out)) {
        ms12_output_master(buffer, priv_data, size, output_format);
        /* update the master pcm frame, which is used for av sync */
        ms12->master_pcm_frames += size / (2 * 2);
    } else
        return ret;
    if (adev->debug_flag > 1) {
        ALOGI("-%s() ret %d", __FUNCTION__, ret);
    }

    return ret;
}

int stereo_pcm_output_l(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;
    bool do_sync_flag = adev->patch_src  == SRC_DTV && patch && patch->skip_amadec_flag;
    int ret = 0;

    /* dtv avsync prechecking and processing */
    if (do_sync_flag && aml_out->dtvsync_enable) {
        aml_ms12_dec_info_t ms12_info;
        dtvsync_process_res process_result = DTVSYNC_AUDIO_OUTPUT;
        ms12_info.data_type = AUDIO_FORMAT_PCM_16_BIT;
        ms12_info.pcm_type = NORMAL_LPCM;
        process_result = aml_dtvsync_ms12_process_policy(priv_data, &ms12_info);
        if (process_result == DTVSYNC_AUDIO_DROP) {
            ms12->master_pcm_frames += size / (2 * 2);
            return ret;
        }
    }
    ret = stereo_pcm_output(buffer, priv_data, size, NULL);
    /* update the master pcm frame, which is used for av sync */
    if (!is_dolbyms12_dap_enable(aml_out)) {
        ms12->master_pcm_frames += size / (2 * 2);
    }
    return ret;
}

int dap_pcm_output(void *buffer, void *priv_data, size_t size,aml_ms12_dec_info_t *ms12_info __unused) {
    return dap_pcm_output_l(buffer, priv_data, size);
}

int stereo_pcm_output(void *buffer, void *priv_data, size_t size, aml_ms12_dec_info_t *ms12_info __unused) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    void *output_buffer = buffer;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    int ret = 0;
    int i;

    if (adev->debug_flag > 1) {
        ALOGI("+%s() size %zu", __FUNCTION__, size);
    }

    ms12->stereo_pcm_frames += size / (2 * 2);
    /*dump ms12 pcm output*/
    if (get_ms12_dump_enable(DUMP_MS12_OUTPUT_SPDIF_PCM)) {
        dump_ms12_output_data(buffer, size, MS12_OUTPUT_SPDIF_PCM_FILE);
    }

    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("ms12_stereo_pcm", buffer, size);
    }


    /*it has dap output, then this will be used for spdif output*/
    if (is_dolbyms12_dap_enable(aml_out)) {
        if (get_buffer_write_space (&ms12->spdif_ring_buffer) >= (int) size) {
            ring_buffer_write(&ms12->spdif_ring_buffer, buffer, size, UNCOVER_WRITE);
        }
    } else {
        ms12_output_master(buffer, priv_data, size, output_format);
    }

    if (adev->debug_flag > 1) {
        ALOGI("-%s() ret %d", __FUNCTION__, ret);
    }

    return ret;
}

int bitstream_output_l(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    bool do_sync_flag = adev->patch_src  == SRC_DTV && patch && patch->skip_amadec_flag;
    int ret = 0;

    /* dtv avsync prechecking and processing */
    if (do_sync_flag && aml_out->dtvsync_enable) {
        aml_dtvsync_t *aml_dtvsync = patch->dtvsync;
        struct dtvsync_audio_policy *async_policy = NULL;
        async_policy = &(aml_dtvsync->apolicy);
        if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {
            return DTVSYNC_AUDIO_DROP;
        }
    }
    ret = bitstream_output(buffer, priv_data, size);
    return ret;
}


int bitstream_output(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = AUDIO_FORMAT_AC3;
    struct bitstream_out_desc *bitstream_out = &ms12->bitstream_out[BITSTREAM_OUTPUT_A];
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    int ret = 0;
    int bitstream_delay_ms = 0;
    int out_size = 0;
    ms12->bitsteam_cnt++;

    if (adev->debug_flag > 1) {
        ALOGI("+%s() size %zu,dual_output = %d, optical_format = 0x%0x, sink_format = 0x%x out total=%d main in=%d",
            __FUNCTION__, size, aml_out->dual_output_flag, adev->optical_format, adev->sink_format, ms12->bitsteam_cnt, ms12->input_total_ms);
    }

    if (ms12->is_bypass_ms12) {
        return 0;
    }

    ms12->is_dolby_atmos = (dolby_ms12_get_input_atmos_info() == 1);

    if (adev->optical_format == AUDIO_FORMAT_PCM_16_BIT) {
        return 0;
    }

    if (ms12->optical_format != AUDIO_FORMAT_E_AC3) {
        return 0;
    }

    if (adev->patch_src ==  SRC_DTV && aml_out->need_drop_size > 0) {
        if (adev->debug_flag > 1)
            ALOGI("func:%s, av sync drop data,need_drop_size=%d\n",
                __FUNCTION__, aml_out->need_drop_size);
        return ret;
    }

    /*dump ms12 bitstream output*/
    if (get_ms12_dump_enable(DUMP_MS12_OUTPUT_BITSTREAN)) {
        dump_ms12_output_data(buffer, size, MS12_OUTPUT_BITSTREAM_FILE);
    }

    output_format = AUDIO_FORMAT_E_AC3;

    ms12_spdif_encoder(buffer, size, output_format, ms12->iec61937_ddp_buf, &out_size);

    ret = aml_ms12_spdif_output_new(stream_out, bitstream_out, AUDIO_FORMAT_IEC61937, AUDIO_FORMAT_E_AC3, ms12->iec61937_ddp_buf, out_size);

    bitstream_delay_ms = aml_audio_spdifout_get_delay(bitstream_out->spdifout_handle);
    ALOGV("%s delay=%d", __func__, bitstream_delay_ms);

    if (adev->debug_flag > 1) {
        ALOGI("-%s() ret %d", __FUNCTION__, ret);
    }

    return ret;
}

int spdif_bitstream_output_l(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    bool do_sync_flag = adev->patch_src  == SRC_DTV && patch && patch->skip_amadec_flag;
    int ret = 0;

    /* dtv avsync prechecking and processing */
    if (do_sync_flag && aml_out->dtvsync_enable) {
        aml_dtvsync_t *aml_dtvsync = patch->dtvsync;
        struct dtvsync_audio_policy *async_policy = NULL;
        async_policy = &(aml_dtvsync->apolicy);
        if (async_policy->audiopolicy == MEDIASYNC_AUDIO_DROP_PCM) {
            return DTVSYNC_AUDIO_DROP;
        }
    }
    ret = spdif_bitstream_output(buffer, priv_data, size);
    return ret;
}

int spdif_bitstream_output(void *buffer, void *priv_data, size_t size)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)priv_data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int   bitstream_id = BITSTREAM_OUTPUT_A;
    struct bitstream_out_desc *bitstream_out = NULL;
    struct audio_stream_out *stream_out = (struct audio_stream_out *)aml_out;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    audio_format_t output_format = AUDIO_FORMAT_AC3;
    int ret = 0;

    if (adev->debug_flag > 1) {
        ALOGI("+%s() size %zu,dual_output = %d, optical_format = 0x%x, sink_format = 0x%x out total=%d main in=%d",
            __FUNCTION__, size, aml_out->dual_output_flag, adev->optical_format, adev->sink_format, ms12->bitsteam_cnt, ms12->input_total_ms);
    }

    if (ms12->is_bypass_ms12) {
        /*non dual bitstream case, we only have one spdif*/
        if (!ms12->dual_bitstream_support) {
            return 0;
        }
        /*input is ac3, we can bypass it*/
        if (ms12->main_input_fmt == AUDIO_FORMAT_AC3) {
            return 0;
        }
        /*when main stream is paused, we also doesn't need output spdif*/
        if (ms12->is_continuous_paused) {
            return 0;
        }

    }

    if (ms12->dual_bitstream_support) {
        bitstream_id = BITSTREAM_OUTPUT_B;
    }

    bitstream_out = &ms12->bitstream_out[bitstream_id];

    if (adev->optical_format == AUDIO_FORMAT_PCM_16_BIT) {
        return 0;
    }

    if (ms12->optical_format != AUDIO_FORMAT_AC3 && !ms12->dual_bitstream_support) {
        return 0;
    }

    if (adev->patch_src ==  SRC_DTV && aml_out->need_drop_size > 0) {
        if (adev->debug_flag > 1)
            ALOGI("func:%s, av sync drop data,need_drop_size=%d\n",
                __FUNCTION__, aml_out->need_drop_size);
        return ret;
    }

    /*dump ms12 bitstream output*/
    if (get_ms12_dump_enable(DUMP_MS12_OUTPUT_BITSTREAN2)) {
        dump_ms12_output_data(buffer, size, MS12_OUTPUT_BITSTREAM2_FILE);
    }

    ret = aml_ms12_spdif_output_new(stream_out, bitstream_out, output_format, output_format, buffer, size);
    if (adev->debug_flag > 1) {
        ALOGI("-%s() ret %d", __FUNCTION__, ret);
    }
    return ret;
}


int mat_bitstream_output(void *buffer __unused, void *priv_data __unused, size_t size __unused)
{
    return 0;
}


static void *dolby_ms12_threadloop(void *data)
{
    ALOGI("+%s() ", __FUNCTION__);
    struct aml_stream_out *aml_out = (struct aml_stream_out *)data;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    if (ms12 == NULL) {
        ALOGE("%s ms12 pointer invalid!", __FUNCTION__);
        goto Error;
    }

    if (ms12->dolby_ms12_enable) {
        dolby_ms12_set_quit_flag(ms12->dolby_ms12_thread_exit);
    }

    prctl(PR_SET_NAME, (unsigned long)"DOLBY_MS12");
    aml_set_thread_priority("DOLBY_MS12", ms12->dolby_ms12_threadID);

    /*affinity the thread to cpu 2/3 which has few IRQ*/
    aml_audio_set_cpu23_affinity();

    while ((ms12->dolby_ms12_thread_exit == false) && (ms12->dolby_ms12_enable)) {
        ALOGV("%s() goto dolby_ms12_scheduler_run", __FUNCTION__);
        if (ms12->dolby_ms12_ptr) {
            dolby_ms12_scheduler_run(ms12->dolby_ms12_ptr);
        } else {
            ALOGE("%s() ms12->dolby_ms12_ptr is NULL, fatal error!", __FUNCTION__);
            break;
        }
        ALOGV("%s() dolby_ms12_scheduler_run end", __FUNCTION__);
    }
    ALOGI("%s remove   ms12 stream %p", __func__, aml_out);
    if (continous_mode(adev)) {
        pthread_mutex_lock(&adev->alsa_pcm_lock);
        aml_alsa_output_close((struct audio_stream_out*)aml_out);
        adev->spdif_encoder_init_flag = false;
        struct pcm *pcm = adev->pcm_handle[DIGITAL_DEVICE];
        if (aml_out->dual_output_flag && pcm) {
            ALOGI("%s close dual output pcm handle %p", __func__, pcm);
            pcm_close(pcm);
            adev->pcm_handle[DIGITAL_DEVICE] = NULL;
            aml_out->dual_output_flag = 0;
        }
        pthread_mutex_unlock(&adev->alsa_pcm_lock);
        release_audio_stream((struct audio_stream_out *)aml_out);
    }
    adev->ms12_out = NULL;
    ALOGI("-%s(), exit dolby_ms12_thread\n", __FUNCTION__);
    return ((void *)0);

Error:
    ALOGI("-%s(), exit dolby_ms12_thread, because of erro input params\n", __FUNCTION__);
    return ((void *)0);
}

int set_system_app_mixing_status(struct aml_stream_out *aml_out, int stream_status)
{
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_OFF;
    int ret = 0;

    if (STREAM_STANDBY == stream_status) {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_OFF;
    } else {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    }

    //when under continuous_audio_mode, system app sound mixing always on.
    if (adev->continuous_audio_mode) {
        system_app_mixing_status = SYSTEM_APP_SOUND_MIXING_ON;
    }

    adev->system_app_mixing_status = system_app_mixing_status;

    if (adev->debug_flag) {
        ALOGI("%s stream-status %d set system-app-audio-mixing %d current %d continuous_audio_mode %d\n", __func__,
              stream_status, system_app_mixing_status, dolby_ms12_get_system_app_audio_mixing(), adev->continuous_audio_mode);
    }

    dolby_ms12_set_system_app_audio_mixing(system_app_mixing_status);

    if (ms12->dolby_ms12_enable) {
        pthread_mutex_lock(&ms12->lock);
        set_dolby_ms12_runtime_system_mixing_enable(ms12, system_app_mixing_status);
        pthread_mutex_unlock(&ms12->lock);
        ALOGI("%s return %d stream-status %d set system-app-audio-mixing %d\n",
              __func__, ret, stream_status, system_app_mixing_status);
        return ret;
    }

    return 1;
}


static int nbytes_of_dolby_ms12_downmix_output_pcm_frame()
{
    int pcm_out_chanenls = 2;
    int bytes_per_sample = 2;

    return pcm_out_chanenls*bytes_per_sample;
}

int dolby_ms12_main_open(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    ms12->ms12_main_stream_out = aml_out;
    ms12->main_input_fmt = hal_internal_format;
    aml_out->is_ms12_main_decoder = true;
    if (adev->continuous_audio_mode && (aml_out->virtual_buf_handle == NULL)) {
        uint64_t buf_ns_begin  = MS12_MAIN_INPUT_BUF_NONEPCM_NS;
        uint64_t buf_ns_target = MS12_MAIN_INPUT_BUF_NONEPCM_NS;
        if (audio_is_linear_pcm(aml_out->hal_internal_format)) {
            buf_ns_begin  = MS12_MAIN_INPUT_BUF_PCM_NS;
            buf_ns_target = MS12_MAIN_INPUT_BUF_PCM_NS;
        }
        audio_virtual_buf_open(&aml_out->virtual_buf_handle
            , "ms12 main input"
            , buf_ns_begin
            , buf_ns_target
            , MS12_MAIN_BUF_INCREASE_TIME_MS);
    }
    if (!audio_is_linear_pcm(aml_out->hal_internal_format)) {
        dynamic_set_dolby_ms12_drc_parameters(ms12);
    }

    return 0;
}

int dolby_ms12_main_close(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    aml_out->is_ms12_main_decoder = false;

    /** for low probability timing case, open1-->***-->open2->close1->****-->close2
    *** like above case, the ms12_main_stream_out is setted null when close1 output_stream,
    *** it lead to ms12 flush/pause/resume message cant send to ms12 thread in open2 output_stream.
    *** so add ms12_main_stream_out address pointed check to protect this case.
    **/
    if ((unsigned char *)aml_out == (unsigned char*)ms12->ms12_main_stream_out) {
        ms12->ms12_main_stream_out = NULL;
        ms12->is_bypass_ms12 = false;
        adev->ms12.main_input_fmt = AUDIO_FORMAT_PCM_16_BIT;

        /*when main stream is closed, we must set the pause to false*/
        dolby_ms12_set_pause_flag(false);
        adev->ms12.is_continuous_paused = false;

        /*the main stream is closed, we should update the sink format now*/
        if (adev->active_outputs[STREAM_PCM_NORMAL]) {
            get_sink_format(&adev->active_outputs[STREAM_PCM_NORMAL]->stream);
        }
    } else {
        ALOGD("%s  aml_out is not equal with ms12_main_stream_out, ms12 resource not release.", __func__);
    }

    if (aml_out->virtual_buf_handle) {
        audio_virtual_buf_close(&aml_out->virtual_buf_handle);
    }
    if (ms12->dolby_ms12_enable) {
        set_ms12_drc_mode_for_2ch_downmixed_output(ms12, DOLBY_DRC_LINE_MODE);
    }
    return 0;
}


int dolby_ms12_main_flush(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    ms12->main_input_ns = 0;
    ms12->main_output_ns = 0;
    ms12->main_input_rate = DDP_OUTPUT_SAMPLE_RATE;
    ms12->main_buffer_min_level = 0xFFFFFFFF;
    ms12->main_buffer_max_level = 0;
    ms12->last_frames_postion = 0;
    ms12->last_ms12_pcm_out_position = 0;
    adev->ms12.ms12_position_update = false;
    adev->ms12.main_input_start_offset_ns = 0;
    aml_out->main_input_ns = 0;

    if (aml_out->hal_internal_format == AUDIO_FORMAT_AC4) {
        ms12->master_pcm_frames = 0;
    }

    dolby_ms12_flush_main_input_buffer();

    if (ms12->spdif_dec_handle) {
        aml_spdif_decoder_reset(ms12->spdif_dec_handle);
    }

    if (ms12->ac3_parser_handle) {
        aml_ac3_parser_reset(ms12->ac3_parser_handle);
    }
    if (ms12->ms12_bypass_handle) {
        aml_ms12_bypass_reset(ms12->ms12_bypass_handle);
    }
    ALOGI("%s exit", __func__);
    return 0;
}

void dolby_ms12_app_flush()
{
    dolby_ms12_flush_app_input_buffer();
}

bool is_ms12_continous_mode(struct aml_audio_device *adev)
{
    if ((eDolbyMS12Lib == adev->dolby_lib_type) && (adev->continuous_audio_mode)) {
        return true;
    } else {
        return false;
    }
}

bool is_dolby_ms12_main_stream(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    bool is_bitstream_stream = !audio_is_linear_pcm(hal_internal_format);
    bool is_hwsync_pcm_stream = (audio_is_linear_pcm(hal_internal_format) && (aml_out->flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC));
    if (is_bitstream_stream || is_hwsync_pcm_stream) {
        return true;
    } else {
        return false;
    }
}

bool is_support_ms12_reset(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    bool is_atmos_supported = is_platform_supported_ddp_atmos(adev->hdmi_descs.ddp_fmt.atmos_supported, adev->active_outport, adev->is_TV);
    bool need_reset_ms12_out = !is_ms12_out_ddp_5_1_suitable(is_atmos_supported);
    /* we meet 3 conditions:
     * 1. edid atmos support not match with currently ms12 output
     * 2. it is the main stream
     * 3. it has write some data
     */
    if (is_dolby_ms12_main_stream(stream) && need_reset_ms12_out) {
        return true;
    }

    return false;
}


/*
 *The audio data(direct/offload/hwsync) should bypass Dolby MS12,
 *if Audio Mixing is Off, and (Sink & Output) format are both EAC3,
 *specially, the dual decoder is false and continuous audio mode is false.
 *because Dolby MS12 is working at LiveTV+(Dual Decoder) or Continuous Mode.
 */
 bool is_bypass_dolbyms12(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    bool is_dts = is_dts_format(hal_internal_format);
    bool is_dolby_audio = is_dolby_format(hal_internal_format);

    return (is_dts
            || is_high_rate_pcm(stream)
            || (is_multi_channel_pcm(stream) && (adev->hdmi_format == BYPASS)));
}

bool is_audio_postprocessing_add_dolbyms12_dap(struct aml_audio_device *adev)
{
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    bool is_dap_enable = (adev->active_outport == OUTPORT_SPEAKER) && (!adev->ms12.dap_bypass_enable);

    if (ms12->dolby_ms12_enable && is_dap_enable && (ms12->output_config & MS12_OUTPUT_MASK_SPEAKER)) {
        is_dap_enable =  true;
    }
    else {
        is_dap_enable =  false;
    }

    return is_dap_enable;
}

bool is_dolbyms12_dap_enable(struct aml_stream_out *aml_out) {
    struct aml_audio_device *adev = aml_out->dev;

#if 0
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    bool is_dap_enable = (adev->active_outport == OUTPORT_SPEAKER) && (!adev->ms12.dap_bypass_enable);
    is_dap_enable = (ms12->dolby_ms12_enable && is_dap_enable && (ms12->output_config & MS12_OUTPUT_MASK_SPEAKER)) ? true : false;
    return is_dap_enable;
#else
    return is_audio_postprocessing_add_dolbyms12_dap(adev);
#endif
}

int dolby_ms12_hwsync_init(void) {
    return dolby_ms12_hwsync_init_internal();
}

int dolby_ms12_hwsync_release(void) {
    return dolby_ms12_hwsync_release_internal();
}

int dolby_ms12_hwsync_checkin_pts(int offset, int apts) {
    return dolby_ms12_hwsync_checkin_pts_internal(offset, apts);
}
int dolby_ms12_output_insert_oneframe(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ret = 0;
    char *mute_pcm_buffer = 0;
    char *mute_raw_buffer = 0;
    int  pcm_buffer_size = MS12_PCM_FRAME_SIZE;
    int  raw_buffer_size = MS12_DDP_FRAME_SIZE;
    size_t output_buffer_bytes = 0;
    size_t raw_size = 0;
    void *output_buffer = NULL;
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    struct bitstream_out_desc *bitstream_out = &ms12->bitstream_out[BITSTREAM_OUTPUT_A];
    bool b_raw_out = false;

    mute_pcm_buffer =  aml_audio_calloc(1, pcm_buffer_size);
    mute_raw_buffer =  aml_audio_calloc(1, raw_buffer_size);

    if (mute_pcm_buffer == NULL ||
        mute_raw_buffer == NULL) {
        ret = -1;
        goto exit;
    }

    if (ms12->optical_format == AUDIO_FORMAT_AC3 || ms12->optical_format == AUDIO_FORMAT_E_AC3) {
        output_format = ms12->optical_format;
        b_raw_out = true;
        if (output_format == AUDIO_FORMAT_AC3) {
            raw_size = sizeof(ms12_muted_dd_raw);
            memcpy(mute_raw_buffer, ms12_muted_dd_raw, raw_size);
        } else {
            raw_size = sizeof(ms12_muted_ddp_raw);
            memcpy(mute_raw_buffer, ms12_muted_ddp_raw, raw_size);
        }
    }

    /*insert pcm data*/
    if (audio_hal_data_processing((struct audio_stream_out *)aml_out, mute_pcm_buffer, pcm_buffer_size, &output_buffer, &output_buffer_bytes, AUDIO_FORMAT_PCM_16_BIT) == 0) {
        ret = hw_write((struct audio_stream_out *)aml_out, output_buffer, output_buffer_bytes, AUDIO_FORMAT_PCM_16_BIT);
    }

    /*insert raw data*/
    if (b_raw_out) {
        ret = aml_ms12_spdif_output_new(stream, bitstream_out, output_format, output_format, mute_raw_buffer, raw_size);
    }

exit:
    if (mute_pcm_buffer) {
        aml_audio_free(mute_pcm_buffer);
    }
    if (mute_raw_buffer) {
        aml_audio_free(mute_raw_buffer);
    }

    return ret;
}

unsigned long long dolby_ms12_get_main_bytes_consumed(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    return dolby_ms12_get_consumed_payload();
}

unsigned long long dolby_ms12_get_main_pcm_generated(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    unsigned long long decoded_frame = 0;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    uint64_t main_input_offset_frame = 0;
    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    main_input_offset_frame = CONVERT_NS_TO_48K_FRAME_NUM(ms12->main_input_start_offset_ns);

    if (!audio_is_linear_pcm(hal_internal_format)) {
        if (adev->ms12.nbytes_of_dmx_output_pcm_frame) {
            decoded_frame = dolby_ms12_get_n_bytes_pcmout_of_udc() / adev->ms12.nbytes_of_dmx_output_pcm_frame;
            if (!adev->continuous_audio_mode
                && aml_out->hal_rate != DDP_OUTPUT_SAMPLE_RATE
                && aml_out->hal_rate != 0) {
                decoded_frame = decoded_frame * DDP_OUTPUT_SAMPLE_RATE / aml_out->hal_rate;
            }
        }
    } else {
        decoded_frame = dolby_ms12_get_consumed_payload() / 4;
    }
    return (main_input_offset_frame + decoded_frame);
}

bool is_rebuild_the_ms12_pipeline(    audio_format_t main_input_fmt, audio_format_t hal_internal_format)
{
    ALOGD("%s line %d main_input_fmt %#x hal_internal_format %#x\n",__func__, __LINE__, main_input_fmt, hal_internal_format);

    bool is_ac4_alive = (main_input_fmt == AUDIO_FORMAT_AC4);
    bool is_mat_alive = (main_input_fmt == AUDIO_FORMAT_MAT);
    bool is_ott_format_alive = (main_input_fmt == AUDIO_FORMAT_AC3) || \
                                ((main_input_fmt & AUDIO_FORMAT_E_AC3) == AUDIO_FORMAT_E_AC3) || \
                                (main_input_fmt == AUDIO_FORMAT_PCM_16_BIT);
    ALOGD("%s line %d is_ac4_alive %d is_mat_alive %d is_ott_format_alive %d\n",__func__, __LINE__, is_ac4_alive, is_mat_alive, is_ott_format_alive);

    bool request_ac4_alive = (hal_internal_format == AUDIO_FORMAT_AC4);
    bool request_mat_alive = (hal_internal_format == AUDIO_FORMAT_MAT);
    bool request_ott_format_alive = (hal_internal_format == AUDIO_FORMAT_AC3) || \
                                ((hal_internal_format & AUDIO_FORMAT_E_AC3) == AUDIO_FORMAT_E_AC3) || \
                                (hal_internal_format == AUDIO_FORMAT_PCM_16_BIT);
    ALOGD("%s line %d request_ac4_alive %d request_mat_alive %d request_ott_format_alive %d\n",__func__, __LINE__, request_ac4_alive, request_mat_alive, request_ott_format_alive);

    if (request_ac4_alive && (is_ac4_alive^request_ac4_alive)) {
        //new AC4 stream appears when last stream played MAT/DD/DDP
        ALOGD("%s line %d main_input_fmt %#x hal_internal_format %#x request_ac4_alive^is_mat_alive %d request_ac4_alive^is_ott_format_alive %d\n",
            __func__, __LINE__, main_input_fmt, hal_internal_format, request_ac4_alive^is_mat_alive, request_ac4_alive^is_ott_format_alive);
        return (request_ac4_alive^is_mat_alive) || (request_ac4_alive^is_ott_format_alive);
    }
    else if (request_mat_alive && (is_mat_alive^request_mat_alive)) {
        //new MAT stream appears when last steam played AC4/DD/DDP
        ALOGD("%s line %d main_input_fmt %#x hal_internal_format %#x (request_mat_alive^is_ac4_alive) %d (request_mat_alive^is_ott_format_alive) %d\n",
            __func__, __LINE__, main_input_fmt, hal_internal_format, (request_mat_alive^is_ac4_alive), (request_mat_alive^is_ott_format_alive));
        return (request_mat_alive^is_ac4_alive) || (request_mat_alive^is_ott_format_alive);
    }
    else if (request_ott_format_alive && (is_ott_format_alive^request_ott_format_alive)){
        //new ott(dd/ddp/ddp_joc) format appears when last stream played AC4/MAT
        ALOGD("%s line %d main_input_fmt %#x hal_internal_format %#x (request_ott_format_alive^is_ac4_alive) %d (request_ott_format_alive^is_mat_alive) %d\n",
            __func__, __LINE__, main_input_fmt, hal_internal_format, (request_ott_format_alive^is_ac4_alive), (request_ott_format_alive^is_mat_alive));
        return (request_ott_format_alive^is_ac4_alive) || (request_ott_format_alive^is_mat_alive);
    }
    else {
        ALOGE("%s line %d main_input_fmt %#x hal_internal_format %#x return false\n",
            __func__, __LINE__, main_input_fmt, hal_internal_format);
        return false;
    }
}

bool is_need_reset_ms12_continuous(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    unsigned int hal_rate = aml_out->hal_rate;
    audio_format_t hal_internal_format = ms12_get_audio_hal_format(aml_out->hal_internal_format);
    /*check the UI setting and source/sink format */
    if (is_bypass_dolbyms12(stream)) {
        return false;
    }

    if (!adev->continuous_audio_mode || !adev->ms12.dolby_ms12_enable) {
        return false;
    }

    /*IEC61937 DDP format, the real samplerate need device by 4*/
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        if ((hal_internal_format & AUDIO_FORMAT_E_AC3) == AUDIO_FORMAT_E_AC3) {
            hal_rate /= 4;
        }
    }

    if (is_dolby_ms12_support_compression_format(hal_internal_format) && \
         (is_rebuild_the_ms12_pipeline(adev->ms12.main_input_fmt,hal_internal_format) || \
          hal_rate != adev->ms12.main_input_sr)) {
        return true;
    }

    return false;
}

bool is_ms12_output_compatible(struct audio_stream_out *stream, audio_format_t new_sink_format, audio_format_t new_optical_format) {
    bool is_compatible = false;
    int  output_config = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);

    if (adev->hdmi_format == BYPASS || adev->hdmi_format == PCM) {
        /*for bypass case and pcm case, it is always compatible*/
        return true;
    }
    output_config = get_ms12_output_mask(new_sink_format, new_optical_format, false);
    is_compatible = (ms12->output_config & output_config);
    ALOGI("ms12 current out=%#x new output=%#x is_compatible=%d", ms12->output_config, output_config, is_compatible);
    return is_compatible;

}

int dolby_ms12_main_pipeline_latency_frames(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    /*after enable ms12 v1.3.2, we need implement it*/
    return 0;
}

