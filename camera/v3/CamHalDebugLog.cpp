/**
*  @copyright Copyright (c) 2022,2023 Amlogic, Inc.
*
*  This file and its contents ("Software") are protected by intellectual property rights including,
* without limitation, China. and/or foreign copyrights.  This Software is also the confidential and
* proprietary information of Amlogic, Inc. and its licensors.  You may not use, reproduce,
* disclose, distribute, modify, or otherwise prepare derivative works of this Software or any
* portion thereof except pursuant to a signed license agreement or nondisclosure agreement with
*  Amlogic, Inc. or its authorized affiliates.  In the absence of such an agreement, you agree to
* promptly notify and return this Software to Amlogic, Inc.
*
*  THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL Amlogic, INC. OR ITS AFFILIATES BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
* BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
*  COMPUTER FAILURE OR MALFUNCTION; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
*  @history 2023-1-13 created by zhiwei.zhang@amlogic.com
*
*/

#define LOG_TAG "CamHalDbgLog"


#include "stdio.h"
#include <android/log.h>
#include <cutils/properties.h>

#include <utils/Log.h>

// 4 INFO LOG; 5 DEBUG LOG; 6 VERBOSE LOG
#define DEFAULT_LOG_LEVEL  5

static volatile int32_t gCamHal_LogLevel = DEFAULT_LOG_LEVEL;


int getCamHalLogLevel()
{
    return gCamHal_LogLevel;
}

int updateCamHalLogLevel()
{
    char level_value[92];
    int tmp = DEFAULT_LOG_LEVEL;

    if (property_get("vendor.media.camera.loglevel", level_value, NULL) > 0) {
        sscanf(level_value, "%d", &tmp);
        ALOGD("read vendor.media.camera.loglevel property %s value %d\n", level_value, tmp);
    } else {
        ALOGD("Can not read vendor.media.camera.loglevel, using default value %d\n", tmp);
    }

    gCamHal_LogLevel = tmp;

    return tmp;
}

