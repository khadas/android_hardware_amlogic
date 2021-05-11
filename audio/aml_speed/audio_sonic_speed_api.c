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

#define LOG_TAG "aml_audio_speed"

#include <cutils/log.h>
#include <stdlib.h>
#include "aml_malloc_debug.h"
#include "audio_sonic_speed_api.h"
#include "sonic_speed_wrapper.h"


#define OUPUT_BUF_SIZE (1024 * 64)
#define MINUM_SPEED_OUTPUT_FRAMES 512



int sonic_speed_open(void **handle, audio_speed_config_t *speed_config)
{
    int ret = -1;
    sonic_speed_handle_t *speed = NULL;

    if (speed_config->aformat != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGE("Not support Format =%d \n", speed_config->aformat);
        return -1;
    }

    speed = (sonic_speed_handle_t *)aml_audio_calloc(1, sizeof(sonic_speed_handle_t));
    if (speed == NULL) {
        ALOGE("malloc speed_para failed\n");
        return -1;
    }

    speed->speed  = speed_config->speed;
    speed->channels  = speed_config->channels;
    speed->input_sr  = speed_config->input_sr;
    speed->format = speed_config->aformat;

    ret = sonic_speed_init(speed,
                                speed->speed,
                                speed->input_sr,
                                speed->channels);

    if (ret < 0) {
        ALOGE("sonic_speed_init failed\n");
        goto exit;
    }

    *handle = speed;
    return 0;

exit:
    if (speed) {
        aml_audio_free(speed);
        *handle = 0;
    }
    ALOGE("sonic speed open failed\n");
    return -1;

}

void sonic_speed_close(void *handle)
{
    sonic_speed_handle_t *speed = (sonic_speed_handle_t *)handle;

    if (speed == NULL) {
        ALOGE("sonic speed is NULL\n");
        return;
    }
    ALOGD("speed close\n");
    sonic_speed_release(handle);
    aml_audio_free(speed);

    return;
}

int sonic_speed_process(void *handle, void * in_buffer, size_t bytes, void * out_buffer, size_t *out_size)
{
    sonic_speed_handle_t *speed = NULL;
    int ret = -1;

    int framesize, speed_samples = 0, speed_frames = 0;
    int min_outsize = 0;

    speed = (sonic_speed_handle_t *)handle;

    if (handle == NULL) {
        ALOGE("sonic speed is NULL\n");
        return ret;
    }

    framesize = audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT) * speed->channels;

    /*do speed for one period.*/
    sonic_speed_write(speed, (char *)in_buffer, bytes);
    min_outsize = MINUM_SPEED_OUTPUT_FRAMES * framesize;

    do {

       speed_samples  = sonic_speed_read(speed, (char *)out_buffer + speed_frames * framesize, min_outsize);
       speed_frames  += speed_samples;

    } while (speed_samples >  0);

    ALOGV("input_size = %d, speed_frames = %d \n",
    bytes, speed_frames);

    *out_size = speed_frames * framesize;
    if (*out_size > OUPUT_BUF_SIZE) {
         ALOGW("sonic_speed out_size  %d overflow !!", *out_size);
         *out_size = OUPUT_BUF_SIZE;
    }
    return 0;
}

audio_speed_func_t audio_sonic_speed_func = {
    .speed_open                 = sonic_speed_open,
    .speed_close                = sonic_speed_close,
    .speed_process              = sonic_speed_process,
};

