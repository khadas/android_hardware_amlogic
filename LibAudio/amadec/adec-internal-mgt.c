/*
 * Copyright (C) 2018 Amlogic Corporation.
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
 *
 *  DESCRIPTION:
 *      brief  Audio Dec Message Loop Thread.
 *
 */

#define LOG_TAG "amadec"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>

#include <audio-dec.h>
#include <adec-pts-mgt.h>
#include <cutils/properties.h>
#include <dts_enc.h>
#include <Amsysfsutils.h>
#include <amthreadpool.h>
#include <amconfigutils.h>
#include <unistd.h>
#include "audiodsp_update_format.h"



#define MULTICH_SUPPORT_PROPERTY "media.multich.support.info"
#define PCM_88_96_SUPPORT        "media.libplayer.88_96K"

extern int RegisterDecode(aml_audio_dec_t *audec);
extern void get_output_func(struct aml_audio_dec* audec);

/*
static int set_tsync_enable(int enable)
{

    char *path = "/sys/class/tsync/enable";
    return amsysfs_set_sysfs_int(path, enable);
}
*/
typedef struct {
    //  int no;
    int audio_id;
    char type[16];
} audio_type_t;

audio_type_t audio_type[] = {
    {ACODEC_FMT_AAC, "aac"},
    {ACODEC_FMT_AC3, "ac3"},
    {ACODEC_FMT_DTS, "dts"},
    {ACODEC_FMT_FLAC, "flac"},
    {ACODEC_FMT_COOK, "cook"},
    {ACODEC_FMT_AMR, "amr"},
    {ACODEC_FMT_RAAC, "raac"},
    {ACODEC_FMT_WMA, "wma"},
    {ACODEC_FMT_WMAPRO, "wmapro"},
    {ACODEC_FMT_ALAC, "alac"},
    {ACODEC_FMT_VORBIS, "vorbis"},
    {ACODEC_FMT_AAC_LATM, "aac_latm"},
    {ACODEC_FMT_APE, "ape"},
    {ACODEC_FMT_MPEG, "mp3"},//mp3
    {ACODEC_FMT_MPEG2, "mp3"},//mp2
    {ACODEC_FMT_MPEG1, "mp3"},//mp1
    {ACODEC_FMT_EAC3, "eac3"},
    {ACODEC_FMT_TRUEHD, "thd"},
    {ACODEC_FMT_PCM_S16BE, "pcm"},
    {ACODEC_FMT_PCM_S16LE, "pcm"},
    {ACODEC_FMT_PCM_U8, "pcm"},
    {ACODEC_FMT_PCM_BLURAY, "pcm"},
    {ACODEC_FMT_WIFIDISPLAY, "pcm"},
    {ACODEC_FMT_ALAW, "pcm"},
    {ACODEC_FMT_MULAW, "pcm"},

    {ACODEC_FMT_ADPCM, "adpcm"},
    {ACODEC_FMT_WMAVOI, "wmavoi"},
    {ACODEC_FMT_DRA, "dra"},
    {ACODEC_FMT_NULL, "null"},

};

extern int match_types(const char *filetypestr, const char *typesetting);

/**
 * \brief start audio dec
 * \param audec pointer to audec
 * \return 0 on success otherwise -1 if an error occurred
 */
int match_types(const char *filetypestr, const char *typesetting)
{
    const char * psets = typesetting;
    const char *psetend;
    int psetlen = 0;
    char typestr[64] = "";
    if (filetypestr == NULL || typesetting == NULL) {
        return 0;
    }

    while (psets && psets[0] != '\0') {
        psetlen = 0;
        psetend = strchr(psets, ',');
        if (psetend != NULL && psetend > psets && psetend - psets < 64) {
            psetlen = psetend - psets;
            memcpy(typestr, psets, psetlen);
            typestr[psetlen] = '\0';
            psets = &psetend[1]; //skip ";"
        } else {
            strcpy(typestr, psets);
            psets = NULL;
        }
        if (strlen(typestr) > 0 && (strlen(typestr) == strlen(filetypestr))) {
            if (strstr(filetypestr, typestr) != NULL) {
                return 1;
            }
        }
    }
    return 0;
}


int vdec_pts_pause(void)
{
    return amsysfs_set_sysfs_str(TSYNC_EVENT, "VIDEO_PAUSE:0x1");
}

int  adec_thread_wait(aml_audio_dec_t *audec, int microseconds)
{
    struct timespec pthread_ts;
    struct timeval now;
    adec_thread_mgt_t *mgt = &audec->thread_mgt;
    int ret;

    gettimeofday(&now, NULL);
    pthread_ts.tv_sec = now.tv_sec + (microseconds + now.tv_usec) / 1000000;
    pthread_ts.tv_nsec = ((microseconds + now.tv_usec) * 1000) % 1000000000;
    pthread_mutex_lock(&mgt->pthread_mutex);
    ret = pthread_cond_timedwait(&mgt->pthread_cond, &mgt->pthread_mutex, &pthread_ts);
    pthread_mutex_unlock(&mgt->pthread_mutex);
    return ret;
}
int adec_thread_wakeup(aml_audio_dec_t *audec)
{
    adec_thread_mgt_t *mgt = &audec->thread_mgt;
    int ret;

    pthread_mutex_lock(&mgt->pthread_mutex);
    ret = pthread_cond_signal(&mgt->pthread_cond);
    pthread_mutex_unlock(&mgt->pthread_mutex);

    return ret;
}

int audiodec_init(aml_audio_dec_t *audec)
{
    int ret = 0;
    pthread_t    tid;
    adec_print("audiodec_init!");
    adec_message_pool_init(audec);
    //get_output_func(audec);
    audec->format_changed_flag = 0;
    audec->audio_decoder_enabled  = -1;//default set a invalid value
    audec->mix_lr_channel_enable  = -1;
    audec->pre_gain_enable  = -1;
    audec->pre_gain = 1.0;
    audec->pre_mute = 0;
    audec->VersionNum = -1;
    audec->refresh_pts_readytime_ms = 0;
    {
        RegisterDecode(audec);
        ret = amthreadpool_pthread_create(&tid, NULL, (void *)adec_armdec_loop, (void *)audec);
        pthread_mutex_init(&audec->thread_mgt.pthread_mutex, NULL);
        pthread_cond_init(&audec->thread_mgt.pthread_cond, NULL);
        audec->thread_mgt.pthread_id = tid;
        pthread_setname_np(tid, "AmadecArmdecLP");
    }
    if (ret != 0) {
        adec_print("Create adec main thread failed!\n");
        return ret;
    }
    adec_print("Create adec main thread success! tid = %ld \n", tid);
    audec->thread_pid = tid;
    return ret;
}
