/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/videodev2.h>


#include "mediactl.h"
#include "tools.h"
#include "v4l2videodev.h"


int v4l2_video_open(struct media_entity *entity)
{
    if (entity->fd != -1) {
        return 0;
    }

    entity->fd = open(entity->devname, O_RDWR);
    if (entity->fd == -1) {
        int ret = -errno;
        media_dbg(entity->media,
              "%s: Failed to open subdev device node %s\n", __func__,
              entity->devname);
        return ret;
    } else {
        media_dbg(entity->media,
              "%s: open subdev device node %s ok, fd %d \n", __func__,
              entity->devname, entity->fd);
    }

    return 0;
}


void v4l2_video_close(struct media_entity *entity)
{
    close(entity->fd);
    entity->fd = -1;
}



int v4l2_video_get_format(struct media_entity *entity,
                      struct v4l2_format *v4l2_fmt)
{

    int ret;

    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;

    ret = ioctl (entity->fd, VIDIOC_G_FMT, v4l2_fmt);
    if (ret < 0) {
        media_dbg(entity->media,
              "Error: get format, ret %d.\n", ret);
        return ret;
    }

    return 0;
}

int v4l2_video_set_format(struct media_entity *entity,
              struct v4l2_format * v4l2_fmt)
{

    int ret;

    ret = v4l2_video_open(entity);
    if (ret < 0) {
        return ret;
    }


    ret = ioctl (entity->fd, VIDIOC_S_FMT, v4l2_fmt);
    if (ret < 0) {
        media_dbg(entity->media,
              "Error: set format, ret %d.\n", ret);
        return ret;
    }

    media_dbg(entity->media,
          "set format ok, ret %d.\n", ret);

    return 0;
}


int v4l2_video_get_capability(struct media_entity *entity,
                        struct v4l2_capability * v4l2_cap)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;

    ret = ioctl (entity->fd, VIDIOC_QUERYCAP, v4l2_cap);
    if (ret < 0) {
        media_dbg(entity->media,
            "Error: get capability fail, ret %d\n", ret);
        return ret;
    }

    media_dbg(entity->media,
        "VIDIOC_QUERYCAP: success \n");

    return 0;
}



int v4l2_video_req_bufs(struct media_entity *entity,
                        struct v4l2_requestbuffers * v4l2_rb)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;

    ret = ioctl (entity->fd, VIDIOC_REQBUFS, v4l2_rb);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: request buffer, ret %d.\n", ret);
        return ret;
    }

    media_dbg(entity->media,
                " request buf ok\n");
    return ret;
}

int v4l2_video_query_buf(struct media_entity *entity,
                       struct v4l2_buffer *v4l2_buf)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;


    ret= ioctl (entity->fd, VIDIOC_QUERYBUF, v4l2_buf);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: query buffer ret %d.\n", ret);
        return ret;
    }
    media_dbg(entity->media,
            "query buffer success \n");
    return ret;
}

int v4l2_video_q_buf(struct media_entity *entity,
                       struct v4l2_buffer *v4l2_buf)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;


    ret = ioctl (entity->fd, VIDIOC_QBUF, v4l2_buf);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: queue buffer ret %d.\n", ret);
        return ret;
    }

    media_dbg(entity->media,
            "queue buffer success \n");
    return ret;

}



int v4l2_video_dq_buf(struct media_entity *entity,
                       struct v4l2_buffer *v4l2_buf)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;


    ret = ioctl (entity->fd, VIDIOC_DQBUF, v4l2_buf);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: dq buffer ret %d.\n", ret);
        return ret;
    }

    media_dbg(entity->media,
            "dq buffer success \n");
    return ret;

}


int v4l2_video_stream_on(struct media_entity *entity, int type)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;

    ret = ioctl (entity->fd, VIDIOC_STREAMON, &type);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: streamon.failed. ret %d \n", ret);
    }
    media_dbg(entity->media,
            "streamon   success \n");
    return ret;

}

int v4l2_video_stream_off(struct media_entity *entity, int type)
{
    int ret = -1;
    ret = v4l2_video_open(entity);
    if (ret < 0)
        return ret;

    ret = ioctl (entity->fd, VIDIOC_STREAMOFF, &type);
    if (ret < 0) {
        media_dbg(entity->media,
                "Error: streamon.failed. ret %d \n", ret);
    }
    media_dbg(entity->media,
            "streamon   success \n");
    return ret;

}



