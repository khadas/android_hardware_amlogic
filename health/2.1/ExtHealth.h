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
 */

#ifndef ANDROID_HARDWARE_HEALTH_AMLOGIC_V2_1_HEALTH_H
#define ANDROID_HARDWARE_HEALTH_AMLOGIC_V2_1_HEALTH_H
#include <health/utils.h>
#include <health2impl/Health.h>

namespace android {
namespace hardware {
namespace health {
namespace V2_1 {
namespace extension {

using ::android::sp;
using ::android::hardware::health::InitHealthdConfig;
using ::android::hardware::health::V2_1::IHealth;
using ::android::hardware::health::V2_1::implementation::Health;

struct ExtHealth : public Health {
     ExtHealth(std::unique_ptr<healthd_config>&& config);
protected:
     virtual void UpdateHealthInfo(HealthInfo* health_info) override;
};

extern "C" IHealth* HIDL_FETCH_IHealth(const char* instance);

}  // namespace extension
}  // namespace V2_1
}  // namespace health
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_HEALTH_AMLOGIC_V2_1_HEALTH_H
