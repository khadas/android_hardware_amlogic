
#define LOG_TAG "IspMgr"

#include <cstdlib>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include <vector>

#include "staticPipe.h"

namespace android {

std::vector<struct pipe_info*> staticPipe::supportedPipes;

static struct pipe_info isp_pipe_0 = {
   .media_dev_name   = "/dev/media0",
   .sensor_ent_name  = "imx290-0",
   .csiphy_ent_name  = "isp-csiphy",
   .adap_ent_name    = "isp-adapter",
   .isp_ent_name     = "isp-core",
   .video_ent_name   = "isp-output0",
   .video_stats_name = "isp-stats",
   .video_param_name = "isp-param",
   .ispDev           = true,
};

static struct pipe_info isp_pipe_1 = {
   .media_dev_name   = "/dev/media1",
   .sensor_ent_name  = "imx290-1",
   .csiphy_ent_name  = "t7-csi2phy-1",
   .adap_ent_name    = "t7-adapter-1",
   .video_ent_name   = "t7-video-1-0",
   .ispDev           = true,
};

static struct pipe_info auto_pipe_0 = {
   .media_dev_name  = "/dev/media0",
   .sensor_ent_name = "ov5640-0",
   .csiphy_ent_name = "t7-csi2phy-0",
   .adap_ent_name   = "t7-adapter-0",
   .video_ent_name  = "t7-video-0-0",
   .ispDev          = false,
};

static struct pipe_info auto_pipe_1 = {
    .media_dev_name  = "/dev/media1",
    .sensor_ent_name = "ov5640-1",
    .csiphy_ent_name = "t7-csi2phy-1",
    .adap_ent_name   = "t7-adapter-1",
    .video_ent_name  = "t7-video-1-0",
    .ispDev          = false,
};

int staticPipe::constructStaticPipe() {
    if (supportedPipes.size() > 0) {
        ALOGD("already init");
        return 0;
    }
    char property[PROPERTY_VALUE_MAX];
    struct pipe_info *pipe_0;
    struct pipe_info *pipe_1;
    property_get("vendor.media.isp.enable", property, "true");
    if (strstr(property, "false")) {
        pipe_0 = &auto_pipe_0;
        pipe_1 = &auto_pipe_1;
        ALOGD("%s isp disable", __FUNCTION__);
    } else {
        pipe_0 = &isp_pipe_0;
        pipe_1 = &isp_pipe_1;
        ALOGD("%s isp enable", __FUNCTION__);
    }
    // main sensor
    if (property_get("ro.media.camera.sensor.main", property, NULL) > 0) {
        if (strstr(property, "imx290")) {
            pipe_0->sensor_ent_name = "imx290-0";
        } else if (strstr(property, "ov5640")) {
            pipe_0->sensor_ent_name = "ov5640-0";
        } else if (strstr(property, "imx415")) {
            pipe_0->sensor_ent_name = "imx415-0";
        }
    }
    ALOGD("%s main sensor use %s", __FUNCTION__, pipe_0->sensor_ent_name);
    //sub sensor
    if (property_get("ro.media.camera.sensor.sub", property, NULL) > 0) {
        if (strstr(property, "imx290")) {
            pipe_1->sensor_ent_name = "imx290-1";
        } else if (strstr(property, "ov5640")) {
            pipe_1->sensor_ent_name = "ov5640-1";
        }
    }
    ALOGD("%s sub sensor use %s", __FUNCTION__, pipe_1->sensor_ent_name);
    supportedPipes.push_back(pipe_0);
    supportedPipes.push_back(pipe_1);
    return 0;
}
}
