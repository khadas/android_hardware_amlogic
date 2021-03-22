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

#define LOG_TAG "aml_audio_dca_dec"
//#define LOG_NDEBUG 0

#include <unistd.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <sound/asound.h>
#include <cutils/log.h>
#include <tinyalsa/asoundlib.h>

#include "audio_hw_utils.h"
#include "aml_dca_dec_api.h"
#include "aml_audio_resample_manager.h"

#define DOLBY_DTSHD_LIB_PATH     "/odm/lib/libHwAudio_dtshd.so"

enum {
    EXITING_STATUS = -1001,
    NO_ENOUGH_DATA = -1002,
};
#define MAX_DECODER_FRAME_LENGTH 32768
#define READ_PERIOD_LENGTH 2048 * 4
#define DTS_TYPE_I     0xB
#define DTS_TYPE_II    0xC
#define DTS_TYPE_III   0xD
#define DTS_TYPE_IV    0x11

#define IEC61937_HEADER_LENGTH  8
#define IEC_DTS_HD_APPEND_LNGTH 12
#define IEC61937_PA_OFFSET  0
#define IEC61937_PA_SIZE    2
#define IEC61937_PB_OFFSET  2
#define IEC61937_PB_SIZE    2
#define IEC61937_PC_OFFSET  4
#define IEC61937_PC_SIZE    2
#define IEC61937_PD_OFFSET  6
#define IEC61937_PD_SIZE    2

#define AML_DCA_SW_CORE_16M                         0x7ffe8001
#define AML_DCA_SW_CORE_14M                         0x1fffe800
#define AML_DCA_SW_CORE_24M                         0xfe80007f
#define AML_DCA_SW_CORE_16                          0xfe7f0180
#define AML_DCA_SW_CORE_14                          0xff1f00e8
#define AML_DCA_SW_CORE_24                          0x80fe7f01
#define AML_DCA_SW_SUBSTREAM_M                      0x64582025
#define AML_DCA_SW_SUBSTREAM                        0x58642520
#define AML_DCA_PROP_DEBUG_FLAG                     "vendor.media.audio.dtsdebug"
#define AML_DCA_PROP_DUMP_INPUT_RAW                 "vendor.media.audio.dtsdump.input.raw"
#define AML_DCA_PROP_DUMP_OUTPUT_PCM                "vendor.media.audio.dtsdump.output.pcm"
#define AML_DCA_PROP_DUMP_OUTPUT_RAW                "vendor.media.audio.dtsdump.output.raw"
#define AML_DCA_DUMP_FILE_DIR                       "/data/vendor/audiohal/"

struct dca_dts_debug {
    bool debug_flag;
    FILE* fp_pcm;
    FILE* fp_input_raw;
    FILE* fp_output_raw;
};
static struct dca_dts_debug dts_debug = {0};

///static struct pcm_info pcm_out_info;
/*dts decoder lib function*/
int (*dts_decoder_init)(int, int);
int (*dts_decoder_cleanup)();
int (*dts_decoder_process)(char * , int , int *, char *, int *, struct pcm_info *, char *, int *);
void *gDtsDecoderLibHandler = NULL;
static int _dts_syncword_scan(unsigned char *read_pointer, unsigned int *pTemp0);
static int _dts_frame_scan(struct dca_dts_dec *dts_dec);
static int _dts_pcm_output(struct dca_dts_dec *dts_dec);
static int _dts_raw_output(struct dca_dts_dec *dts_dec);


static int _dts_syncword_scan(unsigned char *read_pointer, unsigned int *pTemp0)
{
    unsigned int ui32Temp0 = 0;
    unsigned int ui32Temp1 = 0;

    ui32Temp0  = read_pointer[0];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[1];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[2];
    ui32Temp0 <<= 8;
    ui32Temp0 |= read_pointer[3];

    ui32Temp1  = read_pointer[4];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[5];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[6];
    ui32Temp1 <<= 8;
    ui32Temp1 |= read_pointer[7];

    /* 16-bit bit core stream*/
    if ( ui32Temp0 == AML_DCA_SW_CORE_16  || ui32Temp0 == AML_DCA_SW_CORE_14 ||
         ui32Temp0 == AML_DCA_SW_CORE_16M || ui32Temp0 == AML_DCA_SW_CORE_14M ||
         ui32Temp0 == AML_DCA_SW_SUBSTREAM|| ui32Temp0 ==AML_DCA_SW_SUBSTREAM_M)
    {
        *pTemp0 = ui32Temp0;
        return 1;
    }

    if ((ui32Temp0 & 0xffffff00) == (AML_DCA_SW_CORE_24 & 0xffffff00) &&
       ((ui32Temp1 >> 16) & 0xFF)== (AML_DCA_SW_CORE_24 & 0xFF)) {
        *pTemp0 = ui32Temp0;
        return 1;
    }

    return 0;
}

static int _dts_frame_scan(struct dca_dts_dec *dts_dec)
{
    int frame_size = 0;
    int unuse_size = 0;
    unsigned char *read_pointer = NULL;
    struct ring_buffer* input_rbuffer = &dts_dec->input_ring_buf;
    struct dts_frame_info* frame_info = &dts_dec->frame_info;

    read_pointer = input_rbuffer->start_addr + frame_info->check_pos;
    if (read_pointer <= input_rbuffer->wr) {
        unuse_size = input_rbuffer->wr - read_pointer;
    } else {
        unuse_size = input_rbuffer->size + input_rbuffer->wr - read_pointer;
    }

    //ALOGD("remain :%d, unuse_size:%d, is_dts:%d, is_iec61937:%d"
    //    , dts_dec->remain_size, unuse_size, frame_info->is_dtscd, frame_info->is_iec61937);
    if (dts_dec->is_dtscd) {
        frame_size = dts_dec->remain_size;
    } else if (dts_dec->is_iec61937) {
        int drop_size = 0;
        if (!frame_info->syncword || (frame_info->size <= 0)) {
            bool syncword_flag = false;

            //DTS_SYNCWORD_IEC61937 : 0xF8724E1F
            while (!syncword_flag && (unuse_size > IEC61937_HEADER_LENGTH)) {
                if (read_pointer[0] == 0x72 && read_pointer[1] == 0xf8
                    && read_pointer[2] == 0x1f && read_pointer[3] == 0x4e) {
                    syncword_flag = true;
                    frame_info->iec61937_data_type = read_pointer[4] & 0x1f;
                } else if (read_pointer[0] == 0xf8 && read_pointer[1] == 0x72
                           && read_pointer[2] == 0x4e && read_pointer[3] == 0x1f) {
                    syncword_flag = true;
                    frame_info->is_little_endian = true;
                    frame_info->iec61937_data_type = read_pointer[5] & 0x1f;
                }

                if (syncword_flag) {
                    if ((frame_info->iec61937_data_type == DTS_TYPE_I)
                        || (frame_info->iec61937_data_type == DTS_TYPE_II)
                        || (frame_info->iec61937_data_type == DTS_TYPE_III)
                        || (frame_info->iec61937_data_type == DTS_TYPE_IV)) {
                        frame_info->syncword = 0xF8724E1F;
                    } else {
                        syncword_flag = false;
                    }
                }

                if (!syncword_flag) {
                    read_pointer++;
                    unuse_size--;
                    if (read_pointer > (input_rbuffer->start_addr + input_rbuffer->size)) {
                        read_pointer = input_rbuffer->start_addr;
                    }
                }
            }

            //ALOGD("DTS Sync=%d, little endian=%d, dts type=0x%x\n", syncword_flag, frame_info->is_little_endian, frame_info->iec61937_data_type);
            if (syncword_flag) {
                // point to pd
                read_pointer = read_pointer + IEC61937_PD_OFFSET;
                //ALOGD("read_pointer[0]:0x%x read_pointer[1]:0x%x",read_pointer[0],read_pointer[1]);
                if (frame_info->is_little_endian) {
                    frame_size = (read_pointer[1] | read_pointer[0] << 8);
                } else {
                    frame_size = (read_pointer[0] | read_pointer[1] << 8);
                }

                if (frame_info->iec61937_data_type == DTS_TYPE_I ||
                    frame_info->iec61937_data_type == DTS_TYPE_II ||
                    frame_info->iec61937_data_type == DTS_TYPE_III) {
                    // these DTS type use bits length for PD
                    frame_size = frame_size >> 3;
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                } else if ((frame_info->iec61937_data_type == DTS_TYPE_IV)
                            && (unuse_size > IEC_DTS_HD_APPEND_LNGTH + IEC61937_HEADER_LENGTH)) {
                    /*refer kodi how to add 12 bytes header for DTS HD
                    01 00 00 00 00 00 00 00 fe fe ** **, last 2 bytes for data size
                    */
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                    if (read_pointer[0] == 0x00 && read_pointer[1] == 0x01 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe) {
                        frame_size = (read_pointer[10] | read_pointer[11] << 8);
                    } else if ((read_pointer[0] == 0x01 && read_pointer[1] == 0x00 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe)) {
                        frame_size = (read_pointer[11] | read_pointer[10] << 8);
                    } else {
                        ALOGE("DTS HD error data\n");
                        frame_size = 0;
                    }
                    //ALOGD("size data=0x%x 0x%x\n",read_pointer[10],read_pointer[11]);
                    // point to the address after 12 bytes header
                    read_pointer = read_pointer + IEC_DTS_HD_APPEND_LNGTH;
                }
            }

            if (read_pointer >= input_rbuffer->rd) {
                drop_size = read_pointer - input_rbuffer->rd;
            } else {
                drop_size = input_rbuffer->size + read_pointer - input_rbuffer->rd;
            }

            if (drop_size > 0) {
                ring_buffer_seek(input_rbuffer, drop_size);
                dts_dec->remain_size -= drop_size;
            }
        }else {
            frame_size = frame_info->size;
        }

        //to do know why
        if (frame_size == 2013) {
            frame_size = 2012;
        }

        //ALOGD("remain:%d, frame:%d, read:%p, rd:%p, drop:%d"
        //    , dts_dec->remain_size, frame_size, read_pointer, input_rbuffer->rd, drop_size);

        frame_info->check_pos = read_pointer - input_rbuffer->start_addr;
        frame_info->size = frame_size;
        if ((dts_dec->remain_size >= frame_size) && (frame_size > 0)) {
            frame_info->syncword = 0;
            frame_info->check_pos += frame_size;
            if (frame_info->check_pos >= input_rbuffer->size) {
                frame_info->check_pos -= input_rbuffer->size;
            }
        } else {
            frame_size = 0;
        }
    } else {
        unsigned int syncword = 0;
        unsigned int check_size = 0;
        int tmp_syncword_pos = -1;
        while ((frame_size <= 0) && (unuse_size > IEC61937_HEADER_LENGTH)) {
            if (_dts_syncword_scan(read_pointer, &syncword)) {
                tmp_syncword_pos = read_pointer - input_rbuffer->start_addr;
                if (syncword == frame_info->syncword) {
                    if (tmp_syncword_pos >= frame_info->syncword_pos) {
                        frame_size = tmp_syncword_pos - frame_info->syncword_pos;
                    } else {
                        frame_size = input_rbuffer->size + tmp_syncword_pos - frame_info->syncword_pos;
                    }
                    frame_info->syncword_pos = tmp_syncword_pos;
                    frame_info->syncword = syncword;
                } else if (!frame_info->syncword) {
                    frame_info->syncword_pos = tmp_syncword_pos;
                    frame_info->syncword = syncword;
                }
                //ALOGD("syncword :0x%x, syncword_pos:%d", frame_info->syncword, frame_info->syncword_pos);
            }

            unuse_size--;
            read_pointer++;
            if (read_pointer > (input_rbuffer->start_addr + input_rbuffer->size)) {
                read_pointer = input_rbuffer->start_addr;
            }
        }

        frame_info->check_pos = read_pointer - input_rbuffer->start_addr;
        if (frame_info->check_pos >= frame_info->syncword_pos) {
            check_size = frame_info->check_pos - frame_info->syncword_pos;
        } else {
            check_size = input_rbuffer->size + frame_info->check_pos - frame_info->syncword_pos;
        }

        //ALOGD("check_pos:%d, syncword_pos:%d, read_pointer:%p, check_size:%d"
        //    , frame_info->check_pos, frame_info->syncword_pos, read_pointer, check_size);

        // no valid frame was found beyond size of MAX_DECODER_FRAME_LENGTH, meybe it is dirty data, so drop it
        if ((frame_size <= 0) && (check_size >= MAX_DECODER_FRAME_LENGTH)) {
            ring_buffer_seek(input_rbuffer, check_size);
            dts_dec->remain_size -= check_size;
            frame_info->syncword_pos = 0;
            frame_info->syncword = 0;
        }
    }

    if (dts_debug.debug_flag) {
        ALOGD("%s remain size:%d, frame size:%d, is dtscd:%d, is iec61937:%d"
            ,__func__, dts_dec->remain_size, frame_size, dts_dec->is_dtscd, dts_dec->is_iec61937);
    }

    return frame_size;
}

static int _dts_pcm_output(struct dca_dts_dec *dts_dec)
{
    int ret = 0;
    aml_dec_t *aml_dec = (aml_dec_t *)dts_dec;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    unsigned char* copy_buffer = dec_pcm_data->buf;
    int copy_size = dts_dec->outlen_pcm;
#if 0
    if (dts_dec->pcm_out_info.sample_rate > 0 && dts_dec->pcm_out_info.sample_rate != 48000) {
        if (dts_dec->resample_handle) {
            if (dts_dec->pcm_out_info.sample_rate != (int)dts_dec->resample_handle->resample_config.input_sr) {
                audio_resample_config_t resample_config;
                ALOGD("Sample rate is changed from %d to %d, reset the resample\n",dts_dec->resample_handle->resample_config.input_sr, dts_dec->pcm_out_info.sample_rate);
                aml_audio_resample_close(dts_dec->resample_handle);
                dts_dec->resample_handle = NULL;
            }
        }

        if (dts_dec->resample_handle == NULL) {
            audio_resample_config_t resample_config;
            ALOGI("init resampler from %d to 48000!, channel num = %d\n",
                dts_dec->pcm_out_info.sample_rate, dts_dec->pcm_out_info.channel_num);
            resample_config.aformat   = AUDIO_FORMAT_PCM_16_BIT;
            resample_config.channels  = dts_dec->pcm_out_info.channel_num;
            resample_config.input_sr  = dts_dec->pcm_out_info.sample_rate;
            resample_config.output_sr = 48000;
            ret = aml_audio_resample_init((aml_audio_resample_t **)&dts_dec->resample_handle, AML_AUDIO_ANDROID_RESAMPLE, &resample_config);
            if (ret < 0) {
                ALOGE("resample init error\n");
                return -1;
            }
        }

        ret = aml_audio_resample_process(dts_dec->resample_handle, dec_pcm_data->buf, dts_dec->outlen_pcm);
        if (ret < 0) {
            ALOGE("resample process error\n");
            return -1;
        }
        copy_buffer = dts_dec->resample_handle->resample_buffer;
        copy_size = dts_dec->resample_handle->resample_size;

        if (copy_buffer && ((0 <  copy_size) && (copy_size < dec_pcm_data->buf_size))) {
            memcpy(dec_pcm_data->buf, copy_buffer, copy_size);
        }
    }

    if (dts_debug.fp_pcm) {
        fwrite(copy_buffer, 1, copy_size, dts_debug.fp_pcm);
    }
#endif
    dec_pcm_data->data_format = AUDIO_FORMAT_PCM_16_BIT;
    dec_pcm_data->data_ch = dts_dec->pcm_out_info.channel_num;
    dec_pcm_data->data_sr = dts_dec->pcm_out_info.sample_rate;
    dec_pcm_data->data_len = dts_dec->outlen_pcm;

    return 0;
}

static int _dts_raw_output(struct dca_dts_dec *dts_dec)
{
    aml_dec_t *aml_dec = (aml_dec_t *)dts_dec;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    if (dts_debug.fp_output_raw) {
        fwrite(dec_raw_data->buf, 1, dts_dec->outlen_raw, dts_debug.fp_output_raw);
    }

    dec_raw_data->data_format = AUDIO_FORMAT_IEC61937;
    if (aml_dec->format == AUDIO_FORMAT_DTS_HD && (dts_dec->digital_raw == AML_DEC_CONTROL_CONVERT)) {
        dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
    } else {
        dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
    }
    dec_raw_data->data_ch = 2;
    dec_raw_data->data_sr = dts_dec->pcm_out_info.sample_rate;
    dec_raw_data->data_len = dts_dec->outlen_raw;
    return 0;
}


static int unload_dts_decoder_lib()
{
    if (dts_decoder_cleanup != NULL) {
        (*dts_decoder_cleanup)();
    }
    dts_decoder_init = NULL;
    dts_decoder_process = NULL;
    dts_decoder_cleanup = NULL;
    if (gDtsDecoderLibHandler != NULL) {
        dlclose(gDtsDecoderLibHandler);
        gDtsDecoderLibHandler = NULL;
    }
    return 0;
}

static int dca_decoder_init(aml_dec_control_type_t digital_raw)
{
    gDtsDecoderLibHandler = dlopen(DOLBY_DTSHD_LIB_PATH, RTLD_NOW);
    if (!gDtsDecoderLibHandler) {
        ALOGE("%s, failed to open (libstagefright_soft_dtshd.so), %s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[gDtsDecoderLibHandler]", __FUNCTION__, __LINE__);
    }

    dts_decoder_init = (int (*)(int, int)) dlsym(gDtsDecoderLibHandler, "dca_decoder_init");
    if (dts_decoder_init == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_init:]", __FUNCTION__, __LINE__);
    }

    dts_decoder_process = (int (*)(char * , int , int *, char *, int *, struct pcm_info *, char *, int *))
                          dlsym(gDtsDecoderLibHandler, "dca_decoder_process");
    if (dts_decoder_process == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_process:]", __FUNCTION__, __LINE__);
    }

    dts_decoder_cleanup = (int (*)()) dlsym(gDtsDecoderLibHandler, "dca_decoder_deinit");
    if (dts_decoder_cleanup == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[dts_decoder_cleanup:]", __FUNCTION__, __LINE__);
    }

    /*TODO: always decode*/
    (*dts_decoder_init)(1, digital_raw);
    return 0;
Error:
    unload_dts_decoder_lib();
    return -1;
}

static int dca_decode_process(unsigned char*input, int input_size, unsigned char *outbuf,
                              int *out_size, unsigned char *spdif_buf, int *raw_size, struct pcm_info *pcm_out_info)
{
    int outputFrameSize = 0;
    int used_size = 0;
    int decoded_pcm_size = 0;
    int ret = -1;

    if (dts_decoder_process == NULL) {
        return ret;
    }

    ret = (*dts_decoder_process)((char *) input
                                 , input_size
                                 , &used_size
                                 , (char *) outbuf
                                 , out_size
                                 , pcm_out_info
                                 , (char *) spdif_buf
                                 , (int *) raw_size);
    if (ret == 0) {
        ALOGV("decode ok");
    }

    return used_size;
}


int dca_decoder_init_patch(aml_dec_t **ppaml_dec, aml_dec_config_t *dec_config)
{
    struct dca_dts_dec *dts_dec = NULL;
    aml_dec_t  *aml_dec = NULL;

    ALOGI("%s enter", __func__);
    dts_dec = aml_audio_calloc(1, sizeof(struct dca_dts_dec));
    if (dts_dec == NULL) {
        ALOGE("%s malloc dts_dec failed\n", __func__);
        return -1;
    }

    aml_dec = &dts_dec->aml_dec;
    aml_dca_config_t *dca_config = (aml_dca_config_t *)dec_config;

    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;

    dts_dec->is_dtscd    = dca_config->is_dtscd;
    dts_dec->digital_raw = dca_config->digital_raw;
    dts_dec->is_iec61937 = dca_config->is_iec61937;
    aml_dec->format = dca_config->format;

    dts_dec->status = dca_decoder_init(dts_dec->digital_raw);
    if (dts_dec->status < 0) {
        goto error;
    }
    dts_dec->status = 1;
    dts_dec->remain_size = 0;
    dts_dec->decoder_process = dca_decode_process;

    dts_dec->frame_info.syncword = 0;
    dts_dec->frame_info.syncword_pos = 0;
    dts_dec->frame_info.check_pos = 0;
    dts_dec->frame_info.is_little_endian = false;
    dts_dec->frame_info.iec61937_data_type = 0;
    dts_dec->frame_info.size = 0;

    dts_dec->inbuf = (unsigned char*) aml_audio_malloc(MAX_DECODER_FRAME_LENGTH);
    dec_pcm_data->buf_size = MAX_DECODER_FRAME_LENGTH;
    dec_pcm_data->buf = (unsigned char *)aml_audio_malloc(dec_pcm_data->buf_size);
    dec_raw_data->buf_size = MAX_DECODER_FRAME_LENGTH;
    dec_raw_data->buf = (unsigned char *)aml_audio_malloc(dec_raw_data->buf_size);
    if (!dec_pcm_data->buf || !dec_raw_data->buf || !dts_dec->inbuf) {
        ALOGE("%s malloc memory failed!", __func__);
        goto error;
    }
    memset(dec_pcm_data->buf, 0, dec_pcm_data->buf_size);
    memset(dec_raw_data->buf , 0, dec_raw_data->buf_size);

    if (ring_buffer_init(&dts_dec->input_ring_buf, MAX_DECODER_FRAME_LENGTH)) {
        ALOGE("%s init ring buffer failed!", __func__);
        goto error;
    }

    if (getprop_bool(AML_DCA_PROP_DUMP_INPUT_RAW)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_input_raw.dts", AML_DCA_DUMP_FILE_DIR);
        dts_debug.fp_input_raw = fopen(name, "a+");
        if (!dts_debug.fp_input_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (getprop_bool(AML_DCA_PROP_DUMP_OUTPUT_RAW)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_output_raw.dts", AML_DCA_DUMP_FILE_DIR);
        dts_debug.fp_output_raw = fopen(name, "a+");
        if (!dts_debug.fp_output_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (getprop_bool(AML_DCA_PROP_DUMP_OUTPUT_PCM)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_%d_%dch.pcm", AML_DCA_DUMP_FILE_DIR, 48000, 2);
        dts_debug.fp_pcm = fopen(name, "a+");
        if (!dts_debug.fp_pcm) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (getprop_bool(AML_DCA_PROP_DEBUG_FLAG)) {
        ALOGD("true");
        dts_debug.debug_flag = true;
    } else {
        ALOGD("false");
        dts_debug.debug_flag = false;
    }
    *ppaml_dec = aml_dec;

    ALOGI("%s success", __func__);
    return 0;

error:
    if (dts_dec) {
        if (dts_dec->inbuf) {
            aml_audio_free(dts_dec->inbuf);
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
        }
        ring_buffer_release(&dts_dec->input_ring_buf);
        aml_audio_free(dts_dec);
    }
    ALOGE("%s failed", __func__);
    return -1;
}

int dca_decoder_release_patch(aml_dec_t *aml_dec)
{
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;

    ALOGI("%s enter", __func__);
    if (dts_decoder_cleanup != NULL) {
        (*dts_decoder_cleanup)();
    }

    if (dts_dec) {
        if (dts_dec->inbuf) {
            aml_audio_free(dts_dec->inbuf);
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
        }
        ring_buffer_release(&dts_dec->input_ring_buf);

        if (dts_dec->resample_handle) {
            aml_audio_resample_close(dts_dec->resample_handle);
            dts_dec->resample_handle = NULL;
        }

        if (dts_debug.fp_input_raw) {
            fclose(dts_debug.fp_input_raw);
            dts_debug.fp_input_raw = NULL;
        }

        if (dts_debug.fp_output_raw) {
            fclose(dts_debug.fp_output_raw);
            dts_debug.fp_output_raw = NULL;
        }

        if (dts_debug.fp_pcm) {
            fclose(dts_debug.fp_pcm);
            dts_debug.fp_pcm = NULL;
        }
        aml_audio_free(dts_dec);
    }
    return 1;
}

int dca_decoder_process_patch(aml_dec_t *aml_dec, unsigned char *buffer, int bytes)
{
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;
    struct ring_buffer *input_rbuffer = &dts_dec->input_ring_buf;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    int frame_size = 0;
    int used_size = 0;

    if (bytes > 0) {
        if (dts_debug.fp_input_raw) {
            fwrite(buffer, 1, bytes, dts_debug.fp_input_raw);
        }

        if (get_buffer_write_space(input_rbuffer) >= bytes) {
            ring_buffer_write(input_rbuffer, buffer, bytes, 0);
            dts_dec->remain_size += bytes;
            if (dts_debug.debug_flag) {
                ALOGD("%s: remain:%d input data size:%d" , __func__,dts_dec->remain_size, bytes);
            }
        } else {
            ALOGE("%s:%d ring buffer haven`t enough space, lost data size:%d", __func__, __LINE__, bytes);
        }
    }

    do {
        if ((frame_size = _dts_frame_scan(dts_dec)) > 0) {
            ring_buffer_read(input_rbuffer, dts_dec->inbuf, frame_size);
            dts_dec->remain_size -= frame_size;

            used_size = dts_dec->decoder_process(dts_dec->inbuf,
                                    frame_size,
                                    dec_pcm_data->buf,
                                    &dts_dec->outlen_pcm,
                                    dec_raw_data->buf,
                                    &dts_dec->outlen_raw,
                                    &dts_dec->pcm_out_info);
            if (dts_debug.debug_flag) {
                ALOGD("%s: used_size:%d, pcm(len:%d, sr:%d, ch:%d), raw len:%d\n"
                    , __func__, used_size, dts_dec->outlen_pcm, dts_dec->pcm_out_info.sample_rate
                    , dts_dec->pcm_out_info.channel_num, dts_dec->outlen_raw);
            }

            if ((dts_dec->outlen_pcm > 0) && (used_size > 0)) {
                _dts_pcm_output(dts_dec);
            }

            if ((dts_dec->outlen_raw > 0) && (used_size > 0)) {
                _dts_raw_output(dts_dec);
            }
        }
    } while ((frame_size > 0) && (dts_dec->outlen_pcm <= 0));

    if ((dts_dec->outlen_pcm > 0) && (used_size > 0)) {
        return 0;
    } else {
        return -1;
    }
}

static int Write_buffer(struct aml_audio_parser *parser, unsigned char *buffer, int size)
{
    int writecnt = -1;
    int sleep_time = 0;

    if (parser->decode_ThreadExitFlag == 1) {
        ALOGI("decoder exiting status %s\n", __func__);
        return EXITING_STATUS;
    }
    while (parser->decode_ThreadExitFlag == 0) {
        if (get_buffer_write_space(&parser->aml_ringbuffer) < size) {
            sleep_time++;
            usleep(3000);
        } else {
            writecnt = ring_buffer_write(&parser->aml_ringbuffer,
                                         (unsigned char*) buffer, size, UNCOVER_WRITE);
            break;
        }
        if (sleep_time > 1000) { //wait for max 1s to get audio data
            ALOGW("[%s] time out to write audio buffer data! wait for 1s\n", __func__);
            return (parser->decode_ThreadExitFlag == 1) ? EXITING_STATUS : NO_ENOUGH_DATA;
        }
    }
    return writecnt;
}

#define DTS_DECODER_ENABLE
static void *decode_threadloop(void *data)
{
    struct aml_audio_parser *parser = (struct aml_audio_parser *) data;
    unsigned char *outbuf = NULL;
    unsigned char *inbuf = NULL;
    unsigned char *outbuf_raw = NULL;
    unsigned char *read_pointer = NULL;
    int valid_lib = 1;
    int outlen = 0;
    int outlen_raw = 0;
    int outlen_pcm = 0;
    int remain_size = 0;
    bool get_frame_size_ok = 0;
    bool little_end = false;
    bool SyncFlag = false;
    int used_size = 0;
    unsigned int u32AlsaFrameSize = 0;
    int s32AlsaReadFrames = 0;
    int mSample_rate = 0;
    int s32DtsFramesize = 0;
    int mChNum = 0;
    unsigned char temp;
    int i, j;
    aml_dec_control_type_t digital_raw = AML_DEC_CONTROL_DECODING;
    int mute_count = 5;
    struct pcm_info  pcm_out_info;
    int dts_type = 0;
    int data_offset = 0;

    if (NULL == parser) {
        ALOGE("%s:%d parser == NULL", __func__, __LINE__);
        return NULL;
    }

    ALOGI("++ %s:%d in_sr = %d, out_sr = %d\n", __func__, __LINE__, parser->in_sample_rate, parser->out_sample_rate);
    outbuf = (unsigned char*) aml_audio_malloc(MAX_DECODER_FRAME_LENGTH * 4 + MAX_DECODER_FRAME_LENGTH + 8);
    if (!outbuf) {
        ALOGE("%s:%d malloc output buffer failed", __func__, __LINE__);
        return NULL;
    }

    outbuf_raw = outbuf + MAX_DECODER_FRAME_LENGTH;
    inbuf = (unsigned char*) aml_audio_malloc(MAX_DECODER_FRAME_LENGTH * 4 * 2);

    if (!inbuf) {
        ALOGE("%s:%d malloc input buffer failed", __func__, __LINE__);
        aml_audio_free(outbuf);
        return NULL;
    }
#ifdef DTS_DECODER_ENABLE
    if (dts_decoder_init != NULL) {
        unload_dts_decoder_lib();
    }
    //use and bt output need only pcm output,need not raw output
    if (dca_decoder_init(0)) {
        ALOGW("%s:%d dts decoder init failed, maybe no lisensed dts decoder", __func__, __LINE__);
        valid_lib = 0;
    }
    //parser->decode_enabled = 1;
#endif
    if (parser->in_sample_rate != parser->out_sample_rate) {
        parser->aml_resample.input_sr = parser->in_sample_rate;
        parser->aml_resample.output_sr = parser->out_sample_rate;
        parser->aml_resample.channels = 2;
        resampler_init(&parser->aml_resample);
    }

    struct aml_stream_in *in = parser->in;
    u32AlsaFrameSize = in->config.channels * pcm_format_to_bits(in->config.format) / 8;
    prctl(PR_SET_NAME, (unsigned long)"audio_dca_dec");
    while (parser->decode_ThreadExitFlag == 0) {
        outlen = 0;
        outlen_raw = 0;
        outlen_pcm = 0;
        SyncFlag = 0;
        s32DtsFramesize = 0;
        if (parser->decode_ThreadExitFlag == 1) {
            ALOGI("%s:%d exit threadloop!", __func__, __LINE__);
            break;
        }

        //here we call decode api to decode audio frame here
        if (remain_size + READ_PERIOD_LENGTH <= (MAX_DECODER_FRAME_LENGTH * 4 * 2)) { //input buffer size
            // read raw DTS data for alsa
            s32AlsaReadFrames = pcm_read(parser->aml_pcm, inbuf + remain_size, READ_PERIOD_LENGTH);
            if (s32AlsaReadFrames < 0) {
                usleep(1000);
                continue;
            } else {
#if 0
                FILE *dump_origin = NULL;
                dump_origin = fopen("/data/tmp/pcm_read.raw", "a+");
                if (dump_origin != NULL) {
                    fwrite(inbuf + remain_size, READ_PERIOD_LENGTH, 1, dump_origin);
                    fclose(dump_origin);
                } else {
                    ALOGW("[Error] Can't write to /data/tmp/pcm_read.raw");
                }
#endif
            }
            remain_size += s32AlsaReadFrames * u32AlsaFrameSize;
        }

#ifdef DTS_DECODER_ENABLE
        if (valid_lib == 0) {
            continue;
        }
#endif
        //find header and get paramters
        read_pointer = inbuf;
        if  (remain_size > MAX_DECODER_FRAME_LENGTH)
        {
            while (parser->decode_ThreadExitFlag == 0 && !SyncFlag && remain_size > IEC61937_HEADER_LENGTH) {
                //DTS_SYNCWORD_IEC61937 : 0xF8724E1F
                if (read_pointer[0] == 0x72 && read_pointer[1] == 0xf8
                    && read_pointer[2] == 0x1f && read_pointer[3] == 0x4e) {
                    SyncFlag = true;
                    dts_type = read_pointer[4] & 0x1f;
                } else if (read_pointer[0] == 0xf8 && read_pointer[1] == 0x72
                           && read_pointer[2] == 0x4e && read_pointer[3] == 0x1f) {
                    SyncFlag = true;
                    little_end = true;
                    dts_type = read_pointer[5] & 0x1f;
                }

                if (SyncFlag == 0) {
                    read_pointer++;
                    remain_size--;
                }
            }
            ALOGV("DTS Sync:%d little endian:%d dts type:%d, remain_size:%d",SyncFlag,little_end,dts_type, remain_size);
            if (SyncFlag) {
                // point to pd
                read_pointer = read_pointer + IEC61937_PD_OFFSET;
                //ALOGD("read_pointer[0]:0x%x read_pointer[1]:0x%x",read_pointer[0],read_pointer[1]);
                if (!little_end) {
                    s32DtsFramesize = (read_pointer[0] | read_pointer[1] << 8);
                } else {
                    s32DtsFramesize = (read_pointer[1] | read_pointer[0] << 8);
                }

                if (dts_type == DTS_TYPE_I ||
                    dts_type == DTS_TYPE_II ||
                    dts_type == DTS_TYPE_III) {
                    // these DTS type use bits length for PD
                    s32DtsFramesize = s32DtsFramesize >> 3;
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                    data_offset = IEC61937_HEADER_LENGTH;
                } else if (dts_type == DTS_TYPE_IV) {
                    /*refer kodi how to add 12 bytes header for DTS HD
                    01 00 00 00 00 00 00 00 fe fe ** **, last 2 bytes for data size
                    */
                    // point to the address after pd
                    read_pointer = read_pointer + IEC61937_PD_SIZE;
                    if (remain_size < (IEC_DTS_HD_APPEND_LNGTH + IEC61937_HEADER_LENGTH)) {
                        // point to pa
                        memmove(inbuf, read_pointer - IEC61937_HEADER_LENGTH, remain_size);
                        ALOGD("Not enough data for DTS HD header parsing\n");
                        continue;
                    }

                    if (read_pointer[0] == 0x00 && read_pointer[1] == 0x01 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe) {
                        s32DtsFramesize = (read_pointer[10] | read_pointer[11] << 8);
                    } else if ((read_pointer[0] == 0x01 && read_pointer[1] == 0x00 && read_pointer[8] == 0xfe && read_pointer[9] == 0xfe)) {
                        s32DtsFramesize = (read_pointer[11] | read_pointer[10] << 8);
                    } else {
                        ALOGE("DTS HD error data\n");
                        s32DtsFramesize = 0;
                    }
                    //ALOGD("size data=0x%x 0x%x\n",read_pointer[10],read_pointer[11]);
                    // point to the address after 12 bytes header
                    read_pointer = read_pointer + IEC_DTS_HD_APPEND_LNGTH;
                    data_offset = IEC_DTS_HD_APPEND_LNGTH + IEC61937_HEADER_LENGTH;
                } else {
                    ALOGW("Unknow DTS type=0x%x", dts_type);
                    s32DtsFramesize = 0;
                    data_offset = IEC61937_PD_OFFSET;
                }

                if (s32DtsFramesize <= 0) {
                    ALOGW("wrong data for DTS,skip the header remain=%d data offset=%d", remain_size, data_offset);
                    remain_size = remain_size - data_offset;

                    if (remain_size < 0) {
                        remain_size = 0;
                        ALOGE("Carsh issue happens\n");
                    }
                    memmove(inbuf, read_pointer, remain_size);
                    continue;
                }

                //to do know why
                if (s32DtsFramesize == 2013) {
                    s32DtsFramesize = 2012;
                }

                ALOGV("frame_size:%d dts_dec->remain_size:%d little_end:%d", s32DtsFramesize, remain_size, little_end);
                // the remain size contain the header and raw data size
                if (remain_size < (s32DtsFramesize + data_offset)) {
                    // point to pa and copy these bytes
                    memmove(inbuf, read_pointer - data_offset, remain_size);
                    s32DtsFramesize = 0;
                } else {
                    // there is enough data, header has been used, update the remain size
                    remain_size = remain_size - data_offset;
                }
            } else {
                s32DtsFramesize = 0;
            }
        }

#ifdef DTS_DECODER_ENABLE
        ALOGV("%s:%d Framesize:%d, remain_size:%d, dts_type:%d", __func__, __LINE__, s32DtsFramesize, remain_size, dts_type);
        used_size = dca_decode_process(read_pointer, s32DtsFramesize, outbuf,
                                       &outlen_pcm, outbuf_raw, &outlen_raw,&pcm_out_info);
    if (getprop_bool("media.audiohal.dtsdump")) {
        FILE *dump_fp = NULL;
        dump_fp = fopen("/data/audio_hal/audio2dca.dts", "a+");
        if (dump_fp != NULL) {
            fwrite(read_pointer, s32DtsFramesize, 1, dump_fp);
            fclose(dump_fp);
        } else {
            ALOGW("[Error] Can't write to /data/audio_hal/audio2dca.raw");
        }
    }

#else
        used_size = s32DtsFramesize;
#endif
        if (used_size > 0) {
            remain_size -= used_size;
            ALOGV("%s:%d decode success used_size:%d, outlen_pcm:%d", __func__, __LINE__, used_size, outlen_pcm);
            memmove(inbuf, read_pointer + used_size, remain_size);
        } else {
            ALOGV("%s:%d decode failed, used_size:%d", __func__, __LINE__, used_size);
        }

#ifdef DTS_DECODER_ENABLE
        //only need pcm data
        if (outlen_pcm > 0) {
            // here only downresample, so no need to malloc more buffer
            if (parser->in_sample_rate != parser->out_sample_rate) {
                int out_frame = outlen_pcm >> 2;
                out_frame = resample_process(&parser->aml_resample, out_frame, (short*) outbuf, (short*) outbuf);
                outlen_pcm = out_frame << 2;
            }
            parser->data_ready = 1;
            //here when pcm -> dts,some error frames maybe received by deocder
            //and I tried to use referrence tools to decoding the frames and the output also has noise frames.
            //so here we try to mute count outbuf frame to wrok arount the issuse
            if (mute_count > 0) {
                memset(outbuf, 0, outlen_pcm);
                mute_count--;
            }
#if 1
            if (getprop_bool("media.audiohal.dtsdump")) {
                FILE *dump_fp = NULL;
                dump_fp = fopen("/data/audio_hal/dtsout.pcm", "a+");
                if (dump_fp != NULL) {
                    fwrite(outbuf, outlen_pcm, 1, dump_fp);
                    fclose(dump_fp);
                } else {
                    ALOGW("[Error] Can't write to /data/audio_hal/dtsout.pcm");
                }
            }
#endif
            Write_buffer(parser, outbuf, outlen_pcm);
        }

#endif
    }
    parser->decode_enabled = 0;
    if (inbuf) {
        aml_audio_free(inbuf);
    }
    if (outbuf) {
        aml_audio_free(outbuf);
    }
#ifdef DTS_DECODER_ENABLE
    unload_dts_decoder_lib();
#endif
    ALOGI("-- %s:%d", __func__, __LINE__);
    return NULL;
}


static int start_decode_thread(struct aml_audio_parser *parser)
{
    int ret = 0;

    ALOGI("%s:%d enter", __func__, __LINE__);
    parser->decode_enabled = 1;
    parser->decode_ThreadExitFlag = 0;
    ret = pthread_create(&parser->decode_ThreadID, NULL, &decode_threadloop, parser);
    if (ret != 0) {
        ALOGE("%s:%d, Create thread fail!", __FUNCTION__, __LINE__);
        return -1;
    }
    return 0;
}

static int stop_decode_thread(struct aml_audio_parser *parser)
{
    ALOGI("++%s:%d enter", __func__, __LINE__);
    parser->decode_ThreadExitFlag = 1;
    pthread_join(parser->decode_ThreadID, NULL);
    parser->decode_ThreadID = 0;
    ALOGI("--%s:%d exit", __func__, __LINE__);
    return 0;
}

int dca_decode_init(struct aml_audio_parser *parser)
{
    ring_buffer_reset(&(parser->aml_ringbuffer));
    struct aml_stream_in *in = parser->in;
    parser->aml_pcm = in->pcm;
    parser->in_sample_rate = in->config.rate;
    parser->out_sample_rate = in->requested_rate;
    parser->decode_dev_op_mutex = &in->lock;
    parser->data_ready = 0;
    return start_decode_thread(parser);
}

int dca_decode_release(struct aml_audio_parser *parser)
{
    int s32Ret = 0;
    s32Ret = stop_decode_thread(parser);
    ring_buffer_reset(&(parser->aml_ringbuffer));
    return s32Ret;
}

aml_dec_func_t aml_dca_func = {
    .f_init                 = dca_decoder_init_patch,
    .f_release              = dca_decoder_release_patch,
    .f_process              = dca_decoder_process_patch,
    .f_config               = NULL,
    .f_info                 = NULL,
};

