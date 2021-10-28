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

#ifndef _AML_AUDIO_STREAM_H_
#define _AML_AUDIO_STREAM_H_

#include <system/audio.h>
#include <pthread.h>
#include "aml_ringbuffer.h"
#include "audio_hw.h"
#include "audio_hw_profile.h"
#include "audio_dtv_utils.h"
#include "aml_audio_hal_avsync.h"
#include "aml_dtvsync.h"
#define RAW_USECASE_MASK ((1<<STREAM_RAW_DIRECT) | (1<<STREAM_RAW_HWSYNC) | (1<<STREAM_RAW_PATCH))
/*
 * 1.AUDIO_FORMAT_PCM_16_BIT is suitable for Speaker
 * 2.AUDIO_FORMAT_AC3 / AUDIO_FORMAT_E_AC3 / AUDIO_FORMAT_E_AC3_JOC is suitable for Sink device(up to DDP)
 * 3.AUDIO_FORMAT_MAT(1.0-truehd/2.0-pcm/2.1-pcm-atmos) is suitable for Sink device(up to MAT&depvalue=1/3)
 * 4.AUDIO_FORMAT_DTS / AUDIO_FORMAT_DTS_HD is suitable for Sink device(up to dts/dts-hd)
 */
#define IS_EXTERNAL_DECODER_SUPPORT_FORMAT(format) \
    ((format == AUDIO_FORMAT_PCM_16_BIT) ||\
        (format == AUDIO_FORMAT_AC3) ||\
        ((format & AUDIO_FORMAT_E_AC3) == AUDIO_FORMAT_E_AC3) ||\
        ((format & AUDIO_FORMAT_MAT) == AUDIO_FORMAT_MAT) ||\
        (format == AUDIO_FORMAT_DTS) ||\
        (format == AUDIO_FORMAT_DTS_HD))


typedef uint32_t usecase_mask_t;

/* sync with tinymix before TXL */
enum input_source {
    SRC_NA = -1,
    LINEIN  = 0,
    ATV     = 1,
    HDMIIN  = 2,
    SPDIFIN = 3,
    ARCIN   = 4,
};

/* sync with tinymix after auge */
enum auge_input_source {
    TDMIN_A = 0,
    TDMIN_B = 1,
    TDMIN_C = 2,
    SPDIFIN_AUGE = 3,
    PDMIN = 4,
    FRATV = 5,
    TDMIN_LB    = 6,
    LOOPBACK_A  = 7,
    FRHDMIRX    = 8,
    LOOPBACK_B  = 9,
    SPDIFIN_LB  = 10,
    EARCRX_DMAC = 11,
    RESERVED_0  = 12,
    RESERVED_1  = 13,
    RESERVED_2  = 14,
    VAD     = 15,
};

/*
 *@brief get this value by adev_set_parameters(), command is "hdmi_format"
 */
enum digital_format {
    PCM = 0,
    DD = 4,
    AUTO = 5,
    BYPASS = 6
};

enum stream_write_func {
    OUT_WRITE_NEW = 0,
    MIXER_AUX_BUFFER_WRITE_SM = 1,
    MIXER_MAIN_BUFFER_WRITE_SM = 2,
    MIXER_MMAP_BUFFER_WRITE_SM = 3,
    MIXER_AUX_BUFFER_WRITE = 4,
    MIXER_MAIN_BUFFER_WRITE = 5,
    MIXER_APP_BUFFER_WRITE = 6,
    PROCESS_BUFFER_WRITE = 7,

    MIXER_WRITE_FUNC_MAX
};

/**\brief Audio output mode*/
typedef enum {
    AM_AOUT_OUTPUT_STEREO,     /**< Stereo output*/
    AM_AOUT_OUTPUT_DUAL_LEFT,  /**< Left audio output to dual channel*/
    AM_AOUT_OUTPUT_DUAL_RIGHT, /**< Right audio output to dual channel*/
    AM_AOUT_OUTPUT_SWAP,        /**< Swap left and right channel*/
    AM_AOUT_OUTPUT_LRMIX       /**< mix left and right channel*/
} AM_AOUT_OutputMode_t;

enum {
    ATTEND_TYPE_NONE = 0,
    ATTEND_TYPE_ARC,
    ATTEND_TYPE_EARC
};

static inline bool is_main_write_usecase(stream_usecase_t usecase)
{
    return usecase > 0;
}

static inline bool is_digital_raw_format(audio_format_t format)
{
    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_E_AC3_JOC:
    case AUDIO_FORMAT_AC4:
    case AUDIO_FORMAT_MAT:
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
    case AUDIO_FORMAT_DOLBY_TRUEHD:
    case AUDIO_FORMAT_IEC61937:
        return true;
    default:
        return false;
    }
}

static inline bool is_dolby_format(audio_format_t format) {
    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3:
    case AUDIO_FORMAT_E_AC3_JOC:
    case AUDIO_FORMAT_DOLBY_TRUEHD:
        return true;
    default:
        return false;
    }
}

inline bool is_dts_format(audio_format_t format) {
    switch (format) {
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD:
    ///< audio_format_t does not include dts_express. So we get the format(dts_express especially) from the decoder.
    // case AUDIO_FORMAT_DTS_EXPRESS:
        return true;
    default:
        return false;
    }
}

static inline stream_usecase_t attr_to_usecase(uint32_t devices __unused,
        audio_format_t format, uint32_t flags)
{
    // hwsync case
    if ((flags & AUDIO_OUTPUT_FLAG_HW_AV_SYNC) && (format != AUDIO_FORMAT_IEC61937)
        && !(flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ)) {
        if (audio_is_linear_pcm(format)) {
            return STREAM_PCM_HWSYNC;
        } else if (is_digital_raw_format(format)) {
            return STREAM_RAW_HWSYNC;
        } else {
            return STREAM_USECASE_MAX;
        }
    }

    // non hwsync cases
    if (/*devices == AUDIO_DEVICE_OUT_HDMI ||*/
        is_digital_raw_format(format)) {
        return STREAM_RAW_DIRECT;
    } else if ((flags & AUDIO_OUTPUT_FLAG_DIRECT) && audio_is_linear_pcm(format)) {
        if (flags & AUDIO_OUTPUT_FLAG_MMAP_NOIRQ) {
            // AAudio case
            return STREAM_PCM_MMAP;
        } else {
            //multi-channel LPCM or hi-res LPCM
            return STREAM_PCM_DIRECT;
        }
    } else {
        return STREAM_PCM_NORMAL;
    }
}
static inline stream_usecase_t convert_usecase_mask_to_stream_usecase(usecase_mask_t mask)
{
    int i = 0;
    for (i = 0; i < STREAM_USECASE_MAX; i++) {
        if ((1 << i) & mask) {
            break;
        }
    }
    ALOGI("%s mask %#x i %d", __func__, mask, i);
    if (i >= STREAM_USECASE_MAX) {
        return STREAM_USECASE_MAX;
    } else {
        return (stream_usecase_t)i;
    }
}

static inline alsa_device_t usecase_to_device(stream_usecase_t usecase)
{
    switch (usecase) {
    case STREAM_PCM_NORMAL:
    case STREAM_PCM_DIRECT:
    case STREAM_PCM_HWSYNC:
    case STREAM_PCM_PATCH:
        return I2S_DEVICE;
    case STREAM_RAW_PATCH:
    case STREAM_RAW_DIRECT:
    case STREAM_RAW_HWSYNC:
        return DIGITAL_DEVICE;
    default:
        return I2S_DEVICE;
    }
}

static inline bool is_hdmi_out(enum OUT_PORT active_outport) {
    return (active_outport == OUTPORT_HDMI_ARC || active_outport == OUTPORT_HDMI);
}

typedef void (*dtv_avsync_process_cb)(struct aml_audio_patch* patch,struct aml_stream_out* stream_out);


/* all latency in unit 'ms' */
struct audio_patch_latency_detail {
    unsigned int ringbuffer_latency;
    unsigned int user_tune_latency;
    unsigned int alsa_in_latency;
    unsigned int alsa_i2s_out_latency;
    unsigned int alsa_spdif_out_latency;
    unsigned int ms12_latency;
    unsigned int total_latency;
};

struct aml_audio_patch {
    struct audio_hw_device *dev;
    ring_buffer_t aml_ringbuffer;
    ring_buffer_t tvin_ringbuffer;
    ring_buffer_t assoc_ringbuffer;
    pthread_t audio_input_threadID;
    pthread_t audio_cmd_process_threadID;
    pthread_t audio_output_threadID;
    pthread_t audio_parse_threadID;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    void *in_buf;
    size_t in_buf_size;
    size_t numDecodedSamples;
    size_t numOutputSamples;
    void *out_buf;
    size_t out_buf_size;
    void *out_tmpbuf;
    size_t out_tmpbuf_size;
    int tvin_buffer_inited;
    int assoc_buffer_inited;
    int cmd_process_thread_exit;
    int input_thread_exit;
    int output_thread_exit;
    void *audio_parse_para;
    audio_devices_t input_src;
    audio_format_t  aformat;
    int  sample_rate;
    int input_sample_rate;
    audio_channel_mask_t chanmask;
    audio_channel_mask_t in_chanmask;
    int in_sample_rate;
    audio_format_t in_format;

    audio_devices_t output_src;
    bool is_dtv_src;
    audio_channel_mask_t out_chanmask;
    int out_sample_rate;
    audio_format_t out_format;
    /*start play strategy*/
    int startplay_avsync_flag;
    unsigned long startplay_firstvpts;
    unsigned long startplay_first_checkinapts;
    unsigned long startplay_pcrpts;
    unsigned long startplay_apts_lookup;
    unsigned int startplay_vpts;

#if 0
    struct ring_buffer
    struct thread_read
    struct thread_write
    struct audio_mixer;
    void *output_process_buf;
    void *mixed_buf;
#endif

    /* for AVSYNC tuning */
    int vltcy;
    int altcy;
    int average_vltcy;
    int average_altcy;
    int avsync_sample_accumed;
    int max_video_latency;
    int min_video_latency;
    bool need_do_avsync;
    bool input_signal_stable;
    bool is_avsync_start;
    bool skip_frames;
    int timeout_avsync_cnt;
    bool game_mode;
    struct audio_patch_latency_detail audio_latency;
    /* end of AVSYNC tuning */
    /*for dtv play parameters */
    int dtv_aformat;
    int dtv_has_video;
    int dtv_decoder_state;
    int dtv_decoder_cmd;
    int dtv_first_apts_flag; /*first apts looked up flag*/
    unsigned char dtv_NchOriginal;
    unsigned char dtv_lfepresent;
    unsigned int dtv_first_apts;
    unsigned int dtv_pcm_writed;
    unsigned int dtv_pcm_readed;
    unsigned int dtv_decoder_ready;
    unsigned int input_thread_created;
    unsigned int ouput_thread_created;
    unsigned int decoder_offset ;
    unsigned int outlen_after_last_validpts;
    unsigned long last_valid_pts;
    unsigned int first_apts_lookup_over; /*cache audio data before start-play flag*/
    int dtv_symple_rate;
    int dtv_pcm_channel;
    bool dtv_replay_flag;  //set for the first play
    unsigned int dtv_output_clock;
    unsigned int dtv_default_i2s_clock;
    unsigned int dtv_default_spdif_clock;
    unsigned int spdif_format_set;
    int spdif_step_clk;
    int i2s_step_clk;
    int dtv_audio_mode;
    int tsync_mode;
    int dtv_apts_lookup;
    int dtv_audio_tune;
    int pll_state;
    unsigned int last_chenkin_apts;
    unsigned int last_apts;
    unsigned int last_pcrpts;
    unsigned int cur_outapts;
    unsigned int anchor_apts;
    unsigned int show_first_frame;
    dtv_avsync_process_cb avsync_callback;
    pthread_mutex_t dtv_output_mutex;
    pthread_mutex_t dtv_input_mutex;
    pthread_cond_t  dtv_cmd_process_cond;
    pthread_mutex_t dtv_cmd_process_mutex;
    pthread_mutex_t assoc_mutex;
    pthread_mutex_t apts_cal_mutex;
    /*end dtv play*/
    struct resample_para dtv_resample;
    unsigned char *resample_outbuf;
    AM_AOUT_OutputMode_t   mode;
    bool ac3_pcm_dropping;
    int last_audio_delay;
    //add only for debug.
    int dtv_log_retry_cnt;
    unsigned int last_apts_record;
    unsigned int last_vpts_record;
    unsigned int last_pcrpts_record;
    struct timespec last_debug_record;
    bool pcm_inserting;
    int tsync_pcr_debug;
    int pre_latency;
    bool ad_substream_checked_flag;
    int a_discontinue_threshold;
    struct avsync_para  sync_para;
    int pid;
    int i2s_div_factor;
    struct timespec speed_time;
    struct timespec slow_time;
    struct audiohal_debug_para debug_para;
    int media_sync_id;
    struct mAudioEsDataInfo *mADEsData;
    void *demux_handle;
    void *demux_info;
    aml_dtvsync_t *dtvsync;
    int uio_fd;
    struct cmd_node *dtv_cmd_list;
    void *dtv_package_list;
    struct package *cur_package;
    bool skip_amadec_flag;
};

struct audio_stream_out;
struct audio_stream_in;

stream_usecase_t convert_usecase_mask_to_stream_usecase(usecase_mask_t mask);

/*
 *@brief get sink format by logic min(source format / digital format / sink capability)
 * For Speaker/Headphone output, sink format keep PCM-16bits
 * For optical output, min(dd, source format, digital format)
 * For HDMI_ARC output
 *      1.digital format is PCM, sink format is PCM-16bits
 *      2.digital format is dd, sink format is min (source format,  AUDIO_FORMAT_AC3)
 *      3.digital format is auto, sink format is min (source format, digital format)
 */
void get_sink_format(struct audio_stream_out *stream);

/*@brief check the hdmi rx audio stability by HW register */
bool is_hdmi_in_stable_hw(struct audio_stream_in *stream);
/*@brief check the hdmix rx audio format stability by SW parser */
bool is_hdmi_in_stable_sw(struct audio_stream_in *stream);
/*@brief check the ATV audio stability by HW register */
bool is_atv_in_stable_hw(struct audio_stream_in *stream);
int set_audio_source(struct aml_mixer_handle *mixer_handle,
        enum input_source audio_source, bool is_auge);
int enable_HW_resample(struct aml_mixer_handle *mixer_handle, int enable_sr);
bool Stop_watch(struct timespec start_ts, int64_t time);
bool signal_status_check(audio_devices_t in_device, int *mute_time,
                         struct audio_stream_in *stream);

int set_resample_source(struct aml_mixer_handle *mixer_handle, enum ResampleSource source);
int set_spdifin_pao(struct aml_mixer_handle *mixer_handle,int enable);

/*
 *@brief clean the tmp_buffer_8ch&audioeffect_tmp_buffer and release audio stream
 */
void  release_audio_stream(struct audio_stream_out *stream);
/*@brief check the AV audio stability by HW register */
bool is_av_in_stable_hw(struct audio_stream_in *stream);
bool is_dual_output_stream(struct audio_stream_out *stream);
int get_spdifin_samplerate(struct aml_mixer_handle *mixer_handle);
int get_hdmiin_samplerate(struct aml_mixer_handle *mixer_handle);
int get_hdmiin_channel(struct aml_mixer_handle *mixer_handle);
hdmiin_audio_packet_t get_hdmiin_audio_packet(struct aml_mixer_handle *mixer_handle);

/* dumpsys media.audio_flinger interfaces */
const char *audio_port_role_to_str(audio_port_role_t role);
const char *audio_port_type_to_str(audio_port_type_t type);
void aml_stream_out_dump(struct aml_stream_out *aml_out, int fd);
void aml_audio_port_config_dump(struct audio_port_config *port_config, int fd);
void aml_audio_patch_dump(struct audio_patch *patch, int fd);
void aml_audio_patches_dump(struct aml_audio_device* aml_dev, int fd);
int aml_dev_dump_latency(struct aml_audio_device *aml_dev, int fd);
void audio_patch_dump(struct aml_audio_device* aml_dev, int fd);
bool is_use_spdifb(struct aml_stream_out *out);
bool is_dolby_ms12_support_compression_format(audio_format_t format);
bool is_dolby_ddp_support_compression_format(audio_format_t format);
bool is_direct_stream_and_pcm_format(struct aml_stream_out *out);
bool is_mmap_stream_and_pcm_format(struct aml_stream_out *out);
void get_audio_indicator(struct aml_audio_device *dev, char *temp_buf);
void update_audio_format(struct aml_audio_device *adev, audio_format_t format);
int audio_route_set_hdmi_arc_mute(struct aml_mixer_handle *mixer_handle, int enable);
int audio_route_set_spdif_mute(struct aml_mixer_handle *mixer_handle, int enable);
int reconfig_read_param_through_hdmiin(struct aml_audio_device *aml_dev,
                                       struct aml_stream_in *stream_in,
                                       ring_buffer_t *ringbuffer, int buffer_size);
int input_stream_channels_adjust(struct audio_stream_in *stream, void* buffer, size_t bytes);


/*
 *@brief update the sink format after HDMI/HDMI-ARC hot pluged
 * return zero if success.
 */
int update_sink_format_after_hotplug(struct aml_audio_device *adev);

int stream_check_reconfig_param(struct audio_stream_out *stream);


void create_tvin_buffer(struct aml_audio_patch *patch);
void release_tvin_buffer(struct aml_audio_patch *patch);
void tv_in_write(struct audio_stream_out *stream, const void* buffer, size_t bytes);
int tv_in_read(struct audio_stream_in *stream, void* buffer, size_t bytes);
int set_tv_source_switch_parameters(struct audio_hw_device *dev, struct str_parms *parms);
void tv_do_ease_out(struct aml_audio_device *aml_dev);

/*
*@breif check tv signal need to mute or not
* return false if signal need to mute
*/
bool check_tv_stream_signal (struct audio_stream_in *stream);

/*
 * @breif set HDMIIN audio mode: "SPDIF", "I2S", "TDM"
 * return negative if fails.
 */
int set_hdmiin_audio_mode(struct aml_mixer_handle *mixer_handle, char *mode);

enum hdmiin_audio_mode {
    HDMIIN_MODE_SPDIF = 0,
    HDMIIN_MODE_I2S   = 1,
    HDMIIN_MODE_TDM   = 2
};
enum hdmiin_audio_mode get_hdmiin_audio_mode(struct aml_mixer_handle *mixer_handle);

#endif /* _AML_AUDIO_STREAM_H_ */
