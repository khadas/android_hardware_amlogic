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

struct sensorConfig {
    ALG_SENSOR_EXP_FUNC_S expFunc;
    void (*cmos_set_sensor_entity)(struct media_entity * sensor_ent);
    void (*cmos_get_sensor_calibration)(aisp_calib_info_t * calib);
    int sensorWidth;// max width
    int sensorHeight;// max height
    const char* sensorEntityName;
};

struct sensorConfig *matchSensorConfig(media_stream_t *stream);
void cmos_sensor_control_cb(struct sensorConfig *cfg, ALG_SENSOR_EXP_FUNC_S *stSnsExp);
void cmos_set_sensor_entity(struct sensorConfig *cfg, struct media_entity * sensor_ent);
void cmos_get_sensor_calibration(struct sensorConfig *cfg, aisp_calib_info_t * calib);

#endif
