/*
 * Copyright (C) 2010 Amlogic Corporation.
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



/*
 harman_dsp_process.c
 */
#define LOG_TAG "harman_dsp"
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dlfcn.h>

#include <cutils/log.h>
#include <cutils/properties.h>

#include "harman_dsp_process.h"

int set_dsp_mode(void) {
    char value[PROPERTY_VALUE_MAX];
    if (property_get("persist.harman.dspmode", value, "0") > 0) {
        int dspmode = atoi(value);
        /* 0 is normal mode; 1 is hw test mode; 2 is bypass mode*/
        if (dspmode <= 2 && dsp_setParameter(0x10010070, &dspmode) == 0) {
            ALOGI("%s: set dsp mode is %d", __func__, dspmode);
        } else {
            ALOGE("%s: set dsp mode fail: %d", __func__, dspmode);
            return -1;
        }
    }
    return 0;
}

int set_EQ_mode(int EQ_mode) {
    if (EQ_mode < NUM_EQ_MODE && EQ_mode >= EQ_STANDARD
            && dsp_setParameter(0x10010030, &EQ_mode) == 0) {
        ALOGI("%s: set EQ mode is %d", __func__, EQ_mode);
    } else {
        ALOGE("%s: set EQ mode fail: %d", __func__, EQ_mode);
        return -1;
    }
    return 0;
}

int set_subwoofer_volume(int volume) {
    if (dsp_setParameter(0x10010080, &volume) == 0) {
        ALOGI("%s: set subwoofer volume is %d", __func__, volume);
    } else {
        ALOGI("%s: set subwoofer volume fail %d", __func__, volume);
        return -1;
    }
    return 0;
}

//-------------------------HARMAN DSP---------------------------------------------------------
typedef void* PSTATE;
PSTATE (*DSP_init)(int inStrTypeId, int numInChannels, int numOutChannels, int dataIsInterleaved, int sampleRate);
int (*DSP_release)(PSTATE state);
int (*DSP_setParameter)(PSTATE state, int paramID, void* paramVal);
int (*DSP_process)(void* inBuffer, void* outBuffer,  void* aecBuffer, void* mePtr, int num_frames);
static void *gDSPLibHandler = NULL;
static PSTATE gDSPState = NULL;

static int pcm_format_convert(int pcm_format)
{
    int ret = -1;
    switch (pcm_format)
    {
        case 0: /* PCM_FORMAT_S16_LE */
        {
            ret = 16; /*int16*/
        }
        break;

        case 1: /* PCM_FORMAT_S32_LE */
        {
            ret = 32; /*int32*/
        }
        break;

        case 4: /* PCM_FORMAT_S24_LE_3LE */
        {
            ret = 24; /*int24*/
        }
        break;

        default:
            ret = -1;
            break;
    }
    return ret;
}

int dsp_init(int inStrTypeId, int numInChannels, int numOutChannels, int sampleRate)
{
    int format;

    if (DSP_init == NULL)
    {
        ALOGE("%s, pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    format = pcm_format_convert(inStrTypeId);

    if (format < 0)
    {
        ALOGE("unsupported type\n");
        return -2;
    }

    gDSPState = (*DSP_init)(format, numInChannels, numOutChannels, 1, sampleRate);
    if (gDSPState == NULL)
    {
        ALOGE("%s, gDSPState == NULL.\n", __FUNCTION__);
        return -3;
    }

    return 0;
}

int dsp_release(void)
{
    int ret = 0;
    if (DSP_release == NULL)
    {
        ALOGE("%s, pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*DSP_release)(gDSPState);

    ALOGI("%s %s\n", __FUNCTION__, ret == 0 ? "ok":"fail");
    /* ok is 0*/
    return ret;
}

int dsp_setParameter(int paramID, void* paramVal)
{
    int ret = 0;
    if (DSP_setParameter == NULL)
    {
        ALOGE("%s, pls load lib first.\n", __FUNCTION__);
        return -1;
    }

    ret = (*DSP_setParameter)(gDSPState, paramID, paramVal);
    ALOGI("%s %s\n", __FUNCTION__, ret == 0 ? "ok":"fail");

    /* ok is 0*/
    return ret;
}

static int getprop_bool(const char * path)
{
    char buf[PROPERTY_VALUE_MAX];
    int ret = -1;

    ret = property_get(path, buf, NULL);
    if (ret > 0)
    {
        if (strcasecmp(buf, "true") == 0 || strcmp(buf, "1") == 0)
            return 1;
    }

    return 0;
}


int dsp_process(void* inBuffer, void* outBuffer, void* aecBuffer, int num_frames)
{
    int ret = 0;

    if (DSP_process == NULL)
    {
        ALOGE("%s, pls load lib first.\n", __FUNCTION__);
        return -1;
    }
    if (getprop_bool("media.audiohal_dump_in_en" ))
    {
        FILE *dump_fp = NULL;
        dump_fp = fopen("/data/local/tmp/in.pcm", "a+");
        if (dump_fp != NULL)
        {
            fwrite(inBuffer, num_frames * 20, 1, dump_fp); //2ch*4bytes
            fclose(dump_fp);
        }
        else
        {
            ALOGE("[Error] Can't write to /data/local/tmp/in.pcm");
        }
    }
    ret = (*DSP_process)(inBuffer, outBuffer, aecBuffer, gDSPState, num_frames);

    if (getprop_bool("media.audiohal_dump_out_en" ))
    {
        FILE *dump_fp = NULL;
        dump_fp = fopen("/data/local/tmp/out.pcm", "a+");
        if (dump_fp != NULL)
        {
            fwrite(outBuffer, num_frames * 20, 1, dump_fp); //5ch*4bytes
            fclose(dump_fp);
        }
        else
        {
            ALOGE("[Error] Can't write to /data/local/tmp/out.pcm");
        }
    }
    /* ok is 0*/

    return ret;
}


int unload_DSP_lib(void)
{
    dsp_release();
    gDSPState = NULL;

    DSP_init = NULL;
    DSP_release = NULL;
    DSP_setParameter = NULL;
    DSP_process = NULL;

    if (gDSPLibHandler != NULL)
    {
        dlclose(gDSPLibHandler);
        ALOGE("%s dlclose \n", __FUNCTION__);
        gDSPLibHandler = NULL;
    }

    return 0;
}

int load_DSP_lib(void)
{
    unload_DSP_lib();

    gDSPLibHandler = dlopen("/vendor/lib/soundfx/libharman_dsp.so", RTLD_NOW);
    if (!gDSPLibHandler)
    {
        ALOGE("%s, failed to load harman_dsp lib (libharman_dsp.so)\n", __FUNCTION__);
        goto Error;
    }

    DSP_init = (PSTATE (*)(int,int,int,int,int))dlsym(gDSPLibHandler, "InitEffect");
    if (DSP_init == NULL)
    {
        ALOGE("%s, fail find func DSP_init()\n", __FUNCTION__);
        goto Error;
    }

    DSP_release = (int (*)(void *))dlsym(gDSPLibHandler, "Deallocate");
    if (DSP_release == NULL)
    {
        ALOGE("%s, fail find func DSP_release()\n", __FUNCTION__);
        goto Error;
    }

    DSP_setParameter = (int (*)(void*, int, void*))dlsym(gDSPLibHandler, "SetParam");
    if (DSP_setParameter == NULL)
    {
        ALOGE("%s, fail find func DSP_setParameter()\n", __FUNCTION__);
        goto Error;
    }

    DSP_process = (int (*)(void*, void*, void*, void*, int))dlsym(gDSPLibHandler, "Process");
    if (DSP_process == NULL)
    {
        ALOGE("%s, fail find func DSP_process()\n", __FUNCTION__);
        goto Error;
    }

    ALOGD("%s, load dsp lib ok \n", __FUNCTION__);
    return 0;

Error:
    unload_DSP_lib();

    return -1;

}


