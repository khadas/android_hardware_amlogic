/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/

#ifndef __DEWARP_H__
#define __DEWARP_H__
#include <cutils/properties.h>
#include "CameraConfig.h"
#include "IonIf.h"
#include "dewarp_api.h"
#include "gdc_api.h"
/*-------------Notice--------------------------
ROTATION_MAX represent supported rotation angle.
[0,90,180,270]
*/
#define ROTATION_MAX 4
enum dewarpcam2port {
    DEWARP_CAM2PORT_PREVIEW = 0,
    DEWARP_CAM2PORT_CAPTURE,
    DEWARP_CAM2PORT_RECORD,
    DEWARP_CAM2PORT_DPTZ_PREVIEW,
    DEWARP_CAM2PORT_USB_PREVIEW,
    DEWARP_CAM2PORT_USB_RECORD,
    DEWARP_CAM2PORT_USB_CAPTURE,
    DEWARP_CAM2PORT_USB_PREVIEW_1,
    DEWARP_CAM2PORT_USB_RECORD_1,
    DEWARP_CAM2PORT_USB_CAPTURE_1,
    DEWARP_CAM2PORT_USB_TRANSITION,
    DEWARP_CAM2PORT_VDIN_PREVIEW,
    DEWARP_CAM2PORT_VDIN_RECORD,
    DEWARP_CAM2PORT_VDIN_CAPTURE,
};

enum Rotation    {
    ROTATION_0 = 0,
    ROTATION_90 = 1,
    ROTATION_180 = 2,
    ROTATION_270 = 3,
};

typedef struct GDCInParam {
    uint32_t i_width;
    uint32_t i_height;
    uint32_t o_width;
    uint32_t o_height;
    uint32_t rotation;
} GDCInParam_t;

namespace android {
    class DeWarp {
        private:
            DeWarp(int groupId,Rotation rotation);
            ~DeWarp();
            void set_input_buffer(int in_fd);
            void set_output_buffer(int out_fd);
            bool load_config_file();
            int dewarp_init() ;
            int gdc_init();
            void gdc_exit();
            int dewarp_to_libgdc_format(int dewarp_format);
        private:
            static DeWarp* mInstance[ISP_PORT_NUM][ROTATION_MAX];
            static Mutex mMutex;
            static int dptz_CropX, dptz_CropY, dptz_CropWidth, dptz_CropHeight;
            struct gdc_usr_ctx_s *mGDCContext = nullptr;
            struct dewarp_params mDewarp_params;
            int mFw_fd = -1;
            uint8_t* mFw_buffer = nullptr;
            IONInterface* mION;
        public:
            static DeWarp* getInstance(int groupId,int proj_mode,Rotation rotation);
            static void putInstance(std::pair<int, int> range);
            static void putInstance(int groupId);
            void gdc_do_fisheye_correction() ;
            static void set_src_ROI(int x, int y, int w, int h);
            void setCrop();
        public:
            int mInput_fd = -1;
            int mOutput_fd = -1;
            int mProj_mode = -1;
            Rotation mRotation;
            int mGroupId = -1;
    };
}
#endif
