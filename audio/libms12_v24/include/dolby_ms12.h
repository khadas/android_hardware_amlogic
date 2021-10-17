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

#ifndef _DOLBY_MS12_C_H_
#define _DOLBY_MS12_C_H_



#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>


//get the handle of dlopen "/odm/lib/ms12/libdolbyms12.so"
int get_libdolbyms12_handle(char *dolby_ms12_path);

//release the handle of dlopen
int release_libdolbyms12_handle(void);

/*@@
    @brief dolby ms12 self cleanup
*/
void dolby_ms12_self_cleanup(void);

/*@@
    @brief get dolby ms12 output max size(bytes)
*/
int get_dolby_ms12_output_max_size(void);
/*@@
    @brief Get the handle "dolby_mS12_pointer" as dolby ms12 with config argv[][] and argc

    @int argc
    @char **argv
*/
void * dolby_ms12_init(int argc, char **argv);

//release dolby ms12
void dolby_ms12_release(void *dolby_mS12_pointer);

/*@@
    @brief Input main[dolby/he-aac/pcm]
    @if single input as pcm, use this api
    @if dual input such as "multi-pcm + stereo&16bites pcm", we choose multi-pcm through this api

    @void *dolby_mS12_pointer //dolby ms12 handle
    @const void *audio_stream_out_buffer //main input buffer address
    @size_t audio_stream_out_buffer_size //main input buffer size
    @int audio_stream_out_format //main format
    @int audio_stream_out_channel_num //main channel num
    @int audio_stream_out_sample_rate //main sample rate
*/
int dolby_ms12_input_main(void *dolby_mS12_pointer
                          , const void *input_main_buffer
                          , size_t audio_stream_out_buffer_size
                          , int audio_stream_out_format
                          , int audio_stream_out_channel_num
                          , int audio_stream_out_sample_rate);


/*@@
    @brief input associate data, format contains [dolby/he-aac], and here the input main format is same as the associate format

    @void *dolby_mS12_pointer //dolby ms12 handle
    @const void *audio_stream_out_buffer //associate input buffer address
    @size_t audio_stream_out_buffer_size //associate input buffer size
    @int audio_stream_out_format //associate format
    @int audio_stream_out_channel_num //associate channel num
    @int audio_stream_out_sample_rate //associate sample rate
*/
int dolby_ms12_input_associate(void *dolby_mS12_pointer
                               , const void *audio_stream_out_buffer
                               , size_t audio_stream_out_buffer_size
                               , int audio_stream_out_format
                               , int audio_stream_out_channel_num
                               , int audio_stream_out_sample_rate
                              );


/*@@
    @brief Input system data, format is stereo-16bits PCM.
    @when input dual stream, such as "dolby + pcm" or "multi-pcm + stereo&16bits-pcm"

    @void *dolby_mS12_pointer //dolby ms12 handle
    @const void *audio_stream_out_buffer //system input buffer address
    @size_t audio_stream_out_buffer_size //system input buffer size
    @int audio_stream_out_format //system format
    @int audio_stream_out_channel_num //system channel num
    @int audio_stream_out_sample_rate //system sample rate
*/
int dolby_ms12_input_system(void *dolby_mS12_pointer
                            , const void *audio_stream_out_buffer
                            , size_t audio_stream_out_buffer_size
                            , int audio_stream_out_format
                            , int audio_stream_out_channel_num
                            , int audio_stream_out_sample_rate);

int dolby_ms12_input_app(void *dolby_mS12_pointer
                            , const void *audio_stream_out_buffer
                            , size_t audio_stream_out_buffer_size
                            , int audio_stream_out_format
                            , int audio_stream_out_channel_num
                            , int audio_stream_out_sample_rate);


#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK

/*@@
    @brief register the output callback

    @void *callback //output callback handle
    @void *priv_data //priv data
*/
int dolby_ms12_register_output_callback(void *callback, void *priv_data);

#else
/*@@
    @brief Get output data

    @void *dolby_mS12_pointer //dolby ms12 handle
    @const void *ms12_out_buffer //output buffer address
    @size_t request_out_buffer_size //request data size
*/
int dolby_ms12_output(void *dolby_mS12_pointer
                      , const void *ms12_out_buffer
                      , size_t request_out_buffer_size
                     );
#endif

/*@@
    @brief get all the runtime config params, as the style of "int argc, char **argv"

    @void *dolby_mS12_pointer //dolby ms12 handle
    @int argc
    @char **argv
*/
int dolby_ms12_update_runtime_params(void *dolby_mS12_pointer, int argc, char **argv);

/*@@
    @brief get all the runtime config params, as the style of "int argc, char **argv"

    @void *dolby_mS12_pointer //dolby ms12 handle
    @int argc
    @char **argv
*/
int dolby_ms12_update_runtime_params_nolock(void *dolby_mS12_pointer, int argc, char **argv);


/*@@
    @brief dolby ms12 scheduler run

    @void *dolby_mS12_pointer //dolby ms12 handle
*/
int dolby_ms12_scheduler_run(void *dolbyMS12_pointer);

/*@@
    @brief set the quit flag for dolby ms12 scheduler run

    @int is_quit
*/
int dolby_ms12_set_quit_flag(int is_quit);

/*@@
    @brief flush ms12 input buffer(main/associate)
*/
void dolby_ms12_flush_input_buffer(void);

void dolby_ms12_flush_main_input_buffer(void);

void dolby_ms12_flush_app_input_buffer(void);

/*@@
    @brief get the n bytes consumed by ms12 decoder
*/
unsigned long long dolby_ms12_get_decoder_n_bytes_consumed(void *ms12_pointer, int format, int is_main);


/*@@
    @brief get the pcm output size

    @*all_output_size, all the data from ms12
    @*ms12_generate_zero_size, all the ms12 generate zero size
*/
void dolby_ms12_get_pcm_output_size(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size);

/*@@
    @brief get the bitstream output size

    @*all_output_size, all the data from ms12
    @*ms12_generate_zero_size, all the ms12 generate zero size
*/
void dolby_ms12_get_bitsteam_output_size(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size);

/*@@
    @brief get main buffer avail
*/
int dolby_ms12_get_main_buffer_avail(int *max_size);

/*@@
    @brief get associate buffer avail
*/
int dolby_ms12_get_associate_buffer_avail(void);

/*@@
    @brief get system buffer avail
*/
int dolby_ms12_get_system_buffer_avail(int * max_size);

void dolby_ms12_set_main_dummy(int type, int dummy);

int dolby_ms12_get_gain(int idx);


/*@@
    @brief get dolby atmos info
*/
int dolby_ms12_get_input_atmos_info();


/*@@
    @brief set the main audio volume
*/
int dolby_ms12_set_main_volume(float volume);

/*@@
    @brief set the mat stream profile
*/
int dolby_ms12_set_mat_stream_profile(int stream_profile);

/*@@
    @brief enable the atmos drop
*/
int dolby_ms12_enable_atmos_drop(int atmos_drop);

/*@@
    @brief get PCM's nframes which outputed by decoder
*/
unsigned long long dolby_ms12_get_decoder_nframes_pcm_output(void *ms12_pointer, int format, int is_main);


/*@@
    @brief set dolby-ms12's debug level
*/
void dolby_ms12_set_debug_level(int level);

/*@@
    @brief get the sys consumed size
*/
unsigned long long dolby_ms12_get_consumed_sys_audio();

/*@@
    @brief get the total delay(which means frame nums)
*/
int dolby_ms12_get_total_nframes_delay(void *ms12_pointer);

int dolby_ms12_hwsync_init_internal(void);

int dolby_ms12_hwsync_release_internal(void);

int dolby_ms12_hwsync_checkin_pts_internal(int offset, int apts);

/*@@
    @brief get the total delay for stereo out
*/
int dolby_ms12_get_latency_for_stereo_out(int *latency);

/*@@
    @brief get the total delay for multichannel out
*/
int dolby_ms12_get_latency_for_multichannel_out(int *latency);

/*@@
    @brief get the total delay for dap speaker out
*/
int dolby_ms12_get_latency_for_dap_speaker_out(int *latency);

/*@@
    @brief get the total delay for dap headphone out
*/
int dolby_ms12_get_latency_for_dap_headphone_out(int *latency);

/*@@
    @brief get the total delay for ddp out
*/
int dolby_ms12_get_latency_for_ddp_out(int *latency);

/*@@
    @brief get the total delay for dd out
*/
int dolby_ms12_get_latency_for_dd_out(int *latency);

/*@@
    @brief get the total delay for mat out
*/
int dolby_ms12_get_latency_for_mat_out(int *latency);

/**
 * @brief MAT Encoder init
 */
int dolby_ms12_mat_encoder_init
    (int b_lfract_precision                     /**< [in] 0: disable lfract precision. 1: enable lfract precision */
    , int b_chmod_locking                       /**< [in] 0: disable chmod locking. 1: enable  chmod locking */
    , unsigned int *p_matenc_maxoutbufsize      /**< [out] Pointer of MAT encoder mat outbuf size */
    , int b_iec_header                          /**< [in] 0: output raw mat format. 1: outptu IEC61937 format */
    , int dbg_enable                            /**< [in] 0: disable debug. 1: enable debug */
    , void **mat_enc_handle                     /**< [out] Pointer of MAT encoder handle */
    );



/**
 * @brief MAT Encoder cleanup
 */
void dolby_ms12_mat_encoder_cleanup
    (void *mat_enc_handle                       /**< [in] Pointer of MAT encoder handle */
    );

/**
 * @brief MAT Encoder decode process
 */
int dolby_ms12_mat_encoder_process
    (void *mat_enc_handle               /**< [in] Pointer of MAT encoder handle */
    , const unsigned char *in_buf       /**< [in] Pointer of mlp data buffer */
    , int n_bytes_in_buf                /**< [in] Size in bytes of in_buf */
    , const unsigned char *out_buf      /**< [out] Pointer of mat data buffer output by MAT encoder */
    , int *n_bytes_out_buf              /**< [out] Size in bytes of mat data buffer output by MAT encoder */
    , int out_buf_max_size              /**< [in] Max size in bytes of in_buf buffer */
    , int *nbytes_consumed              /**< [out] Size in bytes of consumed in_buf data */
    );



/**
 * @brief This function set parameter for MAT Encoder.
 */
int dolby_ms12_mat_encoder_config
    (void *mat_enc_handle   /**< [in] Pointer of MAT encoder handle */
    , int config_type       /**< [in] mat encoder config type */
    , int *config           /**< [in] mat encoder config value */
    );

#ifdef __cplusplus
}
#endif

#endif //end of _DOLBY_MS12_C_H_
