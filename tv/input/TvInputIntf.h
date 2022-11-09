/*
 * Copyright (C) 2011 The Android Open Source Project
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
 *  @author   Tellen Yu
 *  @version  1.0
 *  @date     2018/1/12
 *  @par function description:
 *  - 1 tv_input hal to tvserver interface
 */

#ifndef _ANDROID_TV_INPUT_INTERFACE_H_
#define _ANDROID_TV_INPUT_INTERFACE_H_

#include <pthread.h>
#include <semaphore.h>
#include <queue>
#include <unistd.h>

#include "TvServerHidlClient.h"
#ifdef SUPPORT_DTVKIT
#include "DTVKitHidlClient.h"
#endif

using namespace android;

#define HDMI_MAX_SUPPORT_NUM 4
#define VPP_SOURCE_TYPE "/sys/class/video/tvin_source_type"

typedef enum tvin_surface_type_e {
    TVIN_SOURCE_TYPE_OTHERS = 0,
    TVIN_SOURCE_TYPE_DECODER = 1,  /**DTV**/
    TVIN_SOURCE_TYPE_VDIN = 2,   /**ATV HDMIIN CVBS**/
} tvin_surface_type_t;

typedef enum tv_source_input_e {
    SOURCE_INVALID = -1,
    SOURCE_TV = 0,
    SOURCE_AV1,
    SOURCE_AV2,
    SOURCE_YPBPR1,
    SOURCE_YPBPR2,
    SOURCE_HDMI1,
    SOURCE_HDMI2,
    SOURCE_HDMI3,
    SOURCE_HDMI4,
    SOURCE_VGA,
    SOURCE_MPEG,
    SOURCE_DTV,
    SOURCE_SVIDEO,
    SOURCE_IPTV,
    SOURCE_DUMMY,
    SOURCE_SPDIF,
    SOURCE_ADTV,
    SOURCE_AUX,
    SOURCE_ARC,
    SOURCE_DTVKIT,
    SOURCE_MAX,
    SOURCE_DTVKIT_PIP = SOURCE_DTVKIT + 100,
} tv_source_input_t;

typedef struct source_connect_s {
    int msgType;
    int source;
    int state;
} source_connect_t;

class TvPlayObserver {
public:
    TvPlayObserver() {};
    virtual ~TvPlayObserver() {};
    virtual void onTvEvent (const source_connect_t &scrConnect) = 0;
};

class TvInputIntf : public TvListener {
public:
    TvInputIntf();
    ~TvInputIntf();
    void init();
    int startTv(tv_source_input_t source_input);
    int stopTv(tv_source_input_t source_input);
    int switchSourceInput(tv_source_input_t source_input);
    int getSourceConnectStatus(tv_source_input_t source_input);
    int getCurrentSourceInput();
    int checkSourceStatus(tv_source_input_t source_input, bool check_status);
    tv_source_input_t checkWaitSource(bool check_status);
    tv_source_input_t checkHoldSource();
    bool getSourceStatus();
    void setSourceStatus(bool status);
    bool isTvPlatform();
    int getStreamGivenId();
    void setStreamGivenId(int stream_id);
    int getDeviceGivenId();
    void setDeviceGivenId(int device_id);
    int getHdmiAvHotplugDetectOnoff();
    int setTvObserver (TvPlayObserver *ob);
    int getSupportInputDevices(int *devices, int *count);
    int getHdmiPort(tv_source_input_t source_input);
    bool isMultiDemux();
    virtual void notify(const tv_parcel_t &parcel);
    int writeSurfaceTypetoVpp(tvin_surface_type_t type);
    void setStreamTunnelId(int id);

private:
    pthread_mutex_t mMutex;
    int mStreamGivenId;
    int mDeviceGivenId;
    bool mSourceStatus;
    bool mIsTv;
    int mTunnelId;
    std::queue<tv_source_input_t> start_queue;
    std::queue<tv_source_input_t> stop_queue;
    std::queue<tv_source_input_t> hold_queue;
    tv_source_input_t mSourceInput;
    sp<TvServerHidlClient> mTvSession;
    int writeSys(const char *path, const char *val);
#ifdef SUPPORT_DTVKIT
    sp<DTVKitHidlClient> mDkSession;
#endif
    TvPlayObserver *mpObserver;
};

#endif/*_ANDROID_TV_INPUT_INTERFACE_H_*/
