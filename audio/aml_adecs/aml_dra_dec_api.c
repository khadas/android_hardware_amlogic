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

#define LOG_TAG "aml_audio_dra_dec"
//#define LOG_NDEBUG 0

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "audio_data_process.h"
#include "aml_malloc_debug.h"

#if ANDROID_PLATFORM_SDK_VERSION > 29
#define DRA_LIB_PATH "/odm/lib/libdra.so"
#else
#define DRA_LIB_PATH "/vendor/lib/libdra.so"
#endif

#define DRA_MAX_LENGTH (1024 * 64)
#define DRA_REMAIN_BUFFER_SIZE (4096 * 10)
#define DRA_AD_NEED_CACHE_FRAME_COUNT  2

typedef struct _audio_info {
    int bitrate;
    int samplerate;
    int channels;
    int file_profile;
    int error_num;
} AudioInfo;

typedef struct dra_decoder_operations {
    const char * name;
    int nAudioDecoderType;
    int nInBufSize;
    int nOutBufSize;
    int (*init)(void *);
    int (*decode)(void *, char *outbuf, int *outlen, char *inbuf, int inlen);
    int (*release)(void *);
    int (*getinfo)(void *, AudioInfo *pAudioInfo);
    void * priv_data;//point to audec
    void * priv_dec_data;//decoder private data
    void *pdecoder; // decoder instance
    int channels;
    unsigned long pts;
    int samplerate;
    int bps;
    int extradata_size; ///< extra data size
    char extradata[4096];
    int NchOriginal;
    int lfepresent;
} dra_decoder_operations_t;

struct dra_dec_t {
    aml_dec_t  aml_dec;
    aml_dra_config_t dra_config;
    dra_decoder_operations_t dra_op;
    dra_decoder_operations_t ad_dra_op;
    void *pdecoder;
    char remain_data[DRA_REMAIN_BUFFER_SIZE];
    int remain_size;
    bool ad_decoder_supported;
    bool ad_mixing_enable;
    int advol_level;
    int  mixer_level;
    char ad_remain_data[DRA_REMAIN_BUFFER_SIZE];
    int ad_need_cache_frames;
    int ad_remain_size;
};

static  int unload_dra_decoder_lib(struct dra_dec_t *dra_dec)
{
    dra_decoder_operations_t *dra_op = &dra_dec->dra_op;
    dra_decoder_operations_t *ad_dra_op = &dra_dec->ad_dra_op;
    if (dra_op != NULL) {
        dra_op->init = NULL;
        dra_op->decode = NULL;
        dra_op->release = NULL;
        dra_op->getinfo = NULL;
    }

    if (ad_dra_op != NULL) {
        ad_dra_op->init = NULL;
        ad_dra_op->decode = NULL;
        ad_dra_op->release = NULL;
        ad_dra_op->getinfo = NULL;
    }
    if (dra_dec->pdecoder != NULL) {
        dlclose(dra_dec->pdecoder);
        dra_dec->pdecoder = NULL;
    }
    return 0;
}


static  int load_dra_decoder_lib(struct dra_dec_t *dra_dec)
{
    dra_decoder_operations_t *dra_op = &dra_dec->dra_op;
    dra_decoder_operations_t *ad_dra_op = &dra_dec->ad_dra_op;
    dra_dec->pdecoder = dlopen(DRA_LIB_PATH, RTLD_NOW);
    if (!dra_dec->pdecoder) {
        ALOGE("%s, failed to open (libdra.so), %s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[dra_op->pdecoder]", __FUNCTION__, __LINE__);
    }

    dra_op->init = ad_dra_op->init = (int (*)(void *)) dlsym(dra_dec->pdecoder, "audio_dec_init");
    if (dra_op->init == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d> audio_dec_init", __FUNCTION__, __LINE__);
    }

    dra_op->decode = ad_dra_op->decode = (int (*)(void *, char * outbuf, int * outlen, char * inbuf, int inlen))
                                         dlsym(dra_dec->pdecoder, "audio_dec_decode");
    if (dra_op->decode  == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1 ;
    } else {
        ALOGV("<%s::%d>--[audio_dec_decode:]", __FUNCTION__, __LINE__);
    }

    dra_op->release = ad_dra_op->release = (int (*)(void *)) dlsym(dra_dec->pdecoder, "audio_dec_release");
    if (dra_op->release == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_release:]", __FUNCTION__, __LINE__);
    }

    dra_op->getinfo = ad_dra_op->getinfo = (int (*)(void *, AudioInfo * pAudioInfo)) dlsym(dra_dec->pdecoder, "audio_dec_getinfo");
    if (dra_op->getinfo == NULL) {
        ALOGI("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_getinfo:]", __FUNCTION__, __LINE__);
    }

    return 0;
}

static int dra_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct dra_dec_t *dra_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_dra_config_t *dra_config = NULL;
    dec_data_info_t *dec_dra_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("dec config is NULL\n");
        return -1;
    }
    dra_config = &dec_config->dra_config;

    /*if (dra_config->channel <= 0 || dra_config->channel > 8) {
        ALOGE("dra config channel is invalid=%d\n", dra_config->channel);
        return -1;
    }

    if (dra_config->samplerate <= 0 ||dra_config->samplerate > 192000) {
        ALOGE("dra config samplerate is invalid=%d\n", dra_config->samplerate);
        return -1;
    }*/

    if (dra_config->channel == 0) {
        dra_config->channel = 2;
    }
    if (dra_config->samplerate == 0) {
        dra_config->samplerate = 48000;
    }
    if (dra_config->dra_format == 0) {
        dra_config->dra_format = AUDIO_FORMAT_DRA;
    }

    dra_dec = aml_audio_calloc(1, sizeof(struct dra_dec_t));
    if (dra_dec == NULL) {
        ALOGE("malloc dra_dec failed\n");
        return -1;
    }

    aml_dec = &dra_dec->aml_dec;

    memcpy(&dra_dec->dra_config, dra_config, sizeof(aml_dra_config_t));
    ALOGI("%s dra format=%x samplerate =%d ch=%d\n", __FUNCTION__,
          dra_config->dra_format, dra_config->samplerate, dra_config->channel);

    dec_dra_data = &aml_dec->dec_pcm_data;
    dec_dra_data->buf_size = DRA_MAX_LENGTH;
    dec_dra_data->buf = (unsigned char*) aml_audio_calloc(1, dec_dra_data->buf_size);
    if (!dec_dra_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }

    ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    ad_dec_pcm_data->buf_size = DRA_MAX_LENGTH;
    ad_dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, ad_dec_pcm_data->buf_size);
    if (!ad_dec_pcm_data->buf) {
        ALOGE("malloc ad buffer failed\n");
        goto exit;
    }

    ALOGI("ad_dec_pcm_data->buf %p", ad_dec_pcm_data->buf);
    if (load_dra_decoder_lib(dra_dec) == 0) {

        int ret = dra_dec->dra_op.init((void *)&dra_dec->dra_op);
        if (ret != 0) {
            ALOGI("dra decoder init failed !");
            goto exit;
        }
        ret = dra_dec->ad_dra_op.init((void *)&dra_dec->ad_dra_op);
        if (ret != 0) {
            ALOGI("dra decoder init failed !");
            goto exit;
        }
    } else {
        goto exit;
    }
    aml_dec->status = 1;
    dra_dec->ad_need_cache_frames = DRA_AD_NEED_CACHE_FRAME_COUNT;
    *ppaml_dec = (aml_dec_t *)dra_dec;
    dra_dec->ad_decoder_supported = dec_config->ad_decoder_supported;
    dra_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
    dra_dec->mixer_level = dec_config->mixer_level;
    dra_dec->advol_level = dec_config->advol_level;

    ALOGI("dra_dec->ad_decoder_supported %d", dra_dec->ad_decoder_supported);
    dra_dec->remain_size = 0;
    memset(dra_dec->remain_data , 0 , DRA_REMAIN_BUFFER_SIZE * sizeof(char));
    dra_dec->ad_remain_size = 0;
    memset(dra_dec->ad_remain_data , 0 , DRA_REMAIN_BUFFER_SIZE * sizeof(char));
    ALOGE("%s success", __func__);
    return 0;

exit:
    if (dra_dec) {
        if (dec_dra_data->buf) {
            aml_audio_free(dec_dra_data->buf);
        }
        if (ad_dec_pcm_data) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        aml_audio_free(dra_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;
}

static int dra_decoder_release(aml_dec_t * aml_dec)
{
    dec_data_info_t *dec_pcm_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;
    struct dra_dec_t *dra_dec = (struct dra_dec_t *)aml_dec;
    dra_decoder_operations_t *dra_op = &dra_dec->dra_op;
    dra_decoder_operations_t *ad_dra_op = &dra_dec->ad_dra_op;
    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        dra_op->release((void *)dra_op);

        ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;
        ALOGI("%s ad_dec_pcm_data->buf %p", __func__, ad_dec_pcm_data->buf);
        if (ad_dec_pcm_data->buf) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        ad_dra_op->release((void *)ad_dra_op);

        unload_dra_decoder_lib(dra_dec);
        aml_audio_free(aml_dec);
    }
    ALOGI("%s success", __func__);
    return 0;
}
static void dump_dra_data(void *buffer, int size, char *file_name)
{
    if (property_get_bool("vendor.audio.dra.outdump", false)) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGI("%s buffer %p size %d flen %d\n", __FUNCTION__, buffer, size, flen);
            fclose(fp1);
        }
    }
}


static int dra_decoder_process(aml_dec_t * aml_dec, unsigned char*buffer, int bytes)
{
    struct dra_dec_t *dra_dec = NULL;
    aml_dra_config_t *dra_config = NULL;
    AudioInfo pAudioInfo, pADAudioInfo;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return -1;
    }

    dra_dec = (struct dra_dec_t *)aml_dec;
    dra_config = &dra_dec->dra_config;
    dra_decoder_operations_t *dra_op = &dra_dec->dra_op;
    dra_decoder_operations_t *ad_dra_op = &dra_dec->ad_dra_op;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    int used_size = 0;
    int used_size_return = 0;
    int mark_remain_size;
    ALOGV("++%s, remain_size %d in_bytes %d ad_supported %d ad_mixing %d",
          __FUNCTION__, dra_dec->remain_size , bytes, dra_dec->ad_decoder_supported, dra_dec->ad_mixing_enable);
    if (bytes > 0) {
        if (dra_dec->remain_size + bytes > DRA_REMAIN_BUFFER_SIZE) {
            memcpy(dra_dec->remain_data + dra_dec->remain_size, buffer, DRA_REMAIN_BUFFER_SIZE - dra_dec->remain_size);
            used_size_return = DRA_REMAIN_BUFFER_SIZE -  dra_dec->remain_size;
            dra_dec->remain_size = DRA_REMAIN_BUFFER_SIZE;
        } else {
            memcpy(dra_dec->remain_data + dra_dec->remain_size, buffer, bytes);
            used_size_return = bytes;
            dra_dec->remain_size += bytes;
        }
    }
    mark_remain_size = dra_dec->remain_size;
    dec_pcm_data->data_len = 0;

    while (dra_dec->remain_size > used_size) {
        int pcm_len = DRA_MAX_LENGTH;
        int decode_len = dra_op->decode(dra_op, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len, (char *)dra_dec->remain_data + used_size, dra_dec->remain_size  - used_size);
        ALOGV("decode ret_len %d in_size %d pcm_len %d remain %d, used %d",
              decode_len, (dra_dec->remain_size  - used_size) , pcm_len, dra_dec->remain_size, used_size);
        if (decode_len > 0) {
            used_size += decode_len;
            dec_pcm_data->data_len += pcm_len;
            if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
                ALOGE("decode len %d  > buf_size %d ", dec_pcm_data->data_len, dec_pcm_data->buf_size);
                used_size_return = bytes;
                dra_dec->remain_size = 0;
                break;
            }
            ALOGV("decode_len %d in %d pcm_len %d used_size %d",
                  decode_len,  dra_dec->remain_size , pcm_len, used_size);

            if (dec_pcm_data->data_len) {
                dra_dec->remain_size = dra_dec->remain_size - used_size;
                if (used_size >= mark_remain_size) {
                    used_size_return = bytes;
                    dra_dec->remain_size = 0;
                } else {
                    dra_dec->remain_size = mark_remain_size - used_size;
                    memmove(dra_dec->remain_data, dra_dec->remain_data + used_size, dra_dec->remain_size);
                }
                break;
            } else if (used_size >= dra_dec->remain_size) {
                used_size_return = bytes;
                dra_dec->remain_size = 0;
                break;
            }
        } else {
            if (dra_dec->remain_size  > used_size) {
                dra_dec->remain_size = dra_dec->remain_size - used_size;
                if (dra_dec->remain_size > DRA_REMAIN_BUFFER_SIZE) {
                    ALOGE("dra_dec->remain_size %d > %d  ,overflow", dra_dec->remain_size , DRA_REMAIN_BUFFER_SIZE);
                    used_size_return = bytes;
                    dra_dec->remain_size = 0;
                } else {
                    memmove(dra_dec->remain_data, dra_dec->remain_data + used_size, dra_dec->remain_size);
                }
            } else {
                dra_dec->remain_size = 0;
                used_size_return = bytes;
            }
            ALOGV("decode_len==%d in %d pcm_len %d used_size %d dra_dec->remain_size %d",
                  decode_len,  bytes, pcm_len, used_size, dra_dec->remain_size);
            break;
        }
    }
    dra_op->getinfo(dra_op, &pAudioInfo);
    if (pAudioInfo.channels == 1 && dec_pcm_data->data_len) {
        int16_t *samples_data = (int16_t *)dec_pcm_data->buf;
        int i = 0, samples_num, samples;
        samples_num = dec_pcm_data->data_len / sizeof(int16_t);
        for (; i < samples_num; i++) {
            samples = samples_data[samples_num - i - 1] ;
            samples_data [ 2 * (samples_num - i - 1) ] = samples;
            samples_data [ 2 * (samples_num - i - 1) + 1] = samples;
        }
        dec_pcm_data->data_len  = dec_pcm_data->data_len * 2;
        pAudioInfo.channels = 2;
    }
    if (pAudioInfo.channels == 0) {
        pAudioInfo.channels = 2;
    }
    if (pAudioInfo.samplerate == 0) {
        pAudioInfo.samplerate = 48000;
    }
    if (dra_dec->ad_decoder_supported) {
        used_size = 0;
        if (aml_dec->ad_size > 0) {
            if ((aml_dec->ad_size + dra_dec->ad_remain_size) > DRA_REMAIN_BUFFER_SIZE) {
                ALOGE("dra_dec->ad_remain_size %d > %d  ,overflow", dra_dec->ad_remain_size , DRA_REMAIN_BUFFER_SIZE);
                dra_dec->ad_remain_size = 0;
                memset(dra_dec->ad_remain_data , 0 , DRA_REMAIN_BUFFER_SIZE);
            }
            memcpy(dra_dec->ad_remain_data + dra_dec->ad_remain_size, aml_dec->ad_data, aml_dec->ad_size);
            dra_dec->ad_remain_size += aml_dec->ad_size;
            aml_dec->ad_size = 0;
        }

        if (dra_dec->ad_need_cache_frames && dec_pcm_data->data_len) {
            dra_dec->ad_need_cache_frames--;
        }

        ad_dec_pcm_data->data_len = 0;
        ALOGV("dra_dec->ad_remain_size %d", dra_dec->ad_remain_size);
        while (dra_dec->ad_remain_size > used_size &&  !dra_dec->ad_need_cache_frames && dec_pcm_data->data_len) {
            int pcm_len = DRA_MAX_LENGTH;
            int decode_len = ad_dra_op->decode(ad_dra_op, (char *)(ad_dec_pcm_data->buf + ad_dec_pcm_data->data_len), &pcm_len, (char *)dra_dec->ad_remain_data + used_size, dra_dec->ad_remain_size - used_size);
            ALOGV("ad decode_len %d in %d pcm_len %d used_size %d", decode_len,  dra_dec->ad_remain_size, pcm_len, used_size);
            if (decode_len > 0) {
                used_size += decode_len;
                ad_dec_pcm_data->data_len += pcm_len;
                if (ad_dec_pcm_data->data_len > ad_dec_pcm_data->buf_size) {
                    ALOGV("decode len %d ", ad_dec_pcm_data->data_len);
                }
                if (ad_dec_pcm_data->data_len) {
                    memmove(dra_dec->ad_remain_data, dra_dec->ad_remain_data + used_size, dra_dec->ad_remain_size);
                    dra_dec->ad_remain_size = dra_dec->ad_remain_size - used_size;
                    break;
                }
            } else {
                if (dra_dec->ad_remain_size > used_size) {
                    if (used_size) {
                        dra_dec->ad_remain_size = dra_dec->ad_remain_size - used_size;
                        if (dra_dec->ad_remain_size > DRA_REMAIN_BUFFER_SIZE) {
                            ALOGE("dra_dec->ad_remain_size %d > %d  ,overflow", dra_dec->ad_remain_size , DRA_REMAIN_BUFFER_SIZE);
                            dra_dec->ad_remain_size = 0;
                            dra_dec->ad_need_cache_frames = DRA_AD_NEED_CACHE_FRAME_COUNT;
                        } else {
                            memmove(dra_dec->ad_remain_data, dra_dec->ad_remain_data + used_size, dra_dec->ad_remain_size);
                        }
                    }
                }
                ALOGV("ad dra_dec->ad_remain_size %d ad_dec_pcm_data->data_len %d used_size %d", dra_dec->ad_remain_size, ad_dec_pcm_data->data_len, used_size);
                break;
            }
        }
        ad_dra_op->getinfo(ad_dra_op, &pADAudioInfo);
        dump_dra_data(ad_dec_pcm_data->buf, ad_dec_pcm_data->data_len, "/data/dra_ad.pcm");
        //ALOGV("pADAudioInfo.channels %d pAudioInfo.channels %d", pADAudioInfo.channels, pAudioInfo.channels);
        if (pADAudioInfo.channels == 1 && pAudioInfo.channels == 2) {
            int16_t *samples_data = (int16_t *)ad_dec_pcm_data->buf;
            int i = 0, samples_num, samples;
            samples_num = ad_dec_pcm_data->data_len / sizeof(int16_t);
            for (; i < samples_num; i++) {
                samples = samples_data[samples_num - i - 1] ;
                samples_data [ 2 * (samples_num - i - 1) ] = samples;
                samples_data [ 2 * (samples_num - i - 1) + 1] = samples;
            }
            ad_dec_pcm_data->data_len  = ad_dec_pcm_data->data_len * 2;
        }
        if (dra_dec->ad_mixing_enable) {
            int frames_written = 0;

            float mixing_coefficient = 1.0f - (float)(dra_dec->mixer_level  + 32) / 64;
            float ad_mixing_coefficient = (dra_dec->advol_level * 1.0f / 100) * (float)(dra_dec->mixer_level  + 32) / 64;
            ALOGV("mixing_coefficient %f ad_mixing_coefficient %f", mixing_coefficient, ad_mixing_coefficient);
            apply_volume(mixing_coefficient, dec_pcm_data->buf, sizeof(uint16_t), dec_pcm_data->data_len);
            apply_volume(ad_mixing_coefficient, ad_dec_pcm_data->buf, sizeof(uint16_t), ad_dec_pcm_data->data_len);

            frames_written = do_mixing_2ch(dec_pcm_data->buf, ad_dec_pcm_data->buf ,
                                           dec_pcm_data->data_len / 4 , AUDIO_FORMAT_PCM_16_BIT, AUDIO_FORMAT_PCM_16_BIT);
            ALOGV("frames_written %d dec_pcm_data->data_len %d", frames_written, dec_pcm_data->data_len);
            dec_pcm_data->data_len = frames_written * 4;
        }
    }
    dec_pcm_data->data_sr  = pAudioInfo.samplerate;
    dec_pcm_data->data_ch  = pAudioInfo.channels;
    dec_pcm_data->data_format  = dra_config->dra_format;
    if (dec_pcm_data->data_len != ad_dec_pcm_data->data_len) {
        ALOGV("dec_pcm_data->data_len %d ad_dec_pcm_data->data_len %d", dec_pcm_data->data_len , ad_dec_pcm_data->data_len);
    }
    ad_dec_pcm_data->data_len  = 0;
    dump_dra_data(dec_pcm_data->buf, dec_pcm_data->data_len, "/data/dra_output.pcm");

    ALOGV("--%s, decoder_pcm_len %d buffer len %d used_size_return %d",
          __FUNCTION__, dec_pcm_data->data_len, dec_pcm_data->buf_size, used_size_return);
    return used_size_return;
}

static int dra_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{

    int ret = -1;
    struct dra_dec_t *dra_dec = (struct dra_dec_t *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        //dec_info->remain_size = dra_dec->remain_size;
        return 0;
    default:
        break;
    }
    return ret;

}

int dra_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    struct dra_dec_t *dra_dec = (struct dra_dec_t *)aml_dec;

    if (dra_dec == NULL) {
        return ret;
    }
    switch (config_type) {
    case AML_DEC_CONFIG_MIXER_LEVEL: {
        dra_dec->mixer_level = dec_config->mixer_level;
        ALOGI("dec_config->mixer_level %d", dec_config->mixer_level);
        break;
    }
    case AML_DEC_CONFIG_MIXING_ENABLE: {
        dra_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
        ALOGI("dec_config->ad_mixing_enable %d", dec_config->ad_mixing_enable);
        break;
    }
    case AML_DEC_CONFIG_AD_VOL: {
        dra_dec->advol_level = dec_config->advol_level;
        ALOGI("dec_config->advol_level %d", dec_config->advol_level);
        break;
    }

    default:
        break;
    }

    return ret;
}

aml_dec_func_t aml_dra_func = {
    .f_init                 = dra_decoder_init,
    .f_release              = dra_decoder_release,
    .f_process              = dra_decoder_process,
    .f_config               = dra_decoder_config,
    .f_info                 = dra_decoder_getinfo,
};
