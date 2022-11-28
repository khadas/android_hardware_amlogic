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

#define LOG_TAG "TvInputIntf"

#include <utils/Log.h>
#include <string.h>
#include "TvInputIntf.h"
#include "tvcmd.h"
#include <math.h>
#include <cutils/properties.h>
#ifdef SUPPORT_DTVKIT
#include <json/json.h>
#endif

#define TV_INPUT_VERSION "V2.00"

using namespace android;

TvInputIntf::TvInputIntf() : mpObserver(nullptr) {
    mTvSession = TvServerHidlClient::connect(CONNECT_TYPE_HAL);
    mTvSession->setListener(this);

#ifdef SUPPORT_DTVKIT
    mDkSession = DTVKitHidlClient::connect(DTVKitHidlClient::CONNECT_TYPE_HAL);
#endif

    pthread_mutex_init(&mMutex, NULL);

    char buf[PROPERTY_VALUE_MAX] = { 0 };
    int ret = property_get("ro.vendor.platform.has.tvuimode", buf, "false");
    if (ret > 0 && !strncmp(buf, "true", 4))
        mIsTv = true;
    else
        mIsTv = false;

    ALOGI("create TvInputIntf: mIsTv = %d, %s.", mIsTv, TV_INPUT_VERSION);

    init();
}

TvInputIntf::~TvInputIntf()
{
    init();

    mTvSession.clear();

#ifdef SUPPORT_DTVKIT
    mDkSession.clear();
#endif

    pthread_mutex_destroy(&mMutex);
}

void TvInputIntf::init()
{
    if (mSourceStatus && mSourceInput != SOURCE_INVALID)
        stopTv(mSourceInput);

    if (checkHoldSource() != SOURCE_INVALID)
        stopTv(mSourceInput);

    pthread_mutex_lock(&mMutex);

    mStreamGivenId = -1;
    mDeviceGivenId = -1;
    mSourceStatus = false;
    mSourceInput = SOURCE_INVALID;
    mTunnelId = -1;

    while (!start_queue.empty())
        start_queue.pop();

    while (!stop_queue.empty())
        stop_queue.pop();

    while (!hold_queue.empty())
        hold_queue.pop();

    pthread_mutex_unlock(&mMutex);
}

int TvInputIntf::setTvObserver ( TvPlayObserver *ob )
{
    //ALOGI("setTvObserver:%p", ob);
    mpObserver = ob;
    return 0;
}

void TvInputIntf::notify(const tv_parcel_t &parcel)
{
    source_connect_t srcConnect;
    srcConnect.msgType = parcel.msgType;
    srcConnect.source = parcel.bodyInt[0];
    srcConnect.state = parcel.bodyInt[1];

    //ALOGI("notify type:%d, %p", srcConnect.msgType, mpObserver);
    if (mpObserver != NULL)
        mpObserver->onTvEvent(srcConnect);
}

int TvInputIntf::startTv(tv_source_input_t source_input)
{
    int ret = 0;
#ifdef SUPPORT_DTVKIT
    Json::Value json;
    json[0] = "";
    Json::FastWriter writer;
#endif

    pthread_mutex_lock(&mMutex);

    ALOGD("startTv source_input: %d.", source_input);

    setSourceStatus(true);

    if (SOURCE_DTVKIT == source_input || SOURCE_DTVKIT_PIP == source_input) {
#ifdef SUPPORT_DTVKIT
        mDkSession->request(std::string("Dvb.requestDtvDevice"),
                writer.write(json));
#endif
        ret = 0;
    } else {
        mTvSession->setTunnelId(mTunnelId);
        ret = mTvSession->startTv();

    }


    pthread_mutex_unlock(&mMutex);

    return ret;
}

int TvInputIntf::stopTv(tv_source_input_t source_input)
{
    int ret = 0;
#ifdef SUPPORT_DTVKIT
    Json::Value json;
    json[0] = "";
    Json::FastWriter writer;
#endif

    pthread_mutex_lock(&mMutex);

    ALOGD("stopTv source_input: %d.", source_input);

    setSourceStatus(false);

    if (SOURCE_DTVKIT == source_input || SOURCE_DTVKIT_PIP == source_input) {
#ifdef SUPPORT_DTVKIT
        mDkSession->request(std::string("Dvb.releaseDtvDevice"),
                writer.write(json));
#endif
        ret = 0;
    } else {
        ret = mTvSession->stopTv();
        mTvSession->setTunnelId(-1);
    }
    pthread_mutex_unlock(&mMutex);

    return ret;
}

int TvInputIntf::switchSourceInput(tv_source_input_t source_input)
{
    int ret = 0;

    pthread_mutex_lock(&mMutex);

    mSourceInput = source_input;

    ALOGD("switchSourceInput: %d.", source_input);

    if (SOURCE_DTVKIT == source_input || SOURCE_DTVKIT_PIP == source_input)
        ret = 0;
    else
        ret = mTvSession->switchInputSrc(source_input);

    pthread_mutex_unlock(&mMutex);

    return ret;
}

int TvInputIntf::getSourceConnectStatus(tv_source_input_t source_input)
{
    if (SOURCE_DTVKIT == source_input || SOURCE_DTVKIT_PIP == source_input)
        return 0;
    else
        return mTvSession->getInputSrcConnectStatus(source_input);
}

int TvInputIntf::getCurrentSourceInput()
{
    ALOGD("getCurrentSourceInput: mSourceInput %d.", mSourceInput);

    if (mSourceInput == SOURCE_DTVKIT)
        return SOURCE_DTVKIT;
    else if (mSourceInput == SOURCE_DTVKIT_PIP)
        return SOURCE_DTVKIT_PIP;
    else
        return mTvSession->getCurrentInputSrc();
}

int TvInputIntf::checkSourceStatus(tv_source_input_t source_input, bool check_status)
{
    int ret = 0;

    pthread_mutex_lock(&mMutex);

    if (mSourceInput != SOURCE_INVALID && source_input != SOURCE_INVALID && mSourceInput != source_input) {
        if (!check_status && mSourceStatus) {
            start_queue.push(source_input);

            ret = -EBUSY;
        }

        if (check_status && !mSourceStatus) {
            stop_queue.push(source_input); /* maybe handle stop queue. */

            ret = -EBUSY;
        }
    }

    if (check_status && mSourceStatus && (SOURCE_DTVKIT == source_input || SOURCE_DTVKIT_PIP == source_input))
        hold_queue.push(source_input);

    ALOGD("%s [mSourceInput: %d], [switch source_input: %d], [mSourceStatus: %d], [check_status: %d], status: %d (%s).",
            __FUNCTION__, mSourceInput, source_input, mSourceStatus, check_status, ret, ret ? "BUSY" : "OK");

    pthread_mutex_unlock(&mMutex);

    return ret;
}

tv_source_input_t TvInputIntf::checkWaitSource(bool check_status)
{
    tv_source_input_t source = SOURCE_INVALID;

    pthread_mutex_lock(&mMutex);

    if (check_status && !start_queue.empty()) {
        source = start_queue.front();
        start_queue.pop();
    } else if (!check_status && !stop_queue.empty()) {
        source = stop_queue.front();
        stop_queue.pop();
    }

    ALOGD("checkWaitSource: source %d.", source);

    pthread_mutex_unlock(&mMutex);

    return source;
}

tv_source_input_t TvInputIntf::checkHoldSource()
{
    tv_source_input_t source = SOURCE_INVALID;

    pthread_mutex_lock(&mMutex);

    if (!hold_queue.empty()) {
        source = hold_queue.front();
        hold_queue.pop();
    }

    ALOGD("checkHoldSource: source %d.", source);

    pthread_mutex_unlock(&mMutex);

    return source;
}

bool TvInputIntf::getSourceStatus()
{
    ALOGD("getSourceStatus: mSourceStatus %d.", mSourceStatus);

    return mSourceStatus;
}

void TvInputIntf::setSourceStatus(bool status)
{
	mSourceStatus = status;
}

bool TvInputIntf::isTvPlatform()
{
    ALOGD("isTvPlatform: %d.", mIsTv);

    return mIsTv;
}

int TvInputIntf::getStreamGivenId()
{
    return mStreamGivenId;
}

void TvInputIntf::setStreamGivenId(int stream_id)
{
    pthread_mutex_lock(&mMutex);

    mStreamGivenId = stream_id;

    pthread_mutex_unlock(&mMutex);
}

int TvInputIntf::getDeviceGivenId()
{
    return mDeviceGivenId;
}

void TvInputIntf::setDeviceGivenId(int device_id)
{
    pthread_mutex_lock(&mMutex);

    mDeviceGivenId = device_id;

    pthread_mutex_unlock(&mMutex);
}

void TvInputIntf::setStreamTunnelId(int id)
{
    mTunnelId = id;

}

int TvInputIntf::getHdmiAvHotplugDetectOnoff()
{
    return mTvSession->getHdmiAvHotplugStatus();
}

int TvInputIntf::getSupportInputDevices(int *devices, int *count)
{
    std::string serverDevices = mTvSession->getSupportInputDevices();
    const char *input_list = serverDevices.c_str();
    ALOGD("getAllTvDevices input list = %s", input_list);

    if (0 == strcmp(input_list, "null")) {
        *count = 0;
        return 0;
    }

    int len = 0;
    const char *seg = ",";
    char *pT = strtok((char*)input_list, seg);
    bool needVirtualDtvkit = false;
    while (pT) {
        len ++;
        *devices = atoi(pT);
        if (*devices == SOURCE_DTVKIT) {
            //add for pip hardware support
            needVirtualDtvkit = true;
        }
        ALOGD("devices: %d: %d", len , *devices);
        devices ++;
        pT = strtok(NULL, seg);
    }
    if (needVirtualDtvkit) {
        *devices = SOURCE_DTVKIT_PIP;
        len++;
    }
    *count = len;
    return 0;

}

int TvInputIntf::getHdmiPort(tv_source_input_t source_input) {
    return mTvSession->getHdmiPorts(source_input);
}

bool TvInputIntf::isMultiDemux() {
    if (access("/sys/class/stb/demux0_source", F_OK) == 0) {
        return false;
    } else {
        return true;
    }
}

int TvInputIntf::writeSurfaceTypetoVpp(tvin_surface_type_t type) {
    char buf[4] = {0};
    snprintf(buf, 4, "%d", type);
    return writeSys(VPP_SOURCE_TYPE, buf);
}

int TvInputIntf::writeSys(const char *path, const char *val) {
    int fd, size;

    if ((fd = open(path, O_RDWR)) < 0) {
        ALOGE("writeSys, open %s fail.", path);
        return -1;
    }

    size = strlen(val);
    if (write(fd, val, size) != size) {
        ALOGE("writeSys, write %d size error: %s\n", size, strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);
    return 0;
}

