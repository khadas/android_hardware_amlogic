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
#ifndef AML_AUDIO_SPEED_MANAGER_H
#define AML_AUDIO_SPEED_MANAGER_H


typedef enum {
    AML_AUDIO_SIMPLE_SPEED,
    AML_AUDIO_SONIC_SPEED,
} speed_type_t;


typedef struct audio_speed_config {
    int aformat;
    float speed;
    unsigned int input_sr;
    unsigned int channels;
} audio_speed_config_t;


typedef struct aml_audio_speed {
    speed_type_t speed_type;
    audio_speed_config_t speed_config;
    float speed_rate;
    unsigned int frame_bytes;
    size_t speed_size;   /*the speedd data size*/
    size_t speed_buffer_size; /*the total buffer size*/
    void *speed_buffer;
    void * speed_handle;
    size_t total_in;
    size_t total_out;
} aml_audio_speed_t;


typedef struct audio_speed_func {
    int (*speed_open)(void **handle, audio_speed_config_t *speed_config);
    void (*speed_close)(void *handle);
    int (*speed_process)(void *handle, void * in_buffer, size_t bytes, void * out_buffer, size_t * out_size);

} audio_speed_func_t;



int aml_audio_speed_init(aml_audio_speed_t ** ppspeed_handle, speed_type_t speed_type, audio_speed_config_t *speed_config);

int aml_audio_speed_close(aml_audio_speed_t * speed_handle);

int aml_audio_speed_process(aml_audio_speed_t * speed_handle, void * in_data, size_t size);

int aml_audio_speed_reset(aml_audio_speed_t * aml_audio_speed);
int aml_audio_speed_process_wrapper(aml_audio_speed_t **speed_handle, void *buffer, size_t len,float speed, int sr, int ch_num);

#endif

