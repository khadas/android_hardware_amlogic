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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <cutils/log.h>
#include <tinyalsa/asoundlib.h>
#include <cutils/properties.h>

#include "audio_hw.h"
#include "audio_eq_drc_compensation.h"
#include "aml_volume_utils.h"

#undef  LOG_TAG
#define LOG_TAG  "audio_hw_primary"

#if ANDROID_PLATFORM_SDK_VERSION < 29
#define MODEL_SUM_DEFAULT_PATH "/odm/etc/tvconfig/model/model_sum.ini"
#elif ANDROID_PLATFORM_SDK_VERSION > 29
#define MODEL_SUM_DEFAULT_PATH "/mnt/vendor/odm_ext/etc/tvconfig/model/model_sum.ini"
#endif


static struct audio_file_config_s dev_cfg[2] = {
    {/*amlogic inner EQ & DRC*/
        "AMLOGIC_SOC_INI_PATH",
        "",
    },
    {/*ext amp EQ & DRC*/
        "EXT_AMP_INI_PATH",
        "",
    }
};

uint32_t swapInt32(uint32_t value)
{
    return ((value & 0x000000FF) << 24) |
           ((value & 0x0000FF00) << 8) |
           ((value & 0x00FF0000) >> 8) |
           ((value & 0xFF000000) >> 24) ;
}

int16_t swapInt16(int16_t value)
{
    return ((value & 0x00FF) << 8) |
           ((value & 0xFF00) >> 8) ;
}

static int get_model_name(char *model_name, int size)
{
    int ret = -1;
    char node[PROPERTY_VALUE_MAX];

    ret = property_get("vendor.tv.model_name", node, "");
    if (ret <= 0) {
        snprintf(model_name, size, "FHD");
        ALOGD("%s: Can't get model name! use default model_name (%s)",
            __FUNCTION__, model_name);
    } else {
        snprintf(model_name, size, "%s", node);
        ALOGD("%s: Model Name (%s)", __FUNCTION__, model_name);
    }

    return ret;
}

static int eq_drc_ctl_value_set(int card, int val, char *name)
{
    int ret = -1, i, num_ctl_values;
    struct mixer_ctl *ctl;
    struct mixer *mixer;

    mixer = mixer_open(card);
    if (mixer == NULL) {
        ALOGE("%s: mixer is closed", __FUNCTION__);
        return -1;
    }

    ctl = mixer_get_ctl_by_name(mixer, name);
    if (ctl == NULL) {
        ALOGE("%s: get mixer ctl failed: %s", __FUNCTION__, name);
        goto ERROR;
    }

    num_ctl_values = mixer_ctl_get_num_values(ctl);
    for (i = 0; i < num_ctl_values; i++) {
        if (mixer_ctl_set_value(ctl, i, val)) {
            ALOGE("%s: set value = %d failed", __FUNCTION__, val);
            goto ERROR;
        }
    }

    ret = 0;
ERROR:
    mixer_close(mixer);
    return ret;
}

static int eq_status_set(struct audio_eq_drc_info_s *p_attr, int card)
{
    int ret;

    ret = eq_drc_ctl_value_set(card, p_attr->eq.enable, p_attr->eq.eq_enable_name);
    if (ret < 0) {
        ALOGE("%s: set EQ status failed", __FUNCTION__);
    }

    return ret;
}


static int drc_status_set(struct audio_eq_drc_info_s *p_attr, int card)
{
    int ret;

    ret = eq_drc_ctl_value_set(card, p_attr->fdrc.enable, p_attr->fdrc.fdrc_enable_name);
    if (ret < 0) {
        ALOGE("%s: set DRC status failed", __FUNCTION__);
    };

    ret = eq_drc_ctl_value_set(card, p_attr->mdrc.enable, p_attr->mdrc.mdrc_enable_name);
    if (ret < 0) {
        ALOGE("%s: set MDRC status failed", __FUNCTION__);
    };

    return ret;
}

static int volume_set(struct audio_eq_drc_info_s *p_attr, int card)
{
    int ret = 0;

    if (!p_attr->volume.enable)
        goto exit;

    ret = eq_drc_ctl_value_set(card, p_attr->volume.master, p_attr->volume.master_name);
    if (ret < 0) {
        ALOGE("%s: set Master volume failed", __FUNCTION__);
    }

    ret = eq_drc_ctl_value_set(card, p_attr->volume.ch1, p_attr->volume.ch1_name);
    if (ret < 0) {
        ALOGE("%s: set CH1 volume failed", __FUNCTION__);
    }

    ret = eq_drc_ctl_value_set(card, p_attr->volume.ch2, p_attr->volume.ch2_name);
    if (ret < 0) {
        ALOGE("%s: set CH2 volume failed", __FUNCTION__);
    }

    ret = eq_drc_ctl_value_set(card, p_attr->volume.LL_vol, p_attr->volume.LL_vol_name);
    if (ret < 0) {
        ALOGE("%s: set LL volume failed", __FUNCTION__);
    }

    ret = eq_drc_ctl_value_set(card, p_attr->volume.RR_vol, p_attr->volume.RR_vol_name);
    if (ret < 0) {
        ALOGE("%s: set RR volume failed", __FUNCTION__);
    }

exit:
    return ret;
}

static int aml_table_set(struct audio_data_s *table, int card, char *name)
{
    struct mixer_ctl *ctl;
    struct mixer *mixer;
    char param_buf[1024];
    int byte_size = 0;
    unsigned int *ptr;

    mixer = mixer_open(card);
    if (mixer == NULL) {
        ALOGE("%s: mixer is closed", __FUNCTION__);
        return -1;
    }

    ctl = mixer_get_ctl_by_name(mixer, name);
    if (ctl == NULL) {
        ALOGE("%s: get mixer ctl failed", __FUNCTION__);
        goto ERROR;
    }

    for (int i = 0; i < table->reg_cnt; i++) {
        ptr = table->regs[i].data;
        memset(param_buf, 0, 1024);
        byte_size = sprintf(param_buf, "0x%x ", *ptr++);
        for (int j = 1; j < table->regs[i].len; j++) {
            byte_size += sprintf(param_buf + byte_size, "0x%8.8x ", *ptr++);
        }
        int ret = mixer_ctl_set_array(ctl, param_buf, byte_size);
        if (ret < 0)
            ALOGE("[%s:%d] failed to set array, error: %d\n",
                    __FUNCTION__, __LINE__, ret);
    }

ERROR:
    mixer_close(mixer);
    return 0;
}

static int drc_set(struct eq_drc_data *pdata)
{
    char *name = pdata->aml_attr->fdrc.fdrc_table_name;
    struct audio_data_s *table = pdata->aml_attr->fdrc.fdrc_table;

    /*fullband drc setting*/
    if (pdata->aml_attr->fdrc.enable)
        aml_table_set(table, pdata->card, name);

    if (pdata->aml_attr->mdrc.enable) {
        name = pdata->aml_attr->mdrc.mdrc_table_name;
        table = pdata->aml_attr->mdrc.mdrc_table;

        /*mulitband drc setting*/
        aml_table_set(table, pdata->card, name);

        name = pdata->aml_attr->mdrc.crossover_table_name;
        table = pdata->aml_attr->mdrc.crossover_table;

        /*mulitband drc crossover setting*/
        aml_table_set(table, pdata->card, name);
    }
    return 0;
}

static int eq_mode_set(struct eq_drc_data *pdata, int eq_mode)
{
    struct audio_data_s *table = pdata->aml_attr->eq.eq_table[eq_mode];
    char *name = pdata->aml_attr->eq.eq_table_name;

    if (eq_mode >= pdata->aml_attr->eq.eq_table_num) {
        ALOGE("%s: eq mode is invalid", __FUNCTION__);
        return -1;
    }

    if (pdata->aml_attr->eq.enable)
        aml_table_set(table, pdata->card, name);

    return 0;
}

static int ext_table_set(struct audio_data_s *table, int card, char *name)
{
    struct mixer_ctl *ctl;
    struct mixer *mixer;
    unsigned int tlv_header_size = 0, tlv_size = 0;
    char *param_buf, *param_ptr;
    unsigned int *ptr;

    if (table == NULL) {
        ALOGE("%s: didn't get the table!", __FUNCTION__);
        return -1;
    }

    mixer = mixer_open(card);
    if (mixer == NULL) {
        ALOGE("%s: mixer is closed", __FUNCTION__);
        return -1;
    }

    ctl = mixer_get_ctl_by_name(mixer, name);
    if (ctl == NULL) {
        ALOGE("%s: get mixer ctl failed", __FUNCTION__);
        goto ERROR;
    }

    if (mixer_ctl_is_access_tlv_rw(ctl)) {
        tlv_header_size = TLV_HEADER_SIZE;
    }

    param_buf = (char *)calloc(1, (MAX_STRING_TABLE_MAX * sizeof(char)));
    if (param_buf == NULL) {
        goto ERROR;
    }

    param_ptr = param_buf + tlv_header_size;

    for (int i = 0; i < table->reg_cnt; i++) {
        for (int j = 0; j < table->regs[i].len; j++, param_ptr++) {
            *param_ptr = (char)table->regs[i].data[j];
        }
        tlv_size += table->regs[i].len;
    }

    ptr = (unsigned int *)param_buf;
    if (mixer_ctl_is_access_tlv_rw(ctl)) {
        ptr[0] = 0;
        ptr[1] = tlv_size;
    }

    ALOGD("%s: param_count = %d, name = %s, tlv_header_size = %d",
            __FUNCTION__, tlv_size, name, tlv_header_size);

    int ret = mixer_ctl_set_array(ctl, param_buf, (tlv_size + tlv_header_size));
    if (ret < 0)
        ALOGE("[%s:%d] failed to set array, error: %d\n",
                __FUNCTION__, __LINE__, ret);

    free(param_buf);
ERROR:
    mixer_close(mixer);
    return 0;
}

int ext_eq_mode_set(struct eq_drc_data *pdata, int eq_mode, int amp_num)
{
    struct audio_data_s *table = pdata->ext_attr[amp_num]->eq.eq_table[eq_mode];
    char *name = pdata->ext_attr[amp_num]->eq.eq_table_name;

    if (eq_mode >= pdata->ext_attr[amp_num]->eq.eq_table_num) {
        ALOGE("%s: eq mode is invalid", __FUNCTION__);
        return -1;
    }

    if (pdata->ext_attr[amp_num]->eq.enable)
        ext_table_set(table, pdata->card, name);

    return 0;
}

static int ext_drc_set(struct eq_drc_data *pdata, int amp_num)
{
    struct audio_data_s *table =  pdata->ext_attr[amp_num]->fdrc.fdrc_table;
    char *name = pdata->ext_attr[amp_num]->fdrc.fdrc_table_name;

    if (pdata->ext_attr[amp_num]->fdrc.enable)
        ext_table_set(table, pdata->card, name);

    return 0;
}

int eq_drc_init(struct eq_drc_data *pdata)
{
    int i, ret;
    char model_name[50] = {0};
    const char *filename = MODEL_SUM_DEFAULT_PATH;

    pdata->s_gain.atv = 1.0;
    pdata->s_gain.dtv = 1.0;
    pdata->s_gain.hdmi= 1.0;
    pdata->s_gain.av = 1.0;
    pdata->s_gain.media = 1.0;
    pdata->p_gain.speaker= 1.0;
    pdata->p_gain.spdif_arc = 1.0;
    pdata->p_gain.headphone = 1.0;

    ret = get_model_name(model_name, sizeof(model_name));
    if (ret < 0) {
        return -1;
    }

    /*parse amlogic ini file*/
    ret = parse_audio_sum(filename, model_name, &dev_cfg[0]);
    if (ret == 0) {
        ret = parse_audio_gain(dev_cfg[0].ini_file, pdata);
        if (ret < 0 ) {
            ALOGE("%s: Get amlogic gain config failed!", __FUNCTION__);
        } else {
            if (pdata->s_gain.enable) {
                pdata->s_gain.atv = DbToAmpl(pdata->s_gain.atv);
                pdata->s_gain.dtv = DbToAmpl(pdata->s_gain.dtv);
                pdata->s_gain.hdmi = DbToAmpl(pdata->s_gain.hdmi);
                pdata->s_gain.av = DbToAmpl(pdata->s_gain.av);
                pdata->s_gain.media = DbToAmpl(pdata->s_gain.media);
            }
            if (pdata->p_gain.enable) {
                pdata->p_gain.speaker = DbToAmpl(pdata->p_gain.speaker);
                pdata->p_gain.spdif_arc = DbToAmpl(pdata->p_gain.spdif_arc);
                pdata->p_gain.headphone = DbToAmpl(pdata->p_gain.headphone);
            }
        }

        pdata->aml_attr = (struct audio_eq_drc_info_s *)calloc(1, sizeof(struct audio_eq_drc_info_s));
        if (!pdata->aml_attr) {
            ALOGE("%s: calloc amlogic audio_eq_drc_info_s failed", __FUNCTION__);
            return -1;
        }

        parse_audio_eq_drc_status(dev_cfg[0].ini_file, pdata->aml_attr);
        volume_set(pdata->aml_attr, pdata->card);
        eq_status_set(pdata->aml_attr, pdata->card);
        drc_status_set(pdata->aml_attr, pdata->card);
        parse_audio_eq_drc_table(dev_cfg[0].ini_file, pdata->aml_attr);
        eq_mode_set(pdata, 0);
        drc_set(pdata);
    }

    ret = parse_audio_sum(filename, model_name, &dev_cfg[1]);
    if (ret == 0) {
        parse_AMP_num(dev_cfg[1].ini_file, pdata);
        for (int i = 0; i < pdata->ext_amp_num; i++) {
            pdata->ext_attr[i] = (struct audio_eq_drc_info_s *)calloc(1, sizeof(struct audio_eq_drc_info_s));
            if (!pdata->ext_attr[i]) {
                ALOGE("%s: calloc amlogic audio_eq_drc_info_s failed", __FUNCTION__);
                return -1;
            }
            pdata->ext_attr[i]->id = i;
            parse_audio_eq_drc_status(dev_cfg[1].ini_file, pdata->ext_attr[i]);
            volume_set(pdata->ext_attr[i], pdata->card);
            eq_status_set(pdata->ext_attr[i], pdata->card);
            drc_status_set(pdata->ext_attr[i], pdata->card);
            parse_audio_eq_drc_table(dev_cfg[1].ini_file, pdata->ext_attr[i]);
            ext_eq_mode_set(pdata, 0, i);
            ext_drc_set(pdata, i);
        }
    }

    return 0;
}

int eq_drc_release(struct eq_drc_data *pdata)
{
    if (pdata->aml_attr) {
        free_eq_drc_table(pdata->aml_attr);
        free(pdata->aml_attr);
        pdata->aml_attr = NULL;
    }

    for (int i = 0; i < pdata->ext_amp_num; i++) {
        if (pdata->ext_attr[i]) {
            free_eq_drc_table(pdata->ext_attr[i]);
            free(pdata->ext_attr[i]);
            pdata->ext_attr[i]= NULL;
        }
    }

    return 0;
}

int set_AQ_parameters(struct audio_hw_device *dev, struct str_parms *parms)
{
    struct aml_audio_device *adev = (struct aml_audio_device *)dev;
    int ret = -1, val = 0;
    char value[64];

    ret = str_parms_get_str(parms, "SOURCE_GAIN", value, sizeof(value));
    if (ret >= 0) {
        float fAtvGainDb = 0, fDtvGainDb = 0, fHdmiGainDb = 0, fAvGainDb = 0, fMediaGainDb = 0;
        sscanf(value,"%f %f %f %f %f", &fAtvGainDb, &fDtvGainDb, &fHdmiGainDb, &fAvGainDb, &fMediaGainDb);
        ALOGI("%s() audio source gain: atv:%f, dtv:%f, hdmiin:%f, av:%f, media:%f", __func__,
        fAtvGainDb, fDtvGainDb, fHdmiGainDb, fAvGainDb, fMediaGainDb);
        adev->eq_data.s_gain.atv = DbToAmpl(fAtvGainDb);
        adev->eq_data.s_gain.dtv = DbToAmpl(fDtvGainDb);
        adev->eq_data.s_gain.hdmi = DbToAmpl(fHdmiGainDb);
        adev->eq_data.s_gain.av = DbToAmpl(fAvGainDb);
        adev->eq_data.s_gain.media = DbToAmpl(fMediaGainDb);
        goto exit;
    }

    ret = str_parms_get_str(parms, "POST_GAIN", value, sizeof(value));
    if (ret >= 0) {
        sscanf(value,"%f %f %f", &adev->eq_data.p_gain.speaker, &adev->eq_data.p_gain.spdif_arc,
                &adev->eq_data.p_gain.headphone);
        ALOGI("%s() audio device gain: speaker:%f, spdif_arc:%f, headphone:%f", __func__,
        adev->eq_data.p_gain.speaker, adev->eq_data.p_gain.spdif_arc,
        adev->eq_data.p_gain.headphone);
        goto exit;
    }

    ret = str_parms_get_int(parms, "EQ_MODE", &val);
    if (ret >= 0) {
        if (eq_mode_set(&adev->eq_data, val) < 0)
            ALOGE("%s: eq_mode_set failed", __FUNCTION__);
        goto exit;
    }

exit:
    return ret;
}

