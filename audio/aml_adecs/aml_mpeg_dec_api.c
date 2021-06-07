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

#define LOG_TAG "aml_audio_mad_dec"
//#define LOG_NDEBUG 0

#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>
#include "aml_dec_api.h"
#include "audio_data_process.h"
#include "aml_malloc_debug.h"

#define MAD_LIB_PATH "/vendor/lib/libmad.so"

#define MAD_MAX_LENGTH (1024 * 64)
#define MAD_REMAIN_BUFFER_SIZE (4096 * 10)
#define MAD_AD_NEED_CACHE_FRAME_COUNT  2


typedef struct _audio_info {
    int bitrate;
    int samplerate;
    int channels;
    int file_profile;
    int error_num;
} AudioInfo;

typedef struct mad_decoder_operations {
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
    int extradata_size;      ///< extra data size
    char extradata[4096];
    int NchOriginal;
    int lfepresent;
}mad_decoder_operations_t;

struct mad_dec_t {
    aml_dec_t  aml_dec;
    aml_mad_config_t mad_config;
    mad_decoder_operations_t mad_op;
    mad_decoder_operations_t ad_mad_op;
    void *pdecoder;
    char remain_data[MAD_REMAIN_BUFFER_SIZE];
    int remain_size;
    bool ad_decoder_supported;
    bool ad_mixing_enable;
    int advol_level;
    int  mixer_level;
    char ad_remain_data[MAD_REMAIN_BUFFER_SIZE];
    int ad_remain_size;
    int ad_need_cache_frames;
};

static  int unload_mad_decoder_lib(struct mad_dec_t *mad_dec)
{
    mad_decoder_operations_t *mad_op = &mad_dec->mad_op;
    mad_decoder_operations_t *ad_mad_op = &mad_dec->ad_mad_op;
    if (mad_op != NULL ) {
        mad_op->init = NULL;
        mad_op->decode = NULL;
        mad_op->release = NULL;
        mad_op->getinfo = NULL;

    }

    if (ad_mad_op != NULL ) {
        ad_mad_op->init = NULL;
        ad_mad_op->decode = NULL;
        ad_mad_op->release = NULL;
        ad_mad_op->getinfo = NULL;
    }
    if (mad_dec->pdecoder) {
        dlclose(mad_dec->pdecoder);
        mad_dec->pdecoder = NULL;
    }

    return 0;
}
static  int load_mad_decoder_lib(struct mad_dec_t *mad_dec)
{
    mad_decoder_operations_t *mad_op = &mad_dec->mad_op;
    mad_decoder_operations_t *ad_mad_op = &mad_dec->ad_mad_op;
    mad_dec->pdecoder = dlopen(MAD_LIB_PATH, RTLD_NOW);
    if (!mad_dec->pdecoder) {
        ALOGE("%s, failed to open (libfaad.so), %s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[faad_op->pdecoder]", __FUNCTION__, __LINE__);
    }

    mad_op->init = ad_mad_op->init = (int (*) (void *)) dlsym(mad_dec->pdecoder, "audio_dec_init");
    if (mad_op->init == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d> audio_dec_init", __FUNCTION__, __LINE__);
    }

    mad_op->decode = ad_mad_op->decode = (int (*)(void *, char *outbuf, int *outlen, char *inbuf, int inlen))
                          dlsym(mad_dec->pdecoder, "audio_dec_decode");
    if (mad_op->decode  == NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1 ;
    } else {
        ALOGV("<%s::%d>--[audio_dec_decode:]", __FUNCTION__, __LINE__);
    }

    mad_op->release = ad_mad_op->release = (int (*)(void *)) dlsym(mad_dec->pdecoder, "audio_dec_release");
    if ( mad_op->release== NULL) {
        ALOGE("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_release:]", __FUNCTION__, __LINE__);
    }

    mad_op->getinfo = ad_mad_op->getinfo = (int (*)(void *, AudioInfo *pAudioInfo)) dlsym(mad_dec->pdecoder, "audio_dec_getinfo");
    if (mad_op->getinfo == NULL) {
        ALOGI("%s,cant find decoder lib,%s\n", __FUNCTION__, dlerror());
        return -1;
    } else {
        ALOGV("<%s::%d>--[audio_dec_getinfo:]", __FUNCTION__, __LINE__);
    }

    return 0;
}

static int mad_decoder_init(aml_dec_t **ppaml_dec, aml_dec_config_t * dec_config)
{
    struct mad_dec_t *mad_dec;
    aml_dec_t  *aml_dec = NULL;
    aml_mad_config_t *mad_config = NULL;
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t * ad_dec_pcm_data = NULL;

    if (dec_config == NULL) {
        ALOGE("AAC config is NULL\n");
        return -1;
    }
    mad_config = &dec_config->mad_config;

    /*if (aac_config->channel <= 0 || aac_config->channel > 8) {
        ALOGE("AAC config channel is invalid=%d\n", aac_config->channel);
        return -1;
    }

    if (aac_config->samplerate <= 0 ||aac_config->samplerate > 192000) {
        ALOGE("AAC config samplerate is invalid=%d\n", aac_config->samplerate);
        return -1;
    }*/

    mad_dec = aml_audio_calloc(1, sizeof(struct mad_dec_t));
    if (mad_dec == NULL) {
        ALOGE("malloc mad_dec failed\n");
        return -1;
    }

    aml_dec = &mad_dec->aml_dec;

    memcpy(&mad_dec->mad_config, mad_config, sizeof(aml_mad_config_t));
    ALOGI("AAC format=%d samplerate =%d ch=%d\n", mad_config->mpeg_format,
          mad_config->samplerate, mad_config->channel);

    dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_pcm_data->buf_size = MAD_MAX_LENGTH;
    dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, dec_pcm_data->buf_size);
    if (!dec_pcm_data->buf) {
        ALOGE("malloc buffer failed\n");
        goto exit;
    }
    ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    ad_dec_pcm_data->buf_size = MAD_MAX_LENGTH;
    ad_dec_pcm_data->buf = (unsigned char*) aml_audio_calloc(1, ad_dec_pcm_data->buf_size);
    if (!ad_dec_pcm_data->buf) {
        ALOGE("malloc ad buffer failed\n");
        goto exit;
    }
    if (load_mad_decoder_lib(mad_dec) == 0) {

       int ret = mad_dec->mad_op.init((void *)&mad_dec->mad_op);
       if (ret != 0) {
           ALOGI("faad decoder init failed !");
           goto exit;
       }
       ret = mad_dec->ad_mad_op.init((void *)&mad_dec->ad_mad_op);
       if (ret != 0) {
           ALOGI("mad ad decoder init failed !");
           goto exit;
       }
    } else {
         goto exit;
    }
    aml_dec->status = 1;
    *ppaml_dec = (aml_dec_t *)mad_dec;
    mad_dec->ad_decoder_supported = dec_config->ad_decoder_supported;
    mad_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
    mad_dec->mixer_level = dec_config->mixer_level;
    mad_dec->advol_level = dec_config->advol_level;
    mad_dec->ad_need_cache_frames = MAD_AD_NEED_CACHE_FRAME_COUNT;

    ALOGI("mad_dec->ad_decoder_supported %d",mad_dec->ad_decoder_supported);
    mad_dec->remain_size = 0;
    memset(mad_dec->remain_data , 0 , MAD_REMAIN_BUFFER_SIZE * sizeof(char ));
    mad_dec->ad_remain_size = 0;
    memset(mad_dec->ad_remain_data , 0 ,MAD_REMAIN_BUFFER_SIZE * sizeof(char ));
    ALOGE("%s success", __func__);
    return 0;

exit:
    if (mad_dec) {
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        if (ad_dec_pcm_data) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        aml_audio_free(mad_dec);
    }
    *ppaml_dec = NULL;
    ALOGE("%s failed", __func__);
    return -1;
}

static int mad_decoder_release(aml_dec_t * aml_dec)
{
    dec_data_info_t * dec_pcm_data = NULL;
    dec_data_info_t *ad_dec_pcm_data = NULL;
    struct mad_dec_t *mad_dec = (struct mad_dec_t *)aml_dec;
    mad_decoder_operations_t *mad_op = &mad_dec->mad_op;
    mad_decoder_operations_t *ad_mad_op = &mad_dec->ad_mad_op;
    if (aml_dec != NULL) {
        dec_pcm_data = &aml_dec->dec_pcm_data;
        if (dec_pcm_data->buf) {
            aml_audio_free(dec_pcm_data->buf);
        }
        mad_op->release((void *)mad_op);
        ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;
        if (ad_dec_pcm_data->buf) {
            aml_audio_free(ad_dec_pcm_data->buf);
        }
        ad_mad_op->release((void *)ad_mad_op);
        unload_mad_decoder_lib(mad_dec);
        aml_audio_free(aml_dec);
    }
    ALOGE("%s success", __func__);
    return 0;
}
static void dump_mad_data(void *buffer, int size, char *file_name)
{
   if (property_get_bool("vendor.media.mad.dump",false)) {
        FILE *fp1 = fopen(file_name, "a+");
        if (fp1) {
            int flen = fwrite((char *)buffer, 1, size, fp1);
            ALOGI("%s buffer %p size %d flen %d\n", __FUNCTION__, buffer, size,flen);
            fclose(fp1);
        }
    }
}


static int mad_decoder_process(aml_dec_t * aml_dec, unsigned char*buffer, int bytes)
{
    struct mad_dec_t *mad_dec = NULL;
    aml_mad_config_t *mad_config = NULL;
    AudioInfo pAudioInfo,pADAudioInfo;
    if (aml_dec == NULL) {
        ALOGE("%s aml_dec is NULL", __func__);
        return -1;
    }

    mad_dec = (struct mad_dec_t *)aml_dec;
    mad_config = &mad_dec->mad_config;
    mad_decoder_operations_t *mad_op = &mad_dec->mad_op;
    mad_decoder_operations_t *ad_mad_op = &mad_dec->ad_mad_op;
    dec_data_info_t * dec_pcm_data = &aml_dec->dec_pcm_data;
    dec_data_info_t * ad_dec_pcm_data = &aml_dec->ad_dec_pcm_data;

    int used_size = 0;
    int used_size_return = 0;
    int mark_remain_size = mad_dec->remain_size;
    ALOGV("remain_size %d bytes %d ad_decoder_supported %d ad_mixing_enable %d advol_level %d mixer_level %d",
        mad_dec->remain_size ,bytes, mad_dec->ad_decoder_supported, mad_dec->ad_mixing_enable, mad_dec->advol_level,mad_dec->mixer_level );
    if (bytes > 0) {
        memcpy(mad_dec->remain_data + mad_dec->remain_size, buffer, bytes);
        mad_dec->remain_size += bytes;
    }
    dec_pcm_data->data_len = 0;

    while (mad_dec->remain_size >  used_size) {
      int pcm_len = MAD_MAX_LENGTH;
      int decode_len = mad_op->decode(mad_op, (char *)(dec_pcm_data->buf + dec_pcm_data->data_len), &pcm_len, (char *)mad_dec->remain_data + used_size, mad_dec->remain_size  - used_size);
      ALOGV("decode_len %d in %d pcm_len %d used_size %d", decode_len,  mad_dec->remain_size , pcm_len, used_size);
      if (decode_len > 0) {
          used_size += decode_len;
          dec_pcm_data->data_len += pcm_len;
          if (dec_pcm_data->data_len > dec_pcm_data->buf_size) {
              ALOGE("decode len %d  > buf_size %d ", dec_pcm_data->data_len, dec_pcm_data->buf_size);
              break;
          }
          if (dec_pcm_data->data_len) {
              if (used_size >= mark_remain_size) {
                  used_size_return = used_size - mark_remain_size;
                  mad_dec->remain_size = 0;
              } else {
                   used_size_return = 0;
                   mad_dec->remain_size = mark_remain_size - used_size;
                   memmove(mad_dec->remain_data, mad_dec->remain_data + used_size, mad_dec->remain_size );
              }
              break;
          }

      } else {
          if (mad_dec->remain_size  > used_size) {
            mad_dec->remain_size = mad_dec->remain_size - used_size;
            if (mad_dec->remain_size > MAD_REMAIN_BUFFER_SIZE) {
                ALOGE("mad_dec->remain_size %d > %d  ,overflow", mad_dec->remain_size , MAD_REMAIN_BUFFER_SIZE );
                mad_dec->remain_size = 0;
            } else {
                memmove(mad_dec->remain_data, mad_dec->remain_data + used_size, mad_dec->remain_size );
            }

          }
          used_size_return = bytes;
          ALOGV("decode_len %d in %d pcm_len %d used_size %d mad_dec->remain_size %d", decode_len,  bytes, pcm_len, used_size, mad_dec->remain_size);
          break;
      }
    }
    mad_op->getinfo(mad_op,&pAudioInfo);

    if (pAudioInfo.channels == 1 && dec_pcm_data->data_len) {
            int16_t *samples_data = (int16_t *)dec_pcm_data->buf;
            int i = 0, samples_num,samples;
            samples_num = dec_pcm_data->data_len / sizeof(int16_t);
            for (; i < samples_num; i++) {
                samples = samples_data[samples_num - i -1] ;
                samples_data [ 2 * (samples_num - i -1) ] = samples;
                samples_data [ 2 * (samples_num - i -1) + 1]= samples;
            }
            dec_pcm_data->data_len  = dec_pcm_data->data_len * 2;
            pAudioInfo.channels = 2;
    }

    if (mad_dec->ad_decoder_supported ) {
        used_size = 0;
        if (aml_dec->ad_size > 0) {
            if ((aml_dec->ad_size + mad_dec->ad_remain_size) > MAD_REMAIN_BUFFER_SIZE) {
                 ALOGE("mad_dec->ad_remain_size %d > %d  ,overflow", mad_dec->ad_remain_size , MAD_REMAIN_BUFFER_SIZE );
                 mad_dec->ad_remain_size = 0;
                 memset(mad_dec->ad_remain_data , 0 , MAD_REMAIN_BUFFER_SIZE);
            }
            memcpy(mad_dec->ad_remain_data + mad_dec->ad_remain_size, aml_dec->ad_data, aml_dec->ad_size);
            mad_dec->ad_remain_size += aml_dec->ad_size;
            aml_dec->ad_size = 0;
        }

        if (mad_dec->ad_need_cache_frames && dec_pcm_data->data_len) {
            mad_dec->ad_need_cache_frames--;
        }

        ad_dec_pcm_data->data_len = 0;
        ALOGV("mad_dec->ad_remain_size %d", mad_dec->ad_remain_size);
        while (mad_dec->ad_remain_size > used_size && !mad_dec->ad_need_cache_frames && dec_pcm_data->data_len) {
            int pcm_len = MAD_MAX_LENGTH;
            int decode_len = ad_mad_op->decode(ad_mad_op, (char *)(ad_dec_pcm_data->buf + ad_dec_pcm_data->data_len), &pcm_len, (char *)mad_dec->ad_remain_data + used_size, mad_dec->ad_remain_size - used_size);
            ALOGV("ad decode_len %d in %d pcm_len %d used_size %d", decode_len,  mad_dec->ad_remain_size, pcm_len, used_size);
            if (decode_len > 0) {
                used_size += decode_len;
                ad_dec_pcm_data->data_len += pcm_len;
                if (ad_dec_pcm_data->data_len > ad_dec_pcm_data->buf_size) {
                    ALOGE("decode len %d  > buf_size %d ", dec_pcm_data->data_len, dec_pcm_data->buf_size);
                    break;
                }
                mad_dec->ad_remain_size = mad_dec->ad_remain_size - used_size;
                memmove(mad_dec->ad_remain_data, mad_dec->ad_remain_data + used_size, mad_dec->ad_remain_size );
                aml_dec->ad_size = 0;
                if(ad_dec_pcm_data->data_len) 
                    break;
            } else {
                if (mad_dec->ad_remain_size > used_size) {
                    if (used_size) {
                       mad_dec->ad_remain_size = mad_dec->ad_remain_size - used_size;
                       if (mad_dec->ad_remain_size > MAD_REMAIN_BUFFER_SIZE) {
                            ALOGE("mad_dec->ad_remain_size %d > %d  ,overflow", mad_dec->ad_remain_size , MAD_REMAIN_BUFFER_SIZE );
                            mad_dec->ad_remain_size = 0;
                            mad_dec->ad_need_cache_frames = MAD_AD_NEED_CACHE_FRAME_COUNT;
                       } else {
                           memmove(mad_dec->ad_remain_data, mad_dec->ad_remain_data + used_size, mad_dec->ad_remain_size);
                       }
                    }       
                }
                ALOGV("ad mad_dec->ad_remain_size %d ad_dec_pcm_data->data_len %d used_size %d", mad_dec->ad_remain_size, ad_dec_pcm_data->data_len, used_size);
                break;
            }
        }
        ad_mad_op->getinfo(ad_mad_op,&pADAudioInfo);
        dump_mad_data(ad_dec_pcm_data->buf, ad_dec_pcm_data->data_len, "/data/mad_ad.pcm");
        ALOGV("pADAudioInfo.channels %d pAudioInfo.channels %d",pADAudioInfo.channels, pAudioInfo.channels);
        if (pADAudioInfo.channels == 1 && pAudioInfo.channels == 2) {
            int16_t *samples_data = (int16_t *)ad_dec_pcm_data->buf;
            int i = 0, samples_num,samples;
            samples_num = ad_dec_pcm_data->data_len / sizeof(int16_t);
            for (; i < samples_num; i++) {
                samples = samples_data[samples_num - i -1] ;
                samples_data [ 2 * (samples_num - i -1) ] = samples;
                samples_data [ 2 * (samples_num - i -1) + 1]= samples;
            }
            ad_dec_pcm_data->data_len  = ad_dec_pcm_data->data_len * 2;
        }

        if (mad_dec->ad_mixing_enable) {
            struct audioCfg data_cfg;
            int frames_written = 0;
            data_cfg.channelCnt = pAudioInfo.channels;
            data_cfg.format = AUDIO_FORMAT_PCM_16_BIT;
            data_cfg.sampleRate = pAudioInfo.samplerate;

            float mixing_coefficient = 1.0f - (float)(mad_dec->mixer_level  + 32 ) / 64;
            float ad_mixing_coefficient = (mad_dec->advol_level * 1.0f / 100 ) * (float)(mad_dec->mixer_level  + 32 ) / 64;
            ALOGV("mixing_coefficient %f ad_mixing_coefficient %f",mixing_coefficient, ad_mixing_coefficient);
            apply_volume(mixing_coefficient, dec_pcm_data->buf, sizeof(uint16_t), dec_pcm_data->data_len);
            apply_volume(ad_mixing_coefficient, ad_dec_pcm_data->buf, sizeof(uint16_t), ad_dec_pcm_data->data_len);


            frames_written = do_mixing_2ch(dec_pcm_data->buf, ad_dec_pcm_data->buf ,
                dec_pcm_data->data_len / 4 , data_cfg, data_cfg);
            ALOGV("frames_written %d dec_pcm_data->data_len %d",frames_written, dec_pcm_data->data_len);
            dec_pcm_data->data_len = frames_written * 4;
        }

    }

    dec_pcm_data->data_sr  = pAudioInfo.samplerate;
    dec_pcm_data->data_ch  = pAudioInfo.channels;
    dec_pcm_data->data_format  = mad_config->mpeg_format;
    if (dec_pcm_data->data_len != ad_dec_pcm_data->data_len ) {
        ALOGV("dec_pcm_data->data_len %d ad_dec_pcm_data->data_len %d",dec_pcm_data->data_len ,ad_dec_pcm_data->data_len);
    }
    ad_dec_pcm_data->data_len  = 0;
    dump_mad_data(dec_pcm_data->buf, dec_pcm_data->data_len, "/data/mad_output.pcm");
    ALOGV("used_size_return %d decode len %d buffer len %d ", used_size_return, dec_pcm_data->data_len, dec_pcm_data->buf_size);
    return used_size_return;
}

static int mad_decoder_getinfo(aml_dec_t *aml_dec, aml_dec_info_type_t info_type, aml_dec_info_t * dec_info)
{

    int ret = -1;
    struct mad_dec_t *mad_dec= (struct mad_dec_t *)aml_dec;

    switch (info_type) {
    case AML_DEC_REMAIN_SIZE:
        //dec_info->remain_size = ddp_dec->remain_size;
        return 0;
    default:
        break;
    }
    return ret;
}

int mad_decoder_config(aml_dec_t * aml_dec, aml_dec_config_type_t config_type, aml_dec_config_t * dec_config)
{
    int ret = -1;
    struct mad_dec_t *mad_dec = (struct mad_dec_t *)aml_dec;

    if (mad_dec == NULL ) {
        return ret;
    }
    switch (config_type) {
    case AML_DEC_CONFIG_MIXER_LEVEL: {
        mad_dec->mixer_level = dec_config->mixer_level;
        ALOGI("dec_config->mixer_level %d",dec_config->mixer_level);
        break;
    }
    case AML_DEC_CONFIG_MIXING_ENABLE: {
        mad_dec->ad_mixing_enable = dec_config->ad_mixing_enable;
        ALOGI("dec_config->ad_mixing_enable %d",dec_config->ad_mixing_enable);
        break;
    }
    case AML_DEC_CONFIG_AD_VOL: {
        mad_dec->advol_level = dec_config->advol_level;
        ALOGI("dec_config->advol_level %d",dec_config->advol_level);
        break;
    }

    default:
        break;
    }

    return ret;
}
aml_dec_func_t aml_mad_func = {
    .f_init                 = mad_decoder_init,
    .f_release              = mad_decoder_release,
    .f_process              = mad_decoder_process,
    .f_config               = mad_decoder_config,
    .f_info                 = mad_decoder_getinfo,
};
