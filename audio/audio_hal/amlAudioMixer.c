/*
 * Copyright (C) 2018 The Android Open Source Project
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

#define LOG_TAG "amlaudioMixer"
//#define LOG_NDEBUG 0
#define DEBUG_DUMP 0

#define __USE_GNU
#include <cutils/log.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>
#include <stdlib.h>
#include <system/audio.h>
#include <aml_volume_utils.h>
#include <inttypes.h>

#include "amlAudioMixer.h"
#include "audio_hw_utils.h"
#include "hw_avsync.h"
#include "audio_hwsync.h"
#include "audio_data_process.h"
#include "audio_virtual_buf.h"

#include "audio_hw.h"
#include "a2dp_hal.h"
#include "audio_bt_sco.h"
#include "aml_malloc_debug.h"


#define MIXER_WRITE_PERIOD_TIME_NANO        (MIXER_FRAME_COUNT * 1000000000LL / 48000)

#define SILENCE_FRAME_MAX                   (6144)

struct ring_buf_desc {
    struct ring_buffer *buffer;
    struct pcm_config cfg;
    int period_time;
    int valid;
};

enum mixer_state {
    MIXER_IDLE,             // no active tracks
    MIXER_INPORTS_ENABLED,  // at least one active track, but no track has any data ready
    MIXER_INPORTS_READY,    // at least one active track, and at least one track has data
    MIXER_DRAIN_TRACK,      // drain currently playing track
    MIXER_DRAIN_ALL,        // fully drain the hardware
};

enum {
    INPORT_NORMAL,   // inport not underrun
    INPORT_UNDERRUN, //inport doesn't have data, underrun may happen later
    INPORT_FEED_SILENCE_DONE, //underrun will happen quickly, we feed some silence data to avoid noise
};

//simple mixer support: 2 in , 1 out
struct amlAudioMixer {
    struct input_port *in_ports[NR_INPORTS];
    unsigned int inportsMasks; // records of inport IDs
    unsigned int supportedInportsMasks; // 1<< NR_EXTRA_INPORTS - 1
    struct output_port *out_ports[MIXER_OUTPUT_PORT_NUM];
    pthread_mutex_t inport_lock;
    ssize_t (*write)(struct amlAudioMixer *mixer, void *buffer, int bytes);
    //struct pcm_config mixer_cfg;
    //int period_time;
    void *tmp_buffer;       /* mixer temp output buffer. */
    size_t frame_size_tmp;
    uint32_t hwsync_frame_size;
    pthread_t out_mixer_tid;
    pthread_mutex_t lock;
    int exit_thread : 1;
    int mixing_enable : 1;
    enum mixer_state state;
    struct timespec tval_last_write;
    struct aml_audio_device *adev;
    bool continuous_output;
    //int init_ok : 1;
};

int mixer_set_state(struct amlAudioMixer *audio_mixer, enum mixer_state state)
{
    audio_mixer->state = state;
    return 0;
}

int mixer_set_continuous_output(struct amlAudioMixer *audio_mixer,
        bool continuous_output)
{
    audio_mixer->continuous_output = continuous_output;
    return 0;
}

bool mixer_is_continuous_enabled(struct amlAudioMixer *audio_mixer)
{
    return audio_mixer->continuous_output;
}

enum mixer_state mixer_get_state(struct amlAudioMixer *audio_mixer)
{
    return audio_mixer->state;
}

static unsigned int mixer_get_inport_index(struct amlAudioMixer *audio_mixer)
{
    unsigned int index = 0;

    index = (~audio_mixer->inportsMasks) & audio_mixer->supportedInportsMasks;
    index = __builtin_ctz(index);
    ALOGV("%s(), index %d", __func__, index);
    return index;
}

int init_mixer_input_port(struct amlAudioMixer *audio_mixer,
        struct audio_config *config,
        audio_output_flags_t flags,
        int (*on_notify_cbk)(void *data),
        void *notify_data,
        int (*on_input_avail_cbk)(void *data),
        void *input_avail_data,
        meta_data_cbk_t on_meta_data_cbk,
        void *meta_data,
        float volume)
{
    struct input_port *port = NULL;
    aml_mixer_input_port_type_e port_index;
    struct aml_stream_out *aml_out = notify_data;
    bool direct_on = false;

    if (audio_mixer == NULL || config == NULL || notify_data == NULL) {
        ALOGE("[%s:%d] NULL pointer", __func__, __LINE__);
        return -EINVAL;
    }

    /* if direct on, ie. the ALSA buffer is full, no need padding data anymore  */
    direct_on = (audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_DIRECT] != NULL);
    port = new_input_port(MIXER_FRAME_COUNT, config, flags, volume, direct_on);
    port_index = mixer_get_inport_index(audio_mixer);
    LOG_ALWAYS_FATAL_IF(port_index > NR_INPORTS);

    if (audio_mixer->in_ports[port_index] != NULL) {
        ALOGW("[%s:%d] inport index:%s already exists! recreate", __func__, __LINE__, mixerInputType2Str(port_index));
        free_input_port(audio_mixer->in_ports[port_index]);
    }

    port->ID = port_index;
    ALOGI("[%s:%d] input port:%s, size %d frames, frame_write_sum:%" PRId64 "", __func__, __LINE__,
        mixerInputType2Str(port->enInPortType), MIXER_FRAME_COUNT, aml_out->frame_write_sum);
    audio_mixer->in_ports[port_index] = port;
    audio_mixer->inportsMasks |= 1 << port_index;
    aml_out->inputPortID = port_index;

    set_port_notify_cbk(port, on_notify_cbk, notify_data);
    set_port_input_avail_cbk(port, on_input_avail_cbk, input_avail_data);
    if (on_meta_data_cbk && meta_data) {
        port->is_hwsync = true;
        set_port_meta_data_cbk(port, on_meta_data_cbk, meta_data);
    }
    port->initial_frames = aml_out->frame_write_sum;
    return 0;
}

int delete_mixer_input_port(struct amlAudioMixer *audio_mixer,
        unsigned int port_index)
{
    ALOGI("[%s:%d] input port:%d", __func__, __LINE__, port_index);
    if (audio_mixer->in_ports[port_index]) {
        free_input_port(audio_mixer->in_ports[port_index]);
        audio_mixer->in_ports[port_index] = NULL;
        audio_mixer->inportsMasks &= ~(1 << port_index);
    }
    return 0;
}

int send_mixer_inport_message(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index , enum PORT_MSG msg)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    if (port == NULL) {
        ALOGE("%s(), port index %d, inval", __func__, port_index);
        return -EINVAL;
    }

    return send_inport_message(port, msg);
}

void set_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer,
        uint32_t frame_size)
{
    aml_mixer_input_port_type_e port_index = AML_MIXER_INPUT_PORT_PCM_SYSTEM;
    enum MIXER_OUTPUT_PORT out_port_index = MIXER_OUTPUT_PORT_PCM;
    struct input_port *in_port = NULL;
    struct output_port *out_port = audio_mixer->out_ports[out_port_index];
    int port_cnt = 0;
    for (; port_index < AML_MIXER_INPUT_PORT_BUTT; port_index++) {
        struct input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port) {
            //resize_input_port_buffer(in_port, MIXER_IN_BUFFER_SIZE);
        }
    }

    //resize_output_port_buffer(out_port, MIXER_IN_BUFFER_SIZE);
    ALOGI("%s framesize %d", __func__, frame_size);
    audio_mixer->hwsync_frame_size = frame_size;
}

uint32_t get_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer)
{
    return audio_mixer->hwsync_frame_size;
}

uint32_t get_mixer_inport_consumed_frames(
        struct amlAudioMixer *audio_mixer, aml_mixer_input_port_type_e port_index)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }
    return get_inport_consumed_size(port) / port->cfg.frame_size;
}

int set_mixer_inport_volume(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index, float vol)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    if (vol > 1.0 || vol < 0) {
        ALOGE("%s(), invalid vol %f", __func__, vol);
        return -EINVAL;
    }
    set_inport_volume(port, vol);
    return 0;
}

float get_mixer_inport_volume(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return 0;
    }
    return get_inport_volume(port);
}

int mixer_write_inport(struct amlAudioMixer *audio_mixer,
        unsigned int port_index, const void *buffer, int bytes)
{
    struct input_port *port = audio_mixer->in_ports[port_index];
    int             written = 0;
    int64_t         cur_time_ns = 0;
    struct timespec cur_timestamp;

    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    clock_gettime(CLOCK_MONOTONIC, &cur_timestamp);
    cur_time_ns = (long long)cur_timestamp.tv_sec * 1000000000 + (long long)cur_timestamp.tv_nsec;
    if (cur_time_ns - port->last_write_time_ns > port->buffer_len_ns) {
        ALOGI("[%s:%d] input port:%s start write to input buffer", __func__, __LINE__, mixerInputType2Str(port->enInPortType));
        port->first_write = true;
    }
    port->last_write_time_ns = cur_time_ns;

    written = port->write(port, buffer, bytes);
    if (get_inport_state(port) != ACTIVE) {
        ALOGI("[%s:%d] input port:%s is active now", __func__, __LINE__, mixerInputType2Str(port->enInPortType));
        set_inport_state(port, ACTIVE);
    }
    ALOGV("%s(), signal line %d portIndex %d", __func__, __LINE__, port_index);
    return written;
}

int mixer_read_inport(struct amlAudioMixer *audio_mixer,
        unsigned int port_index, void *buffer, int bytes)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    return port->read(port, buffer, bytes);
}

int mixer_set_inport_state(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index, enum port_state state)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    return set_inport_state(port, state);
}

enum port_state mixer_get_inport_state(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    return get_inport_state(port);
}
//TODO: handle message queue
static void mixer_procs_msg_queue(struct amlAudioMixer *audio_mixer __unused)
{
    ALOGV("++%s start", __func__);
    return;
}

static struct output_port *get_outport(struct amlAudioMixer *audio_mixer,
        enum MIXER_OUTPUT_PORT port_index)
{
    return audio_mixer->out_ports[port_index];
}

size_t get_outport_data_avail(struct output_port *outport)
{
    return outport->bytes_avail;
}

int set_outport_data_avail(struct output_port *outport, size_t avail)
{
    if (avail > outport->data_buf_len) {
        ALOGE("%s(), invalid avail %zu", __func__, avail);
        return -EINVAL;
    }
    outport->bytes_avail = avail;
    return 0;
}

static bool is_output_data_avail(struct amlAudioMixer *audio_mixer,
        enum MIXER_OUTPUT_PORT port_index)
{
    struct output_port *outport = NULL;

    ALOGV("++%s start", __func__);
    /* init check */
    //if(amlAudioMixer_check_status(audio_mixer))
    //    return false;

    outport = get_outport(audio_mixer, port_index);
    return !!get_outport_data_avail(outport);
    //return true;
}

int init_mixer_output_port(struct amlAudioMixer *audio_mixer,
        struct pcm *pcm_handle,
        struct audioCfg cfg,
        size_t buf_frames
        //,struct audio_config *config,
        //audio_output_flags_t flags,
        //int (*on_notify_cbk)(void *data),
        //void *notify_data,
        //int (*on_input_avail_cbk)(void *data),
        ///void *input_avail_data
        )
{
    struct output_port *port = new_output_port(MIXER_OUTPUT_PORT_PCM,
            pcm_handle, cfg, buf_frames);
    enum MIXER_OUTPUT_PORT port_index = port->enOutPortType;
#ifdef ENABLE_AEC_APP
    port->aec = audio_mixer->adev->aec;
#endif
    //set_port_notify_cbk(port, on_notify_cbk, notify_data);
    //set_port_input_avail_cbk(port, on_input_avail_cbk, input_avail_data);
    audio_mixer->out_ports[port_index] = port;
    return 0;
}

int delete_mixer_output_port(struct amlAudioMixer *audio_mixer,
        enum MIXER_OUTPUT_PORT port_index)
{
    free_output_port(audio_mixer->out_ports[port_index]);
    audio_mixer->out_ports[port_index] = NULL;
    return 0;
}

static int mixer_output_write(struct amlAudioMixer *audio_mixer)
{
    enum MIXER_OUTPUT_PORT port_index = 0;
    audio_config_base_t in_data_config = {48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT};
    struct output_port *out_port = audio_mixer->out_ports[port_index];
    struct aml_audio_device *adev = audio_mixer->adev;
    int count = 3;

    out_port->sound_track_mode = audio_mixer->adev->sound_track_mode;
    while (is_output_data_avail(audio_mixer, port_index)) {
        // out_write_callbacks();
        if (adev->active_outport == OUTPORT_A2DP) {
            int ret = a2dp_out_write(adev, &in_data_config, out_port->data_buf, out_port->bytes_avail);
            if (ret == 0 && count-- > 0) {
                 continue;
            }
        } else if (is_sco_port(adev->active_outport)) {
            write_to_sco(adev, &in_data_config, out_port->data_buf, out_port->bytes_avail);
        } else {
            out_port->write(out_port, out_port->data_buf, out_port->bytes_avail);
        }
        set_outport_data_avail(out_port, 0);
    };
    return 0;
}

#define DEFAULT_KERNEL_FRAMES (DEFAULT_PLAYBACK_PERIOD_SIZE*DEFAULT_PLAYBACK_PERIOD_CNT)

static int mixer_update_tstamp(struct amlAudioMixer *audio_mixer)
{
    struct output_port *out_port = audio_mixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    struct input_port *in_port = NULL;
    unsigned int avail;
    unsigned int masks = audio_mixer->inportsMasks;
    //struct timespec *timestamp;

    while (masks) {
        int i = 31 - __builtin_clz(masks);
        masks &= ~(1 << i);
        if (NULL == audio_mixer->in_ports[i]) {
            continue;
        }
        if (audio_mixer->in_ports[i]->enInPortType == AML_MIXER_INPUT_PORT_PCM_SYSTEM) {
            in_port = audio_mixer->in_ports[i];
            break;
        }
    }

    /*only deal with system audio */
    if (in_port == NULL || out_port == NULL)
        return 0;
    if (out_port->pcm_handle == NULL)
        return 0;

    if (pcm_get_htimestamp(out_port->pcm_handle, &avail, &in_port->timestamp) == 0) {
        size_t kernel_buf_size = DEFAULT_KERNEL_FRAMES;
        int64_t signed_frames = in_port->mix_consumed_frames - kernel_buf_size + avail;
        if (signed_frames < 0) {
            signed_frames = 0;
        }
        in_port->presentation_frames = in_port->initial_frames + signed_frames;
        ALOGV("%s() present frames:%" PRId64 ", initial %" PRId64 ", consumed %" PRId64 ", sec:%ld, nanosec:%ld",
                __func__,
                in_port->presentation_frames,
                in_port->initial_frames,
                in_port->mix_consumed_frames,
                in_port->timestamp.tv_sec,
                in_port->timestamp.tv_nsec);
    }

    return 0;
}

static bool is_mixer_inports_ready(struct amlAudioMixer *audio_mixer)
{
    aml_mixer_input_port_type_e port_index = 0;
    int port_cnt = 0, ready = 0;
    for (port_index = 0; port_index < AML_MIXER_INPUT_PORT_BUTT; port_index++) {
        struct input_port *in_port = audio_mixer->in_ports[port_index];
        ALOGV("%s() port index %d, port ptr %p", __func__, port_index, in_port);
        if (in_port) {
            port_cnt++;
            if (in_port->rbuf_avail(in_port) >= in_port->data_len_bytes) {
                ALOGV("port %d data ready", port_index);
                ready++;
            } else {
                ALOGV("port %d data not ready", port_index);
            }
        }
    }

    return (port_cnt == ready);
}

inline float get_fade_step_by_size(int fade_size, int frame_size)
{
    return 1.0/(fade_size/frame_size);
}

int init_fade(struct fade_out *fade_out, int fade_size,
        int sample_size, int channel_cnt)
{
    fade_out->vol = 1.0;
    fade_out->target_vol = 0;
    fade_out->fade_size = fade_size;
    fade_out->sample_size = sample_size;
    fade_out->channel_cnt = channel_cnt;
    fade_out->stride = get_fade_step_by_size(fade_size, sample_size * channel_cnt);
    ALOGI("%s() size %d, stride %f", __func__, fade_size, fade_out->stride);
    return 0;
}

int process_fade_out(void *buf, int bytes, struct fade_out *fout)
{
    int i = 0;
    int frame_cnt = bytes / fout->sample_size / fout->channel_cnt;
    int16_t *sample = (int16_t *)buf;

    if (fout->channel_cnt != 2 || fout->sample_size != 2)
        ALOGE("%s(), not support yet", __func__);
    ALOGI("++++fade out vol %f, size %d", fout->vol, fout->fade_size);
    for (i = 0; i < frame_cnt; i++) {
        sample[i] = sample[i]*fout->vol;
        sample[i+1] = sample[i+1]*fout->vol;
        fout->vol -= fout->stride;
        if (fout->vol < 0)
            fout->vol = 0;
    }
    fout->fade_size -= bytes;
    ALOGI("----fade out vol %f, size %d", fout->vol, fout->fade_size);

    return 0;
}

static int update_inport_avail(struct input_port *in_port)
{
    // first throw away the padding frames
    if (in_port->padding_frames > 0) {
        in_port->padding_frames -= in_port->data_buf_frame_cnt;
        set_inport_pts_valid(in_port, false);
    } else {
        in_port->mix_consumed_frames += in_port->data_buf_frame_cnt;
        set_inport_pts_valid(in_port, true);
    }
    in_port->data_valid = 1;
    return 0;
}

static void process_port_msg(struct input_port *port)
{
    struct port_message *msg = get_inport_message(port);
    if (msg) {
        ALOGI("%s(), msg: %s", __func__, port_msg_to_str(msg->msg_what));
        switch (msg->msg_what) {
        case MSG_PAUSE: {
            struct aml_stream_out *out = (struct aml_stream_out *)port->notify_cbk_data;
            audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
            //ALOGI("[%s:%d] hwsync:%p tsync pause", __func__, __LINE__, hwsync);
            if ((hwsync != NULL) && (hwsync->use_mediasync)) {
                aml_hwsync_set_tsync_pause(hwsync);
            }
            set_inport_state(port, PAUSING);
            break;
        }
        case MSG_FLUSH:
            set_inport_state(port, FLUSHING);
            break;
        case MSG_RESUME: {
            struct aml_stream_out *out = (struct aml_stream_out *)port->notify_cbk_data;
            audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
            //ALOGI("[%s:%d] hwsync:%p tsync resume", __func__, __LINE__, hwsync);
            if ((hwsync != NULL) && (hwsync->use_mediasync)) {
                aml_hwsync_set_tsync_resume(hwsync);
            }
            set_inport_state(port, RESUMING);
            break;
        }
        default:
            ALOGE("%s(), not support", __func__);
        }

        remove_inport_message(port, msg);
    }
}

int mixer_flush_inport(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index)
{
    struct input_port *in_port = audio_mixer->in_ports[port_index];

    if (!in_port) {
        return -EINVAL;
    }

    return reset_input_port(in_port);
}

static int mixer_inports_read(struct amlAudioMixer *audio_mixer)
{
    unsigned int port_index = 0;
    unsigned int masks = audio_mixer->inportsMasks;

    ALOGV("++%s(), line %d", __func__, __LINE__);
    while (masks) {
        struct input_port *in_port;
        int ID = 31 - __builtin_clz(masks);

        masks &= ~(1 << ID);
        in_port = audio_mixer->in_ports[ID];

        if (in_port) {
            int ret = 0, fade_out = 0, fade_in = 0;
            aml_mixer_input_port_type_e type = in_port->enInPortType;

            process_port_msg(in_port);
            enum port_state state = get_inport_state(in_port);

            if (type == AML_MIXER_INPUT_PORT_PCM_DIRECT) {
                //if in pausing states, don't retrieve data
                if (state == PAUSING) {
                    fade_out = 1;
                } else if (state == RESUMING) {
                    struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
                    audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
                    fade_in = 1;
                    ALOGI("[%s:%d] input port:%s tsync resume", __func__, __LINE__, mixerInputType2Str(type));
                    aml_hwsync_set_tsync_resume(hwsync);
                    set_inport_state(in_port, ACTIVE);
                    in_port->first_write = true;
                } else if (state == STOPPED || state == PAUSED || state == FLUSHED) {
                    ALOGV("[%s:%d] input port:%s stopped, paused or flushed", __func__, __LINE__, mixerInputType2Str(type));
                    continue;
                } else if (state == FLUSHING) {
                    mixer_flush_inport(audio_mixer, ID);
                    ALOGI("[%s:%d] input port:%s flushing->flushed", __func__, __LINE__, mixerInputType2Str(type));
                    set_inport_state(in_port, FLUSHED);
                    continue;
                }
                if (get_inport_state(in_port) == ACTIVE && in_port->data_valid) {
                    ALOGI("[%s:%d] input port:%s data already valid", __func__, __LINE__, mixerInputType2Str(type));
                    continue;
                }
            } else {
                if (in_port->data_valid) {
                    ALOGI("[%s:%d] input port ID:%d data already valid", __func__, __LINE__, ID);
                    continue;
                }
            }

            int input_avail_size = in_port->rbuf_avail(in_port);
            ALOGV("[%s:%d] input port:%s, portId:%d, avail:%d, masks:%#x, inportsMasks:%#x, data_len_bytes:%zu", __func__, __LINE__,
                mixerInputType2Str(type), ID, input_avail_size, masks, audio_mixer->inportsMasks, in_port->data_len_bytes);
            if (input_avail_size >= in_port->data_len_bytes) {
                if (in_port->first_write == true) {
                    if (input_avail_size < in_port->inport_start_threshold) {
                        //ALOGI("[%s:%d] input port:%s waiting to reach inport_start_threshold, portId:%d, avail:%d, inport_start_threshold:%d", __func__, __LINE__,
                        //               inportType2Str(type), ID, input_avail_size, in_port->inport_start_threshold);
                        continue;
                    } else {
                        ALOGI("[%s:%d] input port:%s first start, portId:%d, avail:%d", __func__, __LINE__, mixerInputType2Str(type), ID, input_avail_size);
                        in_port->first_write = false;
                    }
                }
                ret = mixer_read_inport(audio_mixer, ID, in_port->data, in_port->data_len_bytes);
                if (ret == (int)in_port->data_len_bytes) {
                    if (fade_out) {
                        struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
                        audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;

                        ALOGI("[%s:%d] output port:%s fade out, pausing->pausing_1, tsync pause audio",
                            __func__, __LINE__, mixerInputType2Str(type));
                        if (audio_mixer->adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
                            memset(in_port->r_buf->start_addr, 0, in_port->r_buf->size);
                        aml_hwsync_set_tsync_pause(hwsync);
                        audio_fade_func(in_port->data, ret, 0);
                        set_inport_state(in_port, PAUSED);
                    } else if (fade_in) {
                        ALOGI("[%s:%d] input port:%s fade in", __func__, __LINE__, mixerInputType2Str(type));
                        audio_fade_func(in_port->data, ret, 1);
                        set_inport_state(in_port, ACTIVE);
                    }
                    update_inport_avail(in_port);
                    if (getprop_bool("vendor.media.audiohal.inport") &&
                            (in_port->enInPortType == AML_MIXER_INPUT_PORT_PCM_DIRECT)) {
                            aml_audio_dump_audio_bitstreams("/data/audio/inportDirectFade.raw",
                                    in_port->data, in_port->data_len_bytes);
                    }
                } else {
                    ALOGW("[%s:%d] port:%s read fail, have read:%d Byte, need %zu Byte", __func__, __LINE__,
                        mixerInputType2Str(type), ret, in_port->data_len_bytes);
                }
            } else {
                struct aml_audio_device     *adev = audio_mixer->adev;
                if (adev->debug_flag) {
                    ALOGD("[%s:%d] port:%d ring buffer data is not enough", __func__, __LINE__, ID);
                }
            }
        }
    }

    return 0;
}

int check_mixer_state(struct amlAudioMixer *audio_mixer)
{
    aml_mixer_input_port_type_e port_index = 0;
    int inport_avail = 0, inport_ready = 0;

    ALOGV("++%s(), line %d", __func__, __LINE__);
    for (port_index = 0; port_index < AML_MIXER_INPUT_PORT_BUTT; port_index++) {
        struct input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port) {
            inport_avail = 1;

            // only when one or more inport is active, mixer is ready
            if (get_inport_state(in_port) == ACTIVE || get_inport_state(in_port) == PAUSING
                    || get_inport_state(in_port) == PAUSING_1)
                inport_ready = 1;
        }
    }

    if (inport_ready)
        audio_mixer->state = MIXER_INPORTS_READY;
    else if (inport_avail)
        audio_mixer->state = MIXER_INPORTS_ENABLED;
    else
        audio_mixer->state = MIXER_IDLE;

    return 0;
}

int mixer_need_wait_forever(struct amlAudioMixer *audio_mixer)
{
    return mixer_get_state(audio_mixer) != MIXER_INPORTS_READY;
}

static inline int16_t CLIP16(int r)
{
    return (r >  0x7fff) ? 0x7fff :
           (r < -0x8000) ? 0x8000 :
           r;
}

static uint32_t hwsync_align_to_frame(uint32_t consumed_size, uint32_t frame_size)
{
    return consumed_size - (consumed_size % frame_size);
}

static int retrieve_hwsync_header(struct amlAudioMixer *audio_mixer,
        struct input_port *in_port, struct output_port *out_port)
{
    uint32_t frame_size = get_mixer_hwsync_frame_size(audio_mixer);
    uint32_t port_consumed_size = get_inport_consumed_size(in_port);
    int diff_ms = 0;
    struct hw_avsync_header header;
    int ret = 0;

    if (frame_size == 0) {
        ALOGV("%s(), invalid frame size 0", __func__);
        return -EINVAL;
    }

    if (!in_port->is_hwsync) {
        ALOGE("%s(), not hwsync port", __func__);
        return -EINVAL;
    }

    memset(&header, 0, sizeof(struct hw_avsync_header));
    ALOGV("direct out port bytes before cbk %zu", get_outport_data_avail(out_port));
    if (!in_port->meta_data_cbk) {
        ALOGE("no meta_data_cbk set!!");
        return -EINVAL;
    }
    ALOGV("%s(), port %p, data %p", __func__, in_port, in_port->meta_data_cbk_data);
    ret = in_port->meta_data_cbk(in_port->meta_data_cbk_data,
                port_consumed_size, &header, &diff_ms);
    if (ret < 0) {
        if (ret != -EAGAIN)
            ALOGE("meta_data_cbk fail err = %d!!", ret);
        return ret;
    }
    ALOGV("%s(), meta data cbk, diffms = %d", __func__, diff_ms);
    if (diff_ms > 0) {
        in_port->bytes_to_insert = diff_ms * 48 * 4;
    } else if (diff_ms < 0) {
        in_port->bytes_to_skip = -diff_ms * 48 * 4;
    }

    return 0;
}


static int mixer_do_mixing_32bit(struct amlAudioMixer *audio_mixer)
{
    struct input_port *in_port_sys = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_SYSTEM];
    struct input_port *in_port_drct = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_DIRECT];
    struct output_port *out_port = audio_mixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    struct aml_audio_device *adev = audio_mixer->adev;
    int16_t *data_sys, *data_drct, *data_mixed;
    int mixing = 0, sys_only = 0, direct_only = 0;
    int dirct_okay = 0, sys_okay = 0;
    float dirct_vol = 1.0, sys_vol = 1.0;
    int mixed_32 = 0;
    size_t i = 0, mixing_len_bytes = 0;
    size_t frames = 0;
    size_t frames_written = 0;
    float gain_speaker = adev->sink_gain[OUTPORT_SPEAKER];

    if (!out_port) {
        ALOGE("%s(), out null !!!", __func__);
        return 0;
    }
    if (!in_port_sys && !in_port_drct) {
        ALOGE("%s(), sys or direct pcm must exist!!!", __func__);
        return 0;
    }

    if (in_port_sys && in_port_sys->data_valid) {
        sys_okay = 1;
    }
    if (in_port_drct && in_port_drct->data_valid) {
        dirct_okay = 1;
    }
    if (sys_okay && dirct_okay) {
        mixing = 1;
    } else if (dirct_okay) {
        ALOGV("only direct okay");
        direct_only = 1;
    } else if (sys_okay) {
        sys_only = 1;
    } else {
        ALOGV("%s(), sys direct both not ready!", __func__);
        return -EINVAL;
    }

    data_mixed = (int16_t *)out_port->data_buf;
    memset(audio_mixer->tmp_buffer, 0 , MIXER_FRAME_COUNT * MIXER_OUT_FRAME_SIZE);
    if (mixing) {
        ALOGV("%s() mixing", __func__);
        data_sys = (int16_t *)in_port_sys->data;
        data_drct = (int16_t *)in_port_drct->data;
        mixing_len_bytes = in_port_drct->data_len_bytes;
        //TODO: check if the two stream's frames are equal
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/audiodrct.raw",
                    in_port_drct->data, in_port_drct->data_len_bytes);
            aml_audio_dump_audio_bitstreams("/data/audio/audiosyst.raw",
                    in_port_sys->data, in_port_sys->data_len_bytes);
        }
        if (in_port_drct->is_hwsync && in_port_drct->bytes_to_insert < mixing_len_bytes) {
            retrieve_hwsync_header(audio_mixer, in_port_drct, out_port);
        }

        // insert data for direct hwsync case, only send system sound
        if (in_port_drct->bytes_to_insert >= mixing_len_bytes) {
            frames = mixing_len_bytes / in_port_drct->cfg.frame_size;
            ALOGD("%s() insert mixing data, need %zu, insert length %zu",
                    __func__, in_port_drct->bytes_to_insert, mixing_len_bytes);
            //memcpy(data_mixed, data_sys, mixing_len_bytes);
            //memcpy(audio_mixer->tmp_buffer, data_sys, mixing_len_bytes);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/systbeforemix.raw",
                        data_sys, in_port_sys->data_len_bytes);
            }
            frames_written = do_mixing_2ch(audio_mixer->tmp_buffer, data_sys,
                frames, in_port_sys->cfg, out_port->cfg);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/sysAftermix.raw",
                        audio_mixer->tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
            }
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->tmp_buffer,
                    frames, 2, 8);

            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/dataInsertMixed.raw",
                        data_mixed, frames * out_port->cfg.frame_size);
            }
            in_port_drct->bytes_to_insert -= mixing_len_bytes;
            in_port_sys->data_valid = 0;
            set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
        } else {
            frames = mixing_len_bytes / in_port_drct->cfg.frame_size;
            frames_written = do_mixing_2ch(audio_mixer->tmp_buffer, data_drct,
                frames, in_port_drct->cfg, out_port->cfg);
            if (DEBUG_DUMP)
                aml_audio_dump_audio_bitstreams("/data/audio/tmpMixed0.raw",
                    audio_mixer->tmp_buffer, frames * audio_mixer->frame_size_tmp);
            frames_written = do_mixing_2ch(audio_mixer->tmp_buffer, data_sys,
                frames, in_port_sys->cfg, out_port->cfg);
            if (DEBUG_DUMP)
                aml_audio_dump_audio_bitstreams("/data/audio/tmpMixed1.raw",
                    audio_mixer->tmp_buffer, frames * audio_mixer->frame_size_tmp);
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->tmp_buffer,
                    frames, 2, 8);

            in_port_drct->data_valid = 0;
            in_port_sys->data_valid = 0;
            set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
        }
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/data_mixed.raw",
                out_port->data_buf, frames * out_port->cfg.frame_size);
        }
    }

    if (sys_only) {
        frames = in_port_sys->data_buf_frame_cnt;
        ALOGV("%s() sys_only, frames %zu", __func__, frames);
        mixing_len_bytes = in_port_sys->data_len_bytes;
        data_sys = (int16_t *)in_port_sys->data;
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/audiosyst.raw",
                    in_port_sys->data, mixing_len_bytes);
        }
        // processing data and make convertion according to cfg
        // processing_and_convert(data_mixed, data_sys, frames, in_port_sys->cfg, out_port->cfg);
        frames_written = do_mixing_2ch(audio_mixer->tmp_buffer, data_sys,
                frames, in_port_sys->cfg, out_port->cfg);
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/sysTmp.raw",
                    audio_mixer->tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
        }
        if (adev->is_TV) {
            apply_volume(gain_speaker, audio_mixer->tmp_buffer,
                sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
        }
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/sysvol.raw",
                    audio_mixer->tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
        }

        extend_channel_2_8(data_mixed, audio_mixer->tmp_buffer, frames, 2, 8);

        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/extandsys.raw",
                    data_mixed, frames * out_port->cfg.frame_size);
        }
        in_port_sys->data_valid = 0;
        set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
    }

    if (direct_only) {
        ALOGV("%s() direct_only", __func__);
        //dirct_vol = get_inport_volume(in_port_drct);
        mixing_len_bytes = in_port_drct->data_len_bytes;
        data_drct = (int16_t *)in_port_drct->data;
        ALOGV("%s() direct_only, inport consumed %zu",
                __func__, get_inport_consumed_size(in_port_drct));

        if (in_port_drct->is_hwsync && in_port_drct->bytes_to_insert < mixing_len_bytes) {
            retrieve_hwsync_header(audio_mixer, in_port_drct, out_port);
        }

        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/audiodrct.raw",
                    in_port_drct->data, mixing_len_bytes);
        }
        // insert 0 data to delay audio
        if (in_port_drct->bytes_to_insert >= mixing_len_bytes) {
            frames = mixing_len_bytes / in_port_drct->cfg.frame_size;
            ALOGD("%s() inserting direct_only, need %zu, insert length %zu",
                    __func__, in_port_drct->bytes_to_insert, mixing_len_bytes);
            memset(data_mixed, 0, mixing_len_bytes);
            extend_channel_2_8(data_mixed, audio_mixer->tmp_buffer,
                    frames, 2, 8);
            in_port_drct->bytes_to_insert -= mixing_len_bytes;
            set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
        } else {
            ALOGV("%s() direct_only, vol %f", __func__, dirct_vol);
            frames = mixing_len_bytes / in_port_drct->cfg.frame_size;
            //cpy_16bit_data_with_gain(data_mixed, data_drct,
            //        in_port_drct->data_len_bytes, dirct_vol);
            ALOGV("%s() direct_only, frames %zu, bytes %zu", __func__, frames, mixing_len_bytes);

            frames_written = do_mixing_2ch(audio_mixer->tmp_buffer, data_drct,
                frames, in_port_drct->cfg, out_port->cfg);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/dirctTmp.raw",
                        audio_mixer->tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
            }
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->tmp_buffer,
                    frames, 2, 8);

            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/exDrct.raw",
                        data_mixed, frames * out_port->cfg.frame_size);
            }
            in_port_drct->data_valid = 0;
            set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
        }
    }

    if (0) {
        aml_audio_dump_audio_bitstreams("/data/audio/data_mixed.raw",
                out_port->data_buf, mixing_len_bytes);
    }
    return 0;
}

static int mixer_add_mixing_data(void *pMixedBuf, struct input_port *pInputPort, struct output_port *pOutputPort)
{
    if (pInputPort->data_buf_frame_cnt < MIXER_FRAME_COUNT) {
        ALOGE("[%s:%d] input port type:%s buf frames:%zu too small", __func__, __LINE__,
            mixerInputType2Str(pInputPort->enInPortType), pInputPort->data_buf_frame_cnt);
        return -EINVAL;
    }
    do_mixing_2ch(pMixedBuf, pInputPort->data, MIXER_FRAME_COUNT, pInputPort->cfg, pOutputPort->cfg);
    pInputPort->data_valid = 0;
    return 0;
}

static int mixer_do_mixing_16bit(struct amlAudioMixer *audio_mixer)
{
    bool is_data_valid = false;
    struct input_port           *pstInputPort = NULL;
    struct output_port          *pstOutPort = audio_mixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    struct aml_audio_device     *adev = audio_mixer->adev;
    unsigned int masks = audio_mixer->inportsMasks;

    if (NULL == pstOutPort) {
        ALOGE("[%s:%d] outport is null", __func__, __LINE__);
        return 0;
    }

    memset(audio_mixer->tmp_buffer, 0, MIXER_FRAME_COUNT * MIXER_OUT_FRAME_SIZE);
    while (masks) {
        struct input_port *in_port;
        int i = 31 - __builtin_clz(masks);

        masks &= ~(1 << i);
        pstInputPort = audio_mixer->in_ports[i];
        if (NULL == pstInputPort) {
            continue;
        }
        if (0 == pstInputPort->data_valid) {
            if (adev->debug_flag) {
                ALOGI("[%s:%d] inport:%s, but no valid data, maybe underrun", __func__, __LINE__,
                    mixerInputType2Str(pstInputPort->enInPortType));
            }
            continue;
        }
        is_data_valid = true;
        if (getprop_bool("vendor.media.audiohal.indump")) {
            char acFilePathStr[ENUM_TYPE_STR_MAX_LEN];
            sprintf(acFilePathStr, "/data/audio/%s", mixerInputType2Str(pstInputPort->enInPortType));
            aml_audio_dump_audio_bitstreams(acFilePathStr, pstInputPort->data, pstInputPort->data_len_bytes);
        }
        if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
            check_audio_level(mixerInputType2Str(pstInputPort->enInPortType), pstInputPort->data, pstInputPort->data_len_bytes);
        }
        if (AML_MIXER_INPUT_PORT_PCM_DIRECT == pstInputPort->enInPortType) {
            if (pstInputPort->is_hwsync && pstInputPort->bytes_to_insert < pstInputPort->data_len_bytes) {
                retrieve_hwsync_header(audio_mixer, pstInputPort, pstOutPort);
            }
            if (pstInputPort->bytes_to_insert >= pstInputPort->data_len_bytes) {
                pstInputPort->bytes_to_insert -= pstInputPort->data_len_bytes;
                ALOGD("[%s:%d] PCM_DIRECT inport insert mute data, still need %zu, inserted length %zu", __func__, __LINE__,
                        pstInputPort->bytes_to_insert, pstInputPort->data_len_bytes);
                continue;
            }
        }
        mixer_add_mixing_data(audio_mixer->tmp_buffer, pstInputPort, pstOutPort);
    }
    /* only check the valid on a2dp case, normal alsa output we need continuous output,
     * otherwise it will cause noise at the end
     */
    if (!is_data_valid && (adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)) {
        if (adev->debug_flag) {
            ALOGI("[%s:%d] inport no valid data", __func__, __LINE__);
        }
        return -1;
    }

    if (adev->is_TV) {
        apply_volume(adev->sink_gain[OUTPORT_SPEAKER], audio_mixer->tmp_buffer, sizeof(uint16_t),
            MIXER_FRAME_COUNT * pstOutPort->cfg.frame_size);
    }
    memcpy(pstOutPort->data_buf, audio_mixer->tmp_buffer, MIXER_FRAME_COUNT * pstOutPort->cfg.frame_size);
    if (getprop_bool("vendor.media.audiohal.outdump")) {
        aml_audio_dump_audio_bitstreams("/data/audio/audio_mixed", pstOutPort->data_buf,
            MIXER_FRAME_COUNT * pstOutPort->cfg.frame_size);
    }
    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("audio_mixed", pstOutPort->data_buf, MIXER_FRAME_COUNT * pstOutPort->cfg.frame_size);
    }
    set_outport_data_avail(pstOutPort, MIXER_FRAME_COUNT * pstOutPort->cfg.frame_size);
    return 0;
}

int notify_mixer_input_avail(struct amlAudioMixer *audio_mixer)
{
    aml_mixer_input_port_type_e port_index = 0;
    for (port_index = 0; port_index < AML_MIXER_INPUT_PORT_BUTT; port_index++) {
        struct input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port && in_port->on_input_avail_cbk)
            in_port->on_input_avail_cbk(in_port->input_avail_cbk_data);
    }

    return 0;
}

int notify_mixer_exit(struct amlAudioMixer *audio_mixer)
{
    aml_mixer_input_port_type_e port_index = 0;
    for (port_index = 0; port_index < AML_MIXER_INPUT_PORT_BUTT; port_index++) {
        struct input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port && in_port->on_notify_cbk)
            in_port->on_notify_cbk(in_port->notify_cbk_data);
    }

    return 0;
}

static int set_thread_affinity(void)
{
    cpu_set_t cpuSet;
    int sastat = 0;

    CPU_ZERO(&cpuSet);
    CPU_SET(2, &cpuSet);
    CPU_SET(3, &cpuSet);
    sastat = sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet);
    if (sastat) {
        ALOGW("%s(), failed to set cpu affinity", __FUNCTION__);
        return sastat;
    }

    return 0;
}

#define THROTTLE_TIME_US 3000
static void *mixer_32b_threadloop(void *data)
{
    struct amlAudioMixer *audio_mixer = data;
    enum MIXER_OUTPUT_PORT port_index = MIXER_OUTPUT_PORT_PCM;
    aml_mixer_input_port_type_e in_index = AML_MIXER_INPUT_PORT_PCM_SYSTEM;
    int ret = 0;

    ALOGI("++%s start", __func__);

    audio_mixer->exit_thread = 0;
    prctl(PR_SET_NAME, "amlAudioMixer32");
    set_thread_affinity();
    while (!audio_mixer->exit_thread) {
        //pthread_mutex_lock(&audio_mixer->lock);
        //mixer_procs_msg_queue(audio_mixer);
        // processing throttle
        struct timespec tval_new;
        clock_gettime(CLOCK_MONOTONIC, &tval_new);
        const uint32_t delta_us = tspec_diff_to_us(audio_mixer->tval_last_write, tval_new);
        ret = mixer_inports_read(audio_mixer);
        if (ret < 0) {
            //usleep(5000);
            ALOGV("%s %d data not enough, next turn", __func__, __LINE__);
            notify_mixer_input_avail(audio_mixer);
            continue;
            //notify_mixer_input_avail(audio_mixer);
            //continue;
        }
        notify_mixer_input_avail(audio_mixer);
        ALOGV("%s %d do mixing", __func__, __LINE__);
        mixer_do_mixing_32bit(audio_mixer);
        uint64_t tpast_us = 0;
        clock_gettime(CLOCK_MONOTONIC, &tval_new);
        tpast_us = tspec_diff_to_us(audio_mixer->tval_last_write, tval_new);
        // audio patching should not in this write
        // TODO: fix me, make compatible with source output
        if (!audio_mixer->adev->audio_patching) {
            mixer_output_write(audio_mixer);
            mixer_update_tstamp(audio_mixer);
        }
    }

    ALOGI("--%s", __func__);
    return NULL;
}

static int mixer_do_continous_output(struct amlAudioMixer *audio_mixer)
{
    struct output_port *out_port = audio_mixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    int16_t *data_mixed = (int16_t *)out_port->data_buf;
    size_t frames = 4;
    size_t bytes = frames * out_port->cfg.frame_size;

    memset(data_mixed, 0 , bytes);
    set_outport_data_avail(out_port, bytes);
    mixer_output_write(audio_mixer);
    return 0;
}

static uint32_t get_mixer_inport_count(struct amlAudioMixer *audio_mixer)
{
    aml_mixer_input_port_type_e enPortIndex = AML_MIXER_INPUT_PORT_PCM_SYSTEM;
    uint32_t                    u32PortCnt = 0;
    for (; enPortIndex < AML_MIXER_INPUT_PORT_BUTT; enPortIndex++) {
        if (audio_mixer->in_ports[enPortIndex]) {
            u32PortCnt++;
        }
    }
    return u32PortCnt;
}

static void mixer_outport_feed_silence_frames(struct amlAudioMixer *audio_mixer)
{
    struct output_port *out_port = get_outport(audio_mixer, MIXER_OUTPUT_PORT_PCM);
    struct input_port *in_port_direct = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_DIRECT];
    struct input_port *in_port_system = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_SYSTEM];
    struct aml_stream_out *out = NULL;

    if (out_port == NULL) {
        ALOGE("%s(), port invalid", __func__);
        return;
    }

    int16_t *data_mixed = (int16_t *)out_port->data_buf;
    size_t frames = 128;
    size_t silence_frames = SILENCE_FRAME_MAX;
    size_t bytes = frames * out_port->cfg.frame_size;


    if (in_port_direct && in_port_direct->notify_cbk_data) {
        out = (struct aml_stream_out *)in_port_direct->notify_cbk_data;
    } else if (in_port_system && in_port_system->notify_cbk_data) {
        out = (struct aml_stream_out *)in_port_system->notify_cbk_data;
    }

    /*for a2dp, we don't need feed silence data*/
    if (out && (out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)) {
        return;
    }

    if (bytes > out_port->data_buf_len) {
        bytes = out_port->data_buf_len;
        frames = bytes/out_port->cfg.frame_size;
    }
    memset(data_mixed, 0 , bytes);
    do {
        set_outport_data_avail(out_port, bytes);
        mixer_output_write(audio_mixer);
        silence_frames -= frames;
    } while (silence_frames > 0);
    return;
}

static bool is_submix_disable(struct amlAudioMixer *audio_mixer) {
    struct aml_audio_device *adev = audio_mixer->adev;

    if (adev->audio_patching) {
        return true;
    } else if (is_bypass_submix_active(adev)) {
        return true;
    }
    return false;
}

static void *mixer_16b_threadloop(void *data)
{
    struct amlAudioMixer        *audio_mixer = data;
    struct audio_virtual_buf    *pstVirtualBuffer = NULL;

    ALOGI("[%s:%d] began create thread", __func__, __LINE__);
    if (audio_mixer->mixing_enable == 0) {
        pthread_exit(0);
        ALOGI("[%s:%d] mixing_enable is 0 exit thread", __func__, __LINE__);
        return NULL;
    }
    audio_mixer->exit_thread = 0;
    prctl(PR_SET_NAME, "amlAudioMixer16");
    set_thread_affinity();
    aml_set_thread_priority("amlAudioMixer16", audio_mixer->out_mixer_tid);
    while (!audio_mixer->exit_thread) {
        if (pstVirtualBuffer == NULL) {
            audio_virtual_buf_open((void **)&pstVirtualBuffer, "mixer_16bit_thread",
                    MIXER_WRITE_PERIOD_TIME_NANO * 4, MIXER_WRITE_PERIOD_TIME_NANO * 4, 0);
            audio_virtual_buf_process((void *)pstVirtualBuffer, MIXER_WRITE_PERIOD_TIME_NANO * 4);
        }
        mixer_inports_read(audio_mixer);

        audio_virtual_buf_process((void *)pstVirtualBuffer, MIXER_WRITE_PERIOD_TIME_NANO);
        notify_mixer_input_avail(audio_mixer);
        mixer_do_mixing_16bit(audio_mixer);

        if (audio_mixer->adev->debug_flag > 0) {
            ALOGD("[%s:%d] audio_patching:%d", __func__, __LINE__, audio_mixer->adev->audio_patching);
        }
        if (!is_submix_disable(audio_mixer)) {
            mixer_output_write(audio_mixer);
            mixer_update_tstamp(audio_mixer);
        }
    }
    if (pstVirtualBuffer != NULL) {
        audio_virtual_buf_close((void **)&pstVirtualBuffer);
    }

    ALOGI("[%s:%d] exit thread", __func__, __LINE__);
    return NULL;
}

uint32_t mixer_get_inport_latency_frames(struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index)
{
    struct input_port *port = audio_mixer->in_ports[port_index];
    int written = 0;

    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return 0;
    }

    return port->get_latency_frames(port);
}

uint32_t mixer_get_outport_latency_frames(struct amlAudioMixer *audio_mixer)
{
    struct output_port *port = get_outport(audio_mixer, MIXER_OUTPUT_PORT_PCM);
    if (port == NULL) {
        ALOGE("%s(), port invalid", __func__);
    }
    return outport_get_latency_frames(port);
}

int pcm_mixer_thread_run(struct amlAudioMixer *audio_mixer)
{
    struct output_port *out_pcm_port = NULL;
    int ret = 0;

    ALOGI("++%s()", __func__);
    if (audio_mixer == NULL) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }

    out_pcm_port = audio_mixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    if (out_pcm_port == NULL) {
        ALOGE("%s(), out port not initialized", __func__);
        return -EINVAL;
    }

    if (audio_mixer->out_mixer_tid > 0) {
        ALOGE("%s(), out mixer thread already running", __func__);
        return -EINVAL;
    }
    audio_mixer->mixing_enable = 1;
    ALOGI("++%s() audio_mixer->mixing_enable %d", __func__, audio_mixer->mixing_enable);
    switch (out_pcm_port->cfg.format) {
    case AUDIO_FORMAT_PCM_32_BIT:
        ret = pthread_create(&audio_mixer->out_mixer_tid,
                NULL, mixer_32b_threadloop, audio_mixer);
        if (ret < 0)
            ALOGE("%s() thread run failed.", __func__);
        break;
    case AUDIO_FORMAT_PCM_16_BIT:
        ret = pthread_create(&audio_mixer->out_mixer_tid,
                NULL, mixer_16b_threadloop, audio_mixer);
        if (ret < 0)
            ALOGE("%s() thread run failed.", __func__);
        break;
    default:
        ALOGE("%s(), format not supported", __func__);
        break;
    }
    ALOGI("++%s() audio_mixer->mixing_enable %d, pthread_create ret %d", __func__, audio_mixer->mixing_enable, ret);

    return ret;
}

int pcm_mixer_thread_exit(struct amlAudioMixer *audio_mixer)
{
    ALOGD("+%s()", __func__);
    audio_mixer->mixing_enable = 0;
    ALOGI("++%s() audio_mixer->mixing_enable %d", __func__, audio_mixer->mixing_enable);
    // block exit
    audio_mixer->exit_thread = 1;
    pthread_join(audio_mixer->out_mixer_tid, NULL);
    audio_mixer->out_mixer_tid = 0;

    notify_mixer_exit(audio_mixer);
    return 0;
}
struct amlAudioMixer *newAmlAudioMixer(
        struct pcm *pcm_handle,
        struct audioCfg cfg,
        struct aml_audio_device *adev)
{
    struct amlAudioMixer *audio_mixer = NULL;
    int ret = 0;
    ALOGD("%s()", __func__);

    if (!pcm_handle) {
        ALOGE("%s(), NULL pcm handle", __func__);
        return NULL;
    }
    audio_mixer = aml_audio_calloc(1, sizeof(*audio_mixer));
    if (audio_mixer == NULL) {
        ALOGE("%s(), no memory", __func__);
        return NULL;
    }

    // 2 channel  32bit
    audio_mixer->tmp_buffer = aml_audio_calloc(1, MIXER_FRAME_COUNT * MIXER_OUT_FRAME_SIZE);
    if (audio_mixer->tmp_buffer == NULL) {
        ALOGE("%s(), no memory", __func__);
        goto err_tmp;
    }
    // 2 channel X sample bytes;
    audio_mixer->frame_size_tmp = 2 * audio_bytes_per_sample(cfg.format);

    mixer_set_state(audio_mixer, MIXER_IDLE);
    audio_mixer->adev = adev;
    ret = init_mixer_output_port(audio_mixer, pcm_handle,
            cfg, MIXER_FRAME_COUNT);
    if (ret < 0) {
        ALOGE("%s(), init mixer out port failed", __func__);
        goto err_state;
    }
    audio_mixer->inportsMasks = 0;
    audio_mixer->supportedInportsMasks = (1 << NR_INPORTS) - 1;
    pthread_mutex_init(&audio_mixer->lock, NULL);
    return audio_mixer;

err_state:
    aml_audio_free(audio_mixer->tmp_buffer);
    audio_mixer->tmp_buffer = NULL;
err_tmp:
    aml_audio_free(audio_mixer);
    audio_mixer = NULL;

    return audio_mixer;
}

void freeAmlAudioMixer(struct amlAudioMixer *audio_mixer)
{
    if (audio_mixer) {
        pthread_mutex_destroy(&audio_mixer->lock);
        aml_audio_free(audio_mixer);
    }
}

int64_t mixer_latency_frames(struct amlAudioMixer *audio_mixer)
{
    (void)audio_mixer;
    /* TODO: calc the mixer buf latency
    * Now using estimated buffer length
    */
    return MIXER_FRAME_COUNT;
}

int mixer_get_presentation_position(
        struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index,
        uint64_t *frames,
        struct timespec *timestamp)
{
    struct input_port *port = audio_mixer->in_ports[port_index];

    if (!port) {
        ALOGW("%s(), port not ready now", __func__);
        return -EINVAL;
    }

    *frames = port->presentation_frames;
    *timestamp = port->timestamp;
    if (!is_inport_pts_valid(port)) {
        ALOGW("%s(), not valid now", __func__);
        return -EINVAL;
    }
    return 0;
}

int mixer_set_padding_size(
        struct amlAudioMixer *audio_mixer,
        aml_mixer_input_port_type_e port_index,
        int padding_bytes)
{
    struct input_port *port = audio_mixer->in_ports[port_index];
    if (!port) {
        ALOGE("%s(), NULL pointer", __func__);
        return -EINVAL;
    }
    return set_inport_padding_size(port, padding_bytes);
}

int mixer_outport_pcm_restart(struct amlAudioMixer *audio_mixer)
{
    struct output_port *port = get_outport(audio_mixer, MIXER_OUTPUT_PORT_PCM);

    if (port == NULL) {
        ALOGE("%s(), port invalid", __func__);
        return -EINVAL;
    }

    outport_pcm_restart(port);
    return 0;
}

bool has_hwsync_stream_running(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct subMixing *sm = adev->sm;
    if (sm == NULL)
        return false;
    struct amlAudioMixer *audio_mixer = sm->mixerData;
    if (audio_mixer == NULL)
        return false;

    unsigned int masks = audio_mixer->inportsMasks;
    struct input_port *in_port;

    while (masks) {
        int i = 31 - __builtin_clz(masks);
        masks &= ~(1 << i);
        in_port = audio_mixer->in_ports[i];
        if (NULL == in_port) {
            continue;
        }
        if (in_port->enInPortType == AML_MIXER_INPUT_PORT_PCM_DIRECT && in_port->notify_cbk_data) {
            struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
            if ((out != aml_out) && out->hw_sync_mode && !out->standby)
                return true;
        }
    }
    return false;
}
void mixer_dump(int s32Fd, const struct aml_audio_device *pstAmlDev)
{
    if (NULL == pstAmlDev || NULL == pstAmlDev->sm) {
        dprintf(s32Fd, "[AML_HAL] [%s:%d] device or sub mixing is NULL !\n", __func__, __LINE__);
        return;
    }
    struct amlAudioMixer *pstAudioMixer = (struct amlAudioMixer *)pstAmlDev->sm->mixerData;
    if (NULL == pstAudioMixer) {
        dprintf(s32Fd, "[AML_HAL] [%s:%d] amlAudioMixer is NULL !\n", __func__, __LINE__);
        return;
    }
    dprintf(s32Fd, "[AML_HAL]---------------------input port description cnt: [%d]--------------\n",
        get_mixer_inport_count(pstAudioMixer));
    aml_mixer_input_port_type_e enInPort = AML_MIXER_INPUT_PORT_PCM_SYSTEM;
    for (; enInPort < AML_MIXER_INPUT_PORT_BUTT; enInPort++) {
        struct input_port *pstInputPort = pstAudioMixer->in_ports[enInPort];
        if (pstInputPort) {
            dprintf(s32Fd, "[AML_HAL]  input port type: %s\n", mixerInputType2Str(pstInputPort->enInPortType));
            dprintf(s32Fd, "[AML_HAL]      Channel       : %10d     | Format    : %#10x\n",
                pstInputPort->cfg.channelCnt, pstInputPort->cfg.format);
            dprintf(s32Fd, "[AML_HAL]      FrameCnt      : %10zu     | data size : %10zu Byte\n",
                pstInputPort->data_buf_frame_cnt, pstInputPort->data_len_bytes);
            dprintf(s32Fd, "[AML_HAL]      is_hwsync     : %10d     | rbuf size : %10d Byte\n",
                pstInputPort->is_hwsync, pstInputPort->r_buf->size);
        }
    }
    dprintf(s32Fd, "[AML_HAL]---------------------output port description----------------------\n");
    struct output_port *pstOutPort = pstAudioMixer->out_ports[MIXER_OUTPUT_PORT_PCM];
    if (pstOutPort) {
        dprintf(s32Fd, "[AML_HAL]      Channel       : %10d     | Format    : %#10x\n", pstOutPort->cfg.channelCnt, pstOutPort->cfg.format);
        dprintf(s32Fd, "[AML_HAL]      FrameCnt      : %10zu     | data size : %10zu Byte\n", pstOutPort->data_buf_frame_cnt, pstOutPort->data_buf_len);
    } else {
        dprintf(s32Fd, "[AML_HAL] not find output port description!!!\n");
    }
}

