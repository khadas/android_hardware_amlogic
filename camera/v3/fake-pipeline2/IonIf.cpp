/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#define LOG_TAG "Camportal_ION Interface"

#include "IonIf.h"
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <utils/Log.h>
#include <utils/threads.h>
#include <ui/GraphicBufferAllocator.h>
#include <hardware/gralloc1.h>
#include <amlogic/am_gralloc_ext.h>
#include "CamHalDebugLog.h"
namespace android {
IONInterface* IONInterface::mIONInstance = nullptr;

Mutex IONInterface::mLock;
int IONInterface::mCount = 0;
IONBufferNode IONInterface::mPicBuffers[MAX_BUFFER_NUM];
IONInterface::IONInterface() {
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        mPicBuffers[i].vaddr = nullptr;
        mPicBuffers[i].buffer_handle = NULL;
        mPicBuffers[i].share_fd = -1;
        mPicBuffers[i].size = 0;
        mPicBuffers[i].IsUsed = false;
    }
    CAMHAL_LOGD("%s construct\n", __FUNCTION__);
}

IONInterface::~IONInterface() {
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mPicBuffers[i].IsUsed)
            release_node(&mPicBuffers[i]);
    }
}

IONInterface* IONInterface::get_instance() {
    CAMHAL_LOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&mLock);
    mCount++;
    if (mIONInstance != nullptr)
        return mIONInstance;
    CAMHAL_LOGD("%s: create new ion object \n", __FUNCTION__);
    mIONInstance = new IONInterface;
    return mIONInstance;
}

void IONInterface::put_instance() {
    CAMHAL_LOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&mLock);
    mCount = mCount - 1;
    if (!mCount && mIONInstance != nullptr) {
        CAMHAL_LOGD("%s delete ION Instance \n", __FUNCTION__);
        delete mIONInstance;
        mIONInstance = nullptr;
    }
}

uint8_t* IONInterface::alloc_buffer(size_t size, int* share_fd, bufferMode mode) {
    CAMHAL_LOGD("%s\n", __FUNCTION__);
    IONBufferNode* pBuffer = nullptr;
    int i = 0;
    Mutex::Autolock lock(&mLock);
    for (i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mPicBuffers[i].IsUsed == false) {
            mPicBuffers[i].IsUsed = true;
            pBuffer = &mPicBuffers[i];
            CAMHAL_LOGD("---------------%s:size= %zu,buffer idx = %d\n", __FUNCTION__, size, i);
            break;
        }
    }
    if (i == MAX_BUFFER_NUM) {
        CAMHAL_LOGE("%s: alloc fail",__FUNCTION__);
        return nullptr;
    }
    pBuffer->size = size;

    uint32_t stride;
    int format = 33; /* HAL_PIXEL_FORMAT_BLOB */
    uint64_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    if (mode == cache) {
        CAMHAL_LOGD("ION mode is cache");
        usage = (usage | GRALLOC1_CONSUMER_USAGE_CPU_READ_OFTEN);
    }
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();

    if (NO_ERROR != allocService.allocate(size, 1, format, 1, usage,
                                          &pBuffer->buffer_handle, &stride, 0, "camera")) {
        CAMHAL_LOGE("alloc buffer failed");
        return nullptr;
    }
    if (pBuffer->buffer_handle) {
        pBuffer->share_fd = am_gralloc_get_buffer_fd((native_handle_t *)pBuffer->buffer_handle);
        if (pBuffer->share_fd < 0) {
            allocService.free(pBuffer->buffer_handle);
            CAMHAL_LOGE("get fd fail");
            return nullptr;
        }
    }
    uint8_t* cpu_ptr = (uint8_t*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                                      pBuffer->share_fd, 0);
    if (MAP_FAILED == cpu_ptr) {
        CAMHAL_LOGE("ion mmap error!\n");
        return nullptr;
    }
    if (cpu_ptr == nullptr)
        CAMHAL_LOGE("cpu_ptr is NULL");
    pBuffer->vaddr = cpu_ptr;

    CAMHAL_LOGE("vaddr=%p, share_fd = %d",pBuffer->vaddr,pBuffer->share_fd);
    *share_fd = pBuffer->share_fd;
    return pBuffer->vaddr;
}

int IONInterface::release_node(IONBufferNode* pBuffer) {
    GraphicBufferAllocator & allocService = GraphicBufferAllocator::get();
    pBuffer->IsUsed = false;
    CAMHAL_LOGD("-----------%s: vaddr = %p", __FUNCTION__, pBuffer->vaddr);
    int ret = munmap(pBuffer->vaddr, pBuffer->size);
    if (ret)
        CAMHAL_LOGD("munmap fail: %s\n", strerror(errno));

    /* do not need close share_fd */
    pBuffer->share_fd = -1;
    if (pBuffer->buffer_handle) {
        allocService.free(pBuffer->buffer_handle);
        pBuffer->buffer_handle = NULL;
    }
    return ret;
}

void IONInterface::free_buffer(int share_fd) {
    CAMHAL_LOGD("%s\n", __FUNCTION__);
    IONBufferNode* pBuffer = nullptr;
    Mutex::Autolock lock(&mLock);
    for (int i = 0; i < MAX_BUFFER_NUM; i++) {
        if (mPicBuffers[i].IsUsed && share_fd == mPicBuffers[i].share_fd) {
            pBuffer = &mPicBuffers[i];
            CAMHAL_LOGD("---------------%s:buffer idx = %d\n", __FUNCTION__,i);
            break;
        }
    }
    if (pBuffer == nullptr) {
        CAMHAL_LOGD("%s:free buffer error,not find related buffer!\n",__FUNCTION__);
        return;
    }
    release_node(pBuffer);
}
}

