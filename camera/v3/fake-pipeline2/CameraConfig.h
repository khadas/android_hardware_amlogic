/*
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/
#ifndef __CAMERA_CONFIG_H__
#define __CAMERA_CONFIG_H__

#include <stdio.h>

#include "media-v4l2/mediaApi.h"

#define ISP_PORT_NUM 10

namespace android {
    struct CropInfo {
        uint32_t srcWidth;
        uint32_t srcHeight;
        uint32_t width;
        uint32_t height;
        uint32_t offset_x;
        uint32_t offset_y;
        CropInfo() : srcWidth(0), srcHeight(0), width(0), height(0), offset_x(0), offset_y(0) {}
    };
    class CameraConfig {
        public:
            static CameraConfig* mInstance[ISP_PORT_NUM];
            struct GDCParam {
                uint32_t o_width;
                uint32_t o_height;
                uint32_t o_stride;
                uint32_t i_stride;
                uint32_t planeNum;
                uint32_t i_width;
                uint32_t i_height;
                CropInfo mCropInfo;
                bool facingback;
            };
            struct GDCParam mGDCParam;
            struct GE2DParam {
                uint32_t src_width;
                uint32_t src_height;
            };
            struct GE2DParam mGE2DParam;
            media_stream_t mSensorParam;
            uint32_t getOutputWidth(void);
            void setOutputWidth(uint32_t width);
            uint32_t getOutputHeight(void);
            void setOutputHeight(uint32_t height);
            uint32_t getOutputStride(void);
            void setOutputStride(uint32_t stride);
            uint32_t getInputStride(void);
            void setInputStride(uint32_t stride);
            void setSensorCfg(media_stream_t& stream);
            uint32_t getInputWidth(void);
            void setInputWidth(uint32_t width);
            uint32_t getInputHeight(void);
            void setInputHeight(uint32_t height);
            CropInfo getCropInfo(void);
            void setCropInfo(CropInfo      inputCropInfo);
            inline bool getFacing(void) { return mGDCParam.facingback;}
            inline void setFacing(bool __facing) { mGDCParam.facingback = __facing; }
        private:
            CameraConfig(int groupId);
            ~CameraConfig();
        public:
            static CameraConfig* getInstance(int groupId);
            static void deleteInstance(std::pair<int, int> range);
    };
}

#endif
