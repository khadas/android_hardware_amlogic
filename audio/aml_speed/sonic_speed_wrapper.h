#ifndef _AML_SONIC_H_
#define _AML_SONIC_H_

#include <stdint.h>
#include <stdlib.h>
#include <system/audio.h>
#include "sonic.h"

typedef struct sonic_speed_handle {
    float speed;
    unsigned int input_sr;
    unsigned int format;
    unsigned int channels;
    unsigned int ringbuf_size;
    void *output_buf;
    sonicStream stream;
} sonic_speed_handle_t;

int sonic_speed_init(sonic_speed_handle_t *handle,
                          float speed,
                          int sr,
                          int ch);

int sonic_speed_write(sonic_speed_handle_t *handle, void *buf, size_t write_size);
int sonic_speed_read(sonic_speed_handle_t *handle, void *buf, size_t read_size);
int sonic_speed_release(sonic_speed_handle_t *handle);
#endif

