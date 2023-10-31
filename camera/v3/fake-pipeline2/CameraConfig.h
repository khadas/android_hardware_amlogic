/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#ifndef __CAMERA_CONFIG_H__
#define __CAMERA_CONFIG_H__

#include <stdio.h>

#include "media-v4l2/mediaApi.h"

#define ISP_PORT_NUM 6

namespace android {
    struct CropInfo {
        uint32_t originWidth;
        uint32_t originHeight;
        uint32_t width;
        uint32_t height;
        CropInfo() : originWidth(0), originHeight(0), width(0), height(0) {}
    };

    class CameraConfig {
        public:
            static CameraConfig* mInstance[ISP_PORT_NUM];
            struct GDCParam {
                uint32_t width;
                uint32_t height;
                uint32_t stride;
                uint32_t input_stride;
                uint32_t planeNum;
                uint32_t input_width;
                uint32_t input_height;
                CropInfo mCropInfo;
            };
            struct GDCParam mGDCParam;
            struct GE2DParam {
                uint32_t src_width;
                uint32_t src_height;
            };
            struct GE2DParam mGE2DParam;
            media_stream_t mSensorParam;
            uint32_t getWidth(void);
            void setWidth(uint32_t width);
            uint32_t getHeight(void);
            void setHeight(uint32_t height);
            uint32_t getStride(void);
            void setStride(uint32_t stride);
            uint32_t getInputStride(void);
            void setInputStride(uint32_t stride);
            void setSensorCfg(media_stream_t& stream);
            uint32_t getInputWidth(void);
            void setInputWidth(uint32_t width);
            uint32_t getInputHeight(void);
            void setInputHeight(uint32_t height);
            CropInfo getCropInfo(void);
            void setCropInfo(CropInfo      inputCropInfo);
        private:
            CameraConfig(int groupId);
            ~CameraConfig();
        public:
            static CameraConfig* getInstance(int groupId);
            static void deleteInstance();
    };
}

#endif
