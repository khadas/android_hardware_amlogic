/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/

#define LOG_TAG "CamHAL_v4l2utils"

#include "V4l2Utils.h"
#include <log/log.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdbool.h>
#include <cutils/native_handle.h>


namespace android {

    void V4l2Utils::set_sensor_power(int videofd,int on) {
        struct v4l2_control ctrl;
        memset(&ctrl, 0, sizeof(ctrl));
        ctrl.id=ISP_V4L2_CID_SET_SENSOR_POWER;
        ctrl.value=on;
        if (-1 == ioctl (videofd, VIDIOC_S_CTRL, &ctrl)) {
            printf("ISP set sensor power failed\n");
            return;
        } else printf("ISP set sensor power %d \n",on);
    }
    void V4l2Utils::set_md_enable(int videofd, int enable)
    {
        struct v4l2_control ctrl;
        ctrl.id = ISP_V4L2_CID_SET_SENSOR_MD_EN;
        ctrl.value = enable;
        if (-1 == ioctl (videofd, VIDIOC_S_CTRL, &ctrl)) {
            printf("set_md_enable failed\n");
        }
    }

    void V4l2Utils::set_decmpr_enable(int videofd, int enable)
    {
        struct v4l2_control ctrl;
        ctrl.id = ISP_V4L2_CID_SET_SENSOR_DECMPR_EN;
        ctrl.value = enable;
        if (-1 == ioctl (videofd, VIDIOC_S_CTRL, &ctrl)) {
            printf("set_decmpr_enable failed\n");
        }
    }

    void V4l2Utils::set_lossless_enable(int videofd, int enable)
    {
        struct v4l2_control ctrl;
        ctrl.id = ISP_V4L2_CID_SET_SENSOR_DECMPR_LOSSLESS_EN;
        ctrl.value = enable;
        if (-1 == ioctl (videofd, VIDIOC_S_CTRL, &ctrl)) {
            printf("set_lossless_enable failed\n");
        }
    }
    //static unsigned char md_data[ISP_V4L2_MD_STATS_SIZE];
    void V4l2Utils::get_md_stats(int videofd, int cnt)
    {
        struct v4l2_ext_controls ctrls;
        struct v4l2_ext_control ext_ctrl;
        if (cnt % 25 != 0)
            return;
        unsigned char* md_data = (unsigned char *)malloc(ISP_V4L2_MD_STATS_SIZE);
        /*get*/
        memset(&ext_ctrl, 0, sizeof(ext_ctrl));
        memset(&ctrls, 0, sizeof(ctrls));
        ext_ctrl.id = ISP_V4L2_CID_MD_STATS;
        ext_ctrl.ptr = md_data;
        ext_ctrl.size = ISP_V4L2_MD_STATS_SIZE;
        ctrls.which = 0;
        ctrls.count = 1;
        ctrls.controls = &ext_ctrl;

        if (-1 == ioctl (videofd, VIDIOC_G_EXT_CTRLS, &ctrls)) {
            printf("get md stats failed\n");
            return;
        } else {
            /*md stats data*/
            //save_img(md_data, 4096, 6, cnt*4);
        }
    }

    int V4l2Utils::dump(const uint8_t* data, int size, int width, int height) {

        std::string dump_name = "/data/dump_hal_"+ std::to_string(width) + "_" + std::to_string(height) +".yuv";
        ALOGE("dump %s\n",dump_name.c_str());
        int fd = open(dump_name.c_str(),O_RDWR|O_APPEND|O_CREAT,0666);
        if (fd < 0) {
            ALOGE("open %s fail",dump_name.c_str());
            return -1;
        }
        int ret = write(fd,(void*)data, size);
        if (ret == -1) {
            ALOGE("write file fail:%s",strerror(errno));
            return -1;
        }
        close(fd);

        return 0;
    }
    void V4l2Utils::set_notify_3A_is_capturing(int videofd, int isCapturing) {

        struct v4l2_control ctrl;
        int ret = 0;
        ctrl.id = ISP_V4L2_CID_SET_IS_CAPTURING;
        ctrl.value = isCapturing;
        ret = ioctl (videofd, VIDIOC_S_CTRL, &ctrl);
        if (ret < 0 ) {
            ALOGE("notify to 3A capture status failed: %s\n",strerror(errno));
        }
    }
    int V4l2Utils::set_ae_fps_range(int videofd, int min_fps, int max_fps)
    {
        struct v4l2_ext_controls ctrls;
        struct v4l2_ext_control ext_ctrl;
        uint32_t fps_range[2];//fps_range[0]:min fps_range[1]:max
        /*set*/
        memset(&ext_ctrl, 0, sizeof(ext_ctrl));
        memset(&ctrls, 0, sizeof(ctrls));
        //[min,max] must in [0,30]
        if ( min_fps >= 0 ) {
            fps_range[0] = min_fps;
        } else {
            return -1;
        }

        if ( max_fps >= 0 && max_fps <= 30) {
            fps_range[1] = max_fps;
        } else {
            return -1;
        }
        ext_ctrl.id = ISP_V4L2_CID_SET_FPS_RANGE;
        ext_ctrl.ptr = fps_range;
        ext_ctrl.size = sizeof(fps_range);
        ctrls.which = 0;
        ctrls.count = 1;
        ctrls.controls = &ext_ctrl;

        if (-1 == ioctl (videofd, VIDIOC_S_EXT_CTRLS, &ctrls)) {
                ALOGE("camera hal set_ae_fps_range failed\n");
                return -1;
        }
        return 0;
    }
}
