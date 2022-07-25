#ifndef __ISP_MGR_H__
#define __ISP_MGR_H__

#include <cstdlib>
#include <thread>
#include <utils/threads.h>
#include <utils/Mutex.h>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/mediaApi.h"

#include "aml_isp_api.h"

#include "sensor/sensor_config.h"

const size_t  kMaxRetryCount   = 100;
const int64_t kSyncWaitTimeout = 300000000LL; // 300ms

const size_t kIspStatsNbBuffers = 4;
const size_t kIspStatsWidth = 1024;
const size_t kIspStatsHeight = 256;

const size_t kIspParamsNbBuffers = 1;
const size_t kIspParamsWidth = 1024;
const size_t kIspParamsHeight = 256;

typedef void (*isp_alg2user)(uint32_t ctx_id, void *param);
typedef void (*isp_alg2kernel)(uint32_t ctx_id, void *param);
typedef void (*isp_enable)(uint32_t ctx, void *pstAlgCtx, void *calib);
typedef void (*isp_disable)(uint32_t ctx_id);

struct ispIF {
    isp_alg2user   alg2User   = nullptr;
    isp_alg2kernel alg2Kernel = nullptr;
    isp_enable     algEnable  = nullptr;
    isp_disable    algDisable = nullptr;
};

struct bufferInfo {
    void* addr = nullptr;
    int   size = 0;
    int   dma_fd = -1;
};

struct v4l2BufferInfo {
    v4l2BufferInfo() {
        memset(&rb, 0, sizeof(struct v4l2_requestbuffers));
        memset(&format, 0, sizeof(struct v4l2_format));
    }
    struct v4l2_requestbuffers rb;
    struct v4l2_format         format;
    struct bufferInfo          mem[8];
};

namespace android {

class IspMgr: public Thread, public virtual RefBase {
  public:
    IspMgr(int id);
    ~IspMgr();
  public:
    status_t configure(struct media_stream *stream);
    status_t start();
    status_t stop();
  public:
    static struct ispIF  mIspIF;
  protected:
    virtual status_t     readyToRun();
    virtual bool         threadLoop();
    static  status_t     pollDevices(const std::vector<struct media_entity *> &devices,
                             std::vector<struct media_entity *> &activeDevices,
                             std::vector<struct media_entity *> &inactiveDevices,
                             int timeOut, int flushFd = -1,
                             int events = POLLPRI | POLLIN | POLLERR);
  private:
    int                                mId;
    Mutex                              mLock;
    bool                               mStart;
    struct media_stream*               mMediaStream  = nullptr;
    struct sensorConfig*               mSensorConfig = nullptr;
    int                                mFlushFd[2];
    std::vector<struct media_entity *> mPollingDevices;
    std::vector<struct media_entity *> mActiveDevices;
    std::vector<struct media_entity *> mInactiveDevices;
    v4l2BufferInfo                     mISPStats;
    v4l2BufferInfo                     mISParams;
    aisp_calib_info_t                  mCalibInfo;
    AML_ALG_CTX_S                      mPstAlgCtx;
};

}
#endif
