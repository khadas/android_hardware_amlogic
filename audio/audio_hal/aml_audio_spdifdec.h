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

#endif //
