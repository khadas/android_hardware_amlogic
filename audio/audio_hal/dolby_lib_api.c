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

#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <unistd.h>
#include <cutils/log.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <string.h>

#include "dolby_lib_api.h"

#ifndef RET_OK
#define RET_OK 0
#endif

#ifndef RET_FAIL
#define RET_FAIL -1
#endif

#ifndef MS12_V24_ENABLE
    #define MS12_VERSION    "1.3"
#else
    #define MS12_VERSION    "2.4"
#endif


/*
 *@brief file_accessible
 */
static int file_accessible(char *path)
{
    // file is readable or not
    if (access(path, R_OK) == 0) {
        return RET_OK;
    } else {
        return RET_FAIL;
    }
}

char * get_ms12_path (void)
{
    ALOGI("%s return lib %s", __func__, DOLBY_MS12_LIB_PATH_A);
    return DOLBY_MS12_LIB_PATH_A;

}

bool is_ms12_lib_match(void *hDolbyMS12LibHanle) {
    bool b_match = false;
    char * (*FunDolbMS12GetVersion)(void) = NULL;

    /*get dolby version*/
    if (hDolbyMS12LibHanle) {
        FunDolbMS12GetVersion = (char * (*)(void)) dlsym(hDolbyMS12LibHanle, "ms12_get_version");
        if (FunDolbMS12GetVersion) {
            if (strstr((*FunDolbMS12GetVersion)(), MS12_VERSION) != NULL) {
                b_match = true;
            }
            if (b_match == false) {
                ALOGE("ms12 doesn't match build version =%s lib %s", MS12_VERSION, (*FunDolbMS12GetVersion)());
            } else {
                ALOGI("ms12 match build version =%s lib %s", MS12_VERSION, (*FunDolbMS12GetVersion)());
            }
        } else {
            b_match = false;
            ALOGE("ms12 version not found, try ddp lib");
        }
    }
    return b_match;

}

/*
 *@brief detect_dolby_lib_type
 */
enum eDolbyLibType detect_dolby_lib_type(void) {
    enum eDolbyLibType retVal = eDolbyNull;

    void *hDolbyMS12LibHanle = NULL;
    void *hDolbyDcvLibHanle = NULL;

    // the priority would be "MS12 > DCV" lib
    ALOGI("%s return lib %s", __func__, DOLBY_MS12_LIB_PATH_A);
    if (RET_OK == file_accessible(DOLBY_MS12_LIB_PATH_A)) {
        ALOGI("%s file_accessible got ok", __func__);
        retVal = eDolbyMS12Lib;
    }

    // MS12 is first priority
    if (eDolbyMS12Lib == retVal)
    {
        //try to open lib see if it's OK?
        hDolbyMS12LibHanle = dlopen(DOLBY_MS12_LIB_PATH_A, RTLD_NOW);
        if (hDolbyMS12LibHanle != NULL)
        {
            bool b_match = is_ms12_lib_match(hDolbyMS12LibHanle);
            dlclose(hDolbyMS12LibHanle);
            hDolbyMS12LibHanle = NULL;

            /*check ms12 verson*/
            if (b_match) {
                ALOGI("%s,FOUND libdolbyms12 lib\n", __FUNCTION__);
                return eDolbyMS12Lib;
            }
        }
        else {
            ALOGE("%s, failed to FIND libdolbyms12.so, %s\n", __FUNCTION__, dlerror());
        }
    }

    // dcv is second priority
    if (RET_OK == file_accessible(DOLBY_DCV_LIB_PATH_A)) {
        retVal = eDolbyDcvLib;
    } else {
        retVal = eDolbyNull;
    }

    if (eDolbyDcvLib == retVal)
    {
        //try to open lib see if it's OK?
        hDolbyDcvLibHanle  = dlopen(DOLBY_DCV_LIB_PATH_A, RTLD_NOW);
    }

    if (hDolbyDcvLibHanle != NULL)
    {
        dlclose(hDolbyDcvLibHanle);
        hDolbyDcvLibHanle = NULL;
        ALOGI("%s,FOUND libHwAudio_dcvdec lib\n", __FUNCTION__);
        return eDolbyDcvLib;
    }

    ALOGE("%s, failed to FIND libdolbyms12.so and libHwAudio_dcvdec.so, %s\n", __FUNCTION__, dlerror());
    return eDolbyNull;
}

int dolby_lib_decode_enable(eDolbyLibType_t lib_type) {
    int enable = 0;
    if (lib_type == eDolbyMS12Lib) {
        enable = 1;
    } else if (lib_type == eDolbyDcvLib) {
        unsigned int filesize = -1;
        struct stat stat_info;
        if (stat(DOLBY_DCV_LIB_PATH_A, &stat_info) < 0) {
            enable = 0;
        } else {
            filesize = stat_info.st_size;
            if (filesize > 500*1024) {
                enable = 1;
            } else {
                enable = 0;
            }
        }
    } else {
        enable = 0;
    }
    return enable;
}


#ifndef MS12_V24_ENABLE
typedef enum ms_dap_mode_t
{
    DAP_NO_PROC = 0,
    DAP_CONTENT_PROC = 1,
    DAP_DEVICE_PROC = 2,
    DAP_DEVICE_CONTENT_PROC = DAP_DEVICE_PROC | DAP_CONTENT_PROC,
    DAP_SI_PROC = 4,
} ms_dap_mode_t;

int get_ms12_dap_init_mode(bool is_tv)
{
    int dap_init_mode = 0;

    if (is_tv) {
        dap_init_mode = DAP_SI_PROC;
    }
    else {
        dap_init_mode = DAP_NO_PROC;
    }

    return dap_init_mode;
}

bool is_ms12_tuning_dat_in_dut() //Invalid in Dolby MS12 V1.3
{
    return false;
}
#else

typedef enum ms_dap_mode_t
{
    DAP_NO_PROC = 0,
    DAP_CONTENT_PROC = 1,
    DAP_CONTENT_PROC_DEVICE_PROC = 2
} ms_dap_mode_t;

int get_ms12_dap_init_mode(bool is_tv)
{
    int dap_init_mode = 0;

    if (is_tv) {
        dap_init_mode = DAP_CONTENT_PROC_DEVICE_PROC;
    }
    else {
        dap_init_mode = DAP_NO_PROC;
    }

    return dap_init_mode;
}

bool is_ms12_tuning_dat_in_dut() //availabe in Dolby MS12 V2.4 or later
{
    if (file_accessible(DOLBY_TUNING_DAT) == 0)
        return true;
    else
        return false;
}

#endif



int dts_lib_decode_enable() {
    int enable = 0;
    unsigned int filesize = -1;
    struct stat stat_info = {0};

    if (stat(DTS_DCA_LIB_PATH_A, &stat_info) < 0) {
        enable = 0;
    } else {
        filesize = stat_info.st_size;
        if (filesize > 500*1024) {
            enable = 1;
        } else {
            enable = 0;
        }
    }

    return enable;
}
