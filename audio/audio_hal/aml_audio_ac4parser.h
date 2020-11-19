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

#ifndef _AML_AUDIO_AC4PARSER_H_
#define _AML_AUDIO_AC4PARSER_H_

struct ac4_parser_info {
    int frame_size;     /*bitsteam size*/
    int frame_rate;   /*ac4 frame rate*/
    int sample_rate;    /*sample rate*/
};


int aml_ac4_parser_open(void **pparser_handle);
int aml_ac4_parser_close(void *parser_handle);
int aml_ac4_parser_process(void *parser_handle, const void *buffer, int32_t numBytes, int32_t *used_size, void **output_buf, int32_t *out_size, struct ac4_parser_info * ac4_info);
int aml_ac4_parser_reset(void *parser_handle);


#endif
