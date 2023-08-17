/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef __DW9800W_API_H__
#define __DW9800W_API_H__

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/mediaApi.h"

void vcm_set_ent_dw9800w(struct media_entity *ent);
void vcm_set_pos_dw9800w(uint32_t ctx, uint16_t position);
uint8_t vcm_is_moving_dw9800w(uint32_t ctx);
const LENS_PARAM_T *vcm_get_param_dw9800w(uint32_t ctx);

#endif
