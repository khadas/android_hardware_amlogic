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

#ifndef _AML_AVSYNC_TUNING_H_
#define _AML_AVSYNC_TUNING_H_

/* FIXME: add more SAMPLERATE and CHANNEL COUNT support */
#define SAMPLE_RATE_MS (48)
#define CHANNEL_CNT (2)
#define FRAME_SIZE (2)

/* check avsync latency times*/
#define AVSYNC_SAMPLE_MAX_CNT (3)
#define AVSYNC_ALSA_OUT_MAX_LATENCY (60)
#define AVSYNC_ALSA_OUT_MAX_LATENCY_ARC (50)
#define AVSYNC_RINGBUFFER_MIN_LATENCY (20)
#define AVSYNC_SKIP_CNT (0)

#define MAT_MULTIPLIER 16
#define MS12_DECODER_LATENCY 32
#define MS12_ENCODER_LATENCY 32
#define MS12_PIPELINE_LATENCY 6
#define MS12_DD_DDP_BUFERR_LATENCY 10
#define MS12_MAT_BUFERR_LATENCY 10
#define MS12_DAP_LATENCY 0
#define AVR_LATENCY (60)
#define AVR_LATENCY_PCM (10)

struct aml_audio_patch;
struct aml_audio_device;

int calc_frame_to_latency(int frames, audio_format_t format);
int aml_dev_try_avsync(struct aml_audio_patch *patch);
int tuning_spker_latency(struct aml_audio_device *adev,
                         int16_t *sink_buffer, int16_t *src_buffer, size_t bytes);
int aml_dev_sample_audio_path_latency(struct aml_audio_device *aml_dev, char *latency_details);
int aml_dev_sample_video_path_latency(struct aml_audio_patch *patch);
int aml_dev_avsync_diff_in_path(struct aml_audio_patch *patch, int *av_diff,
        int *Altcy, char *latency_details);
#endif /*_AML_AVSYNC_TUNING_H_ */
