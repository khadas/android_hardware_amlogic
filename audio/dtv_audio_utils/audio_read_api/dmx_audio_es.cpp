//#define LOG_NDEBUG 0
#define LOG_TAG "dmx_audio_es"
#include <RefBase.h>
#include <cutils/trace.h>
#include <cutils/properties.h>
#include <AmDemuxWrapper.h>
#include <AmHwDemuxWrapper.h>
#include <AmHwMultiDemuxWrapper.h>
#include <dmx.h>
extern "C" {
#include <dmx_audio_es.h>
}
AmHwMultiDemuxWrapper *demux_wrapper = NULL;
AM_Dmx_Audio_ErrorCode_t Open_Dmx_Audio (int demux_id) {
    Am_DemuxWrapper_OpenPara_t avpara;
    ALOGI("init demux_id %d ",demux_id);
    Am_DemuxWrapper_OpenPara_t * pavpara = &avpara;
    pavpara->dev_no = demux_id;
    //pavpara->device_type = Dev_Type_AV;
    if (demux_wrapper == NULL)
        demux_wrapper = new AmHwMultiDemuxWrapper();
    demux_wrapper->AmDemuxWrapperOpen(pavpara);
    demux_wrapper->AmDemuxWrapperSetTSSource (pavpara,AM_AV_TSSource_t(demux_id));
    //demux_wrapper->AmDemuxWrapperSetVideoParam(0x100,VFORMAT_H264);
    //demux_wrapper->AmDemuxWrapperSetAudioParam(pid,fmt,security_mem_level);
    //demux_wrapper->AmDemuxWrapperStart();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Init_Dmx_Main_Audio(int fmt, int pid, int security_mem_level) {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s fmt %d pid %d security_mem_level %d ",__FUNCTION__, fmt, pid, security_mem_level);
    aud_pid = pid;
    demux_wrapper->AmDemuxWrapperOpenMain(pid,fmt,security_mem_level);

    return AM_AUDIO__Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_Main_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStopMain();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Start_Dmx_Main_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStartMain();
    return AM_AUDIO__Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_Main_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperCloseMain();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Init_Dmx_AD_Audio(int fmt, int pid, int security_mem_level) {
    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s fmt %d pid %d security_mem_level %d ",__FUNCTION__, fmt, pid, security_mem_level);
    aud_ad_pid = pid;
    demux_wrapper->AmDemuxWrapperOpenAD(pid,fmt,security_mem_level);
    return AM_AUDIO__Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Stop_Dmx_AD_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStopAD();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Start_Dmx_AD_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStartAD();
    return AM_AUDIO__Dmx_SUCCESS;
}
AM_Dmx_Audio_ErrorCode_t Destroy_Dmx_AD_Audio() {

    if (demux_wrapper == NULL) {
        ALOGI("demux not open !!!");
        return AM_AUDIO_Dmx_ERROR;
    }
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperCloseAD();
    return AM_AUDIO__Dmx_SUCCESS;
}

AM_Dmx_Audio_ErrorCode_t Close_Dmx_Audio() {
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    ALOGI("%s %d",__FUNCTION__, __LINE__);
    demux_wrapper->AmDemuxWrapperStop();
    demux_wrapper->AmDemuxWrapperClose();
    if (demux_wrapper)
        delete demux_wrapper;
    demux_wrapper = NULL;
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t Get_MainAudio_Es(struct mAudioEsDataInfo  **mAudioEsData) {
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    //mEsDataInfo* mEsdata ;
    if (true) {
        ret = demux_wrapper->AmDemuxWrapperReadData(aud_pid, (mEsDataInfo **)mAudioEsData,1);
        ALOGV("get_audio_es_package ret %d mEsdata  %p",ret,*mAudioEsData);
        if (*mAudioEsData == NULL) {
            return AM_AUDIO_Dmx_ERROR;
        }
        if ((*mAudioEsData)->size == 0) {
            return AM_AUDIO_Dmx_ERROR;
        }

        ALOGV("mEsdata->pts : %lld size:%d \n",(*mAudioEsData)->pts,(*mAudioEsData)->size);
    } else {
         ALOGI("ReadBuffer::OnReadBuffer end!!\n");
         return AM_AUDIO_Dmx_ERROR;
    }
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

AM_Dmx_Audio_ErrorCode_t Get_ADAudio_Es(struct mAudioEsDataInfo  **mAudioEsData) {
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    //mEsDataInfo* mEsdata ;
    ret = demux_wrapper->AmDemuxWrapperReadData(aud_ad_pid, (mEsDataInfo **)mAudioEsData,1);
    ALOGV("Get_ADAudio_Es ret %d mEsdata  %p",ret,*mAudioEsData);
    if (*mAudioEsData == NULL) {
        return AM_AUDIO_Dmx_ERROR;
    }
    if ((*mAudioEsData)->size == 0) {
        return AM_AUDIO_Dmx_ERROR;
    }

    ALOGV("mEsdata->pts : %lld size:%d \n",(*mAudioEsData)->pts,(*mAudioEsData)->size);
    return (AM_Dmx_Audio_ErrorCode_t)ret;
}

