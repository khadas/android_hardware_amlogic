/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef AMHWDEMUX_WRAPPER_H
#define AMHWDEMUX_WRAPPER_H
//#include <VideodecWrapper.h>
#include "AmDemuxWrapper.h"
#include <Mutex.h>
#include <TSPMessage.h>

extern "C" {

}
#ifndef DMX_TRUE
/**\brief Boolean value: true*/
#define DMX_TRUE        (1)
#endif

#ifndef DMX_FALSE
/**\brief Boolean value: false*/
#define DMX_FALSE       (0)
#endif



/*Data input source type*/
typedef enum
{
    TS_FROM_DEMOD = 0,                          // TS Data input from demod
    TS_FROM_MEMORY = 1                          // TS Data input from memory
} AM_Source_Type_t;


/**\brief demux device*/
typedef enum
{
    DMX_DEV_UNKNOWN = -1,
    DMX_DEV_NO0,                    /**< demux 0*/
    DMX_DEV_NO1,                    /**< demux 1*/
    DMX_DEV_NO2,                   /**< demux 2*/
    DMX_DEV_MAX,
} AM_DMX_DevNo_t;

/**\brief av*/
typedef enum
{
    AV_DEV_UNKNOWN = -1,
    AV_DEV_NO,
} AM_AV_DevNo_t;


/**\brief av*/
typedef enum
{
    DSC_DEV_UNKNOWN = -1,
    DSC_DEV_NO,
} AM_DSC_DevNo_t;

/**\brief TS stream input source*/
typedef enum
{
    AM_AV_TS_SRC_DMX0,                   /**< Demux 0*/
    AM_AV_TS_SRC_DMX1,                   /**< Demux 1*/
    AM_AV_TS_SRC_DMX2                    /**< Demux 2*/
} AM_AV_TSSource_t;


/**\brief Input source of the descrambler*/
typedef enum {
    AM_DSC_SRC_DMX0,         /**< Demux device 0*/
    AM_DSC_SRC_DMX1,         /**< Demux device 1*/
    AM_DSC_SRC_DMX2,         /**< Demux device 2*/
    AM_DSC_SRC_BYPASS        /**< Bypass TS data*/
} AM_DSC_Source_t;

/**\brief Input source of the demux*/
typedef enum
{
    AM_DMX_SRC_TS0,                    /**< TS input port 0*/
    AM_DMX_SRC_TS1,                    /**< TS input port 1*/
    AM_DMX_SRC_TS2,                    /**< TS input port 2*/
    AM_DMX_SRC_TS3,                    /**< TS input port 3*/
    AM_DMX_SRC_HIU,                     /**< HIU input (memory)*/
    AM_DMX_SRC_HIU1
} AM_DMX_Source_t;

typedef struct Dmx_Table
{
    AM_DMX_DevNo_t dev_no;
    int is_used;
} Dmx_Table_t;





/**\brief playback cmd*/
typedef enum
{
    AV_PLAY_START,                   /**< start playback*/
    AV_PLAY_PAUSE,                   /**< pause playback*/
    AV_PLAY_RESUME,                  /**< resume playback*/
    AV_PLAY_FF,                      /**< ff playback*/
    AV_PLAY_FB,                      /**< fb playback*/
    AV_PLAY_SEEK,                    /**< seek playback*/
    AV_PLAY_SWITCH_AUDIO              /**< switch audio playback*/
} AV_PlayCmd_t;

/**\brief dmx state*/
typedef enum
{
    AM_Dmx_IsScramble,                   /**< is scramble stream or not*/
} AV_Dmx_States_t;

class  AmHwDemuxWrapper : public AmDemuxWrapper{

public:
   AmHwDemuxWrapper();
   virtual ~AmHwDemuxWrapper();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperOpen(Am_DemuxWrapper_OpenPara_t *para);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetTSSource(Am_DemuxWrapper_OpenPara_t *para,const AM_DevSource_t src);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStart();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperWriteData(Am_TsPlayer_Input_buffer_t* Pdata, int *pWroteLen, uint64_t timeout);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperReadData(int pid, mEsDataInfo **mEsData,uint64_t timeout); //???????
   virtual  AM_DmxErrorCode_t AmDemuxWrapperFlushData(int pid); //???
   virtual  AM_DmxErrorCode_t AmDemuxWrapperPause();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperResume();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioParam(int aid, AM_AV_AFormat_t afmt, int security_mem_level);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetAudioDescParam(int aid, AM_AV_AFormat_t afmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetSubtitleParam(int sid, int stype);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperSetVideoParam(int vid, AM_AV_VFormat_t vfmt);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperGetStates (int * value , int statetype);
   virtual  AM_DmxErrorCode_t AmDemuxWrapperStop();
   virtual  AM_DmxErrorCode_t AmDemuxWrapperClose();

private:
   TSPMutex          mMutex;
   mutable TSPMutex  mLock;
   AM_DMX_DevNo_t mDmxDevNo;
   AM_DSC_DevNo_t mDSCDevNo;
   AM_AV_DevNo_t  mAvDevNo;
   AM_AV_VFormat_t mVid_fmt;
   AM_AV_AFormat_t mAud_fmt;
   int      mAud_id;
   int      mVid_id;
   int      mSub_id;
   int      mSub_type;
   AM_AV_PFormat_t  mPkg_fmt;
   //int mode_type;
   //AM_AV_DrmMode_t mDrm_mode;
   Am_DemuxWrapper_OpenPara_t  mPara;
   Am_DemuxWrapper_OpenPara_t * mPPara;
   //char* mDmxDevName;
   //char* mAvDevName;
};

#endif
