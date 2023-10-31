/*
 * Copyright (c) 2021 Amazon.com, Inc. or its affiliates.  All rights reserved.
 *
 * PROPRIETARY/CONFIDENTIAL.  USE IS SUBJECT TO LICENSE TERMS.
*/


#define LOG_TAG "deWarp"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>
#include "dewarp.h"
#include "CamHalDebugLog.h"
#include "ispMgr/staticPipe.h"

namespace android {

    DeWarp* DeWarp::mInstance[ISP_PORT_NUM][ROTATION_MAX] = {{nullptr}};
    Mutex DeWarp::mMutex;
    int DeWarp::dptz_CropX, DeWarp::dptz_CropY, DeWarp::dptz_CropWidth, DeWarp::dptz_CropHeight;

    DeWarp::DeWarp(int groupId,Rotation rotation):
        mRotation(rotation),
        mGroupId(groupId)
    {
        CAMHAL_LOGD("%s: E \n",__FUNCTION__);
        //gdc init and alloc buffer for gdc
        memset(&mDewarp_params,0,sizeof(struct dewarp_params));
        if (mGDCContext == nullptr) {
            mGDCContext = (struct gdc_usr_ctx_s*)malloc(sizeof(struct gdc_usr_ctx_s));
            memset(mGDCContext, 0, sizeof(struct gdc_usr_ctx_s));
        }

        CameraConfig* config = CameraConfig::getInstance(mGroupId);
        mION = IONInterface::get_instance();
        gdc_init(config->mGDCParam.width,config->mGDCParam.height,YUV420_SEMIPLANAR,config->mGDCParam.planeNum);
    }

    DeWarp::~DeWarp() {
        gdc_exit();
        if (mGDCContext) {
            free(mGDCContext);
            mGDCContext = nullptr;
        }
        if (mION) {
            mION->put_instance();
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
        CAMHAL_LOGD("%s: E \n",__FUNCTION__);
        ATRACE_CALL();
        struct gdc_settings_ex *gdc_gs = &mGDCContext->gs_ex;
        uint8_t* fw_buffer = nullptr;
        int fw_max_len = 300 * 1024;

        //----alloc memory
        fw_buffer = mION->alloc_buffer(fw_max_len, &mFw_fd);
        if (fw_buffer == nullptr) {
            CAMHAL_LOGE("failed to allocate config buffer %d",fw_max_len);
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
        CAMHAL_LOGD("%s: E \n",__FUNCTION__);
        if (mGroupId <= DEWARP_CAM2PORT_RECORD) {
            // mipi sensor use dewarp
            int ret = 0;
            CameraConfig* config = CameraConfig::getInstance(mGroupId);
            char property[PROPERTY_VALUE_MAX];
            property_get("vendor.camhal.use.dewarp.linear", property, "true");
            if (strstr(property, "true")) {
                mProj_mode = PROJ_MODE_LINEAR;
            } else {
                mProj_mode = PROJ_MODE_EQUIDISTANCE;
            }
            GDCInParam in_params;
            in_params.width = width;
            in_params.height = height;
            in_params.rotation = mRotation;
            ret = staticPipe::fetchSensorGdcParameter(
                            &config->mSensorParam, in_params, &mDewarp_params);
            if (!ret) {
                return 0;
            } else {
                struct input_param* in   = &mDewarp_params.input_param;
                struct output_param* out = &mDewarp_params.output_param;
                struct proj_param *proj  = &mDewarp_params.proj_param[0];
                struct win_param *win    = &mDewarp_params.win_param[0];
                struct clb_param *clb    = &mDewarp_params.clb_param[0];

                mDewarp_params.proc_param.replace_0 = 0;
                mDewarp_params.proc_param.replace_1 = 128;
                mDewarp_params.proc_param.replace_2 = 128;

                mDewarp_params.proc_param.edge_0 = 0;
                mDewarp_params.proc_param.edge_1 = 128;
                mDewarp_params.proc_param.edge_2 = 128;

                char property[PROPERTY_VALUE_MAX];
                int width_tmp = in_params.width;
                int height_tmp = in_params.height;
                mDewarp_params.win_num = 1;
                if (config->getInputWidth() > 0 && config->getInputHeight() > 0) {
                    in->width = config->getInputWidth();
                    in->height = config->getInputHeight();
                } else {
                    in->width = width;
                    in->height = height;
                }
                in->offset_x = 0;
                in->offset_y = 0;
                in->fov = 120;

                mDewarp_params.color_mode = YUV420_SEMIPLANAR;
                /*ROTATION_90 ROTATION_270 output need exchange width and height,input no need*/
                out->width = (in_params.rotation == Rotation::ROTATION_0 || in_params.rotation == Rotation::ROTATION_180) ? width_tmp : height_tmp;
                out->height = (in_params.rotation == Rotation::ROTATION_0 || in_params.rotation == Rotation::ROTATION_180) ? height_tmp : width_tmp;

                property_get("vendor.camhal.use.dewarp.linear", property, "true");
                if (strstr(property, "true")) {
                    proj[0].projection_mode = PROJ_MODE_LINEAR;
                } else {
                    proj[0].projection_mode = PROJ_MODE_EQUIDISTANCE;
                }
                proj[0].pan = 0;
                proj[0].tilt = 0;
                proj[0].rotation = (int)in_params.rotation*90;
                proj[0].zoom = 1.01;
                proj[0].strength_hor = 1.0;
                proj[0].strength_ver = 1.0;

                win[0].win_start_x = 0;
                win[0].win_end_x = in_params.width - 1;
                win[0].win_start_y = 0;
                win[0].win_end_y = in_params.height - 1;
                win[0].img_start_x = 0;
                win[0].img_end_x = in_params.width - 1;
                win[0].img_start_y = 0;
                win[0].img_end_y = in_params.height - 1;
                win[0].mesh_x_len = 32;
                win[0].mesh_y_len = 32;

                if (in_params.width * in_params.height >= 4000 * 3000) {
                    clb[0].fx = 2005.21;
                    clb[0].fy = 2003.09;
                    clb[0].cx = 2111.56;
                    clb[0].cy = 1546.96;
                    clb[0].k1 = -0.390926;
                    clb[0].k2 = 0.485256;
                    clb[0].p1 = -5.5352e-5;
                    clb[0].p2 = -0.000159784;
                    clb[0].k3 = 0.273339;
                    clb[0].k4 = -0.317642;
                    clb[0].k5 = 0.25681;
                    clb[0].k6 = 0.433505;
                } else if (in_params.width * in_params.height >= 3840 * 2160) {
                    clb[0].fx = 1838.88;
                    clb[0].fy = 1837.62;
                    clb[0].cx = 1929.09;
                    clb[0].cy = 1071.01;
                    clb[0].k1 = 0.852712;
                    clb[0].k2 = -1.29004;
                    clb[0].p1 = 0.000272335;
                    clb[0].p2 = -0.000129222;
                    clb[0].k3 = 1.19002;
                    clb[0].k4 = 0.962421;
                    clb[0].k5 = -1.60834;
                    clb[0].k6 = 1.40821;
                } else if (in_params.width * in_params.height >= 1920 * 1080) {
                    clb[0].fx = 919.098;
                    clb[0].fy = 918.893;
                    clb[0].cx = 964.031;
                    clb[0].cy = 535.15;
                    clb[0].k1 = 0.851741;
                    clb[0].k2 = -1.15605;
                    clb[0].p1 = 0.000277852;
                    clb[0].p2 = -1.54521e-5;
                    clb[0].k3 = 1.11151;
                    clb[0].k4 = 0.962627;
                    clb[0].k5 = -1.48042;
                    clb[0].k6 = 1.33319;
                } else if (in_params.width * in_params.height >= 1440 * 1080) {
                    clb[0].fx = 695.433;
                    clb[0].fy = 694.781;
                    clb[0].cx = 722.275;
                    clb[0].cy = 535.666;
                    clb[0].k1 = -0.352378;
                    clb[0].k2 = 0.500009;
                    clb[0].p1 = -1.41251e-5;
                    clb[0].p2 = -0.00025844;
                    clb[0].k3 = 0.270799;
                    clb[0].k4 = -0.275094;
                    clb[0].k5 = 0.262535;
                    clb[0].k6 = 0.436324;
                } else if (in_params.width * in_params.height >= 1280 * 720) {
                    clb[0].fx = 611.879;
                    clb[0].fy = 611.12;
                    clb[0].cx = 643.006;
                    clb[0].cy = 357.604;
                    clb[0].k1 = 0.795054;
                    clb[0].k2 = -1.11909;
                    clb[0].p1 = 0.000304329;
                    clb[0].p2 = -0.000119665;
                    clb[0].k3 = 1.01965;
                    clb[0].k4 = 0.900531;
                    clb[0].k5 = -1.4214;
                    clb[0].k6 = 1.22587;
                } else {
                    clb[0].fx = 611.879;
                    clb[0].fy = 611.12;
                    clb[0].cx = 643.006;
                    clb[0].cy = 357.604;
                    clb[0].k1 = 0.795054;
                    clb[0].k2 = -1.11909;
                    clb[0].p1 = 0.000304329;
                    clb[0].p2 = -0.000119665;
                    clb[0].k3 = 1.01965;
                    clb[0].k4 = 0.900531;
                    clb[0].k5 = -1.4214;
                    clb[0].k6 = 1.22587;
                }

                mDewarp_params.tile_x_step = 16;
                mDewarp_params.tile_y_step = 16;
                mDewarp_params.prm_mode = 1;
                return 0;
            }

        } else {
            // usb sensor use dewarp
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

            CameraConfig* config = CameraConfig::getInstance(mGroupId);
            if (config->getInputWidth() > 0 && config->getInputHeight() > 0) {
                in->width = config->getInputWidth();
                in->height = config->getInputHeight();
            } else {
                in->width = width;
                in->height = height;
            }
            int mirror_value = 0;
            int origin_width = config->getCropInfo().originWidth;
            int origin_height = config->getCropInfo().originHeight;
            int crop_width = config->getCropInfo().width;
            int crop_height = config->getCropInfo().height;
            if (origin_width != 0 && origin_height != 0 && crop_width != 0 && crop_height != 0) {
                in->offset_x = (crop_width - origin_width) / 2 + 1;
                in->offset_y = (crop_height - origin_height) / 2 + 1;
            } else {
                in->offset_x = 0;
                in->offset_y = 0;
            }
            in->fov = 120;
            mDewarp_params.color_mode = YUV420_SEMIPLANAR;
            /*ROTATION_90 ROTATION_270 output need exchange width and height,input no need*/
            width = (mRotation == Rotation::ROTATION_0 || mRotation == Rotation::ROTATION_180) ? width_tmp : height_tmp;
            height = (mRotation == Rotation::ROTATION_0 || mRotation == Rotation::ROTATION_180) ? height_tmp : width_tmp;
            out->width = width;
            out->height = height;
            property_get("vendor.camhal.use.dewarp.linear", property, "true");
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
            if (height >= 4000)
                proj[0].zoom = 1.000;
            else if (height >= 1080)
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

            if (origin_width != 0 && origin_height != 0 && crop_width != 0 && crop_height != 0) {
                proj[0].strength_hor = (float) origin_width / (float) crop_width;
                proj[0].strength_ver = (float) origin_height / (float) crop_height;
            } else {
                proj[0].strength_hor = 1.0;
                proj[0].strength_ver = 1.0;
            }
            property_get("vendor.camhal.use.dewarp.mirror", property, "0");
            mirror_value = atoi(property);
            proj[0].mirror = mirror_value;

            win[0].win_start_x = 0;
            win[0].win_end_x = width - 1;
            win[0].win_start_y = 0;
            win[0].win_end_y = height - 1;
            win[0].img_start_x = 0;
            win[0].img_end_x = width - 1;
            win[0].img_start_y = 0;
            win[0].img_end_y = height - 1;
            win[0].mesh_x_len = 64;
            win[0].mesh_y_len = 64;


            mDewarp_params.tile_x_step = 32;
            mDewarp_params.tile_y_step = 32;
            mDewarp_params.prm_mode = 0;

            return 0;
        }
    }


    int DeWarp::gdc_init(size_t width, size_t height, int format , int plane_number) {
        CAMHAL_LOGD("%s: width = %d, height = %d, format=%d, plane_number=%dE \n",
                __FUNCTION__,  (int)width,  (int)height,format,plane_number);
        CameraConfig* config = CameraConfig::getInstance(mGroupId);
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
            o_y_stride = (config->mGDCParam.stride > 0) ? config->mGDCParam.stride : AXI_WORD_ALIGN(o_width);
            i_c_stride = AXI_WORD_ALIGN(i_width);
            o_c_stride = (config->mGDCParam.stride > 0) ? config->mGDCParam.stride : AXI_WORD_ALIGN(o_width);
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
            E_GDC("Error unknown format\n");
            return ret;
        }

        ret = gdc_create_ctx(mGDCContext);
        if (ret < 0) {
            CAMHAL_LOGE("failed to gdc_create_ctx");
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
        //CAMHAL_LOGE("%s-%d         i_width:%d i_height:%d,i_y_stride:%d i_c_stride:%d o_width:%d o_height:%d o_y_stride:%d o_c_stride:%d \n",__func__,__LINE__,\
        //                       i_width,  i_height,   i_y_stride,   i_c_stride,   o_width,   o_height,   o_y_stride,   o_c_stride);

        //-----load gdc config
        if (!load_config_file(width, height,plane_number)) {
            CAMHAL_LOGE("failed to load gdc config");
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

    void DeWarp::putInstance(int      groupId) {
        int i = 0;
        for (i = 0; i < ROTATION_MAX;i++) {
            if (mInstance[groupId][i] != nullptr) {
                delete mInstance[groupId][i];
                mInstance[groupId][i] = nullptr;
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
            //CAMHAL_LOGD("gdc process \n");
            set_input_buffer(mInput_fd);
            set_output_buffer(mOutput_fd);
            int ret = gdc_process(mGDCContext);
            if (ret < 0) {
                CAMHAL_LOGE("gdc ioctl failed\n");
                gdc_exit();
                break;
            }
        } while(0);
    }

    void DeWarp::gdc_exit() {
        CAMHAL_LOGD("%s: E \n",__FUNCTION__);
        ATRACE_CALL();
        if (mFw_fd != -1) {
           mION->free_buffer(mFw_fd);
           mFw_fd = -1;
        }
        auto gdc_gs = &mGDCContext->gs_ex;
        gdc_gs->config_buffer.shared_fd = -1;
        gdc_gs->config_buffer.u_base_fd = -1;
        gdc_gs->config_buffer.v_base_fd = -1;
        gdc_gs->input_buffer.shared_fd = -1;
        gdc_gs->input_buffer.u_base_fd = -1;
        gdc_gs->input_buffer.v_base_fd = -1;
        gdc_gs->output_buffer.shared_fd = -1;
        gdc_gs->output_buffer.u_base_fd = -1;
        gdc_gs->output_buffer.v_base_fd = -1;

        gdc_destroy_ctx(mGDCContext);

    }

    void DeWarp::set_src_ROI(int x, int y, int w, int h) {
        dptz_CropX      = x;
        dptz_CropY      = y;
        dptz_CropWidth  = w;
        dptz_CropHeight = h;
    }

}
