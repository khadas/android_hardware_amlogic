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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0
#include <cutils/log.h>
#include <tinyalsa/asoundlib.h>
#include <audio_utils/channels.h>
#include <aml_android_utils.h>

#include "audio_bt_sco.h"
#include "alsa_config_parameters.h"
#include "alsa_device_parser.h"
#include "aml_audio_timer.h"


static const struct pcm_config config_bt = {
    .channels = 1,
    .rate = VX_NB_SAMPLING_RATE,
    .period_size = 1024,
    .period_count = PLAYBACK_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
};

static void dump_output_data(struct aml_bt_output *bt, const void *buffer, uint32_t size)
{
    if (getprop_bool("vendor.media.audiohal.outdump")) {
        char acFilePathStr[ENUM_TYPE_STR_MAX_LEN];
        sprintf(acFilePathStr, "/data/audio/bt_sco_%0.1fK_%dC_2B.pcm", bt->cfg.rate/1000.0, bt->cfg.channels);
        aml_audio_dump_audio_bitstreams(acFilePathStr, buffer, size);
    }
}

int open_btSCO_device(struct aml_audio_device *adev, size_t frames)
{
    struct aml_bt_output *bt = &adev->bt_output;
    unsigned int card = adev->card;
    unsigned int port = PORT_PCM;
    struct pcm *pcm = NULL;
    struct pcm_config cfg;
    size_t resample_in_frames = 0;
    size_t output_frames = 0;
    int ret = 0;

    if (bt->active) {
        AM_LOGW("Alread has been opened.");
        return 0;
    }

    /* check to update port */
    port = alsa_device_update_pcm_index(port, PLAYBACK);
    ALOGD("%s(), open card(%d) port(%d)", __func__, card, port);
    bt->cfg = config_bt;
    /* correct the BT sampling rate when WB is set */
    if (adev->bt_wbs || aml_getprop_bool("sys.vendor.audio.btwbs"))
        bt->cfg.rate = VX_WB_SAMPLING_RATE;
    ALOGD("%s(), BT SCO sampling rate(%d)", __func__, bt->cfg.rate);

    pthread_mutex_lock(&bt->lock);
    pcm = pcm_open(card, port, PCM_OUT, &bt->cfg);
    if (!pcm_is_ready(pcm)) {
        ALOGE("%s() cannot open pcm_out: %s, card %d, device %d",
                __func__, pcm_get_error(pcm), card, port);
        pcm_close (pcm);
        ret = -ENOENT;
        goto err;
    }

    bt->pcm_bt = pcm;
    ret = create_resampler(MM_FULL_POWER_SAMPLING_RATE,
                            bt->cfg.rate,
                            bt->cfg.channels,
                            RESAMPLER_QUALITY_DEFAULT,
                            NULL,
                            &bt->resampler);
    if (ret != 0) {
        ALOGE("cannot create resampler for bt");
        goto err_res;
    }

    output_frames = frames * bt->cfg.rate / MM_FULL_POWER_SAMPLING_RATE + 1;
    bt->bt_out_buffer = aml_audio_calloc(1, output_frames * 2);
    if (bt->bt_out_buffer == NULL) {
        ALOGE ("cannot malloc memory for bt_out_buffer");
        ret = -ENOMEM;
        goto err_out_buf;
    }
    bt->bt_out_frames = 0;

    bt->resampler_buffer = aml_audio_calloc(1, frames * 2);
    if (bt->resampler_buffer == NULL) {
        ALOGE ("cannot malloc memory for resampler_buffer");
        ret = -ENOMEM;
        goto err_resampler_buf;
    }
    bt->resampler_in_frames = 0;
    bt->resampler_buffer_size_in_frames = frames;
    bt->active = true;
    pthread_mutex_unlock(&bt->lock);

    return 0;

err_resampler_buf:
    aml_audio_free(bt->bt_out_buffer);
err_out_buf:
    release_resampler(bt->resampler);
    bt->resampler = NULL;
err_res:
    pcm_close(bt->pcm_bt);
    bt->pcm_bt = NULL;
err:
    pthread_mutex_unlock(&bt->lock);
    return ret;
}

void close_btSCO_device(struct aml_audio_device *adev)
{
    struct aml_bt_output *bt = &adev->bt_output;
    struct pcm *pcm = bt->pcm_bt;

    AM_LOGI("+++");
    pthread_mutex_lock(&bt->lock);
    if (!bt->active) {
        AM_LOGW("Alread has been closed.");
        goto exit;
    }
    if (pcm) {
        pcm_close(pcm);
        bt->pcm_bt = NULL;
    }
    if (bt->resampler) {
        release_resampler(bt->resampler);
        bt->resampler = NULL;
    }
    if (bt->bt_out_buffer)
        aml_audio_free(bt->bt_out_buffer);
    if (bt->resampler_buffer)
        aml_audio_free(bt->resampler_buffer);
    bt->active = false;

exit:
    pthread_mutex_unlock(&bt->lock);
}

ssize_t write_to_sco(struct aml_audio_device *adev, audio_config_base_t *config,
    const void *buffer, size_t bytes)
{
    struct aml_bt_output *bt = &adev->bt_output;
    size_t frame_size = audio_channel_count_from_out_mask(config->channel_mask) * audio_bytes_per_sample(config->format);
    size_t in_frames = bytes / frame_size;
    size_t out_frames = in_frames * bt->cfg.rate / MM_FULL_POWER_SAMPLING_RATE + 1;;
    int16_t *in_buffer = (int16_t *)buffer;
    int16_t *out_buffer = (int16_t *)bt->bt_out_buffer;
    unsigned int i;
    int ret = 0;

    if (adev->debug_flag) {
        ALOGI("[%s:%d] bytes:%zu, out_device:%#x", __func__, __LINE__, bytes, adev->out_device);
    }

    if (!bt->active) {
        ret = open_btSCO_device(adev, in_frames);
        if (ret) {
            ALOGD("%s: open btsco pcm fail", __func__);
            return bytes;
        }
    }

    /* Discard right channel */
    for (i = 1; i < in_frames; i++) {
        in_buffer[i] = in_buffer[i * 2];
    }
    /* The frame size is now half */
    frame_size /= 2;

    //prepare input buffer
    if (bt->resampler) {
        size_t frames_needed = bt->resampler_in_frames + in_frames;
        if (bt->resampler_buffer_size_in_frames < frames_needed) {
            bt->resampler_buffer_size_in_frames = frames_needed;
            bt->resampler_buffer = (int16_t *)aml_audio_realloc(bt->resampler_buffer,
                    bt->resampler_buffer_size_in_frames * frame_size);
        }

        memcpy(bt->resampler_buffer + bt->resampler_in_frames,
                buffer, in_frames * frame_size);
        bt->resampler_in_frames += in_frames;

        size_t res_in_frames = bt->resampler_in_frames;
        bt->resampler->resample_from_input(bt->resampler,
                     bt->resampler_buffer, &res_in_frames,
                     (int16_t*)bt->bt_out_buffer, &out_frames);
        //prepare output buffer
        bt->resampler_in_frames -= res_in_frames;
        if (bt->resampler_in_frames) {
            memmove(bt->resampler_buffer,
                bt->resampler_buffer + bt->resampler_in_frames,
                bt->resampler_in_frames * frame_size);
        }
    }

    if (bt->pcm_bt) {
        pcm_write(bt->pcm_bt, bt->bt_out_buffer, out_frames * frame_size);
        dump_output_data(bt, bt->bt_out_buffer, out_frames * frame_size);
    }
    return bytes;
}

bool is_sco_port(enum OUT_PORT outport)
{
    return (outport == OUTPORT_BT_SCO_HEADSET) || (outport == OUTPORT_BT_SCO);
}


