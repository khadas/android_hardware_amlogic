/*
 * Media controller test application
 *
 * Copyright (C) 2010-2014 Ideas on board SPRL
 *
 * Contact: Laurent Pinchart <laurent.pinchart@ideasonboard.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __MEDIAPI_H__
#define __MEDIAPI_H__

#include <utils/Log.h>
#include <android/log.h>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"

#include "aml_isp_api.h"

#define V4L2_META_AML_ISP_CONFIG    v4l2_fourcc('A', 'C', 'F', 'G') /* Aml isp config */
#define V4L2_META_AML_ISP_STATS     v4l2_fourcc('A', 'S', 'T', 'S') /* Aml isp statistics */

typedef struct pipe_info {
    const char *media_dev_name ;
    const char *sensor_ent_name ;
    const char *csiphy_ent_name;
    const char *adap_ent_name;
    const char *isp_ent_name;
    const char *video_ent_name;
    const char *video_stats_name;
    const char *video_param_name;
    const bool  ispDev;
} pipe_info_t;

typedef struct stream_configuration{
    struct aml_format format;
} stream_configuration_t;

typedef struct media_stream {
    char media_dev_name[64];
    char sensor_ent_name[32];
    char csiphy_ent_name[32];
    char adap_ent_name[32];
    char isp_ent_name[32];
    char video_ent_name[32];
    char video_stats_name[32];
    char video_param_name[32];

    struct media_device  *media_dev;

    struct media_entity  *sensor_ent;
    struct media_entity  *csiphy_ent;
    struct media_entity  *adap_ent;
    struct media_entity  *isp_ent;
    struct media_entity  *video_ent;
    struct media_entity  *video_stats;
    struct media_entity  *video_param;
} media_stream_t;

bool entInPipe(struct pipe_info * pipe, char * ent_name, int name_len);

void mediaLog(const char *fmt, ...);

struct pipe_info *mediaFindMatchedPipe(
       std::vector<struct pipe_info *> &supportedPipes, struct media_device *media_dev);

int mediaStreamInit(media_stream_t * stream, struct pipe_info *pipe_info_ptr, struct media_device * dev);

int createLinks(media_stream_t *stream);

int setSdFormat(media_stream_t *stream, stream_configuration_t *cfg);

int setImgFormat(media_stream_t *stream, stream_configuration_t *cfg);


int setDataFormat(media_stream_t *camera, stream_configuration_t *cfg);


int setConfigFormat(media_stream_t *camera, stream_configuration_t *cfg);


int mediaStreamConfig(media_stream_t * stream, stream_configuration_t *cfg);

#endif /* __MEDIAPI_H__ */

