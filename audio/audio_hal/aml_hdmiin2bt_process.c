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
#include "audio_hw_utils.h"
#include "aml_audio_stream.h"

#include "aml_hdmiin2bt_process.h"

#include "aml_ddp_dec_api.h" /*ddp decoder api*/
#include "aml_dts_dec_api.h" /*dts decoder api*/
//#include "aml_mat_dec_api.h" /*mat decoder api*/

int in_reset_config_param(struct aml_stream_in *in, AML_INPUT_STREAM_CONFIG_TYPE_E enType, const void *pValue)
{
    struct aml_audio_device *adev = in->dev;

    switch (enType) {
        case AML_INPUT_STREAM_CONFIG_TYPE_CHANNELS:
            in->config.channels = *(unsigned int *)pValue;
            ALOGD("%s:%d Config channel nummer to %d", __func__, __LINE__, in->config.channels);
            break;

        case AML_INPUT_STREAM_CONFIG_TYPE_PERIODS:
            in->config.period_size = *(unsigned int *)pValue;
            ALOGD("%s:%d Config Period size to %d", __func__, __LINE__, in->config.period_size);
            break;
        default:
            ALOGW("%s:%d not support input stream type:%#x", __func__, __LINE__, enType);
            return -1;
    }
    return 0;
}

/* here to fix pcm switch to raw nosie issue ,it is caused by hardware format detection later than output
so we delay pcm output one frame to work around the issue,but it has a negative effect on av sync when normal
pcm playback.abouot delay audio 21.3*BT_AND_USB_PERIOD_DELAY_BUF_CNT ms */
void processBtAndUsbCardData(struct aml_stream_in *in, audio_format_t format, void *pBuffer, size_t bytes)
{
    bool bIsBufNull = false;
    for (int i=0; i<BT_AND_USB_PERIOD_DELAY_BUF_CNT; i++) {
        if (NULL == in->pBtUsbPeriodDelayBuf[i]) {
            bIsBufNull = true;
        }
    }

    if (NULL == in->pBtUsbTempDelayBuf || in->delay_buffer_size != bytes || bIsBufNull) {
        in->pBtUsbTempDelayBuf = realloc(in->pBtUsbTempDelayBuf, bytes);
        memset(in->pBtUsbTempDelayBuf, 0, bytes);
        for (int i=0; i<BT_AND_USB_PERIOD_DELAY_BUF_CNT; i++) {
            in->pBtUsbPeriodDelayBuf[i] = realloc(in->pBtUsbPeriodDelayBuf[i], bytes);
            memset(in->pBtUsbPeriodDelayBuf[i], 0, bytes);
        }
        in->delay_buffer_size = bytes;
    }

    if (AUDIO_FORMAT_PCM_16_BIT == format || AUDIO_FORMAT_PCM_32_BIT == format) {
        memcpy(in->pBtUsbTempDelayBuf, pBuffer, bytes);
        memcpy(pBuffer, in->pBtUsbPeriodDelayBuf[BT_AND_USB_PERIOD_DELAY_BUF_CNT-1], bytes);
        for (int i=BT_AND_USB_PERIOD_DELAY_BUF_CNT-1; i>0; i--) {
            memcpy(in->pBtUsbPeriodDelayBuf[i], in->pBtUsbPeriodDelayBuf[i-1], bytes);
        }
        memcpy(in->pBtUsbPeriodDelayBuf[0], in->pBtUsbTempDelayBuf, bytes);
    }
}
#if 0
static void release_the_decoder(audio_format_t format, struct aml_audio_parser *parser)
{
    /*release the last decoder*/
    switch (format) {
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_E_AC3:
        {
            dcv_decode_release(parser);
            break;
        }
        case AUDIO_FORMAT_MAT:
        case AUDIO_FORMAT_DOLBY_TRUEHD:
        {
            //mat_decode_release(parser);
            break;
        }
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
        {
            dca_decode_release(parser);
            break;
        }
        case AUDIO_FORMAT_INVALID:
        {
            ALOGD("cur format invalid, do nothing");
            break;
        }
        case AUDIO_FORMAT_PCM_16_BIT:
        default:
        {
            ALOGW("No need to release the decoder!");
            break;
        }
    }
}

static void init_the_decoder(audio_format_t format, struct aml_audio_parser *parser)
{
    /*init the new decoder*/
    switch (format) {
        case AUDIO_FORMAT_AC3:
        case AUDIO_FORMAT_E_AC3:
        {
            dcv_decode_init(parser);
            parser->enCurDecType = AML_AUDIO_DECODER_TYPE_DOLBY;
            break;
        }
        case AUDIO_FORMAT_MAT:
        case AUDIO_FORMAT_DOLBY_TRUEHD:
        {
            //mat_decode_init(parser);
            parser->enCurDecType = AML_AUDIO_DECODER_TYPE_DOLBY;
            break;
        }
        case AUDIO_FORMAT_DTS:
        case AUDIO_FORMAT_DTS_HD:
        {
            dca_decode_init(parser);
            parser->enCurDecType = AML_AUDIO_DECODER_TYPE_DTS;
            break;
        }
        case AUDIO_FORMAT_PCM_16_BIT:
        {
            parser->enCurDecType = AML_AUDIO_DECODER_TYPE_NONE;
            ALOGW("No need to init the decoder!");
            break;
        }
        case AUDIO_FORMAT_INVALID:
        {
            ALOGI("cur format invalid, do nothing");
            break;
        }
        default:
        {
            ALOGW("This format unsupport or no need to reset decoder!");
            break;
        }
    }

}
#endif
void processHdmiInputFormatChange(struct aml_stream_in *in, struct aml_audio_parser *parser)
{
    struct aml_audio_device *adev = NULL;
    audio_format_t enCurFormat = AUDIO_FORMAT_INVALID;

    if (!in || !parser) {
        ALOGE("%s line %d in %p parser %p\n", __func__, __LINE__, in, parser);
        return ;
    }

    adev = in->dev;
    enCurFormat = audio_parse_get_audio_type(parser->audio_parse_para);
    hdmiin_audio_packet_t cur_audio_packet = get_hdmiin_audio_packet(&adev->alsa_mixer);
    /* some times the audio packet is none, but we alreay get the audio type
     * when we do reconfig, we need both info correct.
     */
    if (enCurFormat != parser->aformat && cur_audio_packet != AUDIO_PACKET_NONE) {
        ALOGD("%s line %d input format changed from %#x to %#x, PreDecType:%#x",
            __func__, __LINE__, parser->aformat, enCurFormat, parser->enCurDecType);

        for (int i=0; i < BT_AND_USB_PERIOD_DELAY_BUF_CNT; i++) {
            memset(in->pBtUsbPeriodDelayBuf[i], 0, in->delay_buffer_size);
        }

        parser->in = in;

        //release_the_decoder(parser->aformat, parser);
        reconfig_read_param_through_hdmiin(adev, in, NULL, 0);
        //init_the_decoder(enCurFormat, parser);
        parser->aformat = enCurFormat;
    }
}

size_t parserRingBufferDataRead(struct aml_audio_parser *parser, void* buffer, size_t bytes)
{
    /*if data is ready, read from buffer.*/
    if (parser->data_ready == 1) {
        int ret = ring_buffer_read(&parser->aml_ringbuffer, (unsigned char*)buffer, bytes);
        if (ret < 0) {
            ALOGE("%s:%d parser in_read err", __func__, __LINE__);
        } else if (ret == 0) {
            unsigned int u32TimeoutMs = 40;
            while (u32TimeoutMs > 0) {
                usleep(5000);
                ret = ring_buffer_read(&parser->aml_ringbuffer, (unsigned char*)buffer, bytes);
                if (parser->aformat == AUDIO_FORMAT_INVALID) { // don't need to wait when the format is unavailable
                    break;
                }
                if (ret > 0) {
                    bytes = ret;
                    break;
                }
                u32TimeoutMs -= 5;
            }
            if (u32TimeoutMs <= 0) {
                memset (buffer, 0, bytes);
                ALOGW("%s:%d read parser ring buffer timeout 40 ms, insert mute data", __func__, __LINE__);
            }
        } else {
            bytes = ret;
        }
    } else {
        memset (buffer, 0, bytes);
    }

    return bytes;
}

