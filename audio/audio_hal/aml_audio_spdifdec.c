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
#define LOG_TAG "audio_spdif_decoder"
//#define LOG_NDEBUG 0

#include <cutils/log.h>
#include "audio_hw.h"
#include "audio_format_parse.h"
#include "aml_audio_spdifdec.h"

#define IEC61937_HEADER_PA_LITTLE  0xF872
#define IEC61937_HEADER_PB_LITTLE  0x4E1F

#define IEC61937_HEADER_PA_BIG     0x72F8
#define IEC61937_HEADER_PB_BIG     0x1F4E


#define IEC61937_DEFAULT_SIZE (6144)
#define IEC61937_HEADER_SIZE  (8)
#define IEC61937_HEADER_COPY_SIZE (3)  /*the header is 4 bytes, if sync failed, we need at least copy 3 bytes*/
#define IEC61937_PA_OFFSET   (0)
#define IEC61937_PB_OFFSET   (2)
#define IEC61937_PC_OFFSET   (4)
#define IEC61937_PD_OFFSET   (6)
#define IEC61937_HEADER_SYNC_PERIOD  (8)    /*in this period there is no IEC frame, increase this can accelerate the sync speed*/
#define IEC61937_DTSHD_HEAD_SIZE  (12)


#define IEC61937_QUICK_SYNC

enum SPDIF_DEC_STATE {
    SPDIF_DEC_SYNCING,
    SPDIF_DEC_SYNCED,
    SPDIF_DEC_LACK_DATA,
};

struct aml_spdif_decoder {
    audio_format_t format;
    void * buf;
    int32_t buf_size;
    int32_t buf_remain;
    uint32_t status;
    int32_t payload_size;
};

int aml_spdif_decoder_open(void **spdifdec_handle)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = NULL;

    spdif_dec_hanlde = (struct aml_spdif_decoder *)aml_audio_calloc(1, sizeof(struct aml_spdif_decoder));
    if (spdif_dec_hanlde == NULL) {
        ALOGE("%s handle error", __func__);
        goto ERROR;
    }

    spdif_dec_hanlde->buf_size  = IEC61937_DEFAULT_SIZE;
    spdif_dec_hanlde->buf  = aml_audio_calloc(1, IEC61937_DEFAULT_SIZE);
    if (spdif_dec_hanlde->buf == NULL) {
        ALOGE("%s data buffer error", __func__);
        goto ERROR;
    }
    spdif_dec_hanlde->format = AUDIO_FORMAT_INVALID;
    spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;
    spdif_dec_hanlde->buf_remain = 0;
    spdif_dec_hanlde->payload_size = 0;
    *spdifdec_handle = spdif_dec_hanlde;
    ALOGI("%s exit =%p", __func__, spdif_dec_hanlde);
    return 0;
ERROR:
    if (spdif_dec_hanlde) {
        if (spdif_dec_hanlde->buf) {
            aml_audio_free(spdif_dec_hanlde->buf);
            spdif_dec_hanlde->buf = NULL;
        }
        aml_audio_free(spdif_dec_hanlde);
        spdif_dec_hanlde = NULL;
    }
    *spdifdec_handle = NULL;
    ALOGE("%s error", __func__);
    return -1;
}
int aml_spdif_decoder_close(void *phandle)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;

    if (spdif_dec_hanlde) {
        if (spdif_dec_hanlde->buf) {
            aml_audio_free(spdif_dec_hanlde->buf);
            spdif_dec_hanlde->buf = NULL;
        }
        aml_audio_free(spdif_dec_hanlde);
        spdif_dec_hanlde = NULL;
    }
    ALOGE("%s exit", __func__);
    return 0;
}

int aml_spdif_decoder_reset(void *phandle)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;

    if (spdif_dec_hanlde) {
        spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;
        spdif_dec_hanlde->buf_remain = 0;
        spdif_dec_hanlde->payload_size = 0;
    }
    ALOGE("%s exit", __func__);
    return 0;
}

/*
 *Find the position of 61937 sync word in the buffer, need PA/PB/PC/PD, 4*sizeof(short)
 */
static int seek_61937_sync_word(char *buffer, int size)
{
    int i = -1;
    if (size < 8) {
        return i;
    }

    for (i = 0; i < (size - 3); i++) {
        if (buffer[i + 0] == 0x72 && buffer[i + 1] == 0xf8 && buffer[i + 2] == 0x1f && buffer[i + 3] == 0x4e) {
            return i;
        }
        if (buffer[i + 0] == 0x4e && buffer[i + 1] == 0x1f && buffer[i + 2] == 0xf8 && buffer[i + 3] == 0x72) {
            return i;
        }
    }
    return -1;
}

static int16_t swap_int16(int16_t value)
{
    return ((value & 0x00FF) << 8) |
           ((value & 0xFF00) >> 8) ;
}


static int get_iec61937_info(void *phandle, void * buf, int32_t size, int32_t *package_size, int32_t *payload_size)
{
    uint16_t pa = 0;
    uint16_t pb = 0;
    uint16_t pc = 0;
    uint16_t pd = 0;
    uint32_t big_endian = 0;
    uint8_t data_type = 0;
    uint32_t tmp = 0;
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;

    if (size < IEC61937_HEADER_SIZE) {
        return -1;
    }
    pa = *(uint16_t*)((uint8_t*)buf + IEC61937_PA_OFFSET);
    pb = *(uint16_t*)((uint8_t*)buf + IEC61937_PB_OFFSET);
    pc = *(uint16_t*)((uint8_t*)buf + IEC61937_PC_OFFSET);
    pd = *(uint16_t*)((uint8_t*)buf + IEC61937_PD_OFFSET);

    if (pa == IEC61937_HEADER_PA_LITTLE && pb == IEC61937_HEADER_PB_LITTLE) {
        big_endian = 0;
    } else if (pa == IEC61937_HEADER_PA_BIG && pb == IEC61937_HEADER_PB_BIG) {
        big_endian = 1;
        pc = swap_int16(pc);
        pd = swap_int16(pd);
    } else {
        ALOGE("It is not IEC Sync PA=0x%x PB=0x%x", pa, pb);
        return -1;
    }


    /*Data type defined in PC bits 0-6 in IEC 61937-1*/
    data_type = pc & 0x3f;

    switch (data_type) {
        case IEC61937_AC3:
        {
            *package_size = AC3_PERIOD_SIZE;
            /*length code is in bits*/
            *payload_size = pd >> 3;
            spdif_dec_hanlde->format = AUDIO_FORMAT_AC3;
            break;
        }
        case IEC61937_EAC3:
        {
            *package_size = EAC3_PERIOD_SIZE;
            /*length code is in bytes*/
            *payload_size = pd;
            spdif_dec_hanlde->format = AUDIO_FORMAT_E_AC3;
            break;
        }
        case IEC61937_DTS1:
        {
            *package_size = DTS1_PERIOD_SIZE;
            /*length code is in bits*/
            *payload_size = pd >> 3;
            spdif_dec_hanlde->format = AUDIO_FORMAT_DTS;
            break;
        }
        case IEC61937_DTS2:
        {
            *package_size = DTS2_PERIOD_SIZE;
            /*length code is in bits*/
            *payload_size = pd >> 3;
            spdif_dec_hanlde->format = AUDIO_FORMAT_DTS;
            break;
        }
        case IEC61937_DTS3:
        {
            *package_size = DTS3_PERIOD_SIZE;
            /*length code is in bits*/
            *payload_size = pd >> 3;
            spdif_dec_hanlde->format = AUDIO_FORMAT_DTS;
            break;
        }
        case IEC61937_DTSHD:
        {
            /*Value of 8-12bit is framesize*/
            tmp = (pc & 0x7ff) >> 8;
            /*refer to IEC 61937-5 pdf, table 6*/
            *package_size = DTSHD_PERIOD_SIZE << tmp ;
            spdif_dec_hanlde->format = AUDIO_FORMAT_DTS_HD;
            break;
        }
        case IEC61937_MAT:
        {
            *package_size = MAT_PERIOD_SIZE;
            /*length code is in bytes*/
            *payload_size = pd;
            spdif_dec_hanlde->format = AUDIO_FORMAT_MAT;
            break;
        }
        default:
        {
            *package_size = 0;
            spdif_dec_hanlde->format = AUDIO_FORMAT_INVALID;
            ALOGE("unsupport iec61937 PC =0x%x PD=0x%x", pc, pd);
            return -1;
        }
    }

    return 0;
}

/*refer kodi how to add 12 bytes header for DTS HD
01 00 00 00 00 00 00 00 fe fe ** **, last 2 bytes for data size
*/
static int get_dtshd_info(void * buf, int32_t size, int32_t *payload_size)
{
    char *read_pointer = (char*)buf + IEC61937_HEADER_SIZE;
    if (size < (IEC61937_HEADER_SIZE + IEC61937_DTSHD_HEAD_SIZE)) {
        *payload_size = 0;
        return -1;
    }

    if (read_pointer[0] == 0x00 && read_pointer[1] == 0x01 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe) {
        *payload_size = (read_pointer[10] | read_pointer[11] << 8);
    } else if ((read_pointer[0] == 0x01 && read_pointer[1] == 0x00 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe)) {
        *payload_size = (read_pointer[11] | read_pointer[10] << 8);
    } else {
        ALOGE("DTS HD error data\n");
        *payload_size = 0;
        return -1;
    }

    return 0;
}

static int aml_spdif_decoder_do_quick_sync
    (void *phandle
    , const void *inbuf
    , int32_t n_bytes_inbuf
    , int32_t *sync_word_offset
    , int32_t *buf_left
    , int32_t *buf_offset)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;
    uint8_t *spdifdec_buf = NULL;
    bool is_quick_sync_suitable = false;
    uint8_t *buffer = (uint8_t *)inbuf;

    if (!spdif_dec_hanlde || !inbuf || !sync_word_offset || !buf_left || !buf_offset) {
        ALOGE("%s line %d spdif_dec_hanlde %p inbuf %p sync_word_offset %p buf_left %p buf_offset %p\n",
            __func__, __LINE__, spdif_dec_hanlde, inbuf, sync_word_offset, buf_left, buf_offset);
        return -1;
    }

    spdifdec_buf = spdif_dec_hanlde->buf;
    *buf_left = n_bytes_inbuf;

    /*if we have enough data and we are doing syncing, we can sync the header quickly in origial buf*/
    is_quick_sync_suitable = ((spdif_dec_hanlde->buf_remain == 0) &&
                            (spdif_dec_hanlde->status == SPDIF_DEC_SYNCING) &&
                            (n_bytes_inbuf >= IEC61937_HEADER_SIZE));

    if (is_quick_sync_suitable) {
        *sync_word_offset = seek_61937_sync_word((char*)buffer, n_bytes_inbuf);
        /*to avoid the case the sync word accross the input buf, we need copy the last 3 bytes*/
        if (*sync_word_offset < 0) {
            memcpy( spdifdec_buf, buffer + n_bytes_inbuf - IEC61937_HEADER_COPY_SIZE, IEC61937_HEADER_COPY_SIZE);
            spdif_dec_hanlde->buf_remain += IEC61937_HEADER_COPY_SIZE;
            return -1;
        }

        /*we have find the sync word, just copy the papb part*/
        memcpy(spdifdec_buf , buffer + *sync_word_offset, IEC61937_HEADER_SIZE / 2);
        spdif_dec_hanlde->buf_remain += IEC61937_HEADER_SIZE / 2;
        *buf_offset += *sync_word_offset + IEC61937_HEADER_SIZE / 2;
        *buf_left = n_bytes_inbuf - *buf_offset;
        *sync_word_offset = 0;
    }

    return 0;
}

static int aml_spdif_decoder_find_syncword(void *phandle
    , const void *inbuf
    , int32_t n_bytes_inbuf
    , int32_t *sync_word_offset
    , int32_t *buf_left
    , int32_t *buf_offset
    , uint8_t **spdifdec_buf)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;
    int32_t loop_cnt = 0;
    int32_t data_valid = 0;
    int32_t need_size = 0;
    uint8_t *buffer = (uint8_t *)inbuf;

    if (!spdif_dec_hanlde || !inbuf || !sync_word_offset || !buf_left || !buf_offset) {
        ALOGE("%s line %d spdif_dec_hanlde %p inbuf %p sync_word_offset %p buf_left %p buf_offset %p\n",
            __func__, __LINE__, spdif_dec_hanlde, inbuf, sync_word_offset, buf_left, buf_offset);
        return -1;
    }

    /*we need at least period bytes*/
    if (spdif_dec_hanlde->buf_remain < IEC61937_HEADER_SYNC_PERIOD) {
        need_size = IEC61937_HEADER_SYNC_PERIOD - spdif_dec_hanlde->buf_remain;
        /*input data is not enough, just copy to internal buf*/
        if (*buf_left < need_size) {
            memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, *buf_left);
            spdif_dec_hanlde->buf_remain += *buf_left;
            goto ERROR;
        }
        /*make sure the remain buf has period bytes*/
        memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, need_size);
        spdif_dec_hanlde->buf_remain += need_size;
        *buf_offset += need_size;
        *buf_left = n_bytes_inbuf - *buf_offset;

    }

    if (spdif_dec_hanlde->status == SPDIF_DEC_SYNCING) {
        *sync_word_offset = -1;
        while (*sync_word_offset < 0) {
            /*sync the header, we have at least period bytes*/
            if (spdif_dec_hanlde->buf_remain < IEC61937_HEADER_SYNC_PERIOD) {
                ALOGE("we should not get there");
                goto DO_SYNC;
            }
            *sync_word_offset = seek_61937_sync_word((char*)*spdifdec_buf, spdif_dec_hanlde->buf_remain);
            /*if we don't find the header in period bytes, move the last 3 bytes to header*/
            if (*sync_word_offset < 0) {
                memmove(*spdifdec_buf, *spdifdec_buf + spdif_dec_hanlde->buf_remain - IEC61937_HEADER_COPY_SIZE, IEC61937_HEADER_COPY_SIZE);
                spdif_dec_hanlde->buf_remain = IEC61937_HEADER_COPY_SIZE;
                need_size = IEC61937_HEADER_SYNC_PERIOD - spdif_dec_hanlde->buf_remain;
                /*input data is not enough, just copy to internal buf*/
                if (*buf_left < need_size) {
                    memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, *buf_left);
                    spdif_dec_hanlde->buf_remain += *buf_left;
                    /*don't find the header, and there is no enough data*/
                    goto ERROR;
                }
                /*make the spdif dec buf has period bytes*/
                memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, need_size);
                spdif_dec_hanlde->buf_remain += need_size;
                *buf_offset += need_size;
                *buf_left = n_bytes_inbuf - *buf_offset;
            }
            loop_cnt++;
        }
        /*got here means we find the sync word*/
        spdif_dec_hanlde->status = SPDIF_DEC_SYNCED;

        data_valid = spdif_dec_hanlde->buf_remain - *sync_word_offset;
        /*move the header to the beginning of buf*/
        if (*sync_word_offset != 0) {
            memmove(*spdifdec_buf, *spdifdec_buf + *sync_word_offset, data_valid);
        }
        spdif_dec_hanlde->buf_remain = data_valid;

        need_size = IEC61937_HEADER_SYNC_PERIOD - data_valid;
        /*get some bytes to make sure it is at least period bytes*/
        if (need_size > 0) {
            /*check if input has enough data*/
            if (*buf_left < need_size) {
                memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, *buf_left);
                spdif_dec_hanlde->buf_remain += *buf_left;
                goto ERROR;
            }
            /*make sure the remain buf has period bytes*/
            memcpy(*spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset , need_size);
            spdif_dec_hanlde->buf_remain += need_size;
            *buf_offset += need_size;
            *buf_left = n_bytes_inbuf - *buf_offset;
        }
    }

    /*double check here*/
    *sync_word_offset = seek_61937_sync_word((char*)*spdifdec_buf, spdif_dec_hanlde->buf_remain);
    if (*sync_word_offset != 0) {
        ALOGE("we can't get here remain=%d,resync iec61937 header", spdif_dec_hanlde->buf_remain);
        goto DO_SYNC;
    }

    return 0;

DO_SYNC:
    spdif_dec_hanlde->buf_remain = 0;
    spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;

ERROR:
    return -1;
}

static int aml_spdif_decoder_addbytes
    (void *phandle
    , const void *inbuf
    , int32_t n_bytes_inbuf
    , uint8_t *spdifdec_buf
    , int32_t *buf_left
    , int32_t *buf_offset
    , int32_t *used_size
    , void **output_buf
    , int32_t *out_size)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;
    int32_t need_size = 0;
    uint8_t *buffer = (uint8_t *)inbuf;
    int32_t package_size = 0;
    int32_t payload_size = 0;
    int32_t new_buf_size = 0;
    int ret = 0;

    if (!spdif_dec_hanlde || !inbuf || !spdifdec_buf || !buf_left || !buf_offset || !used_size || !out_size) {
        ALOGE("%s line %d spdif_dec_hanlde %p inbuf %p spdifdec_buf %p buf_left %p buf_offset %p used_size %p out_size  %p\n",
            __func__, __LINE__, spdif_dec_hanlde, inbuf, spdifdec_buf, buf_left, buf_offset, used_size, out_size);
        return -1;
    }

    /* we got here means we find the IEC header and
     * it is at the beginning of spedif dec buf and
     * it has at least period bytes, we can get the pc/pd
     */
    ret = get_iec61937_info(phandle, spdifdec_buf, IEC61937_HEADER_SIZE, &package_size, &payload_size);
    if (ret != 0) {
        goto DO_SYNC;
    }

    /*if it is DTS HD, we need more 12 bytes to calculate the payload size*/
    if (spdif_dec_hanlde->format == AUDIO_FORMAT_DTS_HD) {
        need_size = (IEC61937_HEADER_SIZE + IEC61937_DTSHD_HEAD_SIZE) - spdif_dec_hanlde->buf_remain;
        if (need_size > 0) {
            /*if there is no enough data, try to get more*/
            if (*buf_left < need_size) {
                memcpy(spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, *buf_left);
                spdif_dec_hanlde->buf_remain += *buf_left;
                spdif_dec_hanlde->status = SPDIF_DEC_LACK_DATA;
                goto ERROR;
            }
            memcpy(spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, need_size);
            spdif_dec_hanlde->buf_remain += need_size;
            *buf_offset += need_size;
            *buf_left = n_bytes_inbuf - *buf_offset;
        }

        ret = get_dtshd_info(spdifdec_buf, (IEC61937_HEADER_SIZE + IEC61937_DTSHD_HEAD_SIZE), &payload_size);
        if (ret != 0) {
            goto DO_SYNC;
        }
    }

    /*check whether the input data has a complete payload*/
    if (payload_size == 0) {
        ALOGE("%s wrong format=%d package size =%d payload =%d", __func__, spdif_dec_hanlde->format, package_size, payload_size);
        goto DO_SYNC;
    }

    /*we have a complete payload*/
    if ((spdif_dec_hanlde->buf_remain - IEC61937_HEADER_SIZE + *buf_left) >= payload_size) {
        need_size = payload_size - (spdif_dec_hanlde->buf_remain - IEC61937_HEADER_SIZE);
        if (need_size >= 0) {
            new_buf_size = spdif_dec_hanlde->buf_remain + need_size;
            if (new_buf_size > spdif_dec_hanlde->buf_size) {
                spdif_dec_hanlde->buf = aml_audio_realloc(spdif_dec_hanlde->buf, new_buf_size);
                if (spdif_dec_hanlde->buf == NULL) {
                    ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                    goto DO_SYNC;
                }
                ALOGI("%s realloc buf =%d", __func__, new_buf_size);
                spdifdec_buf = spdif_dec_hanlde->buf;
                spdif_dec_hanlde->buf_size = new_buf_size;
            }

            memcpy(spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, need_size);
            *buf_offset += need_size;
            *buf_left = n_bytes_inbuf - *buf_offset;
            /*we may have some stuffing data, we can skip it*/
            if (*buf_left > 0) {
                int32_t stuff_size = package_size - IEC61937_HEADER_SIZE - payload_size;
                if (stuff_size < 0) {
                    ALOGE("%s we can't get here suff size =%d", __func__, stuff_size);
                    goto DO_SYNC;
                }
                if (*buf_left >= stuff_size) {
                    *buf_offset += stuff_size;
                } else {
                    *buf_offset += *buf_left;
                }
            }
            *output_buf = (void*)(spdifdec_buf + IEC61937_HEADER_SIZE);
            *out_size   = payload_size;
            *used_size = *buf_offset;
            ALOGV("OK type=0x%x payload size=%d used size=%d\n", spdif_dec_hanlde->format, payload_size, *buf_offset);
            /*one frame has complete, need find next one*/
            spdif_dec_hanlde->buf_remain = 0;
            spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;
        }else {
            /*internal buf has more data than payload, we only need part of it*/
            *output_buf = (void*)(spdifdec_buf + IEC61937_HEADER_SIZE);
            *out_size   = payload_size;
            /*move need_size bytes back to original buf*/
            *used_size = *buf_offset + need_size;
            ALOGV("wrap data type=0x%x payload size=%d used size=%d back size =%d\n", spdif_dec_hanlde->format, payload_size, *buf_offset, need_size);
            if (*used_size <= 0) {
                ALOGE("%s wrong used size =%d", __func__, *used_size);
                goto DO_SYNC;
            }
            /*one frame has complete, need find next one*/
            spdif_dec_hanlde->buf_remain = 0;
            spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;
        }
    } else {
        /*check whether the input buf size is big enough*/
        new_buf_size = spdif_dec_hanlde->buf_remain + *buf_left;
        if (new_buf_size > spdif_dec_hanlde->buf_size) {
            spdif_dec_hanlde->buf = aml_audio_realloc(spdif_dec_hanlde->buf, new_buf_size);
            if (spdif_dec_hanlde->buf == NULL) {
                ALOGE("%s realloc buf failed =%d", __func__, new_buf_size);
                goto DO_SYNC;
            }
            ALOGI("%s realloc buf =%d", __func__, new_buf_size);
            spdifdec_buf = spdif_dec_hanlde->buf;
            spdif_dec_hanlde->buf_size = new_buf_size;
        }
        memcpy(spdifdec_buf + spdif_dec_hanlde->buf_remain, buffer + *buf_offset, *buf_left);
        spdif_dec_hanlde->buf_remain += *buf_left;
        spdif_dec_hanlde->status = SPDIF_DEC_LACK_DATA;
        goto ERROR;
    }
    if (spdif_dec_hanlde->payload_size != payload_size) {
        spdif_dec_hanlde->payload_size = payload_size;
        ALOGV("IEC61937 format =0x%x package size=%d payload_size=%d", spdif_dec_hanlde->format, package_size, payload_size);
    }
    return 0;


DO_SYNC:
    spdif_dec_hanlde->buf_remain = 0;
    spdif_dec_hanlde->status = SPDIF_DEC_SYNCING;

ERROR:
    return -1;

}



int aml_spdif_decoder_process(void *phandle, const void *inbuf, int32_t n_bytes_inbuf, int32_t *used_size, void **output_buf, int32_t *out_size)
{
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;
    uint8_t *spdifdec_buf = NULL;
    int32_t sync_word_offset = -1;
    int32_t buf_left = 0;
    int32_t buf_offset = 0;
    int32_t ret = 0;

    if (!spdif_dec_hanlde || !inbuf) {
        goto ERROR;
    }

    spdifdec_buf = spdif_dec_hanlde->buf;
    buf_left = n_bytes_inbuf;

    ALOGV("%s input buf size=%d status=%d", __func__, n_bytes_inbuf, spdif_dec_hanlde->status);

    if (aml_spdif_decoder_do_quick_sync(phandle, inbuf, n_bytes_inbuf, &sync_word_offset, &buf_left, &buf_offset)) {
        ALOGV("%s aml_spdif_decoder_do_quick_sync error return status=%d", __func__, spdif_dec_hanlde->status);
        goto ERROR;
    }

    ret = aml_spdif_decoder_find_syncword(phandle, inbuf, n_bytes_inbuf, &sync_word_offset, &buf_left, &buf_offset, &spdifdec_buf);
    if (ret) {
        ALOGV("%s aml_spdif_decoder_find_syncword return %d status=%d", __func__, ret, spdif_dec_hanlde->status);
        goto ERROR;
    }

    ret = aml_spdif_decoder_addbytes(phandle, inbuf, n_bytes_inbuf, spdifdec_buf, &buf_left, &buf_offset, used_size, output_buf, out_size);
    ALOGV("%s aml_spdif_decoder_addbytes return %d status=%d", __func__, ret, spdif_dec_hanlde->status);
    if (ret == 0) {
        return 0;
    }


ERROR:
    *output_buf = NULL;
    *out_size   = 0;
    *used_size = n_bytes_inbuf;
    return 0;
}

int aml_spdif_decoder_getformat(void *phandle) {
    struct aml_spdif_decoder *spdif_dec_hanlde = (struct aml_spdif_decoder *)phandle;
    if (spdif_dec_hanlde == NULL) {
        return -1;
    }

    return (int)spdif_dec_hanlde->format;
}
