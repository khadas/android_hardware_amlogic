/*
 * Copyright (C) 2021 Amlogic Corporation.
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
#define __USE_GNU

#include <cutils/log.h>
#include <dolby_ms12.h>
#include <dolby_ms12_config_params.h>
#include <dolby_ms12_status.h>
#include <aml_android_utils.h>
#include <sys/prctl.h>
#include <cutils/properties.h>
#include <inttypes.h>

#include "audio_hw_ms12.h"
#include "audio_hw_ms12_common.h"
#include "alsa_config_parameters.h"
#include "aml_ac3_parser.h"
#include <sound/asound.h>
#include <tinyalsa/asoundlib.h>
#include "audio_hw.h"
#include "alsa_manager.h"
#include "aml_audio_stream.h"
#include "dolby_lib_api.h"
#include "aml_audio_timer.h"
#include "audio_virtual_buf.h"
#include "ac3_parser_utils.h"
#include "audio_hw_utils.h"
#include "alsa_device_parser.h"
#include "spdif_encoder_api.h"
#include "aml_audio_spdifout.h"
#include "aml_audio_ac3parser.h"
#include "aml_audio_spdifdec.h"
#include "aml_audio_ms12_bypass.h"
#include "aml_malloc_debug.h"

/*
 *@brief
 *Convert from ms12 pointer to aml_auido_device pointer.
 */
#define ms12_to_adev(ms12_ptr)  (struct aml_audio_device *) (((char*) (ms12_ptr)) - offsetof(struct aml_audio_device, ms12))


/*
 *@brief
 *transfer mesg type to string,
 *for more easy to read log.
 */
const char *mesg_type_2_string[MS12_MESG_TYPE_MAX] = {
    "MS12_MESG_TYPE_NONE",
    "MS12_MESG_TYPE_FLUSH",
    "MS12_MESG_TYPE_PAUSE",
    "MS12_MESG_TYPE_RESUME",
    "MS12_MESG_TYPE_SET_MAIN_DUMMY",
    "MS12_MESG_TYPE_UPDATE_RUNTIIME_PARAMS",
    "MS12_MESG_TYPE_EXIT_THREAD",
};


/*****************************************************************************
*   Function Name:  set_dolby_ms12_runtime_pause
*   Description:    set pause or resume to dobly ms12.
*   Parameters:     struct dolby_ms12_desc: ms12 variable pointer
*                   int: state pause or resume
*   Return value:   0: success, or else fail
******************************************************************************/
int set_dolby_ms12_runtime_pause(struct dolby_ms12_desc *ms12, int is_pause)
{
    char parm[12] = "";
    int ret = -1;

    sprintf(parm, "%s %d", "-pause", is_pause);
    if ((strlen(parm) > 0) && ms12) {
        ret = aml_ms12_update_runtime_params(ms12, parm);
    } else {
        ALOGE("%s ms12 is NULL or strlen(parm) is zero", __func__);
        ret = -1;
    }
    return ret;
}

int dolby_ms12_main_pause(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ms12_runtime_update_ret = 0;

    dolby_ms12_set_pause_flag(true);
    //ms12_runtime_update_ret = aml_ms12_update_runtime_params(ms12);
    ms12_runtime_update_ret = set_dolby_ms12_runtime_pause(ms12, true);
    ms12->is_continuous_paused = true;
    ALOGI("%s  ms12_runtime_update_ret:%d", __func__, ms12_runtime_update_ret);

    //1.audio easing duration is 32ms,
    //2.one loop for schedule_run cost about 32ms(contains the hardware costing),
    //3.if [pause, flush] too short, means it need more time to do audio easing
    //so, the delay time for 32ms(pause is completed after audio easing is done) is enough.
    aml_audio_sleep(64000);

    if (aml_out->hw_sync_mode && aml_out->tsync_status != TSYNC_STATUS_PAUSED) {
        //ALOGI(" %s  delay 150ms", __func__);
        //usleep(150 * 1000);
        aml_hwsync_set_tsync_pause(aml_out->hwsync);
        aml_out->tsync_status = TSYNC_STATUS_PAUSED;
    }

    ALOGI("%s  sleep 64ms finished and exit", __func__);
    return 0;
}

int dolby_ms12_main_resume(struct audio_stream_out *stream)
{
    struct aml_stream_out *aml_out = (struct aml_stream_out *)stream;
    struct aml_audio_device *adev = aml_out->dev;
    struct dolby_ms12_desc *ms12 = &(adev->ms12);
    int ms12_runtime_update_ret = 0;

    dolby_ms12_set_pause_flag(false);
    //ms12_runtime_update_ret = aml_ms12_update_runtime_params(ms12);
    ms12_runtime_update_ret = set_dolby_ms12_runtime_pause(ms12, false);
    ms12->is_continuous_paused = false;
    ALOGI("%s  ms12_runtime_update_ret:%d", __func__, ms12_runtime_update_ret);

    return 0;
}

/*****************************************************************************
*   Function Name:  ms12_msg_list_is_empty
*   Description:    check whether the msg list is empty
*   Parameters:     struct dolby_ms12_desc: ms12 variable pointer
*   Return value:   0: not empty, 1 empty
******************************************************************************/
bool ms12_msg_list_is_empty(struct dolby_ms12_desc *ms12)
{
    bool is_emtpy = true;
    if (0 != ms12->ms12_mesg_threadID) {
        pthread_mutex_lock(&ms12->mutex);
        if (!list_empty(&ms12->mesg_list)) {
            is_emtpy = false;
        }
        pthread_mutex_unlock(&ms12->mutex);
    }
    return is_emtpy;
}

/*****************************************************************************
*   Function Name:  audiohal_send_msg_2_ms12
*   Description:    receive message from audio hardware.
*   Parameters:     struct dolby_ms12_desc: ms12 variable pointer
*                   ms12_mesg_type_t: message type
*   Return value:   0: success, or else fail
******************************************************************************/
int audiohal_send_msg_2_ms12(struct dolby_ms12_desc *ms12, ms12_mesg_type_t mesg_type)
{
    int ret = -1;
    if (0 != ms12->ms12_mesg_threadID) {
        struct ms12_mesg_desc *mesg_p = calloc(1, sizeof(struct ms12_mesg_desc));

        if (NULL == mesg_p) {
            ALOGE("%s calloc fail, errno:%s", __func__, strerror(errno));
            return -ENOMEM;
        }

        ALOGI("%s mesg_type:%s entry", __func__, mesg_type_2_string[mesg_type]);
        pthread_mutex_lock(&ms12->mutex);
        mesg_p->mesg_type = mesg_type;
        ALOGV("%s add mesg item", __func__);
        list_add_tail(&ms12->mesg_list, &mesg_p->list);
        pthread_mutex_unlock(&ms12->mutex);

        pthread_cond_signal(&ms12->cond);
        ALOGI("%s mesg_type:%s exit", __func__, mesg_type_2_string[mesg_type]);
        ret = 0;
    } else {
        ALOGE("%s ms12_mesg_threadID is 0, exit directly as ms12_message_thread had some issues!", __func__);
        ret = -1;
    }
    return ret;
}

/*****************************************************************************
*   Function Name:  ms12_message_threadloop
*   Description:    ms12 receive message thread,
*                   it always loop until receive exit message.
*   Parameters:     void *data
*   Return value:   NULL pointer
******************************************************************************/
static void *ms12_message_threadloop(void *data)
{
    struct dolby_ms12_desc *ms12 = (struct dolby_ms12_desc *)data;
    struct aml_audio_device *adev = NULL;

    ALOGI("%s entry.", __func__);
    if (ms12 == NULL) {
        ALOGE("%s ms12 pointer invalid!", __func__);
        goto Error;
    }

    adev = ms12_to_adev(ms12);
    prctl(PR_SET_NAME, (unsigned long)"MS12_CommThread");
    aml_set_thread_priority("ms12_message_thread", pthread_self());

    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(2, &cpuSet);
    CPU_SET(3, &cpuSet);
    int sastat = sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet);
    if (sastat) {
        ALOGW("%s(), failed to set cpu affinity", __func__);
    }

    do {
        struct ms12_mesg_desc *mesg_p = NULL;
        struct listnode *item = NULL;

        pthread_mutex_lock(&ms12->mutex);
        if (list_empty(&ms12->mesg_list)) {
            ALOGD("%s  mesg_list is empty, loop waiting......", __func__);
            pthread_cond_wait(&ms12->cond, &ms12->mutex);
        }

Repop_Mesg:
        if (list_empty(&ms12->mesg_list)) {
            ALOGD("%s list is empty", __func__);
            pthread_mutex_unlock(&ms12->mutex);
            continue;
        }
        item = list_head(&ms12->mesg_list);
        mesg_p = (struct ms12_mesg_desc *)item;
        if (mesg_p->mesg_type > MS12_MESG_TYPE_MAX) {
            ALOGE("%s wrong message type =%d", __func__, mesg_p->mesg_type);
            mesg_p->mesg_type = MS12_MESG_TYPE_NONE;
        }
        ALOGD("%s(), msg type: %s", __func__, mesg_type_2_string[mesg_p->mesg_type]);
        pthread_mutex_unlock(&ms12->mutex);

        while (NULL == ms12->ms12_main_stream_out && true != ms12->CommThread_ExitFlag) {
            ALOGV("%s  ms12_out:%p, waiting ==> ms12_main_stream_out:%p", __func__,adev->ms12_out,ms12->ms12_main_stream_out);
            aml_audio_sleep(5000); //sleep 5ms
        };
        ALOGV("%s  ms12_out:%p, ==> ms12_main_stream_out:%p", __func__,adev->ms12_out,ms12->ms12_main_stream_out);
        switch (mesg_p->mesg_type) {
            case MS12_MESG_TYPE_FLUSH:
                dolby_ms12_main_flush(&ms12->ms12_main_stream_out->stream);//&adev->ms12_out->stream
                break;
            case MS12_MESG_TYPE_PAUSE:
                dolby_ms12_main_pause(&ms12->ms12_main_stream_out->stream);
                break;
            case MS12_MESG_TYPE_RESUME:
                dolby_ms12_main_resume(&ms12->ms12_main_stream_out->stream);
                break;
            case MS12_MESG_TYPE_SET_MAIN_DUMMY:
                break;
            case MS12_MESG_TYPE_UPDATE_RUNTIIME_PARAMS:
                break;
            case MS12_MESG_TYPE_EXIT_THREAD:
                ALOGD("%s mesg exit thread.", __func__);
                break;
            default:
                ALOGD("%s  msg type not support.", __func__);
        }

        pthread_mutex_lock(&ms12->mutex);
        list_remove(&mesg_p->list);
        free(mesg_p);
        if (!list_empty(&ms12->mesg_list)) {
            ALOGD("%s  list no empty and Repop_Mesg again.", __func__);
            goto Repop_Mesg;
        }
        pthread_mutex_unlock(&ms12->mutex);

    } while(true != ms12->CommThread_ExitFlag);

Error:
    ALOGI("%s  exit.", __func__);
    return ((void *)0);
}

/*****************************************************************************
*   Function Name:  ms12_mesg_thread_create
*   Description:    ms12 message thread create, so need to do create work.
*   Parameters:     struct dolby_ms12_desc: ms12 variable pointer
*   Return value:   0: success, or fail
******************************************************************************/
int ms12_mesg_thread_create(struct dolby_ms12_desc *ms12)
{
    int ret = 0;

    list_init(&ms12->mesg_list);
    ms12->CommThread_ExitFlag = false;
    if ((ret = pthread_mutex_init (&ms12->mutex, NULL)) != 0) {
        ALOGE("%s  pthread_mutex_init fail, errono:%s", __func__, strerror(errno));
    } else if((ret = pthread_cond_init(&ms12->cond, NULL)) != 0) {
        ALOGE("%s  pthread_cond_init fail, errono:%s", __func__, strerror(errno));
    } else if((ret = pthread_create(&(ms12->ms12_mesg_threadID), NULL, &ms12_message_threadloop, (void *)ms12)) != 0) {
        ALOGE("%s  pthread_create fail, errono:%s", __func__, strerror(errno));
    } else {
        ALOGD("%s ms12 thread init & create successful, ms12_mesg_threadID:%#lx ret:%d", __func__, ms12->ms12_mesg_threadID, ret);
    }

    return ret;
}

/*****************************************************************************
*   Function Name:  ms12_mesg_thread_destroy
*   Description:    ms12 message thread exit, so need to do release work.
*   Parameters:     struct dolby_ms12_desc: ms12 variable pointer
*   Return value:   0: success
******************************************************************************/
int ms12_mesg_thread_destroy(struct dolby_ms12_desc *ms12)
{
    int ret = 0;

    ALOGD("%s entry, ms12_mesg_threadID:%#lx", __func__, ms12->ms12_mesg_threadID);
    if (ms12->ms12_mesg_threadID != 0) {
        if (!list_empty(&ms12->mesg_list)) {
            struct ms12_mesg_desc *mesg_p = NULL;
            struct listnode *item = NULL;

            list_for_each(item, &ms12->mesg_list){
                item = list_head(&ms12->mesg_list);
                mesg_p = (struct ms12_mesg_desc *)item;

               list_remove(&mesg_p->list);
               free(mesg_p);
            }
        }

        ms12->CommThread_ExitFlag = true;
        ret = audiohal_send_msg_2_ms12(ms12, MS12_MESG_TYPE_EXIT_THREAD);

        pthread_join(ms12->ms12_mesg_threadID, NULL);
        ms12->ms12_mesg_threadID = 0;

        /* destroy cond & mutex*/
        pthread_cond_destroy(&ms12->cond);
        pthread_mutex_destroy(&ms12->mutex);
        ALOGD("%s() ms12_mesg_threadID reset to %ld\n", __FUNCTION__, ms12->ms12_mesg_threadID);
    }

    return ret;
}
