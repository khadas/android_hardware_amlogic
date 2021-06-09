/*
 * Copyright (C) 2021 The Android Open Source Project
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
#include <cutils/properties.h>

#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "a2dp_hal.h"
#include "audio_hw_ms12.h"
#include "aml_audio_avsync_table.h"
#include "aml_audio_spdifout.h"

#define MS12_OUTPUT_5_1_DDP "vendor.media.audio.ms12.output.5_1_ddp"

static int get_ms12_dv_tunnel_input_latency(audio_format_t input_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_DV_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_DV_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_DV_TUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_DV_TUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }

    return latency_ms;
}

static int get_ms12_nontunnel_input_latency(audio_format_t input_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        prop_name = AVSYNC_MS12_NONTUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_NONTUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;

    }
    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }

    return latency_ms;
}


static int get_ms12_tunnel_input_latency(audio_format_t input_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_DDP_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC4: {
        prop_name = AVSYNC_MS12_TUNNEL_AC4_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_AC4_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }

    return latency_ms;
}


static int get_ms12_netflix_nontunnel_input_latency(audio_format_t input_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_DDP_LATENCY;
        break;
    }
    default:
        break;

    }
    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }

    return latency_ms;
}


static int get_ms12_netflix_tunnel_input_latency(audio_format_t input_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (input_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        /*for non tunnel ddp2h/heaac case:netlfix AL1 case */
        prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_PCM_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_PCM_LATENCY;
        break;
    }
    case AUDIO_FORMAT_AC3:
    case AUDIO_FORMAT_E_AC3: {
        /*for non tunnel dolby ddp5.1 case:netlfix AL1 case*/
        prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_DDP_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_DDP_LATENCY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }

    return latency_ms;
}


static int get_ms12_output_latency(audio_format_t output_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (output_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        latency_ms = AVSYNC_MS12_PCM_OUT_LATENCY;
        prop_name = AVSYNC_MS12_PCM_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        latency_ms = AVSYNC_MS12_DD_OUT_LATENCY;
        prop_name = AVSYNC_MS12_DD_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_E_AC3: {
        latency_ms = AVSYNC_MS12_DDP_OUT_LATENCY;
        prop_name = AVSYNC_MS12_DDP_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_MAT: {
        latency_ms = AVSYNC_MS12_MAT_OUT_LATENCY;
        prop_name = AVSYNC_MS12_MAT_OUT_LATENCY_PROPERTY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }
    ALOGV("%s output format =0x%x latency ms =%d", __func__, output_format, latency_ms);
    return latency_ms;

}

static int get_ms12_netflix_output_latency(audio_format_t output_format) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    switch (output_format) {
    case AUDIO_FORMAT_PCM_16_BIT: {
        latency_ms = AVSYNC_MS12_NETFLIX_PCM_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_PCM_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_AC3: {
        latency_ms = AVSYNC_MS12_NETFLIX_DD_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_DD_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_E_AC3: {
        latency_ms = AVSYNC_MS12_NETFLIX_DDP_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_DDP_OUT_LATENCY_PROPERTY;
        break;
    }
    case AUDIO_FORMAT_MAT: {
        latency_ms = AVSYNC_MS12_NETFLIX_MAT_OUT_LATENCY;
        prop_name = AVSYNC_MS12_NETFLIX_MAT_OUT_LATENCY_PROPERTY;
        break;
    }
    default:
        break;
    }

    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }
    ALOGV("%s output format =0x%x latency ms =%d", __func__, output_format, latency_ms);
    return latency_ms;

}


int get_ms12_port_latency( enum OUT_PORT port, audio_format_t output_format)
{
    int latency_ms = 0;
    switch (port)  {
        case OUTPORT_HDMI_ARC:
            if (output_format == AUDIO_FORMAT_AC3)
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_DD_LATENCY;
            else if (output_format == AUDIO_FORMAT_E_AC3)
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_DDP_LATENCY;
            else
                latency_ms = AVSYNC_MS12_HDMI_ARC_OUT_PCM_LATENCY;
            break;
        case OUTPORT_HDMI:
            latency_ms = AVSYNC_MS12_HDMI_OUT_LATENCY;
            break;
        case OUTPORT_SPEAKER:
        case OUTPORT_AUX_LINE:
            latency_ms = AVSYNC_MS12_HDMI_SPEAKER_LATENCY;
            break;
        default :
            break;
    }
    return latency_ms;
}


static int get_ms12_nontunnel_latency_offset(enum OUT_PORT port, audio_format_t input_format, audio_format_t output_format, bool is_netflix)
{
    int latency_ms = 0;
    int input_latency_ms = 0;
    int output_latency_ms = 0;
    int port_latency_ms = 0;
    if (is_netflix) {
        input_latency_ms  = get_ms12_netflix_nontunnel_input_latency(input_format);
        output_latency_ms = get_ms12_netflix_output_latency(output_format);
    } else {
        input_latency_ms  = get_ms12_nontunnel_input_latency(input_format);
        output_latency_ms = get_ms12_output_latency(output_format);
        port_latency_ms   = get_ms12_port_latency(port, output_format);
    }
    latency_ms = input_latency_ms + output_latency_ms + port_latency_ms;
    ALOGV("%s total latency =%d ms in=%d ms out=%d ms port=%d ms", __func__,
        latency_ms, input_latency_ms, output_latency_ms, port_latency_ms);
    return latency_ms;
}

static int get_ms12_tunnel_latency_offset(enum OUT_PORT port
    , audio_format_t input_format
    , audio_format_t output_format
    , bool is_netflix
    , bool is_output_ddp_atmos)
{
    int latency_ms = 0;
    int input_latency_ms = 0;
    int output_latency_ms = 0;
    int port_latency_ms = 0;
    int is_dv = getprop_bool(MS12_OUTPUT_5_1_DDP); /* suppose that Dolby Vision is under test */

    if (is_netflix) {
        input_latency_ms  = get_ms12_netflix_tunnel_input_latency(input_format);
        output_latency_ms = get_ms12_netflix_output_latency(output_format);
        if ((output_format == AUDIO_FORMAT_E_AC3) || (output_format == AUDIO_FORMAT_AC3)) {
            output_latency_ms += AVSYNC_MS12_NETFLIX_DDP_OUT_TUNNEL_TUNNING;
        }
    } else {
        /*
         * TODO:
         * found the different between DDP_JOC and DDP,
         * DDP_JOC will has more 32m(in MS12 pipeline) and 20ms(get_ms12_atmos_latency_offset)
         * and one 32ms in SPDIF Encoder delay for DDP_JOC, but test result is 16ms.
         * so, better to dig into this hard coding part.
         */
        input_latency_ms  = get_ms12_tunnel_input_latency(input_format)
                            + is_output_ddp_atmos * AVSYNC_MS12_TUNNEL_DIFF_DDP_JOC_VS_DDP_LATENCY;
        if (is_dv) {
            input_latency_ms += get_ms12_dv_tunnel_input_latency(input_format);
        }
        output_latency_ms = get_ms12_output_latency(output_format);
        port_latency_ms   = get_ms12_port_latency(port, output_format);
    }
    latency_ms = input_latency_ms + output_latency_ms + port_latency_ms;
    ALOGV("%s total latency =%d ms in=%d ms out=%d ms(is output ddp_atmos %d) port=%d ms", __func__,
        latency_ms, input_latency_ms, output_latency_ms, is_output_ddp_atmos, port_latency_ms);
    return latency_ms;
}


int get_ms12_atmos_latency_offset(bool tunnel, bool is_netflix)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    if (is_netflix) {
        if (tunnel) {
            /*tunnel atmos case*/
            prop_name = AVSYNC_MS12_NETFLIX_TUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NETFLIX_TUNNEL_ATMOS_LATENCY;
        } else {
            /*non tunnel atmos case*/
            prop_name = AVSYNC_MS12_NETFLIX_NONTUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NETFLIX_NONTUNNEL_ATMOS_LATENCY;
        }
    } else {
        if (tunnel) {
            /*tunnel atmos case*/
            prop_name = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_TUNNEL_ATMOS_LATENCY;
        } else {
            /*non tunnel atmos case*/
            prop_name = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY_PROPERTY;
            latency_ms = AVSYNC_MS12_NONTUNNEL_ATMOS_LATENCY;
        }
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}


int get_ms12_bypass_latency_offset(bool tunnel)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;
    if (tunnel) {
        /*tunnel atmos case*/
        prop_name = AVSYNC_MS12_TUNNEL_BYPASS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_TUNNEL_BYPASS_LATENCY;
    } else {
        /*non tunnel atmos case*/
        prop_name = AVSYNC_MS12_NONTUNNEL_BYPASS_LATENCY_PROPERTY;
        latency_ms = AVSYNC_MS12_NONTUNNEL_BYPASS_LATENCY;
    }
    ret = property_get(prop_name, buf, NULL);
    if (ret > 0) {
        latency_ms = atoi(buf);
    }
    return latency_ms;
}


uint32_t out_get_ms12_latency_frames(struct audio_stream_out *stream)
{
    struct aml_stream_out *hal_out = (struct aml_stream_out *)stream;
    snd_pcm_sframes_t frames = 0;
    struct snd_pcm_status status;
    uint32_t whole_latency_frames;
    int ret = 0;
    struct aml_audio_device *adev = hal_out->dev;
    struct aml_stream_out *ms12_out = NULL;
    struct pcm_config *config = &adev->ms12_config;
    int mul = 1;

    if (continous_mode(adev)) {
        ms12_out = adev->ms12_out;
    } else {
        ms12_out = hal_out;
    }

    if (ms12_out == NULL) {
        return 0;
    }

    if (ms12_out->out_device & AUDIO_DEVICE_OUT_ALL_A2DP) {
        return a2dp_out_get_latency(adev) * ms12_out->hal_rate / 1000;
    }

    whole_latency_frames = config->start_threshold;
    if (!ms12_out->pcm || !pcm_is_ready(ms12_out->pcm)) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_STATUS, &status);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    if (status.state != PCM_STATE_RUNNING && status.state != PCM_STATE_DRAINING) {
        return whole_latency_frames / mul;
    }

    ret = pcm_ioctl(ms12_out->pcm, SNDRV_PCM_IOCTL_DELAY, &frames);
    if (ret < 0) {
        return whole_latency_frames / mul;
    }
    ALOGV("%s frames =%ld mul=%d", __func__, frames, mul);
    return frames / mul;
}

uint32_t out_get_ms12_bitstream_latency_ms(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int bitstream_delay_ms = 0;
    struct bitstream_out_desc *bitstream_out = &ms12->bitstream_out[BITSTREAM_OUTPUT_A];
    int ret = 0;

    bitstream_delay_ms = aml_audio_spdifout_get_delay(bitstream_out->spdifout_handle);

    return bitstream_delay_ms;
}



static int aml_audio_output_ddp_atmos(struct audio_stream_out *stream)
{
    int ret = 0;
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;

    bool is_atmos_supported = is_platform_supported_ddp_atmos(
                            adev->hdmi_descs.ddp_fmt.atmos_supported
                            , adev->active_outport
                            , adev->is_TV);

    bool is_ddp_atmos_format = (out->hal_format == AUDIO_FORMAT_E_AC3_JOC);

    return (is_atmos_supported && is_ddp_atmos_format);
}

static int get_ms12_tunnel_video_delay(void) {
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;
    int latency_ms = 0;
    char *prop_name = NULL;

    prop_name= AVSYNC_MS12_TUNNEL_VIDEO_DELAY_PROPERTY;
    latency_ms = AVSYNC_MS12_TUNNEL_VIDEO_DELAY;
    if (prop_name) {
        ret = property_get(prop_name, buf, NULL);
        if (ret > 0) {
            latency_ms = atoi(buf);
        }
    }
    ALOGV("%s latency ms =%d", __func__, latency_ms);
    return latency_ms;

}


int aml_audio_get_ms12_tunnel_latency(struct audio_stream_out *stream)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int32_t latency_frames = 0;
    int32_t alsa_delay = 0;
    int32_t tunning_delay = 0;
    int32_t ms12_pipeline_delay = 0;
    int32_t atmos_tunning_delay = 0;
    int32_t bypass_delay = 0;
    int32_t video_delay = 0;
    bool is_output_ddp_atmos = aml_audio_output_ddp_atmos(stream);

    /*we need get the correct ms12 out pcm */
    alsa_delay = (int32_t)out_get_ms12_latency_frames(stream);
    //ALOGI("latency_frames =%d", latency_frames);
    tunning_delay = get_ms12_tunnel_latency_offset(adev->active_outport,
                                                      out->hal_internal_format,
                                                      adev->ms12.optical_format,
                                                      adev->is_netflix,
                                                      is_output_ddp_atmos) * 48;

    if ((adev->ms12.is_dolby_atmos && adev->ms12_main1_dolby_dummy == false) || adev->atoms_lock_flag) {
        /*
         * In DV AV sync, the ATMOS(DDP_JOC) item, it will add atmos_tunning_delay into the latency_frames.
         * If other case choose an diff value, here seperate by is_netflix.
         */
        atmos_tunning_delay = get_ms12_atmos_latency_offset(true, adev->is_netflix) * 48;
    }
    /*ms12 pipe line has some delay, we need consider it*/
    ms12_pipeline_delay = dolby_ms12_main_pipeline_latency_frames(stream);

    if (adev->ms12.is_bypass_ms12) {
        bypass_delay = get_ms12_bypass_latency_offset(true) * 48;
    }

    if (adev->is_TV) {
        video_delay = get_ms12_tunnel_video_delay() * 48;
    }

    latency_frames = alsa_delay + tunning_delay + atmos_tunning_delay + ms12_pipeline_delay + bypass_delay + video_delay;

    ALOGV("latency frames =%d alsa delay=%d ms tunning delay=%d ms ms12 pipe =%d ms atmos =%d ms video delay %d ms",
        latency_frames, alsa_delay / 48, tunning_delay / 48, ms12_pipeline_delay / 48, atmos_tunning_delay / 48, video_delay / 48);
    return latency_frames;
}


int aml_audio_get_ms12_presentation_position(const struct audio_stream_out *stream, uint64_t *frames, struct timespec *timestamp)
{
    struct aml_stream_out *out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = out->dev;
    int frame_latency = 0, timems_latency = 0;
    bool b_raw_in = false;
    bool b_raw_out = false;
    uint64_t frames_written_hw = out->last_frames_postion;

    if (frames_written_hw == 0) {
        ALOGV("%s(), not ready yet", __func__);
        return -EINVAL;
    }
    *frames = frames_written_hw;
    *timestamp = out->lasttimestamp;

    if (adev->continuous_audio_mode) {
        if (direct_continous((struct audio_stream_out *)stream)) {
            frames_written_hw = adev->ms12.last_frames_postion;
            *timestamp = adev->ms12.timestamp;
        }

        if (out->is_normal_pcm && adev->ms12.dolby_ms12_enable) {
            frames_written_hw = adev->ms12.sys_audio_frame_pos;
            *timestamp = adev->ms12.sys_audio_timestamp;
        }

        *frames = frames_written_hw;

        if (adev->ms12.is_bypass_ms12) {
            frame_latency = get_ms12_bypass_latency_offset(false) * 48;
        } else {
            frame_latency = get_ms12_nontunnel_latency_offset(adev->active_outport,
                                                               out->hal_internal_format,
                                                               adev->ms12.sink_format,
                                                               adev->is_netflix) * 48;
            if ((adev->ms12.is_dolby_atmos && adev->ms12_main1_dolby_dummy == false) || adev->atoms_lock_flag) {
                frame_latency += get_ms12_atmos_latency_offset(false, adev->is_netflix) * 48;
            }
        }
    }

    ALOGV("[%s]adev->active_outport %d out->hal_internal_format %x adev->ms12.sink_format %x adev->continuous_audio_mode %d \n",
            __func__,adev->active_outport, out->hal_internal_format, adev->ms12.sink_format, adev->continuous_audio_mode);
    ALOGV("[%s]adev->ms12.is_bypass_ms12 %d adev->ms12.is_dolby_atmos %d adev->ms12_main1_dolby_dummy %d adev->atmos_lock_flag %d\n",
            __func__,adev->ms12.is_bypass_ms12, adev->ms12.is_dolby_atmos, adev->ms12_main1_dolby_dummy, adev->atoms_lock_flag);
    ALOGV("[%s] frame_latency %d\n",__func__,frame_latency);

    if (frame_latency < 0) {
        *frames -= frame_latency;
    } else if (*frames >= (uint64_t)abs(frame_latency)) {
        *frames -= frame_latency;
    } else {
        *frames = 0;
    }
    if ((out->hal_rate != MM_FULL_POWER_SAMPLING_RATE) &&
        (!is_bypass_dolbyms12((struct audio_stream_out *)stream))) {
        *frames = (*frames * out->hal_rate) / MM_FULL_POWER_SAMPLING_RATE;
    }

    return 0;
}


uint32_t aml_audio_out_get_ms12_latency_frames(struct audio_stream_out *stream) {
    return out_get_ms12_latency_frames(stream);
}

int aml_audio_ms12_update_presentation_position(struct audio_stream_out *stream) {
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;


    return 0;
}
