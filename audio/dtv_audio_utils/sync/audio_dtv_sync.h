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



#ifndef _AUDIO_DTV_SYNC_H_
#define _AUDIO_DTV_SYNC_H_

#include <stdbool.h>
#define TSYNC_LASTCHECKIN_APTS "/sys/class/tsync/last_checkin_apts"
#define TSYNC_CHECKIN_FIRSTAPTS_PATH "/sys/class/tsync/checkin_firstapts"

#define TSYNC_IOC_MAGIC 'T'
#define TSYNC_IOC_SET_FIRST_CHECKIN_APTS _IOW(TSYNC_IOC_MAGIC, 0x02, int)
#define TSYNC_IOC_SET_LAST_CHECKIN_APTS _IOW(TSYNC_IOC_MAGIC, 0x03, int)

void aml_audio_swcheck_init(int audio_path);
int aml_audio_swcheck_get_firstapts(int  audio_path);
int aml_audio_swcheck_checkin_apts(int audio_path, size_t offset, unsigned long apts);
int aml_audio_swcheck_lookup_apts(int audio_path, size_t offset, unsigned long *p_apts);
void aml_audio_swcheck_release(int audio_path);
#endif
