/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef __SENSOR_CONFIG_H__
#define __SENSOR_CONFIG_H__

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

#include "aml_isp_api.h"
#include "aml_isp_tuning.h"

#include "media-v4l2/mediaApi.h"

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

enum sensorType
{
    sensor_raw,
    sensor_yuv,
    sensor_NULL,
};

struct sensorConfig {
    ALG_SENSOR_EXP_FUNC_S expFunc;
    void (*cmos_set_sensor_entity)(struct media_entity * sensor_ent, int wdr, int fps);
    void (*cmos_get_sensor_calibration)(struct media_entity *sensor_ent, aisp_calib_info_t *calib);
    void (*cmos_get_sensor_otp_data)(aisp_calib_info_t * otp);
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    void (*cmos_get_sensor_gdc_parameter)(struct sensorConfig *cfg, GDCInParam in_params, struct dewarp_params *dewarp_params);
#endif
    int sensorWidth;// max width
    int sensorHeight;// max height
    const char* sensorName;
    uint32_t wdrFormat;
    uint32_t sdrFormat;
    uint32_t sdrFormat60HZ;
    sensorType  type;
    const char* otpDevNum;
    const int   otpDevAddr;
    const int   otpDevAddrType;
    const int   otpDevLscAddr;
    const int   otpDevWbAddr;
};

LookupTable *GET_LOOKUP_PTR( aisp_calib_info_t *p_cali, uint32_t idx );
struct sensorConfig *matchSensorConfig(media_stream_t *stream);
struct sensorConfig *matchSensorConfig(const char* sensorEntityName);
void cmos_sensor_control_cb(struct sensorConfig *cfg, ALG_SENSOR_EXP_FUNC_S *stSnsExp);
void cmos_set_sensor_entity(struct sensorConfig *cfg, struct media_entity * sensor_ent, int wdr, int fps);
void cmos_get_sensor_otp_data(struct sensorConfig *cfg, aisp_calib_info_t *otp);
void cmos_get_sensor_calibration(struct sensorConfig *cfg, struct media_entity *sensor_ent, aisp_calib_info_t *calib);
void cmos_get_external_calibration( const char *sensorName, int32_t calibMode, aisp_calib_info_t *c );
#endif
