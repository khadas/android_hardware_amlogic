/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "dw9800wCfg"

#include "CamHalDebugLog.h"
#include "dw9800w_api.h"

#define MIN_STEP 16

struct dw9800w_info {
	struct media_entity  *ent;
	LENS_PARAM_T param;
};

static struct dw9800w_info vcm_info;

static struct dw9800w_info *dw9800_get_info(void)
{
	return &vcm_info;
}

void vcm_set_ent_dw9800w(struct media_entity *ent)
{
	struct dw9800w_info *info = dw9800_get_info();
	LENS_PARAM_T *param = &info->param;

	info->ent = ent;

	param->min_step = MIN_STEP;
}

void vcm_set_pos_dw9800w(uint32_t ctx, uint16_t position)
{
	struct v4l2_ext_control pos;
	struct dw9800w_info *info = dw9800_get_info();

	memset(&pos, 0, sizeof(pos));
	pos.id = V4L2_CID_FOCUS_ABSOLUTE;
	pos.value = position;

	v4l2_subdev_set_ctrls(info->ent, &pos, 1);
}

uint8_t vcm_is_moving_dw9800w(uint32_t ctx)
{
	return 0;
}

const LENS_PARAM_T *vcm_get_param_dw9800w(uint32_t ctx)
{
	struct dw9800w_info *info = dw9800_get_info();

	return &info->param;
}
