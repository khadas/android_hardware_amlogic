/*
 * Copyright (C) 2016 The Android Open Source Project
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
 *  - 1 droidlogic tv hwbinder client
 */

#define LOG_TAG "TvServerHidlClient"
#include <log/log.h>
#include "unistd.h"

#include "include/TvServerHidlClient.h"

namespace android {

Mutex TvServerHidlClient::mLock;

// establish binder interface to tv service
sp<ITvServer> TvServerHidlClient::getTvService()
{
    Mutex::Autolock _l(mLock);

#if 1//PLATFORM_SDK_VERSION >= 26
    sp<ITvServer> tvservice = ITvServer::tryGetService();
    while (tvservice == nullptr) {
         usleep(200*1000);//sleep 200ms
         tvservice = ITvServer::tryGetService();
         ALOGE("tryGet tvserver daemon Service");
    };
    mDeathRecipient = new TvServerDaemonDeathRecipient(this);
    Return<bool> linked = tvservice->linkToDeath(mDeathRecipient, /*cookie*/ 0);
    if (!linked.isOk()) {
        ALOGE("Transaction error in linking to tvserver daemon service death: %s", linked.description().c_str());
    } else if (!linked) {
        ALOGE("Unable to link to tvserver daemon service death notifications");
    } else {
        ALOGI("Link to tvserver daemon service death notification successful");
    }

#else
    Mutex::Autolock _l(mLock);
    if (mTvService.get() == 0) {
        sp<IServiceManager> sm = defaultServiceManager();
        sp<IBinder> binder;
        do {
            binder = sm->getService(String16("tvservice"));
            if (binder != 0)
                break;
            ALOGW("TvService not published, waiting...");
            usleep(500000); // 0.5 s
        } while (true);
        if (mDeathNotifier == NULL) {
            mDeathNotifier = new DeathNotifier();
        }
        binder->linkToDeath(mDeathNotifier);
        mTvService = interface_cast<ITvService>(binder);
    }
    ALOGE_IF(mTvService == 0, "no TvService!?");
    return mTvService;
#endif

    return tvservice;
}

TvServerHidlClient::TvServerHidlClient(tv_connect_type_t type): mType(type)
{
    mTvServer = getTvService();
    if (mTvServer != nullptr) {
        mTvServerHidlCallback = new TvServerHidlCallback(this);
        Return<void> setup = mTvServer->setCallback(mTvServerHidlCallback, static_cast<ConnectType>(type));
        if (!setup.isOk()) {
            ALOGE("Failed to setCallback %s", setup.description().c_str());
        }
    }
}

TvServerHidlClient::~TvServerHidlClient()
{
    disconnect();
}

sp<TvServerHidlClient> TvServerHidlClient::connect(tv_connect_type_t type)
{
    return new TvServerHidlClient(type);
}

void TvServerHidlClient::reconnect()
{
    ALOGI("tvserver client type:%d reconnect", mType);
    if (mTvServer != nullptr) {
        mTvServer.clear();
    }
    //reconnect to server
    mTvServer = getTvService();
    if (mTvServer != nullptr) {
        Return<void> setup = mTvServer->setCallback(mTvServerHidlCallback, static_cast<ConnectType>(mType));
        if (!setup.isOk()) {
            ALOGE("Failed to setCallback %s", setup.description().c_str());
        }
    }
}

void TvServerHidlClient::disconnect()
{
    ALOGD("disconnect");
}

/*
status_t TvServerHidlClient::processCmd(const Parcel &p, Parcel *r __unused)
{
    int cmd = p.readInt32();

    ALOGD("processCmd cmd=%d", cmd);
    return 0;
#if 0
    sp<IAllocator> ashmemAllocator = IAllocator::getService("ashmem");
    if (ashmemAllocator == nullptr) {
        ALOGE("can not get ashmem service");
        return -1;
    }

    size_t size = p.dataSize();
    hidl_memory hidlMemory;
    auto res = ashmemAllocator->allocate(size, [&](bool success, const hidl_memory& memory) {
                if (!success) {
                    ALOGE("ashmem allocate size:%d fail", size);
                }
                hidlMemory = memory;
            });

    if (!res.isOk()) {
        ALOGE("ashmem allocate result fail");
        return -1;
    }

    sp<IMemory> memory = hardware::mapMemory(hidlMemory);
    void* data = memory->getPointer();
    memory->update();
    // update memory however you wish after calling update and before calling commit
    memcpy(data, p.data(), size);
    memory->commit();
    Return<int32_t> ret = mTvServer->processCmd(hidlMemory, (int)size);
    if (!ret.isOk()) {
        ALOGE("Failed to processCmd");
    }
    return ret;
#endif
}*/

void TvServerHidlClient::setListener(const sp<TvListener> &listener)
{
    mListener = listener;
}

int TvServerHidlClient::startTv() {
    Return<int32_t> ret = mTvServer->startTv();
    if (!ret.isOk()) {
        ALOGE("startTv error");
    }
    return ret;
}

int TvServerHidlClient::stopTv() {
    Return<int32_t> ret = mTvServer->stopTv();
    if (!ret.isOk()) {
        ALOGE("stopTv error");
    }
    return ret;
}

int TvServerHidlClient::setTunnelId(int tunnelId) {
    Return<int32_t> ret = mTvServer->setTunnelId(tunnelId);
    if (!ret.isOk()) {
        ALOGE("setTunnelId error");
    }
    return ret;
}

int TvServerHidlClient::switchInputSrc(int32_t inputSrc) {
    //return mTvServer->switchInputSrc(inputSrc);
    Return<int32_t> ret = mTvServer->switchInputSrc(inputSrc);
    if (!ret.isOk()) {
        ALOGE("switchInputSrc error");
    }
    return ret;
}

int TvServerHidlClient::getInputSrcConnectStatus(int32_t inputSrc) {
    //return mTvServer->getInputSrcConnectStatus(inputSrc);
        Return<int32_t> ret = mTvServer->getInputSrcConnectStatus(inputSrc);
    if (!ret.isOk()) {
        ALOGE("getInputSrcConnectStatus error");
    }
    return ret;
}

int TvServerHidlClient::getCurrentInputSrc() {
    //return mTvServer->getCurrentInputSrc();
    Return<int32_t> ret = mTvServer->getCurrentInputSrc();
    if (!ret.isOk()) {
        ALOGE("getCurrentInputSrc error");
    }
    return ret;
}

int TvServerHidlClient::getHdmiAvHotplugStatus() {
    //return mTvServer->getHdmiAvHotplugStatus();
    Return<int32_t> ret = mTvServer->getHdmiAvHotplugStatus();
    if (!ret.isOk()) {
        ALOGE("getHdmiAvHotplugStatus error");
    }
    return ret;
}

std::string TvServerHidlClient::getSupportInputDevices() {
    int ret;
    std::string tvDevices;
    Return<void> result = mTvServer->getSupportInputDevices([&](int32_t result, const ::android::hardware::hidl_string& devices) {
        ret = result;
        tvDevices = devices;
    });
    if (!result.isOk()) {
        ALOGE("getSupportInputDevices error");
    }
    return tvDevices;
}

int TvServerHidlClient::getHdmiPorts(int32_t inputSrc) {
    //return mTvServer->getHdmiPorts(inputSrc);
    Return<int32_t> ret = mTvServer->getHdmiPorts(inputSrc);
    if (!ret.isOk()) {
        ALOGE("getHdmiPorts error");
    }
    return ret;
}

SignalInfo TvServerHidlClient::getCurSignalInfo() {
    SignalInfo signalInfo;
    Return<void> ret = mTvServer->getCurSignalInfo([&](const SignalInfo& info) {
        signalInfo.fmt = info.fmt;
        signalInfo.transFmt = info.transFmt;
        signalInfo.status = info.status;
        signalInfo.frameRate = info.frameRate;
    });
    if (!ret.isOk()) {
        ALOGE("getCurSignalInfo error");
    }
    return signalInfo;
}

int TvServerHidlClient::setMiscCfg(const std::string& key, const std::string& val) {
    //return mTvServer->setMiscCfg(key, val);
    Return<int32_t> ret = mTvServer->setMiscCfg(key, val);
    if (!ret.isOk()) {
        ALOGE("setMiscCfg error");
    }
    return ret;
}

std::string TvServerHidlClient::getMiscCfg(const std::string& key, const std::string& def) {
    std::string miscCfg;
    Return<void> ret = mTvServer->getMiscCfg(key, def, [&](const std::string& cfg) {
        miscCfg = cfg;
    });
    if (!ret.isOk()) {
        ALOGE("getMiscCfg error");
    }
    return miscCfg;
}

int TvServerHidlClient::loadEdidData(int32_t isNeedBlackScreen, int32_t isDolbyVisionEnable) {
    //return mTvServer->loadEdidData(isNeedBlackScreen, isDolbyVisionEnable);
    Return<int32_t> ret = mTvServer->loadEdidData(isNeedBlackScreen, isDolbyVisionEnable);
    if (!ret.isOk()) {
        ALOGE("loadEdidData error");
    }
    return ret;
}

int TvServerHidlClient::updateEdidData(int32_t inputSrc, const std::string& edidData) {
    //return mTvServer->updateEdidData(inputSrc, edidData);
    Return<int32_t> ret = mTvServer->updateEdidData(inputSrc, edidData);
    if (!ret.isOk()) {
        ALOGE("updateEdidData error");
    }
    return ret;
}

int TvServerHidlClient::setHdmiEdidVersion(int32_t port_id, int32_t ver) {
    //return mTvServer->setHdmiEdidVersion(port_id, ver);
    Return<int32_t> ret = mTvServer->setHdmiEdidVersion(port_id, ver);
    if (!ret.isOk()) {
        ALOGE("setHdmiEdidVersion error");
    }
    return ret;
}

int TvServerHidlClient::getHdmiEdidVersion(int32_t port_id) {
    //return mTvServer->getHdmiEdidVersion(port_id);
    Return<int32_t> ret = mTvServer->getHdmiEdidVersion(port_id);
    if (!ret.isOk()) {
        ALOGE("setHdmiEdidVersion error");
    }
    return ret;
}

int TvServerHidlClient::saveHdmiEdidVersion(int32_t port_id, int32_t ver) {
    //return mTvServer->saveHdmiEdidVersion(port_id, ver);
    Return<int32_t> ret = mTvServer->saveHdmiEdidVersion(port_id, ver);
    if (!ret.isOk()) {
        ALOGE("saveHdmiEdidVersion error");
    }
    return ret;
}

int TvServerHidlClient::setHdmiColorRangeMode(int32_t range_mode) {
    //return mTvServer->setHdmiColorRangeMode(range_mode);
    Return<int32_t> ret = mTvServer->setHdmiColorRangeMode(range_mode);
    if (!ret.isOk()) {
        ALOGE("setHdmiColorRangeMode error");
    }
    return ret;
}

int TvServerHidlClient::getHdmiColorRangeMode() {
    //return mTvServer->getHdmiColorRangeMode();
    Return<int32_t> ret = mTvServer->getHdmiColorRangeMode();
    if (!ret.isOk()) {
        ALOGE("getHdmiColorRangeMode error");
    }
    return ret;
}

FormatInfo TvServerHidlClient::getHdmiFormatInfo() {
    FormatInfo info;
    Return<void> ret = mTvServer->getHdmiFormatInfo([&](const FormatInfo formatInfo) {
        info.width     = formatInfo.width;
        info.height    = formatInfo.height;
        info.fps       = formatInfo.fps;
        info.interlace = formatInfo.interlace;
    });
    if (!ret.isOk()) {
       ALOGE("getHdmiFormatInfo error");
    }
    return info;
}

int TvServerHidlClient::handleGPIO(const std::string& key, int32_t is_out, int32_t edge) {
    return mTvServer->handleGPIO(key, is_out, edge);
    Return<int32_t> ret = mTvServer->handleGPIO(key, is_out, edge);
    if (!ret.isOk()) {
        ALOGE("handleGPIO error");
    }
    return ret;
}

int TvServerHidlClient::vdinUpdateForPQ(int32_t gameStatus, int32_t pcStatus, int32_t autoSwitchFlag) {
    //return mTvServer->vdinUpdateForPQ(gameStatus, pcStatus, autoSwitchFlag);
    Return<int32_t> ret = mTvServer->vdinUpdateForPQ(gameStatus, pcStatus, autoSwitchFlag);
    if (!ret.isOk()) {
        ALOGE("vdinUpdateForPQ error");
    }
    return ret;
}

int TvServerHidlClient::setWssStatus(int status) {
    //return mTvServer->setWssStatus(status);
    Return<int32_t> ret = mTvServer->setWssStatus(status);
    if (!ret.isOk()) {
        ALOGE("setWssStatus error");
    }
    return ret;
}

int TvServerHidlClient::setDeviceIdForCec(int DeviceId) {
    //return mTvServer->setDeviceIdForCec(DeviceId);
    Return<int32_t> ret = mTvServer->setDeviceIdForCec(DeviceId);
    if (!ret.isOk()) {
        ALOGE("setDeviceIdForCec error");
    }
    return ret;
}

int TvServerHidlClient::setScreenColorForSignalChange(int screenColor, int is_save) {
    //return mTvServer->setScreenColorForSignalChange(screenColor, is_save);
    Return<int32_t> ret = mTvServer->setScreenColorForSignalChange(screenColor, is_save);
    if (!ret.isOk()) {
        ALOGE("setScreenColorForSignalChange error");
    }
    return ret;
}

int TvServerHidlClient::getScreenColorForSignalChange() {
    //return mTvServer->getScreenColorForSignalChange();
    Return<int32_t> ret = mTvServer->getScreenColorForSignalChange();
    if (!ret.isOk()) {
        ALOGE("getScreenColorForSignalChange error");
    }
    return ret;
}

int TvServerHidlClient::dtvGetSignalSNR() {
    //return mTvServer->dtvGetSignalSNR();
    Return<int32_t> ret = mTvServer->dtvGetSignalSNR();
    if (!ret.isOk()) {
        ALOGE("dtvGetSignalSNR error");
    }
    return ret;
}

BasicVdecState TvServerHidlClient::getBasicVdecStatusInfo(int vdecId) {
    BasicVdecState info;
    Return<void> ret = mTvServer->getBasicVdecStatusInfo(vdecId,[&](const BasicVdecState vInfo) {
        info.decode_time_cost   = vInfo.decode_time_cost;
        info.frame_width        = vInfo.frame_width;
        info.frame_height       = vInfo.frame_height;
        info.frame_rate         = vInfo.frame_rate;
        info.error_count        = vInfo.error_count;
        info.frame_count        = vInfo.frame_count;
        info.error_frame_count  = vInfo.error_frame_count;
        info.drop_frame_count   = vInfo.drop_frame_count;
        info.double_write_mode  = vInfo.double_write_mode;
    });
    if (!ret.isOk()) {
        ALOGE("getBasicVdecStatusInfo error");
    }
    return info;
}

// callback from tv service
Return<void> TvServerHidlClient::TvServerHidlCallback::notifyCallback(const TvHidlParcel& hidlParcel)
{
    ALOGI("notifyCallback event type:%d", hidlParcel.msgType);

#if 0
    Parcel p;

    sp<IMemory> memory = android::hardware::mapMemory(parcelMem);
    void* data = memory->getPointer();
    memory->update();
    // update memory however you wish after calling update and before calling commit
    p.setDataPosition(0);
    p.write(data, size);
    memory->commit();

#endif

    sp<TvListener> listener;
    {
        Mutex::Autolock _l(mLock);
        listener = tvserverClient->mListener;
    }

    tv_parcel_t parcel;
    parcel.msgType = hidlParcel.msgType;
    for (int i = 0; i < hidlParcel.bodyInt.size(); i++) {
        parcel.bodyInt.push_back(hidlParcel.bodyInt[i]);
    }

    for (int j = 0; j < hidlParcel.bodyString.size(); j++) {
        parcel.bodyString.push_back(hidlParcel.bodyString[j]);
    }

    if (listener != NULL) {
        listener->notify(parcel);
    }
    return Void();
}

void TvServerHidlClient::TvServerDaemonDeathRecipient::serviceDied(uint64_t cookie __unused,
        const ::android::wp<::android::hidl::base::V1_0::IBase>& who __unused)
{
    ALOGE("tvserver daemon died");

    usleep(200*1000);//sleep 200ms
    tvserverClient->reconnect();
}
}
