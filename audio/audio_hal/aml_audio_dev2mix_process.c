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
//#define LOG_NDEBUG 0


#include <system/audio.h>
#include <cutils/log.h>

#include "audio_hw.h"
#include "aml_audio_stream.h"
#include "aml_audio_nonms12_render.h"
#include "aml_audio_dev2mix_process.h"
#include "alsa_manager.h"
#include "alsa_config_parameters.h"

struct aml_audio_parser {
    ring_buffer_t           aml_ringbuffer;
    pthread_t               audio_parse_threadID;
    void                    *audio_parse_para;
    audio_format_t          aformat;
    struct aml_stream_in    *in;
    aml_dec_t               *aml_dec;
    unsigned char           *temp_buffer;
    aml_audio_resample_t    *resample_handle;
};

int aml_dev2mix_parser_create(struct audio_hw_device *dev, audio_devices_t input_dev)
{
    struct aml_audio_parser *parser;
    struct aml_audio_device *aml_dev = (struct aml_audio_device *) dev;
    int ret = 0;

    ALOGD("[%s:%d] input dev:%#x", __func__, __LINE__, input_dev);
    parser = aml_audio_calloc(1, sizeof(struct aml_audio_parser));
    if (!parser) {
        return -ENOMEM;
    }

    parser->aformat = AUDIO_FORMAT_PCM_16_BIT;
    aml_dev->dev_to_mix_parser = parser;
    ret = ring_buffer_init(&parser->aml_ringbuffer, 4 * 48 * 128);
    if (ret < 0) {
        ALOGW("[%s:%d] Fail to init audio ringbuffer", __func__, __LINE__);
        return -ENOMEM;
    }
    parser->temp_buffer = aml_audio_calloc(1, DEFAULT_CAPTURE_PERIOD_SIZE);
    ret = creat_pthread_for_audio_type_parse(&parser->audio_parse_threadID,
            &parser->audio_parse_para, &aml_dev->alsa_mixer, input_dev);
    if (ret !=  0) {
        ALOGW("[%s:%d] create format parse thread fail", __func__, __LINE__);
        return -1;
    }
    return ret;
}

int aml_dev2mix_parser_release(struct aml_audio_device *aml_dev)
{
    struct aml_audio_parser *parser = aml_dev->dev_to_mix_parser;

    ALOGD("[%s:%d]", __func__, __LINE__);
    if (aml_dev->dev_to_mix_parser == NULL) {
        ALOGW("[%s:%d] dev_to_mix_parser is NULL", __func__, __LINE__);
        return 0;
    }
    if (parser->aml_dec) {
        aml_decoder_release(parser->aml_dec);
        parser->aml_dec = NULL;
        parser->aformat = AUDIO_FORMAT_PCM_16_BIT;
    }
    if (parser->temp_buffer) {
        aml_audio_free(parser->temp_buffer);
        parser->temp_buffer = NULL;
    }
    if (parser->resample_handle) {
        aml_audio_resample_close(parser->resample_handle);
        parser->resample_handle = NULL;
    }
    exit_pthread_for_audio_type_parse(parser->audio_parse_threadID, &parser->audio_parse_para);
    ring_buffer_release(&parser->aml_ringbuffer);
    aml_audio_free(parser);
    aml_dev->dev_to_mix_parser = NULL;
    return 0;
}

size_t aml_dev2mix_parser_process(struct aml_stream_in *in, unsigned char *buffer, size_t bytes)
{
    struct aml_audio_device *adev = in->dev;
    struct aml_audio_parser *parser = adev->dev_to_mix_parser;
    audio_format_t          cur_format = AUDIO_FORMAT_INVALID;
    size_t                  period_read_size = DEFAULT_CAPTURE_PERIOD_SIZE;
    int                     ret = 0;
    aml_dec_t               *aml_dec = NULL;

    cur_format = audio_parse_get_audio_type(parser->audio_parse_para);
    hdmiin_audio_packet_t cur_audio_packet = get_hdmiin_audio_packet(&adev->alsa_mixer);
    parser->in = in;
    if ((cur_format != parser->aformat && cur_audio_packet != AUDIO_PACKET_NONE) ||
            parser->aml_dec == NULL) {
        ALOGI("[%s:%d] input format changed %#x -> %#x", __func__, __LINE__, parser->aformat, cur_format);
        struct aml_stream_out out;
        out.dev = adev;
        out.hal_internal_format = cur_format;
        out.hal_ch = 2;
        out.hal_rate = 48000;
        if (cur_format != AUDIO_FORMAT_PCM_16_BIT && cur_format != AUDIO_FORMAT_PCM_32_BIT) {
            out.hal_format = AUDIO_FORMAT_IEC61937;
        } else {
            out.hal_format = cur_format;
        }
        if (parser->aml_dec) {
            aml_decoder_release(parser->aml_dec);
            parser->aml_dec = NULL;
        }
        ret = aml_decoder_config_prepare(&out.stream, out.hal_internal_format, &out.dec_config);
        if (ret < 0) {
            ALOGE("[%s:%d] config decoder error", __func__, __LINE__);
            return bytes;
        }

        ret = aml_decoder_init(&parser->aml_dec, out.hal_internal_format, &out.dec_config);
        if (ret < 0) {
            ALOGE("[%s:%d] aml_decoder_init failed", __func__, __LINE__);
            return bytes;
        }
        reconfig_read_param_through_hdmiin(adev, in, NULL, 0);
        ring_buffer_reset(&parser->aml_ringbuffer);
        parser->aformat = cur_format;
    }
    aml_dec = parser->aml_dec;
    if (bytes > DEFAULT_CAPTURE_PERIOD_SIZE) {
        period_read_size = DEFAULT_CAPTURE_PERIOD_SIZE;
    }

    dec_data_info_t         *dec_pcm_data = &aml_dec->dec_pcm_data;
    int                     time_out_cnt = 0;
    size_t                  read_bytes = 0;
    do {
        aml_alsa_input_read(&in->stream, parser->temp_buffer, period_read_size);
        int cur_writed_byte = 0;
        int cur_writed_byte_cnt = 0;
        int decoder_ret = -1;
        do {
            decoder_ret = aml_decoder_process(aml_dec, parser->temp_buffer + cur_writed_byte_cnt,
                period_read_size - cur_writed_byte_cnt, &cur_writed_byte);
            cur_writed_byte_cnt += cur_writed_byte;
            ALOGV("[%s:%d] cur_writed_byte_cnt:%d, use:%d", __func__, __LINE__, cur_writed_byte_cnt, cur_writed_byte);
            if (decoder_ret == AML_DEC_RETURN_TYPE_CACHE_DATA) {
                break;
            }
            ALOGV("[%s:%d] data_len:%d, cur_writed_byte_cnt:%d, cur_writed_byte:%d, data_sr:%d", __func__, __LINE__,
                dec_pcm_data->data_len, cur_writed_byte_cnt, cur_writed_byte, dec_pcm_data->data_sr);
            void  *dec_data = (void *)dec_pcm_data->buf;
            if (dec_pcm_data->data_len > 0) {
                if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE) {
                    ret = aml_audio_resample_process_wrapper(&parser->resample_handle, dec_pcm_data->buf,
                          dec_pcm_data->data_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
                    if (ret != 0) {
                        ALOGW("[%s:%d] resample fail, size:%d, data_sr:%d", __func__, __LINE__, dec_pcm_data->data_len, dec_pcm_data->data_sr);
                    } else {
                        dec_data = parser->resample_handle->resample_buffer;
                        dec_pcm_data->data_len = parser->resample_handle->resample_size;
                    }
                }
                ret = ring_buffer_write(&parser->aml_ringbuffer, dec_data, dec_pcm_data->data_len, UNCOVER_WRITE);
                if (ret != dec_pcm_data->data_len) {
                    ALOGW("[%s:%d] need written:%d, actually written:%d", __func__, __LINE__, dec_pcm_data->data_len, ret);
                }
            }
        } while (cur_writed_byte_cnt < period_read_size || aml_dec->fragment_left_size || decoder_ret == AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN);

        /* here to fix pcm switch to raw nosie issue ,it is caused by hardware format detection later than output
        so we delay pcm output one frame to work around the issue,but it has a negative effect on av sync when normal
        pcm playback. abouot delay audio 64 ms */
        if (get_buffer_read_space(&parser->aml_ringbuffer) >= 4 * 48 * 64) {
             read_bytes += ring_buffer_read(&parser->aml_ringbuffer, buffer + read_bytes, bytes - read_bytes);
        }
        if (time_out_cnt++ >= 100) {
            memset(buffer, 0, bytes);
            ALOGW("[%s:%d] alsa read decode timeout 100 times, read_bytes:%d, bytes:%d", __func__, __LINE__, read_bytes, bytes);
            break;
        }
    } while (read_bytes < bytes);
    ALOGV("[%s:%d] read_bytes:%d, byte:%d, cnt:%d", __func__, __LINE__, read_bytes, bytes, time_out_cnt);
    return bytes;
}

