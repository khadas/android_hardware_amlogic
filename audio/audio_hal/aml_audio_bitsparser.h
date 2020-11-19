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

#ifndef AML_AUDIO_BITPARSER_H
#define AML_AUDIO_BITPARSER_H

struct audio_bit_parser {
    const uint8_t *data_buf;
    size_t data_size;
    uint32_t num_reservoir;  // left-aligned bits
    size_t num_bitsleft;
    bool is_over_read;
};


int aml_audio_bitparser_init(struct audio_bit_parser * bit_parser, const uint8_t *buf, size_t size);

int aml_audio_bitparser_deinit(struct audio_bit_parser * bit_parser);

int aml_audio_bitparser_getBits(struct audio_bit_parser * bit_parser, size_t n);

bool aml_audio_bitparser_skipBits(struct audio_bit_parser * bit_parser, size_t n);



#endif
