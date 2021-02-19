#ifndef _AUDIO_DVB_ES_H_
#define _AUDIO_DVB_ES_H_

typedef enum
{
    AM_AUDIO__Dmx_SUCCESS,
    AM_AUDIO_Dmx_ERROR,
    AM_AUDIO_Dmx_DEVOPENFAIL,
    AM_AUDIO_Dmx_SETSOURCEFAIL,
    AM_AUDIO_Dmx_NOT_SUPPORTED,
    AM_AUDIO_Dmx_CANNOT_OPEN_FILE,
    AM_AUDIO_Dmx_ERR_SYS,
    AM_AUDIO_Dmx_MAX,
} AM_Dmx_Audio_ErrorCode_t;

struct mAudioEsDataInfo {
    uint8_t *data;
    int size;
    int64_t pts;
};
int aud_pid;
int aud_ad_pid;
AM_Dmx_Audio_ErrorCode_t Open_Dmx_Audio (int demux_id);
AM_Dmx_Audio_ErrorCode_t Init_Dmx_Main_Audio(int fmt, int pid, int security_mem_level);
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_Main_Audio();
AM_Dmx_Audio_ErrorCode_t Start_Dmx_Main_Audio();
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_Main_Audio();
AM_Dmx_Audio_ErrorCode_t Init_Dmx_AD_Audio(int fmt, int pid, int security_mem_level);
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_AD_Audio();
AM_Dmx_Audio_ErrorCode_t Start_Dmx_AD_Audio();
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_AD_Audio();
AM_Dmx_Audio_ErrorCode_t Close_Dmx_Audio();
AM_Dmx_Audio_ErrorCode_t Get_MainAudio_Es(struct mAudioEsDataInfo  **mAudioEsData);
AM_Dmx_Audio_ErrorCode_t Get_ADAudio_Es(struct mAudioEsDataInfo  **mAudioEsData);
#endif
