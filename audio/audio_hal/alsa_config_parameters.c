
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

#include <cutils/log.h>
#include <system/audio.h>
#include <hardware/audio.h>
#include <tinyalsa/asoundlib.h>


#include "alsa_config_parameters.h"

#define PERIOD_SIZE                     1024
#define HARDWARE_CHANNEL_STEREO         2
#define HARDWARE_CHANNEL_7_1_MULTI      8
#define LOW_LATENCY_PERIOD_SIZE                     256
#define LOW_LATENCY_PLAYBACK_PERIOD_COUNT           3


/*
 *@brief get the hardware config parameters when the output format is DTS-HD/TRUE-HD
 */
static void get_dts_hd_hardware_config_parameters(
    struct pcm_config *hardware_config
    , unsigned int channels __unused
    , unsigned int rate)
{
    hardware_config->channels = 2;
    hardware_config->format = PCM_FORMAT_S16_LE;
    //TODO, maybe we should use "/sys/class/audiodsp/digtal_codec" as 4
    hardware_config->rate = rate * 4;
    hardware_config->period_count = PLAYBACK_PERIOD_COUNT;
    hardware_config->period_size = PERIOD_SIZE * 4 * 2;
    hardware_config->start_threshold = PLAYBACK_PERIOD_COUNT * hardware_config->period_size;
    hardware_config->avail_min = 0;

    return ;
}

/*
 *@brief get the hardware config parameters when the output format is MAT
*/
static void get_mat_hardware_config_parameters(
    struct pcm_config *hardware_config
    , unsigned int channels __unused
    , unsigned int rate)
{
    hardware_config->channels = 2;
    hardware_config->format = PCM_FORMAT_S16_LE;
    // for android P, p212 platform found that the rate should not muliply by 4
    hardware_config->rate = rate;
    hardware_config->period_count = PLAYBACK_PERIOD_COUNT;
    //hardware_config->period_size = PERIOD_SIZE /* * 4 */;
    hardware_config->period_size = 6144 * 2; /* period_size in frame unit, MAT IEC61937 frame size (61440) bytes */
    hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count/2;
    hardware_config->avail_min = 0;

    return ;
}

/*
 *@brief get the hardware config parameters when the output format is DDP
*/
static void get_ddp_hardware_config_parameters(
    struct pcm_config *hardware_config
    , unsigned int channels __unused
    , unsigned int rate
    , bool continuous_mode)
{
    hardware_config->channels = 2;
    hardware_config->format = PCM_FORMAT_S16_LE;
    hardware_config->rate = rate /* * 4 */;
    hardware_config->period_count = PLAYBACK_PERIOD_COUNT;
    if (continuous_mode) {
        hardware_config->period_size = PERIOD_SIZE * 4 * 2;
        hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 4;
    } else {
        hardware_config->period_size = PERIOD_SIZE * 4;
        hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 2;
    }
    hardware_config->avail_min = 0;

    return ;
}

/*
 *@brief get the hardware config parameters when the output format is DD
*/
static void get_dd_hardware_config_parameters(
    struct pcm_config *hardware_config
    , unsigned int channels __unused
    , unsigned int rate
    , bool continuous_mode)
{
    hardware_config->channels = 2;
    hardware_config->format = PCM_FORMAT_S16_LE;
    hardware_config->rate = rate;
    hardware_config->period_size = PERIOD_SIZE;
    if (continuous_mode) {
        hardware_config->period_count = PLAYBACK_PERIOD_COUNT * 2;
        hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 4;
    } else {
        hardware_config->period_count = PLAYBACK_PERIOD_COUNT;
        hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 2;
    }
    hardware_config->avail_min = 0;

    return ;
}

/*
 *@brief get the hardware config parameters when the output format is PCM
*/
static void get_pcm_hardware_config_parameters(
    struct pcm_config *hardware_config
    , unsigned int channels
    , unsigned int rate
    , bool platform_is_tv
    , bool continuous_mode
    , bool game_mode)
{
    if (platform_is_tv == false) {
        if (channels <= 2) {
            hardware_config->channels = HARDWARE_CHANNEL_STEREO;
            hardware_config->format = PCM_FORMAT_S16_LE;
        }
        else {
            hardware_config->channels = channels;
            hardware_config->format = PCM_FORMAT_S32_LE;
        }
    }
    else {
        hardware_config->channels = channels;
        hardware_config->format = PCM_FORMAT_S32_LE;
    }
    hardware_config->rate = rate;//defualt sample rate = 48KHz
    if (!game_mode)
        hardware_config->period_size = PERIOD_SIZE;
    else
        hardware_config->period_size = LOW_LATENCY_PERIOD_SIZE;
    /*
    Currently, alsa buffer configured a limited buffer size max 32bit * 8 ch  1024 *8
    the configuration will return fail when channel > 8 as need larger dma buffer size.
    to save mem, we use low buffer memory when 8 ch + speaker product.
   */
    if (continuous_mode && channels <= 8) {
        hardware_config->period_count = PLAYBACK_PERIOD_COUNT * 2;
        hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 4;
    } else {
        if (!game_mode) {
            hardware_config->period_count = PLAYBACK_PERIOD_COUNT;
            hardware_config->start_threshold = hardware_config->period_size * hardware_config->period_count / 2;
        } else {
            hardware_config->period_count = LOW_LATENCY_PLAYBACK_PERIOD_COUNT;
            hardware_config->start_threshold = hardware_config->period_size;
        }
    }
    hardware_config->avail_min = 0;


    return ;
}

/*
 *@brief get the hardware config parameters
*/
int get_hardware_config_parameters(
    struct pcm_config *final_config
    , audio_format_t output_format
    , unsigned int channels
    , unsigned int rate
    , bool platform_is_tv
    , bool continuous_mode
    , bool game_mode)
{
    ALOGI("%s()\n", __FUNCTION__);
    //DD+
    /* for raw data, we fixed to 2ch as it use spdif module */
    if (output_format == AUDIO_FORMAT_E_AC3) {
        get_ddp_hardware_config_parameters(final_config, 2, rate, continuous_mode);
    }
    //DD
    else if (output_format == AUDIO_FORMAT_AC3) {
        get_dd_hardware_config_parameters(final_config, 2, rate, continuous_mode);
    }
    //MAT
    else if (output_format == AUDIO_FORMAT_MAT) {
        get_mat_hardware_config_parameters(final_config, 2, rate);

    }
    //DTS-HD/TRUE-HD
    else if ((output_format == AUDIO_FORMAT_DTS_HD) || (output_format == AUDIO_FORMAT_DOLBY_TRUEHD)) {
        get_dd_hardware_config_parameters(final_config, 2, rate, continuous_mode);
    }
    //PCM
    else {
        get_pcm_hardware_config_parameters(final_config, channels, rate,
                platform_is_tv, continuous_mode, game_mode);
    }
    ALOGI("%s() channels %d format %d period_count %d period_size %d rate %d\n",
            __FUNCTION__, final_config->channels, final_config->format, final_config->period_count,
            final_config->period_size, final_config->rate);

    return 0;
}
