/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "lensConfig"

#include "CamHalDebugLog.h"

#include "lens_config.h"

#include "dw9800w/dw9800w_api.h"

#define ARRAY_SIZE(array)   (sizeof(array) / sizeof((array)[0]))

static struct lensConfig dw9800wCfg = {
	.lensName = "dw9800w",
	.lensFunc.pfn_lens_move = vcm_set_pos_dw9800w,
	.lensFunc.pfn_lens_is_moving = vcm_is_moving_dw9800w,
	.lensFunc.pfn_lens_get_parameters = vcm_get_param_dw9800w,
	.lens_set_entity = vcm_set_ent_dw9800w,
};

static struct lensConfig *lensCfgs[] = {
	&dw9800wCfg,
};

struct lensConfig *matchLensConfig(media_stream_t *stream) {
	for (int i = 0; i < ARRAY_SIZE(lensCfgs); i++) {
		if (strstr(stream->lens_ent_name, lensCfgs[i]->lensName)) {
			return lensCfgs[i];
		}
	}

	CAMHAL_LOGE("LKK: fail to match lensConfig\n");

	return nullptr;
}

struct lensConfig *matchLensConfig(const char* lensEntityName) {
	for (int i = 0; i < ARRAY_SIZE(lensCfgs); i++) {
		if (strstr(lensEntityName, lensCfgs[i]->lensName)) {
			return lensCfgs[i];
		}
	}

	CAMHAL_LOGE("LKK: fail to match sensorConfig %s", lensEntityName);

	return nullptr;
}

void lens_control_cb(struct lensConfig *cfg, ALG_LENS_FUNC_S *stLens)
{
	stLens->pfn_lens_move = cfg->lensFunc.pfn_lens_move;
	stLens->pfn_lens_is_moving = cfg->lensFunc.pfn_lens_is_moving;
	stLens->pfn_lens_get_parameters = cfg->lensFunc.pfn_lens_get_parameters;
}

void lens_set_entity(struct lensConfig *cfg, struct media_entity *lens_ent)
{
	(cfg->lens_set_entity)(lens_ent);
}
