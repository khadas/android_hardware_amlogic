/*
 * Copyright (C) 2020 Amlogic Corporation.
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

#define LOG_TAG "audio_hw_primary"
// #define LOG_NDEBUG 0

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <string.h>

#include "aml_audio_matparser.h"

#define STREAM_PROFILE_POS_IN_MAT 8

int get_stream_profile_from_dolby_mat_frame(const char *audio_buffer, size_t audio_bytes)
{
    int stream_profile = 0;
    char *buf = (char *)audio_buffer;
    int stream_profile_pos = 0;

    if (!audio_buffer || (audio_bytes < STREAM_PROFILE_POS_IN_MAT)) {
        ALOGE("%s line %d audio_buffer %p audio_bytes %#zx\n", __FUNCTION__, __LINE__, audio_buffer, audio_bytes);
        return -1;
    }
    else {
        if (IS_MAT_FORMAT_LSB_SYNC(buf[0], buf[1])) {
             stream_profile_pos = STREAM_PROFILE_POS_IN_MAT - 1;
        }
        else if (IS_MAT_FORMAT_MSB_SYNC(buf[0], buf[1])) {
            stream_profile_pos = STREAM_PROFILE_POS_IN_MAT - 2;
        }
        else {
            return -1;
        }

        return buf[stream_profile_pos];
    }

}

