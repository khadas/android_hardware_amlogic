/*
 * Copyright (C) 2018 Amlogic Corporation.
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
#define LOG_TAG "audio_dtv_utils"
//#define LOG_NDEBUG 0

#include <cutils/atomic.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include <cutils/str_parms.h>
#include <errno.h>
#include <fcntl.h>
#include <hardware/hardware.h>
#include <inttypes.h>
#include <linux/ioctl.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/system_properties.h>
#include <system/audio.h>
#include <time.h>
#include <utils/Timers.h>
#include "aml_malloc_debug.h"
#include "audio_dtv_utils.h"

int dtv_package_list_flush(package_list *list)
{
    pthread_mutex_lock(&(list->tslock));
    struct package * dtv_pacakge = NULL;
    while (list->pack_num && list->first) {
        dtv_pacakge = list->first;
        list->first = list->first->next;
        if (dtv_pacakge->data) {
            aml_audio_free(dtv_pacakge->data);
            dtv_pacakge->data = NULL;
        }
        if (dtv_pacakge->ad_data) {
            aml_audio_free(dtv_pacakge->ad_data);
            dtv_pacakge->ad_data = NULL;
        }
        aml_audio_free(dtv_pacakge);
        dtv_pacakge = NULL;
        list->pack_num--;
    }
    pthread_mutex_unlock(&(list->tslock));
    return 0;
}

int dtv_package_list_init(package_list *list)
{
    list->first = NULL;
    list->pack_num = 0;
    list->current = NULL;
    pthread_mutex_init(&list->tslock, NULL);
    return 0;
}
int dtv_package_add(package_list *list, struct package *p)
{
    pthread_mutex_lock(&list->tslock);
    if (list->pack_num == INPUT_PACKAGE_MAXCOUNT) { //enough
        ALOGI("list->pack_num %d",list->pack_num);
        pthread_mutex_unlock(&(list->tslock));
        return -2;
    }
    if (list->pack_num == 0) { //first package
        list->first = p;
        list->current = p;
        list->pack_num = 1;
    } else {
        list->current->next = p;
        list->current = p;
        list->pack_num++;
    }
    pthread_mutex_unlock(&list->tslock);
    return 0;
}

struct package * dtv_package_get(package_list *list)
{
    pthread_mutex_lock(&(list->tslock));
    if (list->pack_num == 0) {
        pthread_mutex_unlock(&list->tslock);
        return NULL;
    }
    struct package *p = list->first;
    if (list->pack_num == 1) {
        list->first = NULL;
        list->pack_num = 0;
        list->current = NULL;
    } else if (list->pack_num > 1) {
        list->first = list->first->next;
        list->pack_num--;
    }
    pthread_mutex_unlock(&list->tslock);
    return p;
}

void init_cmd_list(struct cmd_node *dtv_cmd_list)
{
    dtv_cmd_list->next = NULL;
    dtv_cmd_list->cmd = -1;
    dtv_cmd_list->cmd_num = 0;
    dtv_cmd_list->used = 0;
    dtv_cmd_list->initd = 1;
    pthread_mutex_init(&dtv_cmd_list->dtv_cmd_mutex, NULL);
}

void deinit_cmd_list(struct cmd_node *dtv_cmd_list)
{
    struct cmd_node *cmd_list = dtv_cmd_list;
    pthread_mutex_destroy(&dtv_cmd_list->dtv_cmd_mutex);
    while (cmd_list != NULL) {
        dtv_cmd_list = dtv_cmd_list->next;
        aml_audio_free(cmd_list);
        cmd_list = dtv_cmd_list;
    }
}

int dtv_patch_add_cmd(struct cmd_node *dtv_cmd_list,int cmd, int path_id)
{
    struct cmd_node *list = NULL;
    struct cmd_node *new_cmd_node = NULL;
    int index = 0;
    if (!dtv_cmd_list || dtv_cmd_list->initd == 0) {
        return 0;
    }
    pthread_mutex_lock(&dtv_cmd_list->dtv_cmd_mutex);
    new_cmd_node = aml_audio_malloc(sizeof(struct cmd_node));
    if (!new_cmd_node ) {
        ALOGE("new_cmd_node aml_audio_malloc failed");
        return -1;
    }
    new_cmd_node->cmd = cmd;
    new_cmd_node->path_id = path_id;
    new_cmd_node->next = NULL;
    new_cmd_node->used = 1;
    list = dtv_cmd_list;
    while (list->next != NULL) {
        list = list->next;
    }
    list->next = new_cmd_node;
    dtv_cmd_list->cmd_num++;
    pthread_mutex_unlock(&dtv_cmd_list->dtv_cmd_mutex);
    ALOGI("add by live dtv_patch_add_cmd the cmd is %d \n", cmd);
    return 0;
}

int dtv_patch_get_cmd(struct cmd_node *dtv_cmd_list,int *cmd, int *path_id)
{
    struct cmd_node *dtv_cmd = NULL;
    ALOGI("enter dtv_patch_get_cmd funciton now\n");
    pthread_mutex_lock(&dtv_cmd_list->dtv_cmd_mutex);
    dtv_cmd = dtv_cmd_list->next;
    if (dtv_cmd != NULL) {
        dtv_cmd_list->next = dtv_cmd->next;
        dtv_cmd_list->cmd_num--;
    } else {
        pthread_mutex_unlock(&dtv_cmd_list->dtv_cmd_mutex);
        return -1;
    }

    dtv_cmd->used = 0;
    *cmd = dtv_cmd->cmd;
    *path_id = dtv_cmd->path_id;
    aml_audio_free(dtv_cmd);
    pthread_mutex_unlock(&dtv_cmd_list->dtv_cmd_mutex);
    ALOGI("leave dtv_patch_get_cmd the cmd is %d  path %d\n", *cmd, *path_id);
    return 0;
}
int dtv_patch_cmd_is_empty(struct cmd_node *dtv_cmd_list)
{
    pthread_mutex_lock(&dtv_cmd_list->dtv_cmd_mutex);
    if (dtv_cmd_list->next == NULL) {
        pthread_mutex_unlock(&dtv_cmd_list->dtv_cmd_mutex);
        return 1;
    }
    pthread_mutex_unlock(&dtv_cmd_list->dtv_cmd_mutex);
    return 0;
}

AD_PACK_STATUS_T check_ad_package_status(int64_t main_pts, int64_t ad_pts, aml_demux_audiopara_t *demux_info)
{
    int timems_diff = llabs(main_pts - ad_pts) / 90;

    if (timems_diff > AD_PACK_STATUS_UNNORMAL_THRESHOLD_MS) {
        ALOGI("timems_diff %d it is impossible so drop ad data ", timems_diff);
        return AD_PACK_STATUS_DROP;
    }
    AD_PACK_STATUS_T ad_status = demux_info->ad_package_status;
    int drop_threshold_ms,drop_start_threshold_ms,hold_start_threshold_ms,hold_threshold_ms;
    bool is_dolby_format = (demux_info->main_fmt == ACODEC_FMT_AC3 ||
                            demux_info->main_fmt == ACODEC_FMT_EAC3||
                            demux_info->main_fmt == ACODEC_FMT_AC4);

    if (is_dolby_format) {
       drop_threshold_ms = AD_PACK_STATUS_DROP_THRESHOLD_MS;
       drop_start_threshold_ms = AD_PACK_STATUS_DROP_START_THRESHOLD_MS;
       hold_threshold_ms = AD_PACK_STATUS_HOLD_THRESHOLD_MS;
       hold_start_threshold_ms = AD_PACK_STATUS_HOLD_START_THRESHOLD_MS;
    } else {
       drop_threshold_ms = NON_DOLBY_AD_PACK_STATUS_DROP_THRESHOLD_MS;
       drop_start_threshold_ms = NON_DOLBY_AD_PACK_STATUS_DROP_START_THRESHOLD_MS;
       hold_threshold_ms = NON_DOLBY_AD_PACK_STATUS_HOLD_THRESHOLD_MS;
       hold_start_threshold_ms = NON_DOLBY_AD_PACK_STATUS_HOLD_START_THRESHOLD_MS;

    }
    switch(ad_status) {
        case AD_PACK_STATUS_NORMAL:
            if (main_pts >= ad_pts) {
                timems_diff = (main_pts - ad_pts) / 90;
                if ( timems_diff > drop_threshold_ms) {
                    ALOGI("main and ad timems_diff %d ms  need drop ", timems_diff);
                    ad_status = AD_PACK_STATUS_DROP;
                } else if (timems_diff < hold_start_threshold_ms) {
                    if (is_dolby_format) {
                        ALOGI("main and ad timems_diff %d ms  need hold ", timems_diff);
                        ad_status = AD_PACK_STATUS_HOLD;
                    }
                }
            } else {
                timems_diff = (ad_pts - main_pts) / 90;
                if (timems_diff > hold_threshold_ms) {
                    ALOGI("ad ahead of main timems_diff %d ", timems_diff);
                    ad_status = AD_PACK_STATUS_HOLD;
                }
            }

            break;
        case AD_PACK_STATUS_DROP:
            if (main_pts > ad_pts) {
                timems_diff = (main_pts - ad_pts) / 90;
                if (timems_diff > drop_start_threshold_ms) {
                    ALOGI("main and ad timems_diff %d ms  need drop ", timems_diff);
                    ad_status = AD_PACK_STATUS_DROP;
                } else {
                    ad_status = AD_PACK_STATUS_NORMAL;
                }

            } else {
                timems_diff = (ad_pts - main_pts) / 90;
                if (timems_diff > hold_threshold_ms) {
                    ad_status = AD_PACK_STATUS_HOLD;
                } else {
                    ad_status = AD_PACK_STATUS_NORMAL;
                }
            }
            break;

        case AD_PACK_STATUS_HOLD:

            if (main_pts < ad_pts) {
                timems_diff = (ad_pts - main_pts) / 90;
                if (timems_diff >= 0) {
                    ALOGI("ad ahead of main timems_diff %d ", timems_diff);
                }
                ad_status = AD_PACK_STATUS_HOLD;
            } else {
                timems_diff = (main_pts - ad_pts) / 90;
                if (timems_diff > hold_start_threshold_ms
                    && timems_diff < hold_threshold_ms) {
                    ad_status = AD_PACK_STATUS_NORMAL;
                } else if (timems_diff >= hold_threshold_ms) {
                    if (is_dolby_format) {
                        ad_status = AD_PACK_STATUS_DROP;
                    } else {
                        ad_status = AD_PACK_STATUS_NORMAL;
                    }
                } else {
                    ad_status = AD_PACK_STATUS_HOLD;
                }
            }
            break;
        default:
            ALOGI("invalid status %d ", ad_status);

    }
    ALOGV("main_pts %lld ad_pts %lld ad_status %d timems_diff %d", main_pts, ad_pts, ad_status, timems_diff);

    return ad_status;
}

