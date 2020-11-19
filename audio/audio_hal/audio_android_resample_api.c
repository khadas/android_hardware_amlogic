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
#include <stdlib.h>
#include "aml_audio_stream.h"
#include "audio_android_resample_api.h"
#include "aml_resample_wrap.h"

#define RING_BUF_FRAMES 16384
#define MINUM_RESAMPLE_OUTPUT_SIZE 1024
static size_t in_read_func(void *ring_buffer, void *buf, size_t size)
{
    int ret = -1;
    ring_buffer_t *ringbuffer = ring_buffer;

    ret = ring_buffer_read(ringbuffer, (unsigned char*)buf, size);
    return ret;
}

int android_resample_open(void **handle, audio_resample_config_t *resample_config)
{
    int ret = -1;
    android_resample_handle_t *resample = NULL;

    if (resample_config->aformat != AUDIO_FORMAT_PCM_16_BIT) {
        ALOGE("Not support Format =%d \n", resample_config->aformat);
        return -1;
    }

    resample = (android_resample_handle_t *)aml_audio_calloc(1, sizeof(android_resample_handle_t));
    if (resample == NULL) {
        ALOGE("malloc resample_para failed\n");
        return -1;
    }

    resample->channels  = resample_config->channels;
    resample->input_sr  = resample_config->input_sr;
    resample->output_sr = resample_config->output_sr;

    resample->ringbuf_size = resample->channels * audio_bytes_per_sample(resample_config->aformat) * RING_BUF_FRAMES;
    ret = ring_buffer_init(&resample->ring_buf, resample->ringbuf_size);
    if (ret < 0) {
        ALOGE("ringbuffer init failed\n");
        goto exit;
    }

    ret = android_resample_init(resample,
                                resample->input_sr,
                                resample_config->aformat,
                                resample->channels,
                                in_read_func,
                                &resample->ring_buf);

    if (ret < 0) {
        ALOGE("android_resample_init failed\n");
        goto exit;
    }

    *handle = resample;

    return 0;

exit:
    if (resample) {
        ring_buffer_release(&resample->ring_buf);
        aml_audio_free(resample);
        *handle = 0;
    }
    ALOGE("android resample open failed\n");
    return -1;

}

void android_resample_close(void *handle)
{
    android_resample_handle_t *resample = (android_resample_handle_t *)handle;

    if (resample == NULL) {
        ALOGE("android resample is NULL\n");
        return;
    }
    ALOGD("resmaple close\n");
    android_resample_release(handle);

    ring_buffer_release(&resample->ring_buf);
    aml_audio_free(resample);

    return;
}

int android_resample_process(void *handle, void * in_buffer, size_t bytes, void * out_buffer, size_t *out_size)
{
    android_resample_handle_t *resample = NULL;
    int ret = -1;
    unsigned int input_sr;
    unsigned int output_sr;
    int input_size, input_frames, output_size, output_frames, framesize, resampled_size = 0;
    int min_outsize = 0, min_insize = 0;

    resample = (android_resample_handle_t *)handle;
    if (handle == NULL) {
        ALOGE("simple resample is NULL\n");
        return -1;
    }

    if (get_buffer_write_space(&resample->ring_buf) > (int)bytes) {
        ring_buffer_write(&resample->ring_buf, in_buffer, bytes, UNCOVER_WRITE);
    } else {
        ALOGE("Lost data, bytes:%d\n", bytes);
    }

    input_sr = resample->input_sr;
    output_sr = resample->output_sr;

    framesize = audio_bytes_per_sample(AUDIO_FORMAT_PCM_16_BIT) * resample->channels;

    input_size = bytes;
    input_frames = input_size / framesize;

    output_frames = ((int64_t) input_frames * output_sr) / input_sr;
    output_size = output_frames * framesize;

    /*do resample for one period.*/
    resampled_size  = android_resample_read(resample, (char *)out_buffer, output_size);

    min_insize = MINUM_RESAMPLE_OUTPUT_SIZE;
    min_outsize = ((int64_t) min_insize * output_sr) / input_sr;
    min_insize *= framesize;
    min_outsize *= framesize;

    if (get_buffer_read_space(&resample->ring_buf) > min_insize) {
        resampled_size += android_resample_read(resample, (char *)out_buffer + resampled_size, min_outsize);
    }

    //ALOGD("input_size = %d, resampled_size = %d, left_size = %d\n",
    //    input_size, resampled_size, get_buffer_read_space(&resample->ring_buf));
    *out_size = resampled_size;
    return 0;
}

audio_resample_func_t audio_android_resample_func = {
    .resample_open                 = android_resample_open,
    .resample_close                = android_resample_close,
    .resample_process              = android_resample_process,
};

