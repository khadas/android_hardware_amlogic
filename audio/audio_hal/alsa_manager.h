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

#ifndef _ALSA_MANAGER_H_
#define _ALSA_MANAGER_H_

#include <hardware/audio.h>

typedef enum info_type {
    OUTPUT_INFO_STATUS,      // running or xrun etc..
    OUTPUT_INFO_DELAYFRAME,  // the delay frames
} alsa_info_type_t;

typedef union output_info {
    int delay_ms;
} alsa_output_info_t;

typedef struct aml_stream_config {
    struct audio_config config;
} aml_stream_config_t;

typedef struct aml_device_config {
    uint32_t device_port;

} aml_device_config_t;

typedef enum AML_AUDIO_OUT_DEV_TYPE{
    AML_AUDIO_OUT_DEV_TYPE_SPEAKER                  = 0,
    AML_AUDIO_OUT_DEV_TYPE_SPDIF                    = 1,
    AML_AUDIO_OUT_DEV_TYPE_HEADPHONE                = 2,
    AML_AUDIO_OUT_DEV_TYPE_OTHER                    = 3,

    AML_AUDIO_OUT_DEV_TYPE_BUTT                     = 4,
} aml_audio_out_dev_type_e;

/* 0: alsa auge. 1: alsa non auge. */
/* speaker, spdif, headphone, other. refer aml_audio_out_dev_type_e.*/
extern aml_audio_out_dev_type_e alsa_out_ch_mask[2][AML_AUDIO_OUT_DEV_TYPE_BUTT];

/**
 * pcm open with configs in streams: card, device, pcm_config
 * If device has been opened, close it and reopen with new params
 * and increase the refs count
 */
int aml_alsa_output_open(struct audio_stream_out *stream);

/**
 * decrease the pcm refs count and do pcm close when refs count equals zero.
 */
void aml_alsa_output_close(struct audio_stream_out *stream);
/**
 * pcm_write to the pcm handle saved in stream instance.
 */
size_t aml_alsa_output_write(struct audio_stream_out *stream,
                        void *buffer,
                        size_t bytes);
/**
 * pause the pcm handle saved in stream instance.
 */
int aml_alsa_output_pause(struct audio_stream_out *stream);
/**
 * resume the pcm handle saved in stream instance.
 */
int aml_alsa_output_resume(struct audio_stream_out *stream);
/**
 * stop the pcm handle saved in stream instance.
 */
int aml_alsa_output_stop(struct audio_stream_out *stream);

/**
 * get the stream latency.
 */
int aml_alsa_output_get_latency(struct audio_stream_out *stream);

/*
 *@brief close continuous audio device
 */
void aml_close_continuous_audio_device(struct audio_hw_device *dev);

/**
 * pcm_read to the pcm handle saved in stream instance.
 */
size_t aml_alsa_input_read(struct audio_stream_in *stream,
                        void *buffer,
                        size_t bytes);
int aml_alsa_input_flush(struct audio_stream_in *stream);

int aml_alsa_output_open_new(void **handle, aml_stream_config_t * stream_config, aml_device_config_t *device_config);
void aml_alsa_output_close_new(void *handle);
size_t aml_alsa_output_write_new(void *handle, const void *buffer, size_t bytes);
int aml_alsa_output_getinfo(void *handle, alsa_info_type_t type, alsa_output_info_t * info);
int aml_alsa_output_pause_new(void *handle);
int aml_alsa_output_resume_new(void *handle);
int aml_alsa_output_stop_new(void *handle);
int aml_alsa_output_data_handle(void *handle, void *output_buffer, size_t size, int vaule, bool is_mute);

void alsa_out_reconfig_params(struct audio_stream_out *stream);
enum pcm_format convert_audio_format_2_alsa_format(audio_format_t format);
#endif // _ALSA_MANAGER_H_
