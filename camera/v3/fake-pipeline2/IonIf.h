/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#ifndef _ION_IF_H_
#define _ION_IF_H_

#include <stdio.h>
#include <stdlib.h>
#include <utils/threads.h>

namespace android {
struct IONBufferNode {
    int share_fd;
    int ion_handle;
    uint8_t* vaddr;
    size_t size;
    size_t IsUsed;
};
//#define MAX_BUFFER_NUM (12)
#define MAX_BUFFER_NUM (25)

class IONInterface {
private:
    static int mIONDevice_fd;
    static IONBufferNode mPicBuffers[MAX_BUFFER_NUM];
    static IONInterface* mIONInstance;
    static Mutex IonLock;
    static int mCount;
private:
    IONInterface();
    ~IONInterface();
    int __alloc_buffer(int ion_fd, size_t size,
            int* pShareFd, unsigned int flag, unsigned int alloc_hmask);
public:
    static IONInterface* get_instance();
    static void put_instance();
    uint8_t* alloc_buffer(size_t size, int* share_fd);
    void free_buffer(int share_fd);
    int release_node(IONBufferNode* pBuffer);
};
}
#endif

