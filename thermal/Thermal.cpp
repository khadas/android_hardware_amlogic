/*
 * Copyright (C) 2022 The Android Open Source Project
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

#define LOG_TAG "thermal_service_droidlogic"

#include "Thermal.h"

#include <android-base/logging.h>
#include <android-base/file.h>
#include <utils/Trace.h>

namespace aidl::android::hardware::thermal::impl::droidlogic {

using ndk::ScopedAStatus;

namespace {

 ndk::ScopedAStatus initErrorStatus() {
      return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_STATE,
                                                              "ThermalHAL not initialized properly.");
  }

  ndk::ScopedAStatus readErrorStatus() {
      return ndk::ScopedAStatus::fromExceptionCodeWithMessage(
              EX_ILLEGAL_STATE, "ThermalHal cannot read any sensor data");
  }


bool interfacesEqual(const std::shared_ptr<::ndk::ICInterface>& left,
                     const std::shared_ptr<::ndk::ICInterface>& right) {
    if (left == nullptr || right == nullptr || !left->isRemote() || !right->isRemote()) {
        return left == right;
    }
    return left->asBinder() == right->asBinder();
}

}  // namespace

Thermal::Thermal()
      : thermal_helper_(
                std::bind(&Thermal::sendThermalChangedCallback, this, std::placeholders::_1)) {}


ScopedAStatus Thermal::getCoolingDevices(std::vector<CoolingDevice>* _aidl_return) {
    LOG(VERBOSE) << __func__;
    return getFilteredCoolingDevices(false, CoolingType::BATTERY, _aidl_return);
}

ScopedAStatus Thermal::getCoolingDevicesWithType(CoolingType type,
                                                 std::vector<CoolingDevice>* _aidl_return) {
    LOG(VERBOSE) << __func__ << " CoolingType: " << static_cast<int32_t>(type);
    return getFilteredCoolingDevices(true, type, _aidl_return);
}

ScopedAStatus Thermal::getFilteredCoolingDevices(bool filterType, CoolingType type,
                                                        std::vector<CoolingDevice> *_aidl_return) {
      *_aidl_return = {};
      if (!thermal_helper_.isInitializedOk()) {
          return initErrorStatus();
      }
      if (!thermal_helper_.fillCurrentCoolingDevices(filterType, type, _aidl_return)) {
          return readErrorStatus();
      }
      return ndk::ScopedAStatus::ok();
  }

ScopedAStatus Thermal::getTemperatures(std::vector<Temperature>* _aidl_return) {
    LOG(VERBOSE) << __func__;
    return getFilteredTemperatures(false, TemperatureType::UNKNOWN, _aidl_return);
}

ScopedAStatus Thermal::getTemperaturesWithType(TemperatureType type,
                                               std::vector<Temperature>* _aidl_return) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(type);
    return getFilteredTemperatures(true, type, _aidl_return);
}

  ScopedAStatus Thermal::getFilteredTemperatures(bool filterType, TemperatureType type,
                                                      std::vector<Temperature> *_aidl_return) {
      *_aidl_return = {};
      if (!thermal_helper_.isInitializedOk()) {
          return initErrorStatus();
      }
      if (!thermal_helper_.fillCurrentTemperatures(filterType, type, _aidl_return)) {
          return readErrorStatus();
      }
      return ndk::ScopedAStatus::ok();
  }


ScopedAStatus Thermal::getTemperatureThresholds(
        std::vector<TemperatureThreshold>* _aidl_return) {
    LOG(VERBOSE) << __func__;
    *_aidl_return = {};
    return getFilteredTemperatureThresholds(false, TemperatureType::UNKNOWN, _aidl_return);
}

ScopedAStatus Thermal::getTemperatureThresholdsWithType(
        TemperatureType type,
        std::vector<TemperatureThreshold>* _aidl_return ) {
    LOG(VERBOSE) << __func__ << " TemperatureType: " << static_cast<int32_t>(type);
    return getFilteredTemperatureThresholds(true, type, _aidl_return);
}

ScopedAStatus Thermal::getFilteredTemperatureThresholds(
        bool filterType, TemperatureType type, std::vector<TemperatureThreshold> *_aidl_return) {
    *_aidl_return = {};
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    if (!thermal_helper_.fillTemperatureThresholds(filterType, type, _aidl_return)) {
        return readErrorStatus();
    }
    return ndk::ScopedAStatus::ok();
}


  ScopedAStatus Thermal::registerThermalChangedCallback(
          const std::shared_ptr<IThermalChangedCallback> &callback) {
      ATRACE_CALL();
      return registerThermalChangedCallback(callback, false, TemperatureType::UNKNOWN);
  }

  ScopedAStatus Thermal::registerThermalChangedCallbackWithType(
          const std::shared_ptr<IThermalChangedCallback> &callback, TemperatureType type) {
      ATRACE_CALL();
      return registerThermalChangedCallback(callback, true, type);
  }

ScopedAStatus Thermal::registerThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback, bool filterType,
        TemperatureType type) {
    std::vector<Temperature> temperatures;

    ATRACE_CALL();
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    if (!thermal_helper_.isInitializedOk()) {
        return initErrorStatus();
    }
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    if (std::any_of(callbacks_.begin(), callbacks_.end(), [&](const CallbackSetting &c) {
            return interfacesEqual(c.callback, callback);
        })) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback already registered");
    }
    auto c = callbacks_.emplace_back(callback, filterType, type);
    LOG(INFO) << "a callback has been registered to ThermalHAL, isFilter: " << c.is_filter_type
              << " Type: " << toString(c.type);
    // Send notification right away after successful thermal callback registration
    if (thermal_helper_.fillCurrentTemperatures(filterType, type, &temperatures)) {
        for (const auto &t : temperatures) {
            if (!filterType || t.type == type) {
                LOG(INFO) << "Sending notification: "
                          << " Type: " << toString(t.type) << " Name: " << t.name
                          << " CurrentValue: " << t.value
                          << " ThrottlingStatus: " << toString(t.throttlingStatus);
                c.callback->notifyThrottling(t);
            }
        }
    }
    return ndk::ScopedAStatus::ok();
}

ScopedAStatus Thermal::unregisterThermalChangedCallback(
        const std::shared_ptr<IThermalChangedCallback> &callback) {
    if (callback == nullptr) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Invalid nullptr callback");
    }
    bool removed = false;
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    callbacks_.erase(
            std::remove_if(
                    callbacks_.begin(), callbacks_.end(),
                    [&](const CallbackSetting &c) {
                        if (interfacesEqual(c.callback, callback)) {
                            LOG(INFO)
                                    << "a callback has been unregistered to ThermalHAL, isFilter: "
                                    << c.is_filter_type << " Type: " << toString(c.type);
                            removed = true;
                            return true;
                        }
                        return false;
                    }),
            callbacks_.end());
    if (!removed) {
        return ndk::ScopedAStatus::fromExceptionCodeWithMessage(EX_ILLEGAL_ARGUMENT,
                                                                "Callback wasn't registered");
    }
    return ndk::ScopedAStatus::ok();
}

void Thermal::sendThermalChangedCallback(const std::vector<Temperature> &temps) {
    std::lock_guard<std::mutex> _lock(thermal_callback_mutex_);
    for (auto &t : temps) {
        LOG(INFO) << "Sending notification: "
                  << " Type: " << toString(t.type)
                  << " Name: " << t.name << " CurrentValue: " << t.value << " ThrottlingStatus: "
                  << toString(t.throttlingStatus);
        callbacks_.erase(
            std::remove_if(callbacks_.begin(), callbacks_.end(),
                           [&](const CallbackSetting &c) {
                               if (!c.is_filter_type || t.type == c.type) {
                                   ::ndk::ScopedAStatus ret = c.callback->notifyThrottling(t);
                                   return !ret.isOk();
                               }
                               LOG(ERROR)
                                   << "a Thermal callback is dead, removed from callback list.";
                               return false;
                           }),
            callbacks_.end());
    }
}


}  // namespace aidl::android::hardware::thermal::impl::droidlogic
