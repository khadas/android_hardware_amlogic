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

#include "CamHalDebugLog.h"

#include "mediaApi.h"

void mediaLog(const char *fmt, ...)
{
    va_list args;
    char buf[256];

    va_start(args, fmt);
    vsnprintf(buf, 256, fmt, args);
    va_end(args);

    CAMHAL_LOGVV("%s ", buf);
}

int mediaStreamInit(media_stream_t *stream, struct media_device * dev)
{
    CAMHAL_LOGD("%s ++", __FUNCTION__);
    memset(stream, 0, sizeof(*stream));

    stream->media_dev = dev;

    if (NULL == stream->media_dev) {
        CAMHAL_LOGE("new media dev fail");
        return -1;
    }

    media_debug_set_handler(dev, mediaLog, NULL);

    if (0 != media_device_enumerate(stream->media_dev) ) {
        CAMHAL_LOGE("media_device_enumerate fail");
        return -1;
    }

    int node_num = media_get_entities_count(stream->media_dev);
    for (int i = 0, j = 0; i < node_num; ++i) {
        struct media_entity *ent = media_get_entity(stream->media_dev, i);
        CAMHAL_LOGI("ent %d, name %s ", i, ent->info.name);
        if (strstr(ent->info.name, "csiphy")) {
            sprintf(stream->csiphy_ent_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "adapter")) {
            sprintf(stream->adap_ent_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "imx")
                || strstr(ent->info.name, "ov")
                || strstr(ent->info.name, "os")
                || strstr(ent->info.name, "lt")) {
            sprintf(stream->sensor_ent_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "dw")) {
            sprintf(stream->lens_ent_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "core")) {
            sprintf(stream->isp_ent_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "stats")) {
            sprintf(stream->video_stats_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "param")) {
            sprintf(stream->video_param_name, "%s", ent->info.name);
        } else if (strstr(ent->info.name, "output")) {
            if (j == 0)
                sprintf(stream->video_ent_name0, "%s", ent->info.name);
            else if (j == 1)
                sprintf(stream->video_ent_name1, "%s", ent->info.name);
            else if (j == 2)
                sprintf(stream->video_ent_name2, "%s", ent->info.name);
            else if (j == 3)
                sprintf(stream->video_ent_name3, "%s", ent->info.name);
            else
                CAMHAL_LOGE("invalid index %d, ent %s", j, ent->info.name);
            j++;
        }
    }

    stream->sensor_ent = media_get_entity_by_name(stream->media_dev, stream->sensor_ent_name, strlen(stream->sensor_ent_name));
    if (NULL == stream->sensor_ent) {
        CAMHAL_LOGE("get  sensor_ent fail");
        return -1;
    }

    stream->lens_ent = media_get_entity_by_name(stream->media_dev, stream->lens_ent_name, strlen(stream->lens_ent_name));

    //mandatory
    stream->csiphy_ent = media_get_entity_by_name(stream->media_dev, stream->csiphy_ent_name, strlen(stream->csiphy_ent_name));
    if (NULL == stream->csiphy_ent) {
        CAMHAL_LOGE("get  csiphy_ent fail");
        return -1;
    }

    stream->adap_ent = media_get_entity_by_name(stream->media_dev, stream->adap_ent_name, strlen(stream->adap_ent_name));
    if (NULL == stream->adap_ent) {
        CAMHAL_LOGE("get adap_ent fail");
        return -1;
    }

    stream->video_ent0 = media_get_entity_by_name(stream->media_dev, stream->video_ent_name0, strlen(stream->video_ent_name0));
    if (NULL == stream->video_ent0) {
        CAMHAL_LOGE("get video_ent0 fail");
        return -1;
    }

    //optional
    stream->isp_ent = media_get_entity_by_name(stream->media_dev, stream->isp_ent_name, strlen(stream->isp_ent_name));
    if (NULL == stream->isp_ent) {
        CAMHAL_LOGE("get isp_ent fail");
    }

    stream->video_ent1 = media_get_entity_by_name(stream->media_dev, stream->video_ent_name1, strlen(stream->video_ent_name1));
    if (NULL == stream->video_ent1) {
        CAMHAL_LOGE("get video_ent1 fail");
    }

    stream->video_ent2 = media_get_entity_by_name(stream->media_dev, stream->video_ent_name2, strlen(stream->video_ent_name2));
    if (NULL == stream->video_ent2) {
        CAMHAL_LOGE("get video_ent22 fail");
    }

    stream->video_ent3 = media_get_entity_by_name(stream->media_dev, stream->video_ent_name3, strlen(stream->video_ent_name3));
    if (NULL == stream->video_ent3) {
        CAMHAL_LOGE("get video_ent3 fail");
    }

    stream->video_stats = media_get_entity_by_name(stream->media_dev, stream->video_stats_name, strlen(stream->video_stats_name));
    if (NULL == stream->video_stats) {
        CAMHAL_LOGE("get video_stats fail");
    }

    stream->video_param = media_get_entity_by_name(stream->media_dev, stream->video_param_name, strlen(stream->video_param_name));
    if (NULL == stream->video_param) {
        CAMHAL_LOGE("get video_param fail");
    }

    int ret = v4l2_video_open(stream->video_ent0);
    CAMHAL_LOGD("%s open video0 fd %d ", __FUNCTION__, stream->video_ent0->fd);

    if (stream->video_ent1) {
        ret = v4l2_video_open(stream->video_ent1);
        CAMHAL_LOGD("%s open video1 fd %d ", __FUNCTION__, stream->video_ent1->fd);
    }
    if (stream->video_ent2) {
        ret = v4l2_video_open(stream->video_ent2);
        CAMHAL_LOGD("%s open video2 fd %d ", __FUNCTION__, stream->video_ent2->fd);
    }
    if (stream->video_ent3) {
        ret = v4l2_video_open(stream->video_ent3);
        CAMHAL_LOGD("%s open video3 fd %d ", __FUNCTION__, stream->video_ent3->fd);
    }

    CAMHAL_LOGD("media stream init success");
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

    CAMHAL_LOGD("create link ++");

    if (stream->media_dev == NULL) {
        return 0;
    }

    if (stream->isp_ent) {
        /*source:adap_ent sink:isp_ent*/
        sink_pad = (struct media_pad*) media_entity_get_pad(stream->isp_ent, sink_pad_idx);
        if (!sink_pad) {
            CAMHAL_LOGE("Failed to get isp sink pad[0]");
            return rtn;
        }

        src_pad = (struct media_pad*)media_entity_get_pad(stream->adap_ent, src_pad_idx);
        if (!src_pad) {
            CAMHAL_LOGE("Failed to get adap_ent src pad[1]");
            return rtn;
        }

        rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    }

    /*source:csiphy_ent sink:adap_ent*/
    sink_pad = (struct media_pad*) media_entity_get_pad(stream->adap_ent, sink_pad_idx);
    if (!sink_pad) {
        CAMHAL_LOGE("Failed to get adap sink pad[0]");
        return rtn;
    }

    src_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, src_pad_idx);
    if (!src_pad) {
        CAMHAL_LOGE("Failed to get csiph src pad[1]");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        CAMHAL_LOGE( "Failed to link adap with csiphy");
        return rtn;
    }

    /*source:sensor_ent sink:csiphy_ent*/
    // sensor only has 1 pad
    src_pad =  (struct media_pad*)media_entity_get_pad(stream->sensor_ent, 0);
    if (!src_pad) {
        CAMHAL_LOGE("Failed to get sensor src pad[0]");
        return rtn;
    }

    sink_pad = (struct media_pad*)media_entity_get_pad(stream->csiphy_ent, sink_pad_idx);
    if (!sink_pad) {
        CAMHAL_LOGE("Failed to get csiph sink pad[1]");
        return rtn;
    }

    rtn = media_setup_link( stream->media_dev, src_pad, sink_pad, flag);
    if (0 != rtn) {
        CAMHAL_LOGE( "Failed to link sensor with csiphy");
        return rtn;
    }

    CAMHAL_LOGD("create link success ");
    return rtn;
}

int setSdFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    struct v4l2_mbus_framefmt mbus_format;
    memset(&mbus_format, 0, sizeof(mbus_format));
    mbus_format.width  = cfg->format.width;
    mbus_format.height = cfg->format.height;
    mbus_format.code   = cfg->format.code;
    mbus_format.field = V4L2_FIELD_NONE;

    enum v4l2_subdev_format_whence which = V4L2_SUBDEV_FORMAT_ACTIVE;

    CAMHAL_LOGD("%s ++", __FUNCTION__);

    // sensor source pad fmt
    rtn = v4l2_subdev_set_format(stream->sensor_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set sensor format");
        return rtn;
    }

    if (cfg->vformat[0].fps > 0) {
        rtn = v4l2_subdev_set_fps(stream->sensor_ent, cfg->vformat[0].fps);
        if (rtn < 0) {
            CAMHAL_LOGE("Failed to set sensor fps, use default\n");
        }
    }


    // csiphy source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 0, which);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set csiphy pad[0] format");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->csiphy_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set csiphy pad[1] format");
        return rtn;
    }

    // adap source & sink pad fmt
    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 0, which);

    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set adap pad[0] format");
        return rtn;
    }

    rtn = v4l2_subdev_set_format(stream->adap_ent,
          &mbus_format, 1, which);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set adap pad[1] format");
        return rtn;
    }

    if (stream->isp_ent) {
        // isp source & sink pad fmt
        rtn = v4l2_subdev_set_format(stream->isp_ent,
              &mbus_format, 0, which);
        if (rtn < 0) {
            CAMHAL_LOGE("Failed to set isp pad[0] format");
            return rtn;
        }
#if 0
        rtn = v4l2_subdev_set_format(stream->isp_ent,
              &mbus_format, 1, which);
        if (rtn < 0) {
            CAMHAL_LOGE("Failed to set isp pad[1] format");
            return rtn;
        }
#endif
    }

    CAMHAL_LOGD("%s success --", __FUNCTION__);
    return rtn;
}

int setImgFormat(media_stream_t *stream, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;
    struct v4l2_rect            v4l2_rct;

    CAMHAL_LOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));
    memset (&v4l2_rct, 0, sizeof (struct v4l2_rect));

    for (int i = 0; i < 4; i++) {
        if (cfg->vformat[i].fps > 0) {
            switch (i) {
                case 0: rtn = v4l2_video_set_fps(stream->video_ent0, cfg->vformat[i].fps);break;
                case 1: rtn = v4l2_video_set_fps(stream->video_ent1, cfg->vformat[i].fps);break;
                case 2: rtn = v4l2_video_set_fps(stream->video_ent2, cfg->vformat[i].fps);break;
                case 3: rtn = v4l2_video_set_fps(stream->video_ent3, cfg->vformat[i].fps);break;
                default:
                    break;
            }
            if (rtn < 0) {
                CAMHAL_LOGE("Failed to set video fps, ret %d", rtn);
                return rtn;
            }
        }
    }

    for (int i = 0; i < 4; ++i) {
        if (cfg->vformat[i].width > 0 && cfg->vformat[i].height > 0) {
            if (cfg->vformat[i].nplanes > 1) {
                CAMHAL_LOGE ("not supported yet!");
                return -1;
            }
            v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            v4l2_fmt.fmt.pix_mp.width        = cfg->vformat[i].width;
            v4l2_fmt.fmt.pix_mp.height       = cfg->vformat[i].height;
            v4l2_fmt.fmt.pix_mp.pixelformat  = cfg->vformat[i].fourcc;
            v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;
            CAMHAL_LOGD("%s:%d ++ %dx%d fmt %d", __FUNCTION__, i,
                cfg->vformat[i].width, cfg->vformat[i].height, cfg->vformat[i].fourcc);
            switch (i) {
                case 0: rtn = v4l2_video_set_format( stream->video_ent0, &v4l2_fmt); break;
                case 1: rtn = v4l2_video_set_format( stream->video_ent1, &v4l2_fmt); break;
                case 2: rtn = v4l2_video_set_format( stream->video_ent2, &v4l2_fmt); break;
                case 3: rtn = v4l2_video_set_format( stream->video_ent3, &v4l2_fmt); break;
                default:
                    break;
            }
            if (rtn < 0) {
                CAMHAL_LOGE("Failed to set video fmt, ret %d", rtn);
                return rtn;
            }
        }
        if (cfg->vformat[i].cwidth > 0 && cfg->vformat[i].cheight > 0) {
            v4l2_rct.left   = cfg->vformat[i].xstart;
            v4l2_rct.top    = cfg->vformat[i].ystart;
            v4l2_rct.width  = cfg->vformat[i].cwidth;
            v4l2_rct.height = cfg->vformat[i].cheight;
            CAMHAL_LOGD("%s:%d ++ crop [%d, %d, %d, %d]", __FUNCTION__, i,
                cfg->vformat[i].xstart, cfg->vformat[i].ystart, cfg->vformat[i].cwidth, cfg->vformat[i].cheight);
            switch (i) {
                case 0: rtn = v4l2_video_crop( stream->video_ent0, &v4l2_rct); break;
                case 1: rtn = v4l2_video_crop( stream->video_ent1, &v4l2_rct); break;
                case 2: rtn = v4l2_video_crop( stream->video_ent2, &v4l2_rct); break;
                case 3: rtn = v4l2_video_crop( stream->video_ent3, &v4l2_rct); break;
                default:
                    break;
            }
            if (rtn < 0) {
                CAMHAL_LOGE("Failed to set video crop, ret %d", rtn);
                return rtn;
            }
        }
    }

    CAMHAL_LOGD("%s success --", __FUNCTION__);
    return rtn;
}

int setDataFormat(media_stream_t *camera, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    CAMHAL_LOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = V4L2_META_AML_ISP_STATS;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;

    rtn = v4l2_video_set_format( camera->video_stats,&v4l2_fmt);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set video fmt, ret %d", rtn);
        return rtn;
    }

    CAMHAL_LOGD("%s success", __FUNCTION__);
    return 0;
}

int setConfigFormat(media_stream_t *camera, stream_configuration_t *cfg)
{
    int rtn = -1;
    struct v4l2_format          v4l2_fmt;

    CAMHAL_LOGD("%s ++", __FUNCTION__);

    memset (&v4l2_fmt, 0, sizeof (struct v4l2_format));

    v4l2_fmt.type                    = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l2_fmt.fmt.pix_mp.width        = cfg->format.width;
    v4l2_fmt.fmt.pix_mp.height       = cfg->format.height;
    v4l2_fmt.fmt.pix_mp.pixelformat  = V4L2_META_AML_ISP_CONFIG;
    v4l2_fmt.fmt.pix_mp.field        = V4L2_FIELD_ANY;

    rtn = v4l2_video_set_format( camera->video_param,&v4l2_fmt);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set video fmt, ret %d", rtn);
        return rtn;
    }

    CAMHAL_LOGD("%s success", __FUNCTION__);
    return 0;
}

int media_set_wdrMode(media_stream_t *camera, uint32_t wdr_mode)
{
    int rtn = 0;

    CAMHAL_LOGD("%s ++ wdr_mode : %d \n", __FUNCTION__, wdr_mode);
    if (wdr_mode != ISP_SDR_DCAM_MODE) {
        // sensor wdr mode
        rtn = v4l2_subdev_set_wdr(camera->sensor_ent, wdr_mode);
        if (rtn < 0) {
            CAMHAL_LOGE("Failed to set sensor wdr mode");
            return rtn;
        }
    }

    // adapter wdr mode
    rtn = v4l2_subdev_set_wdr(camera->adap_ent, wdr_mode);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set adapter wdr mode");
        return rtn;
    }

    // isp wdr mode
    rtn = v4l2_subdev_set_wdr(camera->isp_ent, wdr_mode);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set isp wdr mode");
        return rtn;
    }

    CAMHAL_LOGD("%s success --\n", __FUNCTION__);

    return rtn;
}

int mediaStreamConfig(media_stream_t * stream, stream_configuration_t *cfg)
{
    int rtn = -1;

    CAMHAL_LOGD("%s %dx%d ++", __FUNCTION__, cfg->format.width, cfg->format.height);

    rtn = setSdFormat(stream, cfg);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set subdev format");
        return rtn;
    }

    rtn = setImgFormat(stream, cfg);
    if (rtn < 0) {
        CAMHAL_LOGE("Failed to set image format");
        return rtn;
    }

    rtn = createLinks(stream);
    if (rtn) {
        CAMHAL_LOGE( "Failed to create links");
        return rtn;
    }

    CAMHAL_LOGD("Success to config media stream ");
    return 0;
}

