/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "imx335Cfg"

#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <semaphore.h>

#include "CamHalDebugLog.h"

#include "aml_isp_api.h"

#include "imx335_sdr_calibration.h"
#include "imx335_api.h"

typedef struct
{
    int  enWDRMode = 0;
    ALG_SENSOR_DEFAULT_S snsAlgInfo;
    struct media_entity  * sensor_ent;
} ISP_SNS_STATE_S;

static ISP_SNS_STATE_S sensor;

void cmos_set_sensor_entity_imx335(struct media_entity * sensor_ent, int wdr, int fps)
{
    memset(&sensor.snsAlgInfo, 0, sizeof(ALG_SENSOR_DEFAULT_S));
    sensor.sensor_ent = sensor_ent;
    sensor.enWDRMode = wdr;
    //sensor.snsAlgInfo.fps = fps;
}


#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
void cmos_get_sensor_gdc_parameter_imx335(struct sensorConfig *cfg, GDCInParam in_params,
                                              struct dewarp_params *dewarp_params)
{
    //todo remove to sensor files
    CAMHAL_LOGD("%s: E width %u height %u out width %u height %u",__FUNCTION__, in_params.i_width, in_params.i_height, in_params.o_width, in_params.o_height);
    struct input_param* in   = &dewarp_params->input_param;
    struct output_param* out = &dewarp_params->output_param;
    struct proj_param *proj  = &dewarp_params->proj_param[0];
    struct win_param *win    = &dewarp_params->win_param[0];
    struct clb_param *clb    = &dewarp_params->clb_param[0];

    dewarp_params->proc_param.replace_0 = 0;
    dewarp_params->proc_param.replace_1 = 128;
    dewarp_params->proc_param.replace_2 = 128;

    dewarp_params->proc_param.edge_0 = 0;
    dewarp_params->proc_param.edge_1 = 128;
    dewarp_params->proc_param.edge_2 = 128;

    char property[PROPERTY_VALUE_MAX];
    dewarp_params->win_num = 1;
    in->width = in_params.i_width;
    in->height = in_params.i_height;
    in->offset_x = 0;
    in->offset_y = 0;
    in->fov = 120;

    dewarp_params->color_mode = YUV420_SEMIPLANAR;
    /*ROTATION_90 ROTATION_270 output need exchange width and height,input no need*/
    out->width = in_params.o_width;
    out->height = in_params.o_height;
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
    win[0].win_end_x = in_params.o_width - 1;
    win[0].win_start_y = 0;
    win[0].win_end_y = in_params.o_height - 1;
    win[0].img_start_x = 0;
    win[0].img_end_x = in_params.o_width - 1;
    win[0].img_start_y = 0;
    win[0].img_end_y = in_params.o_height - 1;
    win[0].mesh_x_len = 32;
    win[0].mesh_y_len = 32;

    if (in_params.i_width * in_params.i_height >= 4000 * 3000) {
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
    } else if (in_params.i_width * in_params.i_height >= 3840 * 2160) {
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
    } else if (in_params.i_width * in_params.i_height >= 1920 * 1080) {
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
    } else if (in_params.i_width * in_params.i_height >= 1440 * 1080) {
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
    } else if (in_params.i_width * in_params.i_height >= 1280 * 720) {
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

    dewarp_params->tile_x_step = 16;
    dewarp_params->tile_y_step = 16;
    dewarp_params->prm_mode = 0;
}
#endif

void cmos_get_sensor_calibration_imx335(struct media_entity *sensor_ent, aisp_calib_info_t *calib)
{
    if (sensor.enWDRMode == 1)
        CAMHAL_LOGE("imx335 wdr not supported yet!\n");
    else
        Imx335SdrCalibration::dynamic_sdr_calibrations_init_imx335(calib);
}

void cmos_get_sensor_otp_data_imx335(aisp_calib_info_t * otp) {

}

int cmos_get_ae_default_imx335(int ViPipe, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    CAMHAL_LOGD("cmos_get_ae_default\n");

    sensor.snsAlgInfo.active.width = 2592;
    sensor.snsAlgInfo.active.height = 1944;
    sensor.snsAlgInfo.fps = 30*256;

    sensor.snsAlgInfo.sensor_gain_number = 1;

    if (sensor.enWDRMode == 1) {
        // wdr not supported yet!
        CAMHAL_LOGE("imx335 wdr not supported yet!\n");
        sensor.snsAlgInfo.sensor_exp_number = 2;
        sensor.snsAlgInfo.bits = 10;
        sensor.snsAlgInfo.total.width = 2028;
        sensor.snsAlgInfo.total.height = 1220;
        sensor.snsAlgInfo.lines_per_second = sensor.snsAlgInfo.total.height * sensor.snsAlgInfo.fps / 256;
        sensor.snsAlgInfo.pixels_per_line = sensor.snsAlgInfo.total.width;
        sensor.snsAlgInfo.integration_time_min = 1<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = (225 - 3) << SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height*2 - (225 + 3)) << SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = (225 - 3)<<SHUTTER_TIME_SHIFT;
    } else {
        sensor.snsAlgInfo.sensor_exp_number = 1;
        sensor.snsAlgInfo.bits = 12;
        sensor.snsAlgInfo.total.width = 550; // match hmax
        sensor.snsAlgInfo.total.height = 4500; // match vmax
        sensor.snsAlgInfo.lines_per_second = sensor.snsAlgInfo.total.height * sensor.snsAlgInfo.fps / 256;
        sensor.snsAlgInfo.pixels_per_line = sensor.snsAlgInfo.total.width;
        sensor.snsAlgInfo.integration_time_min = 1<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = (sensor.snsAlgInfo.total.height - 9)<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height - 9)<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = (sensor.snsAlgInfo.total.height - 9)<<SHUTTER_TIME_SHIFT;
    }

    sensor.snsAlgInfo.dgain_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_high_accuracy = 1;
    sensor.snsAlgInfo.dgain_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_accuracy = 1;

    sensor.snsAlgInfo.again_log2_max = (72/6)<<(LOG2_GAIN_SHIFT);
    sensor.snsAlgInfo.again_high_log2_max = (72/6)<<(LOG2_GAIN_SHIFT);

    // initial value of AGAIN reg is 0; 0dB;
    sensor.snsAlgInfo.again_log2 = 0 << LOG2_GAIN_SHIFT;
    sensor.snsAlgInfo.again_high_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_high_accuracy = (1<<(LOG2_GAIN_SHIFT))/20;
    sensor.snsAlgInfo.again_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_accuracy = (1<<(LOG2_GAIN_SHIFT))/20;
    if (sensor.enWDRMode == 1) {
        sensor.snsAlgInfo.expos_lines = (0x84b<<(SHUTTER_TIME_SHIFT));
        sensor.snsAlgInfo.expos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
        sensor.snsAlgInfo.sexpos_lines = (0x15<<(SHUTTER_TIME_SHIFT));
        sensor.snsAlgInfo.sexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    } else {
        // initial value of shr0 is 0x9
        sensor.snsAlgInfo.expos_lines = (sensor.snsAlgInfo.total.height - 9)<< SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.expos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
        sensor.snsAlgInfo.sexpos_lines = (1<<(SHUTTER_TIME_SHIFT));
        sensor.snsAlgInfo.sexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    }
    sensor.snsAlgInfo.vsexpos_lines = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vvsexpos_lines = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vvsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));

    sensor.snsAlgInfo.gain_apply_delay = 0;
    sensor.snsAlgInfo.integration_time_apply_delay = 0;
    CAMHAL_LOGD("cmos_get_ae_default++++++\n");

    memcpy(pstAeSnsDft, &sensor.snsAlgInfo, sizeof(ALG_SENSOR_DEFAULT_S));

    return 0;
}

void cmos_again_calc_table_imx335(int ViPipe, uint32_t *pu32AgainLin, uint32_t *pu32AgainDb)
{
    //CAMHAL_LOGD("cmos_again_calc_table: %d, %d\n", *pu32AgainLin, *pu32AgainDb);
    uint32_t again_reg;
    uint32_t u32AgainDb;

    u32AgainDb = *pu32AgainDb;
    u32AgainDb = ((u32AgainDb*20)>>LOG2_GAIN_SHIFT);

    again_reg = (uint32_t)(u32AgainDb);
    if (again_reg > 720/3) //72dB, 0.3dB step.
        again_reg = 720/3;

    if (sensor.snsAlgInfo.u32AGain[0] != again_reg) {
        sensor.snsAlgInfo.u16GainCnt = sensor.snsAlgInfo.gain_apply_delay + 1;
        sensor.snsAlgInfo.u32AGain[0] = again_reg;
    }
}

void cmos_dgain_calc_table_imx335(int ViPipe, uint32_t *pu32DgainLin, uint32_t *pu32DgainDb)
{
    //CAMHAL_LOGD("cmos_dgain_calc_table: %d, %d\n", *pu32DgainLin, *pu32DgainDb);
}

void cmos_inttime_calc_table_imx335(int ViPipe, uint32_t pu32ExpL, uint32_t pu32ExpS, uint32_t pu32ExpVS, uint32_t pu32ExpVVS)
{
    //CAMHAL_LOGD("cmos_inttime_calc_table: %d, %d, %d, %d\n", pu32ExpL, pu32ExpS, pu32ExpVS, pu32ExpVVS);
    uint32_t shutter_time_lines = pu32ExpL >> SHUTTER_TIME_SHIFT;
    uint32_t shutter_time_line_each_frame = sensor.snsAlgInfo.total.height;

    uint32_t shutter_time_lines_short = pu32ExpS >> SHUTTER_TIME_SHIFT;

    //CAMHAL_LOGD("expo: %d, %d\n", shutter_time_lines, shutter_time_lines_short);
    if (sensor.enWDRMode == 0) {
        // sdr
        if (shutter_time_lines > shutter_time_line_each_frame)
            shutter_time_lines = shutter_time_line_each_frame;
        shutter_time_lines = shutter_time_line_each_frame - shutter_time_lines;
        // now shutter_time_lines is reg value.

        if (shutter_time_lines < 9)
            shutter_time_lines = 9;

        if (shutter_time_lines > (shutter_time_line_each_frame - 1))
            shutter_time_lines = (shutter_time_line_each_frame - 1);

    } else {
        if (shutter_time_lines_short < 1)
            shutter_time_lines_short = 1;
        shutter_time_lines_short = 225 - shutter_time_lines_short - 1;
        shutter_time_lines = shutter_time_line_each_frame * 2  - shutter_time_lines - 1;
    }

    if (sensor.snsAlgInfo.u32Inttime[0][0] != shutter_time_lines || sensor.snsAlgInfo.u32Inttime[1][0] != shutter_time_lines_short) {
        sensor.snsAlgInfo.u16IntTimeCnt = sensor.snsAlgInfo.integration_time_apply_delay + 1;
        sensor.snsAlgInfo.u32Inttime[0][0] = shutter_time_lines;
        sensor.snsAlgInfo.u32Inttime[1][0] = shutter_time_lines_short;
    }
}

void cmos_fps_set_imx335(int ViPipe, float f32Fps, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    //CAMHAL_LOGD("-imx335- f32Fps = %f, %d\n",f32Fps, (int32_t)(f32Fps / 256));

    struct v4l2_ext_control fpsCtrl;

    fpsCtrl.id = V4L2_CID_AML_ORIG_FPS;
    fpsCtrl.value = (int32_t)(f32Fps / 256);
    //CAMHAL_LOGD("-imx335- fpsCtrl.value = %d\n",fpsCtrl.value);

    sensor.snsAlgInfo.total.height = ( 3488 * 30 )/fpsCtrl.value;
    sensor.snsAlgInfo.fps = fpsCtrl.value*256;

    sensor.snsAlgInfo.integration_time_max = sensor.snsAlgInfo.total.height << SHUTTER_TIME_SHIFT;
    sensor.snsAlgInfo.integration_time_long_max = sensor.snsAlgInfo.total.height << SHUTTER_TIME_SHIFT;
    sensor.snsAlgInfo.integration_time_limit = sensor.snsAlgInfo.total.height << SHUTTER_TIME_SHIFT;
    sensor.snsAlgInfo.lines_per_second = sensor.snsAlgInfo.total.height * fpsCtrl.value;
    memcpy(pstAeSnsDft, &sensor.snsAlgInfo, sizeof(ALG_SENSOR_DEFAULT_S));

    v4l2_subdev_set_ctrls(sensor.sensor_ent, &fpsCtrl, 1);
}

void cmos_alg_update_imx335(int ViPipe)
{
    uint32_t shutter_time_lines = 0; //, shutter_time_lines_short = 0;
    uint32_t i = 0;

    if ( sensor.snsAlgInfo.u16GainCnt || sensor.snsAlgInfo.u16IntTimeCnt ) {
        if ( sensor.snsAlgInfo.u16GainCnt ) {
            sensor.snsAlgInfo.u16GainCnt--;
            struct v4l2_ext_control gain;
            gain.id = V4L2_CID_GAIN;
            gain.value = sensor.snsAlgInfo.u32AGain[sensor.snsAlgInfo.gain_apply_delay];
            v4l2_subdev_set_ctrls(sensor.sensor_ent, &gain, 1);
        }

        // -------- Integration Time ----------
        if ( sensor.snsAlgInfo.u16IntTimeCnt ) {
            sensor.snsAlgInfo.u16IntTimeCnt--;
            shutter_time_lines = sensor.snsAlgInfo.u32Inttime[0][sensor.snsAlgInfo.integration_time_apply_delay];
            if (sensor.enWDRMode == 0) {
                struct v4l2_ext_control expo;
                expo.id = V4L2_CID_EXPOSURE;
                expo.value = shutter_time_lines;
                v4l2_subdev_set_ctrls(sensor.sensor_ent, &expo, 1);
            }

            if (sensor.enWDRMode) {
                // not support yet.
            }
        }
    }

    for ( i = 3; i > 0; i --) {
        sensor.snsAlgInfo.u32AGain[i] = sensor.snsAlgInfo.u32AGain[i - 1];
        sensor.snsAlgInfo.u32Inttime[0][i] = sensor.snsAlgInfo.u32Inttime[0][i - 1];
        sensor.snsAlgInfo.u32Inttime[1][i] = sensor.snsAlgInfo.u32Inttime[1][i - 1];
    }
}

