/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef __OV16A1Q_API_H__
#define __OV16A1Q_API_H__

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

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/mediaApi.h"

#include "aml_isp_api.h"
#include "aml_isp_tuning.h"

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

int cmos_get_ae_default_ov16a1q(int ViPipe, ALG_SENSOR_DEFAULT_S *pstAeSnsDft);
void cmos_again_calc_table_ov16a1q(int ViPipe, uint32_t *pu32AgainLin, uint32_t *pu32AgainDb);
void cmos_dgain_calc_table_ov16a1q(int ViPipe, uint32_t *pu32DgainLin, uint32_t *pu32DgainDb);
void cmos_inttime_calc_table_ov16a1q(int ViPipe, uint32_t pu32ExpL, uint32_t pu32ExpS, uint32_t pu32ExpVS, uint32_t pu32ExpVVS);
void cmos_fps_set_ov16a1q(int ViPipe, float f32Fps, ALG_SENSOR_DEFAULT_S *pstAeSnsDft);
void cmos_alg_update_ov16a1q(int ViPipe);
void cmos_set_sensor_entity_ov16a1q(struct media_entity * sensor_ent, int wdr, int fps);
void cmos_get_sensor_calibration_ov16a1q(struct media_entity *sensor_ent, aisp_calib_info_t *calib);
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
void cmos_get_sensor_gdc_parameter_ov16a1q(struct sensorConfig *cfg, GDCInParam in_params, struct dewarp_params *dewarp_params);
#endif

#endif
