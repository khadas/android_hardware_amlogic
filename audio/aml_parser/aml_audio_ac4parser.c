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
#define LOG_TAG "audio_ac4_parser"
#include <stdlib.h>
#include <string.h>
#include <cutils/log.h>
#include "aml_audio_ac4parser.h"
#include "aml_audio_bitsparser.h"
#include "aml_malloc_debug.h"

/**
 *
 * This value can be used for the initialization of the AC-4 framer.
 * The value 16 kB is determined by the maximum frame size used in broadcast applications.
 * now we use 16kb * 2 to make sure it can always contain the frame
 */
#define DOLBY_AC4_MAX_FRAMESIZE     (32768)

/* ETSI TS 103 190-2 V1.1.1 Annex C + ETSI TS 103 190-1 V1.2.1 AC-4 frame info */
#define DOLBY_AC4_HEADER_SIZE       (8 + 3)

#define AC4_SYNCWORD_AC40           (0xAC40)
#define AC4_SYNCWORD_AC41           (0xAC41)

#define AC4_SAMPLERATE_48K          (48000)
#define AC4_SAMPLERATE_44K          (44100)


enum PARSER_STATE {
    PARSER_SYNCING,
    PARSER_SYNCED,
    PARSER_LACK_DATA,
};

struct aml_ac4_parser {
    void * buf;
    int32_t buf_size;
    int32_t buf_remain;
    uint32_t status;
    struct audio_bit_parser bit_parser;
};

int aml_ac4_parser_open(void **pparser_handle)
{
    struct aml_ac4_parser *parser_hanlde = NULL;

    parser_hanlde = (struct aml_ac4_parser *)calloc(1, sizeof(struct aml_ac4_parser));
    if (parser_hanlde == NULL) {
        ALOGE("%s handle error", __func__);
        goto error;
    }

    parser_hanlde->buf_size   = DOLBY_AC4_MAX_FRAMESIZE;
    parser_hanlde->buf        = calloc(1, DOLBY_AC4_MAX_FRAMESIZE);
    if (parser_hanlde->buf == NULL) {
        ALOGE("%s data buffer error", __func__);
        free(parser_hanlde);
        parser_hanlde = NULL;
        goto error;
    }
    parser_hanlde->status     = PARSER_SYNCING;
    parser_hanlde->buf_remain = 0;
    *pparser_handle = parser_hanlde;
    ALOGI("%s exit =%p", __func__, parser_hanlde);
    return 0;
error:
    *pparser_handle = NULL;
    ALOGE("%s error", __func__);
    return -1;
}
int aml_ac4_parser_close(void *parser_handle)
{
    struct aml_ac4_parser *parser_hanlde = (struct aml_ac4_parser *)parser_handle;

    if (parser_hanlde) {
        if (parser_hanlde->buf) {
            free(parser_hanlde->buf);
        }
        free(parser_hanlde);
    }
    ALOGI("%s exit", __func__);
    return 0;
}

int aml_ac4_parser_reset(void *parser_handle)
{
    struct aml_ac4_parser *parser_hanlde = (struct aml_ac4_parser *)parser_handle;

    if (parser_hanlde) {
        parser_hanlde->status = PARSER_SYNCING;
        parser_hanlde->buf_remain = 0;
    }
    ALOGI("%s exit", __func__);
    return 0;
}

// ETSI TS 103 190 V1.1.1 Table 83
static uint32_t frame_rate_table_48Khz[16] = {
    23976,
    24000,
    25000,
    29970,
    30000,
    47950,
    48000,
    50000,
    59940,
    60000,
    100000,
    119880,
    120000,
    23440,
    0,
    0,
};

// ETSI TS 103 190 V1.1.1 Table 84
static uint32_t frame_rate_table_44Khz[16] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    21533,  /*11025/512*/
    0,
    0,
};


static int seek_ac4_sync_word(char *buffer, int size)
{
    int i = -1;
    if (size < 2) {
        return -1;
    }
    /*The sync_word can be either 0xAC40 or 0xAC41*/
    for (i = 0; i < (size - 1); i++) {
        if (buffer[i + 0] == 0xAC && buffer[i + 1] == 0x40) {
            return i;
        }
        if (buffer[i + 0] == 0xAC && buffer[i + 1] == 0x41) {
            return i;
        }
    }
    return -1;
}

// ETSI TS 103 190 V1.1.1  4.2.2
static int32_t readVariableBits(struct audio_bit_parser * bit_parser, int32_t nbits)
{
    int32_t value = 0;
    int32_t more_bits = 1;
    while (more_bits) {
        value += aml_audio_bitparser_getBits(bit_parser, nbits);
        more_bits = aml_audio_bitparser_getBits(bit_parser, 1);
        if (!more_bits) {
            break;
        }
        value++;
        value <<= nbits;
    }
    return value;
}


static int parse_ac4_frame_header(struct audio_bit_parser * bit_parser, const unsigned char *frameBuf, int length, struct ac4_parser_info * ac4_info)
{
    uint32_t frame_size = 0;
    uint32_t head_size = 0;
    bool     crc_enable = false;
    bool     b_wait_frames = false;
    uint32_t fs_index = 0;
    uint32_t frame_rate_index = 0;
    uint32_t frame_length = 0;
    if (length < DOLBY_AC4_HEADER_SIZE) {
        return -1;
    }

    aml_audio_bitparser_init(bit_parser, frameBuf, length);

    /* ETSI TS 103 190-2 V1.1.1 Annex C*/
    int32_t syncWord = aml_audio_bitparser_getBits(bit_parser, 16);
    if (syncWord == AC4_SYNCWORD_AC40) {
        crc_enable = false;
    } else if (syncWord == AC4_SYNCWORD_AC41) {
        crc_enable = true;
    } else {
        return -1;
    }
    head_size  = 2;  /*sync word size*/
    frame_size = aml_audio_bitparser_getBits(bit_parser, 16);
    head_size += 2;
    if (frame_size == 0xFFFF) {
        frame_size = aml_audio_bitparser_getBits(bit_parser, 24);
        head_size += 3;
    }

    if (frame_size == 0) {
        ALOGE("Invalid AC4 frame size 0");
        return -1;
    }

    frame_size += head_size;
    if (crc_enable) {
        frame_size += 2;
    }


    // ETSI TS 103 190-2 V1.1.1 6.2.1.1
    uint32_t bitstreamVersion = aml_audio_bitparser_getBits(bit_parser, 2);
    if (bitstreamVersion == 3) {
        bitstreamVersion += readVariableBits(bit_parser, 2);
    }

    aml_audio_bitparser_skipBits(bit_parser, 10); // Sequence Counter

    /*1 bit b_wait_frames*/
    b_wait_frames = aml_audio_bitparser_getBits(bit_parser, 1);

    if (b_wait_frames) {
        /*3bit wait_frames*/
        uint32_t waitFrames = aml_audio_bitparser_getBits(bit_parser, 3);
        if (waitFrames > 0) {
            aml_audio_bitparser_getBits(bit_parser, 2); // reserved;
        }
    }

    // ETSI TS 103 190 V1.1.1 Table 82
    fs_index = aml_audio_bitparser_getBits(bit_parser, 1);

    frame_rate_index = aml_audio_bitparser_getBits(bit_parser, 4);

    ac4_info->frame_size     = frame_size;
    ac4_info->sample_rate    = fs_index ? AC4_SAMPLERATE_48K : AC4_SAMPLERATE_44K;
    if (fs_index) {
        ac4_info->frame_rate = frame_rate_table_48Khz[frame_rate_index];
    } else {
        ac4_info->frame_rate = frame_rate_table_44Khz[frame_rate_index];
    }
    if (ac4_info->frame_rate == 0) {
        ALOGE("invalid ac4 samplerate =%d frame rate index=%d", ac4_info->sample_rate, frame_rate_index);
        return -1;
    }
    ALOGV("ac4 frame size=%d frame rate =%d sample rate=%d wait frame=%d", ac4_info->frame_size, ac4_info->frame_rate, ac4_info->sample_rate, b_wait_frames);

    return 0;
}


int aml_ac4_parser_process(void *parser_handle, const void *in_buffer, int32_t numBytes, int32_t *used_size, void **output_buf, int32_t *out_size, struct ac4_parser_info * ac4_info)
{
    struct aml_ac4_parser *parser_hanlde = (struct aml_ac4_parser *)parser_handle;
    size_t remain = 0;
    uint8_t *buffer = (uint8_t *)in_buffer;
    uint8_t * parser_buf = NULL;
    int32_t sync_word_offset = -1;
    int32_t buf_left = 0;
    int32_t buf_offset = 0;
    int32_t need_size = 0;

    int32_t ret = 0;
    int32_t data_valid = 0;
    int32_t new_buf_size = 0;
    int32_t loop_cnt = 0;
    int32_t frame_size = 0;
    int32_t frame_offset = 0;

    if (parser_hanlde == NULL) {
        ALOGE("error parser_hanlde is NULL");
        goto error;
    }

    if (ac4_info == NULL) {
        ALOGE("error ac4_info is NULL");
        goto error;
    }

    memset(ac4_info, 0, sizeof(struct ac4_parser_info));

    parser_buf = parser_hanlde->buf;
    buf_left   = numBytes;

    ALOGV("%s input buf size=%d status=%d", __func__, numBytes, parser_hanlde->status);

    /*we need at least DOLBY_AC4_HEADER_SIZE bytes*/
    if (parser_hanlde->buf_remain < DOLBY_AC4_HEADER_SIZE) {
        need_size = DOLBY_AC4_HEADER_SIZE - parser_hanlde->buf_remain;
        /*input data is not enough, just copy to internal buf*/
        if (buf_left < need_size) {
            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
            parser_hanlde->buf_remain += buf_left;
            goto error;
        }
        /*make sure the remain buf has DOLBY_AC4_HEADER_SIZE bytes*/
        memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, need_size);
        parser_hanlde->buf_remain += need_size;
        buf_offset += need_size;
        buf_left   = numBytes - buf_offset;

    }

    if (parser_hanlde->status == PARSER_SYNCING) {
        sync_word_offset = -1;
        while (sync_word_offset < 0) {
            /*sync the header, we have at least period bytes*/
            if (parser_hanlde->buf_remain < DOLBY_AC4_HEADER_SIZE) {
                ALOGE("we should not get there");
                parser_hanlde->buf_remain = 0;
                goto error;
            }
            sync_word_offset = seek_ac4_sync_word((char*)parser_buf, parser_hanlde->buf_remain);
            /*if we don't find the header in period bytes, move the last 1 bytes to header*/
            if (sync_word_offset < 0) {
                memmove(parser_buf, parser_buf + parser_hanlde->buf_remain - 1, 1);
                parser_hanlde->buf_remain = 1;
                need_size = DOLBY_AC4_HEADER_SIZE - parser_hanlde->buf_remain;
                /*input data is not enough, just copy to internal buf*/
                if (buf_left < need_size) {
                    memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
                    parser_hanlde->buf_remain += buf_left;
                    /*don't find the header, and there is no enough data*/
                    goto error;
                }
                /*make the buf has DOLBY_AC4_HEADER_SIZE bytes*/
                memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, need_size);
                parser_hanlde->buf_remain += need_size;
                buf_offset += need_size;
                buf_left = numBytes - buf_offset;
            }
            loop_cnt++;
        }
        /*got here means we find the sync word*/
        parser_hanlde->status = PARSER_SYNCED;

        data_valid = parser_hanlde->buf_remain - sync_word_offset;
        /*move the header to the beginning of buf*/
        if (sync_word_offset != 0) {
            memmove(parser_buf, parser_buf + sync_word_offset, data_valid);
        }
        parser_hanlde->buf_remain = data_valid;

        need_size = DOLBY_AC4_HEADER_SIZE - data_valid;
        /*get some bytes to make sure it is at least DOLBY_AC4_HEADER_SIZE bytes*/
        if (need_size > 0) {
            /*check if input has enough data*/
            if (buf_left < need_size) {
                memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
                parser_hanlde->buf_remain += buf_left;
                goto error;
            }
            /*make sure the remain buf has DOLBY_AC4_HEADER_SIZE bytes*/
            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset , need_size);
            parser_hanlde->buf_remain += need_size;
            buf_offset += need_size;
            buf_left = numBytes - buf_offset;
        }


    }

    /*double check here*/
    sync_word_offset = seek_ac4_sync_word((char*)parser_buf, parser_hanlde->buf_remain);
    if (sync_word_offset != 0) {
        ALOGE("we can't get here remain=%d,resync dolby header", parser_hanlde->buf_remain);
        parser_hanlde->buf_remain = 0;
        parser_hanlde->status = PARSER_SYNCING;
        goto error;
    }
    /* we got here means we find the ac4 header and
     * it is at the beginning of  parser buf and
     * it has at least DOLBY_AC4_HEADER_SIZE bytes, we can parse it
     */
    ret = parse_ac4_frame_header(&parser_hanlde->bit_parser, parser_buf, parser_hanlde->buf_remain, ac4_info);

    /*check whether the input data has a complete ac4 frame*/
    if (ret != 0 || ac4_info->frame_size == 0) {
        ALOGE("%s wrong frame size=%d", __func__, ac4_info->frame_size);
        parser_hanlde->buf_remain = 0;
        parser_hanlde->status = PARSER_SYNCING;
        goto error;
    }
    frame_size = ac4_info->frame_size;

    /*we have a complete payload*/
    if ((parser_hanlde->buf_remain + buf_left) >= frame_size) {
        need_size = frame_size - (parser_hanlde->buf_remain);
        if (need_size >= 0) {
            new_buf_size = parser_hanlde->buf_remain + need_size;
            if (new_buf_size > parser_hanlde->buf_size) {
                void * new_buf = realloc(parser_hanlde->buf, new_buf_size);
                if (new_buf == NULL) {
                    ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                    parser_hanlde->buf_remain = 0;
                    parser_hanlde->status = PARSER_SYNCING;
                    goto error;
                }
                parser_hanlde->buf      = new_buf;
                parser_hanlde->buf_size = new_buf_size;
                parser_buf = parser_hanlde->buf;
                ALOGI("%s realloc buf =%d", __func__, new_buf_size);
            }

            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, need_size);
            buf_offset += need_size;
            buf_left = numBytes - buf_offset;

            *output_buf = (void*)(parser_buf);
            *out_size   = frame_size;
            *used_size  = buf_offset;
            ALOGV("OK framesize =%d used size=%d loop_cnt=%d", frame_size, buf_offset, loop_cnt);
            /*one frame has complete, need find next one*/
            parser_hanlde->buf_remain = 0;
            parser_hanlde->status = PARSER_SYNCING;
        } else {
            /*internal buf has more data than framsize, we only need part of it*/
            *output_buf = (void*)(parser_buf);
            *out_size   = frame_size;
            /*move need_size bytes back to original buf*/
            *used_size  = buf_offset + need_size;
            ALOGV("wrap frame size=%d used size=%d back size =%d loop_cnt=%d", frame_size, buf_offset, need_size, loop_cnt);
            if (*used_size <= 0) {
                ALOGE("%s wrong used size =%d", __func__, *used_size);
                parser_hanlde->buf_remain = 0;
                parser_hanlde->status = PARSER_SYNCING;
                goto error;
            }
            /*one frame has complete, need find next one*/
            parser_hanlde->buf_remain = 0;
            parser_hanlde->status = PARSER_SYNCING;
        }
    } else {
        /*check whether the input buf size is big enough*/
        new_buf_size = parser_hanlde->buf_remain + buf_left;
        if (new_buf_size > parser_hanlde->buf_size) {
            void * new_buf = realloc(parser_hanlde->buf, new_buf_size);
            if (new_buf == NULL) {
                ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                parser_hanlde->buf_remain = 0;
                parser_hanlde->status = PARSER_SYNCING;
                goto error;
            }
            parser_hanlde->buf      = new_buf;
            parser_hanlde->buf_size = new_buf_size;
            parser_buf = parser_hanlde->buf;
            ALOGI("%s realloc buf =%d", __func__, new_buf_size);
        }
        memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
        parser_hanlde->buf_remain += buf_left;
        parser_hanlde->status = PARSER_LACK_DATA;
        goto error;
    }

    return 0;

error:
    *output_buf = NULL;
    *out_size   = 0;
    *used_size = numBytes;
    return 0;
}
