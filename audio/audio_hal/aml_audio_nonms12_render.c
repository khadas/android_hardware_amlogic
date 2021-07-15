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

#define LOG_TAG "aml_audio_nonms12_render"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/log.h>
#include <aml_volume_utils.h>


#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "audio_dtv_utils.h"
#include "aml_dec_api.h"
#include "aml_ddp_dec_api.h"
#include "aml_audio_spdifout.h"
#include "alsa_config_parameters.h"

extern unsigned long decoder_apts_lookup(unsigned int offset);
static void aml_audio_stream_volume_process(struct audio_stream_out *stream, void *buf, int sample_size, int channels, int bytes) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    apply_volume_fade(aml_out->last_volume_l, aml_out->volume_l, buf, sample_size, channels, bytes);
    aml_out->last_volume_l = aml_out->volume_l;
    aml_out->last_volume_r = aml_out->volume_r;
    return;
}

static inline bool check_sink_pcm_sr_cap(struct aml_audio_device *adev, int sample_rate)
{
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;

    switch (sample_rate) {
        case 32000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<0));
        case 44100: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<1));
        case 48000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<2));
        case 88200: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<3));
        case 96000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<4));
        case 176400: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<5));
        case 192000: return !!(hdmi_desc->pcm_fmt.sample_rate_mask & (1<<6));
        default: return false;
    }

    return false;
}

ssize_t aml_audio_spdif_output(struct audio_stream_out *stream, void **spdifout_handle, dec_data_info_t * data_info)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *aml_dev = aml_out->dev;
    int ret = 0;

    if (data_info->data_len <= 0) {
        return -1;
    }

    if (aml_dev->patch_src ==  SRC_DTV && aml_out->need_drop_size > 0) {
        if (aml_dev->debug_flag > 1)
            ALOGI("%s, av sync drop data,need_drop_size=%d\n",
                __FUNCTION__, aml_out->need_drop_size);
        return ret;
    }

    if (*spdifout_handle == NULL) {
        spdif_config_t spdif_config = { 0 };
        spdif_config.audio_format = data_info->data_format;
        spdif_config.channel_mask = AUDIO_CHANNEL_OUT_STEREO;
        spdif_config.mute = aml_out->offload_mute;
        if (spdif_config.audio_format == AUDIO_FORMAT_IEC61937) {
            spdif_config.sub_format = data_info->sub_format;
        } else if (audio_is_linear_pcm(spdif_config.audio_format)) {
            if (data_info->data_ch == 6) {
                spdif_config.channel_mask = AUDIO_CHANNEL_OUT_5POINT1;
            } else if (data_info->data_ch == 8) {
                spdif_config.channel_mask = AUDIO_CHANNEL_OUT_7POINT1;
            }
        }
        spdif_config.is_dtscd = data_info->is_dtscd;
        spdif_config.rate = data_info->data_sr;
        ret = aml_audio_spdifout_open(spdifout_handle, &spdif_config);
        if (ret != 0) {
            return -1;
        }
    }

    ALOGV("[%s:%d] format =0x%x length =%d", __func__, __LINE__, data_info->data_format, data_info->data_len);
    aml_audio_spdifout_processs(*spdifout_handle, data_info->buf, data_info->data_len);

    return ret;
}

int aml_audio_nonms12_render(struct audio_stream_out *stream, const void *buffer, size_t bytes)
{
    int decoder_ret = -1,ret = -1;
    int dec_used_size = 0;
    int left_bytes = 0;
    int used_size = 0;
    bool  try_again = false;
    int alsa_latency = 0;
    int decoder_latency = 0;

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    struct aml_native_postprocess *VX_postprocess = &adev->native_postprocess;
    bool do_sync_flag = adev->patch_src  == SRC_DTV && patch && patch->skip_amadec_flag;
    int return_bytes = bytes;
    int out_frames = 0;
    void *input_buffer = (void *)buffer;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int duration = 0;
    bool speed_enabled = false;
    bool dts_pcm_direct_output = false;
    dtvsync_process_res process_result = DTVSYNC_AUDIO_OUTPUT;

    if (aml_out->aml_dec == NULL) {
        config_output(stream, true);
    }
    aml_dec_t *aml_dec = aml_out->aml_dec;
    if (aml_dec) {
        dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
        dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
        dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;
        left_bytes = bytes;

        if (do_sync_flag) {
            if(patch->skip_amadec_flag) {
                aml_dec->in_frame_pts = patch->cur_package->pts;
            } else {
                aml_dec->in_frame_pts = decoder_apts_lookup(patch->decoder_offset);
            }
        }

        do {
            ALOGV("%s() in raw len=%d", __func__, left_bytes);
            used_size = 0;

            decoder_ret = aml_decoder_process(aml_dec, (unsigned char *)buffer + dec_used_size, left_bytes, &used_size);
            if (decoder_ret == AML_DEC_RETURN_TYPE_CACHE_DATA) {
                ALOGV("[%s:%d] cache the data to decode", __func__, __LINE__);
                break;
            } else if (decoder_ret < 0) {
                ALOGV("[%s:%d] aml_decoder_process error, ret:%d", __func__, __LINE__, decoder_ret);

            }
            if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
                if (dec_pcm_data->data_len)
                check_audio_level("dec pcm", dec_pcm_data->buf, dec_pcm_data->data_len);
            }


            left_bytes -= used_size;
            dec_used_size += used_size;
            ALOGV("used_size %d total used size %d %s() left_bytes =%d pcm len =%d raw len=%d",
                used_size, dec_used_size, __func__, left_bytes, dec_pcm_data->data_len, dec_raw_data->data_len);

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
                // aml_audio_dump_audio_bitstreams("/data/dec_data.raw", dec_pcm_data->buf, dec_pcm_data->data_len);
                out_frames += dec_pcm_data->data_len /( 2 * dec_pcm_data->data_ch);
                aml_dec->out_frame_pts = aml_dec->in_frame_pts + (90 * out_frames /(dec_pcm_data->data_sr / 1000));

                audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
                void  *dec_data = (void *)dec_pcm_data->buf;
                int pcm_len = dec_pcm_data->data_len;
                if (patch && adev->patch_src  == SRC_DTV &&
                    (adev->start_mute_flag == 1 || adev->tv_mute)) {
                    memset(dec_pcm_data->buf, 0, dec_pcm_data->data_len);
                }

                if (patch) {
                    patch->sample_rate = dec_pcm_data->data_sr;
                }

                /* For dts certification:
                 * DTS 88.2K/96K pcm direct output case.
                 * If the PCM after decoding is 88.2k/96k, then direct output.
                 * Need to check whether HDMI sink supports 88.2k/96k or not.*/
                if (adev->hdmi_format == PCM
                    && is_dts_format(aml_out->hal_internal_format)
                    && dec_pcm_data->data_sr > 48000
                    && check_sink_pcm_sr_cap(adev, dec_pcm_data->data_sr)) {

                    dts_pcm_direct_output = true;
                }

                if (dec_pcm_data->data_sr != OUTPUT_ALSA_SAMPLERATE ) {
                    ret = aml_audio_resample_process_wrapper(&aml_out->resample_handle, dec_pcm_data->buf,
                    pcm_len, dec_pcm_data->data_sr, dec_pcm_data->data_ch);
                    if (ret != 0) {
                        ALOGE("aml_audio_resample_process_wrapper failed");
                    } else {
                        dec_data = aml_out->resample_handle->resample_buffer;
                        pcm_len = aml_out->resample_handle->resample_size;
                    }
                    aml_out->config.rate = OUTPUT_ALSA_SAMPLERATE;
                } else {
                    if (dec_pcm_data->data_sr > 0)
                        aml_out->config.rate = dec_pcm_data->data_sr;
                }

                /*process the stream volume before mix*/
                aml_audio_stream_volume_process(stream, dec_data, sizeof(int16_t), dec_pcm_data->data_ch, pcm_len);

                if (dec_pcm_data->data_ch == 6) {
                    ret = audio_VX_post_process(VX_postprocess, (int16_t *)dec_data, pcm_len);
                    if (ret > 0) {
                        pcm_len = ret; /* VX will downmix 6ch to 2ch, pcm size will be changed */
                        dec_pcm_data->data_ch /= 3;
                    }
                }

                if (do_sync_flag) {
                    if (dec_pcm_data->data_ch != 0)
                        duration =  (pcm_len * 1000) / (2 * dec_pcm_data->data_ch * aml_out->config.rate);

                    if (patch->skip_amadec_flag) {
                        alsa_latency = 90 *(out_get_alsa_latency_frames(stream)  * 1000) / aml_out->config.rate;;
                        patch->dtvsync->cur_outapts = aml_dec->out_frame_pts - decoder_latency - alsa_latency;

                    }
                    //sync process here
                    if (adev->patch_src  == SRC_DTV && aml_out->dtvsync_enable) {

                        process_result = aml_dtvsync_nonms12_process(stream, duration, &speed_enabled);
                        if (process_result == DTVSYNC_AUDIO_DROP)
                            continue;
                    }
                    if (fabs(aml_out->output_speed - 1.0f) > 1e-6) {
                        ret = aml_audio_speed_process_wrapper(&aml_out->speed_handle, dec_data,
                                                pcm_len, aml_out->output_speed,
                                                OUTPUT_ALSA_SAMPLERATE, dec_pcm_data->data_ch);
                        if (ret != 0) {
                            ALOGE("aml_audio_speed_process_wrapper failed");
                        } else {

                            ALOGV("data_len=%d, speed_size=%d\n", pcm_len, aml_out->speed_handle->speed_size);
                            dec_data = aml_out->speed_handle->speed_buffer;
                            pcm_len = aml_out->speed_handle->speed_size;
                        }
                    }
                }

                if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
                    check_audio_level("render pcm", dec_data, pcm_len);
                }

                aml_hw_mixer_mixing(&adev->hw_mixer, dec_data, pcm_len, output_format);
                if (audio_hal_data_processing(stream, dec_data, pcm_len, &output_buffer, &output_buffer_bytes, output_format) == 0) {
                    if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
                        check_audio_level("after process", output_buffer, output_buffer_bytes);
                    }
                    hw_write(stream, output_buffer, output_buffer_bytes, output_format);
                }
            }

            // write raw data
            /*for pcm case, we check whether it has muti channel pcm or 96k/88.2k pcm */
            if (!dts_pcm_direct_output && !speed_enabled && audio_is_linear_pcm(aml_dec->format) && raw_in_data->data_ch > 2) {
                aml_audio_stream_volume_process(stream, raw_in_data->buf, sizeof(int16_t), raw_in_data->data_ch, raw_in_data->data_len);
                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
            } else if (dts_pcm_direct_output && !speed_enabled) {
                aml_audio_stream_volume_process(stream, dec_pcm_data->buf, sizeof(int16_t), dec_pcm_data->data_ch, dec_pcm_data->data_len);
                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_pcm_data);
            }

            if (!dts_pcm_direct_output && !speed_enabled && aml_out->optical_format != AUDIO_FORMAT_PCM_16_BIT) {
                if (aml_dec->format == AUDIO_FORMAT_E_AC3 || aml_dec->format == AUDIO_FORMAT_AC3) {
                    if (adev->dual_spdif_support) {
                        /*output raw ddp to hdmi*/
                        if (aml_dec->format == AUDIO_FORMAT_E_AC3 && aml_out->optical_format == AUDIO_FORMAT_E_AC3) {
                            if (raw_in_data->data_len)
                                aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
                        }

                        /*output dd data to spdif*/
                        if (dec_raw_data->data_len > 0)
                            aml_audio_spdif_output(stream, &aml_out->spdifout2_handle, dec_raw_data);
                    } else {
                        if (raw_in_data->data_len)
                            aml_audio_spdif_output(stream, &aml_out->spdifout_handle, raw_in_data);
                    }
                } else {
                    aml_audio_spdif_output(stream, &aml_out->spdifout_handle, dec_raw_data);
                }

            }

            /*special case  for dts , dts decoder need to follow aml_dec_api.h */
            if ((aml_out->hal_internal_format == AUDIO_FORMAT_DTS ||
                aml_out->hal_internal_format == AUDIO_FORMAT_DTS_HD )&&
                decoder_ret == AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN ) {
                try_again = true;
            }

        } while ((left_bytes > 0) || aml_dec->fragment_left_size || try_again);
    }

    if (patch && (adev->patch_src  == SRC_DTV)) {
        aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
        if (demux_info && demux_info->dual_decoder_support == 0)
            patch->decoder_offset +=return_bytes;
    }

    return return_bytes;
}
static audio_format_t get_dcvlib_output_format(audio_format_t src_format, struct aml_audio_device *aml_dev)
{
    audio_format_t output_format = AUDIO_FORMAT_PCM_16_BIT;
    if (aml_dev->hdmi_format == AUTO) {
        if (src_format == AUDIO_FORMAT_E_AC3 && aml_dev->sink_capability == AUDIO_FORMAT_E_AC3) {
            output_format = AUDIO_FORMAT_E_AC3;
        } else if (src_format == AUDIO_FORMAT_AC3 &&
                (aml_dev->sink_capability == AUDIO_FORMAT_E_AC3 || aml_dev->sink_capability == AUDIO_FORMAT_AC3)) {
            output_format = AUDIO_FORMAT_AC3;
        }
    }
    return output_format;
}

bool aml_decoder_output_compatible(struct audio_stream_out *stream, audio_format_t sink_format __unused, audio_format_t optical_format) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    bool is_compatible = true;

    if (aml_out->hal_internal_format != aml_out->aml_dec->format) {
        ALOGI("[%s:%d] not compatible. dec format:%#x -> cur format:%#x", __func__, __LINE__,
            aml_out->aml_dec->format, aml_out->hal_internal_format);
        return false;
    }

    if ((aml_out->aml_dec->format == AUDIO_FORMAT_AC3)
        || (aml_out->aml_dec->format == AUDIO_FORMAT_E_AC3)) {
        aml_dcv_config_t* dcv_config = (aml_dcv_config_t *)(&aml_out->dec_config);
        if (((optical_format == AUDIO_FORMAT_PCM_16_BIT) && (dcv_config->digital_raw > AML_DEC_CONTROL_DECODING))
            || ((optical_format == AUDIO_FORMAT_E_AC3) && (dcv_config->digital_raw != AML_DEC_CONTROL_RAW))) {
                is_compatible = false;
        }
    } else if ((aml_out->aml_dec->format == AUDIO_FORMAT_DTS)
                || (aml_out->aml_dec->format == AUDIO_FORMAT_DTS_HD)) {
        aml_dca_config_t* dca_config = (aml_dca_config_t *)(&aml_out->dec_config);
        if ((optical_format == AUDIO_FORMAT_PCM_16_BIT) && (dca_config->digital_raw > AML_DEC_CONTROL_DECODING)) {
            is_compatible = false;
        }
    }

    return is_compatible;
}


static void ddp_decoder_config_prepare(struct audio_stream_out *stream, aml_dcv_config_t * ddp_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_arc_hdmi_desc *p_hdmi_descs = &adev->hdmi_descs;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;

    if (patch ) {
        demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }

    adev->dcvlib_bypass_enable = 0;

    ddp_config->digital_raw = AML_DEC_CONTROL_CONVERT;

    if (demux_info && demux_info->dual_decoder_support) {
        ddp_config->decoding_mode = DDP_DECODE_MODE_AD_DUAL;
    } else if (aml_out->ad_substream_supported) {
        ddp_config->decoding_mode = DDP_DECODE_MODE_AD_SUBSTREAM;
    } else {
        ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
    }

    audio_format_t output_format = get_dcvlib_output_format(aml_out->hal_internal_format, adev);
    if (output_format != AUDIO_FORMAT_PCM_16_BIT && output_format != AUDIO_FORMAT_PCM_32_BIT) {
            ddp_config->decoding_mode = DDP_DECODE_MODE_SINGLE;
    }

    if (aml_out->hal_internal_format == AUDIO_FORMAT_E_AC3) {
        ddp_config->nIsEc3 = 1;
    } else if (aml_out->hal_internal_format == AUDIO_FORMAT_AC3) {
        ddp_config->nIsEc3 = 0;
    }
    /*check if the input format is contained with 61937 format*/
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        ddp_config->is_iec61937 = true;
    } else {
        ddp_config->is_iec61937 = false;
    }

    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_61937:%d, IsEc3:%d"
        , __func__, ddp_config->digital_raw, aml_out->dual_output_flag, ddp_config->is_iec61937, ddp_config->nIsEc3);
    return;
}

static void dts_decoder_config_prepare(struct audio_stream_out *stream, aml_dca_config_t * dts_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    adev->dtslib_bypass_enable = 0;

    dts_config->digital_raw = AML_DEC_CONTROL_CONVERT;
    if (aml_out->hal_format == AUDIO_FORMAT_IEC61937) {
        dts_config->is_iec61937 = true;
    } else {
        dts_config->is_iec61937 = false;
    }
    dts_config->is_dtscd = aml_out->is_dtscd;

    dts_config->dev = (void *)adev;
    ALOGI("%s digital_raw:%d, dual_output_flag:%d, is_iec61937:%d, is_dtscd:%d"
        , __func__, dts_config->digital_raw, aml_out->dual_output_flag, dts_config->is_iec61937, dts_config->is_dtscd);
    return;
}

static void mad_decoder_config_prepare(struct audio_stream_out *stream, aml_mad_config_t * mad_config){
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    mad_config->channel    = aml_out->hal_ch;
    mad_config->samplerate = aml_out->hal_rate;
    mad_config->mpeg_format = aml_out->hal_format;
    return;
}

static void faad_decoder_config_prepare(struct audio_stream_out *stream, aml_faad_config_t * faad_config){
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;

    faad_config->channel    = aml_out->hal_ch;
    faad_config->samplerate = aml_out->hal_rate;
    faad_config->aac_format = aml_out->hal_format;

    return;
}

static void pcm_decoder_config_prepare(struct audio_stream_out *stream, aml_pcm_config_t * pcm_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;

    pcm_config->channel    = aml_out->hal_ch;
    pcm_config->samplerate = aml_out->hal_rate;
    pcm_config->pcm_format = aml_out->hal_format;
    pcm_config->max_out_channels = adev->hdmi_descs.pcm_fmt.max_channels;

    return;
}

int aml_decoder_config_prepare(struct audio_stream_out *stream, audio_format_t format, aml_dec_config_t * dec_config)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = NULL;
    if (patch ) {
        demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    }

    if (demux_info) {
        dec_config->ad_decoder_supported = demux_info->dual_decoder_support;
        dec_config->ad_mixing_enable = demux_info->associate_audio_mixing_enable;
        dec_config->mixer_level = adev->mixing_level;
        dec_config->advol_level = adev->advol_level;
        ALOGI("mixer_level %d adev->associate_audio_mixing_enable %d",adev->mixing_level, demux_info->associate_audio_mixing_enable);
    }

    switch (format) {
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        ddp_decoder_config_prepare(stream, &dec_config->dcv_config);
        break;
    }
    case AUDIO_FORMAT_DTS:
    case AUDIO_FORMAT_DTS_HD: {
        dts_decoder_config_prepare(stream, &dec_config->dca_config);
        break;
    }
    case AUDIO_FORMAT_PCM_16_BIT:
    case AUDIO_FORMAT_PCM_32_BIT:
    case AUDIO_FORMAT_PCM_8_BIT:
    case AUDIO_FORMAT_PCM_8_24_BIT: {
        pcm_decoder_config_prepare(stream, &dec_config->pcm_config);
        break;
    }
    case AUDIO_FORMAT_MP3:
    case AUDIO_FORMAT_MP2: {
        mad_decoder_config_prepare(stream, &dec_config->mad_config);
        break;
    }
    case AUDIO_FORMAT_AAC:
    case AUDIO_FORMAT_AAC_LATM: {
        faad_decoder_config_prepare(stream, &dec_config->faad_config);
        break;
    }
    default:
        break;

    }

    return 0;
}


