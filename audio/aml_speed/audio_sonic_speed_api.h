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

#ifndef AUDIO_ANDROID_SPEED_H
#define AUDIO_ANDROID_SPEED_H

#include "aml_audio_speed_manager.h"

int sonic_speed_open(void **handle, audio_speed_config_t *speed_config);
void sonic_speed_close(void *handle);
int sonic_speed_process(void *handle, void * in_buffer, size_t bytes, void * out_buffer, size_t * out_size);

extern audio_speed_func_t audio_sonic_speed_func;

#endif

