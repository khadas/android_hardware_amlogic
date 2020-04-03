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

#ifndef _A2DP_HAL_H_
#define _A2DP_HAL_H_

#include <sys/types.h>
#include <hardware/audio.h>

#ifdef __cplusplus
extern "C" {
#endif

int a2dp_out_open(struct audio_hw_device* stream);
int a2dp_out_close(struct audio_hw_device* stream);
int a2dp_out_resume(struct audio_stream_out* stream);
int a2dp_out_standby(struct audio_stream* stream);
ssize_t a2dp_out_write(struct audio_stream_out* stream, const void* buffer, size_t bytes);
uint32_t a2dp_out_get_latency(const struct audio_stream_out* stream);
int a2dp_out_set_parameters (struct audio_stream *stream, const char *kvpairs);

#ifdef __cplusplus
}
#endif

#endif
