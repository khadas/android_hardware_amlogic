/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#ifndef __LENS_CONFIG_H__
#define __LENS_CONFIG_H__

#include "aml_isp_api.h"
#include "media-v4l2/mediaApi.h"

struct lensConfig {
	const char *lensName;
	ALG_LENS_FUNC_S lensFunc;
	void (*lens_set_entity)(struct media_entity *ent);
};

struct lensConfig *matchLensConfig(media_stream_t *stream);
struct lensConfig *matchLensConfig(const char *lensEntityName);
void lens_control_cb(struct lensConfig *cfg, ALG_LENS_FUNC_S *stLens);
void lens_set_entity(struct lensConfig *cfg, struct media_entity *lens_ent);

#endif
