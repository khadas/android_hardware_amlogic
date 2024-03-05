/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "imx415Cfg"

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

#include "imx415_sdr_calibration.h"
#include "imx415_wdr_calibration.h"

#include "imx415_api.h"

typedef struct
{
    int  enWDRMode = 0;
    ALG_SENSOR_DEFAULT_S snsAlgInfo;
    struct media_entity  * sensor_ent;
} ISP_SNS_STATE_S;

static ISP_SNS_STATE_S sensor;

void cmos_set_sensor_entity_imx415(struct media_entity * sensor_ent,int wdr, int fps)
{
    memset(&sensor.snsAlgInfo, 0, sizeof(ALG_SENSOR_DEFAULT_S));
    sensor.sensor_ent = sensor_ent;
    sensor.enWDRMode = wdr;
    //sensor.snsAlgInfo.fps = fps;
}

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
void cmos_get_sensor_gdc_parameter_imx415(struct sensorConfig *cfg, GDCInParam in_params,
                                              struct dewarp_params *dewarp_params)
{
    //todo remove to sensor files
    CAMHAL_LOGD("%s: E width %u height %u out width %u height %u",__FUNCTION__, in_params.i_width, in_params.i_height, in_params.o_width, in_params.o_height);
    struct input_param* in   = &dewarp_params->input_param;
    struct output_param* out = &dewarp_params->output_param;
    struct proj_param *proj  = &dewarp_params->proj_param[0];
    struct win_param *win    = &dewarp_params->win_param[0];

    dewarp_params->proc_param.replace_0 = 0;
    dewarp_params->proc_param.replace_1 = 128;
    dewarp_params->proc_param.replace_2 = 128;

    dewarp_params->proc_param.edge_0 = 0;
    dewarp_params->proc_param.edge_1 = 128;
    dewarp_params->proc_param.edge_2 = 128;
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
    if (property_get_bool("vendor.camhal.use.dewarp.linear", true)) {
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

    dewarp_params->tile_x_step = 16;
    dewarp_params->tile_y_step = 16;
    dewarp_params->prm_mode = 0;
}
#endif

void cmos_get_sensor_calibration_imx415(struct media_entity *sensor_ent, aisp_calib_info_t *calib)
{
    if (sensor.enWDRMode == 1) {
        Imx415WdrCalibration::dynamic_wdr_calibrations_init_imx415(calib);
        CAMHAL_LOGD("load wdr calibration");
    } else {
        Imx415SdrCalibration::dynamic_sdr_calibrations_init_imx415(calib);
    }
}

int cmos_get_ae_default_imx415(int ViPipe, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    CAMHAL_LOGD("cmos_get_ae_default\n");

    sensor.snsAlgInfo.active.width = 3840;
    sensor.snsAlgInfo.active.height = 2160;
    sensor.snsAlgInfo.fps = 30*256;
    sensor.snsAlgInfo.bits = 10;

    sensor.snsAlgInfo.sensor_gain_number = 1;

    if (sensor.enWDRMode == 1) {
        // WDR mode
        sensor.snsAlgInfo.sensor_exp_number = 2;

        sensor.snsAlgInfo.total.width = 0x0215;   // sync with reg HMAX
        sensor.snsAlgInfo.total.height = 0x08D0;  // sync with reg VMAX

        // min exposure lines for both long and short exposure;
        // long exposure lines > short exposure lines.
        // min short exposure lines. that is 9;
        sensor.snsAlgInfo.integration_time_min = 9<<SHUTTER_TIME_SHIFT;

        // max exposure lines; without drop fps. for short exposure
        // short exposure time = RHS1 - SHR1;
        // RHS1 is fixed to 0x111 (273). min value of SHR1 is 9;
        // max exposure lines for short exposure is 273 - 9
        sensor.snsAlgInfo.integration_time_max = (273-9)<<SHUTTER_TIME_SHIFT;

        // max exposure line for long exposure.
        // long exposure time = 2 * VMAX - SHR0;
        // SHR0 >= (RHS1 + 9)
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height*2 - (273+9)) <<SHUTTER_TIME_SHIFT;

        // set it to max exposure lines for short exposure.
        sensor.snsAlgInfo.integration_time_limit = (273-9)<<SHUTTER_TIME_SHIFT;
    } else {
        // sdr mode
        sensor.snsAlgInfo.sensor_exp_number = 1;

        sensor.snsAlgInfo.total.width = 0x042A;   // sync with HMAX
        sensor.snsAlgInfo.total.height = 0x08D0;  // sync with reg VMAX

        sensor.snsAlgInfo.integration_time_min = 1<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = (sensor.snsAlgInfo.total.height - 4)<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height - 4)<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = (sensor.snsAlgInfo.total.height - 4)<<SHUTTER_TIME_SHIFT;
    }

    sensor.snsAlgInfo.lines_per_second = (sensor.snsAlgInfo.total.height) * sensor.snsAlgInfo.fps / 256;
    sensor.snsAlgInfo.pixels_per_line = sensor.snsAlgInfo.total.width;

    sensor.snsAlgInfo.dgain_log2 = 0;
    sensor.snsAlgInfo.dgain_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_high_accuracy = 1;
    sensor.snsAlgInfo.dgain_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_accuracy = 1;
    sensor.snsAlgInfo.again_log2 = 0 << LOG2_GAIN_SHIFT;
    sensor.snsAlgInfo.again_log2_max = (48/6)<<(LOG2_GAIN_SHIFT);
    sensor.snsAlgInfo.again_high_log2_max = (48/6)<<(LOG2_GAIN_SHIFT);
    sensor.snsAlgInfo.again_high_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_high_accuracy = (1<<(LOG2_GAIN_SHIFT))/20;
    sensor.snsAlgInfo.again_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_accuracy = (1<<(LOG2_GAIN_SHIFT))/20;

    if (sensor.enWDRMode == 1) {
        // WDR mode

        // long exposure time. calc from reg initial value:SHR0 and VMAX
        // initial VMAX is 0x08D0, SHR0 is 0x105c
        // long exposure time = 2*vmax - SHR0
        sensor.snsAlgInfo.expos_lines = ((2 * sensor.snsAlgInfo.total.height - 0x105c)<<(SHUTTER_TIME_SHIFT));

        // short exposure time. calc from initial value of RSH1 - SHR1
        sensor.snsAlgInfo.sexpos_lines = (0x111 - 0x9)<< SHUTTER_TIME_SHIFT;

    } else {
        // sdr mode
        // long exposure time = vmax - SHR0; in SDR
        sensor.snsAlgInfo.expos_lines = (sensor.snsAlgInfo.total.height - 0x08) << SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.sexpos_lines = (1<<(SHUTTER_TIME_SHIFT));
    }

    sensor.snsAlgInfo.vsexpos_lines = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vvsexpos_lines = (1<<(SHUTTER_TIME_SHIFT));

    sensor.snsAlgInfo.expos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.sexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vvsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));

    sensor.snsAlgInfo.gain_apply_delay = 0;
    sensor.snsAlgInfo.integration_time_apply_delay = 0;
    CAMHAL_LOGD("cmos_get_ae_default++++++\n");

    memcpy(pstAeSnsDft, &sensor.snsAlgInfo, sizeof(ALG_SENSOR_DEFAULT_S));

    return 0;
}

void cmos_again_calc_table_imx415(int ViPipe, uint32_t *pu32AgainLin, uint32_t *pu32AgainDb)
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

void cmos_dgain_calc_table_imx415(int ViPipe, uint32_t *pu32DgainLin, uint32_t *pu32DgainDb)
{
    //CAMHAL_LOGD("cmos_dgain_calc_table: %d, %d\n", *pu32DgainLin, *pu32DgainDb);
}

void cmos_inttime_calc_table_imx415(int ViPipe, uint32_t pu32ExpL, uint32_t pu32ExpS, uint32_t pu32ExpVS, uint32_t pu32ExpVVS)
{
    //CAMHAL_LOGD("cmos_inttime_calc_table: %d, %d, %d, %d\n", pu32ExpL, pu32ExpS, pu32ExpVS, pu32ExpVVS);
    uint32_t shutter_time_lines = pu32ExpL >> SHUTTER_TIME_SHIFT;
    uint32_t shutter_time_line_each_frame = sensor.snsAlgInfo.total.height;

    uint32_t shutter_time_lines_short = pu32ExpS >> SHUTTER_TIME_SHIFT;

    //CAMHAL_LOGD("expo: %d, %d\n", shutter_time_lines, shutter_time_lines_short);
    if (sensor.enWDRMode == 0) {
        if (shutter_time_lines > shutter_time_line_each_frame)
            shutter_time_lines = shutter_time_line_each_frame;

        // sdr, SHR0 reg value = VMAX - integration time;
        shutter_time_lines = shutter_time_line_each_frame - shutter_time_lines;

        // now  shutter_time_lines is SHR0 reg value; SHR0 max value is VMAX-4
        if (shutter_time_lines > (shutter_time_line_each_frame - 4))
            shutter_time_lines = (shutter_time_line_each_frame - 4);

        if (shutter_time_lines < 8)
            shutter_time_lines = 8;

    } else {
        // short exposure lines = RHS1 - SHR1; RHS1 is fixed to 273.
        // SHR1 reg value = 273 - exposure lines;
        shutter_time_lines_short = 273 - shutter_time_lines_short;

        // now shutter_time_lines_short is reg value.
        if (shutter_time_lines_short < 9)
            shutter_time_lines_short = 9;

        if (shutter_time_lines_short > (273 - 8))
            shutter_time_lines_short = (273 - 8);

        // SHR0 reg value = 2*vmax - exposure lines
        shutter_time_lines = shutter_time_line_each_frame * 2  - shutter_time_lines;

        // now shutter_time_lines is reg value.
        if (shutter_time_lines > (shutter_time_line_each_frame * 2  - 8))
            shutter_time_lines = (shutter_time_line_each_frame * 2  - 8);

        if (shutter_time_lines <  (273 + 9))
            shutter_time_lines =  (273 + 9);

    }

    if (sensor.snsAlgInfo.u32Inttime[0][0] != shutter_time_lines || sensor.snsAlgInfo.u32Inttime[1][0] != shutter_time_lines_short) {
        sensor.snsAlgInfo.u16IntTimeCnt = sensor.snsAlgInfo.integration_time_apply_delay + 1;
        sensor.snsAlgInfo.u32Inttime[0][0] = shutter_time_lines;
        sensor.snsAlgInfo.u32Inttime[1][0] = shutter_time_lines_short;
    }
}

void cmos_fps_set_imx415(int ViPipe, float f32Fps, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    CAMHAL_LOGD("-imx415- f32Fps = %f, %d\n",f32Fps, (int32_t)(f32Fps / 256));

    struct v4l2_ext_control fpsCtrl;
    int clk_cnt;
    fpsCtrl.id = V4L2_CID_AML_ORIG_FPS;
    fpsCtrl.value = (int32_t)(f32Fps / 256);
    //CAMHAL_LOGD("-imx415- fpsCtrl.value = %d\n",fpsCtrl.value);

    clk_cnt = sensor.snsAlgInfo.fps * sensor.snsAlgInfo.total.height;
    sensor.snsAlgInfo.total.height = clk_cnt / f32Fps;

    sensor.snsAlgInfo.fps = f32Fps;

    // todo: update vmax?


    if (sensor.enWDRMode == 1) {
        sensor.snsAlgInfo.integration_time_min = 9<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = (273-9)<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height*2 - (273+9)) <<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = (273-9)<<SHUTTER_TIME_SHIFT;
    } else {

        sensor.snsAlgInfo.integration_time_min = 1<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
    }

    sensor.snsAlgInfo.lines_per_second = sensor.snsAlgInfo.total.height * fpsCtrl.value;
    memcpy(pstAeSnsDft, &sensor.snsAlgInfo, sizeof(ALG_SENSOR_DEFAULT_S));

    v4l2_subdev_set_ctrls(sensor.sensor_ent, &fpsCtrl, 1);
}

void cmos_alg_update_imx415(int ViPipe)
{
    uint32_t shutter_time_lines = 0, shutter_time_lines_short = 0;
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
                shutter_time_lines_short = sensor.snsAlgInfo.u32Inttime[1][sensor.snsAlgInfo.integration_time_apply_delay];
                struct v4l2_ext_control expo;
                expo.id = V4L2_CID_EXPOSURE;
                expo.value = (shutter_time_lines_short << 16) | shutter_time_lines;
                v4l2_subdev_set_ctrls(sensor.sensor_ent, &expo, 1);
                //CAMHAL_LOGD("0x%x - high part 0x%x (small SHR1 f-0), low part  0x%x(big SHR0 f-1);",
                //          expo.value, (expo.value >> 16)&0xffff, (expo.value&0xffff));
            }
        }
    }

    for ( i = 3; i > 0; i --) {
        sensor.snsAlgInfo.u32AGain[i] = sensor.snsAlgInfo.u32AGain[i - 1];
        sensor.snsAlgInfo.u32Inttime[0][i] = sensor.snsAlgInfo.u32Inttime[0][i - 1];
        sensor.snsAlgInfo.u32Inttime[1][i] = sensor.snsAlgInfo.u32Inttime[1][i - 1];
    }

}
