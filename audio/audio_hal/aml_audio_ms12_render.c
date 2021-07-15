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

#define LOG_TAG "aml_audio_ms12_render"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <cutils/log.h>


#include "audio_hw.h"
#include "audio_hw_utils.h"
#include "audio_dtv_utils.h"

#include "dolby_lib_api.h"
#include "aml_volume_utils.h"
#include "audio_hw_ms12.h"
#include "aml_audio_timer.h"
#include "alsa_config_parameters.h"

#define MS12_MAIN_WRITE_LOOP_THRESHOLD                  (2000)
#define AUDIO_IEC61937_FRAME_SIZE 4

extern unsigned long decoder_apts_lookup(unsigned int offset);

/*now th latency api is just used for DTV doing avsync by useing mediasync */
int aml_audio_get_cur_ms12_latency(struct audio_stream_out *stream) {

    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    struct aml_audio_patch *patch = adev->audio_patch;
    aml_demux_audiopara_t *demux_info = (aml_demux_audiopara_t *)patch->demux_info;
    int ms12_latencyms = 0;

    uint64_t inputnode_consumed = dolby_ms12_get_main_bytes_consumed(stream);
    uint64_t frames_generated = dolby_ms12_get_main_pcm_generated(stream);
    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {
        /*for ms12 dual_decoder_support input node latency can not be calculated, AC4 and MAT frame is not 32ms*/
        if ((demux_info && demux_info->dual_decoder_support) ||
            aml_out->hal_internal_format == AUDIO_FORMAT_AC4 ||
            aml_out->hal_internal_format == AUDIO_FORMAT_MAT) {
            ms12_latencyms = (frames_generated - ms12->master_pcm_frames) / 48;
       } else {
            ms12_latencyms = (ms12->ms12_main_input_size - inputnode_consumed) / aml_out->ddp_frame_size * 32 + (frames_generated - ms12->master_pcm_frames) / 48;
       }
    } else {
        ms12_latencyms = ((ms12->ms12_main_input_size - inputnode_consumed ) / 4 + frames_generated - ms12->master_pcm_frames) / 48;
    }
    if (adev->debug_flag)
        ALOGI("ms12_latencyms %d  ms12_main_input_size %lld inputnode_consumed %lld frames_generated %lld master_pcm_frames %lld",
        ms12_latencyms, ms12->ms12_main_input_size, inputnode_consumed,frames_generated, ms12->master_pcm_frames);
    return ms12_latencyms;

}

int aml_audio_ms12_process_wrapper(struct audio_stream_out *stream, const void *write_buf, size_t write_bytes)

{
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    int return_bytes = write_bytes;
    int ret = 0;
    int total_write = 0;
    void *buffer = (void *)write_buf;
    struct aml_audio_patch *patch = adev->audio_patch;
    int write_retry =0;
    size_t used_size = 0;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int ms12_write_failed = 0;
    int consume_size = 0,remain_size = 0,ms12_thredhold_size = 256;
    unsigned long long all_pcm_len1 = 0;
    unsigned long long all_pcm_len2 = 0;
    unsigned long long all_zero_len = 0;
    audio_format_t output_format = get_output_format (stream);
    if (adev->debug_flag) {
        ALOGD("%s:%d hal_format:%#x, output_format:0x%x, sink_format:0x%x",
            __func__, __LINE__, aml_out->hal_format, output_format, adev->sink_format);
    }

    remain_size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len1, &all_zero_len);

    if (is_bypass_dolbyms12(stream)) {
        if (adev->debug_flag) {
            ALOGI("%s passthrough dolbyms12, format %#x\n", __func__, aml_out->hal_format);
        }
        output_format = aml_out->hal_internal_format;
        if (audio_hal_data_processing (stream, write_buf, write_bytes, &output_buffer, &output_buffer_bytes, output_format) == 0)
            hw_write (stream, output_buffer, output_buffer_bytes, output_format);
    } else {
        /*not continuous mode, we use sink gain control the volume*/
        if (!continous_mode(adev)) {
            float out_gain = 1.0f;
            out_gain = adev->sink_gain[adev->active_outport];
            if (adev->tv_mute && adev->audio_patch) {
                out_gain = 0.0f;
            }
            /*
             * Because 32k/44.1kHz DDP local playback use the ms12 discontinuous mode. If call dolby_ms12_set_main_volume
             * w/o any restrictions, will set the MS12 volume always 1.0 with HDMI gain(1.000000). So limit the AudioPatch
             * source name to SRC_DTV/SRC_ATV/SRC_LINEIN.
             */
            if (adev->audio_patch && (adev->patch_src == SRC_DTV || adev->patch_src == SRC_ATV || adev->patch_src == SRC_LINEIN)) {
                dolby_ms12_set_main_volume(out_gain);
                aml_out->ms12_vol_ctrl = true;
            }
            /*when it is non continuous mode, we bypass data here*/
            dolby_ms12_bypass_process(stream, write_buf, write_bytes);
        }
        /*begin to write, clear the total write*/
        total_write = 0;
re_write:
        if (adev->debug_flag) {
            ALOGI("%s dolby_ms12_main_process before write_bytes %zu!\n", __func__, write_bytes);
        }

        used_size = 0;
        ret = dolby_ms12_main_process(stream, (char*)write_buf + total_write, write_bytes, &used_size);
        if (ret == 0) {
            if (adev->debug_flag) {
                ALOGI("%s dolby_ms12_main_process return %d, return used_size %zu!\n", __FUNCTION__, ret, used_size);
            }
            if (used_size < write_bytes && write_retry < MS12_MAIN_WRITE_LOOP_THRESHOLD) {
                if (adev->debug_flag) {
                    ALOGI("%s dolby_ms12_main_process used  %zu,write total %zu,left %zu\n", __FUNCTION__, used_size, write_bytes, write_bytes - used_size);
                }
                total_write += used_size;
                write_bytes -= used_size;
                /*if ms12 doesn't consume any data, we need sleep*/
                if (used_size == 0) {
                    aml_audio_sleep(1000);
                }
                if (adev->debug_flag >= 2) {
                    ALOGI("%s sleeep 1ms\n", __FUNCTION__);
                }
                write_retry++;
                if (adev->ms12.dolby_ms12_enable) {
                    goto re_write;
                }
            }
            if (write_retry >= MS12_MAIN_WRITE_LOOP_THRESHOLD) {
                ALOGE("%s main write retry time output,left %zu", __func__, write_bytes);
                //bytes -= write_bytes;
                ms12_write_failed = 1;
            }
        } else {
            ALOGE("%s dolby_ms12_main_process failed %d", __func__, ret);
        }
    }

    int size = dolby_ms12_get_main_buffer_avail(NULL);
    dolby_ms12_get_pcm_output_size(&all_pcm_len2, &all_zero_len);
    if (patch && patch->skip_amadec_flag)
        patch->decoder_offset += remain_size + write_bytes - size;

    return return_bytes;

}
int aml_audio_ms12_render(struct audio_stream_out *stream, const void *buffer, size_t bytes)
{
    int ret = -1;
    int dec_used_size = 0;
    int used_size = 0;
    int left_bytes = 0;
    struct aml_stream_out *aml_out = (struct aml_stream_out *) stream;
    struct aml_audio_device *adev = aml_out->dev;
    int return_bytes = bytes;
    struct aml_audio_patch *patch = adev->audio_patch;
    void *output_buffer = NULL;
    size_t output_buffer_bytes = 0;
    int out_frames = 0;
    int ms12_delayms = 0;
    bool bypass_aml_dec = false;
    bool do_sync_flag = adev->patch_src  == SRC_DTV && patch && patch->skip_amadec_flag;
    if (is_dolby_ms12_support_compression_format(aml_out->hal_internal_format)) {
        bypass_aml_dec = true;
    }

    if (bypass_aml_dec) {
        if (do_sync_flag) {
            if(patch->skip_amadec_flag) {
                if (patch->cur_package->pts == 0) {
                    patch->cur_package->pts = decoder_apts_lookup(patch->decoder_offset);
                }
            }
        }


        ret = aml_audio_ms12_process_wrapper(stream, buffer, bytes);
        if (do_sync_flag) {
            ms12_delayms = aml_audio_get_cur_ms12_latency(stream);
            if(patch->skip_amadec_flag && aml_out->dtvsync_enable) {
                patch->dtvsync->cur_outapts = patch->cur_package->pts - ms12_delayms * 90;
                aml_dtvsync_ms12_get_policy(stream);
            }
        }
    } else {
        if (aml_out->aml_dec == NULL) {
            config_output(stream, true);
        }
        aml_dec_t *aml_dec = aml_out->aml_dec;
        if (do_sync_flag) {
            if(patch->skip_amadec_flag) {
                if (patch->cur_package)
                    aml_dec->in_frame_pts = patch->cur_package->pts;
                if (aml_dec->in_frame_pts == 0) {
                     aml_dec->in_frame_pts = decoder_apts_lookup(patch->decoder_offset);
                }
            }
        }

        if (aml_dec) {
            dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
            dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
            dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;
            left_bytes = bytes;
            do {
                if (adev->debug_flag)
                    ALOGI("left_bytes %d dec_used_size %d", left_bytes, dec_used_size);
                ret = aml_decoder_process(aml_dec, (unsigned char *)buffer + dec_used_size, left_bytes, &used_size);

                if (ret < 0) {
                    ALOGV("aml_decoder_process error");
                    return return_bytes;
                }
                left_bytes -= used_size;
                dec_used_size += used_size;
                ALOGV("%s() ret =%d pcm len =%d raw len=%d", __func__, ret, dec_pcm_data->data_len, dec_raw_data->data_len);
                // write pcm data
                if (dec_pcm_data->data_len > 0) {
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
                    out_frames += dec_pcm_data->data_len /( 2 * dec_pcm_data->data_ch);
                    if (patch && (adev->patch_src  == SRC_DTV))
                        patch->dtv_pcm_writed += dec_pcm_data->data_len;
                    aml_dec->out_frame_pts = aml_dec->in_frame_pts + (90 * out_frames /(dec_pcm_data->data_sr / 1000));
                    //aml_audio_dump_audio_bitstreams("/data/mixing_data.raw", dec_data, dec_pcm_data->data_len);
                    ret = aml_audio_ms12_process_wrapper(stream, dec_data, dec_pcm_data->data_len);
                    if (do_sync_flag) {
                        ms12_delayms = aml_audio_get_cur_ms12_latency(stream);
                        if(patch->skip_amadec_flag) {
                            patch->dtvsync->cur_outapts = aml_dec->out_frame_pts - ms12_delayms * 90;//need consider the alsa delay
                            if (adev->debug_flag)
                                ALOGI("patch->dtvsync->cur_outapts %lld", patch->dtvsync->cur_outapts);
                            if (aml_out->dtvsync_enable)
                                aml_dtvsync_ms12_get_policy(stream);
                        }
                    }
                }

            } while ((left_bytes > 0) || aml_dec->fragment_left_size);
        }

    }

    return return_bytes;
}




