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

#ifndef __AML_DUMP_DEBUG_H__
#define __AML_DUMP_DEBUG_H__

#define CC_DUMP_SRC_TYPE_INPUT         (0)
#define CC_DUMP_SRC_TYPE_OUTPUT        (1)
#define CC_DUMP_SRC_TYPE_INPUT_PARSE   (2)

typedef struct dump_debug_item {
    char name[128];
    int value;
} dump_debug_item_t;

extern dump_debug_item_t aml_debug_items[];

void DoDumpData(const void *data_buf, int size, int aud_src_type);

typedef enum AML_DUMP_DEBUG_INFO {
    AML_DEBUG_AUDIOHAL_DEBUG,
    AML_DEBUG_AUDIOHAL_LEVEL_DETECT,
    AML_DEBUG_AUDIOHAL_HW_SYNC,
    AML_DEBUG_AUDIOHAL_ALSA,
    AML_DUMP_AUDIOHAL_MS12,
    AML_DUMP_AUDIOHAL_ALSA,
    AML_DEBUG_DUMP_MAX,
} AML_DUMP_DEBUG_INFO_T;

#define AML_DEBUG_AUDIOHAL_DEBUG_PROPERTY           "vendor.media.audio.hal.debug"
#define AML_DEBUG_AUDIOHAL_LEVEL_DETECT_PROPERTY    "vendor.media.audiohal.level"
#define AML_DEBUG_AUDIOHAL_HW_SYNC_PROPERTY         "vendor.media.audiohal.hwsync"
#define AML_DEBUG_AUDIOHAL_ALSA_PROPERTY            "vendor.media.audio.hal.alsa"
#define AML_DUMP_AUDIOHAL_MS12_PROPERTY             "vendor.media.audiohal.ms12dump"
#define AML_DUMP_AUDIOHAL_ALSA_PROPERTY             "vendor.media.audiohal.alsadump"



void aml_audio_debug_open(void);
void aml_audio_debug_close(void);
static inline int  get_debug_value(AML_DUMP_DEBUG_INFO_T info_id) {
    return aml_debug_items[info_id].value;
}


#endif

