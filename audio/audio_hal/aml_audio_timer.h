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


#ifndef AML_AUDIO_SLEEP_H
#define AML_AUDIO_SLEEP_H

#include <stdint.h>

#define MSEC_PER_SEC    1000L
#define USEC_PER_MSEC   1000L
#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define USEC_PER_SEC    1000000L
#define NSEC_PER_SEC    1000000000LL
#define FSEC_PER_SEC    1000000000000000LL


int aml_audio_sleep(uint64_t us);

uint64_t aml_audio_get_systime(void);

uint64_t aml_audio_get_systime_ns(void);

int64_t calc_time_interval_us(struct timespec *ts_start, struct timespec *ts_end);

#endif

