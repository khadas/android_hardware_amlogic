/*
 * Copyright (C) 2020 The Android Open Source Project
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
 */

#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)
#define LOG_TAG "android.hardware.power-service.libperfmgr"

#include "Power.h"
#include "PowerHintSession.h"
#include <mutex>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>

#include <utils/Log.h>
#include <utils/Trace.h>

#include "disp-power/DisplayLowPower.h"

namespace aidl {
namespace hardware {
namespace power {
namespace impl {
namespace droidlogic {

using namespace std::chrono_literals;
using ndk::ScopedAStatus;

static int num = 0;

Power::Power(std::shared_ptr<HintManager> hm, std::shared_ptr<DisplayLowPower> dlpw)
    : mHintManager(hm),
      mDisplayLowPower(dlpw),
      mInteractionHandler(nullptr),
      mVRModeOn(false),
      mSustainedPerfModeOn(false) {
    mInteractionHandler = std::make_unique<InteractionHandler>(mHintManager);
    mInteractionHandler->Init();
    mSysCtrl = ::android::SystemControlClient::getInstance();

    // Now start to take powerhint
    ALOGI("PowerHAL ready to process hints");
}

ndk::ScopedAStatus Power::setMode(Mode type, bool enabled) {
    LOG(INFO) << "Power setMode: " << toString(type) << " to: " << enabled;
    ATRACE_INT(toString(type).c_str(), enabled);
    switch (type) {
        case Mode::INTERACTIVE:
            if (enabled) {
                if (num != 0) {
                    std::string value;
                    mSysCtrl->getBootEnv("ubootenv.var.quiescent_env_bk", value);
                    if (!value.empty())
                        LOG(INFO) << "quiescent_env_bk: " << value.c_str();
                    mSysCtrl->setBootEnv("ubootenv.var.quiescent_env_bk", "0");
                }
                mHintManager->EndHint("INTERACTIVE");
            } else {
                mHintManager->DoHint("INTERACTIVE");
            }
            num ++;
            break;
        default:
            LOG(INFO) << "Power mode " << toString(type) << " is not supported now";
            break;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isModeSupported(Mode type, bool *_aidl_return) {
    bool supported = mHintManager->IsHintSupported(toString(type));
    // LOW_POWER handled insides PowerHAL specifically
    if (type == Mode::LOW_POWER) {
        supported = true;
    }
    LOG(INFO) << "Power mode " << toString(type) << " isModeSupported: " << supported;
    *_aidl_return = supported;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::setBoost(Boost type, int32_t durationMs) {
    ATRACE_INT(toString(type).c_str(), durationMs);
    switch (type) {
        case Boost::INTERACTION:
            if (durationMs == 0) {
                /*For DoHint can't sussess until the pre DoHint is timeout.
                  (default duration is std::chrono::milliseconds::max() Ms)
                  We need to set INTERACTION/CPUcmdwe in every input event.
                  So we add timeout as 300ms as default timeout.*/
                mHintManager->DoHint("INTERACTION",(std::chrono::milliseconds)(durationMs+300));
            } else {
                mHintManager->DoHint("INTERACTION", (std::chrono::milliseconds)durationMs);
            }
            break;
        default:
            LOG(INFO) << "Power setBoost " << toString(type) << " is not supported now";
            break;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::isBoostSupported(Boost type, bool *_aidl_return) {
    bool supported = mHintManager->IsHintSupported(toString(type));
    LOG(INFO) << "Power boost " << toString(type) << " isBoostSupported: " << supported;
    *_aidl_return = supported;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::createHintSession(int32_t tgid, int32_t uid,
                                         const std::vector<int32_t>& threadIds,
                                         int64_t durationNanos,
                                         std::shared_ptr<IPowerHintSession>* _aidl_return) {
    if (threadIds.size() == 0) {
        *_aidl_return = nullptr;
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }
    std::shared_ptr<IPowerHintSession> session =
            ndk::SharedRefBase::make<PowerHintSession>(tgid, uid, threadIds, durationNanos);
    mPowerHintSessions.push_back(session);
    *_aidl_return = session;
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus Power::getHintSessionPreferredRate(int64_t* outNanoseconds) {
    *outNanoseconds = std::chrono::nanoseconds(1ms).count();
    return ndk::ScopedAStatus::ok();
}

constexpr const char *boolToString(bool b) {
    return b ? "true" : "false";
}

binder_status_t Power::dump(int fd, const char **, uint32_t) {
    std::string buf(::android::base::StringPrintf(
            "HintManager Running: %s\n"
            "VRMode: %s\n"
            "SustainedPerformanceMode: %s\n",
            boolToString(mHintManager->IsRunning()), boolToString(mVRModeOn),
            boolToString(mSustainedPerfModeOn)));
    // Dump nodes through libperfmgr
    mHintManager->DumpToFd(fd);
    if (!::android::base::WriteStringToFd(buf, fd)) {
        PLOG(ERROR) << "Failed to dump state to fd";
    }
    fsync(fd);
    return STATUS_OK;
}

}  // namespace droidlogic
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace aidl
