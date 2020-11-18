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



#define LOG_TAG "audio_dtv_sync"
#define LOG_NDEBUG 0
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <cutils/log.h>
#include <string.h>
#include <errno.h>
#include <cutils/properties.h>
#include <sys/ioctl.h>
#include "audio_dtv_sync.h"
#define MAX_AUDIO_SUPPORTED 2

#define HWSYNC_APTS_NUM     512

enum hwsync_status {
    CONTINUATION,  // good sync condition
    ADJUSTMENT,    // can be adjusted by discarding or padding data
    RESYNC,        // pts need resync
};

enum tsync_status {
    TSYNC_STATUS_INIT,
    TSYNC_STATUS_RUNNING,
    TSYNC_STATUS_PAUSED,
    TSYNC_STATUS_STOP
};

typedef struct apts_tab {
    int  valid;
    size_t offset;
    unsigned pts;
} apts_tab_t;

typedef struct  audio_swcheck_sync {
    //uint8_t hw_sync_header[HW_AVSYNC_HEADER_SIZE_V2];
    size_t hw_sync_header_cnt;
    int hw_sync_state;
    uint32_t hw_sync_body_cnt;
    uint32_t hw_sync_frame_size;
    int      bvariable_frame_size;
    uint8_t hw_sync_body_buf[8192];  // 4096
    uint8_t body_align[64];
    uint8_t body_align_cnt;
    bool first_apts_flag;//flag to indicate set first apts
    uint64_t first_apts;
    uint64_t last_apts_from_header;
    apts_tab_t pts_tab[HWSYNC_APTS_NUM];
    pthread_mutex_t lock;
    size_t payload_offset;
    int tsync_fd;
    bool use_tsync_check;
    int version_num;
    bool debug_enable;
} audio_swcheck_sync_t;

audio_swcheck_sync_t  *p_swcheck_table[MAX_AUDIO_SUPPORTED];
int dtv_tsync_ioc_set_first_checkin_apts(int fd, const int val)
{
    ALOGI("dtv_tsync_ioc_set_first_checkin_apts % d",val);
    if (fd >= 0) {
        ioctl(fd, TSYNC_IOC_SET_FIRST_CHECKIN_APTS, &val);
        return 0;
    } else {
        ALOGE("err: %s", strerror(errno));
    }
    return -1;
}

int dtv_tsync_ioc_set_last_checkin_apts(int fd, const int val)
{
    if (fd >= 0) {
        ioctl(fd, TSYNC_IOC_SET_LAST_CHECKIN_APTS, &val);
        return 0;
    } else {
        ALOGE(" err: %s", strerror(errno));
    }
    return -1;
}


void aml_audio_swcheck_init(int audio_path)
{
    //int fd = -1;
    audio_swcheck_sync_t *p_swcheck = NULL;
    p_swcheck = malloc(sizeof (audio_swcheck_sync_t));
    if (p_swcheck == NULL ) {
        ALOGI("p_hwsync malloc failed !");
    }
    p_swcheck->first_apts_flag = true;
    //p_swcheck->hw_sync_state = HW_SYNC_STATE_HEADER;
    p_swcheck->hw_sync_header_cnt = 0;
    p_swcheck->hw_sync_frame_size = 0;
    p_swcheck->bvariable_frame_size = 0;
    p_swcheck->version_num = 0;
    memset(p_swcheck->pts_tab, 0, sizeof(apts_tab_t)*HWSYNC_APTS_NUM);
    pthread_mutex_init(&p_swcheck->lock, NULL);
    p_swcheck->payload_offset = 0;
    p_swcheck->use_tsync_check = property_get_bool("vendor.dtv.use_tsync_check",false);
    p_swcheck->debug_enable = property_get_bool("vendor.dtv.checkapts.debug",false);
    p_swcheck->tsync_fd = open("/dev/tsync", O_CREAT | O_RDWR | O_TRUNC, 0666);
    if (p_swcheck->tsync_fd < 0)
        ALOGI("/dev/tsync open failed !!");
    ALOGI("%s done", __func__);
    p_swcheck_table[audio_path] = p_swcheck;
    return;
}
void aml_audio_swcheck_release(int  audio_path)
{
    audio_swcheck_sync_t *p_swcheck = p_swcheck_table[audio_path];
    if (!p_swcheck) {
        return;
    }
	close(p_swcheck->tsync_fd);
    free(p_swcheck);
    p_swcheck = NULL;
    p_swcheck_table[audio_path] = NULL;
    ALOGI("%s done", __func__);
}

int aml_audio_swcheck_get_firstapts(int  audio_path)
{
    audio_swcheck_sync_t *p_swcheck = p_swcheck_table[audio_path];
    if (!p_swcheck) {
        return -1;
    }
    return p_swcheck->first_apts;
    ALOGI("%s done", __func__);
}
int aml_audio_swcheck_get_lastapts(int  audio_path)
{
    audio_swcheck_sync_t *p_swcheck = p_swcheck_table[audio_path];
    if (!p_swcheck) {
        return -1;
    }
    return p_swcheck->last_apts_from_header;
    ALOGI("%s done", __func__);
}

int aml_audio_swcheck_checkin_apts(int audio_path, size_t offset, unsigned apts)
{
    int i = 0;
    int ret = -1;
    audio_swcheck_sync_t *p_swcheck = p_swcheck_table[audio_path];
    if (/*p_swcheck->use_tsync_check */true) {
        if (p_swcheck->first_apts_flag) {
            ret = dtv_tsync_ioc_set_first_checkin_apts(p_swcheck->tsync_fd, apts);
            p_swcheck->first_apts_flag = false;
        }
        ret = dtv_tsync_ioc_set_last_checkin_apts(p_swcheck->tsync_fd, apts);
        if (ret == -1) {
           ALOGI("unable to open file %s,err: %s", TSYNC_LASTCHECKIN_APTS, strerror(errno));
           return -1;
        }
        if (p_swcheck->debug_enable) {
            ALOGI("ret %d apts %d",ret,apts);
        }
        //return ret;
    }

    apts_tab_t *pts_tab = NULL;
    if (!p_swcheck) {
        ALOGE("%s null point", __func__);
        return -1;
    }
    if (p_swcheck->debug_enable) {
        ALOGI("++ %s checkin ,offset %zu,apts 0x%x", __func__, offset, apts);
    }
    pthread_mutex_lock(&p_swcheck->lock);
    if (offset == 0)
        p_swcheck->first_apts = apts;
	p_swcheck->last_apts_from_header = apts;
    pts_tab = p_swcheck->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (!pts_tab[i].valid) {
            pts_tab[i].pts = apts;
            pts_tab[i].offset = offset;
            pts_tab[i].valid = 1;
            if (p_swcheck->debug_enable) {
                ALOGI("%s checkin done,offset %zu,apts 0x%x", __func__, offset, apts);
            }
            ret = 0;
            break;
        }
    }
    p_swcheck->payload_offset = offset;
    pthread_mutex_unlock(&p_swcheck->lock);
    return ret;
}

int aml_audio_swcheck_lookup_apts(int audio_path, size_t offset, unsigned long *p_apts)
{
    int i = 0;
    size_t align  = 0;
    int ret = -1;
    apts_tab_t *pts_tab = NULL;
    uint32_t nearest_pts = 0;
    uint32_t nearest_offset = 0;
    uint32_t min_offset = 0x7fffffff;
    int match_index = -1;
    audio_swcheck_sync_t *p_swcheck = p_swcheck_table[audio_path];
    // add protection to avoid NULL pointer.
    if (!p_swcheck) {
        ALOGE("%s null point", __func__);
        return -1;
    }

    if (p_swcheck->debug_enable) {
        ALOGI("%s offset %zu,first %d", __func__, offset, p_swcheck->first_apts_flag);
    }
    pthread_mutex_lock(&p_swcheck->lock);

    if (!p_swcheck->bvariable_frame_size) {
        if (p_swcheck->hw_sync_frame_size) {
            align = offset - offset % p_swcheck->hw_sync_frame_size;
        } else {
            align = offset;
        }
    } else {
        align = offset;
    }
    pts_tab = p_swcheck->pts_tab;
    for (i = 0; i < HWSYNC_APTS_NUM; i++) {
        if (pts_tab[i].valid) {
            if (pts_tab[i].offset == align) {
                *p_apts = pts_tab[i].pts;
                nearest_offset = pts_tab[i].offset;
                ret = 0;
                if (p_swcheck->debug_enable) {
                    ALOGI("%s first flag %d,pts checkout done,offset %zu,align %zu,pts 0x%lx",
                          __func__, p_swcheck->first_apts_flag, offset, align, *p_apts);
                }
                break;
            } else if (pts_tab[i].offset < align) {
                /*find the nearest one*/
                if ((align - pts_tab[i].offset) < min_offset) {
                    min_offset = align - pts_tab[i].offset;
                    match_index = i;
                    nearest_pts = pts_tab[i].pts;
                    nearest_offset = pts_tab[i].offset;
                }
                pts_tab[i].valid = 0;

            } else if (/*pts_tab[i].offset > align*/ 0) {
                /*find the nearest one*/
                if ((pts_tab[i].offset - align) < min_offset) {
                    min_offset = pts_tab[i].offset - align;
                    match_index = i;
                    nearest_pts = pts_tab[i].pts;
                    nearest_offset = pts_tab[i].offset;
                }
                //pts_tab[i].valid = 0;

            }
        }
    }
    if (i == HWSYNC_APTS_NUM) {
        if (nearest_pts) {
            ret = 0;
            *p_apts = nearest_pts;
            ALOGI("find nearest pts 0x%lx offset %zu align %zu offset %zu", *p_apts, nearest_offset, align, offset);
        } else {
            ALOGE("%s,apts lookup failed,align %zu,offset %zu", __func__, align, offset);
        }
    }
    if (ret == 0) {
        if (p_swcheck->debug_enable) {
            ALOGI("data offset =%d pts offset =%d diff =%d pts=0x%lx ", offset, nearest_offset, offset - nearest_offset, *p_apts);
        }
    }
    pthread_mutex_unlock(&p_swcheck->lock);
    return ret;
}
