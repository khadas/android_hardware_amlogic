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

#define LOG_TAG "mediaApi"

#include <utils/Log.h>
#include <android/log.h>
#include <stdio.h>
#include <string.h>

#include "mediaApi.h"

bool entInPipe(struct pipe_info * pipe, char * ent_name, int name_len)
{
    if (strncmp(ent_name, pipe->sensor_ent_name,name_len) == 0) {
        return true;
    }
#if 0
    else if (strncmp(ent_name, pipe->csiphy_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->adap_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->video_ent_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->video_stats_name,name_len) == 0) {
        return true;
    } else if (strncmp(ent_name, pipe->video_param_name,name_len) == 0) {
        return true;
    }
#endif
    return false;
}

void mediaLog(const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);

    ALOGI("%s ", buf);
}

struct pipe_info *mediaFindMatchedPipe(
       std::vector<struct pipe_info *> &supportedPipes, struct media_device *media_dev)
{
    media_debug_set_handler(media_dev, mediaLog, NULL);

    if (0 != media_device_enumerate(media_dev) ) {
        ALOGE("media_device_enumerate fail");
        return NULL;
    }

    int node_num = media_get_entities_count(media_dev);

    for (int pipe_idx = 0 ; pipe_idx < supportedPipes.size(); ++pipe_idx) {
        bool all_ent_matched = false;
        for (int ii = 0; ii < node_num; ++ii) {
            struct media_entity * ent = media_get_entity(media_dev, ii);
            ALOGI("ent %d, name %s , devnode %s ", ii, ent->info.name, ent->devname);
            if (true == entInPipe(supportedPipes[pipe_idx], ent->info.name, strlen(ent->info.name))) {
                all_ent_matched = true;
                break;
            }
        }
        if (all_ent_matched) {
            return supportedPipes[pipe_idx];
        }
    }
    return NULL;
}

int mediaStreamInit(media_stream_t * stream, struct pipe_info *pipe_info_ptr, struct media_device * dev)
{
    memset(stream, 0, sizeof(*stream));

    strncpy(stream->media_dev_name, pipe_info_ptr->media_dev_name, sizeof(stream->media_dev_name));

    strncpy(stream->sensor_ent_name, pipe_info_ptr->sensor_ent_name, sizeof(stream->sensor_ent_name));
    strncpy(stream->csiphy_ent_name, pipe_info_ptr->csiphy_ent_name, sizeof(stream->csiphy_ent_name));
    strncpy(stream->adap_ent_name,   pipe_info_ptr->adap_ent_name,   sizeof(stream->adap_ent_name));
    strncpy(stream->isp_ent_name,   pipe_info_ptr->isp_ent_name,   sizeof(stream->isp_ent_name));
    strncpy(stream->video_ent_name,  pipe_info_ptr->video_ent_name,  sizeof(stream->video_ent_name));
    strncpy(stream->video_stats_name,   pipe_info_ptr->video_stats_name,   sizeof(stream->video_stats_name));
    strncpy(stream->video_param_name,  pipe_info_ptr->video_param_name,  sizeof(stream->video_param_name));

    stream->media_dev = dev;

    if (NULL == stream->media_dev) {
        ALOGE("new media dev fail");
        return -1;
    }

    media_debug_set_handler(dev, mediaLog, NULL);

    if (0 != media_device_enumerate(stream->media_dev) ) {
        ALOGE("media_device_enumerate fail");
        return -1;
    }

    int node_num = media_get_entities_count(stream->media_dev);
    for (int ii = 0; ii < node_num; ++ii) {
        struct media_entity * ent = media_get_entity(stream->media_dev, ii);
        ALOGI("ent %d, name %s ", ii, ent->info.name);
    }

    stream->sensor_ent = media_get_entity_by_name(stream->media_dev, stream->sensor_ent_name, strlen(stream->sensor_ent_name));
    if (NULL == stream->sensor_ent) {
        ALOGE("get  sensor_ent fail");
        return -1;
    }

    stream->csiphy_ent = media_get_entity_by_name(stream->media_dev, stream->csiphy_ent_name, strlen(stream->csiphy_ent_name));
    if (NULL == stream->csiphy_ent) {
        ALOGE("get  csiphy_ent fail");
        return -1;
    }

    stream->adap_ent = media_get_entity_by_name(stream->media_dev, stream->adap_ent_name, strlen(stream->adap_ent_name));
    if (NULL == stream->adap_ent) {
        ALOGE("get adap_ent fail");
        return -1;
    }

    stream->isp_ent = media_get_entity_by_name(stream->media_dev, stream->isp_ent_name, strlen(stream->isp_ent_name));
    if (NULL == stream->isp_ent) {
        ALOGE("get isp_ent fail");
        return -1;
    }

    stream->video_ent = media_get_entity_by_name(stream->media_dev, stream->video_ent_name, strlen(stream->video_ent_name));
    if (NULL == stream->video_ent) {
        ALOGE("get video_ent fail");
        return -1;
    }

    stream->video_stats = media_get_entity_by_name(stream->media_dev, stream->video_stats_name, strlen(stream->video_stats_name));
    if (NULL == stream->video_stats) {
        ALOGE("get video_stats fail");
        return -1;
    }

    stream->video_param = media_get_entity_by_name(stream->media_dev, stream->video_param_name, strlen(stream->video_param_name));
    if (NULL == stream->video_param) {
        ALOGE("get video_param fail");
        return -1;
    }

    int ret = v4l2_video_open(stream->video_ent);
    ALOGD("%s open video fd, ret %d ", __FUNCTION__, ret);

    ALOGD("media stream init success");
    return 0;
}

int createLinks(media_stream_t *stream)
{
    // 0 = sink; 1 = src
    const int sink_pad_idx = 0;
    const int src_pad_idx = 1;

    int rtn = -1;
    struct media_pad      *src_pad;
    struct media_pad      *sink_pad;

    int flag = MEDIA_LNK_FL_ENABLED;

    ALOGD("create link ++");

    if (stream->media_dev == NULL) {
        return 0;
    }

    /*source:adap_ent sink:isp_ent*/
    sink_pad = (struct media_pad*) media_entity_get_pad(stream->isp_ent, sink_pad_idx);
    if (!sink_pad) {
        ALOGE("Failed to get isp sink pad[0]");
        return rtn;
    }

    src_pad = (struct media_pad*)media_entity_get_pad(stream->adap_ent, src_pad_idx);
    if (!src_pad) {
        ALOGE("Failed to get adap_ent src pad[1]");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);

    /*source:csiphy_ent sink:adap_ent*/
    sink_pad = (struct media_pad*) media_entity_get_pad(stream->adap_ent, sink_pad_idx);
    if (!sink_pad) {
        ALOGE("Failed to get adap sink pad[0]");
        return rtn;
    }

    src_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, src_pad_idx);
    if (!src_pad) {
        ALOGE("Failed to get csiph src pad[1]");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        ALOGE( "Failed to link adap with csiphy");
        return rtn;
    }

    /*source:sensor_ent sink:csiphy_ent*/
    // sensor only has 1 pad
    src_pad =  (struct media_pad*)media_entity_get_pad(stream->sensor_ent, 0);
    if (!src_pad) {
        ALOGE("Failed to get sensor src pad[0]");
        return rtn;
    }

    sink_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, sink_pad_idx);
    if (!sink_pad) {
        ALOGE("Failed to get csiph sink pad[1]");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        ALOGE( "Failed to link sensor with csiphy");
        return rtn;
    }

    ALOGD("create link success ");
    return rtn;
}

int setSdFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    struct v4l2_mbus_framefmt mbus_format;

    mbus_format.width  = cfg->format.width;
    mbus_format.height = cfg->format.height;
    mbus_format.code   = cfg->format.code;

    enum v4l2_subdev_format_whence which = V4L2_SUBDEV_FORMAT_ACTIVE;

    ALOGD("%s ++", __FUNCTION__);

    // sensor source pad fmt
    rtn = v4l2_subdev_set_format(stream->sensor_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        ALOGE("Failed to set sensor format");
        return rtn;
    }

    // csiphy source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        ALOGE("Failed to set csiphy pad[0] format");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        ALOGE("Failed to set csiphy pad[1] format");
        return rtn;
    }

    // adap source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 0, which);

    if (rtn < 0) {
        ALOGE("Failed to set adap pad[0] format");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        ALOGE("Failed to set adap pad[1] format");
        return rtn;
    }

    // isp source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->isp_ent,
          &mbus_format, 0, which);

    if (rtn < 0) {
        ALOGE("Failed to set isp pad[0] format");
        return rtn;
    }
#if 0
    rtn = v4l2_subdev_set_format(stream->isp_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        ALOGE("Failed to set isp pad[1] format");
        return rtn;
    }
#endif
    ALOGD("%s success --", __FUNCTION__);
    return rtn;
}

int setImgFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    ALOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));
    if (cfg->format.nplanes > 1) {
        ALOGE ("not supported yet!");
        return -1;
    }

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = cfg->format.fourcc;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;
    ALOGD("%s ++ %dx%d fmt %d",__FUNCTION__, cfg->format.width, cfg->format.height, cfg->format.fourcc);

    rtn = v4l2_video_set_format( stream->video_ent,&v4l2_fmt);
    if (rtn < 0) {
        ALOGE("Failed to set video fmt, ret %d", rtn);
        return rtn;
    }

    ALOGD("%s success --", __FUNCTION__);
    return rtn;
}

int setDataFormat(media_stream_t *camera, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    ALOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = V4L2_META_AML_ISP_STATS;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;

    rtn = v4l2_video_set_format( camera->video_stats,&v4l2_fmt);
    if (rtn < 0) {
        ALOGE("Failed to set video fmt, ret %d", rtn);
        return rtn;
    }

    ALOGD("%s success", __FUNCTION__);
    return 0;
}

int setConfigFormat(media_stream_t *camera, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    ALOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = V4L2_META_AML_ISP_CONFIG;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;

    rtn = v4l2_video_set_format( camera->video_param,&v4l2_fmt);
    if (rtn < 0) {
        ALOGE("Failed to set video fmt, ret %d", rtn);
        return rtn;
    }

    ALOGD("%s success", __FUNCTION__);
    return 0;
}

int mediaStreamConfig(media_stream_t * stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    ALOGD("%s ++", __FUNCTION__);

    rtn = setSdFormat(stream, cfg);
    if (rtn < 0) {
        ALOGE("Failed to set subdev format");
        return rtn;
    }

    rtn = setImgFormat(stream, cfg);
    if (rtn < 0) {
        ALOGE("Failed to set image format");
        return rtn;
    }

    rtn = createLinks(stream);
    if (rtn) {
        ALOGE( "Failed to create links");
        return rtn;
    }

    ALOGD("Success to config media stream ");
    return 0;
}

