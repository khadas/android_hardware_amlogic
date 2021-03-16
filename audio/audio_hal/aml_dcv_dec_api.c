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

#define LOG_TAG "aml_audio_dcv_dec"

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
#include "aml_dcv_dec_api.h"
#include "aml_ac3_parser.h"

enum {
    EXITING_STATUS = -1001,
    NO_ENOUGH_DATA = -1002,
};

#define     DDPshort            short
#define     DDPerr              short
#define     DDPushort           unsigned short
#define     BYTESPERWRD         2
#define     BITSPERWRD          (BYTESPERWRD*8)
#define     SYNCWRD             ((DDPshort)0x0b77)
#define     MAXFSCOD            3
#define     MAXDDDATARATE       38
#define     BS_STD              8
#define     ISDD(bsid)          ((bsid) <= BS_STD)
#define     MAXCHANCFGS         8
#define     BS_AXE              16
#define     ISDDP(bsid)         ((bsid) <= BS_AXE && (bsid) > 10)
#define     BS_BITOFFSET        40
#define     PTR_HEAD_SIZE       7//20

#define DOLBY_DCV_LIB_PATH_A "/odm/lib/libHwAudio_dcvdec.so"

typedef struct {
    DDPshort       *buf;
    DDPshort        bitptr;
    DDPshort        data;
} DDP_BSTRM;

const DDPshort chanary[MAXCHANCFGS] = { 2, 1, 2, 3, 3, 4, 4, 5 };
enum {
    MODE11 = 0,
    MODE_RSVD = 0,
    MODE10,
    MODE20,
    MODE30,
    MODE21,
    MODE31,
    MODE22,
    MODE32
};
const DDPushort msktab[] = { 0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800,
                             0xfc00, 0xfe00, 0xff00, 0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc,
                             0xfffe, 0xffff
                           };
const DDPshort frmsizetab[MAXFSCOD][MAXDDDATARATE] = {
    /* 48kHz */
    {
        64, 64, 80, 80, 96, 96, 112, 112,
        128, 128, 160, 160, 192, 192, 224, 224,
        256, 256, 320, 320, 384, 384, 448, 448,
        512, 512, 640, 640, 768, 768, 896, 896,
        1024, 1024, 1152, 1152, 1280, 1280
    },
    /* 44.1kHz */
    {
        69, 70, 87, 88, 104, 105, 121, 122,
        139, 140, 174, 175, 208, 209, 243, 244,
        278, 279, 348, 349, 417, 418, 487, 488,
        557, 558, 696, 697, 835, 836, 975, 976,
        1114, 1115, 1253, 1254, 1393, 1394
    },
    /* 32kHz */
    {
        96, 96, 120, 120, 144, 144, 168, 168,
        192, 192, 240, 240, 288, 288, 336, 336,
        384, 384, 480, 480, 576, 576, 672, 672,
        768, 768, 960, 960, 1152, 1152, 1344, 1344,
        1536, 1536, 1728, 1728, 1920, 1920
    }
};

static int (*ddp_decoder_init)(int, int,void **);
static int (*ddp_decoder_cleanup)(void *);
static int (*ddp_decoder_process)(char *, int, int *, int, char *, int *, struct pcm_info *, char *, int *,void *);
static int (*ddp_decoder_config)(void *, ddp_config_type_t, ddp_config_t *);

static void *gDDPDecoderLibHandler = NULL;
static void *handle = NULL;

static DDPerr ddbs_init(DDPshort * buf, DDPshort bitptr, DDP_BSTRM *p_bstrm)
{
    p_bstrm->buf = buf;
    p_bstrm->bitptr = bitptr;
    p_bstrm->data = *buf;
    return 0;
}

static DDPerr ddbs_unprj(DDP_BSTRM *p_bstrm, DDPshort *p_data,  DDPshort numbits)
{
    DDPushort data;
    *p_data = (DDPshort)((p_bstrm->data << p_bstrm->bitptr) & msktab[numbits]);
    p_bstrm->bitptr += numbits;
    if (p_bstrm->bitptr >= BITSPERWRD) {
        p_bstrm->buf++;
        p_bstrm->data = *p_bstrm->buf;
        p_bstrm->bitptr -= BITSPERWRD;
        data = (DDPushort) p_bstrm->data;
        *p_data |= ((data >> (numbits - p_bstrm->bitptr)) & msktab[numbits]);
    }
    *p_data = (DDPshort)((DDPushort)(*p_data) >> (BITSPERWRD - numbits));
    return 0;
}

static int Get_DD_Parameters(void *buf, int *sample_rate, int *frame_size, int *ChNum)
{
    int numch = 0;
    DDP_BSTRM bstrm = {NULL, 0, 0};
    DDP_BSTRM *p_bstrm = &bstrm;
    short tmp = 0, acmod, lfeon, fscod, frmsizecod;
    ddbs_init((short*) buf, 0, p_bstrm);

    ddbs_unprj(p_bstrm, &tmp, 16);
    if (tmp != SYNCWRD) {
        ALOGI("Invalid synchronization word");
        return 0;
    }
    ddbs_unprj(p_bstrm, &tmp, 16);
    ddbs_unprj(p_bstrm, &fscod, 2);
    if (fscod == MAXFSCOD) {
        ALOGI("Invalid sampling rate code");
        return 0;
    }

    if (fscod == 0) {
        *sample_rate = 48000;
    } else if (fscod == 1) {
        *sample_rate = 44100;
    } else if (fscod == 2) {
        *sample_rate = 32000;
    }

    ddbs_unprj(p_bstrm, &frmsizecod, 6);
    if (frmsizecod >= MAXDDDATARATE) {
        ALOGI("Invalid frame size code");
        return 0;
    }

    *frame_size = 2 * frmsizetab[fscod][frmsizecod];

    ddbs_unprj(p_bstrm, &tmp, 5);
    if (!ISDD(tmp)) {
        ALOGI("Unsupported bitstream id");
        return 0;
    }

    ddbs_unprj(p_bstrm, &tmp, 3);
    ddbs_unprj(p_bstrm, &acmod, 3);

    if ((acmod != MODE10) && (acmod & 0x1)) {
        ddbs_unprj(p_bstrm, &tmp, 2);
    }
    if (acmod & 0x4) {
        ddbs_unprj(p_bstrm, &tmp, 2);
    }

    if (acmod == MODE20) {
        ddbs_unprj(p_bstrm, &tmp, 2);
    }
    ddbs_unprj(p_bstrm, &lfeon, 1);


    numch = chanary[acmod];
    if (0) {
        if (numch >= 3) {
            numch = 8;
        } else {
            numch = 2;
        }
    } else {
        numch = 2;
    }
    *ChNum = numch + lfeon;
    //ALOGI("DEBUG:numch=%d sample_rate=%d %p [%s %d]",ChNum,sample_rate,this,__FUNCTION__,__LINE__);
    return numch;
}

static int Get_DDP_Parameters(void *buf, int *sample_rate, int *frame_size, int *ChNum,int *ad_substream_supported)
{
    int numch = 0;
    DDP_BSTRM bstrm = {NULL, 0, 0};
    DDP_BSTRM *p_bstrm = &bstrm;
    short tmp = 0, acmod, lfeon, strmtyp,bsid;
    ddbs_init((short*) buf, 0, p_bstrm);
    ddbs_unprj(p_bstrm, &tmp, 16);
    if (tmp != SYNCWRD) {
        ALOGI("Invalid synchronization word");
        return -1;
    }

    ddbs_unprj(p_bstrm, &strmtyp, 2);
    ddbs_unprj(p_bstrm, &bsid, 3);
    ddbs_unprj(p_bstrm, &tmp, 11);
    if (strmtyp == 0 && bsid != 0) {
       *ad_substream_supported = 1;
    }
    *frame_size = 2 * (tmp + 1);
    if (strmtyp != 0 && strmtyp != 1 && strmtyp != 2) {
        return -1;
    }
    ddbs_unprj(p_bstrm, &tmp, 2);

    if (tmp == 0x3) {
        ALOGI("Half sample rate unsupported");
        return -1;
    } else {
        if (tmp == 0) {
            *sample_rate = 48000;
        } else if (tmp == 1) {
            *sample_rate = 44100;
        } else if (tmp == 2) {
            *sample_rate = 32000;
        }

        ddbs_unprj(p_bstrm, &tmp, 2);
    }
    ddbs_unprj(p_bstrm, &acmod, 3);
    ddbs_unprj(p_bstrm, &lfeon, 1);
    numch = chanary[acmod];
    //numch = 2;
    *ChNum = numch + lfeon;
    //ALOGI("DEBUG[%s %d]:numch=%d,sr=%d,frs=%d",__FUNCTION__,__LINE__,*ChNum,*sample_rate,*frame_size);
    return 0;
}

static DDPerr ddbs_skip(DDP_BSTRM   *p_bstrm, DDPshort    numbits)
{
    p_bstrm->bitptr += numbits;
    while (p_bstrm->bitptr >= BITSPERWRD) {
        p_bstrm->buf++;
        p_bstrm->data = *p_bstrm->buf;
        p_bstrm->bitptr -= BITSPERWRD;
    }
    return 0;
}

static DDPerr ddbs_getbsid(DDP_BSTRM *p_inbstrm,    DDPshort *p_bsid)
{
    DDP_BSTRM    bstrm;

    ddbs_init(p_inbstrm->buf, p_inbstrm->bitptr, &bstrm);
    ddbs_skip(&bstrm, BS_BITOFFSET);
    ddbs_unprj(&bstrm, p_bsid, 5);
    if (!ISDDP(*p_bsid) && !ISDD(*p_bsid)) {
        ALOGI("Unsupported bitstream id");
    }

    return 0;
}

static int Get_Parameters(void *buf, int *sample_rate, int *frame_size, int *ChNum, int *is_eac3,int *ad_substream_supported)
{
    DDP_BSTRM bstrm = {NULL, 0, 0};
    DDP_BSTRM *p_bstrm = &bstrm;
    DDPshort    bsid;
    int chnum = 0;
    uint8_t ptr8[PTR_HEAD_SIZE];

    memcpy(ptr8, buf, PTR_HEAD_SIZE);

    //ALOGI("LZG->ptr_head:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x \n",
    //     ptr8[0],ptr8[1],ptr8[2], ptr8[3],ptr8[4],ptr8[5] );
    if ((ptr8[0] == 0x0b) && (ptr8[1] == 0x77)) {
        int i;
        uint8_t tmp;
        for (i = 0; i < PTR_HEAD_SIZE; i += 2) {
            tmp = ptr8[i];
            ptr8[i] = ptr8[i + 1];
            ptr8[i + 1] = tmp;
        }
    }

    ddbs_init((short*) ptr8, 0, p_bstrm);
    int ret = ddbs_getbsid(p_bstrm, &bsid);
    if (ret < 0) {
        return -1;
    }
    if (ISDDP(bsid)) {
        Get_DDP_Parameters(ptr8, sample_rate, frame_size, ChNum, ad_substream_supported);
        *is_eac3 = 1;
    } else if (ISDD(bsid)) {
        Get_DD_Parameters(ptr8, sample_rate, frame_size, ChNum);
        *is_eac3 = 0;
    }
    return 0;
}

static  int unload_ddp_decoder_lib()
{
    if (ddp_decoder_cleanup != NULL && handle != NULL) {
        (*ddp_decoder_cleanup)(handle);
        handle = NULL;
    }
    ddp_decoder_init = NULL;
    ddp_decoder_process = NULL;
    ddp_decoder_cleanup = NULL;
    ddp_decoder_config = NULL;
    if (gDDPDecoderLibHandler != NULL) {
        dlclose(gDDPDecoderLibHandler);
        gDDPDecoderLibHandler = NULL;
    }
    return 0;
}

static int dcv_decoder_init(int decoding_mode, aml_dec_control_type_t digital_raw)
{
    int input_mode = 1;
    gDDPDecoderLibHandler = dlopen(DOLBY_DCV_LIB_PATH_A, RTLD_NOW);
    if (!gDDPDecoderLibHandler) {
        ALOGE("%s, failed to open (libstagefright_soft_dcvdec.so), %s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[gDDPDecoderLibHandler]", __FUNCTION__, __LINE__);
    }

    ddp_decoder_init = (int (*)(int, int,void **)) dlsym(gDDPDecoderLibHandler, "ddp_decoder_init");
    if (ddp_decoder_init == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[ddp_decoder_init:]", __FUNCTION__, __LINE__);
    }

    ddp_decoder_process = (int (*)(char * , int , int *, int , char *, int *, struct pcm_info *, char *, int *,void *))
                          dlsym(gDDPDecoderLibHandler, "ddp_decoder_process");
    if (ddp_decoder_process == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[ddp_decoder_process:]", __FUNCTION__, __LINE__);
    }

    ddp_decoder_cleanup = (int (*)(void *)) dlsym(gDDPDecoderLibHandler, "ddp_decoder_cleanup");
    if (ddp_decoder_cleanup == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        goto Error;
    } else {
        ALOGV("<%s::%d>--[ddp_decoder_cleanup:]", __FUNCTION__, __LINE__);
    }

    ddp_decoder_config = (int (*)(void *, ddp_config_type_t, ddp_config_t *)) dlsym(gDDPDecoderLibHandler, "ddp_decoder_config");
    if (ddp_decoder_config == NULL) {
        ALOGE("%s,can not find decoder config function,%s\n", __FUNCTION__, dlerror());
    } else {
        ALOGV("<%s::%d>--[ddp_decoder_config:]", __FUNCTION__, __LINE__);
    }

    (*ddp_decoder_init)(decoding_mode, digital_raw, &handle);
    return 0;
Error:
    unload_ddp_decoder_lib();
    return -1;
}

static int dcv_decode_process(unsigned char*input, int input_size, unsigned char *outbuf,
                              int *out_size, char *spdif_buf, int *raw_size, int nIsEc3,
                              struct pcm_info *pcm_out_info)
{
    int outputFrameSize = 0;
    int used_size = 0;
    int decoded_pcm_size = 0;
    int ret = -1;

    if (ddp_decoder_process == NULL) {
        return ret;
    }

    ret = (*ddp_decoder_process)((char *) input
                                 , input_size
                                 , &used_size
                                 , nIsEc3
                                 , (char *) outbuf
                                 , out_size
                                 , pcm_out_info
                                 , (char *) spdif_buf
                                 , (int *) raw_size
                                 ,handle);
    ALOGV("used_size %d,lpcm out_size %d,raw out size %d",used_size,*out_size,*raw_size);
    return used_size;
}

int Write_buffer(struct aml_audio_parser *parser, unsigned char *buffer, int size)
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

#define MAX_DECODER_FRAME_LENGTH 6144
#define READ_PERIOD_LENGTH 2048
#define MAX_DDP_FRAME_LENGTH 2560
#define MAX_DDP_BUFFER_SIZE (MAX_DECODER_FRAME_LENGTH * 4 + MAX_DECODER_FRAME_LENGTH + 8)

void *decode_threadloop(void *data __unused)
{
#if 0
    struct aml_audio_parser *parser = (struct aml_audio_parser *) data;
    unsigned char *outbuf = NULL;
    unsigned char *inbuf = NULL;
    unsigned char *outbuf_raw = NULL;
    unsigned char *read_pointer = NULL;
    int valid_lib = 1;
    int nIsEc3  = 0;
    int outlen = 0;
    int outlen_raw = 0;
    int outlen_pcm = 0;
    int remain_size = 0;
    int used_size = 0;
    int read_size = READ_PERIOD_LENGTH;
    int ret = 0;
    int mSample_rate = 0;
    int mFrame_size = 0;
    int is_eac3 = 0;
    int mChNum = 0;
    int in_sync = 0;
    unsigned char temp;
    int i,ad_substream_supported = 0;
    aml_dec_control_type_t digital_raw = AML_DEC_CONTROL_DECODING;

    ALOGI ("++ %s, in_sr = %d, out_sr = %d\n", __func__, parser->in_sample_rate, parser->out_sample_rate);
    outbuf = (unsigned char*) aml_audio_malloc (MAX_DDP_BUFFER_SIZE);

    if (!outbuf) {
        ALOGE("malloc buffer failed\n");
        return NULL;
    }
    outbuf_raw = outbuf + MAX_DECODER_FRAME_LENGTH;

    inbuf = (unsigned char *) aml_audio_malloc(MAX_DDP_FRAME_LENGTH + read_size);
    if (!inbuf) {
        ALOGE("malloc inbuf failed\n");
        aml_audio_free(outbuf);
        return NULL;
    }

    ret = dcv_decoder_init(DDP_DECODE_MODE_SINGLE, digital_raw);
    if (ret) {
        ALOGW("dec init failed, maybe no lisensed ddp decoder.\n");
        valid_lib = 0;
    }
    //parser->decode_enabled = 1;

    if (parser->in_sample_rate != parser->out_sample_rate) {
        parser->aml_resample.input_sr = parser->in_sample_rate;
        parser->aml_resample.output_sr = parser->out_sample_rate;
        parser->aml_resample.channels = 2;
        resampler_init(&parser->aml_resample);
    }

    prctl(PR_SET_NAME, (unsigned long)"audio_dcv_dec");
    while (parser->decode_ThreadExitFlag == 0) {
        outlen = 0;
        outlen_raw = 0;
        outlen_pcm = 0;
        in_sync = 0;

        if (parser->decode_ThreadExitFlag == 1) {
            ALOGI("%s, exit threadloop! \n", __func__);
            break;
        }

        if (parser->aformat == AUDIO_FORMAT_E_AC3) {
            nIsEc3 = 1;
        } else {
            nIsEc3 = 0;
        }

        //here we call decode api to decode audio frame here
        if (remain_size < MAX_DDP_FRAME_LENGTH) {
            //pthread_mutex_lock(parser->decode_dev_op_mutex);
            ret = pcm_read(parser->aml_pcm, inbuf + remain_size, read_size);
            //pthread_mutex_unlock(parser->decode_dev_op_mutex);
            if (ret < 0) {
                usleep(1000);  //1ms
                continue;
            }
            remain_size += read_size;
        }

        if (valid_lib == 0) {
            continue;
        }

        //find header and get paramters
        read_pointer = inbuf;
        while (parser->decode_ThreadExitFlag == 0 && remain_size > 16) {
            if ((read_pointer[0] == 0x0b && read_pointer[1] == 0x77) || \
                (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b)) {
                Get_Parameters(read_pointer, &mSample_rate, &mFrame_size, &mChNum, &is_eac3, &ad_substream_supported);
                if ((mFrame_size == 0) || (mFrame_size < PTR_HEAD_SIZE) || \
                    (mChNum == 0) || (mSample_rate == 0)) {
                    ALOGI("error dolby frame !!!");
                } else {
                    in_sync = 1;
                    break;
                }
            }
            remain_size--;
            read_pointer++;
        }
        //ALOGI("remain %d, frame size %d\n", remain_size, mFrame_size);

        if (remain_size < mFrame_size || in_sync == 0) {
            //if (in_sync == 1)
            //    ALOGI("remain %d,frame size %d, read more\n",remain_size,mFrame_size);
            memcpy(inbuf, read_pointer, remain_size);
            continue;
        }

        //ALOGI("frame size %d,remain %d\n",mFrame_size,remain_size);
        //do the endian conversion
        if (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b) {
            for (i = 0; i < mFrame_size / 2; i++) {
                temp = read_pointer[2 * i + 1];
                read_pointer[2 * i + 1] = read_pointer[2 * i];
                read_pointer[2 * i] = temp;
            }
        }

        used_size = dcv_decode_process(read_pointer, mFrame_size, outbuf,
                                       &outlen_pcm, (char *) outbuf_raw, &outlen_raw, nIsEc3, &parser->pcm_out_info);
        if (used_size > 0) {
            remain_size -= used_size;
            memcpy(inbuf, read_pointer + used_size, remain_size);
        }

        //ALOGI("outlen_pcm: %d,outlen_raw: %d\n", outlen_pcm, outlen_raw);

        //only need pcm data
        if (outlen_pcm > 0) {
            // here only downresample, so no need to malloc more buffer
            if (parser->in_sample_rate != parser->out_sample_rate) {
                int out_frame = outlen_pcm >> 2;
                out_frame = resample_process (&parser->aml_resample, out_frame, (int16_t*) outbuf, (int16_t*) outbuf);
                outlen_pcm = out_frame << 2;
            }
            parser->data_ready = 1;
            Write_buffer(parser, outbuf, outlen_pcm);
        }
    }

    parser->decode_enabled = 0;
    if (inbuf) {
        aml_audio_free(inbuf);
    }
    if (outbuf) {
        aml_audio_free(outbuf);
    }

    unload_ddp_decoder_lib();
    ALOGI("-- %s\n", __func__);
#endif
    return NULL;
}

static int start_decode_thread(struct aml_audio_parser *parser)
{
    int ret = 0;

    ALOGI("++ %s\n", __func__);
    parser->decode_enabled = 1;
    parser->decode_ThreadExitFlag = 0;
    ret = pthread_create(&parser->decode_ThreadID, NULL, &decode_threadloop, parser);
    if (ret != 0) {
        ALOGE("%s, Create thread fail!\n", __FUNCTION__);
        return -1;
    }
    ALOGI("-- %s\n", __func__);
    return 0;
}

static int stop_decode_thread(struct aml_audio_parser *parser)
{
    ALOGI("++ %s \n", __func__);
    parser->decode_ThreadExitFlag = 1;
    pthread_join(parser->decode_ThreadID, NULL);
    parser->decode_ThreadID = 0;
    ALOGI("-- %s \n", __func__);
    return 0;
}

int dcv_decode_init(struct aml_audio_parser *parser)
{
    return start_decode_thread(parser);
}

int dcv_decode_release(struct aml_audio_parser *parser)
{
    return stop_decode_thread(parser);
}

int dcv_decoder_init_patch(aml_dec_t ** ppaml_dec, aml_dec_config_t * dec_config)
{
    struct dolby_ddp_dec *ddp_dec = NULL;
    aml_dec_t  *aml_dec = NULL;
    aml_dcv_config_t *dcv_config = (aml_dcv_config_t *)dec_config;
    int ret = 0;

    if (dcv_config->decoding_mode != DDP_DECODE_MODE_SINGLE &&
        dcv_config->decoding_mode != DDP_DECODE_MODE_AD_DUAL &&
        dcv_config->decoding_mode != DDP_DECODE_MODE_AD_SUBSTREAM) {
        ALOGE("wrong decoder mode =%d", dcv_config->decoding_mode);
        goto error;
    }

    ddp_dec = aml_audio_calloc(1, sizeof(struct dolby_ddp_dec));
    if (ddp_dec == NULL) {
        ALOGE("malloc ddp_dec failed\n");
        goto error;
    }

    aml_dec = &ddp_dec->aml_dec;

    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;


    ddp_dec->decoding_mode = dcv_config->decoding_mode;
    ddp_dec->digital_raw   = dcv_config->digital_raw;
    ddp_dec->nIsEc3        = dcv_config->nIsEc3;
    ddp_dec->is_iec61937   = dcv_config->is_iec61937;

    aml_dec->format = dcv_config->format;


    ret = dcv_decoder_init(ddp_dec->decoding_mode, ddp_dec->digital_raw);
    ALOGI("dcv_decoder_init decoding mode =%d, ddp_dec->digital_raw=%d ret =%d", ddp_dec->decoding_mode, ddp_dec->digital_raw, ret);
    if (ret < 0) {
        goto error;
    }

    ddp_dec->status = 1;
    ddp_dec->remain_size = 0;
    ddp_dec->outlen_pcm = 0;
    ddp_dec->outlen_raw = 0;
    ddp_dec->curFrmSize = 0;
    ddp_dec->inbuf = NULL;

    ddp_dec->dcv_pcm_writed = 0;
    ddp_dec->dcv_decoded_samples = 0;
    ddp_dec->dcv_decoded_errcount = 0;
    memset(ddp_dec->sysfs_buf, 0, sizeof(ddp_dec->sysfs_buf));

    memset(&ddp_dec->pcm_out_info, 0, sizeof(struct pcm_info));
    memset(&ddp_dec->aml_resample, 0, sizeof(struct resample_para));
    ddp_dec->inbuf_size = MAX_DECODER_FRAME_LENGTH * 4 * 4;
    ddp_dec->inbuf = (unsigned char*) aml_audio_calloc(1, ddp_dec->inbuf_size);

    if (!ddp_dec->inbuf) {
        ALOGE("malloc buffer failed\n");
        ddp_dec->inbuf_size = 0;

        goto error;
    }

    dec_pcm_data->buf_size = MAX_DECODER_FRAME_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto error;
    }

    dec_raw_data->buf_size = MAX_DECODER_FRAME_LENGTH * 4 + 8;
    dec_raw_data->buf = (unsigned char*) aml_audio_calloc(1, dec_raw_data->buf_size);
    if (!dec_raw_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto error;
    }

    raw_in_data->buf_size = MAX_DECODER_FRAME_LENGTH;
    raw_in_data->buf = (unsigned char*) aml_audio_calloc(1, raw_in_data->buf_size);
    if (!raw_in_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto error;
    }



    ddp_dec->decoder_process = dcv_decode_process;
    ddp_dec->get_parameters = Get_Parameters;

    * ppaml_dec = (aml_dec_t *)ddp_dec;
    ALOGI("%s success", __func__);
    return 0;

error:
    if (ddp_dec) {
        if (ddp_dec->inbuf) {
            aml_audio_free(ddp_dec->inbuf);
        }

        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }

        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
        }

        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
        }

        aml_audio_free(ddp_dec);
    }
    * ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;


}

int dcv_decoder_release_patch(aml_dec_t * aml_dec)
{
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_dec;

    if (aml_dec == NULL) {
        ALOGE("%s aml_dec NULL", __func__);
        return -1;
    }

    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;

    if (ddp_decoder_cleanup != NULL && handle != NULL) {
        (*ddp_decoder_cleanup)(handle);
        handle = NULL;
    }

    if (ddp_dec && ddp_dec->status == 1) {
        ddp_dec->status = 0;
        ddp_dec->inbuf_size = 0;
        ddp_dec->remain_size = 0;
        ddp_dec->outlen_pcm = 0;
        ddp_dec->outlen_raw = 0;
        ddp_dec->nIsEc3 = 0;
        ddp_dec->curFrmSize = 0;
        ddp_dec->decoding_mode = DDP_DECODE_MODE_SINGLE;
        ddp_dec->mixer_level = 0;
        ddp_dec->dcv_pcm_writed = 0;
        ddp_dec->dcv_decoded_samples = 0;
        ddp_dec->dcv_decoded_errcount = 0;
        memset(ddp_dec->sysfs_buf, 0, sizeof(ddp_dec->sysfs_buf));
        aml_audio_free(ddp_dec->inbuf);
        ddp_dec->inbuf = NULL;
        ddp_dec->get_parameters = NULL;
        ddp_dec->decoder_process = NULL;
        memset(&ddp_dec->pcm_out_info, 0, sizeof(struct pcm_info));
        //memset(&ddp_dec->aml_resample, 0, sizeof(struct resample_para));

        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
            dec_pcm_data->buf = NULL;
        }

        if (dec_raw_data->buf) {
            aml_audio_free(dec_raw_data->buf);
            dec_raw_data->buf = NULL;
        }
        if (raw_in_data->buf) {
            aml_audio_free(raw_in_data->buf);
            raw_in_data->buf = NULL;
        }
        aml_audio_free(ddp_dec);
    }
    ALOGI("%s exit", __func__);
    return 0;
}
#define IEC61937_HEADER_SIZE 8
int dcv_decoder_get_framesize(unsigned char*buffer, int bytes, int* p_head_offset)
{
    int offset = 0;
    unsigned char *read_pointer = buffer;
    int mSample_rate = 0;
    int mFrame_size = 0;
    int mChNum = 0;
    int is_eac3 = 0;
    int ad_substream_supported = 0;
    ALOGV("%s %x %x\n", __FUNCTION__, read_pointer[0], read_pointer[1]);

    while (offset <  bytes -1) {
        if ((read_pointer[0] == 0x0b && read_pointer[1] == 0x77) || \
                    (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b)) {
            Get_Parameters(read_pointer, &mSample_rate, &mFrame_size, &mChNum,&is_eac3, &ad_substream_supported);
            *p_head_offset = offset;
            ALOGV("%s mFrame_size %d offset %d\n", __FUNCTION__, mFrame_size, offset);
            return mFrame_size;
        }
        offset++;
        read_pointer++;
    }
    return 0;
}
int is_ad_substream_supported(unsigned char *buffer,int write_len) {
    int offset = 0;
    unsigned char *read_pointer = buffer;
    int mSample_rate = 0;
    int mFrame_size = 0;
    int mChNum = 0;
    int is_eac3 = 0;
    int ad_substream_supported = 0;
    ALOGV("%s %x %x\n", __FUNCTION__, read_pointer[0], read_pointer[1]);

    while (offset <  write_len -1) {
        if ((read_pointer[0] == 0x0b && read_pointer[1] == 0x77) || \
                    (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b)) {
            Get_Parameters(read_pointer, &mSample_rate, &mFrame_size, &mChNum,&is_eac3, &ad_substream_supported);
            ALOGI("%s ad_substream_supported %d offset %d\n", __FUNCTION__, ad_substream_supported, offset);
            if (ad_substream_supported)
               return ad_substream_supported;
        }
        offset++;
        read_pointer++;
    }
    return 0;

}
int dcv_decoder_process_patch(aml_dec_t * aml_dec, unsigned char *buffer, int bytes)
{
    int mSample_rate = 0;
    int mFrame_size = 0;
    int mChNum = 0;
    int is_eac3 = 0;
    int in_sync = 0;
    int used_size = 0;
    int i = 0;
    unsigned char *read_pointer = NULL;
    size_t main_frame_deficiency = 0;
    int ret = 0;
    unsigned char temp;
    int outPCMLen = 0;
    int outRAWLen = 0;
    int total_size = 0;
    int read_offset = 0;
    int ad_substream_supported = 0;
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_dec;

    ddp_dec->outlen_pcm = 0;
    ddp_dec->outlen_raw = 0;
    int bit_width = 16;

    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * dec_raw_data = &aml_dec->dec_raw_data;
    dec_data_info_t * raw_in_data  = &aml_dec->raw_in_data;

    dec_pcm_data->data_len = 0;
    dec_raw_data->data_len = 0;
    raw_in_data->data_len  = 0;

#if 0
    if (getprop_bool("vendor.media.audio_hal.ddp.outdump")) {
        FILE *fp1 = fopen("/data/tmp/audio_decode_61937.raw", "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, bytes, fp1);
            fclose(fp1);
        } else {
            ALOGD("could not open files!");
        }
    }
#endif


    /* dual input is from dtv, and it is similar with MS12,
     * main and associate is packaged with IEC61937
     */
    if (ddp_dec->decoding_mode == DDP_DECODE_MODE_AD_DUAL) {
        int dual_decoder_used_bytes = 0;
        int dual_input_ret = 0;
        void *main_frame_buffer = NULL;
        int main_frame_size = 0;
        void *associate_frame_buffer = NULL;
        int associate_frame_size = 0;
        dual_input_ret = scan_dolby_main_associate_frame(buffer
                 , bytes
                 , &dual_decoder_used_bytes
                 , &main_frame_buffer
                 , &main_frame_size
                 , &associate_frame_buffer
                 , &associate_frame_size);
        if (dual_input_ret) {
            ALOGE("%s used size %d dont find the iec61937 format header, rescan next time!\n", __FUNCTION__, dual_decoder_used_bytes);
            goto EXIT;
        }
        ALOGV("main frame size =%d ad frame size =%d", main_frame_size, associate_frame_size);
        if ((main_frame_size + associate_frame_size) > ddp_dec->inbuf_size) {
            ALOGE("too big frame size =%d %d", main_frame_size, associate_frame_size);
            goto EXIT;
        }
        /* copy main data */
        memcpy((char *)ddp_dec->inbuf, main_frame_buffer, main_frame_size);
        /* copy ad data */
        memcpy((char *)ddp_dec->inbuf + main_frame_size, associate_frame_buffer, associate_frame_size);
        ddp_dec->remain_size = main_frame_size + associate_frame_size;
        mFrame_size = main_frame_size + associate_frame_size;
        read_pointer = ddp_dec->inbuf;
        read_offset = 0;
        in_sync = 1;
    }
    else {
        //check if the buffer overflow
        if ((ddp_dec->remain_size + bytes) > ddp_dec->inbuf_size) {
            ALOGE("too big input size =%d %d", ddp_dec->remain_size, bytes);
            goto EXIT;
        }
        memcpy((char *)ddp_dec->inbuf + ddp_dec->remain_size, (char *)buffer, bytes);
        ddp_dec->remain_size += bytes;
        total_size = ddp_dec->remain_size;
        read_pointer = ddp_dec->inbuf;

        //check the sync word of dolby frames
        if (ddp_dec->is_iec61937 == false) {
            while (ddp_dec->remain_size > 16) {
                if ((read_pointer[0] == 0x0b && read_pointer[1] == 0x77) || \
                    (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b)) {
                    Get_Parameters(read_pointer, &mSample_rate, &mFrame_size, &mChNum,&is_eac3, &ad_substream_supported);
                    if ((mFrame_size == 0) || (mFrame_size < PTR_HEAD_SIZE) || \
                        (mChNum == 0) || (mSample_rate == 0)) {
                    } else {
                        in_sync = 1;
                        ddp_dec->sourcesr = mSample_rate;
                        ddp_dec->sourcechnum = mChNum;
                        break;
                    }
                }
                ddp_dec->remain_size--;
                read_pointer++;
            }
        } else {
            //if the dolby audio is contained in 61937 format. for perfermance issue,
            //re-check the PaPbPcPd
            //TODO, we need improve the perfermance issue from the whole pipeline,such
            //as read/write burst size optimization(DD/6144,DD+ 24576..) as some

        while (ddp_dec->remain_size > 16) {
            if ((read_pointer[0] == 0x72 && read_pointer[1] == 0xf8 && read_pointer[2] == 0x1f && read_pointer[3] == 0x4e)||
                   (read_pointer[0] == 0x4e && read_pointer[1] == 0x1f && read_pointer[2] == 0xf8 && read_pointer[3] == 0x72)) {
                        unsigned int pcpd = *(uint32_t*)(read_pointer  + 4);
                        int pc = (pcpd & 0x1f);
                        if (pc == 0x15) {
                            mFrame_size = (pcpd >> 16);
                            in_sync = 1;
                            break;
                        } else if (pc == 0x1) {
                            mFrame_size = (pcpd >> 16) / 8;
                            in_sync = 1;
                            break;
                        }
                }
                ddp_dec->remain_size--;
                read_pointer++;
            }
            read_offset = 8;
        }
    }
    ALOGV("remain %d, frame size %d,in sync %d\n", ddp_dec->remain_size, mFrame_size, in_sync);

    //we do not have one complete dolby frames.we need cache the
    //data and combine with the next input data.
    if (ddp_dec->remain_size < mFrame_size || in_sync == 0) {
        //ALOGI("remain %d,frame size %d, read more\n",remain_size,mFrame_size);
        memcpy(ddp_dec->inbuf, read_pointer, ddp_dec->remain_size);
        return AML_DEC_RETURN_TYPE_CACHE_DATA;
    }
    ddp_dec->curFrmSize = mFrame_size;
    read_pointer += read_offset;
    ddp_dec->remain_size -= read_offset;
    ALOGV("frame size %d,remain %d\n",mFrame_size,ddp_dec->remain_size);
    //do the endian conversion
    if (read_pointer[0] == 0x77 && read_pointer[1] == 0x0b) {
        for (i = 0; i < mFrame_size / 2; i++) {
            temp = read_pointer[2 * i + 1];
            read_pointer[2 * i + 1] = read_pointer[2 * i];
            read_pointer[2 * i] = temp;
        }
    }
    Get_Parameters(read_pointer, &mSample_rate, &mFrame_size, &mChNum,&is_eac3, &ad_substream_supported);

    if (raw_in_data->buf_size >= mFrame_size) {
        raw_in_data->data_len = mFrame_size;
        memcpy(raw_in_data->buf, read_pointer, mFrame_size);

    } else {
        ALOGE("too  big frame size =%d", mFrame_size);
    }

    while (mFrame_size > 0) {
        outPCMLen = 0;
        outRAWLen = 0;
        int current_size = 0;
        ALOGV("ddp_dec->outlen_pcm=%d raw len=%d in =%p dec_pcm_data->buf=%p dec_raw_data->buf=%p",
            ddp_dec->outlen_pcm, ddp_dec->outlen_raw, read_pointer, dec_pcm_data->buf, dec_raw_data->buf);
        current_size = dcv_decode_process((unsigned char*)read_pointer + used_size,
                                             mFrame_size,
                                             (unsigned char *)dec_pcm_data->buf+ ddp_dec->outlen_pcm,
                                             &outPCMLen,
                                             (char *)dec_raw_data->buf + ddp_dec->outlen_raw,
                                             &outRAWLen,
                                             ddp_dec->nIsEc3,
                                             &ddp_dec->pcm_out_info);
        used_size += current_size;
        ddp_dec->outlen_pcm += outPCMLen;
        ddp_dec->outlen_raw += outRAWLen;
        if (used_size > 0)
            mFrame_size -= current_size;
    }
    if (used_size > 0) {
        ddp_dec->remain_size -= used_size;
        memcpy(ddp_dec->inbuf, read_pointer + used_size, ddp_dec->remain_size);
    }
#if 0
    if (getprop_bool("vendor.media.audio_hal.ddp.outdump")) {
        FILE *fp1 = fopen("/data/tmp/audio_decode.raw", "a+");
        if (fp1) {
            int flen = fwrite((char *)ddp_dec->outbuf, 1, ddp_dec->outlen_pcm, fp1);
            fclose(fp1);
        } else {
            ALOGD("could not open files!");
        }
    }
#endif

    if (ddp_dec->outlen_pcm > 0) {
        dec_pcm_data->data_format = AUDIO_FORMAT_PCM_16_BIT;
        dec_pcm_data->data_ch     = 2;
        dec_pcm_data->data_sr     = ddp_dec->pcm_out_info.sample_rate;
        dec_pcm_data->data_len    = ddp_dec->outlen_pcm;
    }

    if (ddp_dec->outlen_raw > 0) {
        dec_raw_data->data_format = AUDIO_FORMAT_IEC61937;
        if (aml_dec->format == AUDIO_FORMAT_E_AC3 && (ddp_dec->digital_raw == AML_DEC_CONTROL_CONVERT)) {
            dec_raw_data->sub_format = AUDIO_FORMAT_AC3;
        } else {
            dec_raw_data->sub_format = aml_dec->format;
        }
        dec_raw_data->data_ch     = 2;
        dec_raw_data->data_sr     = ddp_dec->pcm_out_info.sample_rate;
        dec_raw_data->data_len    = ddp_dec->outlen_raw;
    } else {
        ddp_dec->dcv_decoded_errcount++;
        //sprintf(ddp_dec->sysfs_buf, "decoded_err %d", ddp_dec->dcv_decoded_errcount);
        //sysfs_set_sysfs_str(REPORT_DECODED_INFO, ddp_dec->sysfs_buf);
    }

    /*it stores the input raw data*/
    if (raw_in_data->data_len > 0) {
        raw_in_data->data_format = aml_dec->format;
        raw_in_data->sub_format  = aml_dec->format;
        raw_in_data->data_sr     = mSample_rate;
        /*we don't support 32k ddp*/
        if (raw_in_data->data_sr == 32000) {
            raw_in_data->data_len = 0;
        }
    }

    ddp_dec->dcv_pcm_writed += ddp_dec->outlen_pcm;
    ddp_dec->dcv_decoded_samples = (ddp_dec->dcv_pcm_writed * 8 ) / (2 * bit_width);
    //sprintf(ddp_dec->sysfs_buf, "decoded_frames %d", ddp_dec->dcv_decoded_samples);
    //sysfs_set_sysfs_str(REPORT_DECODED_INFO, ddp_dec->sysfs_buf);

    return 0;
EXIT:
    return -1;
}

int dcv_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_dec;

    if (ddp_decoder_config == NULL || handle == NULL) {
        return ret;
    }
    switch (config_type) {
    case AML_DEC_CONFIG_MIXER_LEVEL: {
        int  mixer_level = dec_config->mixer_level;
        ret = (*ddp_decoder_config)(handle, DDP_CONFIG_MIXER_LEVEL, (ddp_config_t *)&mixer_level);
        break;
    }
    default:
        break;
    }

    return ret;
}

int dcv_decoder_info(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{
    int ret = -1;
    struct dolby_ddp_dec *ddp_dec = (struct dolby_ddp_dec *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        dec_info->remain_size = ddp_dec->remain_size;
        return 0;
    default:
        break;
    }
    return ret;
}



aml_dec_func_t aml_dcv_func = {
    .f_init                 = dcv_decoder_init_patch,
    .f_release              = dcv_decoder_release_patch,
    .f_process              = dcv_decoder_process_patch,
    .f_config               = dcv_decoder_config,
    .f_info                 = dcv_decoder_info,
};

