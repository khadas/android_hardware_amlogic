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

AM_Dmx_Audio_ErrorCode_t init (int demux_id, int fmt, int pid,int security_mem_level);
AM_Dmx_Audio_ErrorCode_t get_audio_es_package(struct mAudioEsDataInfo **mEsData);
AM_Dmx_Audio_ErrorCode_t destroy();
#endif
