/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#define LOG_TAG "Camportal_ION Interface"

#include "IonIf.h"
#include <linux/ion.h>
#include <ion/ion.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include "ion_4.12.h"
namespace android {
IONInterface* IONInterface::mIONInstance = nullptr;
int IONInterface::mIONDevice_fd = -1;
Mutex IONInterface::IonLock;
int IONInterface::mCount = 0;
IONBufferNode IONInterface::mPicBuffers[MAX_BUFFER_NUM];
IONInterface::IONInterface() {
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        mPicBuffers[i].vaddr = nullptr;
        mPicBuffers[i].ion_handle = -1;
        mPicBuffers[i].share_fd = -1;
        mPicBuffers[i].size = 0;
        mPicBuffers[i].IsUsed = false;
    }
    ALOGD("%s construct\n", __FUNCTION__);
}

IONInterface::~IONInterface() {
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mPicBuffers[i].IsUsed)
            release_node(&mPicBuffers[i]);
    }
    if (mIONDevice_fd >= 0) {
        ion_close(mIONDevice_fd);
        mIONDevice_fd = -1;
    }
}

IONInterface* IONInterface::get_instance() {
    ALOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&IonLock);
    mCount++;
    if (mIONInstance != nullptr)
        return mIONInstance;

    mIONDevice_fd = ion_open();
    if (mIONDevice_fd < 0) {
        ALOGE("ion_open failed, %s", strerror(errno));
        mIONDevice_fd = -1;
    }
    ALOGD("%s: create new ion object \n", __FUNCTION__);
    mIONInstance = new IONInterface;
    return mIONInstance;
}

void IONInterface::put_instance() {
    ALOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&IonLock);
    mCount = mCount - 1;
    if (!mCount && mIONInstance != nullptr) {
        ion_close(mIONDevice_fd);
        mIONDevice_fd = -1;
        ALOGD("%s delete ION Instance \n", __FUNCTION__);
        delete mIONInstance;
        mIONInstance=nullptr;
    }
}


int IONInterface::__alloc_buffer(int ion_fd, size_t size,
                        int* pShareFd, unsigned int flag, unsigned int alloc_hmask) {
    int ret = -1;
    int num_heaps = 0;
    unsigned int heap_mask = 0;

    if (ion_query_heap_cnt(ion_fd, &num_heaps) >= 0) {
        ALOGD("num_heaps=%d\n", num_heaps);
        struct ion_heap_data *const heaps =
                (struct ion_heap_data *)malloc(num_heaps * sizeof(struct ion_heap_data));
        if (heaps != NULL && num_heaps) {
            if (ion_query_get_heaps(ion_fd, num_heaps, heaps) >= 0) {
                for (int i = 0; i != num_heaps; ++i) {
                    ALOGD("heaps[%d].type=%d, heap_id=%d\n", i, heaps[i].type, heaps[i].heap_id);
                    if ((1 << heaps[i].type) == alloc_hmask && 0 == strcmp(heaps[i].name, "ion-dev")) {
                        heap_mask = 1 << heaps[i].heap_id;
                        ALOGD("%d, m=%x, 1<<heap_id=%x, heap_mask=%x, name=%s, alloc_hmask=%x\n",
                                heaps[i].type, 1<<heaps[i].type,
                                heaps[i].heap_id, heap_mask,
                                heaps[i].name, alloc_hmask);
                        break;
                    }
                }
            }
            free(heaps);
            if (heap_mask)
                ret = ion_alloc_fd(ion_fd, size, 0, heap_mask, flag, pShareFd);
            else
                ALOGE("don't find match heap!!\n");
        } else {
            ALOGE("heaps is NULL or no heaps,num_heaps=%d\n", num_heaps);
        }
    } else {
        ALOGE("query_heap_cnt fail! no ion heaps for alloc!!!\n");
    }
    if (ret < 0) {
        ALOGE("ion_alloc failed, %s\n", strerror(errno));
        return -ENOMEM;
    }
    return ret;
}


uint8_t* IONInterface::alloc_buffer(size_t size,int* share_fd) {
        ALOGD("%s\n", __FUNCTION__);
        IONBufferNode* pBuffer = nullptr;
        int i =0;
         if (mIONDevice_fd < 0) {
            ALOGE("ion dose not init");
            return nullptr;
        }
        Mutex::Autolock lock(&IonLock);
        for (i = 0; i < MAX_BUFFER_NUM; i++) {
            if (mPicBuffers[i].IsUsed == false) {
                mPicBuffers[i].IsUsed = true;
                pBuffer = &mPicBuffers[i];
                ALOGD("---------------%s:size= %d,buffer idx = %d\n", __FUNCTION__,size,i);
                break;
            }
        }
        if (i == MAX_BUFFER_NUM) {
            ALOGE("%s: alloc fail",__FUNCTION__);
            return nullptr;
        }
        pBuffer->size = size;

        int ret = __alloc_buffer(mIONDevice_fd, size, &pBuffer->share_fd,(1<<30),ION_HEAP_TYPE_DMA_MASK);
        if (ret < 0) {
            ALOGE("%s:ion try alloc memory error!\n",__FUNCTION__);
            return nullptr;
        }

        uint8_t* cpu_ptr = (uint8_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                    pBuffer->share_fd, 0);
            if (MAP_FAILED == cpu_ptr) {
                ALOGE("ion mmap error!\n");
                ion_close(mIONDevice_fd);
                return nullptr;
            }
            if (cpu_ptr == nullptr)
                ALOGE("cpu_ptr is NULL");
        pBuffer->vaddr = cpu_ptr;
        ALOGE("vaddr=%p, share_fd = %d",pBuffer->vaddr,pBuffer->share_fd);
        *share_fd = pBuffer->share_fd;
        return pBuffer->vaddr;
}

int IONInterface::release_node(IONBufferNode* pBuffer) {
    pBuffer->IsUsed = false;
    int ret = munmap(pBuffer->vaddr, pBuffer->size);
    ALOGD("-----------%s: vaddr = %p", __FUNCTION__,pBuffer->vaddr);
    if (ret)
        ALOGD("munmap fail: %s\n",strerror(errno));

    ret = close(pBuffer->share_fd);
    ALOGD("-----------%s: close share_fd = %d", __FUNCTION__,pBuffer->share_fd);
    if (ret != 0)
        ALOGD("close ion shared fd failed for reason %s",strerror(errno));
    return ret;
}

void IONInterface::free_buffer(int share_fd) {
    ALOGD("%s\n", __FUNCTION__);
    IONBufferNode* pBuffer = nullptr;
    Mutex::Autolock lock(&IonLock);
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mPicBuffers[i].IsUsed && share_fd == mPicBuffers[i].share_fd) {
            pBuffer = &mPicBuffers[i];
            ALOGD("---------------%s:buffer idx = %d\n", __FUNCTION__,i);
            break;
        }
    }
    if (pBuffer == nullptr) {
        ALOGD("%s:free buffer error,not find related buffer!\n",__FUNCTION__);
        return;
    }
    release_node(pBuffer);
}
}

