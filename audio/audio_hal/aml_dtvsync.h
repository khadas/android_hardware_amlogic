/*
 * Copyright (C) 2021 Amlogic Corporation.
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



#ifndef _AML_DTVSYNC_H_
#define _AML_DTVSYNC_H_

#include <stdbool.h>
#include "audio_mediasync_wrap.h"
#include "audio_hw_ms12_v2.h"
#include "MediaSyncInterface.h"
#include "dmx_audio_es.h"
//#define SYSTIME_CORRECTION_THRESHOLD        (90000*10/100)

typedef enum {
    DTVSYNC_AUDIO_DROP = 0,
    DTVSYNC_AUDIO_OUTPUT,
} dtvsync_process_res;

void* aml_dtvsync_create();

bool aml_dtvsync_allocinstance(aml_dtvsync_t *p_dtvsync, int32_t* id);

bool aml_dtvsync_bindinstance(aml_dtvsync_t *p_dtvsync, uint32_t id);

bool aml_dtvsync_setParameter(aml_dtvsync_t *p_dtvsync, mediasync_parameter type, void* arg);


bool aml_dtvsync_getParameter(aml_dtvsync_t *p_dtvsync, mediasync_parameter type, void* arg);

bool aml_dtvsync_queue_audio_frame(aml_dtvsync_t *p_dtvsync, struct mediasync_audio_queue_info* info);

bool aml_dtvsync_audioprocess(aml_dtvsync_t *p_dtvsync, int64_t apts, int64_t cur_apts,
                                mediasync_time_unit tunit,
                                struct mediasync_audio_policy* asyncPolicy);


bool aml_dtvsync_insertpcm(struct audio_stream_out *stream, audio_format_t format, int time_ms, bool is_ms12);

bool aml_dtvsync_spdif_insertraw(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms, int is_packed);

bool aml_audio_spdif_insertpcm(struct audio_stream_out *stream,  void **spdifout_handle, int time_ms);

bool aml_dtvsync_ms12_insert_pcm(void *priv_data, int time_ms, enum MS12_PCM_TYPE pcm_type);

bool aml_dtvsync_ms12_insertraw(void *priv_data, int time_ms, audio_format_t output_format);

bool aml_dtvsync_adjustclock(struct audio_stream_out *stream, struct mediasync_audio_policy *p_policy);

dtvsync_process_res aml_dtvsync_nonms12_process(struct audio_stream_out *stream, int duration, bool *speed_enabled);

void aml_dtvsync_ms12_get_policy(struct audio_stream_out *stream);

dtvsync_process_res aml_dtvsync_ms12_process_policy(void *priv_data, aml_ms12_dec_info_t *ms12_info);

bool aml_dtvsync_setPause(aml_dtvsync_t *p_dtvsync, bool pause);

bool aml_dtvsync_reset(aml_dtvsync_t *p_dtvsync);

void aml_dtvsync_release(aml_dtvsync_t *p_dtvsync);

#endif
