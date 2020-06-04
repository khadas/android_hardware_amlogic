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
#define LOG_TAG "audio_ac3_parser"

#include <cutils/log.h>
#include "audio_hw.h"
#include "audio_format_parse.h"
#include "aml_audio_ac3parser.h"

#define DOLBY_DDPP_MAXSIZE          (32768)
#define DOLBY_DDP_HEADER_SIZE       (12)

#define BYTE_REV(a) ((((uint16_t)a) & 0xff) << 8 | ((uint16_t)a) >> 8)

/*====================*/
/*merge from ffmpeg begin====*/

/**
 * Possible frame sizes.
 * from ATSC A/52 Table 5.18 Frame Size Code Table.
 */
static const uint16_t ff_ac3_frame_size_tab[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
};

/**
 * Map audio coding mode (acmod) to number of full-bandwidth channels.
 * from ATSC A/52 Table 5.8 Audio Coding Mode
 */
static const uint8_t ff_ac3_channels_tab[8] = {
    2, 1, 2, 3, 3, 4, 4, 5
};
/** Channel mode (audio coding mode) */
typedef enum {
    AC3_CHMODE_DUALMONO = 0,
    AC3_CHMODE_MONO,
    AC3_CHMODE_STEREO,
    AC3_CHMODE_3F,
    AC3_CHMODE_2F1R,
    AC3_CHMODE_3F1R,
    AC3_CHMODE_2F2R,
    AC3_CHMODE_3F2R
} AC3ChannelMode;



enum PARSER_STATE {
    PARSER_SYNCING,
    PARSER_SYNCED,
    PARSER_LACK_DATA,
};

struct aml_ac3_parser {
    void * buf;
    int32_t buf_size;
    int32_t buf_remain;
    uint32_t status;
    int32_t framesize;
};

int aml_ac3_parser_open(void **pparser_handle)
{
    struct aml_ac3_parser *parser_hanlde = NULL;

    parser_hanlde = (struct aml_ac3_parser *)calloc(1, sizeof(struct aml_ac3_parser));
    if (parser_hanlde == NULL) {
        ALOGE("%s handle error", __func__);
        goto error;
    }

    parser_hanlde->buf_size  = DOLBY_DDPP_MAXSIZE;
    parser_hanlde->buf  = calloc(1, DOLBY_DDPP_MAXSIZE);
    if (parser_hanlde->buf == NULL) {
        ALOGE("%s data buffer error", __func__);
        goto error;
    }
    parser_hanlde->status = PARSER_SYNCING;
    parser_hanlde->buf_remain = 0;
    *pparser_handle = parser_hanlde;
    ALOGI("%s exit =%p", __func__, parser_hanlde);
    return 0;
error:
    *pparser_handle = NULL;
    ALOGE("%s error", __func__);
    return -1;
}
int aml_ac3_parser_close(void *parser_handle)
{
    struct aml_ac3_parser *parser_hanlde = (struct aml_ac3_parser *)parser_handle;

    if (parser_hanlde) {
        if (parser_hanlde->buf) {
            free(parser_hanlde->buf);
        }
        free(parser_hanlde);
    }
    ALOGE("%s exit", __func__);
    return 0;
}

int aml_ac3_parser_reset(void *parser_handle)
{
    struct aml_ac3_parser *parser_hanlde = (struct aml_ac3_parser *)parser_handle;

    if (parser_hanlde) {
        parser_hanlde->status = PARSER_SYNCING;
        parser_hanlde->buf_remain = 0;
    }
    ALOGE("%s exit", __func__);
    return 0;
}


static int seek_dolby_sync_word(char *buffer, int size)
{
    int i = -1;

    for (i = 0; i < (size - 1); i++) {
        if (buffer[i + 0] == 0x0b && buffer[i + 1] == 0x77) {
            return i;
        }
        if (buffer[i + 0] == 0x77 && buffer[i + 1] == 0x0b) {
            return i;
        }
    }
    return -1;
}

static int check_ac3_syncword(const unsigned char *ptr, int size)
{
    if (size < 2) {
        return 0;
    }
    if (ptr[0] == 0x0b && ptr[1] == 0x77) {
        return 1;
    }
    if (ptr[0] == 0x77 && ptr[1] == 0x0b) {
        return 2;
    }

    return 0;
}


/*
 *parse frame header[ATSC Standard,Digital Audio Compression (AC-3, E-AC-3)]
 */
static int parse_dolby_frame_header
(const unsigned char *frameBuf
 , int length
 , int *frame_offset
 , int *frame_size
 , int *channel_num
 , int *numblks
 , int *timeslice_61937
 , int *framevalid_flag
 , int *frame_dependent)
{
    int acmod = 0;
    int lfeOn = 0;
    int nIsEc3 = 0;
    int frame_size_code = 0;
    int sr_code = 0;
    int substreamid = 0;
    int numblk_per_frame;
    char inheader[12] = {0};
    int offset = 0;
    int header = 0;
    int i = 0;
    *channel_num = 2;

    /*TODO, used to correct iec61937 packet*/
    *timeslice_61937 = 0;
    *framevalid_flag = 0;
    *frame_dependent = 0;

    for (i = 0; i < length; ++i) {
        if ((header = check_ac3_syncword(&frameBuf[i], length - i)) > 0) {
            offset = i;
            break;
        }
    }
    /*step 1, frame header 0x0b77/0x770b*/
    if (header == 0) {
        //ALOGE("locate frame header 0x0b77/0x770b failed\n");
        goto error;/*no frame header, maybe need more data*/
    }

    /*step 2, copy 12bytes to inheader,  find one frame*/
    if (length - offset < 12) {
        /*
         *find the sync word 0x0b77/0x770b,
         *but we need 12bytes which will copy to inheader[12], need more data
         */
        ALOGE("data less than one frame!!!\n");
        goto error;
    } else {
        memcpy((void *) inheader, (const void *)(frameBuf + offset), 12);
    }

    if (header == 2) {
        int16_t *p_data = (int16_t *) inheader;
        unsigned int idx;
        unsigned int inheader_len = 12;
        unsigned int top = inheader_len / 2;
        for (idx = 0; idx < top; idx++) {
            p_data[idx] = (int16_t) BYTE_REV(p_data[idx]);
        }
    }

    if (length < 12) {
        ALOGE("%s len %d\n", __FUNCTION__, length);
        goto error;
    } else {
        //ALOGV("dolby head:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",
        //    inheader[0],inheader[1],inheader[2], inheader[3],inheader[4],inheader[5]);
        int bsid = (inheader[5] >> 3) & 0x1f;//bitstream_id,bit[40,44]
        if (bsid > 16) {
            goto error;    //invalid bitstream_id
        }
        if (bsid <= 8) {
            nIsEc3 = 0;
        } else if ((bsid <= 16) && (bsid > 10)) {
            nIsEc3 = 1;
        }

        if (nIsEc3 == 0) {
            int use_bits = 0;

            substreamid = 0;
            sr_code = inheader[4] >> 6;
            if (sr_code == 3) {
                ALOGE("%s error *sr_code %d", __FUNCTION__, sr_code);
                goto error;
            }
            frame_size_code = inheader[4] & 0x3F;
            if (frame_size_code > 37) {
                ALOGE("%s error frame_size_code %d", __FUNCTION__, frame_size_code);
                goto error;
            }
            acmod = (inheader[6] >> 5) & 0x7;// 3bits
            use_bits = use_bits + 3;
            if (acmod == AC3_CHMODE_STEREO) {
                //int dolby_surround_mode = (inheader[6] >> 3) &0x3; // 2bits
                use_bits = use_bits + 2;
            } else {
                if ((acmod & 1)  && (acmod != AC3_CHMODE_MONO)) {
                    //int center_mix_level =  center_levels[ (inheader[6] >> 3) &0x3]; // 2bits
                    use_bits = use_bits + 2;
                }
                if (acmod & 4) {
                    //int surround_mix_level = surround_levels[ (inheader[6] >> 1) &0x3]; // 2bits
                    use_bits = use_bits + 2;
                }
            }
            lfeOn = (inheader[6] >> (8 - use_bits - 1)) & 0x1; // 1bit
            *frame_size = ff_ac3_frame_size_tab[frame_size_code][sr_code] * 2;
            numblk_per_frame = 6;
            *numblks = numblk_per_frame;
            *timeslice_61937 = 1;
            *framevalid_flag = 1;
        } else {
            int numblkscod = 0;
            int strmtyp = (inheader[2] >> 6) & 0x3;
            int substreamid = (inheader[2] >> 3) & 0x7;
            *frame_size = ((inheader[2] & 0x7) * 0x100 + inheader[3] + 1) << 1;
            sr_code = inheader[4] >> 6;
            acmod = (inheader[4] >> 1) & 0x7;
            lfeOn = inheader[4] & 0x1;
            numblkscod = (sr_code == 0x3) ? 0x3 : ((inheader[4] >> 4) & 0x3);
            numblk_per_frame = (numblkscod == 0x3) ? 6 : (numblkscod + 1);
            ALOGV("%s() ec3 numblkscod %d numblk_per_frame %d substreamid %d strmtyp %d\n",
                  __FUNCTION__, numblkscod, numblk_per_frame, substreamid, strmtyp);
            if (substreamid == 0 && strmtyp == 0) {
                if (*framevalid_flag == 0) {
                    *timeslice_61937 = 0;
                    // *numblks += numblk_per_frame;
                    *framevalid_flag = 1;
                } else if (*framevalid_flag == 1) {
                    if (*numblks  == 6) {
                        *timeslice_61937 = 1;
                        // *numblks = numblk_per_frame;
                    } else if (*numblks  > 6) {
                        *timeslice_61937 = 2;
                        // *numblks = numblk_per_frame;
                    } else {
                        *timeslice_61937 = 0;
                        // *numblks += numblk_per_frame;
                    }
                }
            } else if (strmtyp == 1) {
                *timeslice_61937 = 3;
            }
            *numblks = numblk_per_frame;
            *frame_dependent = strmtyp;

        }
        // ALOGV("%s acmod %d lfeOn %d\n", nIsEc3==0?"ac3":"ec3",acmod, lfeOn);
        *channel_num = ff_ac3_channels_tab[acmod] + lfeOn;
    }
    *frame_offset = offset;
    ALOGV("%s frame_offset %d frame_size %d channel_num %d numblks %d timeslice_61937 %d framevalid_flag %d\n",
          __FUNCTION__, *frame_offset, *frame_size, *channel_num, *numblks, *timeslice_61937, *framevalid_flag);

    return 0;
error:
    *frame_offset = 0;
    return 1;
}


int aml_ac3_parser_process(void *parser_handle, const void *in_buffer, int32_t numBytes, int32_t *used_size, void **output_buf, int32_t *out_size, struct ac3_parser_info * ac3_info)
{
    struct aml_ac3_parser *parser_hanlde = (struct aml_ac3_parser *)parser_handle;
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
        goto error;
    }

    if (ac3_info == NULL) {
        goto error;
    }

    memset(ac3_info, 0, sizeof(struct ac3_parser_info));

    parser_buf = parser_hanlde->buf;
    buf_left     = numBytes;

    ALOGV("%s input buf size=%d status=%d", __func__, numBytes, parser_hanlde->status);

    /*we need at least 12 bytes*/
    if (parser_hanlde->buf_remain < DOLBY_DDP_HEADER_SIZE) {
        need_size = DOLBY_DDP_HEADER_SIZE - parser_hanlde->buf_remain;
        /*input data is not enough, just copy to internal buf*/
        if (buf_left < need_size) {
            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
            parser_hanlde->buf_remain += buf_left;
            goto error;
        }
        /*make sure the remain buf has 12 bytes*/
        memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, need_size);
        parser_hanlde->buf_remain += need_size;
        buf_offset += need_size;
        buf_left   = numBytes - buf_offset;

    }

    if (parser_hanlde->status == PARSER_SYNCING) {
        sync_word_offset = -1;
        while (sync_word_offset < 0) {
            /*sync the header, we have at least period bytes*/
            if (parser_hanlde->buf_remain < DOLBY_DDP_HEADER_SIZE) {
                ALOGE("we should not get there");
                parser_hanlde->buf_remain = 0;
                goto error;
            }
            sync_word_offset = seek_dolby_sync_word((char*)parser_buf, parser_hanlde->buf_remain);
            /*if we don't find the header in period bytes, move the last 1 bytes to header*/
            if (sync_word_offset < 0) {
                memmove(parser_buf, parser_buf + parser_hanlde->buf_remain - 1, 1);
                parser_hanlde->buf_remain = 1;
                need_size = DOLBY_DDP_HEADER_SIZE - parser_hanlde->buf_remain;
                /*input data is not enough, just copy to internal buf*/
                if (buf_left < need_size) {
                    memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
                    parser_hanlde->buf_remain += buf_left;
                    /*don't find the header, and there is no enough data*/
                    goto error;
                }
                /*make the buf has 12 bytes*/
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

        need_size = DOLBY_DDP_HEADER_SIZE - data_valid;
        /*get some bytes to make sure it is at least 12 bytes*/
        if (need_size > 0) {
            /*check if input has enough data*/
            if (buf_left < need_size) {
                memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
                parser_hanlde->buf_remain += buf_left;
                goto error;
            }
            /*make sure the remain buf has 12 bytes*/
            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset , need_size);
            parser_hanlde->buf_remain += need_size;
            buf_offset += need_size;
            buf_left = numBytes - buf_offset;
        }


    }

    /*double check here*/
    sync_word_offset = seek_dolby_sync_word((char*)parser_buf, parser_hanlde->buf_remain);
    if (sync_word_offset != 0) {
        ALOGE("we can't get here remain=%d,resync dolby header", parser_hanlde->buf_remain);
        parser_hanlde->buf_remain = 0;
        parser_hanlde->status = PARSER_SYNCING;
        goto error;
    }
    /* we got here means we find the dolby header and
     * it is at the beginning of  parser buf and
     * it has at least 12 bytes, we can parse it
     */
    ret = parse_dolby_frame_header(parser_buf, parser_hanlde->buf_remain,  &frame_offset, &ac3_info->frame_size,
                                   &ac3_info->channel_num, &ac3_info->numblks, &ac3_info->timeslice_61937,
                                   &ac3_info->framevalid_flag,
                                   &ac3_info->frame_dependent);



    /*check whether the input data has a complete ac3 frame*/
    if (ac3_info->frame_size == 0) {
        ALOGE("%s wrong frame size=%d", __func__, ac3_info->frame_size);
        parser_hanlde->buf_remain = 0;
        parser_hanlde->status = PARSER_SYNCING;
        goto error;
    }

    frame_size = ac3_info->frame_size;

    /*we have a complete payload*/
    if ((parser_hanlde->buf_remain + buf_left) >= frame_size) {
        need_size = frame_size - (parser_hanlde->buf_remain);
        if (need_size >= 0) {
            new_buf_size = parser_hanlde->buf_remain + need_size;
            if (new_buf_size > parser_hanlde->buf_size) {
                parser_hanlde->buf = realloc(parser_hanlde->buf, new_buf_size);
                if (parser_hanlde->buf == NULL) {
                    ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                    parser_hanlde->buf_remain = 0;
                    parser_hanlde->status = PARSER_SYNCING;
                    goto error;
                }
                parser_hanlde->buf_size = new_buf_size;
                parser_buf = parser_hanlde->buf;
                ALOGI("%s realloc buf =%d", __func__, new_buf_size);
            }

            memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, need_size);
            buf_offset += need_size;
            buf_left = numBytes - buf_offset;

            *output_buf = (void*)(parser_buf);
            *out_size   = frame_size;
            *used_size = buf_offset;
            ALOGV("OK framesize =%d used size=%d loop_cnt=%d", frame_size, buf_offset, loop_cnt);
            /*one frame has complete, need find next one*/
            parser_hanlde->buf_remain = 0;
            parser_hanlde->status = PARSER_SYNCING;
        } else {
            /*internal buf has more data than framsize, we only need part of it*/
            *output_buf = (void*)(parser_buf);
            *out_size   = frame_size;
            /*move need_size bytes back to original buf*/
            *used_size = buf_offset + need_size;
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
            parser_hanlde->buf = realloc(parser_hanlde->buf, new_buf_size);
            if (parser_hanlde->buf == NULL) {
                ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                parser_hanlde->buf_remain = 0;
                parser_hanlde->status = PARSER_SYNCING;
                goto error;
            }
            parser_hanlde->buf_size = new_buf_size;
            parser_buf = parser_hanlde->buf;
            ALOGI("%s realloc buf =%d", __func__, new_buf_size);
        }
        memcpy(parser_buf + parser_hanlde->buf_remain, buffer + buf_offset, buf_left);
        parser_hanlde->buf_remain += buf_left;
        parser_hanlde->status = PARSER_LACK_DATA;
        goto error;
    }
    if (parser_hanlde->framesize != frame_size) {
        parser_hanlde->framesize = frame_size;
        ALOGI("New frame size =%d", frame_size);
    }
    return 0;

error:
    *output_buf = NULL;
    *out_size   = 0;
    *used_size = numBytes;
    return 0;
}
