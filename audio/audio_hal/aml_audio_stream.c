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

#include "aml_alsa_mixer.h"
#include "aml_audio_stream.h"

#define min(a,b) (((a) < (b)) ? (a) : (b))
#define PCM  0/*AUDIO_FORMAT_PCM_16_BIT*/
#define DD   4/*AUDIO_FORMAT_AC3*/
#define AUTO 5/*choose by sink capability/source format/Digital format*/

/*
 *@brief get sink capability
 */
static audio_format_t get_sink_capability (struct aml_audio_device *adev)
{
    struct aml_arc_hdmi_desc *hdmi_desc = &adev->hdmi_descs;

    bool dd_is_support = hdmi_desc->dd_fmt.is_support;
    bool ddp_is_support = hdmi_desc->ddp_fmt.is_support;

    audio_format_t sink_capability = AUDIO_FORMAT_PCM_16_BIT;

    if (ddp_is_support) {
        sink_capability = AUDIO_FORMAT_E_AC3;
    } else if (dd_is_support) {
        sink_capability = AUDIO_FORMAT_AC3;
    }
    ALOGI ("%s dd support %d ddp support %d\n", __FUNCTION__, dd_is_support, ddp_is_support);
    return sink_capability;
}

/*
 *@brief get sink format by logic min(source format / digital format / sink capability)
 * For Speaker/Headphone output, sink format keep PCM-16bits
 * For optical output, min(dd, source format, digital format)
 * For HDMI_ARC output
 *      1.digital format is PCM, sink format is PCM-16bits
 *      2.digital format is dd, sink format is min (source format,  AUDIO_FORMAT_AC3)
 *      3.digital format is auto, sink format is min (source format, digital format)
 */
void get_sink_format (struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    /*set default value for sink_audio_format/optical_audio_format*/
    audio_format_t sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
    audio_format_t optical_audio_format = AUDIO_FORMAT_PCM_16_BIT;

    audio_format_t sink_capability = get_sink_capability (adev);
    audio_format_t source_format = aml_out->hal_internal_format;

    /*when device is HDMI_ARC*/
    ALOGI ("%s() Sink devices %#x Source format %#x digital_format(hdmi_format) %#x Sink Capability %#x\n",
           __FUNCTION__, adev->active_outport, aml_out->hal_internal_format, adev->hdmi_format, sink_capability);

    if ( (source_format != AUDIO_FORMAT_PCM_16_BIT) && \
         (source_format != AUDIO_FORMAT_AC3) && \
         (source_format != AUDIO_FORMAT_E_AC3) ) {
        /*unsupport format [dts/dts-hd/true-hd]*/
        ALOGI ("%s() source format %#x change to %#x", __FUNCTION__, source_format, AUDIO_FORMAT_PCM_16_BIT);
        source_format = AUDIO_FORMAT_PCM_16_BIT;
    }

    if (adev->active_outport == OUTPORT_HDMI_ARC) {
        switch (adev->hdmi_format) {
        case PCM:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        case DD:
            sink_audio_format = min (source_format, AUDIO_FORMAT_AC3);
            optical_audio_format = sink_audio_format;
            break;
        case AUTO:
            sink_audio_format = min (source_format, sink_capability);
            optical_audio_format = sink_audio_format;
            break;
        default:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        }
    }
    /*when device is SPEAKER/HEADPHONE*/
    else {
        switch (adev->hdmi_format) {
        case PCM:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        case DD:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = min (source_format, AUDIO_FORMAT_AC3);
            break;
        case AUTO:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = min (source_format, AUDIO_FORMAT_AC3);
            break;
        default:
            sink_audio_format = AUDIO_FORMAT_PCM_16_BIT;
            optical_audio_format = sink_audio_format;
            break;
        }
    }
    adev->sink_format = sink_audio_format;
    adev->optical_format = optical_audio_format;

    /* set the dual output format flag */
    if (adev->sink_format != adev->optical_format)
        aml_out->dual_output_flag = true;
    else
        aml_out->dual_output_flag = false;

    ALOGI ("%s sink_format %#x optical_format %#x, dual_output %d\n",
           __FUNCTION__, adev->sink_format, adev->optical_format, aml_out->dual_output_flag);
    return ;
}

bool is_hdmi_in_stable_hw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    int type = 0;
    int stable = 0;

    stable = aml_mixer_ctrl_get_int (AML_MIXER_ID_HDMI_IN_AUDIO_STABLE);
    if (!stable)
        return false;

    type = aml_mixer_ctrl_get_int (AML_MIXER_ID_SPDIFIN_AUDIO_TYPE);
    if (type != in->spdif_fmt_hw) {
        ALOGV ("%s(), in type changed from %d to %d", __func__, in->spdif_fmt_hw, type);
        in->spdif_fmt_hw = type;
        return false;
    }

    return true;
}

bool is_hdmi_in_stable_sw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    struct aml_audio_device *aml_dev = in->dev;
    struct aml_audio_patch *patch = aml_dev->audio_patch;
    audio_format_t fmt;

    /* now, only hdmiin->(spk, hp, arc) cases init the soft parser thread
     * TODO: init hdmiin->mix soft parser too
     */
    if (!patch)
        return true;

    fmt = audio_parse_get_audio_type (patch->audio_parse_para);
    if (fmt != in->spdif_fmt_sw) {
        ALOGV ("%s(), in type changed from %#x to %#x", __func__, in->spdif_fmt_sw, fmt);
        in->spdif_fmt_sw = fmt;
        return false;
    }

    return true;
}

bool is_atv_in_stable_hw (struct audio_stream_in *stream)
{
    struct aml_stream_in *in = (struct aml_stream_in *) stream;
    int type = 0;
    int stable = 0;

    stable = aml_mixer_ctrl_get_int (AML_MIXER_ID_ATV_IN_AUDIO_STABLE);
    if (!stable)
        return false;

    return true;
}

int set_audio_source (int audio_source)
{
    return aml_mixer_ctrl_set_int (AML_MIXER_ID_AUDIO_IN_SRC, audio_source);
}

int enable_HW_resample(int sr, int enable) {
    int value = 0;
    if (enable == 0) {
        aml_mixer_ctrl_set_int(AML_MIXER_ID_HW_RESAMPLE_ENABLE, HW_RESAMPLE_DISABLE);
        return 0;
    }

    /*normally, hdmi audio only support: 32K,44.1K,48K,88.2K,96K,176.4K,192K*/
    if (sr <= 32000)
        value = HW_RESAMPLE_32K;
    else if (sr <= 44100)
        value = HW_RESAMPLE_44K;
    else if (sr <= 48000)
        value = HW_RESAMPLE_48K;
    else if (sr <= 88200)
        value = HW_RESAMPLE_88K;
    else if (sr <= 96000)
        value = HW_RESAMPLE_96K;
    else if (sr <= 176400)
        value = HW_RESAMPLE_176K;
    else if (sr <= 192000)
        value = HW_RESAMPLE_192K;
    aml_mixer_ctrl_set_int(AML_MIXER_ID_HW_RESAMPLE_ENABLE, value);
    return 0;
}

