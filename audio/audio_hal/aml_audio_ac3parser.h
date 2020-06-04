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

#ifndef _AML_AUDIO_AC3PARSER_H_
#define _AML_AUDIO_AC3PARSER_H_

struct ac3_parser_info {
    int frame_size;
    int channel_num;
    int numblks;
    int timeslice_61937;
    int framevalid_flag;
    int frame_dependent;
};


int aml_ac3_parser_open(void **pparser_handle);
int aml_ac3_parser_close(void *parser_handle);
int aml_ac3_parser_process(void *parser_handle, const void *buffer, int32_t numBytes, int32_t *used_size, void **output_buf, int32_t *out_size, struct ac3_parser_info * ac3_info);
int aml_ac3_parser_reset(void *parser_handle);


#endif
