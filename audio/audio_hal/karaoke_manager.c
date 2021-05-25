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
#include <audio_utils/channels.h>

#include "audio_data_process.h"
#include "audio_hw_utils.h"
#include "karaoke_manager.h"
#include "aml_volume_utils.h"
//#include "EffectReverb.h"

#define USB_DEFAULT_PERIOD_SIZE 512
#define USB_DEFAULT_PERIOD_COUNT 2

static ssize_t voice_in_read(struct voice_in *in, void *buffer, size_t bytes)
{
    size_t num_read_buff_bytes = bytes;
    void *read_buff = buffer;
    void *out_buff = buffer;
    int num_device_channels = proxy_get_channel_count(&in->proxy);
    int num_req_channels = in->cfg.channelCnt;
    int ret = 0;

    if (num_device_channels != num_req_channels) {
        ALOGV("%s() device channels: %d, req channels: %d",
            __func__, num_device_channels, num_req_channels);
        num_read_buff_bytes = (num_device_channels * num_read_buff_bytes) / num_req_channels;
    }

    /* Setup/Realloc the conversion buffer (if necessary). */
    if (num_read_buff_bytes != bytes) {
        if (num_read_buff_bytes > in->conversion_buffer_size) {
            ALOGV("num_read_buff_bytes:%d conversion_buffer_size:%d",
                num_read_buff_bytes, in->conversion_buffer_size);
            in->conversion_buffer_size = num_read_buff_bytes;
            in->conversion_buffer = realloc(in->conversion_buffer, in->conversion_buffer_size);
        }
        read_buff = in->conversion_buffer;
    }

    ret = proxy_read(&in->proxy, read_buff, num_read_buff_bytes);
    if (ret == 0) {
        //ALOGV("%s(), num_read_buff_bytes %d", __func__, num_read_buff_bytes);
        if (in->debug) {
            aml_audio_dump_audio_bitstreams("/data/tmp/karaoke_usb.raw", read_buff, num_read_buff_bytes);
        }

        if (num_device_channels != num_req_channels) {
            out_buff = buffer;
            /* Num Channels conversion */
            if (num_device_channels != num_req_channels) {
                enum pcm_format format = proxy_get_format(&in->proxy);
                unsigned sample_size_in_bytes = pcm_format_to_bits(format) / 8;

                num_read_buff_bytes =
                    adjust_channels(read_buff, num_device_channels,
                                    out_buff, num_req_channels,
                                    sample_size_in_bytes, num_read_buff_bytes);
            }
        }
    } else {
        num_read_buff_bytes = 0;
    }

    return num_read_buff_bytes;
}

static ssize_t mic_buffer_read(struct kara_manager *kara, void *buffer, size_t bytes)
{
    if (!kara || !buffer || kara->mic_buffer.size == 0 || !kara->karaoke_start) {
        return 0;
    }

    if (get_buffer_read_space(&kara->mic_buffer) >= (int)bytes) {
        ring_buffer_read(&kara->mic_buffer, buffer, bytes);
        return bytes;
    }

    return 0;
}

static int kara_open_micphone(struct kara_manager *kara, struct audioCfg *cfg)
{
    struct voice_in *in = NULL;
    struct pcm_config proxy_config;
    alsa_device_profile *profile = NULL;
    int ret = 0;

    ALOGI("++%s()", __func__);

    if (!cfg || !kara) {
        ALOGE("%s() NULL pointer, cfg %p, kara %p", __func__, cfg, kara);
        return -EINVAL;
    }

    pthread_mutex_lock(&kara->lock);
    if (kara->karaoke_start == true) {
        ALOGI("%s() karaoke is opened!", __func__);
        pthread_mutex_unlock(&kara->lock);
        return 0;
    }

    in = &kara->in;
    profile = in->in_profile;

    memset(&proxy_config, 0, sizeof(proxy_config));
    proxy_config.channels = profile_get_closest_channel_count(profile, cfg->channelCnt);

    if (profile_is_sample_rate_valid(profile, cfg->sampleRate)) {
        proxy_config.rate = cfg->sampleRate;
    } else {
        ALOGE("USB profile can't support rate: %d", cfg->sampleRate);
        proxy_config.rate = profile_get_default_sample_rate(profile);
    }

    proxy_config.format = PCM_FORMAT_S16_LE;
    proxy_config.period_size = USB_DEFAULT_PERIOD_SIZE;
    proxy_config.period_count = USB_DEFAULT_PERIOD_COUNT;

    in->cfg = *cfg;
    in->debug = 0;;
    ret = proxy_prepare(&in->proxy, profile, &proxy_config);
    if (ret < 0) {
        ALOGE("%s(), proxy prepare fail", __func__);
        goto err;
    }

    ALOGI("%s() open configs: channels %d format %d rate %d",
          __func__, proxy_config.channels, proxy_config.format, proxy_config.rate);

    ALOGV("%s() mixer port configs: channels %d, format %d, rate %d, frame_size %d",
          __func__, in->cfg.channelCnt, in->cfg.format,
          in->cfg.sampleRate, in->cfg.frame_size);

    ret = proxy_open(&in->proxy);
    if (ret < 0) {
        ALOGE("%s(), proxy open fail", __func__);
        goto err;
    }

    in->conversion_buffer = NULL;
    in->conversion_buffer_size = 0;

    kara->buf = NULL;
    kara->buf_len = 0;
    ring_buffer_init(&kara->mic_buffer, USB_DEFAULT_PERIOD_SIZE * 32);
    kara->karaoke_start = true;

    pthread_mutex_unlock(&kara->lock);
    ALOGV("--%s()", __func__);

    return 0;
err:
    pthread_mutex_unlock(&kara->lock);
    kara->karaoke_start = false;
    return ret;
}

static int kara_close_micphone(struct kara_manager *kara)
{
    struct voice_in *in = &kara->in;

    ALOGV("++%s()", __func__);
    pthread_mutex_lock(&kara->lock);
    if (kara->karaoke_start == false) {
        ALOGI("%s() karaoke is closed!", __func__);
        pthread_mutex_unlock(&kara->lock);
        return 0;
    }
    proxy_close(&in->proxy);
    free(in->conversion_buffer);
    in->conversion_buffer = NULL;
    in->conversion_buffer_size = 0;
    free(kara->buf);
    kara->buf = NULL;
    kara->buf_len = 0;
    ring_buffer_release(&kara->mic_buffer);
    kara->karaoke_start = false;
    pthread_mutex_unlock(&kara->lock);
    ALOGV("--%s()", __func__);

    return 0;
}

static int kara_mix_micphone(struct kara_manager *kara, void *buf, size_t bytes)
{
    struct voice_in *in = &kara->in;
    int frames = bytes / in->cfg.frame_size;
    int ret = 0;

    pthread_mutex_lock(&kara->lock);
    if (bytes > kara->buf_len) {
        kara->buf = realloc(kara->buf, bytes);
        kara->buf_len = bytes;
    }

    ret = voice_in_read(in, kara->buf, bytes);
    if (ret) {
        if (kara->kara_mic_mute) {
            memset(kara->buf, 0, bytes);
        } else {
#if 0
            if (kara->reverb_enable) {
                Set_AML_Reverb_Mode(kara->reverb_handle, kara->reverb_mode);
                AML_Reverb_Process(kara->reverb_handle, kara->buf, kara->buf, bytes >> 2);
            }
#endif
            apply_volume(kara->kara_mic_gain, kara->buf, 2, bytes);
        }
        /* mixer to output */
        do_mixing_2ch(buf, kara->buf, frames, in->cfg.format, in->cfg.format);
        if (kara->echo_reference != NULL) {
            struct echo_reference_buffer b;

            b.raw = (void *)kara->buf;
            b.frame_count = frames;
            clock_gettime(CLOCK_REALTIME, &b.time_stamp);
            b.delay_ns = 0;
            kara->echo_reference->write(kara->echo_reference, &b);

            if (in->debug) {
                aml_audio_dump_audio_bitstreams("/data/tmp/kara.raw", kara->buf, bytes);
            }
        }
    }
    pthread_mutex_unlock(&kara->lock);

    return ret;
}

int karaoke_init(struct kara_manager *karaoke, alsa_device_profile *profile)
{
    int ret;

    if (!karaoke || !profile) {
        return -EINVAL;
    }

    ALOGI("%s()", __func__);
    karaoke->open = kara_open_micphone;
    karaoke->read = mic_buffer_read;
    karaoke->close = kara_close_micphone;
    karaoke->mix = kara_mix_micphone;
    karaoke->in.in_profile = profile;
#if 0
    if (!karaoke->reverb_handle) {
        ret = AML_Reverb_Init(&karaoke->reverb_handle);
        if (ret < 0) {
            ALOGE("%s() int Reverb Error!", __func__);
            return -EINVAL;
        }
    }
#endif
    return 0;
}

static void add_echo_reference(struct kara_manager *kara,
                               struct echo_reference_itfe *reference)
{
    pthread_mutex_lock(&kara->lock);
    kara->echo_reference = reference;
    pthread_mutex_unlock(&kara->lock);
}

static void remove_echo_reference(struct kara_manager *kara,
                                  struct echo_reference_itfe *reference)
{
    pthread_mutex_lock(&kara->lock);
    if (kara->echo_reference == reference) {
        /* stop writing to echo reference */
        reference->write(reference, NULL);
        kara->echo_reference = NULL;
    }
    pthread_mutex_unlock(&kara->lock);
}

void put_echo_reference(struct kara_manager *kara,
                          struct echo_reference_itfe *reference)
{
    if (kara->echo_reference != NULL &&
            reference == kara->echo_reference) {
        remove_echo_reference(kara, reference);
        aml_release_echo_reference(reference);
        kara->echo_reference = NULL;
    }
}

struct echo_reference_itfe *get_echo_reference(struct kara_manager *kara,
        audio_format_t format,
        uint32_t channel_count,
        uint32_t sampling_rate)
{
    struct echo_reference_itfe *echo = NULL;

    put_echo_reference(kara, kara->echo_reference);
    if (kara->karaoke_start) {
        uint32_t wr_channel_count = 2;//proxy_get_channel_count(&kara->in.proxy);
        uint32_t wr_sampling_rate = 48000;//proxy_get_sample_rate(&kara->in.proxy);
        ALOGI("%s() rd channel %d, rate %d, wr channel %d rate %d",
            __func__, channel_count, sampling_rate,
            wr_channel_count, wr_sampling_rate);

        int status = aml_create_echo_reference(AUDIO_FORMAT_PCM_16_BIT,
                channel_count,
                sampling_rate,
                format,
                wr_channel_count,
                wr_sampling_rate,
                &echo);
        if (status == 0) {
            add_echo_reference(kara, echo);
            ALOGI("%s() sucess", __func__);
        }
    }

    return echo;
}

