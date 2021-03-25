/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "AmHwDemuxWrapper"
//#include "tsp_platform.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <TSPMessage.h>
#include <AmDemuxWrapper.h>
#include <AmHwDemuxWrapper.h>
//#include "FileSystemIo.h"
//using namespace android;
#define STB_SOURCE_FILE            "/sys/class/stb/source"
#define STREAM_TS_FILE             "/dev/amstream_mpts"
#define STREAM_TS_SCHED_FILE      "/dev/amstream_mpts_sched"
#define AMVIDEO_FILE               "/dev/amvideo"
#define DSC_DEV_NAME               "/dev/dvb0.ca"
#define TRICKMODE_NONE      0x00
#define VALID_PID(_pid_) ((_pid_)>0 && (_pid_)<0x1fff)

#define PATH_LEN                 (64)
#define STATE_LEN                (30)
AM_DmxErrorCode_t Dsc_Open(Am_DemuxWrapper_OpenPara_t **mpara)
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    int fd;
    char buf[PATH_LEN] = {0};
    Am_DemuxWrapper_OpenPara_t * para = *mpara;
    AM_DSC_DevNo_t dev_no = (AM_DSC_DevNo_t)(para->dev_no);
    snprintf(buf, sizeof(buf), DSC_DEV_NAME"%d", dev_no);
    fd = open(buf, O_RDWR);

    if (fd == -1)
    {
        ALOGV("cannot open \"%s\" (%d:%s)", DSC_DEV_NAME, errno, strerror(errno));
        return AM_Dmx_ERROR;
    }
    para->dsc_fd = (void*)(long)fd;
    return AM_Dmx_SUCCESS;

}
AM_DmxErrorCode_t Dmx_SetTSSource(AM_DMX_DevNo_t dev_no, const AM_DMX_Source_t src)
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    char name[PATH_LEN] = {0};
    const char *cmd;
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;

    if (dev_no >= 0 && dev_no < DMX_DEV_MAX)
    {
        snprintf(name, sizeof(name), "/sys/class/stb/demux%d_source", dev_no);
    }
    switch (src)
    {
        case AM_DMX_SRC_TS0:
            cmd = "ts0";
        break;
        case AM_DMX_SRC_TS1:
            cmd = "ts1";
        break;
#if defined(CHIP_8226M) || defined(CHIP_8626X)
        case AM_DMX_SRC_TS2:
            cmd = "ts2";
        break;
#endif
        case AM_DMX_SRC_TS3:
            cmd = "ts3";
        break;
        case AM_DMX_SRC_HIU:
            cmd = "hiu";
        break;
        case AM_DMX_SRC_HIU1:
            cmd = "hiu1";
        break;
        default:
        ALOGV("do not support demux source %d", src);
    }
    /*if (FileSystem_writeFile(name,cmd) < 0)
        ret = AM_Dmx_ERROR;*/
    return ret;
}

AM_DmxErrorCode_t Av_SetTSSource( const AM_AV_TSSource_t src)
{
    const char *cmd;
    ALOGV("%s at #line %d src %d\n",__func__,__LINE__,src);
    switch (src)
    {
        case AM_AV_TS_SRC_DMX0:
            cmd = "dmx0";
        break;
        case AM_AV_TS_SRC_DMX1:
            cmd = "dmx1";
        break;
        case AM_AV_TS_SRC_DMX2:
            cmd = "dmx2";
        break;
        default:
            ALOGV("illegal ts source %d", src);
            return AM_Dmx_NOT_SUPPORTED;
        break;
    }
    /*if (FileSystem_writeFile(STB_SOURCE_FILE, cmd) < 0)
        return AM_Dmx_ERROR;*/

    return AM_Dmx_SUCCESS;
}
AM_DmxErrorCode_t Dsc_SetTSSource(AM_DSC_DevNo_t dev_no, AM_DSC_Source_t src)
{
    const char *cmd;
    char buf[PATH_LEN] = {0};
    ALOGV("%s at #line %d\n",__func__,__LINE__);

    switch (src)
    {
        case AM_DSC_SRC_DMX0:
        cmd = "dmx0";
        break;
        case AM_DSC_SRC_DMX1:
            cmd = "dmx1";
        break;
        case AM_DSC_SRC_DMX2:
            cmd = "dmx2";
        break;
        case AM_DSC_SRC_BYPASS:
            cmd = "bypass";
        break;

        default:
            return AM_Dmx_NOT_SUPPORTED;
        break;
    }

    snprintf(buf, sizeof(buf), "/sys/class/stb/dsc%d_source", dev_no);
    /*if (FileSystem_writeFile(buf, cmd) < 0)
        return AM_Dmx_SUCCESS;*/

    return  AM_Dmx_ERROR;
}

AM_DmxErrorCode_t Start_inject(Am_DemuxWrapper_OpenPara_t **mpara)
{
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    (void)mpara;
#if 0
    int vfd = -1, afd = -1;
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    Am_DemuxWrapper_OpenPara_t * para = *mpara;

    if (check_vfmt_support_sched(para->vid_fmt) == DMX_FALSE)
    {
        vfd = open(STREAM_TS_FILE, O_RDWR);
        if (vfd == -1)
        {
            ALOGV("cannot open device \"%s\"", STREAM_TS_FILE);
            return AM_Dmx_CANNOT_OPEN_FILE;
        }
        else
            ALOGV("open device \"%s\" ok 0x%x \n", STREAM_TS_FILE,vfd);
        para->vid_fd = para->aud_fd =vfd;
    }
    else
    {
        vfd = open(STREAM_TS_SCHED_FILE, O_RDWR);
        if (vfd == -1)
        {
            ALOGV("cannot open device \"%s\" \n", STREAM_TS_SCHED_FILE);
            return AM_Dmx_CANNOT_OPEN_FILE;
        }
        else
            ALOGV("open device \"%s\" ok \n", STREAM_TS_SCHED_FILE);

        para->vid_fd = para->aud_fd = afd = vfd;
    }
    if (para->drm_mode == AM_AV_DRM_WITH_SECURE_INPUT_BUFFER)
    {
        if (ioctl(vfd, AMSTREAM_IOC_SET_DRMMODE, (unsigned long)para->drm_mode) == -1)
        {
              ALOGV("set drm_mode with secure buffer failed\n");
              return AM_Dmx_ERR_SYS;
        }
        else
            ALOGV("ioctl 0x%x ok", AMSTREAM_IOC_SET_DRMMODE);
    }
    if (para->drm_mode == AM_AV_DRM_WITH_NORMAL_INPUT_BUFFER)
    {
        if (ioctl(vfd, AMSTREAM_IOC_SET_DRMMODE, (unsigned long)para->drm_mode) == -1)
        {
            ALOGV("set drm_mode with normal buffer failed\n");
            return AM_Dmx_ERR_SYS;
        }
        else
            ALOGV("ioctl 0x%x ok", AMSTREAM_IOC_SET_DRMMODE);
    }
    if (VALID_PID(para->vid_id))
    {
        dec_sysinfo_t am_sysinfo;
        ALOGV("%s at #line %d\n",__func__,__LINE__);

        if (ioctl(vfd, AMSTREAM_IOC_VFORMAT, para->vid_fmt) == -1)
        {
            ALOGV("set video format failed");
            return AM_Dmx_ERR_SYS;
        }
        if (ioctl(vfd, AMSTREAM_IOC_VID, para->vid_id) == -1)
        {
            ALOGV("set video PID failed");
            return AM_Dmx_ERR_SYS;
        }

        memset(&am_sysinfo,0,sizeof(dec_sysinfo_t));
        if (para->vid_fmt == VFORMAT_VC1)
        {
            am_sysinfo.format = VIDEO_DEC_FORMAT_WVC1;
            am_sysinfo.width  = 1920;
            am_sysinfo.height = 1080;
        }
        else if (para->vid_fmt == VFORMAT_H264)
        {
            am_sysinfo.format = VIDEO_DEC_FORMAT_H264;
            am_sysinfo.width  = 1920;
            am_sysinfo.height = 1080;
        }
        else if (para->vid_fmt == VFORMAT_AVS)
        {
            am_sysinfo.format = VIDEO_DEC_FORMAT_AVS;
            /*am_sysinfo.width  = 1920;
            am_sysinfo.height = 1080;*/
        }
        else if (para->vid_fmt == VFORMAT_HEVC)
        {
            am_sysinfo.format = VIDEO_DEC_FORMAT_HEVC;
            am_sysinfo.width  = 3840;
            am_sysinfo.height = 2160;
        }

        if (ioctl(vfd, AMSTREAM_IOC_SYSINFO, (unsigned long)&am_sysinfo) == -1)
        {
            ALOGV("set AMSTREAM_IOC_SYSINFO");
            return AM_Dmx_ERR_SYS;
        }
        ALOGV("%s at #line %d\n",__func__,__LINE__);
    }
    if (VALID_PID(para->aud_id))
    {
        ALOGV("%s at #line %d\n",__func__,__LINE__);

        if (ioctl(afd, AMSTREAM_IOC_AFORMAT, para->aud_fmt) == -1)
        {
            ALOGV("set audio format failed");
            return AM_Dmx_ERR_SYS;
        }
        if (ioctl(afd, AMSTREAM_IOC_AID, para->aud_id) == -1)
        {
            ALOGV("set audio PID failed");
            return AM_Dmx_ERR_SYS;
        }
#if 0
        if ((para->aud_fmt == AFORMAT_PCM_S16LE) || (para->aud_fmt == AFORMAT_PCM_S16BE) ||
                (para->aud_fmt == AFORMAT_PCM_U8)) {
            ioctl(afd, AMSTREAM_IOC_ACHANNEL, para->channel);
            ioctl(afd, AMSTREAM_IOC_SAMPLERATE, para->sample_rate);
            ioctl(afd, AMSTREAM_IOC_DATAWIDTH, para->data_width);
        }
#endif
        ALOGV("%s at #line %d\n",__func__,__LINE__);
    }
    if (VALID_PID(para->sub_id))
    {
        ALOGV("%s at #line %d\n",__func__,__LINE__);

        if (ioctl(vfd, AMSTREAM_IOC_SID, para->sub_id) == -1)
        {
            ALOGV("set subtitle PID failed");
            return AM_Dmx_ERR_SYS;
        }

        if (ioctl(vfd, AMSTREAM_IOC_SUB_TYPE, para->sub_type) == -1)
        {
            ALOGV("set subtitle type failed");
            return AM_Dmx_ERR_SYS;
        }
    }

    if (vfd != -1)
    {
        if (ioctl(vfd, AMSTREAM_IOC_PORT_INIT, 0) == -1)
        {
            ALOGV("amport init failed");
            return AM_Dmx_ERR_SYS;
        }

        para->cntl_fd = open(AMVIDEO_FILE, O_RDWR);
        if (para->cntl_fd == -1)
        {
            ALOGV("cannot open \"%s\"", AMVIDEO_FILE);
            return AM_Dmx_CANNOT_OPEN_FILE;
        }
        else
            ALOGV("open \"%s\" ok", AMVIDEO_FILE);
        ioctl(para->cntl_fd, AMSTREAM_IOC_TRICKMODE, TRICKMODE_NONE);
    }
#if 1
    if (VALID_PID(para->aud_id))
    {
        //audio_ops->adec_start_decode(afd, para->aud_fmt, has_video, &inj->adec);
    }
#endif
#endif
return ret;
}

AM_DmxErrorCode_t Inject_Write(Am_DemuxWrapper_OpenPara_t **mpara,uint8_t *data, int *size,uint64_t timeout)
{
  int ret = AM_Dmx_SUCCESS;
  (void)timeout;
  Am_DemuxWrapper_OpenPara_t * para = *mpara;
  uint8_t* needwrite = data;
  int send = *size;
  ALOGV("inject data send %d para->vid_fd 0x%x\n", send,para->vid_fd);
  if (send)
    {
        ret = write(para->vid_fd, needwrite, send);
        if ((ret == -1) && (errno != EAGAIN))
        {
            ALOGV("inject data failed errno:%d msg:%s \n", errno, strerror(errno));
            return AM_Dmx_ERR_SYS;
        }
        else if ((ret == -1) && (errno == EAGAIN))
        {
            ret = 0;
        }
    }
    else
        ret = 0;

    *size = ret;

    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t Inject_Pause(Am_DemuxWrapper_OpenPara_t **mpara, AV_PlayCmd_t cmd)
{
    //int ret = AM_Dmx_SUCCESS;
    (void)mpara;
    (void)cmd;
#if 0
    switch (cmd)
    {
        case AV_PLAY_PAUSE:
            if (para->aud_id != 0x1fff)
            {
                //audio_ops->adec_pause_decode(data->adec);
            }
            if (para->cntl_fd != -1)
            {
                ret = ioctl(para->cntl_fd, AMSTREAM_IOC_VPAUSE, 1);
            }
            ALOGV("pause inject");
        break;
        case AV_PLAY_RESUME:
            if (para->aud_id != 0x1fff)
            {
                //audio_ops->adec_resume_decode(data->adec);
            }
            if (para->cntl_fd != -1)
            {
                ret = ioctl(para->cntl_fd, AMSTREAM_IOC_VPAUSE, 0);
            }
            ALOGV("resume inject");
        break;
        default:
            ALOGV("illegal media player command");
            return AM_Dmx_NOT_SUPPORTED;
        break;
    }
#endif
    return AM_Dmx_SUCCESS;
}

AM_DmxErrorCode_t Inject_Stop(Am_DemuxWrapper_OpenPara_t **mpara)
{
    //AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    //Am_DemuxWrapper_OpenPara_t * para = *mpara;
    (void)mpara;
    ALOGV("%s at #line %d TODO\n",__func__,__LINE__);
    return AM_Dmx_SUCCESS;
}

AmHwDemuxWrapper::AmHwDemuxWrapper() {
    ALOGE("%s at # %d\n",__FUNCTION__,__LINE__);
    TSPMutex::Autolock l(mMutex);
    mDmxDevNo = DMX_DEV_UNKNOWN;
    mDSCDevNo = DSC_DEV_UNKNOWN;
    mAvDevNo = AV_DEV_UNKNOWN;
    mPara.vid_id = 0x1fff;
    mPara.aud_id = 0x1fff;
    mPara.sub_id = 0x1fff;
    mPara.vid_fmt = -1;
    mPara.aud_fmt = -1;
    mPara.sub_type = -1;
    mPara.drm_mode = AM_AV_NO_DRM;
    mPara.cntl_fd = -1;
    mPPara = &mPara;
}

AmHwDemuxWrapper::~AmHwDemuxWrapper()
{
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    TSPMutex::Autolock l(mMutex);
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperOpen (
                    Am_DemuxWrapper_OpenPara_t *para)
{

    ALOGV("%s at # %d device_type %d\n",__FUNCTION__,__LINE__,para->device_type);

    TSPMutex::Autolock l(mMutex);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    mPara.device_type = para->device_type ;
    if (para->device_type == (int )TS_FROM_MEMORY/*data from memory*/) {
        mPPara->dev_no =  para->dev_no;
        mDmxDevNo = (AM_DMX_DevNo_t)mPPara->dev_no;
        mAvDevNo = (AM_AV_DevNo_t)mPPara->dev_no;
        ret = Av_SetTSSource((AM_AV_TSSource_t)mPPara->dev_no);
        if (ret) {
            ALOGV("%s at #line %d ret 0x%x\n",__func__,__LINE__,ret);
        }
    } else if (para->device_type == (int)TS_FROM_DEMOD/*data from demod*/) {
        mPPara->dev_no =  para->dev_no;
        mDmxDevNo = (AM_DMX_DevNo_t)mPPara->dev_no;
        mAvDevNo = (AM_AV_DevNo_t)mPPara->dev_no;
        ret = Av_SetTSSource((AM_AV_TSSource_t)mPPara->dev_no);
        if (ret) {
            ALOGV("%s at #line %d ret 0x%x\n",__func__,__LINE__,ret);
        }
    }
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperSetTSSource(
                  Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src) {
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    TSPMutex::Autolock l(mMutex);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    (void) para;
    (void) src;
#if 0
    ret = Dmx_SetTSSource(mDmxDevNo,(AM_DMX_Source_t)src);
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    if (para->device_type == Dev_Type_DMX) //dmx
    {
        ret = Dmx_SetTSSource(mDmxDevNo,(AM_DMX_Source_t)src);
        ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);

    }
    if (para->device_type == Dev_Type_AV) //av
    {
        ret = Av_SetTSSource((AM_AV_TSSource_t)src);//av used the defaults
    }
    if (para->device_type == Dev_Type_DSC) //dsc
    {
        ret = Dsc_SetTSSource(mDSCDevNo, (AM_DSC_Source_t)src);
    }
#endif
    ALOGV("%s at # %d return %d\n",__FUNCTION__,__LINE__,ret);
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperStart()
{
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    Start_inject(&mPPara);
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperWriteData(
    Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout)
{
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    uint8_t * data = Pdata->data;
    int send = (Pdata->size);

    ALOGV("%s at # %d send %d\n",__FUNCTION__,__LINE__,send);
    if (send > 0)
        ret = Inject_Write(&mPPara,data, &send,timeout);
    if (!ret && pWroteLen != NULL)
        *pWroteLen = send;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperReadData(int pid, mEsDataInfo** mEsData,uint64_t timeout)
{
    ALOGV("%s at #line %d TODO\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    (void)pid;
    (void)mEsData;
    (void)timeout;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper:: AmDemuxWrapperFlushData(int pid)
{
    ALOGV("%s at #line %d TODO\n",__func__,__LINE__);
    (void)pid;
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperPause()
{
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    ret = Inject_Pause(&mPPara,AV_PLAY_PAUSE);
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperResume()
{
    ALOGV("%s at # %d\n",__FUNCTION__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    ret = Inject_Pause(&mPPara,AV_PLAY_RESUME);
    return ret ;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt)
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    mVid_id  = vid;
    mVid_fmt = vfmt;
    mPkg_fmt = PFORMAT_TS;//default
    mPPara->vid_id = mVid_id;
    mPPara->vid_fmt = mVid_fmt;
    mPPara->pkg_fmt = mPkg_fmt;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt)
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    mAud_id  = aid;
    mAud_fmt = afmt;
    mPPara->aud_id = mAud_id;
    mPPara->aud_fmt = mAud_fmt;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt)
{
    ALOGV("%s at #line %d TODO\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    (void)aid;
    (void)afmt;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperSetSubtitleParam(int sid, int stype)
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    mSub_id  = sid;
    mSub_type = stype;
    mPPara->sub_id = mSub_id;
    mPPara->sub_type = mSub_type;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperStop()
{
    ALOGV("%s at #line %d\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    Inject_Stop(&mPPara);
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperClose()
{
    ALOGV("%s at #line %d TODO\n",__func__,__LINE__);
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    return ret;
}

AM_DmxErrorCode_t AmHwDemuxWrapper::AmDemuxWrapperGetStates (int * states , int statetype)
{
    AM_DmxErrorCode_t ret = AM_Dmx_SUCCESS;
    char name[PATH_LEN] = {0};
    char value[STATE_LEN] = {0};
    int vflag = 0, aflag = 0 ;
    switch ((AV_Dmx_States_t)statetype)
    {
        case AM_Dmx_IsScramble:
            if (mDmxDevNo >= 0 && mDmxDevNo < DMX_DEV_MAX)
            {
                snprintf(name, sizeof(name), "/sys/class/dmx/demux%d_scramble", mDmxDevNo);
            }
            /*if (FileSystem_readFile(name,value) == 0)*/
            {
                ALOGV("%s at #line %d value %s\n",__func__,__LINE__,value);
                sscanf(value, "%d %d", &vflag,&aflag);
                *states = vflag |aflag;
                if (*states)
                    ALOGV("%s at #line %d states %d\n",__func__,__LINE__,*states);
            }
            break;
         default:
            ALOGV("illegal command");
            return AM_Dmx_NOT_SUPPORTED;
            break;
    }
    return ret;
}