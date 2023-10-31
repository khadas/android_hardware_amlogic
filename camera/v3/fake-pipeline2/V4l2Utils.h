/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/

#ifndef __V4L2UTILS__
#define __V4L2UTILS__

#include <utils/Thread.h>
#include <linux/videodev2.h>
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

using std::string;
using std::vector;
#define ISP_V4L2_CID_BASE               (0x00f00000 | 0xf000)

#define ISP_V4L2_CID_SET_SENSOR_POWER ( ISP_V4L2_CID_BASE + 154 )
#define ISP_V4L2_CID_SET_SENSOR_MD_EN ( ISP_V4L2_CID_BASE + 155 )
#define ISP_V4L2_CID_SET_SENSOR_DECMPR_EN ( ISP_V4L2_CID_BASE + 156 )
#define ISP_V4L2_CID_SET_SENSOR_DECMPR_LOSSLESS_EN ( ISP_V4L2_CID_BASE + 157 )
#define ISP_V4L2_CID_SET_SENSOR_FLICKER_EN ( ISP_V4L2_CID_BASE + 158 )
#define ISP_V4L2_CID_MD_STATS ( ISP_V4L2_CID_BASE + 159 )
#define ISP_V4L2_MD_STATS_SIZE             (4096)
#define ISP_V4L2_CID_SET_IS_CAPTURING ( ISP_V4L2_CID_BASE + 164 )
#define ISP_V4L2_CID_SET_FPS_RANGE ( ISP_V4L2_CID_BASE + 165 )

enum {
    NOT_CAPTURING = 0,
    DO_CAPTURING
};

namespace android {

class V4l2Utils {
    public:
        static void set_sensor_power(int videofd,int on);
        static void set_md_enable(int videofd, int enable);
        static void set_decmpr_enable(int videofd, int enable);
        static void set_lossless_enable(int videofd, int enable);
        static void get_md_stats(int videofd, int cnt);
		static int dump(const uint8_t* data, int size, int width, int height);
        static void set_notify_3A_is_capturing(int videofd, int isCapturing);
        static int set_ae_fps_range(int videofd, int min_fps, int max_fps);
};
}
#endif
