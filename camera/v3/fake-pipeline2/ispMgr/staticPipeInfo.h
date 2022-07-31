#ifndef __STATIC_PIPE_INFO_H__
#define __STATIC_PIPE_INFO_H__

#include <cstdlib>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/tools.h"
#include "media-v4l2/mediaApi.h"

#if 1
struct pipe_info pipe_0 = {
   .media_dev_name   = "/dev/media0",
   .sensor_ent_name  = "imx290-0",
   .csiphy_ent_name  = "isp-csiphy",
   .adap_ent_name    = "isp-adapter",
   .isp_ent_name     = "isp-core",
   .video_ent_name0  = "isp-output0",
   .video_ent_name1  = "isp-output1",
   .video_ent_name2  = "isp-output2",
   .video_ent_name3  = "isp-output3",
   .video_stats_name = "isp-stats",
   .video_param_name = "isp-param",
   .ispDev           = true,
};

struct pipe_info pipe_1 = {
   .media_dev_name   = "/dev/media1",
   .sensor_ent_name  = "imx290-1",
   .csiphy_ent_name  = "t7-csi2phy-1",
   .adap_ent_name    = "t7-adapter-1",
   .video_ent_name0  = "isp-output0",
   .video_ent_name1  = "isp-output1",
   .video_ent_name2  = "isp-output2",
   .video_ent_name3  = "isp-output3",
   .ispDev           = true,
};

static struct pipe_info *supportedPipes[] = {
    &pipe_0,
    &pipe_1
};

#else
struct pipe_info pipe_0 = {
   .media_dev_name  = "/dev/media0",
   .sensor_ent_name = "ov5640-0",
   .csiphy_ent_name = "t7-csi2phy-0",
   .adap_ent_name   = "t7-adapter-0",
   .video_ent_name0 = "t7-video-0-0",
   .ispDev          = false,
};

struct pipe_info pipe_1 = {
    .media_dev_name  = "/dev/media1",
    .sensor_ent_name = "ov5640-1",
    .csiphy_ent_name = "t7-csi2phy-1",
    .adap_ent_name   = "t7-adapter-1",
    .video_ent_name0 = "t7-video-1-0",
    .ispDev          = false,
};

static struct pipe_info * supportedPipes[] = {
    &pipe_0,
    &pipe_1
};
#endif

#endif
