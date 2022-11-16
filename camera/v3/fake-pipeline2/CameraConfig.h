/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#ifndef __CAMERA_CONFIG_H__
#define __CAMERA_CONFIG_H__

#include <stdio.h>
#define ISP_PORT_NUM 2
namespace android {
    class CameraConfig {
        public:
            static CameraConfig* mInstance[ISP_PORT_NUM];
            struct GDCParam {
                uint32_t width;
                uint32_t height;
                uint32_t planeNum;
            };
            struct GDCParam mGDCParam;
            struct GE2DParam {
                uint32_t src_width;
                uint32_t src_height;
            };
            struct GE2DParam mGE2DParam;
            uint32_t getWidth(void);
            void setWidth(uint32_t width);
            uint32_t getHeight(void);
            void setHeight(uint32_t height);
        private:
            CameraConfig(int groupId);
            ~CameraConfig();
        public:
            static CameraConfig* getInstance(int groupId);
            static void deleteInstance();
    };
}

#endif
