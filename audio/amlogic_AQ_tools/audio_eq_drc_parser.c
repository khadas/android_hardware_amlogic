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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <utils/Log.h>

#include "audio_eq_drc_parser.h"
#include "iniparser.h"

#undef  LOG_TAG
#define LOG_TAG "audio_hw_primary"

#define ITEM_DEBUG

#ifdef ITEM_DEBUG
#define ITEM_LOGD(x...) ALOGD(x)
#define ITEM_LOGE(x...) ALOGE(x)
#else
#define ITEM_LOGD(x...)
#define ITEM_LOGE(x...)
#endif

static int error_callback(const char *format, ...)
{
    va_list argptr;
    va_start(argptr, format);
    ITEM_LOGE(format, argptr);
    va_end(argptr);
    return 0;
}

static int parse_audio_source_gain_data(dictionary *pIniParser, struct eq_drc_data *p_attr)
{
    p_attr->s_gain.enable = iniparser_getboolean(pIniParser, "source_gain:sg_enable", 0);
    if (!p_attr->s_gain.enable) {
        ITEM_LOGD("%s, Section -> [source_gain] is disable!\n", __FUNCTION__);
        return 0;
    }

    p_attr->s_gain.atv = iniparser_getdouble(pIniParser, "source_gain:atv", 0);
    ITEM_LOGD("%s, atv is (%f)\n", __FUNCTION__, p_attr->s_gain.atv);

    p_attr->s_gain.dtv = iniparser_getdouble(pIniParser, "source_gain:dtv", 0);
    ITEM_LOGD("%s, dtv is (%f)\n", __FUNCTION__, p_attr->s_gain.dtv);

    p_attr->s_gain.hdmi = iniparser_getdouble(pIniParser, "source_gain:hdmi", 0);
    ITEM_LOGD("%s, hdmi is (%f)\n", __FUNCTION__, p_attr->s_gain.hdmi);

    p_attr->s_gain.av = iniparser_getdouble(pIniParser, "source_gain:av", 0);
    ITEM_LOGD("%s, av is ((%f)\n", __FUNCTION__, p_attr->s_gain.av);

    p_attr->s_gain.media = iniparser_getdouble(pIniParser, "source_gain:media", 0);
    ITEM_LOGD("%s, av is ((%f)\n", __FUNCTION__, p_attr->s_gain.media);

    return 0;
}

static int parse_audio_post_gain_data(dictionary *pIniParser, struct eq_drc_data *p_attr)
{
    p_attr->p_gain.enable = iniparser_getboolean(pIniParser, "post_gain:pg_enable", 0);
    if (!p_attr->p_gain.enable) {
        ITEM_LOGD("%s, Section -> [post_gain] is disable!\n", __FUNCTION__);
        return 0;
    }

    p_attr->p_gain.speaker = iniparser_getdouble(pIniParser, "post_gain:speaker", 0);
    ITEM_LOGD("%s, speaker is (%f)\n", __FUNCTION__, p_attr->p_gain.speaker);

    p_attr->p_gain.spdif_arc = iniparser_getdouble(pIniParser, "post_gain:spdif_arc", 0);
    ITEM_LOGD("%s, spdif_arc is (%f)\n", __FUNCTION__, p_attr->p_gain.spdif_arc);

    p_attr->p_gain.headphone = iniparser_getdouble(pIniParser, "post_gain:headphone", 0);
    ITEM_LOGD("%s, headphone is (%f)\n", __FUNCTION__, p_attr->p_gain.headphone);

    return 0;
}

static int parse_audio_ng_data(dictionary *pIniParser, struct eq_drc_data *p_attr)
{

    p_attr->noise_gate.aml_ng_enable = iniparser_getboolean(pIniParser, "noise_gate:ng_enable", 0);
    if (!p_attr->noise_gate.aml_ng_enable) {
        ITEM_LOGD("%s, noise gate is disable!\n", __FUNCTION__);
        return 0;
    }

    p_attr->noise_gate.aml_ng_level = iniparser_getdouble(pIniParser, "noise_gate:ng_level", -75.0f);
    ITEM_LOGD("%s, noise gate level is (%f)\n", __FUNCTION__, p_attr->noise_gate.aml_ng_level);

    p_attr->noise_gate.aml_ng_attack_time = iniparser_getint(pIniParser, "noise_gate:ng_attack_time", 3000);
    ITEM_LOGD("%s, noise gate attack time (%d)\n", __FUNCTION__, p_attr->noise_gate.aml_ng_attack_time);

    p_attr->noise_gate.aml_ng_release_time = iniparser_getint(pIniParser, "noise_gate:ng_release_time", 50);
    ITEM_LOGD("%s, noise gate release time (%d)\n", __FUNCTION__, p_attr->noise_gate.aml_ng_release_time);

    return 0;
}

static int parse_audio_volume_status(dictionary *pIniParser, struct audio_eq_drc_info_s *p_attr)
{
    const char  *str;
    char buf_section_name[64];

    sprintf(buf_section_name, "volume_%d:", p_attr->id);
    strcpy(p_attr->volume.section_name, buf_section_name);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "vol_enable");
    //ITEM_LOGD("%s, section_name is (%s)\n", __FUNCTION__, buf_section_name);
    p_attr->volume.enable = iniparser_getboolean(pIniParser, buf_section_name, 1);
    if (!p_attr->volume.enable) {
        ITEM_LOGD("%s, HW volume is disable!\n", __FUNCTION__);
        return 0;
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "master");
    p_attr->volume.master = iniparser_getint(pIniParser, buf_section_name, 0);
    ITEM_LOGD("%s, Master Volume is (%d)\n", __FUNCTION__, p_attr->volume.master);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "master_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->volume.master_name, str);
        ITEM_LOGD("%s, Master Volume Name is (%s)\n", __FUNCTION__, p_attr->volume.master_name);
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "ch1");
    p_attr->volume.ch1 = iniparser_getint(pIniParser, buf_section_name, 0);
    ITEM_LOGD("%s, CH1 Volume is (%d)\n", __FUNCTION__, p_attr->volume.ch1);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "ch1_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->volume.ch1_name, str);
        ITEM_LOGD("%s, Master Ch1 Name is (%s)\n", __FUNCTION__, p_attr->volume.ch1_name);
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "ch2");
    p_attr->volume.ch2 = iniparser_getint(pIniParser, buf_section_name, 0);
    ITEM_LOGD("%s, CH2 Volume is  (%d)\n", __FUNCTION__, p_attr->volume.ch2);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "ch2_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->volume.ch2_name, str);
        ITEM_LOGD("%s, Master Ch2 Name is (%s)\n", __FUNCTION__, p_attr->volume.ch2_name);
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "LL_vol");
    p_attr->volume.LL_vol= iniparser_getint(pIniParser, buf_section_name, 0);
    ITEM_LOGD("%s, mixer LL Volume is  (%d)\n", __FUNCTION__, p_attr->volume.LL_vol);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "LL_vol_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->volume.LL_vol_name, str);
        ITEM_LOGD("%s, mixer LL Volume Name is (%s)\n", __FUNCTION__, p_attr->volume.LL_vol_name);
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "RR_vol");
    p_attr->volume.RR_vol= iniparser_getint(pIniParser, buf_section_name, 0);
    ITEM_LOGD("%s, mixer RR Volume is  (%d)\n", __FUNCTION__, p_attr->volume.RR_vol);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->volume.section_name, "RR_vol_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->volume.RR_vol_name, str);
        ITEM_LOGD("%s, mixer RR Volume Name is (%s)\n", __FUNCTION__, p_attr->volume.RR_vol_name);
    }
    return 0;
}

static int parse_audio_eq_status(dictionary *pIniParser, struct audio_eq_drc_info_s* p_attr)
{
    const char  *str;
    char buf_section_name[64];

    sprintf(buf_section_name, "eq_param_%d:", p_attr->id);
    strcpy(p_attr->eq.section_name, buf_section_name);

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_enable");
    p_attr->eq.enable = iniparser_getboolean(pIniParser, buf_section_name, 0);
    if (!p_attr->eq.enable) {
        ITEM_LOGD("%s, eq is disable!\n", __FUNCTION__);
    } else {
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->eq.eq_name, str);
            ITEM_LOGD("%s, EQ is from (%s)\n", __FUNCTION__, p_attr->eq.eq_name);
        }

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_byte_mode");
        p_attr->eq.eq_byte_mode = iniparser_getint(pIniParser, buf_section_name, 0);
        ITEM_LOGD("%s, EQ byte is (%d)\n", __FUNCTION__, p_attr->eq.eq_byte_mode);

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_table_num");
        p_attr->eq.eq_table_num = iniparser_getint(pIniParser, buf_section_name, 0);
        ITEM_LOGD("%s, EQ table num is (%d)\n", __FUNCTION__, p_attr->eq.eq_table_num);

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_table_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->eq.eq_table_name, str);
            ITEM_LOGD("%s, EQ table name is (%s)\n", __FUNCTION__, p_attr->eq.eq_table_name);
        }
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->eq.section_name, "eq_enable_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->eq.eq_enable_name, str);
        ITEM_LOGD("%s, EQ enable name is (%s)\n", __FUNCTION__, p_attr->eq.eq_enable_name);
    }

    return 0;
}

static int parse_audio_drc_status(dictionary *pIniParser, struct audio_eq_drc_info_s* p_attr)
{
    const char  *str;
    char buf_section_name[64];

    sprintf(buf_section_name, "drc_param_%d:", p_attr->id);
    strcpy(p_attr->fdrc.section_name, buf_section_name);
    strcpy(p_attr->mdrc.section_name, buf_section_name);

    /*parse fullband DRC*/
    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_enable");
    p_attr->fdrc.enable = iniparser_getboolean(pIniParser, buf_section_name, 0);
    if (!p_attr->fdrc.enable) {
        ITEM_LOGD("%s, fullband drc is disable!\n", __FUNCTION__);
    } else {
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->fdrc.fdrc_name, str);
            ITEM_LOGD("%s, DRC is from (%s)\n", __FUNCTION__, p_attr->fdrc.fdrc_name);
        }

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_byte_mode");
        p_attr->fdrc.drc_byte_mode = iniparser_getint(pIniParser, buf_section_name, 1);
        ITEM_LOGD("%s, DRC byte is (%d)\n", __FUNCTION__, p_attr->fdrc.drc_byte_mode);
        p_attr->mdrc.drc_byte_mode = p_attr->fdrc.drc_byte_mode;

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_table_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->fdrc.fdrc_table_name, str);
            ITEM_LOGD("%s, DRC table name is (%s)\n", __FUNCTION__, p_attr->fdrc.fdrc_table_name);
        }
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_enable_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->fdrc.fdrc_enable_name, str);
        ITEM_LOGD("%s, DRC enable name is (%s)\n", __FUNCTION__, p_attr->fdrc.fdrc_enable_name);
    }

    /*parse multiband DRC*/
    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "mdrc_enable");
    p_attr->mdrc.enable = iniparser_getboolean(pIniParser, buf_section_name, 0);
    if (!p_attr->mdrc.enable) {
        ITEM_LOGD("%s, multiband drc is disable!\n", __FUNCTION__);
    } else {
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "mdrc_table_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->mdrc.mdrc_table_name, str);
            ITEM_LOGD("%s, multiband DRC table name is (%s)\n", __FUNCTION__, p_attr->mdrc.mdrc_table_name);
        }

        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "crossover_table_name");
        str = iniparser_getstring(pIniParser, buf_section_name, NULL);
        if (str) {
            strcpy(p_attr->mdrc.crossover_table_name, str);
            ITEM_LOGD("%s, multiband DRC crossover name is (%s)\n", __FUNCTION__, p_attr->mdrc.crossover_table_name);
        }
    }

    memset(buf_section_name, 0, 64);
    sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "mdrc_enable_name");
    str = iniparser_getstring(pIniParser, buf_section_name, NULL);
    if (str) {
        strcpy(p_attr->mdrc.mdrc_enable_name, str);
        ITEM_LOGD("%s, multiband DRC enable name is (%s)\n", __FUNCTION__, p_attr->mdrc.mdrc_enable_name);
    }

    return 0;
}

static int transBufferData(const char *data_str, unsigned int *data_buf)
{
    int item_ind = 0;
    char *token;
    char *pSave;
    char *tmp_buf;

    if (data_str == NULL)
        return 0;

    tmp_buf = (char *)calloc(1, (MAX_STRING_TABLE_MAX * sizeof(char)));
    if (tmp_buf == NULL)
        return 0;

    strncpy(tmp_buf, data_str, MAX_STRING_TABLE_MAX - 1);
    token = strtok_r(tmp_buf, ",", &pSave);
    while (token != NULL) {
        data_buf[item_ind] = strtoul(token, NULL, 0);
        item_ind++;
        token = strtok_r(NULL, ",", &pSave);
    }

    free(tmp_buf);
    return item_ind;
}

static int parse_audio_table_data(dictionary *pIniParser, struct audio_data_s *table)
{
    int i = 0, j = 0, k = 0, data_cnt = 0;
    const char  *str = NULL;
    unsigned int *tmp_buf;

    str = iniparser_getstring(pIniParser, table->section_name, NULL);
    if (str == NULL)
        return -1;

    tmp_buf = (unsigned int *)calloc(1, (MAX_INT_TABLE_MAX * sizeof(unsigned int)));
    if (tmp_buf == NULL)
        return -1;

    data_cnt = transBufferData(str, tmp_buf);
    ITEM_LOGD("%s, reg buffer data cnt = %d\n", __FUNCTION__, data_cnt);

    while (i <= data_cnt) {
        if (j >= CC_AUDIO_REG_CNT_MAX) {
            break;
        }

        if (tmp_buf[i] == 0) {
            break;
        }

        table->regs[j].len = tmp_buf[i];

        for (k = 0; k < table->regs[j].len; k++) {
            table->regs[j].data[k] = tmp_buf[i + 1 + k];
        }

        i += table->regs[j].len + 1;
        j += 1;
    }

    table->reg_cnt = j;
    free(tmp_buf);
    return 0;
}

static void PrintRegData(int byte_mode, struct audio_data_s *table)
{
    int i = 0, j = 0, tmp_len = 0;
    char tmp_buf[1024] = {'\0'};

    ITEM_LOGD("%s, reg_cnt = %d\n", __FUNCTION__, table->reg_cnt);

    memset(tmp_buf, 0, 1024);

    for (i = 0; i < table->reg_cnt; i++) {
        tmp_len = strlen(tmp_buf);
        sprintf((char *)tmp_buf + tmp_len, "%d, ", table->regs[i].len);

        for (j = 0; j < table->regs[i].len; j++) {
            tmp_len = strlen(tmp_buf);
            if (byte_mode == 4) {
                sprintf((char *)tmp_buf + tmp_len, "0x%08X, ", table->regs[i].data[j]);
            } else if (byte_mode == 2){
                sprintf((char *)tmp_buf + tmp_len, "0x%04X, ", table->regs[i].data[j]);
            } else if (byte_mode == 1){
                sprintf((char *)tmp_buf + tmp_len, "0x%02X, ", table->regs[i].data[j]);
            }
        }

        ITEM_LOGD("%s", tmp_buf);
        memset(tmp_buf, 0, 1024);
    }

    ITEM_LOGD("\n");
}

static int parse_audio_eq_data(dictionary *pIniParser, struct audio_eq_drc_info_s* p_attr)
{
    int eq_tabel_num = p_attr->eq.eq_table_num;
    int i = 0;
    struct audio_data_s *table;
    char buf_section_name[64];

    for (i = 0; i < eq_tabel_num; i++) {
        table = (struct audio_data_s *)calloc(1, sizeof(struct audio_data_s));
        if (!table) {
            ITEM_LOGE("%s: calloc audio_data_s failed!", __FUNCTION__);
        } else {
            p_attr->eq.eq_table[i] = table;
            memset(buf_section_name, 0, 64);
            sprintf(buf_section_name, "%s%s%d", p_attr->eq.section_name, "eq_table_", i);
            strcpy(table->section_name, buf_section_name);
            ITEM_LOGD("%s, section_name = %s\n", __FUNCTION__, table->section_name);
            parse_audio_table_data(pIniParser, table);
            PrintRegData(p_attr->eq.eq_byte_mode, table);
        }
    }

    return 0;
}

static int parse_audio_fdrc_data(dictionary *pIniParser, struct audio_eq_drc_info_s* p_attr)
{
    struct audio_data_s *table;
    char buf_section_name[64];

    table = (struct audio_data_s *)calloc(1, sizeof(struct audio_data_s));
    if (!table) {
        ITEM_LOGE("%s: calloc audio_data_s failed!", __FUNCTION__);
    } else {
        p_attr->fdrc.fdrc_table = table;
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->fdrc.section_name, "drc_table");
        strcpy(table->section_name, buf_section_name);
        ITEM_LOGD("%s, section_name = %s\n", __FUNCTION__, table->section_name);
        parse_audio_table_data(pIniParser, table);
        PrintRegData(p_attr->fdrc.drc_byte_mode, table);
    }

    return 0;
}

static int parse_audio_mdrc_data(dictionary *pIniParser, struct audio_eq_drc_info_s* p_attr)
{
    struct audio_data_s *table;
    char buf_section_name[64];

    table = (struct audio_data_s *)calloc(1, sizeof(struct audio_data_s));
    if (!table) {
        ITEM_LOGE("%s: calloc audio_data_s failed!", __FUNCTION__);
    } else {
        p_attr->mdrc.mdrc_table = table;
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "mdrc_table");
        strcpy(table->section_name, buf_section_name);
        ITEM_LOGD("%s, section_name = %s\n", __FUNCTION__, table->section_name);
        parse_audio_table_data(pIniParser, table);
        PrintRegData(p_attr->mdrc.drc_byte_mode, table);
    }

    table = (struct audio_data_s *)calloc(1, sizeof(struct audio_data_s));
    if (!table) {
        ITEM_LOGE("%s: calloc audio_data_s failed!", __FUNCTION__);
    } else {
        p_attr->mdrc.crossover_table = table;
        memset(buf_section_name, 0, 64);
        sprintf(buf_section_name, "%s%s", p_attr->mdrc.section_name, "crossover_table");
        strcpy(table->section_name, buf_section_name);
        ITEM_LOGD("%s, section_name = %s\n", __FUNCTION__, table->section_name);
        parse_audio_table_data(pIniParser, table);
        PrintRegData(p_attr->mdrc.drc_byte_mode, table);
    }

    return 0;
}

int parse_audio_sum(const char *file_name, char *model_name, struct audio_file_config_s *dev_cfg)
{
    dictionary *ini = NULL;
    const char *ini_value = NULL;
    char buf[128];

    iniparser_set_error_callback(error_callback);

    ini = iniparser_load(file_name);
    if (ini == NULL) {
        ITEM_LOGE("%s, INI load file (%s) error!\n", __FUNCTION__, file_name);
        goto exit;
    }

    sprintf(buf, "%s:%s", model_name, dev_cfg->ini_header);
    ini_value = iniparser_getstring(ini, buf, NULL);

    if (ini_value == NULL || access(ini_value, F_OK) == -1) {
        ITEM_LOGD("%s, INI File is not exist!\n", __FUNCTION__);
        goto exit;
    }

    memset(dev_cfg->ini_file, 0, sizeof(dev_cfg->ini_file));
    strcpy(dev_cfg->ini_file, ini_value);
    ITEM_LOGD("%s, INI File -> (%s)\n", __FUNCTION__, dev_cfg->ini_file);

    iniparser_freedict(ini);
    return 0;

exit:
    iniparser_freedict(ini);
    return -1;
}

int parse_audio_gain(char *file_name, struct eq_drc_data *p_attr)
{
    dictionary *ini = NULL;

    ini = iniparser_load(file_name);
    if (ini == NULL) {
        ITEM_LOGE("%s, INI load file (%s) error!\n", __FUNCTION__, file_name);
        goto exit;
    }

    parse_audio_source_gain_data(ini, p_attr);
    parse_audio_post_gain_data(ini, p_attr);
    parse_audio_ng_data(ini, p_attr);

exit:
    iniparser_freedict(ini);

    return 0;
}

int parse_audio_eq_drc_status(char *file_name, struct audio_eq_drc_info_s *p_attr)
{
    dictionary *ini = NULL;
    const char *ini_value = NULL;

    ini = iniparser_load(file_name);
    if (ini == NULL) {
        ITEM_LOGE("%s, INI load file (%s) error!\n", __FUNCTION__, file_name);
        goto exit;
    }

    parse_audio_volume_status(ini, p_attr);
    parse_audio_eq_status(ini, p_attr);
    parse_audio_drc_status(ini, p_attr);
exit:
    iniparser_freedict(ini);
    return 0;
}


int parse_audio_eq_drc_table(char *file_name, struct audio_eq_drc_info_s *p_attr)
{
    dictionary *ini = NULL;
    const char *ini_value = NULL;

    ini = iniparser_load(file_name);
    if (ini == NULL) {
        ITEM_LOGE("%s, INI load file (%s) error!\n", __FUNCTION__, file_name);
        goto exit;
    }

    if (p_attr->eq.enable) {
        parse_audio_eq_data(ini, p_attr);
    }
    if (p_attr->fdrc.enable) {
        parse_audio_fdrc_data(ini, p_attr);
    }
    if (p_attr->mdrc.enable) {
        parse_audio_mdrc_data(ini, p_attr);
    }

exit:
    iniparser_freedict(ini);
    return 0;
}

int parse_AMP_num(char *file_name, struct eq_drc_data *pdata)
{
    dictionary *ini = NULL;

    ini = iniparser_load(file_name);
    if (ini == NULL) {
        ITEM_LOGE("%s, INI load file (%s) error!\n", __FUNCTION__, file_name);
        goto exit;
    }

    pdata->ext_amp_num = iniparser_getint(ini, "AMP_info:AMP_num", 1);
    ITEM_LOGD("%s, external AMP num (%d)\n", __FUNCTION__, pdata->ext_amp_num);

exit:
    iniparser_freedict(ini);

    return 0;
}

void free_eq_drc_table(struct audio_eq_drc_info_s *p_attr)
{
    int i = 0;

    for (i = 0; i < p_attr->eq.eq_table_num; i++) {
        if (p_attr->eq.eq_table[i])
            free(p_attr->eq.eq_table[i]);
    }
    if (p_attr->fdrc.fdrc_table)
        free(p_attr->fdrc.fdrc_table);
    if (p_attr->mdrc.mdrc_table)
        free(p_attr->mdrc.mdrc_table);
    if (p_attr->mdrc.crossover_table)
        free(p_attr->mdrc.crossover_table);
}

