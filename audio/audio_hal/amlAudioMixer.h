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


#ifndef _AML_AUDIO_MIXER_H_
#define _AML_AUDIO_MIXER_H_

#include <tinyalsa/asoundlib.h>
#include "aml_ringbuffer.h"
#include "audio_port.h"
#include "karaoke_manager.h"

#define MIXER_OUT_FRAME_SIZE                (8)
#define MIXER_FRAME_COUNT                   (384)
#define MIXER_SAMPLE_RATE_HZ                (48000)
#define MIXER_WRITE_PERIOD_TIME_NANO        (MIXER_FRAME_COUNT * NSEC_PER_SEC / MIXER_SAMPLE_RATE_HZ)


__BEGIN_DECLS
/**
 * Audio mixer:
 * mixing two pcm streams with same configs(16-bits, 2-channels).
 * only support mixing two steams, one main stream and one aux stream.
 * And the output streams configs must be same as input streams.
 * TODO: add formats adaptions.
 */
struct amlAudioMixer;
typedef enum {
    MIXER_IDLE,             // no active tracks
    MIXER_INPORTS_ENABLED,  // at least one active track, but no track has any data ready
    MIXER_INPORTS_READY,    // at least one active track, and at least one track has data
    MIXER_DRAIN_TRACK,      // drain currently playing track
    MIXER_DRAIN_ALL,        // fully drain the hardware
} aml_mixer_state;


/**
 * constructor with mixer output pcm configs
 * return NULL if no enough memory.
 */
struct amlAudioMixer *newAmlAudioMixer(struct aml_audio_device *adev);

/**
 * distructor to free the mixer
 */
void freeAmlAudioMixer(struct amlAudioMixer *audio_mixer);
void mixer_set_hwsync_input_port(struct amlAudioMixer *audio_mixer, uint8_t port_index);
void set_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer, uint32_t frame_size);
uint32_t get_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer);
uint32_t get_mixer_inport_consumed_frames(struct amlAudioMixer *audio_mixer, uint8_t port_index);
int set_mixer_inport_volume(struct amlAudioMixer *audio_mixer, uint8_t port_index, float vol);
int init_mixer_input_port(struct amlAudioMixer *audio_mixer,
        struct audio_config *config,
        audio_output_flags_t flags,
        int (*on_notify_cbk)(void *data),
        void *notify_data,
        int (*on_input_avail_cbk)(void *data),
        void *input_avail_data,
        meta_data_cbk_t on_meta_data_cbk,
        void *meta_data,
        float volume);

uint32_t get_mixer_inport_count(struct amlAudioMixer *audio_mixer);
int delete_mixer_input_port(struct amlAudioMixer *audio_mixer, uint8_t port_index);
int send_mixer_inport_message(struct amlAudioMixer *audio_mixer, uint8_t port_index, PORT_MSG msg);
int mixer_write_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index, const void *buffer, int bytes);
int mixer_read_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index, void *buffer, int bytes);
int mixer_set_inport_state(struct amlAudioMixer *audio_mixer, uint8_t port_index, port_state state);
int mixer_flush_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index);
int pcm_mixer_thread_run(struct amlAudioMixer *audio_mixer);
int pcm_mixer_thread_exit(struct amlAudioMixer *audio_mixer);
struct pcm *pcm_mixer_get_pcm_handle(struct amlAudioMixer *audio_mixer);
uint32_t mixer_get_inport_latency_frames(struct amlAudioMixer *audio_mixer, uint8_t port_index);
uint32_t mixer_get_outport_latency_frames(struct amlAudioMixer *audio_mixer);
int mixer_get_presentation_position(
        struct amlAudioMixer *audio_mixer,
        uint8_t port_index,
        uint64_t *frames,
        struct timespec *timestamp);
int mixer_set_padding_size(
        struct amlAudioMixer *audio_mixer,
        uint8_t port_index,
        int padding_bytes);
int mixer_set_continuous_output(struct amlAudioMixer *audio_mixer, bool continuous_output);
int mixer_outport_pcm_restart(struct amlAudioMixer *audio_mixer);
void mixer_dump(int s32Fd, const struct aml_audio_device *pstAmlDev);
bool has_hwsync_stream_running(struct audio_stream_out *stream);
/* usb karaoke for hal mixer */
int mixer_set_karaoke(struct amlAudioMixer *audio_mixer, struct kara_manager *kara);

__END_DECLS

#endif
