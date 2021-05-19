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

#ifndef _AML_DEV2MIX_PROCESS_H_
#define _AML_DEV2MIX_PROCESS_H_

size_t aml_dev2mix_parser_process(struct aml_stream_in *in, unsigned char *buffer, size_t bytes);
int aml_dev2mix_parser_create(struct audio_hw_device *dev, audio_devices_t input_dev);
int aml_dev2mix_parser_release(struct aml_audio_device *aml_dev);

#endif

