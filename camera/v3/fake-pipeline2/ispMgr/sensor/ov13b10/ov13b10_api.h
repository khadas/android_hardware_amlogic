/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef __OV13B10_API_H__
#define __OV13B10_API_H__

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

int cmos_get_ae_default_ov13b10(int ViPipe, ALG_SENSOR_DEFAULT_S *pstAeSnsDft);
void cmos_again_calc_table_ov13b10(int ViPipe, uint32_t *pu32AgainLin, uint32_t *pu32AgainDb);
void cmos_dgain_calc_table_ov13b10(int ViPipe, uint32_t *pu32DgainLin, uint32_t *pu32DgainDb);
void cmos_inttime_calc_table_ov13b10(int ViPipe, uint32_t pu32ExpL, uint32_t pu32ExpS, uint32_t pu32ExpVS, uint32_t pu32ExpVVS);
void cmos_fps_set_ov13b10(int ViPipe, float f32Fps, ALG_SENSOR_DEFAULT_S *pstAeSnsDft);
void cmos_alg_update_ov13b10(int ViPipe);
void cmos_set_sensor_entity_ov13b10(struct media_entity * sensor_ent);
void cmos_get_sensor_calibration_ov13b10(aisp_calib_info_t * calib);

#endif
