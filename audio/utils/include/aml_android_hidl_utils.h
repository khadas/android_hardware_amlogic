/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#ifndef __AML_ANDROID_HIDL_UTILS_H__
#define __AML_ANDROID_HIDL_UTILS_H__

#include <utils/Log.h>
#include <system/audio.h>
#include <utils/RefBase.h>
#include <utils/Errors.h>
#include <utils/String8.h>

namespace android {

status_t setParameters(const String8& keyValuePairs);
String8  getParameters(const String8& keys);

}

#endif
