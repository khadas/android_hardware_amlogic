/*
 * Copyright (C) 2019 Amlogic Corporation.
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

#define LOG_TAG "aml_mmap_audio"
#define __USE_GNU

#include <cutils/log.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <inttypes.h>
#include <ion_4.12.h>

#include "audio_hw.h"
#include "audio_hw_ms12.h"
#include "audio_hw_utils.h"
#include "audio_virtual_buf.h"

#include "aml_android_utils.h"
#include "aml_audio_timer.h"
#include "aml_mmap_audio.h"
#include "aml_volume_utils.h"


#define MMAP_FRAME_SIZE_BYTE            (4)
#define MMAP_SAMPLE_RATE_HZ             (48000)
#define MMAP_BUFFER_BURSTS_NUM          (4)

#define MMAP_WRITE_SIZE_FRAME           (384) // 8ms
#define MMAP_WRITE_PERIOD_TIME_MS       (MMAP_WRITE_SIZE_FRAME *  1000LL / MMAP_SAMPLE_RATE_HZ)
#define MMAP_WRITE_SIZE_BYTE            (MMAP_WRITE_SIZE_FRAME * MMAP_FRAME_SIZE_BYTE)
#define MMAP_WRITE_PERIOD_TIME_NANO     (MMAP_WRITE_SIZE_FRAME * 1000000000LL / MMAP_SAMPLE_RATE_HZ)
#define MMAP_BUFFER_SIZE_BYTE           (MMAP_WRITE_SIZE_FRAME * MMAP_FRAME_SIZE_BYTE *  MMAP_BUFFER_BURSTS_NUM)

enum {
    MMAP_INIT,
    MMAP_START,
    MMAP_START_DONE,
    MMAP_STOP,
    MMAP_STOP_DONE
};

static FILE *fp1 = NULL;


static void check_audio_level(const void *buffer, size_t bytes) {
    int num_frame = bytes/4;
    int i = 0;
    short *p = (short *)buffer;
    int silence = 0;
    int silence_cnt = 0;
    int max = 0;
    int min = 0;
    int max_pos = 0;

    min = max = *p;
    for (int i=0; i<num_frame;i++) {
        if (max < *(p+2*i)) {
            max = *(p+2*i);
            max_pos = i;
        }
        if (min > *(p+2*i)) {
            min = *(p+2*i);
        }
        if (*(p+2*i) == 0) {
             silence_cnt ++;
        }
    }
    if (max < 10) {
        silence = 1;
    }
    ALOGI("mmap data detect min=%d max=%d silence=%d silence_cnt=%d pos=%d", min, max, silence, silence_cnt, max_pos);
}

static void *outMmapThread(void *pArg) {
    struct aml_stream_out       *out = (struct aml_stream_out *) pArg;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    struct audio_virtual_buf    *pstVirtualBuffer = NULL;
    unsigned char               *pu8CurReadAddr = pstParam->pu8MmapAddr;
    unsigned char               *pu8StartAddr = pstParam->pu8MmapAddr;
    unsigned char               *pu8TempBufferAddr = NULL;
    aml_mmap_thread_param_st    *pstThread = &pstParam->stThreadParam;
    struct timespec timestamp;
    cpu_set_t cpuSet;

    ALOGI("[%s:%d] enter threadloop bExitThread:%d, bStopPlay:%d, mmap addr:%p, out:%p", __func__, __LINE__,
        pstThread->bExitThread, pstThread->bStopPlay, pu8StartAddr, out);
    if (NULL == pu8StartAddr) {
        ALOGE("[%s:%d] pu8MmapAddr is null", __func__, __LINE__);
        return NULL;
    }
    prctl(PR_SET_NAME, (unsigned long)"outMmapThread");
    aml_set_thread_priority("outMmapThread", pstThread->threadId);

    CPU_ZERO(&cpuSet);
    CPU_SET(2, &cpuSet);
    CPU_SET(3, &cpuSet);
    int status = sched_setaffinity(0, sizeof(cpu_set_t), &cpuSet);
    if (status) {
        ALOGW("%s(), failed to set cpu affinity", __func__);
    }

    pu8TempBufferAddr = (unsigned char *)aml_audio_malloc(MMAP_WRITE_SIZE_BYTE);
    while (false == pstThread->bExitThread) {
        if (false == pstThread->bStopPlay) {

            if (pstThread->status == MMAP_START) {
                ALOGI("[%s:%d] MMAP status: start", __func__, __LINE__);
                pu8CurReadAddr = pu8StartAddr;
                pstParam->u32FramePosition = 0;
                clock_gettime(CLOCK_MONOTONIC, &timestamp);
                pstParam->time_nanoseconds = (long long)timestamp.tv_sec * 1000000000 + (long long)timestamp.tv_nsec;
                pstThread->status = MMAP_START_DONE;
                if (pstVirtualBuffer) {
                    audio_virtual_buf_reset(pstVirtualBuffer);
                    audio_virtual_buf_process((void *)pstVirtualBuffer, MMAP_WRITE_PERIOD_TIME_NANO * MMAP_BUFFER_BURSTS_NUM);
                }
            }

            if (pstVirtualBuffer == NULL) {
                audio_virtual_buf_open((void **)&pstVirtualBuffer, "aaudio mmap",
                        MMAP_WRITE_PERIOD_TIME_NANO * MMAP_BUFFER_BURSTS_NUM, MMAP_WRITE_PERIOD_TIME_NANO * MMAP_BUFFER_BURSTS_NUM, 0);
                audio_virtual_buf_process((void *)pstVirtualBuffer, MMAP_WRITE_PERIOD_TIME_NANO * MMAP_BUFFER_BURSTS_NUM);
            }
            unsigned int u32RemainSizeByte =  (MMAP_BUFFER_SIZE_BYTE + pu8StartAddr) - pu8CurReadAddr;
            if (u32RemainSizeByte >= MMAP_WRITE_SIZE_BYTE) {

                memcpy(pu8TempBufferAddr, pu8CurReadAddr, MMAP_WRITE_SIZE_BYTE);
                memset(pu8CurReadAddr, 0, MMAP_WRITE_SIZE_BYTE);
                pu8CurReadAddr += MMAP_WRITE_SIZE_BYTE;
            } else {
                memcpy(pu8TempBufferAddr, pu8CurReadAddr, u32RemainSizeByte);
                memset(pu8CurReadAddr, 0, u32RemainSizeByte);

                memcpy(pu8TempBufferAddr + u32RemainSizeByte, pu8StartAddr, MMAP_WRITE_SIZE_BYTE - u32RemainSizeByte);
                memset(pu8StartAddr, 0, MMAP_WRITE_SIZE_BYTE - u32RemainSizeByte);
                pu8CurReadAddr = pu8StartAddr + MMAP_WRITE_SIZE_BYTE - u32RemainSizeByte;
            }
            pstParam->u32FramePosition += MMAP_WRITE_SIZE_FRAME;
            // Absolutet time must be used when get timestamp.
            clock_gettime(CLOCK_MONOTONIC, &timestamp);
            pstParam->time_nanoseconds = (long long)timestamp.tv_sec * 1000000000 + (long long)timestamp.tv_nsec;

            apply_volume(out->volume_l, pu8TempBufferAddr, 2, MMAP_WRITE_SIZE_BYTE);
            //check_audio_level(pu8TempBufferAddr, MMAP_WRITE_SIZE_BYTE);
            if (out->dev->useSubMix) {
                out->stream.write(&out->stream, pu8TempBufferAddr, MMAP_WRITE_SIZE_BYTE);
            } else {
                out_write_new(&out->stream, pu8TempBufferAddr, MMAP_WRITE_SIZE_BYTE);
            }
            if (aml_getprop_bool("vendor.media.audiohal.outdump")) {
                if (fp1) {
                    fwrite(pu8TempBufferAddr, 1, MMAP_WRITE_SIZE_BYTE, fp1);
                }
            }
            audio_virtual_buf_process((void *)pstVirtualBuffer, MMAP_WRITE_PERIOD_TIME_NANO);
            if (out->dev->debug_flag >= 100) {
                ALOGI("[%s:%d] CurReadAddr:%p, RemainSize:%d, FramePosition:%d offset=%d", __func__, __LINE__,
                    pu8CurReadAddr, u32RemainSizeByte, pstParam->u32FramePosition, pstParam->u32FramePosition%(MMAP_BUFFER_SIZE_BYTE / MMAP_FRAME_SIZE_BYTE));
            }
        } else {
            struct timespec tv;
            clock_gettime(CLOCK_MONOTONIC, &tv);
            // The suspend time set to 30 sec, reduce cpu power consumption.
            // And waitting time can be awakened by out_start func.
            tv.tv_sec += 30;
            pthread_mutex_lock(&pstThread->mutex);
            pthread_cond_timedwait(&pstThread->cond, &pstThread->mutex, &tv);
            pthread_mutex_unlock(&pstThread->mutex);
        }
    }

    if (pstVirtualBuffer != NULL) {
        audio_virtual_buf_close((void **)&pstVirtualBuffer);
    }
    aml_audio_free(pu8TempBufferAddr);
    pu8TempBufferAddr = NULL;
    ALOGI("[%s:%d]  exit threadloop, out:%p", __func__, __LINE__, out);
    return NULL;
}

static int outMmapStart(const struct audio_stream_out *stream)
{
    ALOGI("[%s:%d] stream:%p", __func__, __LINE__, stream);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;

    if ((pstParam->stThreadParam.status != MMAP_INIT && pstParam->stThreadParam.status != MMAP_STOP_DONE)
        || pstParam == NULL) {
        ALOGW("[%s:%d] status:%d error or mmap no init.", __func__, __LINE__, pstParam->stThreadParam.status);
        return -ENODATA;
    }

    if (0 == pstParam->stThreadParam.threadId) {
        ALOGE("[%s:%d] exit threadloop", __func__, __LINE__);
        return -ENOSYS;
    }
    if (aml_getprop_bool("vendor.media.audiohal.outdump")) {
        fp1 = fopen("/data/audio/pcm_mmap", "a+");
    }
    pstParam->u32FramePosition = 0;
    pstParam->stThreadParam.bStopPlay = false;
    pstParam->stThreadParam.status = MMAP_START;
    //dolby_ms12_app_flush();
    pthread_mutex_lock(&pstParam->stThreadParam.mutex);
    pthread_cond_signal(&pstParam->stThreadParam.cond);
    pthread_mutex_unlock(&pstParam->stThreadParam.mutex);
    ALOGI("--[%s:%d] stream:%p", __func__, __LINE__, stream);
    return 0;
}

static int outMmapStop(const struct audio_stream_out *stream)
{
    ALOGI("[%s:%d] stream:%p", __func__, __LINE__, stream);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;

    if (pstParam->stThreadParam.status != MMAP_START_DONE || pstParam == NULL) {
        ALOGW("[%s:%d] status:%d not start done or mmap not init", __func__, __LINE__, pstParam->stThreadParam.status);
        return -ENODATA;
    }

    //dolby_ms12_app_flush();
    if (aml_getprop_bool("vendor.media.audiohal.outdump")) {
        if (fp1) {
            fclose(fp1);
            fp1 = NULL;
        }
    }
    // suspend threadloop.
    pstParam->stThreadParam.status = MMAP_STOP;
    /*sleep some time, to make sure the read thread read all the data*/
    aml_audio_sleep(8 * 1000);
    pstParam->stThreadParam.bStopPlay = true;
    pstParam->stThreadParam.status = MMAP_STOP_DONE;
    memset(pstParam->pu8MmapAddr, 0, MMAP_BUFFER_SIZE_BYTE);
    pstParam->u32FramePosition = 0;
    ALOGI("[--%s:%d] stream:%p", __func__, __LINE__, stream);
    return 0;
}

static int outMmapCreateBuffer(const struct audio_stream_out *stream,
                                             int32_t min_size_frames,
                                             struct audio_mmap_buffer_info *info)
{
    ALOGI("[%s:%d], stream:%p, min_size_frames:%d", __func__, __LINE__, stream, min_size_frames);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    int ret = 0;

    if (NULL == pstParam || min_size_frames >= INT_MAX || min_size_frames <= 0) {
        ALOGW("[%s:%d] pstParam is null or min_size_frames invalid. ", __func__, __LINE__);
        return -ENOSYS;
    }

    info->shared_memory_address = pstParam->pu8MmapAddr;
    info->shared_memory_fd = pstParam->s32IonShareFd;
    info->buffer_size_frames = MMAP_BUFFER_SIZE_BYTE / MMAP_FRAME_SIZE_BYTE;
    info->burst_size_frames  = MMAP_WRITE_SIZE_FRAME;

    aml_mmap_thread_param_st *pstThread = &pstParam->stThreadParam;
    if (pstThread->threadId != 0) {
        ALOGW("[%s:%d] mmap thread already exist, recreate thread", __func__, __LINE__);
        pstThread->bExitThread = true;
        pstThread->bStopPlay = true;
        pthread_mutex_lock(&pstThread->mutex);
        pthread_cond_signal(&pstThread->cond);
        pthread_mutex_unlock(&pstThread->mutex);
        pthread_join(pstThread->threadId, NULL);
        memset(pstThread, 0, sizeof(aml_mmap_thread_param_st));
    } else {
        pthread_mutex_init (&pstThread->mutex, NULL);
    }
    pthread_condattr_init(&pstThread->condAttr);
    pthread_condattr_setclock(&pstThread->condAttr, CLOCK_MONOTONIC);
    pthread_cond_init(&pstThread->cond, &pstThread->condAttr);
    pstThread->bExitThread = false;
    pstThread->bStopPlay = true;
    pstThread->status = MMAP_INIT;
    ret = pthread_create(&pstThread->threadId, NULL, &outMmapThread, out);
    if (ret != 0) {
        ALOGE("[%s:%d], Create thread fail!", __func__, __LINE__);
        return -1;
    }
    ALOGI("[%s:%d], mmap_fd:%d, mmap address:%p", __func__, __LINE__, info->shared_memory_fd, pstParam->pu8MmapAddr);
    return 0;
}

static int outMmapGetPosition(const struct audio_stream_out *stream,
                                           struct audio_mmap_position *position)
{
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;

    position->time_nanoseconds = pstParam->time_nanoseconds;
    position->position_frames = pstParam->u32FramePosition;

    if (position->position_frames == 0 || pstParam->stThreadParam.status != MMAP_START_DONE) {
        ALOGW("[%s:%d] status:%d not start done or position:%d is 0", __func__, __LINE__,
            pstParam->stThreadParam.status, position->position_frames);
        return -ENOSYS;
    }

    if (out->dev->debug_flag >= 100) {
        ALOGD("[%s:%d] stream:%p, position_frames:%d, nano:%lld frame diff=%d time diff=%" PRId64 "",
            __func__, __LINE__, stream,
            position->position_frames, (long long)position->time_nanoseconds,
            (position->position_frames - out->last_mmap_position ) * 1000/48,
            (position->time_nanoseconds - out->last_mmap_nano_second)/1000);
    }
    out->last_mmap_position = position->position_frames;
    out->last_mmap_nano_second   = position->time_nanoseconds;

    return 0;
}

int outMmapInit(struct aml_stream_out *out)
{
    ALOGI("[%s:%d] stream:%p", __func__, __LINE__, out);
    int                         ret = 0;
    int                         num_heaps = 0;
    unsigned int                heap_mask = 0;
    aml_mmap_audio_param_st     *pstParam = NULL;

    out->stream.start = outMmapStart;
    out->stream.stop = outMmapStop;
    out->stream.create_mmap_buffer = outMmapCreateBuffer;
    out->stream.get_mmap_position = outMmapGetPosition;

    if (out->pstMmapAudioParam) {
       ALOGW("[%s:%d] already init, can't again init", __func__, __LINE__);
       return 0;
    }
    out->pstMmapAudioParam = (aml_mmap_audio_param_st *)aml_audio_malloc(sizeof(aml_mmap_audio_param_st));
    pstParam = out->pstMmapAudioParam;
    if (pstParam == NULL) {
       ALOGW("[%s:%d] mmap audio param memory malloc fail", __func__, __LINE__);
       return -1;
    }
    memset(pstParam, 0, sizeof(aml_mmap_audio_param_st));

    pstParam->s32IonFd = ion_open();
    if (pstParam->s32IonFd < 0) {
       ALOGE("[%s:%d] ion_open fail! s32IonFd:%d", __func__, __LINE__, pstParam->s32IonFd);
       return -1;
    }

    ret = ion_query_heap_cnt(pstParam->s32IonFd, &num_heaps);
    if (ret < 0) {
        ALOGE("[%s:%d] ion_query_heap_cnt fail! no ion heaps for alloc!!! ret:%#x", __func__, __LINE__, ret);
        return -ENOMEM;
    }
    struct ion_heap_data * const heaps = (struct ion_heap_data *) malloc (num_heaps * sizeof(struct ion_heap_data));
    if (num_heaps <= 0 || heaps == NULL) {
        ALOGE("[%s:%d] heaps is NULL or no heaps, num_heaps:%d", __func__, __LINE__, num_heaps);
        return -ENOMEM;
    }
    ret = ion_query_get_heaps(pstParam->s32IonFd, num_heaps, heaps);
    if (ret < 0) {
        ALOGE("[%s:%d] ion_query_get_heaps fail! no ion heaps for alloc!!! ret:%#x", __func__, __LINE__, ret);
        return -ENOMEM;
    }
    for (int i = 0; i != num_heaps; ++i) {
        if (out->dev->debug_flag >= 100)  {
            ALOGD("[%s:%d] heaps[%d].type:%d, heap_id:%d", __func__, __LINE__, i, heaps[i].type, heaps[i].heap_id);
        }
        if ((1 << heaps[i].type) == ION_HEAP_SYSTEM_MASK) {
            heap_mask = 1 << heaps[i].heap_id;
            if (out->dev->debug_flag >= 100)  {
                ALOGD("[%s:%d] Got it name:%s, type:%#x, 1<<type:%#x, heap_id:%d, heap_mask:%#x", __func__, __LINE__,
                    heaps[i].name, heaps[i].type, 1<<heaps[i].type, heaps[i].heap_id, heap_mask);
            }
            break;
        }
    }
    free(heaps);
    if (heap_mask == 0) {
        ALOGE("[%s:%d] don't find match heap!!!", __func__, __LINE__);
        return -ENOMEM;
    }

    ret = ion_alloc_fd(pstParam->s32IonFd, MMAP_BUFFER_SIZE_BYTE, 0, heap_mask, 0, &pstParam->s32IonShareFd);
    if (ret < 0) {
       ALOGE("[%s:%d] ion_alloc_fd failed, ret:%#x, errno:%d", __func__, __LINE__, ret, errno);
       return -ENOMEM;
    }

    pstParam->pu8MmapAddr = mmap(NULL, MMAP_BUFFER_SIZE_BYTE,  PROT_WRITE | PROT_READ,
                                   MAP_SHARED, pstParam->s32IonShareFd, 0);
    ALOGI("[%s:%d] s32IonFd:%d, s32IonShareFd:%d, pu8MmapAddr:%p", __func__, __LINE__,
        pstParam->s32IonFd, pstParam->s32IonShareFd, pstParam->pu8MmapAddr);
    return 0;
}

int outMmapDeInit(struct aml_stream_out *out)
{
    ALOGI("[%s:%d] stream:%p", __func__, __LINE__, out);
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    if (NULL == pstParam) {
       ALOGW("[%s:%d]  uninitialized, can't deinit", __func__, __LINE__);
       return 0;
    }

    pstParam->stThreadParam.bExitThread = true;
    pthread_mutex_lock(&pstParam->stThreadParam.mutex);
    pthread_cond_signal(&pstParam->stThreadParam.cond);
    pthread_mutex_unlock(&pstParam->stThreadParam.mutex);
    if (pstParam->stThreadParam.threadId != 0) {
       pthread_join(pstParam->stThreadParam.threadId, NULL);
    }

    munmap(pstParam->pu8MmapAddr, MMAP_BUFFER_SIZE_BYTE);
    close(pstParam->s32IonShareFd);
    ion_close(pstParam->s32IonFd);
    aml_audio_free(pstParam);
    out->pstMmapAudioParam = NULL;
    return 0;
}

