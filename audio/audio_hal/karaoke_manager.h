/*
 * Copyright (C) 2018 Amlogic Corporation.
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


#ifndef _KARAOKE_MANAGER_H
#define _KARAOKE_MANAGER_H

#include <alsa_device_profile.h>
#include <alsa_device_proxy.h>
#include <pthread.h>
#include <aml_ringbuffer.h>
#include <aml_echo_reference.h>
#include <audio_data_process.h>

#include "sub_mixing_factory.h"

struct audioCfg;

struct voice_in {
    struct audioCfg cfg;
    bool debug;
    alsa_device_profile *in_profile;
    alsa_device_proxy proxy;
    void *conversion_buffer;
    size_t conversion_buffer_size;
};

struct kara_manager {
    pthread_mutex_t lock;
    bool karaoke_on;
    bool karaoke_enable;
    bool karaoke_start;
    bool kara_mic_mute;
    float kara_mic_gain;
    struct voice_in in;
    void *buf;
    size_t buf_len;
    ring_buffer_t mic_buffer;
    struct echo_reference_itfe *echo_reference;
    int (*open)(struct kara_manager *in, struct audioCfg *cfg);
    int (*close)(struct kara_manager *in);
    /* mixer audio data to output */
    int (*mix)(struct kara_manager *in, void *buffer, size_t bytes);
    /* read data from ringbuffer */
    ssize_t (*read)(struct kara_manager *in, void *buffer, size_t bytes);
    /* reverb for mic */
    void *reverb_handle;
    bool reverb_enable;
    int reverb_mode;
};

int karaoke_init(struct kara_manager *karaoke, alsa_device_profile *profile);
void put_echo_reference(struct kara_manager *kara,
                          struct echo_reference_itfe *reference);

struct echo_reference_itfe *get_echo_reference(struct kara_manager *kara,
        audio_format_t format,
        uint32_t channel_count,
        uint32_t sampling_rate);

#endif
