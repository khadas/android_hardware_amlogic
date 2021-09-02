/*
 * hardware/amlogic/audio/utils/aml_dump_debug.c
 *
 * Copyright (C) 2017 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include <aml_dump_debug.h>
#include <aml_android_utils.h>

#undef  LOG_TAG
#define LOG_TAG "aml_dump_debug"

static int gDumpDataFd = -1;

void DoDumpData(const void *data_buf, int size, int aud_src_type) {
    int tmp_type = -1;
    char prop_value[PROPERTY_VALUE_MAX] = { 0 };
    char file_path[PROPERTY_VALUE_MAX] = { 0 };

    memset(prop_value, '\0', PROPERTY_VALUE_MAX);
    property_get("vendor.media.audiohal.dumpdata.en", prop_value, "null");
    if (strcasecmp(prop_value, "null") == 0
            || strcasecmp(prop_value, "0") == 0) {
        if (gDumpDataFd >= 0) {
            close(gDumpDataFd);
            gDumpDataFd = -1;
        }
        return;
    }

    property_get("vendor.media.audiohal.dumpdata.src", prop_value, "null");
    if (strcasecmp(prop_value, "null") == 0
            || strcasecmp(prop_value, "input") == 0
            || strcasecmp(prop_value, "0") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_INPUT;
    } else if (strcasecmp(prop_value, "output") == 0
            || strcasecmp(prop_value, "1") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_OUTPUT;
    } else if (strcasecmp(prop_value, "input_parse") == 0
            || strcasecmp(prop_value, "2") == 0) {
        tmp_type = CC_DUMP_SRC_TYPE_INPUT_PARSE;
    }

    if (tmp_type != aud_src_type) {
        return;
    }

    memset(file_path, '\0', PROPERTY_VALUE_MAX);
    property_get("vendor.media.audiohal.dumpdata.path", file_path, "null");
    if (strcasecmp(file_path, "null") == 0) {
        file_path[0] = '\0';
    }

    if (gDumpDataFd < 0 && file_path[0] != '\0') {
        if (access(file_path, 0) == 0) {
            gDumpDataFd = open(file_path, O_RDWR | O_SYNC);
            if (gDumpDataFd < 0) {
                ALOGE("%s, Open device file \"%s\" error: %s.\n",
                        __FUNCTION__, file_path, strerror(errno));
            }
        } else {
            gDumpDataFd = open(file_path, O_WRONLY | O_CREAT | O_EXCL,
                    S_IRUSR | S_IWUSR);
            if (gDumpDataFd < 0) {
                ALOGE("%s, Create device file \"%s\" error: %s.\n",
                        __FUNCTION__, file_path, strerror(errno));
            }
        }
    }

    if (gDumpDataFd >= 0) {
        write(gDumpDataFd, data_buf, size);
    }
    return;
}

typedef struct aml_dump_debug {
    pthread_t    threadid;
    bool         bexit;
    dump_debug_item_t  *items;

} aml_dump_debug_t;

static aml_dump_debug_t * g_debug_handle = NULL;

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 *this table sequence must match with enum AML_DUMP_DEBUG_INFO
 *!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
 */
dump_debug_item_t aml_debug_items[AML_DEBUG_DUMP_MAX] = {
    {AML_DEBUG_AUDIOHAL_DEBUG_PROPERTY,                 0},    //AML_DEBUG_AUDIOHAL_DEBUG
    {AML_DEBUG_AUDIOHAL_LEVEL_DETECT_PROPERTY,          0},    //AML_DEBUG_AUDIOHAL_LEVEL_DETECT
    {AML_DEBUG_AUDIOHAL_HW_SYNC_PROPERTY,               0},    //AML_DEBUG_AUDIOHAL_HW_SYNC
    {AML_DEBUG_AUDIOHAL_ALSA_PROPERTY,                  0},    //AML_DEBUG_AUDIOHAL_ALSA
    {AML_DUMP_AUDIOHAL_MS12_PROPERTY,                   0},    //AML_DUMP_AUDIOHAL_MS12
    {AML_DUMP_AUDIOHAL_ALSA_PROPERTY,                   0},    //AML_DUMP_AUDIOHAL_ALSA
};

static void aml_debug_update(void)
{
    int i = 0;
    int ret = -1;
    char buf[PROPERTY_VALUE_MAX];
    for (i = 0; i < AML_DEBUG_DUMP_MAX; i++) {
        ret = property_get(aml_debug_items[i].name, buf, NULL);
        if (ret > 0) {
            aml_debug_items[i].value = strtol (buf, NULL, 0);
        }
        ALOGV("%s = 0x%x", aml_debug_items[i].name, aml_debug_items[i].value);
    }
    return;
}

static void *aml_debug_Thread(void *pArg)
{
    aml_dump_debug_t * p_handle = (aml_dump_debug_t *)pArg;

    while (!p_handle->bexit) {
        aml_debug_update();
        usleep(1000 * 1000);
    }
    ALOGI("exit %s", __FUNCTION__);
    return ((void *)0);
}


void aml_audio_debug_open(void)
{
    if (g_debug_handle == NULL) {
        g_debug_handle = calloc(1, sizeof(aml_dump_debug_t));
        if (g_debug_handle) {
            if (pthread_create(&g_debug_handle->threadid, NULL, &aml_debug_Thread, (void *)g_debug_handle)) {
                ALOGE("%s create thread failed", __FUNCTION__);
                return;
            }
            g_debug_handle->bexit = false;
            g_debug_handle->items = aml_debug_items;
        } else {
            ALOGE("%s calloc failed", __FUNCTION__);
            return;
        }
    }
    ALOGI("%s exit", __FUNCTION__);
    return;
}

void aml_audio_debug_close(void)
{
    aml_dump_debug_t * p_handle = g_debug_handle;
    if (p_handle) {
        p_handle->bexit = true;
        if (p_handle->threadid != 0) {
            pthread_join(p_handle->threadid, NULL);
        }
        free(p_handle);
        g_debug_handle = NULL;
    }
    ALOGI("%s exit", __FUNCTION__);
    return;
}

