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

using namespace android;

TvInputIntf::TvInputIntf() : mpObserver(nullptr) {
    mSourceStatus = false;
    mSourceInput = SOURCE_INVALID;
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
}

TvInputIntf::~TvInputIntf()
{
    mTvSession.clear();

#ifdef SUPPORT_DTVKIT
    mDkSession.clear();
#endif

    pthread_mutex_destroy(&mMutex);
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

    mSourceStatus = true;

    if (SOURCE_DTVKIT == source_input) {
#ifdef SUPPORT_DTVKIT
        mDkSession->request(std::string("Dvb.requestDtvDevice"),
                writer.write(json));
#endif
        ret = 0;
    } else
        ret = mTvSession->startTv();

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

    mSourceStatus = false;

    if (SOURCE_DTVKIT == source_input) {
#ifdef SUPPORT_DTVKIT
        mDkSession->request(std::string("Dvb.releaseDtvDevice"),
                writer.write(json));
#endif
        ret = 0;
    } else
        ret = mTvSession->stopTv();

    pthread_mutex_unlock(&mMutex);

    return ret;
}

int TvInputIntf::switchSourceInput(tv_source_input_t source_input)
{
    int ret = 0;

    pthread_mutex_lock(&mMutex);

    mSourceInput = source_input;

    ALOGD("switchSourceInput: %d.", source_input);

    if (SOURCE_DTVKIT == source_input)
        ret = 0;
    else
        ret = mTvSession->switchInputSrc(source_input);

    pthread_mutex_unlock(&mMutex);

    return ret;
}

int TvInputIntf::getSourceConnectStatus(tv_source_input_t source_input)
{
    if (SOURCE_DTVKIT == source_input)
        return 0;
    else
        return mTvSession->getInputSrcConnectStatus(source_input);
}

int TvInputIntf::getCurrentSourceInput()
{
    ALOGD("getCurrentSourceInput: mSourceInput %d.", mSourceInput);

    if (mSourceInput == SOURCE_DTVKIT)
        return SOURCE_DTVKIT;
    else
        return mTvSession->getCurrentInputSrc();
}

int TvInputIntf::checkSourceStatus(tv_source_input_t source_input, bool check_status)
{
    int ret = 0;

    pthread_mutex_lock(&mMutex);

    ALOGD("Current [mSourceInput: %d], [switch source_input: %d], [mSourceStatus: %d], [check_status: %d].",
            mSourceInput, source_input, mSourceStatus, check_status);

    if (mSourceInput != SOURCE_INVALID && source_input != SOURCE_INVALID && mSourceInput != source_input) {
        if (!check_status && mSourceStatus) {
            if (!check_status)
                start_queue.push(source_input);
            else
                stop_queue.push(source_input); /* maybe handle stop queue. */

            ret = -EBUSY;
        }
    }

    ALOGD("checkSourceStatus: ret %d (%s).", ret, strerror(ret));

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

bool TvInputIntf::getSourceStatus()
{
    ALOGD("getSourceStatus: mSourceStatus %d.", mSourceStatus);

    return mSourceStatus;
}

bool TvInputIntf::isTvPlatform()
{
    ALOGD("isTvPlatform: %d.", mIsTv);

    return mIsTv;
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

    int len = 0;
    const char *seg = ",";
    char *pT = strtok((char*)input_list, seg);
    while (pT) {
        len ++;
        *devices = atoi(pT);
        ALOGD("devices: %d: %d", len , *devices);
        devices ++;
        pT = strtok(NULL, seg);
    }
    *count = len;
    return 0;

}

int TvInputIntf::getHdmiPort(tv_source_input_t source_input) {
    return mTvSession->getHdmiPorts(source_input);
}

