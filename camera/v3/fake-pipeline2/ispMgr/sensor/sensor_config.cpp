/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "sensorConfig"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>

#include "sensor_config.h"

#include "imx290/imx290_api.h"
#include "imx415/imx415_api.h"
#include "ov13b10/ov13b10_api.h"

#define ARRAY_SIZE(array)   (sizeof(array) / sizeof((array)[0]))

struct sensorConfig imx290Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx290,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx290,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx290,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx290,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx290,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx290,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx290,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx290,
    .sensorWidth      = 1920,
    .sensorHeight     = 1080,
    .sensorName       = "imx290",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .type             = sensor_raw,
};
struct sensorConfig imx415Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx415,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx415,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx415,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx415,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx415,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx415,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx415,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx415,
    .sensorWidth      = 3840,
    .sensorHeight     = 2160,
    .sensorName       = "imx415",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .type             = sensor_raw,
};

struct sensorConfig ov13b10Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_ov13b10,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_ov13b10,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_ov13b10,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_ov13b10,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_ov13b10,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_ov13b10,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_ov13b10,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_ov13b10,
    .sensorWidth      = 4208,
    .sensorHeight     = 3120,
    .sensorName       = "ov13b10",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .type             = sensor_raw,
};

struct sensorConfig ov5640Cfg = {
    .sensorWidth      = 1920,
    .sensorHeight     = 1080,
    .sensorName       = "ov5640",
    .wdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .sdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .type             = sensor_yuv,
};

struct sensorConfig *supportedCfgs[] = {
    &imx290Cfg,
    &imx415Cfg,
    &ov13b10Cfg,
    &ov5640Cfg,
};

struct sensorConfig *matchSensorConfig(media_stream_t *stream) {
    for (int i = 0; i < ARRAY_SIZE(supportedCfgs); i++) {
        if (strstr(stream->sensor_ent_name, supportedCfgs[i]->sensorName)) {
            return supportedCfgs[i];
        }
    }
    ALOGE("fail to match sensorConfig");
    return nullptr;
}

struct sensorConfig *matchSensorConfig(const char* sensorEntityName) {
    for (int i = 0; i < ARRAY_SIZE(supportedCfgs); i++) {
        if (strstr(sensorEntityName, supportedCfgs[i]->sensorName)) {
            return supportedCfgs[i];
        }
    }
    ALOGE("fail to match sensorConfig %s", sensorEntityName);
    return nullptr;
}

void cmos_sensor_control_cb(struct sensorConfig *cfg, ALG_SENSOR_EXP_FUNC_S *stSnsExp)
{
    stSnsExp->pfn_cmos_alg_update = cfg->expFunc.pfn_cmos_alg_update;
    stSnsExp->pfn_cmos_get_alg_default = cfg->expFunc.pfn_cmos_get_alg_default;
    stSnsExp->pfn_cmos_again_calc_table = cfg->expFunc.pfn_cmos_again_calc_table;
    stSnsExp->pfn_cmos_dgain_calc_table = cfg->expFunc.pfn_cmos_dgain_calc_table;
    stSnsExp->pfn_cmos_inttime_calc_table = cfg->expFunc.pfn_cmos_inttime_calc_table;
    stSnsExp->pfn_cmos_fps_set = cfg->expFunc.pfn_cmos_fps_set;
}

void cmos_set_sensor_entity(struct sensorConfig *cfg, struct media_entity *sensor_ent, int wdr)
{
    (cfg->cmos_set_sensor_entity)(sensor_ent, wdr);
}

void cmos_get_sensor_calibration(struct sensorConfig *cfg, aisp_calib_info_t *calib)
{
    (cfg->cmos_get_sensor_calibration)(calib);
}

