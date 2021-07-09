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
#define MMAP_WRITE_PERIOD_TIME_MS       (MMAP_WRITE_SIZE_FRAME * MSEC_PER_SEC / MMAP_SAMPLE_RATE_HZ)
#define MMAP_WRITE_SIZE_BYTE            (MMAP_WRITE_SIZE_FRAME * MMAP_FRAME_SIZE_BYTE)
#define MMAP_WRITE_PERIOD_TIME_NANO     (MMAP_WRITE_SIZE_FRAME * NSEC_PER_SEC / MMAP_SAMPLE_RATE_HZ)
#define MMAP_BUFFER_SIZE_BYTE           (MMAP_WRITE_SIZE_FRAME * MMAP_FRAME_SIZE_BYTE *  MMAP_BUFFER_BURSTS_NUM)

enum {
    MMAP_INIT,
    MMAP_START,
    MMAP_START_DONE,
    MMAP_STOP,
    MMAP_STOP_DONE
};

static FILE *fp1 = NULL;

static void *outMmapThread(void *pArg) {
    struct aml_stream_out       *out = (struct aml_stream_out *) pArg;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    struct audio_virtual_buf    *pstVirtualBuffer = NULL;
    unsigned char               *pu8CurReadAddr = pstParam->pu8MmapAddr;
    unsigned char               *pu8StartAddr = pstParam->pu8MmapAddr;
    unsigned char               *pu8TempBufferAddr = NULL;
    aml_mmap_thread_param_st    *pstThread = &pstParam->stThreadParam;
    struct timespec timestamp;

    AM_LOGI("enter threadloop bExitThread:%d, bStopPlay:%d, mmap addr:%p, out:%p",
        pstThread->bExitThread, pstThread->bStopPlay, pu8StartAddr, out);
    R_CHECK_POINTER_LEGAL(NULL, pu8StartAddr, "")
    prctl(PR_SET_NAME, (unsigned long)"outMmapThread");
    aml_set_thread_priority("outMmapThread", pstThread->threadId);
    aml_audio_set_cpu23_affinity();

    pu8TempBufferAddr = (unsigned char *)aml_audio_malloc(MMAP_WRITE_SIZE_BYTE);
    while (false == pstThread->bExitThread) {
        if (false == pstThread->bStopPlay) {

            if (pstThread->status == MMAP_START) {
                AM_LOGI("MMAP status: start");
                pu8CurReadAddr = pu8StartAddr;
                pstParam->u32FramePosition = 0;
                clock_gettime(CLOCK_MONOTONIC, &timestamp);
                pstParam->time_nanoseconds = (long long)timestamp.tv_sec * NSEC_PER_SEC + (long long)timestamp.tv_nsec;
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
            pstParam->time_nanoseconds = (long long)timestamp.tv_sec * NSEC_PER_SEC + (long long)timestamp.tv_nsec;

            if (get_debug_value(AML_DEBUG_AUDIOHAL_LEVEL_DETECT)) {
                check_audio_level("aaudio_in", pu8TempBufferAddr, MMAP_WRITE_SIZE_BYTE);
            }

            apply_volume(out->volume_l, pu8TempBufferAddr, 2, MMAP_WRITE_SIZE_BYTE);

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
                AM_LOGI("CurReadAddr:%p, RemainSize:%d, FramePosition:%d offset=%d",
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
    AM_LOGI(" exit threadloop, out:%p", out);
    return NULL;
}

static int outMmapStart(const struct audio_stream_out *stream)
{
    AM_LOGI("stream:%p", stream);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;

    if ((pstParam->stThreadParam.status != MMAP_INIT && pstParam->stThreadParam.status != MMAP_STOP_DONE)
        || pstParam == NULL) {
        AM_LOGW("status:%d error or mmap no init.", pstParam->stThreadParam.status);
        return -ENODATA;
    }

    if (0 == pstParam->stThreadParam.threadId) {
        AM_LOGE("exit threadloop");
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
    AM_LOGI("--stream:%p", stream);
    return 0;
}

static int outMmapStop(const struct audio_stream_out *stream)
{
    AM_LOGI("stream:%p", stream);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;

    if (pstParam->stThreadParam.status != MMAP_START_DONE || pstParam == NULL) {
        AM_LOGW("status:%d not start done or mmap not init", pstParam->stThreadParam.status);
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
    AM_LOGI("--stream:%p", stream);
    return 0;
}

static int outMmapCreateBuffer(const struct audio_stream_out *stream,
                                             int32_t min_size_frames,
                                             struct audio_mmap_buffer_info *info)
{
    AM_LOGI("stream:%p, min_size_frames:%d", stream, min_size_frames);
    struct aml_stream_out       *out = (struct aml_stream_out *) stream;
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    int ret = 0;
    R_CHECK_POINTER_LEGAL(-ENOSYS, pstParam, "");
    R_CHECK_PARAM_LEGAL(ret, min_size_frames, -1, INT_MAX - 1, "");

    info->shared_memory_address = pstParam->pu8MmapAddr;
    info->shared_memory_fd = pstParam->s32IonShareFd;
    info->buffer_size_frames = MMAP_BUFFER_SIZE_BYTE / MMAP_FRAME_SIZE_BYTE;
    info->burst_size_frames  = MMAP_WRITE_SIZE_FRAME;

    aml_mmap_thread_param_st *pstThread = &pstParam->stThreadParam;
    if (pstThread->threadId != 0) {
        AM_LOGW("mmap thread already exist, recreate thread");
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
    R_CHECK_RET(ret, "Create thread fail!");
    AM_LOGI("mmap_fd:%d, mmap address:%p", info->shared_memory_fd, pstParam->pu8MmapAddr);
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
        AM_LOGW("status:%d not start done or position:%d is 0",
            pstParam->stThreadParam.status, position->position_frames);
        return -ENOSYS;
    }
    if (out->dev->debug_flag >= 100) {
        AM_LOGD("stream:%p, position_frames:%d, nano:%lld frame diff=%lu ms time diff=%lld ms", stream,
            position->position_frames, (long long)position->time_nanoseconds,
            (position->position_frames - out->last_mmap_position) * MSEC_PER_SEC / MMAP_SAMPLE_RATE_HZ,
            (position->time_nanoseconds - out->last_mmap_nano_second) / NSEC_PER_MSEC);
    }
    out->last_mmap_position = position->position_frames;
    out->last_mmap_nano_second   = position->time_nanoseconds;

    return 0;
}

int outMmapInit(struct aml_stream_out *out)
{
    AM_LOGI("stream:%p", out);
    int                         ret = 0;
    int                         num_heaps = 0;
    unsigned int                heap_mask = 0;
    aml_mmap_audio_param_st     *pstParam = NULL;

    out->stream.start = outMmapStart;
    out->stream.stop = outMmapStop;
    out->stream.create_mmap_buffer = outMmapCreateBuffer;
    out->stream.get_mmap_position = outMmapGetPosition;

    if (out->pstMmapAudioParam) {
       AM_LOGW("already init, can't again init");
       return 0;
    }
    out->pstMmapAudioParam = (aml_mmap_audio_param_st *)aml_audio_malloc(sizeof(aml_mmap_audio_param_st));
    pstParam = out->pstMmapAudioParam;
    R_CHECK_POINTER_LEGAL(-1, pstParam, "mmap memory malloc fail");
    memset(pstParam, 0, sizeof(aml_mmap_audio_param_st));

    pstParam->s32IonFd = ion_open();
    if (pstParam->s32IonFd < 0) {
       AM_LOGE("ion_open fail! s32IonFd:%d", pstParam->s32IonFd);
       return -1;
    }

    ret = ion_query_heap_cnt(pstParam->s32IonFd, &num_heaps);
    if (ret < 0) {
        AM_LOGE("ion_query_heap_cnt fail! no ion heaps for alloc!!! ret:%#x", ret);
        return -ENOMEM;
    }
    struct ion_heap_data * const heaps = (struct ion_heap_data *) malloc (num_heaps * sizeof(struct ion_heap_data));
    if (num_heaps <= 0 || heaps == NULL) {
        AM_LOGE("heaps is NULL or no heaps, num_heaps:%d", num_heaps);
        return -ENOMEM;
    }
    ret = ion_query_get_heaps(pstParam->s32IonFd, num_heaps, heaps);
    if (ret < 0) {
        AM_LOGE("ion_query_get_heaps fail! no ion heaps for alloc!!! ret:%#x", ret);
        return -ENOMEM;
    }
    for (int i = 0; i != num_heaps; ++i) {
        if (out->dev->debug_flag >= 100)  {
            AM_LOGD("heaps[%d].type:%d, heap_id:%d", i, heaps[i].type, heaps[i].heap_id);
        }
        if ((1 << heaps[i].type) == ION_HEAP_SYSTEM_MASK) {
            heap_mask = 1 << heaps[i].heap_id;
            if (out->dev->debug_flag >= 100)  {
                AM_LOGD("Got it name:%s, type:%#x, 1<<type:%#x, heap_id:%d, heap_mask:%#x",
                    heaps[i].name, heaps[i].type, 1<<heaps[i].type, heaps[i].heap_id, heap_mask);
            }
            break;
        }
    }
    free(heaps);
    if (heap_mask == 0) {
        AM_LOGE("don't find match heap!!!");
        return -ENOMEM;
    }

    ret = ion_alloc_fd(pstParam->s32IonFd, MMAP_BUFFER_SIZE_BYTE, 0, heap_mask, 0, &pstParam->s32IonShareFd);
    if (ret < 0) {
       AM_LOGE("ion_alloc_fd failed, ret:%#x, errno:%d", ret, errno);
       return -ENOMEM;
    }

    pstParam->pu8MmapAddr = mmap(NULL, MMAP_BUFFER_SIZE_BYTE,  PROT_WRITE | PROT_READ,
                                   MAP_SHARED, pstParam->s32IonShareFd, 0);
    AM_LOGI("s32IonFd:%d, s32IonShareFd:%d, pu8MmapAddr:%p",
        pstParam->s32IonFd, pstParam->s32IonShareFd, pstParam->pu8MmapAddr);
    return 0;
}

int outMmapDeInit(struct aml_stream_out *out)
{
    AM_LOGI("stream:%p", out);
    aml_mmap_audio_param_st     *pstParam = (aml_mmap_audio_param_st *)out->pstMmapAudioParam;
    R_CHECK_POINTER_LEGAL(0, pstParam, "uninitialized, can't deinit");

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

