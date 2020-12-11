//#define LOG_NDEBUG 0
#define LOG_TAG "audio_es"
#include <RefBase.h>
#include <cutils/trace.h>
#include <cutils/properties.h>
#include <AmDemuxWrapper.h>
#include <AmHwDemuxWrapper.h>
#include <AmHwMultiDemuxWrapper.h>
#include <dmx.h>
extern "C" {
#include <audio_es.h>
}
AmHwMultiDemuxWrapper *demux_wrapper = NULL;
int audio_pid = -1;
AM_Dmx_Audio_ErrorCode_t init (int demux_id,int fmt, int pid, int security_mem_level) {
    Am_DemuxWrapper_OpenPara_t avpara;
    audio_pid = pid;
    ALOGI("init demux_id %d pid %d fmt %d security_mem_level %d",demux_id,pid,fmt,security_mem_level);
    Am_DemuxWrapper_OpenPara_t * pavpara = &avpara;
    pavpara->dev_no = demux_id;
    //pavpara->device_type = Dev_Type_AV;
    if (demux_wrapper == NULL)
        demux_wrapper = new AmHwMultiDemuxWrapper();
    demux_wrapper->AmDemuxWrapperOpen(pavpara);
    demux_wrapper->AmDemuxWrapperSetTSSource (pavpara,AM_AV_TSSource_t(demux_id));
    //demux_wrapper->AmDemuxWrapperSetVideoParam(0x100,VFORMAT_H264);
    demux_wrapper->AmDemuxWrapperSetAudioParam(pid,fmt,security_mem_level);
    demux_wrapper->AmDemuxWrapperStart();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t destroy() {
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    demux_wrapper->AmDemuxWrapperStop();
    demux_wrapper->AmDemuxWrapperClose();
    if (demux_wrapper)
        delete demux_wrapper;
    demux_wrapper = NULL;
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t get_audio_es_package(struct mAudioEsDataInfo  **mAudioEsData) {
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    //mEsDataInfo* mEsdata ;
    if (true) {
        ret = demux_wrapper->AmDemuxWrapperReadData(audio_pid, (mEsDataInfo **)mAudioEsData,1);
        ALOGV("get_audio_es_package ret %d mEsdata  %p",ret,*mAudioEsData);
        if (*mAudioEsData == NULL) {
            return AM_AUDIO_Dmx_ERROR;
        }
        if ((*mAudioEsData)->size == 0) {
            return AM_AUDIO_Dmx_ERROR;
        }

        ALOGV("mEsdata->pts : %" PRId64 " size:%d \n",(*mAudioEsData)->pts,(*mAudioEsData)->size);
    } else {
         ALOGI("ReadBuffer::OnReadBuffer end!!\n");
         return AM_AUDIO_Dmx_ERROR;
    }
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}
