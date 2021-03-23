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

#define LOG_TAG "audio_hw_primary"



#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/log.h>


#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "aml_dec_api.h"
#include "aml_audio_spdifout.h"
#define OUTPUT_BUFFER_SIZE (6 * 1024)
#define OUTPUT_ALSA_SAMPLERATE  (48000)


ssize_t aml_audio_spdif_output(struct audio_stream_out *stream, void **spdifout_handle, dec_data_info_t * data_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int ret = 0;

    if (data_info->data_len <= 0) {
        return -1;
    }

    if (*spdifout_handle == NULL) {
        spdif_config_t spdif_config = { 0 };
        spdif_config.audio_format = data_info->data_format;
        if (spdif_config.audio_format == AUDIO_FORMAT_IEC61937) {
            spdif_config.sub_format = data_info->sub_format;
        }
        spdif_config.rate = data_info->data_sr;
        ret = aml_audio_spdifout_open(spdifout_handle, &spdif_config);
        if (ret != 0) {
            return -1;
        }
    }
    if (aml_dev->tv_mute ||
        (aml_dev->patch_src == SRC_DTV && aml_dev->start_mute_flag == 1)) {
        memset(data_info->buf, 0, data_info->data_len);
    }
    ALOGV("[%s:%d] format =0x%x length =%d", __func__, __LINE__, data_info->data_format, data_info->data_len);
    aml_audio_spdifout_processs(*spdifout_handle, data_info->buf, data_info->data_len);

    return ret;
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


int aml_audio_decoder_process_wrapper(struct audio_stream_out *stream, const void *buffer, size_t bytes)
{
    int ret = -1;
    int dec_used_size = 0;
    int left_bytes = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    int return_bytes = bytes;
    struct aml_audio_patch *patch = adev->audio_patch;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;

    if (aml_out->aml_dec == NULL) {
        config_output(stream, true);
    }
    aml_dec_t *aml_dec = aml_out->aml_dec;
    if (aml_dec) {
        dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
        dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
        dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;
        left_bytes = bytes;
        do {
            ret = aml_decoder_process(aml_dec, (unsigned char *)buffer, left_bytes, &dec_used_size);
            if (ret < 0) {
                ALOGV("aml_decoder_process error");
                return return_bytes;
            }
            left_bytes -= dec_used_size;
            ALOGV("%s() ret =%d pcm len =%d raw len=%d", __func__, ret, dec_pcm_data->data_len, dec_raw_data->data_len);
            if (aml_out->optical_format != adev->optical_format) {
                ALOGI("optical format change from 0x%x --> 0x%x", aml_out->optical_format, adev->optical_format);
                aml_out->optical_format = adev->optical_format;
                if (aml_out->spdifout_handle != NULL) {
                    aml_audio_spdifout_close(aml_out->spdifout_handle);
                    aml_out->spdifout_handle = NULL;
                }
                if (aml_out->spdifout2_handle != NULL) {
                    aml_audio_spdifout_close(aml_out->spdifout2_handle);
                    aml_out->spdifout2_handle = NULL;
                }

            }
            // write pcm data
            if (dec_pcm_data->data_len > 0) {
                audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
                void  *dec_data = (void *)dec_pcm_data->buf;
                if (adev->patch_src  == SRC_DTV && adev->start_mute_flag == 1) {
                    memset(dec_pcm_data->buf, 0, dec_pcm_data->data_len);
                }
                if (dec_pcm_data->data_sr > 0) {
                    aml_out->config.rate = dec_pcm_data->data_sr;
                }
                if (patch) {
                    patch->sample_rate = dec_pcm_data->data_sr;
                }
                if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE ) {
                     ret = aml_audio_resample_process_wrapper(&aml_out->resample_handle, dec_pcm_data->buf, dec_pcm_data->data_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
                     if (ret != 0) {
                         ALOGI("aml_audio_resample_process_wrapper failed");
                     } else {
                         dec_data = aml_out->resample_handle->resample_buffer;
                         dec_pcm_data->data_len = aml_out->resample_handle->resample_size;
                     }
                 }
                //aml_audio_dump_audio_bitstreams("/data/mixing_data.raw", dec_data, dec_pcm_data->data_len);
                aml_hw_mixer_mixing(&adev->hw_mixer, dec_data, dec_pcm_data->data_len, output_format);
                if (audio_hal_data_processing(stream, dec_data, dec_pcm_data->data_len, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                    hw_write(stream, output_buffer, output_buffer_bytes, output_format);
                }
            }

            if (aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {
                // write raw data
                if (dec_raw_data->data_sr > 0) {
                    aml_out->config.rate = dec_raw_data->data_sr;
                }

                if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                    /*output raw ddp to hdmi*/
                    if (aml_dec->format == AUDIO_FORMAT_E_AC3 && aml_out->optical_format == AUDIO_FORMAT_E_AC3) {
                        aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
                    }
                    /*output dd data to spdif*/
                    aml_audio_spdif_output(stream, &aml_out->spdifout2_handle, dec_raw_data);
                } else {
                    aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_raw_data);
                }
            }
        } while (dec_pcm_data->data_len || dec_raw_data->data_len);
    }
    return return_bytes;
}


