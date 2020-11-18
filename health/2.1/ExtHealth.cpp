/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <memory>
#include <string_view>
#include <hal_conversion.h>

#include "ExtHealth.h"

namespace android {
namespace hardware {
namespace health {
namespace V2_1 {
namespace extension {

using namespace std::literals;
using ::android::hardware::health::V2_1::implementation::Health;
using android::hardware::health::V1_0::hal_conversion::convertToHealthInfo;

ExtHealth::ExtHealth(std::unique_ptr<healthd_config>&& config) : Health(std::move(config))
{
}

int healthd_board_battery_update(struct android::BatteryProperties *props)
{
    props->chargerAcOnline = true;
    props->chargerUsbOnline = false;
    props->chargerWirelessOnline = false;
    props->maxChargingCurrent = 0;
    props->maxChargingVoltage = 0;
    props->batteryStatus = 2;
    props->batteryHealth = 2;
    props->batteryPresent = false;
    props->batteryLevel = 50;
    props->batteryVoltage = 4;
    props->batteryTemperature = 40;
    props->batteryCurrent = 0;
    props->batteryCycleCount = 0;
    props->batteryFullCharge = 0;
    props->batteryChargeCounter = 10;
    props->batteryTechnology = "Li-ion";

    return 0;
}


void ExtHealth::UpdateHealthInfo(HealthInfo* health_info)
{
    struct BatteryProperties props;
    // fake data for batteryChargeTimeToFullNowSeconds there is no battery
    health_info->batteryChargeTimeToFullNowSeconds = 3000ll;
    //fake data for batteryFullChargeDesignCapacityUah there is no battery
    health_info->batteryFullChargeDesignCapacityUah = 200000;
    healthd_board_battery_update(&props);
    convertToHealthInfo(&props, health_info->legacy.legacy);
}

// Passthrough implementation of the health service. Use default configuration.
// It does not invoke callbacks unless update() is called explicitly. No
// background thread is spawned to handle callbacks.
//
// The passthrough implementation is only allowed in recovery mode, charger, and
// opened by the hwbinder service.
// If Android is booted normally, the hwbinder service is used instead.
//
// This implementation only implements the "default" instance. It rejects
// other instance names.
// Note that the Android framework only reads values from the "default"
// health HAL 2.1 instance.
extern "C" IHealth* HIDL_FETCH_IHealth(const char* instance) {
    if (instance != "default"sv) {
        return nullptr;
    }
    auto config = std::make_unique<healthd_config>();
    InitHealthdConfig(config.get());

    // This implementation uses default config. If you want to customize it
    // (e.g. with healthd_board_init), do it here.

    return new ExtHealth(std::move(config));
}

}  // namespace extension
}  // namespace V2_1
}  // namespace health
}  // namespace hardware
}  // namespace android
