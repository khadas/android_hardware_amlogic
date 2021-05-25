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

#ifndef _AUDIO_PORT_H_
#define _AUDIO_PORT_H_

#include <system/audio.h>
#include <tinyalsa/asoundlib.h>
#include <cutils/list.h>
#include <alsa_device_profile.h>

#include "hw_avsync.h"
#include "sub_mixing_factory.h"
#include "karaoke_manager.h"

/* Max number of pcm mixing ports */
#define NR_INPORTS    (8)

typedef enum {
    IDLE,
    ACTIVE,
    STOPPED,
    FLUSHING,
    FLUSHED,
    RESUMING,
    PAUSING,
    PAUSING_1, //pausing and easing
    PAUSED,
} port_state;

typedef enum {
    MSG_PAUSE,
    MSG_FLUSH,
    MSG_RESUME,
    MSG_CNT
} PORT_MSG;
const char *port_msg_to_str(PORT_MSG msg);

typedef enum AML_MIXER_INPUT_PORT_TYPE{
    AML_MIXER_INPUT_PORT_INVAL          = -1,
    AML_MIXER_INPUT_PORT_PCM_SYSTEM     = 0,
    AML_MIXER_INPUT_PORT_PCM_DIRECT     = 1,
    AML_MIXER_INPUT_PORT_PCM_MMAP       = 2,
    //AML_MIXER_INPUT_PORT_BITSTREAM_RAW  = 3,

    AML_MIXER_INPUT_PORT_BUTT           = 3,
} aml_mixer_input_port_type_e;

struct fade_out {
    float vol;
    float target_vol;
    int fade_size;
    int sample_size;
    int channel_cnt;
    //int frame_size;
    float stride;
};

typedef struct {
    PORT_MSG msg_what;
    struct listnode list;
} port_message;

typedef int (*meta_data_cbk_t)(void *cookie,
            uint64_t offset,
            struct hw_avsync_header *header,
            int *diff_ms);


typedef struct INPUT_PORT {
    aml_mixer_input_port_type_e enInPortType;
    // flags for extra mixing port which duplicates to the basic port type
    unsigned int ID;
    struct audioCfg cfg;
    struct ring_buffer *r_buf;              /* input port ring buffer. */
    char *data;                             /* input port temp buffer. */
    size_t data_buf_frame_cnt;              /* input port temp buffer, data frames for one cycle. */
    size_t data_len_bytes;                  /* input port temp buffer, data size for one cycle. */
    int64_t buffer_len_ns;                   /* input port temp buffer, input buffer size, the unit is ns. */

    int data_valid;
    size_t bytes_to_insert;                 /* insert 0 data count index. Units: Byte */
    size_t bytes_to_skip;                   /* drop data count index. Units: Byte */
    bool is_hwsync;
    size_t consumed_bytes;
    port_state port_status;
    ssize_t (*write)(struct INPUT_PORT *port, const void *buffer, int bytes);
    ssize_t (*read)(struct INPUT_PORT *port, void *buffer, int bytes);
    uint32_t (*get_latency_frames)(struct INPUT_PORT *port);
    int (*rbuf_avail)(struct INPUT_PORT *port);
    void *notify_cbk_data;
    int (*on_notify_cbk)(void *data);
    void *input_avail_cbk_data;
    int (*on_input_avail_cbk)(void *data);
    void *meta_data_cbk_data;

    meta_data_cbk_t meta_data_cbk;
    float volume;
    struct fade_out fout;
    struct listnode msg_list;
    pthread_mutex_t msg_lock;
    struct timespec timestamp;
    /* get from out stream when init */
    uint64_t initial_frames;
    /* consumed by read after init */
    uint64_t mix_consumed_frames;
    uint64_t presentation_frames;
    int padding_frames;
    bool pts_valid;
    bool        first_read;
    int         inport_start_threshold;
} input_port;

typedef enum {
    MIXER_OUTPUT_PORT_INVAL         = -1,
    MIXER_OUTPUT_PORT_STEREO_PCM    = 0,
    MIXER_OUTPUT_PORT_MULTI_PCM     = 1,
    //MIXER_OUTPUT_PORT_BITSTREAM_RAW = 1,
    MIXER_OUTPUT_PORT_NUM           = 2,
} MIXER_OUTPUT_PORT;

typedef struct OUTPUT_PORT {
    MIXER_OUTPUT_PORT enOutPortType;
    struct audioCfg cfg;
    // data buf to hold tmp out data
    char *data_buf;
    size_t buf_frames;
    size_t frames_avail;
    size_t bytes_avail;
    size_t data_buf_frame_cnt;
    size_t data_buf_len;
    struct pcm *pcm_handle;
    port_state port_status;
    struct pcm *loopback_handle;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    ssize_t (*write)(struct OUTPUT_PORT *port, void *buffer, int bytes);
    int (*start)(struct OUTPUT_PORT *port);
    int (*standby)(struct OUTPUT_PORT *port);
    struct timespec tval_last;
    int sound_track_mode;
    /* pcm device need to stop/start to enable same source */
    bool pcm_restart;
    int dummy;
#ifdef ENABLE_AEC_APP
    struct aec_t *aec;
#endif
    struct kara_manager *kara;
} output_port;

bool is_inport_valid(aml_mixer_input_port_type_e index);
bool is_outport_valid(MIXER_OUTPUT_PORT index);

aml_mixer_input_port_type_e get_input_port_type(struct audio_config *config,
        audio_output_flags_t flags);

input_port *new_input_port(
        size_t buf_size,
        struct audio_config *config,
        audio_output_flags_t flags,
        float volume,
        bool direct_on);
int set_inport_padding_size(input_port *port, size_t bytes);
int reset_input_port(input_port *port);
int resize_input_port_buffer(input_port *port, uint buf_size);
int free_input_port(input_port *port);
int set_port_notify_cbk(input_port *port,
        int (*on_notify_cbk)(void *data), void *data);
int set_port_input_avail_cbk(input_port *port,
        int (*on_input_avail_cbk)(void *data), void *data);
int set_port_meta_data_cbk(input_port *port,
        meta_data_cbk_t meta_data_cbk,
        void *data);
int send_inport_message(input_port *port, PORT_MSG msg);
port_message *get_inport_message(input_port *port);
int remove_inport_message(input_port *port, port_message *p_msg);
int remove_all_inport_messages(input_port *port);

int set_inport_state(input_port *port, port_state status);
port_state get_inport_state(input_port *port);
void set_inport_hwsync(input_port *port);
bool is_inport_hwsync(input_port *port);
void set_inport_volume(input_port *port, float vol);
float get_inport_volume(input_port *port);
size_t get_inport_consumed_size(input_port *port);
int inport_buffer_level(input_port *port);
int output_get_default_config(struct audioCfg *cfg);
int output_get_alsa_config(output_port *out_port, struct pcm_config *alsa_config);

output_port *new_output_port(
        MIXER_OUTPUT_PORT port_index,
        struct audioCfg *config,
        size_t buf_frames);

int free_output_port(output_port *port);
int resize_output_port_buffer(output_port *port, size_t buf_frames);
int outport_get_latency_frames(output_port *port);
int set_inport_pts_valid(input_port *in_port, bool valid);
bool is_inport_pts_valid(input_port *in_port);
void outport_pcm_restart(output_port *port);
int outport_stop_pcm(output_port *port);
int outport_set_dummy(output_port *port, bool en);

/* set karaoke to audio port */
int outport_set_karaoke(output_port *port, struct kara_manager *kara);

#endif /* _AUDIO_PORT_H_ */
