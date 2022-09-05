/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/

#define LOG_NDEBUG 0
#define LOG_TAG "deWarp"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>
#include "dewarp.h"

namespace android {

    DeWarp* DeWarp::mInstance[ISP_PORT_NUM][ROTATION_MAX] = {{nullptr}};
    Mutex DeWarp::mMutex;
    int DeWarp::dptz_CropX, DeWarp::dptz_CropY, DeWarp::dptz_CropWidth, DeWarp::dptz_CropHeight;

    DeWarp::DeWarp(int groupId,Rotation rotation):
        mRotation(rotation),
        mGroupId(groupId)
    {
        ALOGD("%s: E \n",__FUNCTION__);
        //gdc init and alloc buffer for gdc
        memset(&mDewarp_params,0,sizeof(struct dewarp_params));
        if (mGDCContext == nullptr)
            mGDCContext = (struct gdc_usr_ctx_s*)malloc(sizeof(struct gdc_usr_ctx_s));

        CameraConfig* config = CameraConfig::getInstance(mGroupId);

        gdc_init(config->mGDCParam.width,config->mGDCParam.height,YUV420_SEMIPLANAR,config->mGDCParam.planeNum);
    }

    DeWarp::~DeWarp() {
        gdc_exit();
        if (mGDCContext) {
            free(mGDCContext);
            mGDCContext = nullptr;
        }

    }
    void DeWarp::set_input_buffer(int in_fd) {
        ATRACE_CALL();
        struct gdc_settings_ex *gdc_gs = &mGDCContext->gs_ex;
        gdc_gs->input_buffer.plane_number = 1;
        gdc_gs->input_buffer.shared_fd = in_fd;
        gdc_gs->input_buffer.mem_alloc_type = mGDCContext->mem_type ;
    }
    void DeWarp::set_output_buffer(int out_fd) {
        ATRACE_CALL();
        struct gdc_settings_ex *gdc_gs = &mGDCContext->gs_ex;
        gdc_gs->output_buffer.plane_number = 1;
        gdc_gs->output_buffer.shared_fd = out_fd;
        gdc_gs->output_buffer.mem_alloc_type = mGDCContext->mem_type ;
    }

    bool DeWarp::load_config_file(size_t width, size_t height,int plane_number) {
        ALOGD("%s: E \n",__FUNCTION__);
        ATRACE_CALL();
        struct gdc_settings_ex *gdc_gs = &mGDCContext->gs_ex;
        uint8_t* fw_buffer = nullptr;
        int fw_max_len = 300 * 1024;

        //----alloc memory
        IONInterface* IONDevice = IONInterface::get_instance();
        fw_buffer = IONDevice->alloc_buffer(fw_max_len, &mFw_fd);
        if (fw_buffer == nullptr) {
            ALOGE("failed to allocate config buffer %d",fw_max_len);
            return false;
        }
        //generate firmware
        dewarp_gen_config(&mDewarp_params, (int*)fw_buffer);

        gdc_gs->config_buffer.plane_number = plane_number;
        gdc_gs->config_buffer.mem_alloc_type = mGDCContext->mem_type;
        gdc_gs->config_buffer.shared_fd = mFw_fd;

        return true;
    }

    int DeWarp::dewarp_init(size_t width, size_t height, int format) {
        ALOGD("%s: E \n",__FUNCTION__);
        struct input_param* in   = &mDewarp_params.input_param;
        struct output_param* out = &mDewarp_params.output_param;
        struct proj_param *proj  = &mDewarp_params.proj_param[0];
        struct win_param *win    = &mDewarp_params.win_param[0];
        //struct dptz_param *dptz_param  = &mDewarp_params.dptz_param;
        /*For yuv color space, set dummy data
            (0,128,128) is black

            replace_0:y
            replace_1:u or v
            replace_2:v or u

            edge_0:y
            edge_1:u or v
            edge_2:v or u
        */
        mDewarp_params.proc_param.replace_0 = 0;
        mDewarp_params.proc_param.replace_1 = 128;
        mDewarp_params.proc_param.replace_2 = 128;

        mDewarp_params.proc_param.edge_0 = 0;
        mDewarp_params.proc_param.edge_1 = 128;
        mDewarp_params.proc_param.edge_2 = 128;
        char property[PROPERTY_VALUE_MAX];
        int width_tmp = width;
        int height_tmp = height;
        mDewarp_params.win_num = 1;
        in->width = width;
        in->height = height;
        in->offset_x = 0;
        in->offset_y = 0;
        in->fov = 120;
        mDewarp_params.color_mode = YUV420_SEMIPLANAR;
        /*ROTATION_90 ROTATION_270 ouput need exchange width and height,input no need*/
        width = (mRotation == Rotation::ROTATION_0 || mRotation == Rotation::ROTATION_180) ? width_tmp : height_tmp;
        height = (mRotation == Rotation::ROTATION_0 || mRotation == Rotation::ROTATION_180) ? height_tmp : width_tmp;

        out->width = width;
        out->height = height;
        property_get("vendor.camhal.use.dewarp.linear", property, "false");
        if (strstr(property, "true")) {
            proj[0].projection_mode = PROJ_MODE_LINEAR;
            mProj_mode = PROJ_MODE_LINEAR;
        } else {
            proj[0].projection_mode = PROJ_MODE_EQUISOLID;
            mProj_mode = PROJ_MODE_EQUISOLID;
        }
        proj[0].pan = 0;
        proj[0].tilt = 0;
        proj[0].rotation = (int)mRotation*90;
        if (height >= 1080)
            proj[0].zoom = 1.005;
        else if(height >= 720)
            proj[0].zoom = 1.006;
        else if(height >= 480)
            proj[0].zoom = 1.01;
        else if(height >= 288)
            proj[0].zoom = 1.015;
        else
            proj[0].zoom = 1.02;

        if (proj[0].rotation == 180 || proj[0].rotation == 270) {
            if (height >= 1080)
                proj[0].zoom = 1.006;
            else if(height >= 720)
                proj[0].zoom = 1.01;
            else if(height >= 480)
                proj[0].zoom = 1.015;
            else if(height >= 288)
                proj[0].zoom = 1.02;
            else
                proj[0].zoom = 1.025;
        }

        proj[0].strength_hor = 1.0;
        proj[0].strength_ver = 1.0;

        win[0].win_start_x = 0;
        win[0].win_end_x = width - 1;
        win[0].win_start_y = 0;
        win[0].win_end_y = height - 1;
        win[0].img_start_x = 0;
        win[0].img_end_x = width - 1;
        win[0].img_start_y = 0;
        win[0].img_end_y = height - 1;

        mDewarp_params.tile_x_step = 16;
        mDewarp_params.tile_y_step = 16;
        mDewarp_params.prm_mode = 0;
#if 0
        memset(property,0,sizeof(property));
        property_get("vendor.camhal.use.dewarp.dptz", property, "false");
        if (strstr(property, "true")) {
            /*DPTZ*/
            if ( mGroupId == DEWARP_CAM2PORT_CAPTURE ) {
                // 2592x1944 -> 2592x1944   (still capture)
                struct dptz_roi_info dptz_roi_info = {
                    0,0,(int)width,(int)height,
                    (int)width,(int)height, (int)width,(int)height };

                dptz_compute_params(&dptz_roi_info, dptz_param, 0);
                mDewarp_params.prm_mode = 3;
            } else if ( mGroupId == DEWARP_CAM2PORT_PREVIEW ) {
                // 2592x1944 -> 1440x1080   (still mode)   -or-
                // 2592x1456 -> 1920x1080   (video mode)
                struct dptz_roi_info dptz_roi_info = {
                    dptz_CropX,dptz_CropY,dptz_CropWidth,dptz_CropHeight,
                    (int)width,(int)height, (int)width,(int)height };

                dptz_compute_params(&dptz_roi_info, dptz_param, 0);
                mDewarp_params.prm_mode = 3;
            } else {
                mDewarp_params.prm_mode = 0;  // linear mode
            }
        }
#endif
        return 0;
    }


    int DeWarp::gdc_init(size_t width, size_t height, int format , int plane_number) {
        ALOGD("%s: width = %d, height = %d, format=%d, plane_number=%dE \n",
                __FUNCTION__,width,height,format,plane_number);
        int i_y_stride = 0;
        int i_c_stride = 0;
        int o_y_stride = 0;
        int o_c_stride = 0;
        int i_width  = 0;
        int i_height = 0;
        int o_width  = 0;
        int o_height = 0;

        struct gdc_settings_ex *gdc_gs = nullptr;
        int ret = -1;

        dewarp_init(width, height, format);

        mGDCContext->custom_fw = 0;                 /* not use builtin fw */
        mGDCContext->mem_type = AML_GDC_MEM_ION;    /* use ION memory to test */
        mGDCContext->plane_number = plane_number;   /* data in one continuous mem block */
        mGDCContext->dev_type = AML_GDC;            /* dewarp */

        int gdc_format = dewarp_to_libgdc_format(mDewarp_params.color_mode);

        i_width  = mDewarp_params.input_param.width;
        i_height = mDewarp_params.input_param.height;
        o_width  = mDewarp_params.output_param.width;
        o_height = mDewarp_params.output_param.height;

        if (gdc_format == NV12) {
            i_y_stride = AXI_WORD_ALIGN(i_width);
            o_y_stride = AXI_WORD_ALIGN(o_width);
            i_c_stride = AXI_WORD_ALIGN(i_width);
            o_c_stride = AXI_WORD_ALIGN(o_width);
        } else if (gdc_format == YV12) {
            i_c_stride = AXI_WORD_ALIGN(i_width / 2);
            o_c_stride = AXI_WORD_ALIGN(o_width / 2);
            i_y_stride = i_c_stride * 2;
            o_y_stride = o_c_stride * 2;
        } else if (gdc_format == Y_GREY) {
            i_y_stride = AXI_WORD_ALIGN(i_width);
            o_y_stride = AXI_WORD_ALIGN(o_width);
            i_c_stride = 0;
            o_c_stride = 0;
        } else {
            E_GDC("Error unknow format\n");
            return ret;
        }

        ret = gdc_create_ctx(mGDCContext);
        if (ret < 0) {
            ALOGE("failed to gdc_create_ctx");
            return ret;
        }
        gdc_gs = &mGDCContext->gs_ex;

        gdc_gs->gdc_config.input_width = i_width;
        gdc_gs->gdc_config.input_height = i_height;
        gdc_gs->gdc_config.input_y_stride = i_y_stride;
        gdc_gs->gdc_config.input_c_stride = i_c_stride;
        gdc_gs->gdc_config.output_width = o_width;
        gdc_gs->gdc_config.output_height = o_height;
        gdc_gs->gdc_config.output_y_stride = o_y_stride;
        gdc_gs->gdc_config.output_c_stride = o_c_stride;
        gdc_gs->gdc_config.format = format;
        gdc_gs->magic = sizeof(*gdc_gs);
        //ALOGE("%s-%d         i_width:%d i_height:%d,i_y_stride:%d i_c_stride:%d o_width:%d o_height:%d o_y_stride:%d o_c_stride:%d \n",__func__,__LINE__,\
        //                       i_width,  i_height,   i_y_stride,   i_c_stride,   o_width,   o_height,   o_y_stride,   o_c_stride);

        //-----load gdc config
        if (!load_config_file(width, height,plane_number)) {
            ALOGE("failed to load gdc config");
            return -1;
        }
        return 0;
    }

    DeWarp* DeWarp::getInstance(int groupId,int proj_mode,Rotation rotation) {

        if ((groupId >= ISP_PORT_NUM) || ((int)rotation >= ROTATION_MAX) )
            return nullptr;

        if (mInstance[groupId][(int)rotation] != nullptr && \
            (mInstance[groupId][(int)rotation]->mProj_mode == proj_mode) && \
            (mInstance[groupId][(int)rotation]->mRotation == rotation)) {

            return mInstance[groupId][(int)rotation];

        } else {
            if (mInstance[groupId][(int)rotation]) delete mInstance[groupId][(int)rotation];
            Mutex::Autolock lock(&mMutex);
            mInstance[groupId][(int)rotation] = new DeWarp(groupId,rotation);
            return mInstance[groupId][(int)rotation];
        }
    }

    void DeWarp::putInstance() {
        int i = 0 ,j = 0;
        for (j = 0; j < ISP_PORT_NUM; j++) {
            for (i = 0; i < ROTATION_MAX;i++) {
                if (mInstance[j][i] != nullptr) {
                    delete mInstance[j][i];
                    mInstance[j][i] = nullptr;
                }
            }
        }
    }

    int DeWarp::dewarp_to_libgdc_format(int dewarp_format)
    {
        int ret = 0;
        switch (dewarp_format) {
        case YUV420_PLANAR:
            ret = YV12;
            break;
        case YUV420_SEMIPLANAR:
            ret = NV12;
            break;
        case YONLY:
            ret = Y_GREY;
            break;
        default:
            printf("format is wrong\n");
            break;
        }
        return ret;
    }

    void DeWarp::gdc_do_fisheye_correction() {
        ATRACE_CALL();
        do {
            //ALOGD("gdc process \n");
            set_input_buffer(mInput_fd);
            set_output_buffer(mOutput_fd);
            int ret = gdc_process(mGDCContext);
            if (ret < 0) {
                ALOGE("gdc ioctl failed\n");
                gdc_exit();
                break;
            }
        } while(0);
    }

    void DeWarp::gdc_exit() {
        ALOGD("%s: E \n",__FUNCTION__);
        ATRACE_CALL();
        if (mFw_fd != -1) {
           IONInterface* IONDevice = IONInterface::get_instance();
           IONDevice->free_buffer(mFw_fd);
           mFw_fd = -1;
        }

        gdc_destroy_ctx(mGDCContext);

    }

    void DeWarp::set_src_ROI(int x, int y, int w, int h) {
        dptz_CropX      = x;
        dptz_CropY      = y;
        dptz_CropWidth  = w;
        dptz_CropHeight = h;
    }

}
