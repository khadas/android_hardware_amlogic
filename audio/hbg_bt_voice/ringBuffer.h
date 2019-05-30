#ifndef __RIGN_BUFFER_H
#define __RIGN_BUFFER_H

#include <pthread.h>
#include <stdio.h>


#ifdef __cplusplus
extern "C" {
#endif

struct RingBuffer{
    unsigned char *buf;
    int len;
    int start_pos, end_pos;
    int validDataLen;
    pthread_mutex_t mutex;
};

struct RingBuffer* InitRingBuffer(int len);
void DeInitRingBuffer(struct RingBuffer* ptr);
int ReadRingBuffer(struct RingBuffer* ptr, unsigned char *data,int len);
int WriteRingBuffer(struct RingBuffer* ptr, unsigned char *data,int len);
void ResetRingBuffer(struct RingBuffer* ptr);
int ReadRingBuffer2(struct RingBuffer* ptr, unsigned char *data,int len);
#ifdef __cplusplus
}
#endif

#endif /* __RIGN_BUFFER_H */
