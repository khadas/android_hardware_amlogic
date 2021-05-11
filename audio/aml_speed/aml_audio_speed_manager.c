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
#include <string.h>
#include <stdlib.h>
#include <system/audio.h>

#include "aml_malloc_debug.h"
#include "audio_sonic_speed_api.h"


#define SPEED_LENGTH (1024 * 16)

static audio_speed_func_t * get_speed_function(speed_type_t speed_type)
{
    switch (speed_type) {
    case AML_AUDIO_SIMPLE_SPEED:
        return NULL;
        break;
    case AML_AUDIO_SONIC_SPEED:
        return &audio_sonic_speed_func;
        break;

    default:
        return NULL;
    }

    return NULL;
}

int aml_audio_speed_init(aml_audio_speed_t ** ppaml_audio_speed, speed_type_t speed_type, audio_speed_config_t *speed_config)
{
    int ret = -1;

    aml_audio_speed_t *aml_audio_speed = NULL;
    audio_speed_func_t * speed_func = NULL;

    if (speed_config == NULL) {
        ALOGE("speed_config is NULL\n");
        return -1;
    }

    if (speed_config->channels == 0 ||
        speed_config->input_sr == 0 ||
        speed_config->speed == 0) {
        ALOGE("Invalid speed config\n");
        return -1;
    }


    if (speed_config->aformat != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGE("Not supported aformat = 0x%x\n", speed_config->aformat);
        return -1;
    }

    aml_audio_speed = (aml_audio_speed_t *)aml_audio_calloc(1, sizeof(aml_audio_speed_t));

    if (aml_audio_speed == NULL) {
        ALOGE("malloc aml_audio_speed failed\n");
        return -1;
    }

    memcpy(&aml_audio_speed->speed_config, speed_config, sizeof(audio_speed_config_t));

    speed_func = get_speed_function(speed_type);

    if (speed_func == NULL) {
        ALOGE("speed_func is NULL\n");
        goto exit;
    }

    aml_audio_speed->speed_type = speed_type;

    aml_audio_speed->speed_rate = (float)speed_config->speed;

    aml_audio_speed->frame_bytes = audio_bytes_per_sample(speed_config->aformat) * speed_config->channels;

    aml_audio_speed->speed_buffer_size =  aml_audio_speed->frame_bytes * SPEED_LENGTH;

    aml_audio_speed->speed_buffer = aml_audio_calloc(1, aml_audio_speed->speed_buffer_size);

    if (aml_audio_speed->speed_buffer == NULL) {
        ALOGE("speed_buffer is NULL\n");
        goto exit;
    }


    ret = speed_func->speed_open(&aml_audio_speed->speed_handle, &aml_audio_speed->speed_config);
    if (ret < 0) {
        ALOGE("speed_open failed\n");
        goto exit;

    }

    * ppaml_audio_speed = aml_audio_speed;

    return 0;

exit:

    if (aml_audio_speed->speed_buffer) {
        aml_audio_free(aml_audio_speed->speed_buffer);
        aml_audio_speed->speed_buffer = NULL;
    }

    if (aml_audio_speed) {
        aml_audio_free(aml_audio_speed);
    }
    * ppaml_audio_speed = NULL;
    return -1;

}

int aml_audio_speed_close(aml_audio_speed_t * aml_audio_speed)
{

    audio_speed_func_t * speed_func = NULL;

    if (aml_audio_speed == NULL) {
        ALOGE("speed_handle is NULL\n");
        return -1;
    }

    speed_func = get_speed_function(aml_audio_speed->speed_type);
    if (speed_func == NULL) {
        ALOGE("speed_func is NULL\n");
    }

    if (speed_func) {
        speed_func->speed_close(aml_audio_speed->speed_handle);
    }

    if (aml_audio_speed->speed_buffer) {
        aml_audio_free(aml_audio_speed->speed_buffer);
        aml_audio_speed->speed_buffer = NULL;
    }

    aml_audio_free(aml_audio_speed);

    return 0;
}

int aml_audio_speed_process(aml_audio_speed_t * aml_audio_speed, void * in_data, size_t size)
{
    size_t out_size = 0;
    int ret = -1;
    unsigned int frame_bytes = 0;
    audio_speed_func_t * speed_func = NULL;

    if (aml_audio_speed == NULL) {
        ALOGE("speed_handle is NULL\n");
        return -1;
    }

    frame_bytes = aml_audio_speed->frame_bytes;

    out_size = size * aml_audio_speed->speed_rate * 2 ; /*we make it for slightly larger*/

    if (out_size > aml_audio_speed->speed_buffer_size) {
        int new_buf_size = out_size;
        aml_audio_speed->speed_buffer = aml_audio_realloc(aml_audio_speed->speed_buffer, new_buf_size);
        if (aml_audio_speed->speed_buffer == NULL) {
            ALOGE("realloc speed_buffer is failed\n");
            return -1;
        }
        ALOGD("realloc speed_buffer size from %zu to %d\n", aml_audio_speed->speed_buffer_size, new_buf_size);
        aml_audio_speed->speed_buffer_size = new_buf_size;
    }

    speed_func = get_speed_function(aml_audio_speed->speed_type);
    if (speed_func == NULL) {
        ALOGE("speed_func is NULL\n");
        return -1;
    }

    memset(aml_audio_speed->speed_buffer, 0, aml_audio_speed->speed_buffer_size);

    ret = speed_func->speed_process(aml_audio_speed->speed_handle,
                                          in_data, size, aml_audio_speed->speed_buffer, &out_size);
    if (ret < 0) {
        aml_audio_speed->speed_size = 0;
        ALOGE("speed error=%d, output size=%zu, buf size=%zu\n",
            ret, out_size, aml_audio_speed->speed_buffer_size);
        return ret;
    }

    aml_audio_speed->speed_size = out_size;
    aml_audio_speed->total_in += size;
    aml_audio_speed->total_out += out_size;
    //ALOGE("total rate=%f\n",(float)aml_audio_speed->total_out/(float)aml_audio_speed->total_in);
    return 0;
}

int aml_audio_speed_reset(aml_audio_speed_t * aml_audio_speed)
{
    int ret = -1;

    audio_speed_func_t * speed_func = NULL;

    if (aml_audio_speed == NULL) {
        ALOGE("speed_handle is NULL\n");
        return -1;
    }

    speed_func = get_speed_function(aml_audio_speed->speed_type);
    if (speed_func == NULL) {
        ALOGE("speed_func is NULL\n");
    }

    if (speed_func && aml_audio_speed->speed_handle) {
        speed_func->speed_close(aml_audio_speed->speed_handle);

        ret = speed_func->speed_open(&aml_audio_speed->speed_handle, &aml_audio_speed->speed_config);
        if (ret < 0) {
            ALOGE("speed_reset failed\n");
            return -1;

        }
    }
    ALOGI("%s", __FUNCTION__);
    return 0;
}

int aml_audio_speed_process_wrapper(aml_audio_speed_t **speed_handle, void *buffer, size_t len,float speed, int sr, int ch_num)
{
   int ret = 0;
   if (*speed_handle) {
        if (speed != (*speed_handle)->speed_config.speed) {
            ALOGD("speed is changed from %f to %f, reset the speed \n",(*speed_handle)->speed_config.speed, speed);
            aml_audio_speed_close(*speed_handle);
            *speed_handle = NULL;
        }
    }

    if (*speed_handle == NULL) {
        audio_speed_config_t speed_config;
        ALOGI("init speed to %f \n",
            speed);
        speed_config.aformat   = AUDIO_FORMAT_PCM_16_BIT;
        speed_config.channels  = ch_num;
        speed_config.input_sr  = sr;
        speed_config.speed = speed;
        ret = aml_audio_speed_init((aml_audio_speed_t **)speed_handle, AML_AUDIO_SONIC_SPEED, &speed_config);
        if (ret < 0) {
            ALOGE("resample init error\n");
            return -1;
        }
    }

    ret = aml_audio_speed_process(*speed_handle, buffer, len);
    if (ret < 0) {
        ALOGE("speed process error\n");
        return -1;
    }
    return ret;
}


