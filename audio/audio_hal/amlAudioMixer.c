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

#define LOG_TAG "amlAudioMixer"
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
#include <audio_utils/primitives.h>

#ifdef ENABLE_AEC_APP
#include "audio_aec.h"
#endif

#include "amlAudioMixer.h"
#include "audio_hw_utils.h"
#include "hw_avsync.h"
#include "audio_hwsync.h"
#include "audio_data_process.h"
#include "audio_virtual_buf.h"

#include "audio_hw.h"
#include "a2dp_hal.h"
#include "audio_bt_sco.h"
#include "aml_audio_timer.h"
#include "aml_malloc_debug.h"


enum {
    INPORT_NORMAL,   // inport not underrun
    INPORT_UNDERRUN, //inport doesn't have data, underrun may happen later
    INPORT_FEED_SILENCE_DONE, //underrun will happen quickly, we feed some silence data to avoid noise
};

//simple mixer support: 2 in , 1 out
struct amlAudioMixer {
    input_port *in_ports[NR_INPORTS];
    uint32_t inportsMasks; // records of inport IDs
    unsigned int supportedInportsMasks; // 1<< NR_EXTRA_INPORTS - 1
    MIXER_OUTPUT_PORT cur_output_port_type;
    output_port *out_ports[MIXER_OUTPUT_PORT_NUM];
    pthread_mutex_t inport_lock;
    ssize_t (*write)(struct amlAudioMixer *mixer, void *buffer, int bytes);
    void *in_tmp_buffer;                    /* mixer temp input buffer. */
    void *out_tmp_buffer;                   /* mixer temp output buffer. */
    size_t frame_size_tmp;
    size_t tmp_buffer_size;                 /* mixer temp buffer size. */
    uint32_t hwsync_frame_size;
    pthread_t out_mixer_tid;
    pthread_mutex_t lock;
    int exit_thread : 1;
    int mixing_enable : 1;
    aml_mixer_state state;
    struct timespec tval_last_write;
    struct aml_audio_device *adev;
    bool continuous_output;
    //int init_ok : 1;
    int submix_standby;
    //aml_audio_mixer_run_state_type_e run_state;
};


int mixer_set_state(struct amlAudioMixer *audio_mixer, aml_mixer_state state)
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

aml_mixer_state mixer_get_state(struct amlAudioMixer *audio_mixer)
{
    return audio_mixer->state;
}

/**
 * Returns the first initialized port according to pMasks and resets
 * the corresponding bit. Can be called repeatedly to iterate over
 * initialized ports.
 */
static inline input_port *mixer_get_inport_by_mask_right_first(
        struct amlAudioMixer *audio_mixer, uint32_t *pMasks)
{
    uint8_t bit_position = get_bit_position_in_mask(NR_INPORTS - 1, pMasks);
    return audio_mixer->in_ports[bit_position];
}

/**
 * Returns the index of the first available supported port.
 */
static unsigned int mixer_get_available_inport_index(struct amlAudioMixer *audio_mixer)
{
    unsigned int index = 0;
    index = (~audio_mixer->inportsMasks) & audio_mixer->supportedInportsMasks;
    index = __builtin_ctz(index);
    AM_LOGV("inportsMasks:%#x, index %d", audio_mixer->inportsMasks, index);
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
    R_CHECK_POINTER_LEGAL(-EINVAL, audio_mixer, "");
    R_CHECK_POINTER_LEGAL(-EINVAL, config, "");
    R_CHECK_POINTER_LEGAL(-EINVAL, notify_data, "");

    input_port *in_port = NULL;
    uint8_t port_index = -1;
    struct aml_stream_out *aml_out = notify_data;
    bool direct_on = false;

    if (aml_out->inputPortID != -1) {
       AM_LOGW("stream input port id:%d exits delete it.", aml_out->inputPortID);
       delete_mixer_input_port(audio_mixer, aml_out->inputPortID);
    }
    /* if direct on, ie. the ALSA buffer is full, no need padding data anymore  */
    direct_on = (audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_DIRECT] != NULL);
    in_port = new_input_port(MIXER_FRAME_COUNT, config, flags, volume, direct_on);
    port_index = mixer_get_available_inport_index(audio_mixer);
    R_CHECK_PARAM_LEGAL(-1, port_index, 0, NR_INPORTS, "");

    if (audio_mixer->in_ports[port_index] != NULL) {
        AM_LOGW("inport index:[%d]%s already exists! recreate", port_index, mixerInputType2Str(port_index));
        free_input_port(audio_mixer->in_ports[port_index]);
    }

    in_port->ID = port_index;
    AM_LOGI("input port:%s, size %d frames, frame_write_sum:%lld",
        mixerInputType2Str(in_port->enInPortType), MIXER_FRAME_COUNT, aml_out->frame_write_sum);
    audio_mixer->in_ports[port_index] = in_port;
    audio_mixer->inportsMasks |= 1 << port_index;
    aml_out->inputPortID = port_index;

    set_port_notify_cbk(in_port, on_notify_cbk, notify_data);
    set_port_input_avail_cbk(in_port, on_input_avail_cbk, input_avail_data);
    if (on_meta_data_cbk && meta_data) {
        in_port->is_hwsync = true;
        set_port_meta_data_cbk(in_port, on_meta_data_cbk, meta_data);
    }
    in_port->initial_frames = aml_out->frame_write_sum;
    return 0;
}

int delete_mixer_input_port(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    R_CHECK_PARAM_LEGAL(-EINVAL, port_index, 0, NR_INPORTS, "");
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);

    AM_LOGI("input port ID:%d, type:%s, cur mask:%#x", port_index,
        mixerInputType2Str(in_port->enInPortType), audio_mixer->inportsMasks);
    pthread_mutex_lock(&audio_mixer->lock);
    pthread_mutex_lock(&audio_mixer->inport_lock);
    free_input_port(in_port);
    audio_mixer->in_ports[port_index] = NULL;
    audio_mixer->inportsMasks &= ~(1 << port_index);
    pthread_mutex_unlock(&audio_mixer->inport_lock);
    pthread_mutex_unlock(&audio_mixer->lock);
    return 0;
}

int send_mixer_inport_message(struct amlAudioMixer *audio_mixer, uint8_t port_index, PORT_MSG msg)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return send_inport_message(in_port, msg);
}

void set_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer, uint32_t frame_size)
{
    AM_LOGI("framesize %d", frame_size);
    audio_mixer->hwsync_frame_size = frame_size;
}

uint32_t get_mixer_hwsync_frame_size(struct amlAudioMixer *audio_mixer)
{
    return audio_mixer->hwsync_frame_size;
}

uint32_t get_mixer_inport_consumed_frames(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return get_inport_consumed_size(in_port) / in_port->cfg.frame_size;
}

int set_mixer_inport_volume(struct amlAudioMixer *audio_mixer, uint8_t port_index, float vol)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    if (vol > 1.0 || vol < 0) {
        AM_LOGE("invalid vol %f", vol);
        return -EINVAL;
    }
    set_inport_volume(in_port, vol);
    return 0;
}

float get_mixer_inport_volume(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return get_inport_volume(in_port);
}

int mixer_write_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index, const void *buffer, int bytes)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    int         written = 0;

    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    written = in_port->write(in_port, buffer, bytes);
    if (get_inport_state(in_port) != ACTIVE) {
        AM_LOGI("input port:%s is active now", mixerInputType2Str(in_port->enInPortType));
        set_inport_state(in_port, ACTIVE);
    }
    AM_LOGV("portIndex %d", port_index);
    return written;
}

int mixer_read_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index, void *buffer, int bytes)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return in_port->read(in_port, buffer, bytes);
}

int mixer_set_inport_state(struct amlAudioMixer *audio_mixer, uint8_t port_index, port_state state)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return set_inport_state(in_port, state);
}

port_state mixer_get_inport_state(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return get_inport_state(in_port);
}
//TODO: handle message queue
static void mixer_procs_msg_queue(struct amlAudioMixer *audio_mixer __unused)
{
    AM_LOGV("start");
    return;
}


static output_port *mixer_get_cur_outport(struct amlAudioMixer *audio_mixer)
{
    output_port *handle = NULL;
    R_CHECK_PARAM_LEGAL(NULL, audio_mixer->cur_output_port_type, MIXER_OUTPUT_PORT_STEREO_PCM, MIXER_OUTPUT_PORT_NUM-1, "");
    handle = audio_mixer->out_ports[audio_mixer->cur_output_port_type];
    R_CHECK_POINTER_LEGAL(NULL, handle, "get pcm handle fail, cur output type:%s[%d]",
        mixerOutputType2Str(audio_mixer->cur_output_port_type), audio_mixer->cur_output_port_type);
    return handle;
}

size_t get_outport_data_avail(output_port *outport)
{
    return outport->bytes_avail;
}

int set_outport_data_avail(output_port *outport, size_t avail)
{
    if (avail > outport->data_buf_len) {
        AM_LOGE("invalid avail %zu", avail);
        return -EINVAL;
    }
    outport->bytes_avail = avail;
    return 0;
}

int init_mixer_output_port(struct amlAudioMixer *audio_mixer,
        MIXER_OUTPUT_PORT output_type,
        struct audioCfg *config,
        size_t buf_frames)
{
    struct aml_audio_device     *adev = audio_mixer->adev;
    output_port *out_port = new_output_port(output_type, config, buf_frames);
    R_CHECK_POINTER_LEGAL(-1, out_port, "new_output_port fail");
    MIXER_OUTPUT_PORT port_index = out_port->enOutPortType;
    audio_mixer->cur_output_port_type = output_type;

    //set_port_notify_cbk(port, on_notify_cbk, notify_data);
    //set_port_input_avail_cbk(port, on_input_avail_cbk, input_avail_data);
    audio_mixer->out_ports[port_index] = out_port;
    if (config->channelCnt > 2) {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_MULTI_CH_LPCM);
    } else {
        aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_STEREO_PCM);
    }

#ifdef ENABLE_AEC_APP
    out_port->aec = audio_mixer->adev->aec;
    struct pcm_config alsa_config;
    output_get_alsa_config(out_port, &alsa_config);
    int aec_ret = init_aec_reference_config(out_port->aec, alsa_config);
    NO_R_CHECK_RET(aec_ret, "AEC: Speaker config init failed!");
#endif
    return 0;
}

int delete_mixer_output_port(struct amlAudioMixer *audio_mixer, MIXER_OUTPUT_PORT port_index)
{
    struct aml_audio_device     *adev = audio_mixer->adev;
#ifdef ENABLE_AEC_APP
    destroy_aec_reference_config(adev->aec);
#endif
    audio_mixer->cur_output_port_type = MIXER_OUTPUT_PORT_INVAL;
    free_output_port(audio_mixer->out_ports[port_index]);
    audio_mixer->out_ports[port_index] = NULL;
    aml_mixer_ctrl_set_int(&adev->alsa_mixer, AML_MIXER_ID_SPDIF_FORMAT, AML_STEREO_PCM);
    return 0;
}

static int mixer_output_startup(struct amlAudioMixer *audio_mixer)
{
    MIXER_OUTPUT_PORT port_index = 0;
    output_port *out_port = audio_mixer->out_ports[port_index];

    ALOGI("++%s start open", __func__);
    out_port->start(out_port);
    audio_mixer->submix_standby = 0;

    return 0;
}

int mixer_output_standby(struct amlAudioMixer *audio_mixer)
{
    ALOGI("[%s:%d] request sleep thread", __func__, __LINE__);
    int timeoutMs = 200;
    (void)audio_mixer;
    //audio_mixer->run_state = AML_AUDIO_MIXER_RUN_STATE_REQ_SLEEP;
    return 0;
}

static int mixer_thread_sleep(struct amlAudioMixer *audio_mixer)
{
    output_port *out_port = audio_mixer->out_ports[MIXER_OUTPUT_PORT_STEREO_PCM];

    if (false == audio_mixer->submix_standby) {
        ALOGI("[%s:%d] start going to standby", __func__, __LINE__);
        out_port->standby(out_port);
        audio_mixer->submix_standby = true;
    }
    pthread_mutex_lock(&audio_mixer->lock);
    ALOGI("[%s:%d] the thread is sleeping", __func__, __LINE__);
    //audio_mixer->run_state = AML_AUDIO_MIXER_RUN_STATE_SLEEP;
    //pthread_cond_wait(&audio_mixer->cond, &audio_mixer->lock);
    ALOGI("[%s:%d] the thread is awakened", __func__, __LINE__);
    pthread_mutex_unlock(&audio_mixer->lock);
    return 0;
}

int mixer_output_dummy(struct amlAudioMixer *audio_mixer, bool en)
{
    MIXER_OUTPUT_PORT port_index = 0;
    output_port *out_port = audio_mixer->out_ports[port_index];

    ALOGI("++%s(), en = %d", __func__, en);
    outport_set_dummy(out_port, en);

    return 0;
}

static int mixer_output_write(struct amlAudioMixer *audio_mixer)
{
    audio_config_base_t in_data_config = {48000, AUDIO_CHANNEL_OUT_STEREO, AUDIO_FORMAT_PCM_16_BIT};
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    struct aml_audio_device *adev = audio_mixer->adev;
    int count = 3;

    out_port->sound_track_mode = audio_mixer->adev->sound_track_mode;
    while (out_port->bytes_avail > 0) {
        // out_write_callbacks();
        if (adev->active_outport == OUTPORT_A2DP) {
            if (out_port->cfg.channelCnt == 1) {
                in_data_config.channel_mask = AUDIO_CHANNEL_OUT_MONO;
            } else if (out_port->cfg.channelCnt == 2) {
                in_data_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            } else {
                AM_LOGW("not supported channel:%d", out_port->cfg.channelCnt);
                return out_port->bytes_avail;
            }
            in_data_config.sample_rate = out_port->cfg.sampleRate;
            in_data_config.format = out_port->cfg.format;

            int ret = a2dp_out_write(adev, &in_data_config, out_port->data_buf, out_port->bytes_avail);
            if (ret == 0 && count-- > 0) {
                 continue;
            }
        } else if (is_sco_port(adev->active_outport)) {
            if (out_port->cfg.channelCnt == 1) {
                in_data_config.channel_mask = AUDIO_CHANNEL_OUT_MONO;
            } else if (out_port->cfg.channelCnt == 2) {
                in_data_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
            } else {
                AM_LOGW("not supported channel:%d", out_port->cfg.channelCnt);
                return out_port->bytes_avail;
            }
            in_data_config.sample_rate = out_port->cfg.sampleRate;
            in_data_config.format = out_port->cfg.format;
            write_to_sco(adev, &in_data_config, out_port->data_buf, out_port->bytes_avail);
        } else {
            pthread_mutex_lock(&audio_mixer->adev->alsa_pcm_lock);
            if (audio_mixer->submix_standby)
                mixer_output_startup(audio_mixer);
            out_port->write(out_port, out_port->data_buf, out_port->bytes_avail);
            pthread_mutex_unlock(&audio_mixer->adev->alsa_pcm_lock);
        }
        set_outport_data_avail(out_port, 0);
    };
    return 0;
}

int init_mixer_temp_buffer(struct amlAudioMixer *audio_mixer)
{
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    R_CHECK_POINTER_LEGAL(-1, out_port, "not find output port.");
    audio_mixer->frame_size_tmp = out_port->cfg.channelCnt * audio_bytes_per_sample(out_port->cfg.format);
    audio_mixer->tmp_buffer_size = MIXER_FRAME_COUNT * audio_mixer->frame_size_tmp;
    audio_mixer->in_tmp_buffer = aml_audio_realloc(audio_mixer->in_tmp_buffer, audio_mixer->tmp_buffer_size);
    R_CHECK_POINTER_LEGAL(-1, audio_mixer->in_tmp_buffer, "allocate amlAudioMixer fail.");

    audio_mixer->out_tmp_buffer = aml_audio_realloc(audio_mixer->out_tmp_buffer, audio_mixer->tmp_buffer_size);
    if (audio_mixer->out_tmp_buffer == NULL) {
        AM_LOGE("allocate amlAudioMixer out_tmp_buffer no memory");
        aml_audio_free(audio_mixer->in_tmp_buffer);
        audio_mixer->in_tmp_buffer = NULL;
        return -1;
    }
    return 0;
}

void deinit_mixer_temp_buffer(struct amlAudioMixer *audio_mixer)
{
    if (audio_mixer->in_tmp_buffer) {
        free(audio_mixer->in_tmp_buffer);
        audio_mixer->in_tmp_buffer = NULL;
    }
    if (audio_mixer->out_tmp_buffer) {
        free(audio_mixer->out_tmp_buffer);
        audio_mixer->out_tmp_buffer = NULL;
    }
}

#define DEFAULT_KERNEL_FRAMES (DEFAULT_PLAYBACK_PERIOD_SIZE*DEFAULT_PLAYBACK_PERIOD_CNT)

static int mixer_update_tstamp(struct amlAudioMixer *audio_mixer)
{
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    input_port *in_port = NULL;
    unsigned int avail;
    uint32_t masks = audio_mixer->inportsMasks;

    while (masks) {
        in_port = mixer_get_inport_by_mask_right_first(audio_mixer, &masks);
        if (NULL != in_port && in_port->enInPortType == AML_MIXER_INPUT_PORT_PCM_SYSTEM) {
            break;
        }
    }

    /*only deal with system audio */
    if (in_port == NULL || out_port == NULL || out_port->pcm_handle == NULL) {
        AM_LOGV("in_port:%p or out_port:%p or pcm handle:%p is null",
            in_port, out_port, out_port->pcm_handle);
        return 0;
    }

    if (pcm_get_htimestamp(out_port->pcm_handle, &avail, &in_port->timestamp) == 0) {
        size_t kernel_buf_size = DEFAULT_KERNEL_FRAMES;
        int64_t signed_frames = in_port->mix_consumed_frames - kernel_buf_size + avail;
        if (signed_frames < 0) {
            signed_frames = 0;
        }
        in_port->presentation_frames = in_port->initial_frames + signed_frames;
        AM_LOGV("present frames:%lld, initial %lld, consumed %lld, sec:%ld, nanosec:%ld",
                in_port->presentation_frames,
                in_port->initial_frames,
                in_port->mix_consumed_frames,
                in_port->timestamp.tv_sec,
                in_port->timestamp.tv_nsec);
    }

    return 0;
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
    AM_LOGI("size %d, stride %f", fade_size, fade_out->stride);
    return 0;
}

int process_fade_out(void *buf, int bytes, struct fade_out *fout)
{
    int i = 0;
    int frame_cnt = bytes / fout->sample_size / fout->channel_cnt;
    int16_t *sample = (int16_t *)buf;

    if (fout->channel_cnt != 2 || fout->sample_size != 2)
        AM_LOGE("not support yet");
    AM_LOGI("++++fade out vol %f, size %d", fout->vol, fout->fade_size);
    for (i = 0; i < frame_cnt; i++) {
        sample[i] = sample[i]*fout->vol;
        sample[i+1] = sample[i+1]*fout->vol;
        fout->vol -= fout->stride;
        if (fout->vol < 0)
            fout->vol = 0;
    }
    fout->fade_size -= bytes;
    AM_LOGI("----fade out vol %f, size %d", fout->vol, fout->fade_size);

    return 0;
}

static int update_inport_avail(input_port *in_port)
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

static void process_port_msg(input_port *in_port)
{
    port_message *msg = get_inport_message(in_port);
    if (msg) {
        AM_LOGI("msg: %s", port_msg_to_str(msg->msg_what));
        switch (msg->msg_what) {
        case MSG_PAUSE: {
            struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
            audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
            //AM_LOGI("[%s:%d] hwsync:%p tsync pause", hwsync);
            if ((hwsync != NULL) && (hwsync->use_mediasync)) {
                aml_hwsync_set_tsync_pause(hwsync);
            }
            set_inport_state(in_port, PAUSING);
            break;
        }
        case MSG_FLUSH:
            set_inport_state(in_port, FLUSHING);
            break;
        case MSG_RESUME: {
            struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
            audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
            //AM_LOGI("[%s:%d] hwsync:%p tsync resume", hwsync);
            if ((hwsync != NULL) && (hwsync->use_mediasync)) {
                aml_hwsync_set_tsync_resume(hwsync);
            }
            set_inport_state(in_port, RESUMING);
            break;
        }
        default:
            AM_LOGE("not support");
        }

        remove_inport_message(in_port, msg);
    }
}

int mixer_flush_inport(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return reset_input_port(in_port);
}

static int mixer_inports_read(struct amlAudioMixer *audio_mixer)
{
    unsigned int masks = audio_mixer->inportsMasks;
    AM_LOGV("+++");
    while (masks) {
        input_port *in_port = NULL;
        in_port = mixer_get_inport_by_mask_right_first(audio_mixer, &masks);
        if (NULL == in_port) {
            continue;
        }
        int ret = 0, fade_out = 0, fade_in = 0;
        aml_mixer_input_port_type_e type = in_port->enInPortType;
        process_port_msg(in_port);
        port_state state = get_inport_state(in_port);

        if (type == AML_MIXER_INPUT_PORT_PCM_DIRECT) {
            //if in pausing states, don't retrieve data
            if (state == PAUSING) {
                fade_out = 1;
            } else if (state == RESUMING) {
                struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
                audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;
                fade_in = 1;
                AM_LOGI("input port:%s tsync resume", mixerInputType2Str(type));
                aml_hwsync_set_tsync_resume(hwsync);
                set_inport_state(in_port, ACTIVE);
            } else if (state == STOPPED || state == PAUSED || state == FLUSHED) {
                AM_LOGV("input port:%s stopped, paused or flushed", mixerInputType2Str(type));
                continue;
            } else if (state == FLUSHING) {
                mixer_flush_inport(audio_mixer, in_port->ID);
                AM_LOGI("input port:%s flushing->flushed", mixerInputType2Str(type));
                set_inport_state(in_port, FLUSHED);
                continue;
            }
            if (get_inport_state(in_port) == ACTIVE && in_port->data_valid) {
                AM_LOGI("input port:%s data already valid", mixerInputType2Str(type));
                continue;
            }
        } else {
            if (in_port->data_valid) {
                AM_LOGI("input port ID:%d port:%s, data already valid", in_port->ID, mixerInputType2Str(type));
                continue;
            }
        }

        int input_avail_size = in_port->rbuf_avail(in_port);
        AM_LOGV("input port:%s, portId:%d, avail:%d, masks:%#x, inportsMasks:%#x, data_len_bytes:%zu",
            mixerInputType2Str(type), in_port->ID, input_avail_size, masks, audio_mixer->inportsMasks, in_port->data_len_bytes);
        if (input_avail_size >= in_port->data_len_bytes) {
            if (in_port->first_read) {
                if (input_avail_size < in_port->inport_start_threshold) {
                    continue;
                } else {
                    AM_LOGI("input port:%s first start, portId:%d, avail:%d",
                        mixerInputType2Str(type), in_port->ID, input_avail_size);
                    in_port->first_read = false;
                }
            }
            ret = mixer_read_inport(audio_mixer, in_port->ID, in_port->data, in_port->data_len_bytes);
            if (ret == (int)in_port->data_len_bytes) {
                if (fade_out) {
                    struct aml_stream_out *out = (struct aml_stream_out *)in_port->notify_cbk_data;
                    audio_hwsync_t *hwsync = (out != NULL) ? (out->hwsync) : NULL;

                    AM_LOGI("output port:%s fade out, pausing->pausing_1, tsync pause audio", mixerInputType2Str(type));
                    if (audio_mixer->adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)
                        memset(in_port->r_buf->start_addr, 0, in_port->r_buf->size);
                    aml_hwsync_set_tsync_pause(hwsync);
                    audio_fade_func(in_port->data, ret, 0);
                    set_inport_state(in_port, PAUSED);
                } else if (fade_in) {
                    AM_LOGI("input port:%s fade in", mixerInputType2Str(type));
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
                AM_LOGW("port:%s read fail, have read:%d Byte, need %zu Byte",
                    mixerInputType2Str(type), ret, in_port->data_len_bytes);
            }
        } else {
            struct aml_audio_device     *adev = audio_mixer->adev;
            if (adev->debug_flag) {
                AM_LOGD("port:%d ring buffer data is not enough", in_port->ID);
            }
        }
    }

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
        input_port *in_port, output_port *out_port)
{
    uint32_t frame_size = get_mixer_hwsync_frame_size(audio_mixer);
    uint32_t port_consumed_size = get_inport_consumed_size(in_port);
    int diff_ms = 0;
    struct hw_avsync_header header;
    int ret = 0;

    if (frame_size == 0) {
        AM_LOGV("invalid frame size 0");
        return -EINVAL;
    }

    if (!in_port->is_hwsync) {
        AM_LOGE("not hwsync port");
        return -EINVAL;
    }

    memset(&header, 0, sizeof(struct hw_avsync_header));
    AM_LOGV("direct out port bytes before cbk %zu", out_port->bytes_avail);
    if (!in_port->meta_data_cbk) {
        AM_LOGE("no meta_data_cbk set!!");
        return -EINVAL;
    }
    AM_LOGV("port %p, data %p", in_port, in_port->meta_data_cbk_data);
    ret = in_port->meta_data_cbk(in_port->meta_data_cbk_data,
                port_consumed_size, &header, &diff_ms);
    if (ret < 0) {
        if (ret != -EAGAIN)
            AM_LOGE("meta_data_cbk fail err = %d!!", ret);
        return ret;
    }
    AM_LOGV("meta data cbk, diffms = %d", diff_ms);
    if (diff_ms > 0) {
        in_port->bytes_to_insert = diff_ms * 48 * 4;
    } else if (diff_ms < 0) {
        in_port->bytes_to_skip = -diff_ms * 48 * 4;
    }

    return 0;
}


static int mixer_do_mixing_32bit(struct amlAudioMixer *audio_mixer)
{
    input_port *in_port_sys = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_SYSTEM];
    input_port *in_port_drct = audio_mixer->in_ports[AML_MIXER_INPUT_PORT_PCM_DIRECT];
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
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
        AM_LOGE("out null !!!");
        return 0;
    }
    if (!in_port_sys && !in_port_drct) {
        AM_LOGE("sys or direct pcm must exist!!!");
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
        AM_LOGV("only direct okay");
        direct_only = 1;
    } else if (sys_okay) {
        sys_only = 1;
    } else {
        AM_LOGV("sys direct both not ready!");
        return -EINVAL;
    }

    data_mixed = (int16_t *)out_port->data_buf;
    memset(audio_mixer->out_tmp_buffer, 0 , MIXER_FRAME_COUNT * out_port->cfg.frame_size);
    if (mixing) {
        AM_LOGV("mixing");
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
            AM_LOGD("insert mixing data, need %zu, insert length %zu",
                    in_port_drct->bytes_to_insert, mixing_len_bytes);
            //memcpy(data_mixed, data_sys, mixing_len_bytes);
            //memcpy(audio_mixer->out_tmp_buffer, data_sys, mixing_len_bytes);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/systbeforemix.raw",
                        data_sys, in_port_sys->data_len_bytes);
            }
            frames_written = do_mixing_2ch(audio_mixer->out_tmp_buffer, data_sys,
                frames, in_port_sys->cfg.format, out_port->cfg.format);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/sysAftermix.raw",
                        audio_mixer->out_tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
            }
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->out_tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->out_tmp_buffer,
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
            frames_written = do_mixing_2ch(audio_mixer->out_tmp_buffer, data_drct,
                frames, in_port_drct->cfg.format, out_port->cfg.format);
            if (DEBUG_DUMP)
                aml_audio_dump_audio_bitstreams("/data/audio/tmpMixed0.raw",
                    audio_mixer->out_tmp_buffer, frames * audio_mixer->frame_size_tmp);
            frames_written = do_mixing_2ch(audio_mixer->out_tmp_buffer, data_sys,
                frames, in_port_sys->cfg.format, out_port->cfg.format);
            if (DEBUG_DUMP)
                aml_audio_dump_audio_bitstreams("/data/audio/tmpMixed1.raw",
                    audio_mixer->out_tmp_buffer, frames * audio_mixer->frame_size_tmp);
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->out_tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->out_tmp_buffer,
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
        AM_LOGV("sys_only, frames %zu", frames);
        mixing_len_bytes = in_port_sys->data_len_bytes;
        data_sys = (int16_t *)in_port_sys->data;
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/audiosyst.raw",
                    in_port_sys->data, mixing_len_bytes);
        }
        // processing data and make convertion according to cfg
        // processing_and_convert(data_mixed, data_sys, frames, in_port_sys->cfg, out_port->cfg);
        frames_written = do_mixing_2ch(audio_mixer->out_tmp_buffer, data_sys,
                frames, in_port_sys->cfg.format, out_port->cfg.format);
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/sysTmp.raw",
                    audio_mixer->out_tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
        }
        if (adev->is_TV) {
            apply_volume(gain_speaker, audio_mixer->out_tmp_buffer,
                sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
        }
        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/sysvol.raw",
                    audio_mixer->out_tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
        }

        extend_channel_2_8(data_mixed, audio_mixer->out_tmp_buffer, frames, 2, 8);

        if (DEBUG_DUMP) {
            aml_audio_dump_audio_bitstreams("/data/audio/extandsys.raw",
                    data_mixed, frames * out_port->cfg.frame_size);
        }
        in_port_sys->data_valid = 0;
        set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
    }

    if (direct_only) {
        AM_LOGV("direct_only");
        //dirct_vol = get_inport_volume(in_port_drct);
        mixing_len_bytes = in_port_drct->data_len_bytes;
        data_drct = (int16_t *)in_port_drct->data;
        AM_LOGV("direct_only, inport consumed %zu",
                get_inport_consumed_size(in_port_drct));

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
            AM_LOGD("inserting direct_only, need %zu, insert length %zu",
                    in_port_drct->bytes_to_insert, mixing_len_bytes);
            memset(data_mixed, 0, mixing_len_bytes);
            extend_channel_2_8(data_mixed, audio_mixer->out_tmp_buffer,
                    frames, 2, 8);
            in_port_drct->bytes_to_insert -= mixing_len_bytes;
            set_outport_data_avail(out_port, frames * out_port->cfg.frame_size);
        } else {
            AM_LOGV("direct_only, vol %f", dirct_vol);
            frames = mixing_len_bytes / in_port_drct->cfg.frame_size;
            //cpy_16bit_data_with_gain(data_mixed, data_drct,
            //        in_port_drct->data_len_bytes, dirct_vol);
            AM_LOGV("direct_only, frames %zu, bytes %zu", frames, mixing_len_bytes);

            frames_written = do_mixing_2ch(audio_mixer->out_tmp_buffer, data_drct,
                frames, in_port_drct->cfg.format, out_port->cfg.format);
            if (DEBUG_DUMP) {
                aml_audio_dump_audio_bitstreams("/data/audio/dirctTmp.raw",
                        audio_mixer->out_tmp_buffer, frames * FRAMESIZE_32BIT_STEREO);
            }
            if (adev->is_TV) {
                apply_volume(gain_speaker, audio_mixer->out_tmp_buffer,
                    sizeof(uint32_t), frames * FRAMESIZE_32BIT_STEREO);
            }

            extend_channel_2_8(data_mixed, audio_mixer->out_tmp_buffer,
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

static int mixer_add_mixing_data(void *pMixedBuf, void *input, input_port *in_port, output_port *out_port)
{
    if (in_port->data_buf_frame_cnt < MIXER_FRAME_COUNT) {
        AM_LOGE("input port type:%s buf frames:%zu too small",
            mixerInputType2Str(in_port->enInPortType), in_port->data_buf_frame_cnt);
        return -EINVAL;
    }
    int mixing_frames = MIXER_FRAME_COUNT * out_port->cfg.channelCnt / 2;
    do_mixing_2ch(pMixedBuf, input, mixing_frames, in_port->cfg.format, out_port->cfg.format);
    in_port->data_valid = 0;
    AM_LOGV("input port ID:%d  channels:%d", in_port->ID, out_port->cfg.channelCnt);
    return 0;
}

static int mixer_do_mixing_16bit(struct amlAudioMixer *audio_mixer)
{
    bool                        is_data_valid = false;
    input_port                  *in_port = NULL;
    output_port                 *out_port = mixer_get_cur_outport(audio_mixer);
    struct aml_audio_device		*adev = audio_mixer->adev;
    char                        acFilePathStr[ENUM_TYPE_STR_MAX_LEN] = {0};
    uint32_t                    need_output_ch = 2;
    uint32_t                    cur_output_ch = 2;
    uint32_t                    masks = 0;

    R_CHECK_POINTER_LEGAL(-1, out_port, "");
    cur_output_ch = out_port->cfg.channelCnt;
    masks = audio_mixer->inportsMasks;
    while (masks) {
        in_port = mixer_get_inport_by_mask_right_first(audio_mixer, &masks);
        /* If not connected A2DP, and HDMI RX supports multi-channel, so we have multi-channel output.
         * Otherwise the default 2 channel output.
         */
        if (NULL != in_port && in_port->cfg.channelCnt > need_output_ch &&
            in_port->cfg.channelCnt <= adev->hdmi_descs.pcm_fmt.max_channels &&
            !(adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)) {
            need_output_ch = in_port->cfg.channelCnt;
        }
    }

    if (need_output_ch != cur_output_ch) {
        AM_LOGI("output channel change, need_out_ch:%d -> cur_out_ch:%d", need_output_ch, cur_output_ch);
        struct audioCfg cfg;
        output_get_default_config(&cfg);
        cfg.channelCnt = need_output_ch;
        if (need_output_ch > 2 && cur_output_ch == 2) {
            delete_mixer_output_port(audio_mixer, MIXER_OUTPUT_PORT_STEREO_PCM);
            init_mixer_output_port(audio_mixer, MIXER_OUTPUT_PORT_MULTI_PCM, &cfg, MIXER_FRAME_COUNT);
            init_mixer_temp_buffer(audio_mixer);
        } else if (need_output_ch == 2 && cur_output_ch > 2) {
            delete_mixer_output_port(audio_mixer, MIXER_OUTPUT_PORT_MULTI_PCM);
            init_mixer_output_port(audio_mixer, MIXER_OUTPUT_PORT_STEREO_PCM, &cfg, MIXER_FRAME_COUNT);
            init_mixer_temp_buffer(audio_mixer);
        } else {
            AM_LOGW("Number of unsupported channels:%d", need_output_ch);
            return -1;
        }
    }
    memset(audio_mixer->out_tmp_buffer, 0, audio_mixer->tmp_buffer_size);
    masks = audio_mixer->inportsMasks;
    while (masks) {
        in_port = mixer_get_inport_by_mask_right_first(audio_mixer, &masks);
        if (NULL == in_port || 0 == in_port->data_valid) {
            continue;
        }
        is_data_valid = true;
        if (getprop_bool("vendor.media.audiohal.indump")) {
            char acFilePathStr[ENUM_TYPE_STR_MAX_LEN];
            sprintf(acFilePathStr, "/data/audio/%s_%d", mixerInputType2Str(in_port->enInPortType), in_port->ID);
            aml_audio_dump_audio_bitstreams(acFilePathStr, in_port->data, in_port->data_len_bytes);
        }
        if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
            check_audio_level(mixerInputType2Str(in_port->enInPortType), in_port->data, in_port->data_len_bytes);
        }
        if (AML_MIXER_INPUT_PORT_PCM_DIRECT == in_port->enInPortType) {
            if (in_port->is_hwsync && in_port->bytes_to_insert < in_port->data_len_bytes) {
                retrieve_hwsync_header(audio_mixer, in_port, out_port);
            }
            if (in_port->bytes_to_insert >= in_port->data_len_bytes) {
                in_port->bytes_to_insert -= in_port->data_len_bytes;
                AM_LOGD("PCM_DIRECT inport insert mute data, still need %zu, inserted length %zu",
                        in_port->bytes_to_insert, in_port->data_len_bytes);
                continue;
            }
        }

        if (in_port->cfg.channelCnt == need_output_ch) {
            mixer_add_mixing_data(audio_mixer->out_tmp_buffer, in_port->data, in_port, out_port);
        } else {
            int minCh = MIN(in_port->cfg.channelCnt, need_output_ch);
            /* Upmix/Downmix: -1: fill zeros data in output channel. */
            /* Upmix: Filled with zeros and put at the end of each audio frame. */
            /* Downmix: Drop the other channel data. */
            int8_t idxary[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
            for (int i=0; i<minCh; i++) {
                idxary[i] = i;
            }
            memcpy_by_index_array(audio_mixer->in_tmp_buffer, need_output_ch, in_port->data,
                in_port->cfg.channelCnt, idxary, 2, MIXER_FRAME_COUNT);
            mixer_add_mixing_data(audio_mixer->out_tmp_buffer, audio_mixer->in_tmp_buffer, in_port, out_port);
        }
    }
    /* only check the valid on a2dp case, normal alsa output we need continuous output,
     * otherwise it will cause noise at the end
     */
    if (!is_data_valid && (adev->out_device & AUDIO_DEVICE_OUT_ALL_A2DP)) {
        if (adev->debug_flag) {
            AM_LOGI("inport no valid data");
        }
        return -1;
    }

    if (adev->is_TV) {
        apply_volume(adev->sink_gain[OUTPORT_SPEAKER], audio_mixer->out_tmp_buffer, sizeof(uint16_t),
            audio_mixer->tmp_buffer_size);
    }
    memcpy(out_port->data_buf, audio_mixer->out_tmp_buffer, audio_mixer->tmp_buffer_size);
    if (getprop_bool("vendor.media.audiohal.outdump")) {
        sprintf(acFilePathStr, "/data/audio/audio_mixed_%dch", need_output_ch);
        aml_audio_dump_audio_bitstreams(acFilePathStr, out_port->data_buf, audio_mixer->tmp_buffer_size);
    }
    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
        check_audio_level("audio_mixed", out_port->data_buf, audio_mixer->tmp_buffer_size);
    }
    set_outport_data_avail(out_port, audio_mixer->tmp_buffer_size);
    return 0;
}

int notify_mixer_input_avail(struct amlAudioMixer *audio_mixer)
{
    for (uint8_t port_index = 0; port_index < NR_INPORTS; port_index++) {
        input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port && in_port->on_input_avail_cbk)
            in_port->on_input_avail_cbk(in_port->input_avail_cbk_data);
    }

    return 0;
}

int notify_mixer_exit(struct amlAudioMixer *audio_mixer)
{
    for (uint8_t port_index = 0; port_index < NR_INPORTS; port_index++) {
        input_port *in_port = audio_mixer->in_ports[port_index];
        if (in_port && in_port->on_notify_cbk)
            in_port->on_notify_cbk(in_port->notify_cbk_data);
    }

    return 0;
}

#define THROTTLE_TIME_US 3000
static void *mixer_32b_threadloop(void *data)
{
    struct amlAudioMixer *audio_mixer = data;
    int ret = 0;

    AM_LOGI("++start");

    audio_mixer->exit_thread = 0;
    prctl(PR_SET_NAME, "amlAudioMixer32");
    aml_audio_set_cpu23_affinity();
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
            AM_LOGV("data not enough, next turn");
            notify_mixer_input_avail(audio_mixer);
            continue;
            //notify_mixer_input_avail(audio_mixer);
            //continue;
        }
        notify_mixer_input_avail(audio_mixer);
        AM_LOGV("do mixing");
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

    AM_LOGI("--");
    return NULL;
}

uint32_t get_mixer_inport_count(struct amlAudioMixer *audio_mixer)
{
    return __builtin_popcount(audio_mixer->inportsMasks);
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

    AM_LOGI("begin create thread");
    if (audio_mixer->mixing_enable == 0) {
        pthread_exit(0);
        AM_LOGI("mixing_enable is 0 exit thread");
        return NULL;
    }
    audio_mixer->exit_thread = 0;
    prctl(PR_SET_NAME, "amlAudioMixer16");
    aml_audio_set_cpu23_affinity();
    aml_set_thread_priority("amlAudioMixer16", audio_mixer->out_mixer_tid);
    while (!audio_mixer->exit_thread) {
        if (pstVirtualBuffer == NULL) {
            audio_virtual_buf_open((void **)&pstVirtualBuffer, "mixer_16bit_thread",
                    MIXER_WRITE_PERIOD_TIME_NANO * 4, MIXER_WRITE_PERIOD_TIME_NANO * 4, 0);
            audio_virtual_buf_process((void *)pstVirtualBuffer, MIXER_WRITE_PERIOD_TIME_NANO * 4);
        }
        pthread_mutex_lock(&audio_mixer->lock);
        mixer_inports_read(audio_mixer);
        pthread_mutex_unlock(&audio_mixer->lock);

        audio_virtual_buf_process((void *)pstVirtualBuffer, MIXER_WRITE_PERIOD_TIME_NANO);
        pthread_mutex_lock(&audio_mixer->lock);
        notify_mixer_input_avail(audio_mixer);
        mixer_do_mixing_16bit(audio_mixer);
        pthread_mutex_unlock(&audio_mixer->lock);

        if (!is_submix_disable(audio_mixer)) {
            pthread_mutex_lock(&audio_mixer->lock);
            mixer_output_write(audio_mixer);
            mixer_update_tstamp(audio_mixer);
            pthread_mutex_unlock(&audio_mixer->lock);
        }
    }
    if (pstVirtualBuffer != NULL) {
        audio_virtual_buf_close((void **)&pstVirtualBuffer);
    }

    AM_LOGI("exit thread");
    return NULL;
}

uint32_t mixer_get_inport_latency_frames(struct amlAudioMixer *audio_mixer, uint8_t port_index)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return in_port->get_latency_frames(in_port);
}

uint32_t mixer_get_outport_latency_frames(struct amlAudioMixer *audio_mixer)
{
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    R_CHECK_POINTER_LEGAL(0, out_port, "");
    return outport_get_latency_frames(out_port);
}

int pcm_mixer_thread_run(struct amlAudioMixer *audio_mixer)
{
    int ret = 0;
    AM_LOGI("++");
    R_CHECK_POINTER_LEGAL(-EINVAL, audio_mixer, "");
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    R_CHECK_POINTER_LEGAL(-EINVAL, out_port, "not initialized");

    if (audio_mixer->out_mixer_tid > 0) {
        AM_LOGE("out mixer thread already running");
        return -EINVAL;
    }
    audio_mixer->mixing_enable = 1;
    switch (out_port->cfg.format) {
    case AUDIO_FORMAT_PCM_32_BIT:
        ret = pthread_create(&audio_mixer->out_mixer_tid, NULL, mixer_32b_threadloop, audio_mixer);
        break;
    case AUDIO_FORMAT_PCM_16_BIT:
        ret = pthread_create(&audio_mixer->out_mixer_tid, NULL, mixer_16b_threadloop, audio_mixer);
        break;
    default:
        AM_LOGE("format not supported");
        break;
    }
    if (ret < 0) {
        AM_LOGE("thread run failed.");
    }
    AM_LOGI("++mixing_enable:%d, format:%#x", audio_mixer->mixing_enable, out_port->cfg.format);

    return ret;
}

int pcm_mixer_thread_exit(struct amlAudioMixer *audio_mixer)
{
    audio_mixer->mixing_enable = 0;
    AM_LOGI("++ audio_mixer->mixing_enable %d", audio_mixer->mixing_enable);
    // block exit
    audio_mixer->exit_thread = 1;
    pthread_join(audio_mixer->out_mixer_tid, NULL);
    audio_mixer->out_mixer_tid = 0;

    notify_mixer_exit(audio_mixer);
    return 0;
}

struct pcm *pcm_mixer_get_pcm_handle(struct amlAudioMixer *audio_mixer)
{
    output_port *pstOutPort = mixer_get_cur_outport(audio_mixer);
    R_CHECK_POINTER_LEGAL(NULL, pstOutPort, "get pcm handle fail, cur output type:%s",
        mixerOutputType2Str(audio_mixer->cur_output_port_type));
    return pstOutPort->pcm_handle;
}

struct amlAudioMixer *newAmlAudioMixer(struct aml_audio_device *adev)
{
    struct amlAudioMixer *audio_mixer = NULL;
    int ret = 0;
    AM_LOGD("");

    audio_mixer = aml_audio_calloc(1, sizeof(*audio_mixer));
    R_CHECK_POINTER_LEGAL(NULL, audio_mixer, "allocate amlAudioMixer:%d no memory", sizeof(struct amlAudioMixer));
    audio_mixer->adev = adev;
    audio_mixer->submix_standby = 1;
    mixer_set_state(audio_mixer, MIXER_IDLE);
    struct audioCfg cfg;
    output_get_default_config(&cfg);
    ret = init_mixer_output_port(audio_mixer, MIXER_OUTPUT_PORT_STEREO_PCM, &cfg, MIXER_FRAME_COUNT);
    if (ret < 0) {
        AM_LOGE("init mixer out port failed");
        goto err_state;
    }
    init_mixer_temp_buffer(audio_mixer);
    audio_mixer->inportsMasks = 0;
    audio_mixer->supportedInportsMasks = (1 << NR_INPORTS) - 1;
    pthread_mutex_init(&audio_mixer->lock, NULL);
    pthread_mutex_init(&audio_mixer->inport_lock, NULL);
    return audio_mixer;

err_state:
    deinit_mixer_temp_buffer(audio_mixer);
err_tmp:
    aml_audio_free(audio_mixer);
    return NULL;
}

void freeAmlAudioMixer(struct amlAudioMixer *audio_mixer)
{
    if (audio_mixer == NULL) {
        AM_LOGE("audio_mixer NULL pointer");
        return;
    }
    pthread_mutex_destroy(&audio_mixer->lock);
    pthread_mutex_destroy(&audio_mixer->inport_lock);
    if (audio_mixer->cur_output_port_type == MIXER_OUTPUT_PORT_STEREO_PCM ||
        audio_mixer->cur_output_port_type == MIXER_OUTPUT_PORT_MULTI_PCM) {
        delete_mixer_output_port(audio_mixer, audio_mixer->cur_output_port_type);
    }
    deinit_mixer_temp_buffer(audio_mixer);
    free(audio_mixer);
}

int mixer_get_presentation_position(
        struct amlAudioMixer *audio_mixer,
        uint8_t port_index,
        uint64_t *frames,
        struct timespec *timestamp)
{
    int ret = 0;
    R_CHECK_PARAM_LEGAL(-1, port_index, 0, NR_INPORTS, "");
    pthread_mutex_lock(&audio_mixer->inport_lock);
    input_port *in_port = audio_mixer->in_ports[port_index];
    if (in_port == NULL) {
        AM_LOGE("in_port is null pointer, port_index:%d", port_index);
        pthread_mutex_unlock(&audio_mixer->inport_lock);
        return -EINVAL;
    }
    *frames = in_port->presentation_frames;
    *timestamp = in_port->timestamp;
    if (!is_inport_pts_valid(in_port)) {
        AM_LOGW("not valid now");
        ret = -EINVAL;
    }
    pthread_mutex_unlock(&audio_mixer->inport_lock);
    return ret;
}

int mixer_set_padding_size(
        struct amlAudioMixer *audio_mixer,
        uint8_t port_index,
        int padding_bytes)
{
    input_port *in_port = audio_mixer->in_ports[port_index];
    R_CHECK_POINTER_LEGAL(-EINVAL, in_port, "port_index:%d", port_index);
    return set_inport_padding_size(in_port, padding_bytes);
}

int mixer_outport_pcm_restart(struct amlAudioMixer *audio_mixer)
{
    output_port *out_port = mixer_get_cur_outport(audio_mixer);
    R_CHECK_POINTER_LEGAL(-EINVAL, out_port, "");
    outport_pcm_restart(out_port);
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

    while (masks) {
        input_port *in_port = mixer_get_inport_by_mask_right_first(audio_mixer, &masks);
        if (NULL != in_port && in_port->enInPortType == AML_MIXER_INPUT_PORT_PCM_DIRECT
            && in_port->notify_cbk_data) {
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
        dprintf(s32Fd, "[AML_HAL] [%s:%d] struct amlAudioMixer is NULL !\n", __func__, __LINE__);
        return;
    }
    dprintf(s32Fd, "[AML_HAL]---------------input port description cnt: [%d](masks:%#x)---------\n",
        get_mixer_inport_count(pstAudioMixer), pstAudioMixer->inportsMasks);
    for (uint8_t index=0; index < NR_INPORTS; index++) {
        input_port *pstInputPort = pstAudioMixer->in_ports[index];
        if (pstInputPort) {
            dprintf(s32Fd, "[AML_HAL]  input port type: %s(ID:%d)\n", mixerInputType2Str(pstInputPort->enInPortType), pstInputPort->ID);
            dprintf(s32Fd, "[AML_HAL]      Channel       : %10d     | Format            : %#10x\n",
                pstInputPort->cfg.channelCnt, pstInputPort->cfg.format);
            dprintf(s32Fd, "[AML_HAL]      FrameCnt      : %10d     | data size         : %10d Byte\n",
                pstInputPort->data_buf_frame_cnt, pstInputPort->data_len_bytes);
            dprintf(s32Fd, "[AML_HAL]      rbuf size     : %10d Byte| Avail size        : %10d Byte\n",
                pstInputPort->r_buf->size, get_buffer_read_space(pstInputPort->r_buf));
            dprintf(s32Fd, "[AML_HAL]      is_hwsync     : %10d     | start_threshold   : %10d Byte\n",
                pstInputPort->is_hwsync, pstInputPort->inport_start_threshold);
        }
    }
    dprintf(s32Fd, "[AML_HAL]---------------------output port description----------------------\n");
    output_port *pstOutPort = mixer_get_cur_outport(pstAudioMixer);
    if (pstOutPort) {
        dprintf(s32Fd, "[AML_HAL]  output port type: %s\n", mixerOutputType2Str(pstOutPort->enOutPortType));
        dprintf(s32Fd, "[AML_HAL]      Channel       : %10d     | Format            : %#10x\n", pstOutPort->cfg.channelCnt, pstOutPort->cfg.format);
        dprintf(s32Fd, "[AML_HAL]      FrameCnt      : %10d     | data size         : %10d Byte\n",
            pstOutPort->data_buf_frame_cnt, pstOutPort->data_buf_len);
    } else {
        dprintf(s32Fd, "[AML_HAL] not find output port description!!!\n");
    }
}

int mixer_set_karaoke(struct amlAudioMixer *audio_mixer, struct kara_manager *kara)
{
    MIXER_OUTPUT_PORT port_index = MIXER_OUTPUT_PORT_STEREO_PCM;
    output_port *out_port = audio_mixer->out_ports[port_index];

    ALOGI("++%s(), set karaoke = %p", __func__, kara);
    outport_set_karaoke(out_port, kara);

    return 0;
}

