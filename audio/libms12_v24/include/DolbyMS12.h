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

#ifndef _DOLBY_MS12_H_
#define _DOLBY_MS12_H_

#include <utils/Mutex.h>

#include "DolbyMS12ConfigParams.h"

#ifdef __cplusplus

typedef enum  {
    MS12_CONFIG_MAIN_VOLUME,
    MS12_CONFIG_MAT_STREAM_PROFILE,
    MS12_CONFIG_GAME_MODE,
    MS12_CONFIG_ATMOS_DROP,    /*drop the frist 2 frames for atmos case*/
    MS12_CONFIG_MIXER_MAX_SIZE_ENABLED, /*enable the mixer max size to 1536, this can save cpu bandwidth*/
    MS12_CONFIG_STEREO_OUT_LATENCY,
    MS12_CONFIG_MULTICHANNEL_OUT_LATENCY,
    MS12_CONFIG_DAP_SPEAKER_OUT_LATENCY,
    MS12_CONFIG_DAP_HEADPHONE_OUT_LATENCY,
    MS12_CONFIG_DDP_OUT_LATENCY,
    MS12_CONFIG_DD_OUT_LATENCY,
    MS12_CONFIG_MAT_OUT_LATENCY,
}ms12_config_type_t;

typedef union ms12_config {
    float main_volume;
    int mat_stream_profile;
}ms12_config_t;

typedef enum  {
    MAT_ENC_CONFIG_MIXER_LEVEL, //runtime param
    MAT_ENC_CONFIG_OUT_BITDEPTH, //static param
    MAT_ENC_CONFIG_OUT_CH, //static param
} mat_enc_config_type_t;

typedef union mat_config {
    int mixer_level;
    int out_bitdepth;
    int out_ch;
} mat_enc_config_t;

struct aml_audio_info{
    int is_dolby_atmos;
    int reserved_a;
    int reserved_b;
    int reserved_c;
};

namespace android
{
typedef int (*output_callback)(void *buffer, void *priv, size_t size);

class DolbyMS12
{

public:
    // static DolbyMS12* getInstance();

    DolbyMS12();
    virtual ~DolbyMS12();
    virtual int     GetLibHandle(char *dolby_ms12_path);
    virtual void    ReleaseLibHandle(void);
    virtual int     GetMS12OutputMaxSize(void);
    virtual void *  DolbyMS12Init(int configNum, char **configParams);
    virtual void    DolbyMS12Release(void *dolbyMS12_pointer);
    virtual int     DolbyMS12InputMain(
        void *dolbyMS12_pointer
        , const void *audio_stream_out_buffer //ms12 input buffer
        , size_t audio_stream_out_buffer_size //ms12 input buffer size
        , int audio_stream_out_format
        , int audio_stream_out_channel_num
        , int audio_stream_out_sample_rate
    );
    virtual int     DolbyMS12InputAssociate(
        void *dolbyMS12_pointer
        , const void *audio_stream_out_buffer //ms12 input buffer
        , size_t audio_stream_out_buffer_size //ms12 input buffer size
        , int audio_stream_out_format
        , int audio_stream_out_channel_num
        , int audio_stream_out_sample_rate
    );
    virtual int     DolbyMS12InputSystem(
        void *dolbyMS12_pointer
        , const void *audio_stream_out_buffer //ms12 input buffer
        , size_t audio_stream_out_buffer_size //ms12 input buffer size
        , int audio_stream_out_format
        , int audio_stream_out_channel_num
        , int audio_stream_out_sample_rate
    );
    virtual int     DolbyMS12InputApp(
        void *dolbyMS12_pointer
        , const void *audio_stream_out_buffer //ms12 input buffer
        , size_t audio_stream_out_buffer_size //ms12 input buffer size
        , int audio_stream_out_format
        , int audio_stream_out_channel_num
        , int audio_stream_out_sample_rate
    );

#ifdef REPLACE_OUTPUT_BUFFER_WITH_CALLBACK

    virtual int     DolbyMS12RegisterOutputCallback(output_callback callback, void *priv_data);

#else

    virtual int     DolbyMS12Output(
        void *dolbyMS12_pointer
        , const void *ms12_out_buffer //ms12 output buffer
        , size_t request_out_buffer_size //ms12 output buffer size
    );

#endif

    virtual int     DolbyMS12UpdateRuntimeParams(
        void *DolbyMS12Pointer
        , int configNum
        , char **configParams);

    virtual int     DolbyMS12UpdateRuntimeParamsNoLock(
        void *DolbyMS12Pointer
        , int configNum
        , char **configParams);

    virtual int     DolbyMS12SchedulerRun(void *DolbyMS12Pointer);

    virtual void    DolbyMS12SetQuitFlag(int is_quit);

    virtual void    DolbyMS12FlushInputBuffer(void);

    virtual void    DolbyMS12FlushMainInputBuffer(void);

    virtual void    DolbyMS12FlushAppInputBuffer(void);

    virtual void    DolbyMS12SetMainDummy(int type, int dummy);

    virtual unsigned long long DolbyMS12GetDecoderNBytesConsumed(void *ms12_pointer, int format, int is_main);

    virtual void    DolbyMS12GetPCMOutputSize(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size);

    virtual void    DolbyMS12GetBitstreamOutputSize(unsigned long long *all_output_size, unsigned long long *ms12_generate_zero_size);

    virtual int     DolbyMS12GetMainBufferAvail(int * max_size);

    virtual int     DolbyMS12GetAssociateBufferAvail(void);

    virtual int     DolbyMS12GetSystemBufferAvail(int * max_size);

    virtual int     DolbyMS12GetGain(int);

    virtual int     DolbyMS12SetMainVolume(float volume);
    virtual int     DolbyMS12SetMATStreamProfile(int stream_profile);
    virtual int     DolbyMS12SetAtmosDrop(int atmos_drop);
    virtual int     DolbyMS12GetInputISDolbyAtmos();

    virtual unsigned long long DolbyMS12GetDecoderNFramesPcmOutput(void *ms12_pointer, int format, int is_main);

    virtual void DolbyMS12SetDebugLevel(int);

    virtual unsigned long long DolbyMS12GetNBytesConsumedSysSound(void);

    virtual int DolbyMS12GetTotalNFramesDelay(void *);

    virtual int     DolbyMS12HWSyncInit(void);
    virtual int     DolbyMS12HWSyncRelease(void);
    virtual int     DolbyMS12HWSyncChecinPTS(int offset, int apts);
    virtual int     DolbyMS12GetLatencyForStereoOut(int *latency);
    virtual int     DolbyMS12GetLatencyForMultiChannelOut(int *latency);
    virtual int     DolbyMS12GetLatencyForDAPSpeakerOut(int *latency);
    virtual int     DolbyMS12GetLatencyForDAPHeadphoneOut(int *latency);
    virtual int     DolbyMS12GetLatencyForDDPOut(int *latency);
    virtual int     DolbyMS12GetLatencyForDDOut(int *latency);
    virtual int     DolbyMS12GetLatencyForMATOut(int *latency);
    virtual int     DolbyMS12MATEncoderInit
        (int b_lfract_precision
        , int b_chmod_locking
        , unsigned int *p_matenc_maxoutbufsize
        , int b_iec_header
        , int dbg_enable
        , void **mat_enc_handle
        );

    virtual void     DolbyMS12MatEncoderCleanup(void *mat_enc_handle);

    virtual int      DolbyMS12MATEncoderProcess
        (void *mat_enc_handle
        , const unsigned char *in_buf
        , int n_bytes_in_buf
        , const unsigned char *out_buf
        , int *n_bytes_out_buf
        , int out_buf_max_size
        , int *nbytes_consumed
        );


    virtual int      DolbyMS12MATEncoderConfig
        (void *mat_enc_handle
        , mat_enc_config_type_t config_type
        , mat_enc_config_t *config
        );
    // protected:


private:
    // DolbyMS12(const DolbyMS12&);
    // DolbyMS12& operator = (const DolbyMS12&);
    // static android::Mutex mLock;
    // static DolbyMS12 *gInstance;
    void *mDolbyMS12LibHanle;
};  // class DolbyMS12

}   // android
#endif


#endif  // _DOLBY_MS12_H_
