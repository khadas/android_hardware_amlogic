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



#ifndef _AUDIO_HWSYNC_WRAP_H_
#define _AUDIO_HWSYNC_WRAP_H_

#include <stdbool.h>
#include "audio_hwsync.h"

#define TSYNC_FIRSTAPTS "/sys/class/tsync/firstapts"
#define TSYNC_FIRSTVPTS "/sys/class/tsync/firstvpts"
#define TSYNC_PCRSCR    "/sys/class/tsync/pts_pcrscr"
#define TSYNC_EVENT     "/sys/class/tsync/event"
#define TSYNC_APTS      "/sys/class/tsync/pts_audio"
#define TSYNC_VPTS      "/sys/class/tsync/pts_video"
#define TSYNC_ENABLE    "/sys/class/tsync/enable"
#define TSYNC_MODE      "/sys/class/tsync/mode"

void* aml_hwsync_wrap_mediasync_create (void);
void aml_hwsync_wrap_set_tsync_init(audio_hwsync_t *p_hwsync);
int aml_hwsync_wrap_get_tsync_vpts(audio_hwsync_t *p_hwsync, uint32_t *pts);
int aml_hwsync_wrap_get_tsync_firstvpts(audio_hwsync_t *p_hwsync, uint32_t *pts);
void aml_hwsync_wrap_set_tsync_pause(audio_hwsync_t *p_hwsync);
void aml_hwsync_wrap_set_tsync_resume(audio_hwsync_t *p_hwsync);
int aml_hwsync_wrap_set_tsync_start_pts(audio_hwsync_t *p_hwsync, uint32_t pts);
int aml_hwsync_wrap_set_tsync_start_pts64(audio_hwsync_t *p_hwsync,uint64_t pts);
void aml_hwsync_wrap_set_tsync_stop(audio_hwsync_t *p_hwsync);
int aml_hwsync_wrap_get_tsync_pts(audio_hwsync_t *p_hwsync, uint64_t *pts);
int aml_hwsync_wrap_reset_tsync_pcrscr(audio_hwsync_t *p_hwsync, uint64_t pts);
bool aml_hwsync_wrap_get_id(void *mediasync, int32_t* id);
bool aml_hwsync_wrap_set_id(audio_hwsync_t *p_hwsync, uint32_t id);
bool aml_hwsync_wrap_release(audio_hwsync_t *p_hwsync);
void aml_hwsync_wrap_wait_video_start(audio_hwsync_t *p_hwsync, uint32_t wait_count);
void aml_hwsync_wrap_wait_video_drop(audio_hwsync_t *p_hwsync, uint64_t cur_pts, uint32_t wait_count);

#endif
