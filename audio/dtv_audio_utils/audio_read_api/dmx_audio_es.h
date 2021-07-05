#ifndef _AUDIO_DVB_ES_H_
#define _AUDIO_DVB_ES_H_
#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)
#define VALID_AD_FMT(fmt)  ((fmt == ACODEC_FMT_EAC3) || (fmt == ACODEC_FMT_AC3) || \
    (fmt == ACODEC_FMT_MPEG) || (fmt == ACODEC_FMT_MPEG1) || \
    (fmt == ACODEC_FMT_MPEG2) || (fmt == ACODEC_FMT_AAC) || (fmt == ACODEC_FMT_AAC_LATM))

#define  DVB_DEMUX_ID_BASE 20
#define DVB_DEMUX_SUPPORT_MAX_NUM 6

typedef enum
{
    AM_AUDIO_Dmx_SUCCESS,
    AM_AUDIO_Dmx_ERROR,
    AM_AUDIO_Dmx_DEVOPENFAIL,
    AM_AUDIO_Dmx_SETSOURCEFAIL,
    AM_AUDIO_Dmx_NOT_SUPPORTED,
    AM_AUDIO_Dmx_CANNOT_OPEN_FILE,
    AM_AUDIO_Dmx_ERR_SYS,
    AM_AUDIO_Dmx_MAX,
} AM_Dmx_Audio_ErrorCode_t;

typedef struct aml_demux__audiopara {
    int demux_id;
    int security_mem_level;
    int output_mode;
    bool has_video;
    int main_fmt;
    int main_pid;
    int ad_fmt;
    int ad_pid;
    int dual_decoder_support;
    int associate_audio_mixing_enable;
    int media_sync_id;
    int ad_package_status;
    struct mAudioEsDataInfo *mEsData;
    struct mAudioEsDataInfo *mADEsData;
} aml_demux_audiopara_t;


typedef enum {
    DTVSYNC_AUDIO_UNKNOWN = 0,
    DTVSYNC_AUDIO_NORMAL_OUTPUT,
    DTVSYNC_AUDIO_DROP_PCM,
    DTVSYNC_AUDIO_INSERT,
    DTVSYNC_AUDIO_HOLD,
    DTVSYNC_AUDIO_MUTE,
    DTVSYNC_AUDIO_RESAMPLE,
    DTVSYNC_AUDIO_ADJUST_CLOCK,
} dtvsync_policy;


struct dtvsync_audio_policy {
    dtvsync_policy audiopolicy;
    int32_t  param1;
    int32_t  param2;
};


typedef struct  aml_dtvsync {
    bool use_mediasync;
    void* mediasync;
    void* mediasync_new;
    int mediasync_id;
    int64_t cur_outapts;
    int cur_speed;
    struct dtvsync_audio_policy apolicy;
    int pcm_dropping;
    int duration;
    struct package *dtv_pacakge;
} aml_dtvsync_t;


typedef struct aml_dtv_audio_instances {
    int demux_index_working;
    int dvb_path_count;
    void *demux_handle[DVB_DEMUX_SUPPORT_MAX_NUM];
    aml_demux_audiopara_t demux_info[DVB_DEMUX_SUPPORT_MAX_NUM];
    aml_dtvsync_t dtvsync[DVB_DEMUX_SUPPORT_MAX_NUM];
} aml_dtv_audio_instances_t;

struct mAudioEsDataInfo {
    uint8_t *data;
    int size;
    int64_t pts;
    int used_size;
};
AM_Dmx_Audio_ErrorCode_t Open_Dmx_Audio (void **demux_handle, int  demux_id, int security_mem_level);
AM_Dmx_Audio_ErrorCode_t Init_Dmx_Main_Audio(void *demux_handle, int fmt, int pid);
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_Main_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Start_Dmx_Main_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_Main_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Init_Dmx_AD_Audio(void *demux_handle, int fmt, int pid);
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_AD_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Start_Dmx_AD_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_AD_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Close_Dmx_Audio(void *demux_handle);
AM_Dmx_Audio_ErrorCode_t Get_MainAudio_Es(void *demux_handle, struct mAudioEsDataInfo  **mAudioEsData);
AM_Dmx_Audio_ErrorCode_t Get_ADAudio_Es(void *demux_handle, struct mAudioEsDataInfo  **mAudioEsData);
AM_Dmx_Audio_ErrorCode_t Get_Audio_LastES_Apts(void *demux_handle , int64_t *last_queue_es_apts);

#endif
