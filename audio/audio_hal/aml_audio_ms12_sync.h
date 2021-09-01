/*
 * Copyright (C) 2021 The Android Open Source Project
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


#include "audio_hw.h"

int aml_audio_get_ms12_tunnel_latency(struct audio_stream_out *stream);

int aml_audio_get_ms12_presentation_position(const struct audio_stream_out *stream, uint64_t *frames, struct timespec *timestamp);

uint32_t aml_audio_out_get_ms12_latency_frames(struct audio_stream_out *stream);

uint32_t out_get_ms12_latency_frames(struct audio_stream_out *stream);

uint32_t out_get_ms12_bitstream_latency_ms(struct audio_stream_out *stream);

int aml_audio_get_nonms12_tunnel_latency(struct audio_stream_out * stream);

/*
 *@brief for audio dtv, get the ms12-lib latency.
 * The return value unit is the number of samples.
 */
int aml_audio_dtv_get_ms12_latency(struct audio_stream_out *stream);

/*
 *@brief for audio dtv, get the bypass ms12 tuning latency(ms).
 * The return value unit is millisecond(ms).
 */
 int dtv_get_ms12_bypass_latency_offset(void);

/*
 *@brief for audio dtv, get the ddp-lib(nonms12) latency.
 * The return value unit is the number of samples.
 */
int aml_audio_dtv_get_nonms12_latency(struct audio_stream_out * stream);

