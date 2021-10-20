/*
 * Copyright (C) 2017 Amlogic Corporation.
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

#ifndef _DOLBY_LIB_API_H_
#define _DOLBY_LIB_API_H_

#if ANDROID_PLATFORM_SDK_VERSION > 29
#ifdef MS12_V24_ENABLE
#define DOLBY_MS12_LIB_PATH_A "/odm/lib/ms12/libdolbyms12.so"
#else
#define DOLBY_MS12_LIB_PATH_A "/odm/lib/libdolbyms12.so"
#endif
#define DOLBY_DCV_LIB_PATH_A "/odm/lib/libHwAudio_dcvdec.so"
#define DTS_DCA_LIB_PATH_A "/odm/lib/libHwAudio_dtshd.so"
#else
#define DOLBY_MS12_LIB_PATH_A "/vendor/lib/libdolbyms12.so"
#define DOLBY_DCV_LIB_PATH_A "/vendor/lib/libHwAudio_dcvdec.so"
#define DTS_DCA_LIB_PATH_A "/vendor/lib/libHwAudio_dtshd.so"
#endif

#define DOLBY_TUNING_DAT "/vendor/etc/ms12_tuning.dat"

/** Dolby Lib Type used in Current System */
typedef enum eDolbyLibType {
    eDolbyNull  = 0,
    eDolbyDcvLib  = 1,
    eDolbyMS12Lib = 2,
} eDolbyLibType_t;


enum eDolbyLibType detect_dolby_lib_type(void);
int dolby_lib_decode_enable(eDolbyLibType_t lib_type);
int dts_lib_decode_enable();
char * get_ms12_path (void);
/*
 *@brief get ms12 dap init mode value
 */
int get_ms12_dap_init_mode(bool is_tv);

/*
 *@brief check that the MS12 Tuning dat is existing or not.
 */
bool is_ms12_tuning_dat_in_dut();

#endif //_DOLBY_LIB_API_H_
