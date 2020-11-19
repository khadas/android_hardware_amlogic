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

#ifndef _AML_AUDIO_MS12_BYPASS_H_
#define _AML_AUDIO_MS12_BYPASS_H_

struct bypass_frame_info {
    int32_t audio_format;
    int32_t samplerate;
    bool dependency_frame;
    int numblks;
};


int aml_ms12_bypass_open(void **pparser_handle);
int aml_ms12_bypass_close(void *parser_handle);
int aml_ms12_bypass_checkin_data(void *parser_handle, const void *buffer, int32_t numBytes, struct bypass_frame_info * data_info);
int aml_ms12_bypass_checkout_data(void *phandle, void **output_buf, int32_t *out_size, uint64_t offset, struct bypass_frame_info *frame_info);
int aml_ms12_bypass_reset(void *parser_handle);

#endif
