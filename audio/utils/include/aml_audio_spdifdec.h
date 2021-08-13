/*
 * Copyright (C) 2020 Amlogic Corporation.
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

#ifndef _AML_AUDIO_SPDIFDEC_H_
#define _AML_AUDIO_SPDIFDEC_H_

int aml_spdif_decoder_open(void **spdifdec_handle);
int aml_spdif_decoder_close(void *phandle);
int aml_spdif_decoder_process(void *phandle, const void *inbuf, int32_t n_bytes_inbuf, int32_t *used_size, void **output_buf, int32_t *out_size);
int aml_spdif_decoder_getformat(void *phandle);
int aml_spdif_decoder_reset(void *phandle);


/*IEC61937 package presamble Pc value 0-4bit*/
enum IEC61937_PC_Value {
    IEC61937_NULL               = 0x00,          ///< NULL data
    IEC61937_AC3                = 0x01,          ///< AC-3 data
    IEC61937_DTS1               = 0x0B,          ///< DTS type I   (512 samples)
    IEC61937_DTS2               = 0x0C,          ///< DTS type II  (1024 samples)
    IEC61937_DTS3               = 0x0D,          ///< DTS type III (2048 samples)
    IEC61937_DTSHD              = 0x11,          ///< DTS HD data
    IEC61937_EAC3               = 0x15,          ///< E-AC-3 data
    IEC61937_MAT                = 0x16,          ///< MAT data
    IEC61937_PAUSE              = 0x03,          ///< Pause
};


#ifndef AC3_PERIOD_SIZE
#define AC3_PERIOD_SIZE  (6144)
#endif

#ifndef EAC3_PERIOD_SIZE
#define EAC3_PERIOD_SIZE (24576)
#endif

#define MAT_PERIOD_SIZE  (61440)
#define DTS1_PERIOD_SIZE (2048)
#define DTS2_PERIOD_SIZE (4096)
#define DTS3_PERIOD_SIZE (8192)

#define DTSHD_PERIOD_SIZE   (512*8)
#define DTSHD_PERIOD_SIZE_1 (512*32)
#define DTSHD_PERIOD_SIZE_2 (512*48)

#endif