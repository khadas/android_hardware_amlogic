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
*/


#ifndef CAMHAL_DEBUG_LOG_H
#define CAMHAL_DEBUG_LOG_H

#include <stdint.h>
#include <utils/Log.h>


///Camera HAL Logging Functions

extern int getCamHalLogLevel();
extern int updateCamHalLogLevel();
#define CAMHAL_LOG_TAG   "[camhal]"
// define CAMHAL_DEBUG in Android.mk FILE. otherwise, it is a release version.
#ifndef CAMHAL_DEBUG

#define CAMHAL_LOGVV(str, ...)  void(0)
#define CAMHAL_LOGV(str, ...)   void(0)
#define CAMHAL_LOGD(str, ...)   void(0)
#define CAMHAL_LOGI(str, ...)   ALOGI(CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define CAMHAL_LOGW(str, ...)   ALOGW(CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define CAMHAL_LOGE(str, ...)   ALOGE(CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)
#define CAMHAL_LOGF(str, ...)   ALOGF(CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#undef CAMHAL_LOG_FUNCTION_ENTER
#undef CAMHAL_LOG_FUNCTION_EXIT
#define CAMHAL_LOG_FUNCTION_ENTER() void(0)
#define CAMHAL_LOG_FUNCTION_EXIT()  void(0)

#ifndef CAMHAL_BUILD_NAME
#define CAMHAL_BUILD_NAME  "==camera hal release==="
#endif


#else

#ifndef CAMHAL_BUILD_NAME
#define CAMHAL_BUILD_NAME  "===|||camera debug|||==="
#endif

#define CAMHAL_LOGVV(str,...)     ALOGV_IF(getCamHalLogLevel() >= 7,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGV(str,...)     ALOGV_IF(getCamHalLogLevel() >= 6,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGD(str, ...)    ALOGD_IF(getCamHalLogLevel() >=5,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__, __FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGI(str, ...)    ALOGI_IF(getCamHalLogLevel() >=4,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__,__FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGW(str, ...)    ALOGW_IF(getCamHalLogLevel() >=3,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__,__FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGE(str, ...)    ALOGE_IF(getCamHalLogLevel() >=2,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__,__FUNCTION__, ##__VA_ARGS__)

#define CAMHAL_LOGF(str, ...)    ALOGF_IF(getCamHalLogLevel() >=1,CAMHAL_LOG_TAG " %5d %s - " str, __LINE__,__FUNCTION__, ##__VA_ARGS__)

#undef CAMHAL_LOG_FUNCTION_ENTER
#undef CAMHAL_LOG_FUNCTION_EXIT
#define CAMHAL_LOG_FUNCTION_ENTER()      CAMHAL_LOGV("ENTER")
#define CAMHAL_LOG_FUNCTION_EXIT()       CAMHAL_LOGV("EXIT")

#endif

#endif //CAMHAL_DEBUG_LOG_H

