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

#define LOG_TAG "aml_audio_dts_dec"
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
#include <cutils/properties.h>

#include "audio_hw.h"
#include "aml_dts_dec_api.h"



#define DOLBY_DTSHD_LIB_PATH     "/odm/lib/libHwAudio_dtshd.so"

#define MAX_DCA_FRAME_LENGTH 32768
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


#define DCA_CHECK_NULL_STR(x)     (NULL == x ? #x : "")

enum
{
    EXITING_STATUS = -1001,
    NO_ENOUGH_DATA = -1002,
};

///< From dtshd_dec_api_common.h. It belongs to the pubheader, so it won't change.
enum DTS_STRMTYPE_MASK
{
    DTSSTRMTYPE_UNKNOWN              = 0x00000000,
    DTSSTRMTYPE_DTS_LEGACY           = 0x00000001,
    DTSSTRMTYPE_DTS_ES_MATRIX        = 0x00000002,
    DTSSTRMTYPE_DTS_ES_DISCRETE      = 0x00000004,
    DTSSTRMTYPE_DTS_9624             = 0x00000008,
    DTSSTRMTYPE_DTS_ES_8CH_DISCRETE  = 0x00000010,
    DTSSTRMTYPE_DTS_HIRES            = 0x00000020,
    DTSSTRMTYPE_DTS_MA               = 0x00000040,
    DTSSTRMTYPE_DTS_LBR              = 0x00000080,
    DTSSTRMTYPE_DTS_LOSSLESS         = 0x00000100,
    DTSSTRMTYPE_DTS_HEADPHONE        = 0x10000000   ///< for headphoneX, fake streamtype
};

struct dca_dts_debug {
    bool debug_flag;
    FILE* fp_pcm;
    FILE* fp_input_raw;
    FILE* fp_output_raw;
};
static struct dca_dts_debug dts_debug = {0};

static unsigned int dca_initparam_out_ch = 2;

///static struct pcm_info pcm_out_info;
/*dts decoder lib function*/
static int (*dts_decoder_init)(int, int);
static int (*dts_decoder_cleanup)();
static int (*dts_decoder_process)(char * , int , int *, char *, int *, struct pcm_info *, char *, int *);
static int (*dts_decoder_config)(dca_config_type_e, union dca_config_s *);
static int (*dts_decoder_getinfo)(dca_info_type_e, union dca_info_s *);
void *gDtsDecoderLibHandler = NULL;
static int _dts_syncword_scan(unsigned char *read_pointer, unsigned int *pTemp0);
static int _dts_frame_scan(struct dca_dts_dec *dts_dec);
static int _dts_pcm_output(struct dca_dts_dec *dts_dec);
static int _dts_raw_output(struct dca_dts_dec *dts_dec);
static int _dts_stream_type_mapping(unsigned int stream_type);

static void endian16_convert(void *buf, int size)
{
    int i;
    unsigned short *p = (unsigned short *)buf;
    for (i = 0; i < size / 2; i++, p++) {
      *p = ((*p & 0xff) << 8) | ((*p) >> 8);
    }
}

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

        // no valid frame was found beyond size of MAX_DCA_FRAME_LENGTH, meybe it is dirty data, so drop it
        if ((frame_size <= 0) && (check_size >= MAX_DCA_FRAME_LENGTH)) {
            ring_buffer_seek(input_rbuffer, check_size);
            dts_dec->remain_size -= check_size;
            frame_info->syncword_pos = 0;
            frame_info->syncword = 0;
            frame_size = -1;
        } else if ((frame_size <= 0) && (check_size < MAX_DCA_FRAME_LENGTH)) {
            frame_size = 0;
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
    aml_dec_t *aml_dec = (aml_dec_t *)dts_dec;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    int channel_num = dts_dec->pcm_out_info.channel_num;

    /* VX(VirtualX) uses 2CH as input by default.
     * When the output changes from stereo to multi-ch,
     * VX needs at least one frame of decoded information to configure the number of channel correctly.
     * So mute to the first frame to avoid abnormal sound caused by wrong configuration.
     */
    if (channel_num > 2 && aml_dec->frame_cnt == 0) {
        ALOGI("mute the first frame");
        memset(dec_pcm_data->buf, 0, dts_dec->outlen_pcm);
    }

    if (dts_debug.fp_pcm) {
        fwrite(dec_pcm_data->buf, 1, dts_dec->outlen_pcm, dts_debug.fp_pcm);
    }

    dec_pcm_data->data_format = AUDIO_FORMAT_PCM_16_BIT;
    dec_pcm_data->data_ch = channel_num;
    dec_pcm_data->data_sr = dts_dec->pcm_out_info.sample_rate;
    dec_pcm_data->data_len = dts_dec->outlen_pcm;

    return 0;
}

static int _dts_raw_output(struct dca_dts_dec *dts_dec)
{
    aml_dec_t *aml_dec = (aml_dec_t *)dts_dec;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    unsigned int syncword = 0;
    if (dts_debug.fp_output_raw) {
        fwrite(dec_raw_data->buf, 1, dts_dec->outlen_raw, dts_debug.fp_output_raw);
    }

    dec_raw_data->data_format = AUDIO_FORMAT_IEC61937;
    if (aml_dec->format == AUDIO_FORMAT_DTS_HD && (dts_dec->digital_raw == AML_DEC_CONTROL_CONVERT)) {
        dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
    } else {
        dec_raw_data->sub_format = AUDIO_FORMAT_DTS;
    }
    /*we check whether the data is iec61937 or dts raw*/
    dec_raw_data->is_dtscd = _dts_syncword_scan(dec_raw_data->buf, &syncword);
    dec_raw_data->data_ch = 2;
    if (dts_dec->pcm_out_info.sample_rate == 44100)
        dec_raw_data->data_sr = 44100;
    else
        dec_raw_data->data_sr = 48000;
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
    dts_decoder_config = NULL;
    dts_decoder_getinfo = NULL;
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

    dts_decoder_config = (int (*)(dca_config_type_e, union dca_config_s *)) dlsym(gDtsDecoderLibHandler, "dca_decoder_config");
    if (dts_decoder_config == NULL) {
        ALOGE("%s,can not find decoder config function,%s\n", __FUNCTION__, dlerror());
    } else {
        ALOGV("<%s::%d>--[dts_decoder_config:]", __FUNCTION__, __LINE__);
    }

    dts_decoder_getinfo = (int (*)(dca_info_type_e, union dca_info_s *)) dlsym(gDtsDecoderLibHandler, "dca_decoder_getinfo");
    if (dts_decoder_getinfo == NULL) {
        ALOGE("%s,can not find decoder getinfo function,%s\n", __FUNCTION__, dlerror());
    } else {
        ALOGV("<%s::%d>--[dts_decoder_getinfo:]", __FUNCTION__, __LINE__);
    }

    /*TODO: always decode*/
    (*dts_decoder_init)(1, digital_raw);

    if (dts_decoder_config) {
        dca_config_t dca_config;
        memset(&dca_config, 0, sizeof(dca_config));
        dca_config.output_ch = dca_initparam_out_ch;
        (*dts_decoder_config)(DCA_CONFIG_OUT_CH, (dca_config_t *)&dca_config);
    }
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

static int _dts_stream_type_mapping(unsigned int stream_type)
{
    int dts_type = TYPE_DTS;
    int temp_stream_type;

    temp_stream_type = stream_type & (~DTSSTRMTYPE_DTS_HEADPHONE);
    switch (temp_stream_type) {
        case DTSSTRMTYPE_DTS_LEGACY:
        case DTSSTRMTYPE_DTS_ES_MATRIX:
        case DTSSTRMTYPE_DTS_ES_DISCRETE:
        case DTSSTRMTYPE_DTS_9624:
        case DTSSTRMTYPE_DTS_ES_8CH_DISCRETE:
        case DTSSTRMTYPE_DTS_HIRES:
            dts_type = TYPE_DTS;
            break;
        case DTSSTRMTYPE_DTS_MA:
        case DTSSTRMTYPE_DTS_LOSSLESS:
            dts_type = TYPE_DTS_HD;
            break;
        case DTSSTRMTYPE_DTS_LBR:
            dts_type = TYPE_DTS_EXPRESS;
            break;

        case DTSSTRMTYPE_DTS_HEADPHONE:
            dts_type = TYPE_DTS_HP;
            break;

        case DTSSTRMTYPE_UNKNOWN:
        default:
            dts_type = TYPE_DTS;
            break;
    }

    return dts_type;
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
    aml_dca_config_t *dca_config = &dec_config->dca_config;

    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t *raw_in_data  = &aml_dec->raw_in_data; ///< no use

    dts_dec->is_dtscd    = dca_config->is_dtscd;
    dts_dec->digital_raw = dca_config->digital_raw;
    dts_dec->is_iec61937 = dca_config->is_iec61937;
    aml_dec->frame_cnt = 0;
    aml_dec->format = dca_config->format;
    dts_dec->stream_type = 0;
    dts_dec->is_headphone_x = false;

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

    dts_dec->inbuf = (unsigned char*) aml_audio_malloc(MAX_DCA_FRAME_LENGTH);  ///< same as dca decoder
    dec_pcm_data->buf_size = MAX_DCA_FRAME_LENGTH * 2;
    dec_pcm_data->buf = (unsigned char *)aml_audio_malloc(dec_pcm_data->buf_size);
    dec_raw_data->buf_size = MAX_DCA_FRAME_LENGTH;
    dec_raw_data->buf = (unsigned char *)aml_audio_malloc(dec_raw_data->buf_size);
    if (!dec_pcm_data->buf || !dec_raw_data->buf || !dts_dec->inbuf) {
        ALOGE("%s malloc memory failed!", __func__);
        goto error;
    }
    memset(dec_pcm_data->buf, 0, dec_pcm_data->buf_size);
    memset(dec_raw_data->buf , 0, dec_raw_data->buf_size);
    memset(raw_in_data, 0, sizeof(dec_data_info_t));  ///< no use

    if (ring_buffer_init(&dts_dec->input_ring_buf, MAX_DCA_FRAME_LENGTH * 2)) {
        ALOGE("%s init ring buffer failed!", __func__);
        goto error;
    }

    if (property_get_bool(AML_DCA_PROP_DUMP_INPUT_RAW, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_input_raw.dts", AML_DCA_DUMP_FILE_DIR);
        dts_debug.fp_input_raw = fopen(name, "a+");
        if (!dts_debug.fp_input_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DCA_PROP_DUMP_OUTPUT_RAW, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_output_raw.dts", AML_DCA_DUMP_FILE_DIR);
        dts_debug.fp_output_raw = fopen(name, "a+");
        if (!dts_debug.fp_output_raw) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DCA_PROP_DUMP_OUTPUT_PCM, 0)) {
        char name[64] = {0};
        snprintf(name, 64, "%sdts_%d_%dch.pcm", AML_DCA_DUMP_FILE_DIR, 48000, 2);
        dts_debug.fp_pcm = fopen(name, "a+");
        if (!dts_debug.fp_pcm) {
            ALOGW("[Error] Can't write to %s", name);
        }
    }

    if (property_get_bool(AML_DCA_PROP_DEBUG_FLAG, 0)) {
        ALOGD("true");
        dts_debug.debug_flag = true;
    } else {
        ALOGD("false");
        dts_debug.debug_flag = false;
    }
    *ppaml_dec = aml_dec;

    aml_dec->dev = dca_config->dev;

    ALOGI("%s success", __func__);
    return 0;

error:
    if (dts_dec) {
        if (dts_dec->inbuf) {
            aml_audio_free(dts_dec->inbuf);
            dts_dec->inbuf = NULL;
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
            dec_raw_data->buf = NULL;
        }
        ring_buffer_release(&dts_dec->input_ring_buf);
        aml_audio_free(dts_dec);
        aml_dec = NULL;
        *ppaml_dec = NULL;
    }
    ALOGE("%s failed", __func__);
    return -1;
}

int dca_decoder_release_patch(aml_dec_t *aml_dec)
{
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;
    struct aml_audio_device *adev = NULL;
    dec_data_info_t *dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t *dec_raw_data = &aml_dec->dec_raw_data;

    ALOGI("%s enter", __func__);
    unload_dts_decoder_lib();

    if (dts_dec) {
        if (dts_dec->inbuf) {
            aml_audio_free(dts_dec->inbuf);
            dts_dec->inbuf = NULL;
        }
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }
        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
            dec_raw_data->buf = NULL;
        }
        ring_buffer_release(&dts_dec->input_ring_buf);

        /*if (dts_dec->resample_handle) {
            aml_audio_resample_close(dts_dec->resample_handle);
            dts_dec->resample_handle = NULL;
        }*/

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

        adev = (struct aml_audio_device *)(aml_dec->dev);
        adev->dts_hd.stream_type = 0;
        adev->dts_hd.is_headphone_x = false;
        aml_dec->frame_cnt = 0;

        aml_audio_free(dts_dec);
        dts_dec = NULL;
    }
    return 1;
}

int dca_decoder_process_patch(aml_dec_t *aml_dec, unsigned char *buffer, int bytes)
{
    struct dca_dts_dec *dts_dec = NULL;
    struct aml_audio_device *adev = NULL;
    struct ring_buffer *input_rbuffer = NULL;
    dec_data_info_t *dec_pcm_data = NULL;
    dec_data_info_t *dec_raw_data = NULL;
    int frame_size = 0;
    int used_size = 0;

    if (!aml_dec || !buffer) {
        ALOGE("[%s:%d] Invalid parameter: %s %s", __func__, __LINE__, DCA_CHECK_NULL_STR(aml_dec), DCA_CHECK_NULL_STR(buffer));
        return AML_DEC_RETURN_TYPE_FAIL;
    }

    dts_dec = (struct dca_dts_dec *)aml_dec;
    input_rbuffer = &dts_dec->input_ring_buf;
    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_raw_data = &aml_dec->dec_raw_data;
    dts_dec->outlen_pcm = 0;
    dts_dec->stream_type = TYPE_DTS;
    dts_dec->is_headphone_x = false;

    adev = (struct aml_audio_device *)(aml_dec->dev);
    if (!adev) {
        ALOGE("[%s:%d] Invalid parameter %s", __func__, __LINE__, DCA_CHECK_NULL_STR(adev));
        return AML_DEC_RETURN_TYPE_FAIL;
    }

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

    frame_size = _dts_frame_scan(dts_dec);
    if (frame_size > 0) {
        ring_buffer_read(input_rbuffer, dts_dec->inbuf, frame_size);
        dts_dec->remain_size -= frame_size;
        if (dts_dec->is_iec61937 && !dts_dec->frame_info.is_little_endian) {
            endian16_convert(dts_dec->inbuf, frame_size);
        }

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

        if ( ((dts_dec->outlen_pcm > 0) || (dts_dec->outlen_raw > 0)) && (used_size > 0) ) {
            ///< get dts stream type, display audio info banner.
            if (!dts_decoder_getinfo) {
                dts_dec->stream_type = -1;
                dts_dec->is_headphone_x = false;
            } else {
                dca_info_t dca_info;
                memset(&dca_info, 0, sizeof(dca_info));
                int ret = (*dts_decoder_getinfo)(DCA_STREAM_INFO, (dca_info_t *)&dca_info);
                if (!ret) {
                    dts_dec->stream_type = _dts_stream_type_mapping(dca_info.stream_info.stream_type);
                    dts_dec->is_headphone_x = !!(dca_info.stream_info.stream_type & DTSSTRMTYPE_DTS_HEADPHONE);
                } else {
                    dts_dec->stream_type = -1;
                    dts_dec->is_headphone_x = false;
                }
            }

            adev->dts_hd.stream_type = dts_dec->stream_type;
            adev->dts_hd.is_headphone_x = dts_dec->is_headphone_x;
        }

        if ((dts_dec->outlen_pcm > 0) && (used_size > 0)) {
            /* Cache a lot of data, needs to be decoded multiple times. */
            aml_dec->frame_cnt++;
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        } else {
            return AML_DEC_RETURN_TYPE_NEED_DEC_AGAIN;
        }
    } else if (frame_size == 0) {
        return AML_DEC_RETURN_TYPE_CACHE_DATA;
    } else {
        return AML_DEC_RETURN_TYPE_FAIL;
    }
}

int dca_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t *aml_dec_config)
{
    int ret = -1;
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;

    if (!dts_decoder_config || !dts_dec) {
        if (!aml_dec_config)
            return ret;

        ///< static param, will take effect after decoder_init.
        switch (config_type) {
            case AML_DEC_CONFIG_OUTPUT_CHANNEL:
            {
                dca_initparam_out_ch = aml_dec_config->dca_config.output_ch;
                ret = 0;
                break;
            }

            default:    ///< not support runtime param
                return ret;
        }

        return ret;
    }

    if (dts_dec->status != 1) {
        return ret;
    }

    ///< runtime/static param, will take effect immediately.
    switch (config_type) {
        case AML_DEC_CONFIG_OUTPUT_CHANNEL:
        {
            dca_config_t dca_config;
            memset(&dca_config, 0, sizeof(dca_config));
            dca_config.output_ch = aml_dec_config->dca_config.output_ch;
            ret = (*dts_decoder_config)(DCA_CONFIG_OUT_CH, (dca_config_t *)&dca_config);
            break;
        }

        default:
            break;
    }

    return ret;
}

int dca_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t *aml_dec_info)
{
    int ret = -1;
    struct dca_dts_dec *dts_dec = (struct dca_dts_dec *)aml_dec;

    if (!dts_decoder_getinfo || !aml_dec_info || !dts_dec) {
        return ret;
    }

    if (dts_dec->status != 1) {
        return ret;
    }

    switch (info_type) {
        case AML_DEC_STREMAM_INFO:
        {
            dca_info_t dca_info;
            struct aml_audio_device *adev = (struct aml_audio_device *)(aml_dec->dev);
            memset(&dca_info, 0, sizeof(dca_info));
            ret = (*dts_decoder_getinfo)(DCA_STREAM_INFO, (dca_info_t *)&dca_info);
            if (ret >= 0) {
                aml_dec_info->dec_info.stream_ch = dca_info.stream_info.stream_ch;
                aml_dec_info->dec_info.stream_sr = dca_info.stream_info.stream_sr;
                dts_dec->stream_type = _dts_stream_type_mapping(dca_info.stream_info.stream_type);
                dts_dec->is_headphone_x = !!(dca_info.stream_info.stream_type & DTSSTRMTYPE_DTS_HEADPHONE);
                ///< aml_dec_info->dec_info.output_bLFE = // not support yet
            } else {
                memset(aml_dec_info, 0, sizeof(aml_dec_info_t));
            }
            adev->dts_hd.stream_type = dts_dec->stream_type;
            adev->dts_hd.is_headphone_x = dts_dec->is_headphone_x;
            break;
        }

        case AML_DEC_OUTPUT_INFO:
        {
            dca_info_t dca_info;
            memset(&dca_info, 0, sizeof(dca_info));
            ret = (*dts_decoder_getinfo)(DCA_OUTPUT_INFO, (dca_info_t *)&dca_info);
            if (ret >= 0) {
                aml_dec_info->dec_output_info.output_ch = dca_info.output_info.output_ch;
                aml_dec_info->dec_output_info.output_sr = dca_info.output_info.output_sr;
                aml_dec_info->dec_output_info.output_bitwidth = dca_info.output_info.output_bitwidth;
            } else {
                memset(aml_dec_info, 0, sizeof(aml_dec_info_t));
            }
            break;
        }

        default:
            break;
    }
    return ret;
}

int dca_get_out_ch_internal(void)
{
    ///< not init yet.
    if (!dts_decoder_getinfo)
        return 0;

    dca_info_t dca_info;
    memset(&dca_info, 0, sizeof(dca_info));
    int ret = (*dts_decoder_getinfo)(DCA_OUTPUT_INFO, (dca_info_t *)&dca_info);
    if (!ret) {
        return dca_info.output_info.output_ch;
    } else {
        return -1;
    }
}

int dca_set_out_ch_internal(int ch_num)
{
    if (!dts_decoder_config) {
        ///< static param, will take effect after decoder_init.
        dca_initparam_out_ch = ch_num;
        ALOGI("%s: DTS Channel Output Mode = %d!", __FUNCTION__, ch_num);
        return 0;
    }

    dca_config_t dca_config;
    memset(&dca_config, 0, sizeof(dca_config));
    dca_config.output_ch = ch_num;
    ///< static/runtime param, will take effect immediately.
    int ret = (*dts_decoder_config)(DCA_CONFIG_OUT_CH, (dca_config_t *)&dca_config);

    return ret;
}

aml_dec_func_t aml_dca_func = {
    .f_init                 = dca_decoder_init_patch,
    .f_release              = dca_decoder_release_patch,
    .f_process              = dca_decoder_process_patch,
    .f_config               = dca_decoder_config,
    .f_info                 = dca_decoder_getinfo,
};
