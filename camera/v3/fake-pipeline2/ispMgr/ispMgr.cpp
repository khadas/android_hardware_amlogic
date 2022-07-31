#define LOG_TAG "IspMgr"

#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include <vector>

#include "ispMgr.h"

namespace android {

static int getInterface() {
    auto ispIF = &IspMgr::mIspIF;
    auto lib = ::dlopen("libispaml.so", RTLD_NOW);
    if (!lib) {
        char const* err_str = ::dlerror();
        ALOGE("dlopen: error:%s", (err_str ? err_str : "unknown"));
        return -1;
    }
    ispIF->alg2User = (isp_alg2user)::dlsym(lib, "aisp_alg2user");
    if (!ispIF->alg2User) {
        char const* err_str = ::dlerror();
        ALOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        return -1;
    }
    ispIF->alg2Kernel = (isp_alg2kernel)::dlsym(lib, "aisp_alg2kernel");
    if (!ispIF->alg2Kernel) {
        char const* err_str = ::dlerror();
        ALOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        return -1;
    }
    ispIF->algEnable = (isp_enable)::dlsym(lib, "aisp_enable");
    if (!ispIF->algEnable) {
        char const* err_str = ::dlerror();
        ALOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        return -1;
    }
    ispIF->algDisable = (isp_disable)::dlsym(lib, "aisp_disable");
    if (!ispIF->algDisable) {
        char const* err_str = ::dlerror();
        ALOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        return -1;
    }
    ALOGI("%s success", __FUNCTION__);
    return 0;
}

struct ispIF IspMgr::mIspIF;
static int ret = getInterface();

IspMgr::IspMgr(int id) {
    memset(&mCalibInfo, 0, sizeof(aisp_calib_info_t));
    memset(&mPstAlgCtx, 0, sizeof(AML_ALG_CTX_S));
    mFlushFd[0] = -1;
    mFlushFd[1] = -1;
    mId = id;
    mStart = false;
}

IspMgr::~IspMgr() {
    close(mFlushFd[0]);
    close(mFlushFd[1]);
    mFlushFd[0] = -1;
    mFlushFd[1] = -1;
    ALOGD("%s", __FUNCTION__);
}

status_t IspMgr::configure(struct media_stream *stream) {
    ALOGD("%s +", __FUNCTION__);
    int rc;
    Mutex::Autolock _l(mLock);
    mMediaStream = stream;

    if (mFlushFd[1] != -1 || mFlushFd[0] != -1) {
        close(mFlushFd[0]);
        close(mFlushFd[1]);
        mFlushFd[0] = -1;
        mFlushFd[1] = -1;
    }

    rc = pipe(mFlushFd);
    if (rc < 0) {
        ALOGE("Failed to create Flush pipe: %s", strerror(errno));
        return -1;
    }

    /**
     * make the reading end of the pipe non blocking.
     * This helps during flush to read any information left there without
     * blocking
     */
    rc = fcntl(mFlushFd[0], F_SETFL, O_NONBLOCK);
    if (rc < 0) {
        ALOGE("Fail to set flush pipe flag: %s", strerror(errno));
        return -1;
    }

    mPollingDevices.push_back(mMediaStream->video_param);
    mPollingDevices.push_back(mMediaStream->video_stats);

    stream_configuration_t           stream_config;
    stream_config.format.width     = kIspStatsWidth;
    stream_config.format.height    = kIspStatsHeight;
    stream_config.format.nplanes   = 1;

    rc = setDataFormat(mMediaStream, &stream_config);
    if (rc < 0) {
        ALOGE("[stats] Failed to set format");
        return -1;
    }
    stream_config.format.width     = kIspParamsWidth;
    stream_config.format.height    = kIspParamsHeight;
    stream_config.format.nplanes   = 1;

    rc = setConfigFormat(mMediaStream, &stream_config);
    if (rc < 0) {
        ALOGE("[params] Failed to set format");
        return -1;
    }

    mSensorConfig = matchSensorConfig(mMediaStream);
    if (mSensorConfig == nullptr) {
        ALOGE("Failed to matchSensorConfig");
        return -1;
    }
    cmos_set_sensor_entity(mSensorConfig, mMediaStream->sensor_ent);
    cmos_sensor_control_cb(mSensorConfig, &mPstAlgCtx.stSnsExp);
    cmos_get_sensor_calibration(mSensorConfig, &mCalibInfo);
    return rc;
}

status_t IspMgr::start() {
    ALOGD("%s +", __FUNCTION__);
    int rc;
    Mutex::Autolock _l(mLock);
    mStart = true;

    memset (&mISParams.rb, 0, sizeof (struct v4l2_requestbuffers));
    mISParams.rb.count  = kIspParamsNbBuffers;
    mISParams.rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mISParams.rb.memory = V4L2_MEMORY_MMAP;
    rc = v4l2_video_req_bufs(mMediaStream->video_param, &mISParams.rb);
    if (rc < 0) {
        ALOGE("[params] Failed to req_bufs");
        return -1;
    }

    /* map buffers */
    for (int i = 0; i < kIspParamsNbBuffers; i++) {
        struct v4l2_buffer v4l2_buf;
        memset (&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        v4l2_buf.index   = i;
        v4l2_buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory  = V4L2_MEMORY_MMAP;
        rc = v4l2_video_query_buf(mMediaStream->video_param, &v4l2_buf);
        if (rc < 0) {
            ALOGE("[params] error: query buffer %d", rc);
            return -1;
        }

        mISParams.mem[i].size = v4l2_buf.length;
        ALOGD("[params] type video capture. length: %u offset: %u", v4l2_buf.length, v4l2_buf.m.offset);
        mISParams.mem[i].addr = mmap (0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
            mMediaStream->video_param->fd, v4l2_buf.m.offset);
        ALOGD("[params] Buffer[%d] mapped at address 0x%p length: %u offset: %u", i, mISParams.mem[i].addr, v4l2_buf.length, v4l2_buf.m.offset);
        if (mISParams.mem[i].addr == MAP_FAILED) {
            ALOGE("[params] error: mmap buffers");
            return -1;
        }
    }

    char alg_init[kIspParamsHeight * kIspParamsWidth];
    memset(alg_init, 0, sizeof(alg_init));
    (IspMgr::mIspIF.algEnable)(mId, &mPstAlgCtx, &mCalibInfo);
    for (int i = 0; i < kIspParamsNbBuffers; i++) {
        (IspMgr::mIspIF.alg2User)(mId, alg_init);
        (IspMgr::mIspIF.alg2Kernel)(mId, mISParams.mem[i].addr);
        /* queue buffers */
        ALOGD("[params] begin to Queue buf.");
        struct v4l2_buffer v4l2_buf;
        memset (&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        v4l2_buf.index   = i;
        v4l2_buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory  = V4L2_MEMORY_MMAP;
        rc = v4l2_video_q_buf(mMediaStream->video_param, &v4l2_buf);
        if (rc < 0) {
            ALOGE("[params] error: queue buffers, rc:%d i:%d", rc, i);
        }
    }
    rc = v4l2_video_stream_on(mMediaStream->video_param, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (rc < 0) {
        ALOGE("[params] error: streamon");
        return 0;
    }

    memset (&mISPStats.rb, 0, sizeof (struct v4l2_requestbuffers));
    mISPStats.rb.count  = kIspStatsNbBuffers;
    mISPStats.rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mISPStats.rb.memory = V4L2_MEMORY_MMAP;
    rc = v4l2_video_req_bufs(mMediaStream->video_stats, &mISPStats.rb);
    if (rc < 0) {
        ALOGE("[stats] Failed to req_bufs");
        return -1;
    }

    /* map buffers */
    for (int i = 0; i < kIspStatsNbBuffers; i++) {
        struct v4l2_buffer v4l2_buf;
        memset (&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        v4l2_buf.index   = i;
        v4l2_buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory  = V4L2_MEMORY_MMAP;
        rc = v4l2_video_query_buf(mMediaStream->video_stats, &v4l2_buf);
        if (rc < 0) {
            ALOGE("[stats] error: query buffer %d", rc);
            return -1;
        }

        mISPStats.mem[i].size = v4l2_buf.length;
        ALOGD("[stats] video capture. length: %u offset: %u", v4l2_buf.length, v4l2_buf.m.offset);
        mISPStats.mem[i].addr = mmap (0, v4l2_buf.length, PROT_READ | PROT_WRITE, MAP_SHARED,
            mMediaStream->video_stats->fd, v4l2_buf.m.offset);
        ALOGD("[stats] Buffer[%d] mapped at address 0x%p length: %u offset: %u", i, mISPStats.mem[i].addr, v4l2_buf.length, v4l2_buf.m.offset);
        if (mISPStats.mem[i].addr == MAP_FAILED) {
            ALOGE("[stats] error: mmap buffers");
            return -1;
        }
    }

    for (int i = 0; i < kIspStatsNbBuffers; i++) {
        /* queue buffers */
        ALOGD("begin to Queue buf.");
        struct v4l2_buffer v4l2_buf;
        memset (&v4l2_buf, 0, sizeof (struct v4l2_buffer));
        v4l2_buf.index   = i;
        v4l2_buf.type    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf.memory  = V4L2_MEMORY_MMAP;
        rc = v4l2_video_q_buf( mMediaStream->video_stats, &v4l2_buf);
        if (rc < 0) {
            ALOGE("[stats] error: queue buffers, rc:%d i:%d", rc, i);
        }
    }

    rc = v4l2_video_stream_on(mMediaStream->video_stats, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (rc < 0) {
        ALOGE("[stats] error: streamon");
        return 0;
    }

    ALOGD("Video stream is on");
    rc = run("ispMgr", ANDROID_PRIORITY_URGENT_DISPLAY);
    if (rc != 0) {
        ALOGE("Unable to start ispMgr thread: %d", rc);
    }
    ALOGD("start -");
    return rc;
}

status_t IspMgr::stop() {
    ALOGD("%s +", __FUNCTION__);
    int rc;
    {
        Mutex::Autolock _l(mLock);
        if (mStart == false) {
            ALOGI("IspMgr not working");
            return 0;
        } else {
            if (mFlushFd[1] != -1) {
                char buf = 0xf;  // random value to write to flush fd.
                unsigned int size = write(mFlushFd[1], &buf, sizeof(char));
                if (size != sizeof(char))
                    ALOGW("Flush write not completed");
            }
        }
    }
    rc = requestExitAndWait();
    if (rc != 0) {
        ALOGE("Unable to stop IspMgr thread: %d", rc);
    }
    ALOGD("requestExitAndWait-");
    {
        char readbuf;
        if (mFlushFd[0] != -1) {
            unsigned int size = read(mFlushFd[0], (void*) &readbuf, sizeof(char));
            if (size != sizeof(char))
                ALOGW("Flush read not completed.");
        }
    }
    /* stream off */
    rc = v4l2_video_stream_off(mMediaStream->video_stats, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (rc < 0) {
        ALOGE("[stats] error: streamoff");
        return 0;
    }
    rc = v4l2_video_stream_off(mMediaStream->video_param, V4L2_BUF_TYPE_VIDEO_CAPTURE);
    if (rc < 0) {
        ALOGE("[params] error: streamoff");
        return 0;
    }

    (IspMgr::mIspIF.algDisable)(mId);

    memset (&mISPStats.rb, 0, sizeof (struct v4l2_requestbuffers));
    mISPStats.rb.count  = 0;
    mISPStats.rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mISPStats.rb.memory = V4L2_MEMORY_MMAP;
    rc = v4l2_video_req_bufs(mMediaStream->video_stats, &mISPStats.rb);
    if (rc < 0) {
        ALOGE("[stats] error: request buffer.");
    }
    /* unmap buffers */
    for (int i = 0; i < kIspStatsNbBuffers; i++) {
        munmap (mISPStats.mem[i].addr, mISPStats.mem[i].size);
        if (mISPStats.mem[i].dma_fd >= 0)
            close(mISPStats.mem[i].dma_fd);
    }

    mISParams.rb.count  = 0;
    mISParams.rb.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    mISParams.rb.memory = V4L2_MEMORY_MMAP;
    rc = v4l2_video_req_bufs(mMediaStream->video_param, &mISParams.rb);
    if (rc < 0) {
        ALOGE("[params] Failed to req_bufs");
        return -1;
    }
    /* unmap buffers */
    for (int i = 0; i < kIspParamsNbBuffers; i++) {
        munmap (mISParams.mem[i].addr, mISParams.mem[i].size);
        if (mISParams.mem[i].dma_fd >= 0)
            close(mISParams.mem[i].dma_fd);
    }
    ALOGD("stop -");
    return rc;
}

status_t IspMgr::pollDevices(const std::vector<struct media_entity *> &devices,
                                std::vector<struct media_entity *> &activeDevices,
                                std::vector<struct media_entity *> &inactiveDevices,
                                int timeOut, int flushFd, int events) {
    ALOGVV("%s +", __FUNCTION__);
    int numFds = devices.size();
    int totalNumFds = (flushFd != -1) ? numFds + 1 : numFds; //adding one more fd if flushfd given.
    struct pollfd pollFds[totalNumFds];
    int ret = 0;

    for (int i = 0; i < numFds; i++) {
        pollFds[i].fd = devices[i]->fd;
        pollFds[i].events = events | POLLERR; // we always poll for errors, asked or not
        pollFds[i].revents = 0;
    }

    if (flushFd != -1) {
        pollFds[numFds].fd = flushFd;
        pollFds[numFds].events = POLLPRI | POLLIN;
        pollFds[numFds].revents = 0;
    }

    ret = poll(pollFds, totalNumFds, timeOut);
    if (ret <= 0) {
        for (uint32_t i = 0; i < devices.size(); i++) {
            ALOGE("Device %s poll failed (%s)", devices[i]->info.name,
                                              (ret == 0) ? "timeout" : "error");
            if (pollFds[i].revents & POLLERR) {
                ALOGE("%s: device %s received POLLERR", __FUNCTION__, devices[i]->info.name);
            }
        }
        return ret;
    }

    activeDevices.clear();
    inactiveDevices.clear();

    //check first the flush
    if (flushFd != -1) {
        if ((pollFds[numFds].revents & POLLIN) || (pollFds[numFds].revents & POLLPRI)) {
            ALOGD("%s: Poll returning from flush", __FUNCTION__);
            return ret;
        }
    }

    // check other active devices.
    for (int i = 0; i < numFds; i++) {
        if (pollFds[i].revents & POLLERR) {
            ALOGE("%s: received POLLERR", __FUNCTION__);
            return -1;
        }
        // return nodes that have data available
        if (pollFds[i].revents & events) {
            activeDevices.push_back(devices[i]);
        } else
            inactiveDevices.push_back(devices[i]);
    }
    return ret;
}


status_t IspMgr::readyToRun() {
    ALOGD("readyToRun");
    return NO_ERROR;
}

bool IspMgr::threadLoop() {
    int rc;
    ALOGVV("threadLoop+");
    auto pollingDevices = mPollingDevices;
    do {
        rc = IspMgr::pollDevices(pollingDevices, mActiveDevices,
                                 mInactiveDevices, kSyncWaitTimeout, mFlushFd[0]);
        if (mActiveDevices.size() != mPollingDevices.size()) {
            pollingDevices = mInactiveDevices;
            ALOGVV("not all device is ready, continue polling");
            for (int i = 0; i < mInactiveDevices.size(); ++i)
                ALOGVV("InactiveDevices %d: %s", i, mInactiveDevices[i]->info.name);
            continue;
        } else if (mActiveDevices.size() == mPollingDevices.size()) {
            ALOGVV("all device is ready");
        } else {
            ALOGVV("return from flush or error");
            return false;
        }

        struct v4l2_buffer v4l2_buf_stats;
        // dqbuf from video node
        memset (&v4l2_buf_stats, 0, sizeof (struct v4l2_buffer));
        v4l2_buf_stats.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf_stats.memory = V4L2_MEMORY_MMAP;
        rc = v4l2_video_dq_buf(mMediaStream->video_stats, &v4l2_buf_stats);
        if (rc < 0) {
            ALOGE ("[stats] error: dequeue buffer");
            continue;
        }

        struct v4l2_buffer v4l2_buf_param;
        memset (&v4l2_buf_param, 0, sizeof (struct v4l2_buffer));
        v4l2_buf_param.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        v4l2_buf_param.memory = V4L2_MEMORY_MMAP;
        rc = v4l2_video_dq_buf(mMediaStream->video_param, &v4l2_buf_param);
        if (rc < 0) {
            ALOGE ("[params] error: dequeue buffer");
            continue;
        }

        (IspMgr::mIspIF.alg2User)(mId, mISPStats.mem[v4l2_buf_stats.index].addr);
        (IspMgr::mIspIF.alg2Kernel)(mId, mISParams.mem[v4l2_buf_param.index].addr);

        usleep(1000*10);

        rc = v4l2_video_q_buf(mMediaStream->video_stats,  &v4l2_buf_stats);
        if (rc < 0) {
            ALOGE ("[stats] error: queue buffer");
            break;
        }
        rc = v4l2_video_q_buf(mMediaStream->video_param,  &v4l2_buf_param);
        if (rc < 0) {
            ALOGE ("[params] error: queue buffer");
            break;
        }
    } while(0);
    ALOGVV("threadLoop-");
    return true;
}
}
