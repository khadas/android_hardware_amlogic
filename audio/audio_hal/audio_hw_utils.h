/*
 * Copyright (C) 2010 Amlogic Corporation.
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



#ifndef  _AUDIO_HW_UTILS_H_
#define _AUDIO_HW_UTILS_H_
#include <system/audio.h>
#include "audio_hw.h"
#include "audio_hw_dtv.h"
#include "aml_audio_types_def.h"
#include "aml_audio_stream.h"
#include "MediaSyncInterface.h"
#include "aml_dump_debug.h"
#define ENUM_TYPE_STR_MAX_LEN                           (100)
#define REPORT_DECODED_INFO  "/sys/class/amaudio/codec_report_info"


#define ENUM_TYPE_TO_STR_DEFAULT_STR            "INVALID_ENUM"
#define ENUM_TYPE_TO_STR_START(prefix)                      \
    const char *pStr = ENUM_TYPE_TO_STR_DEFAULT_STR;        \
    int prefixLen = strlen(prefix);                         \
    switch (type) {
#define ENUM_TYPE_TO_STR(x)                                 \
    case x:                                                 \
        pStr = #x;                                          \
        pStr += prefixLen;                                  \
        if (strlen(#x) - prefixLen > 70) {                  \
            pStr += 70;                                     \
        }                                                   \
        break;
#define ENUM_TYPE_TO_STR_END                                \
    default:                                                \
        break;                                              \
    }                                                       \
    return pStr;

#define MIN(a,b) (((a) < (b)) ? (a) : (b))

#define AM_LOGV(fmt, ...)  ALOGV("[%s:%d] " fmt, __func__,__LINE__, ##__VA_ARGS__)
#define AM_LOGD(fmt, ...)  ALOGD("[%s:%d] " fmt, __func__,__LINE__, ##__VA_ARGS__)
#define AM_LOGI(fmt, ...)  ALOGI("[%s:%d] " fmt, __func__,__LINE__, ##__VA_ARGS__)
#define AM_LOGW(fmt, ...)  ALOGW("[%s:%d] " fmt, __func__,__LINE__, ##__VA_ARGS__)
#define AM_LOGE(fmt, ...)  ALOGE("[%s:%d] " fmt, __func__,__LINE__, ##__VA_ARGS__)

#define R_CHECK_RET(ret, fmt, ...)                                                              \
    if (ret != 0) {                                                                             \
        AM_LOGE("ret:%d " fmt, ret, ##__VA_ARGS__);                                             \
        return ret;                                                                             \
    }

#define NO_R_CHECK_RET(ret, fmt, ...)                                                           \
    if (ret != 0) {                                                                             \
        AM_LOGE("ret:%d " fmt, ret, ##__VA_ARGS__);                                             \
    }

#define R_CHECK_PARAM_LEGAL(ret, param, min, max, fmt, ...)                                     \
    if (param < min || param > max) {                                                           \
        AM_LOGE("%s:%d is illegal, min:%d, max:%d " fmt, #param, param, min, max, ##__VA_ARGS__);\
        return ret;                                                                             \
    }

#define R_CHECK_POINTER_LEGAL(ret, pointer, fmt, ...)                                           \
    if (pointer == NULL) {                                                                      \
        AM_LOGE("%s is null pointer " fmt, #pointer, ##__VA_ARGS__);                            \
        return ret;                                                                             \
    }



int64_t aml_gettime(void);
int get_sysfs_uint(const char *path, uint *value);
int sysfs_set_sysfs_str(const char *path, const char *val);
int set_sysfs_int(const char *path, int value);
int get_sysfs_int(const char *path);
int mystrstr(char *mystr, char *substr) ;
void set_codec_type(int type);
int get_codec_type(int format);
int getprop_bool(const char *path);
unsigned char codec_type_is_raw_data(int type);
int mystrstr(char *mystr, char *substr);
void *convert_audio_sample_for_output(int input_frames, int input_format, int input_ch, void *input_buf, int *out_size/*,float lvol*/);
int  aml_audio_start_trigger(void *stream);
int check_chip_name(char *name, unsigned int length);
int is_multi_demux();
int aml_audio_get_debug_flag();
int aml_audio_get_default_alsa_output_ch();
int aml_audio_debug_set_optical_format();
int aml_audio_dump_audio_bitstreams(const char *path, const void *buf, size_t bytes);
int aml_audio_get_arc_latency_offset(int format);
int aml_audio_get_ddp_latency_offset(int aformat,  bool dual_spdif);
int aml_audio_get_pcm_latency_offset(int format, bool is_netflix, stream_usecase_t usecase);
int aml_audio_get_hwsync_latency_offset(bool b_raw);
int aml_audio_get_ddp_frame_size();
bool is_stream_using_mixer(struct aml_stream_out *out);
uint32_t out_get_outport_latency(const struct audio_stream_out *stream);
uint32_t out_get_latency_frames(const struct audio_stream_out *stream);
int aml_audio_get_spdif_tuning_latency(void);
int aml_audio_get_arc_tuning_latency(audio_format_t arc_afmt);
int aml_audio_get_src_tune_latency(enum patch_src_assortion patch_src);
int sysfs_get_sysfs_str(const char *path, char *val, int len);
void audio_fade_func(void *buf,int fade_size,int is_fadein);
void ts_wait_time_us(struct timespec *ts, uint32_t time_us);
int cpy_16bit_data_with_gain(int16_t *dst, int16_t *src, int size_in_bytes, float vol);
uint64_t get_systime_ns(void);
int aml_audio_get_hdmi_latency_offset(audio_format_t source_format,
	                                  audio_format_t sink_format,int ms12_enable);
int aml_audio_get_latency_offset(enum OUT_PORT port,audio_format_t source_format,
	                                  audio_format_t sink_format,int ms12_enable);
uint32_t tspec_diff_to_us(struct timespec tval_old,
        struct timespec tval_new);
int aml_audio_get_dolby_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost);
int aml_audio_get_dolby_dap_drc_mode(int *drc_mode, int *drc_cut, int *drc_boost);
void aml_audio_set_cpu23_affinity();
void * aml_audio_get_muteframe(audio_format_t output_format, int * frame_size, int bAtmos);
void aml_audio_switch_output_mode(int16_t *buf, size_t bytes, AM_AOUT_OutputMode_t mode);
bool aml_audio_data_detect(int16_t *buf, size_t bytes, int detect_value);
int aml_audio_data_handle(struct audio_stream_out *stream, const void* buffer, size_t bytes);
int aml_audio_compensate_video_delay( int enable);
int aml_audio_get_ms12_timestamp_offset(void);
int aml_audio_delay_timestamp(struct timespec *timestamp, int delay_time_ms);
int halformat_convert_to_spdif(audio_format_t format, int ch_mask);
int alsa_device_get_port_index(alsa_device_t alsa_device);
int aml_set_thread_priority(char *pName, pthread_t threadId);
uint32_t out_get_alsa_latency_frames(const struct audio_stream_out *stream);
bool is_multi_channel_pcm(struct audio_stream_out *stream);
bool is_high_rate_pcm(struct audio_stream_out *stream);
bool is_disable_ms12_continuous(struct audio_stream_out *stream);
int find_offset_in_file_strstr(char *mystr, char *substr);
float aml_audio_get_s_gain_by_src(struct aml_audio_device *adev, enum patch_src_assortion type);
int android_dev_convert_to_hal_dev(audio_devices_t android_dev, int *hal_dev_port);
int android_fmt_convert_to_dmx_fmt(audio_format_t andorid_fmt);
enum patch_src_assortion android_input_dev_convert_to_hal_patch_src(audio_devices_t android_dev);
enum input_source android_input_dev_convert_to_hal_input_src(audio_devices_t android_dev);

const char* patchSrc2Str(enum patch_src_assortion type);
const char* usecase2Str(stream_usecase_t type);
const char* outputPort2Str(enum OUT_PORT type);
const char* inputPort2Str(enum IN_PORT type);
const char* mixerInputType2Str(aml_mixer_input_port_type_e type);
const char* mediasyncAudiopolicyType2Str(audio_policy type);
const char* dtvAudioPatchCmd2Str(AUDIO_DTV_PATCH_CMD_TYPE type);
const char* hdmiFormat2Str(AML_HDMI_FORMAT_E type);
bool aml_audio_check_sbr_product();
void check_audio_level(const char *name, const void *buffer, size_t bytes);

int aml_audio_trace_int(char *name, int value);
int aml_audio_trace_debug_level(void);

/** convert the audio input format to in buffer's period multi coefficient.
 * @return period multi coefficient(1/4/16)
 */
int convert_audio_format_2_period_mul(audio_format_t format);

static inline void endian16_convert(void *buf, int size)
{
    int i;
    unsigned short *p = (unsigned short *)buf;
    for (i = 0; i < size / 2; i++, p++) {
        *p = ((*p & 0xff) << 8) | ((*p) >> 8);
    }
}

#endif
