/*
 * Copyright (C) 2018 Amlogic Corporation.
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

#define LOG_TAG "aml_audio_resample"

#include <cutils/log.h>
#include <string.h>
#include <stdlib.h>
#include <cutils/properties.h>
#include "aml_malloc_debug.h"
#include "audio_simple_resample_api.h"
#include "audio_android_resample_api.h"
#include "alsa_config_parameters.h"

#define RESAMPLE_LENGTH (1024);
#define ALIGN_FRAME_SIZE (256);

static audio_resample_func_t * get_resample_function(resample_type_t resample_type)
{
    switch (resample_type) {
    case AML_AUDIO_SIMPLE_RESAMPLE:
        return &audio_simple_resample_func;
        break;
    case AML_AUDIO_ANDROID_RESAMPLE:
        return &audio_android_resample_func;
        break;

    default:
        return NULL;
    }

    return NULL;
}

int aml_audio_resample_init(aml_audio_resample_t ** ppaml_audio_resample, resample_type_t resample_type, audio_resample_config_t *resample_config)
{
    int ret = -1;

    aml_audio_resample_t *aml_audio_resample = NULL;
    audio_resample_func_t * resample_func = NULL;

    if (resample_config == NULL) {
        ALOGE("resample_config is NULL\n");
        return -1;
    }

    if (resample_config->channels == 0 ||
        resample_config->input_sr == 0 ||
        resample_config->output_sr == 0) {
        ALOGE("Invalid resample config\n");
        return -1;
    }


    if (resample_config->aformat != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGE("Not supported aformat = 0x%x\n", resample_config->aformat);
        return -1;
    }

    aml_audio_resample = (aml_audio_resample_t *)aml_audio_calloc(1, sizeof(aml_audio_resample_t));

    if (aml_audio_resample == NULL) {
        ALOGE("malloc aml_audio_resample failed\n");
        return -1;
    }

    memcpy(&aml_audio_resample->resample_config, resample_config, sizeof(audio_resample_config_t));

    resample_func = get_resample_function(resample_type);

    if (resample_func == NULL) {
        ALOGE("resample_func is NULL\n");
        goto exit;
    }

    aml_audio_resample->resample_type = resample_type;

    aml_audio_resample->resample_rate = (float)resample_config->output_sr / (float)resample_config->input_sr;

    aml_audio_resample->frame_bytes = audio_bytes_per_sample(resample_config->aformat) * resample_config->channels;

    aml_audio_resample->align_size = sizeof(int16_t) * resample_config->channels * ALIGN_FRAME_SIZE;

    /* alloc more buffer size to align output data */
    aml_audio_resample->resample_buffer_size =  2 * aml_audio_resample->frame_bytes * RESAMPLE_LENGTH + aml_audio_resample->align_size;

    aml_audio_resample->resample_buffer = aml_audio_calloc(1, aml_audio_resample->resample_buffer_size);

    if (aml_audio_resample->resample_buffer == NULL) {
        ALOGE("resample_buffer is NULL\n");
        goto exit;
    }

    ret = resample_func->resample_open(&aml_audio_resample->resample_handle, &aml_audio_resample->resample_config);
    if (ret < 0) {
        ALOGE("resample_open failed\n");
        goto exit;

    }

    * ppaml_audio_resample = aml_audio_resample;

    return 0;

exit:

    if (aml_audio_resample->resample_buffer) {
        aml_audio_free(aml_audio_resample->resample_buffer);
        aml_audio_resample->resample_buffer = NULL;
    }

    if (aml_audio_resample) {
        aml_audio_free(aml_audio_resample);
    }
    * ppaml_audio_resample = NULL;
    return -1;

}

int aml_audio_resample_close(aml_audio_resample_t * aml_audio_resample)
{

    audio_resample_func_t * resample_func = NULL;

    if (aml_audio_resample == NULL) {
        ALOGE("resample_handle is NULL\n");
        return -1;
    }

    resample_func = get_resample_function(aml_audio_resample->resample_type);
    if (resample_func == NULL) {
        ALOGE("resample_func is NULL\n");
    }

    if (resample_func) {
        resample_func->resample_close(aml_audio_resample->resample_handle);
    }

    if (aml_audio_resample->resample_buffer) {
        aml_audio_free(aml_audio_resample->resample_buffer);
        aml_audio_resample->resample_buffer = NULL;
    }

    aml_audio_free(aml_audio_resample);

    return 0;
}

int aml_audio_resample_process(aml_audio_resample_t * aml_audio_resample, void * in_data, size_t size)
{
    size_t out_size = 0;
    int ret = -1;
    unsigned int frame_bytes = 0;
    audio_resample_func_t * resample_func = NULL;

    if (aml_audio_resample == NULL) {
        ALOGE("resample_handle is NULL\n");
        return -1;
    }

    frame_bytes = aml_audio_resample->frame_bytes;

    out_size = size * aml_audio_resample->resample_rate * 2 ; /*we make it for slightly larger*/

    if (out_size > aml_audio_resample->resample_buffer_size) {
        int new_buf_size = out_size + aml_audio_resample->align_size;
        aml_audio_resample->resample_buffer = aml_audio_realloc(aml_audio_resample->resample_buffer, new_buf_size);
        if (aml_audio_resample->resample_buffer == NULL) {
            ALOGE("realloc resample_buffer is failed\n");
            return -1;
        }
        ALOGD("realloc resample_buffer size from %zu to %d\n", aml_audio_resample->resample_buffer_size, new_buf_size);
        aml_audio_resample->resample_buffer_size = new_buf_size;
    }

    resample_func = get_resample_function(aml_audio_resample->resample_type);
    if (resample_func == NULL) {
        ALOGE("resample_func is NULL\n");
        return -1;
    }

    /* move left data to the head of buffer */
    memmove(aml_audio_resample->resample_buffer,
            (char *)aml_audio_resample->resample_buffer + aml_audio_resample->last_copy_size,
            aml_audio_resample->last_left_size);

    memset((char *)aml_audio_resample->resample_buffer + aml_audio_resample->last_left_size, 0,
           aml_audio_resample->resample_buffer_size - aml_audio_resample->last_left_size);

    ret = resample_func->resample_process(aml_audio_resample->resample_handle, in_data, size,
                                          (char *)aml_audio_resample->resample_buffer + aml_audio_resample->last_left_size,
                                          &out_size);
    if (ret < 0) {
        aml_audio_resample->resample_size = 0;
        ALOGE("resmaple error=%d, output size=%zu, buf size=%zu\n",
            ret, out_size, aml_audio_resample->resample_buffer_size);
        return ret;
    }

    int current_left_size = (aml_audio_resample->last_left_size + out_size) % aml_audio_resample->align_size;
    aml_audio_resample->last_copy_size = out_size + aml_audio_resample->last_left_size - current_left_size;
    aml_audio_resample->last_left_size = current_left_size;
    aml_audio_resample->resample_size = aml_audio_resample->last_copy_size;
    aml_audio_resample->total_in += size;
    aml_audio_resample->total_out += aml_audio_resample->last_copy_size;
    //ALOGE("total rate=%f\n",(float)aml_audio_resample->total_out/(float)aml_audio_resample->total_in);

#if 0
        if (getprop_bool("media.audiohal.resample")) {
            FILE *dump_fp = NULL;
            dump_fp = fopen("/data/audio_hal/resamplein.pcm", "a+");
            if (dump_fp != NULL) {
                fwrite(in_data, size, 1, dump_fp);
                fclose(dump_fp);
            } else {
                ALOGW("[Error] Can't write to /data/audio_hal/resamplein.pcm");
            }

            dump_fp = fopen("/data/audio_hal/resampleout.pcm", "a+");
            if (dump_fp != NULL) {
                fwrite(aml_audio_resample->resample_buffer, aml_audio_resample->resample_size, 1, dump_fp);
                fclose(dump_fp);
            } else {
                ALOGW("[Error] Can't write to /data/audio_hal/resampleout.pcm");
            }


        }
#endif

    return 0;
}

int aml_audio_resample_reset(aml_audio_resample_t * aml_audio_resample)
{
    int ret = -1;

    audio_resample_func_t * resample_func = NULL;

    if (aml_audio_resample == NULL) {
        ALOGE("resample_handle is NULL\n");
        return -1;
    }

    resample_func = get_resample_function(aml_audio_resample->resample_type);
    if (resample_func == NULL) {
        ALOGE("resample_func is NULL\n");
    }

    if (resample_func && aml_audio_resample->resample_handle) {
        resample_func->resample_close(aml_audio_resample->resample_handle);

        ret = resample_func->resample_open(&aml_audio_resample->resample_handle, &aml_audio_resample->resample_config);
        if (ret < 0) {
            ALOGE("resample_reset failed\n");
            return -1;

        }
    }
    ALOGI("%s", __FUNCTION__);
    return 0;
}

int aml_audio_resample_process_wrapper(aml_audio_resample_t **resample_handle, void *buffer, size_t len, int sr, int ch_num)
{
   int ret = 0;
   if (*resample_handle) {
        if (sr != (int)(*resample_handle)->resample_config.input_sr) {
            audio_resample_config_t resample_config;
            ALOGD("Sample rate is changed from %d to %d, reset the resample\n",(*resample_handle)->resample_config.input_sr, sr);
            aml_audio_resample_close(*resample_handle);
            *resample_handle = NULL;
        }
    }

    if (*resample_handle == NULL) {
        audio_resample_config_t resample_config;
        ALOGI("init resampler from %d to 48000!, channel num = %d\n",
            sr, ch_num);
        resample_config.aformat   = AUDIO_FORMAT_PCM_16_BIT;
        resample_config.channels  = ch_num;
        resample_config.input_sr  = sr;
        resample_config.output_sr = OUTPUT_ALSA_SAMPLERATE;
        ret = aml_audio_resample_init((aml_audio_resample_t **)resample_handle, AML_AUDIO_ANDROID_RESAMPLE, &resample_config);
        if (ret < 0) {
            ALOGE("resample init error\n");
            return -1;
        }
    }

    ret = aml_audio_resample_process(*resample_handle, buffer, len);
    if (ret < 0) {
        ALOGE("resample process error\n");
        return -1;
    }
    return ret;
}


