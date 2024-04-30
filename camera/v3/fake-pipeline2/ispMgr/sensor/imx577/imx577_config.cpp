/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "imx577Cfg"

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

#include "imx577_sdr_calibration.h"
#include "imx577_api.h"

typedef struct
{
    int  enWDRMode;
    ALG_SENSOR_DEFAULT_S snsAlgInfo;
    struct media_entity  * sensor_ent;
} ISP_SNS_STATE_S;

static ISP_SNS_STATE_S sensor;

void cmos_set_sensor_entity_imx577(struct media_entity * sensor_ent, int wdr, int fps)
{
    sensor.sensor_ent = sensor_ent;
}

void cmos_get_sensor_calibration_imx577(struct media_entity *sensor_ent, aisp_calib_info_t * calib)
{
    ALOGI("tnr global adj 128  custom 0719-1412\n");
    if (sensor.enWDRMode == 1) {
        ALOGE("imx577 don't have wdr mode");
        Imx577SdrCalibration::dynamic_sdr_calibrations_init_imx577(calib);
    } else
        Imx577SdrCalibration::dynamic_sdr_calibrations_init_imx577(calib);
}

int cmos_get_ae_default_imx577(int ViPipe, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ALOGD("cmos_get_ae_default, imx577, wdrmode %d\n", sensor.enWDRMode);

    sensor.snsAlgInfo.active.width = 4048;
    sensor.snsAlgInfo.active.height = 3040;
    sensor.snsAlgInfo.fps = 30;
    sensor.snsAlgInfo.sensor_exp_number = 1;
    sensor.snsAlgInfo.bits = 10;

    sensor.snsAlgInfo.sensor_gain_number = 1;
    sensor.snsAlgInfo.total.width = 8984;
    sensor.snsAlgInfo.total.height = 3116;

    sensor.snsAlgInfo.lines_per_second = sensor.snsAlgInfo.total.height*30;
    sensor.snsAlgInfo.pixels_per_line = sensor.snsAlgInfo.total.width;

    if (sensor.enWDRMode == 1) {
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height*2 - 24) <<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = 198<<SHUTTER_TIME_SHIFT;

        sensor.snsAlgInfo.integration_time_min = 1<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = 198<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = (sensor.snsAlgInfo.total.height*2 - 24) <<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = 198<<SHUTTER_TIME_SHIFT;
    } else {
        sensor.snsAlgInfo.integration_time_min = 8<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_max = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_long_max = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
        sensor.snsAlgInfo.integration_time_limit = sensor.snsAlgInfo.total.height<<SHUTTER_TIME_SHIFT;
    }

    sensor.snsAlgInfo.again_log2_max = 18432; // 2^4.5 = 22.63; 1024/(1024-978) = 22.26
    sensor.snsAlgInfo.again_high_log2_max = 18432; // 2^4.5 = 22.63; 1024/(1024-978) = 22.26
    sensor.snsAlgInfo.dgain_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_log2_max = 0;
    sensor.snsAlgInfo.dgain_high_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_high_accuracy = 1;
    sensor.snsAlgInfo.dgain_accuracy_fmt = 0;
    sensor.snsAlgInfo.dgain_accuracy = 1;
    sensor.snsAlgInfo.again_high_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_high_accuracy = (1<<(LOG2_GAIN_SHIFT))/20;
    sensor.snsAlgInfo.again_accuracy_fmt = 1;
    sensor.snsAlgInfo.again_log2 = 0x0<< LOG2_GAIN_SHIFT;
    sensor.snsAlgInfo.again_high_log2 = 0x0<< LOG2_GAIN_SHIFT;
    sensor.snsAlgInfo.expos_lines = (0xC16<<(LOG2_GAIN_SHIFT));
    sensor.snsAlgInfo.again_accuracy = (1<<(LOG2_GAIN_SHIFT))/512;
    sensor.snsAlgInfo.expos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.sexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));
    sensor.snsAlgInfo.vvsexpos_accuracy = (1<<(SHUTTER_TIME_SHIFT));

    sensor.snsAlgInfo.gain_apply_delay = 0;
    sensor.snsAlgInfo.integration_time_apply_delay = 0;
    ALOGD("cmos_get_ae_default++++++\n");

    memcpy(pstAeSnsDft, &sensor.snsAlgInfo, sizeof(ALG_SENSOR_DEFAULT_S));

    return 0;
}


static uint32_t aisp_math_exp2( int64_t val, int32_t shift_in, int32_t shift_out )
{
    uint32_t fract_part = (((uint32_t)val) & ( ( 1 << shift_in ) - 1 ) );
    uint32_t int_part = ((uint32_t)val) >> shift_in;
    uint32_t res, tmp;
    uint32_t pow_lut[33] = {
    1073741824, 1097253708, 1121280436, 1145833280, 1170923762, 1196563654, 1222764986, 1249540052,
    1276901417, 1304861917, 1333434672, 1362633090, 1392470869, 1422962010, 1454120821, 1485961921,
    1518500250, 1551751076, 1585730000, 1620452965, 1655936265, 1692196547, 1729250827, 1767116489,
    1805811301, 1845353420, 1885761398, 1927054196, 1969251188, 2012372174, 2056437387, 2101467502,
    2147483648};
    if ( shift_in <= 5 ) {
        uint32_t lut_index = fract_part << ( 5 - shift_in );
        res = pow_lut[lut_index] >> ( 30 - shift_out - int_part );
        return res;
    } else {
        uint32_t lut_index = fract_part >> ( shift_in - 5 );
        uint32_t lut_fract = fract_part & ( ( 1 << ( shift_in - 5 ) ) - 1 );
        uint32_t a = pow_lut[lut_index];
        uint32_t b = pow_lut[lut_index + 1];
        res = ( (uint64_t)( b - a ) * lut_fract ) >> ( shift_in - 5 );
        tmp =  ( 30 - shift_out - int_part ) - 1;
        res = ( res + a + (1<<tmp) ) >> ( 30 - shift_out - int_part );

        return ((int64_t)res);
    }
}

void cmos_again_calc_table_imx577(int ViPipe, uint32_t  *ae_sns_hc_again, uint32_t *ae_sns_again)
{
    CAMHAL_LOGD("cmos_again_calc_table: %u, %u\n",  *ae_sns_hc_again, *ae_sns_again);
    int again_reg;
    const int32_t  shift_out  = 8;
    float    again_float = 0.0f;

    again_reg = aisp_math_exp2( *ae_sns_again, LOG2_GAIN_SHIFT, shift_out );
    again_float = (float)again_reg/(float)(1<<shift_out);

    // again_times = 1024/(1024 - reg_val)
    // reg_val = 1024 - 1024/again_times
    again_reg = 1024 - 1024.0f/again_float;

    if (again_reg > 978)
        again_reg = 978;
    if (again_reg < 0)
        again_reg = 0;

    if (sensor.snsAlgInfo.u32AGain[0] != again_reg) {
        sensor.snsAlgInfo.u16GainCnt = sensor.snsAlgInfo.gain_apply_delay + 1;
        sensor.snsAlgInfo.u32AGain[0] = again_reg;
    }

}

void cmos_dgain_calc_table_imx577(int ViPipe, uint32_t *pu32DgainLin, uint32_t *pu32DgainDb)
{
    //ALOGD("cmos_dgain_calc_table: %d, %d\n", *pu32DgainLin, *pu32DgainDb);
}

void cmos_inttime_calc_table_imx577(int ViPipe, uint32_t pu32ExpL, uint32_t pu32ExpS, uint32_t pu32ExpVS, uint32_t pu32ExpVVS)
{
    ALOGD("cmos_inttime_calc_table: %d, %d, %d, %d\n", pu32ExpL, pu32ExpS, pu32ExpVS, pu32ExpVVS);
    uint32_t shutter_time_lines = pu32ExpL >> SHUTTER_TIME_SHIFT;
    uint32_t shutter_time_line_each_frame = sensor.snsAlgInfo.total.height;

    uint32_t shutter_time_lines_short = pu32ExpS >> SHUTTER_TIME_SHIFT;

    //ALOGD("expo: %d, %d\n", shutter_time_lines, shutter_time_lines_short);
    if (sensor.enWDRMode == 0) {
        if (shutter_time_lines > shutter_time_line_each_frame)
            shutter_time_lines = shutter_time_line_each_frame;

        if (shutter_time_lines)
            shutter_time_lines = shutter_time_lines - 1;
        if (shutter_time_lines < 8)
            shutter_time_lines = 8;
    } else {
        if (shutter_time_lines_short < 1)
            shutter_time_lines_short = 1;
        shutter_time_lines_short = 201 - shutter_time_lines_short - 1;
        shutter_time_lines = shutter_time_line_each_frame * 2  - shutter_time_lines - 1 - 26;
    }

    if (sensor.snsAlgInfo.u32Inttime[0][0] != shutter_time_lines || sensor.snsAlgInfo.u32Inttime[1][0] != shutter_time_lines_short) {
        sensor.snsAlgInfo.u16IntTimeCnt = sensor.snsAlgInfo.integration_time_apply_delay + 1;
        sensor.snsAlgInfo.u32Inttime[0][0] = shutter_time_lines;
        sensor.snsAlgInfo.u32Inttime[1][0] = shutter_time_lines_short;
    }
}

void cmos_fps_set_imx577(int ViPipe, float f32Fps, ALG_SENSOR_DEFAULT_S *pstAeSnsDft)
{
    ALOGD("cmos_fps_set: %f\n", f32Fps);
}

void cmos_alg_update_imx577(int ViPipe)
{
    uint32_t shutter_time_lines = 0;//, shutter_time_lines_short = 0;
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
                //shutter_time_lines_short = sensor.snsAlgInfo.u32Inttime[1][sensor.snsAlgInfo.integration_time_apply_delay];
                //imx577_write_register(ViPipe, 0x3020, shutter_time_lines_short & 0xff);
                //imx577_write_register(ViPipe, 0x3021, (shutter_time_lines_short>>8) & 0xff);
                //imx577_write_register(ViPipe, 0x3024, shutter_time_lines&0xff);
                //imx577_write_register(ViPipe, 0x3025, (shutter_time_lines>>8) & 0xff);
                //ALOGD("sensor expo: %d, %d\n", shutter_time_lines, shutter_time_lines_short);
            }
        }
    }

    for ( i = 3; i > 0; i --) {
        sensor.snsAlgInfo.u32AGain[i] = sensor.snsAlgInfo.u32AGain[i - 1];
        sensor.snsAlgInfo.u32Inttime[0][i] = sensor.snsAlgInfo.u32Inttime[0][i - 1];
        sensor.snsAlgInfo.u32Inttime[1][i] = sensor.snsAlgInfo.u32Inttime[1][i - 1];
    }
}

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)

static void print_gdc_parameter(struct dewarp_params *dewarp_params) {
    struct input_param* in = &dewarp_params->input_param;
    struct output_param* out = &dewarp_params->output_param;
    struct proj_param* proj = &dewarp_params->proj_param[0];
    struct win_param* win = &dewarp_params->win_param[0];
    struct meshin_param* mesh = &dewarp_params->meshin_param[0];
    CAMHAL_LOGD("dewarp_param (%d %d %d %d %d)",
        dewarp_params->win_num, dewarp_params->color_mode, dewarp_params->prm_mode,
        dewarp_params->tile_x_step, dewarp_params->tile_y_step);
    CAMHAL_LOGD("input_param (%d %d %d %d %d %f %d)",
        in->width, in->height, in->offset_x, in->offset_y, in->fov, in->radius, in->fisheye);
    CAMHAL_LOGD("output_param (%d %d)", out->width, out->height);
    for (int i = 0; i < dewarp_params->win_num; i++) {
        CAMHAL_LOGD("proj_param[%d] (%d %d %d %d   %f %f %f %d    %f %f %d %d %d %d)",
            i, proj[i].projection_mode, proj[i].pan , proj[i].tilt, proj[i].rotation,
            proj[i].zoom, proj[i].strength_hor, proj[i].strength_ver, proj[i].mirror,
            proj[i].shx, proj[i].shy, proj[i].pitch, proj[i].yaw, proj[i].roll, proj[i].fov);

        CAMHAL_LOGD("win_param[%d] (%d %d %d %d   %d %d %d %d   %d %d %d %d  %d)",
            i, win[i].win_start_x, win[i].win_end_x, win[i].win_start_y, win[i].win_end_y,
            win[i].img_start_x, win[i].img_end_x, win[i].img_start_y, win[i].img_end_y,
            win[i].mesh_x_len, win[i].mesh_y_len, win[i].crop_en, win[i].crop_x_start, win[0].crop_y_start);

        CAMHAL_LOGD("meshin_param[%d] (%d %d %d %d %d %d %p)",
            i, mesh[i].x_start, mesh[i].y_start, mesh[i].x_len, mesh[i].y_len, mesh[i].x_step,
            mesh[i].y_step, mesh[i].meshin_data_table);
    }
}

static void gen_gdc_parameter_default(
            struct sensorConfig *cfg, GDCInParam in_params,
            struct dewarp_params *dewarp_params)
{
    char mesh_path[PROPERTY_VALUE_MAX];
    char property[PROPERTY_VALUE_MAX];
    //todo remove to sensor files
    CAMHAL_LOGD("%s: E in:%ux%u, out %ux%u",
            __FUNCTION__, in_params.i_width, in_params.i_height, in_params.o_width, in_params.o_height);
    struct input_param* in = &dewarp_params->input_param;
    struct output_param* out = &dewarp_params->output_param;
    struct proj_param* proj = &dewarp_params->proj_param[0];
    struct win_param* win = &dewarp_params->win_param[0];
    struct meshin_param* mesh = &dewarp_params->meshin_param[0];

    dewarp_params->proc_param.intrp_mode = 0;
    dewarp_params->proc_param.replace_0 = 0;
    dewarp_params->proc_param.replace_1 = 128;
    dewarp_params->proc_param.replace_2 = 128;

    dewarp_params->proc_param.edge_0 = 0;
    dewarp_params->proc_param.edge_1 = 128;
    dewarp_params->proc_param.edge_2 = 128;
    dewarp_params->win_num = 1;

    property_get("vendor.dewarp.mode", property, "param");
    if (strstr(property, "mesh")) {
        dewarp_params->prm_mode = 2;
        CAMHAL_LOGD("dewarp work in mesh mode");
        property_get("vendor.dewarp.mesh.path", mesh_path, "/data/mesh.txt");
        if ( 0 != access(mesh_path, F_OK | R_OK)) {
            CAMHAL_LOGW("mesh file access fail %s, reset to param mode", mesh_path);
            dewarp_params->prm_mode = 0;
        }
    } else {
        dewarp_params->prm_mode = 0;
        CAMHAL_LOGD("dewarp work in param mode");
    }

    in->width = in_params.i_width;
    in->height = in_params.i_height;

    dewarp_params->color_mode = YUV420_SEMIPLANAR;
    /*ROTATION_90 ROTATION_270 output need exchange width and height,input no need*/
    out->width =  in_params.o_width;
    out->height =  in_params.o_height;

    property_get("vendor.camhal.use.dewarp.linear", property, "true");
    if (strstr(property, "true")) {
        proj[0].projection_mode = PROJ_MODE_LINEAR;
    } else {
        proj[0].projection_mode = PROJ_MODE_EQUIDISTANCE;
    }
    if (dewarp_params->prm_mode == 0) {
        if (strstr(property, "true")) {
            in->fov = 120;
            in->radius = 0;
            in->fisheye = 0;
        } else {
            in->offset_x = property_get_int32("vendor.dewarp.in.offset_x", 0);
            in->offset_y = property_get_int32("vendor.dewarp.in.offset_y", 0);
            in->fov = property_get_int32("vendor.dewarp.in.fov", 190);
            in->radius = property_get_int32("vendor.dewarp.in.radius", 1670);
            in->fisheye = property_get_int32("vendor.dewarp.in.fisheye", 1);
        }
    }

    if (dewarp_params->prm_mode == 0) {
        proj[0].rotation = (int)in_params.rotation*90;
        proj[0].pan = property_get_int32("vendor.dewarp.proj.pan", 0);
        proj[0].tilt = property_get_int32("vendor.dewarp.proj.tilt", 0);
        proj[0].mirror = property_get_int32("vendor.dewarp.proj.mirror", 0);

        proj[0].shx = property_get_int32("vendor.dewarp.proj.shx", 0);
        proj[0].shy = property_get_int32("vendor.dewarp.proj.shy", 0);
        proj[0].pitch = property_get_int32("vendor.dewarp.proj.pitch", 0);  //rotation axis x
        proj[0].yaw = property_get_int32("vendor.dewarp.proj.yaw", 0);      //rotation axis y
        proj[0].roll= property_get_int32("vendor.dewarp.proj.roll", 0);     //rotation axis z
        proj[0].fov = property_get_int32("vendor.dewarp.proj.fov", 100);

        property_get("vendor.dewarp.proj.zoom", property, "1.0");
        proj[0].zoom = atof(property);
        property_get("vendor.dewarp.proj.strength_hor", property, "1.0");
        proj[0].strength_hor = atof(property);
        property_get("vendor.dewarp.proj.strength_ver", property, "1.0");
        proj[0].strength_ver = atof(property);
    } else if (dewarp_params->prm_mode == 2) {
        mesh[0].x_start = property_get_int32("vendor.dewarp.mesh.xstart", -16);
        mesh[0].y_start = property_get_int32("vendor.dewarp.mesh.ystart", -9);
        mesh[0].x_len = property_get_int32("vendor.dewarp.mesh.xlen", 64);
        mesh[0].y_len = property_get_int32("vendor.dewarp.mesh.ylen", 64);
        mesh[0].x_step = property_get_int32("vendor.dewarp.mesh.xstep", 32);
        mesh[0].y_step = property_get_int32("vendor.dewarp.mesh.ystep", 18);
        size_t mesh_size = mesh[0].x_len * mesh[0].y_len * 2;
        static float *mesh_array = nullptr;
        float val;
        FILE *mesh_file = fopen(mesh_path, "r");
        if (mesh_file == nullptr) {
            CAMHAL_LOGE("fail to open mesh file");
            return ;
        }
        if (mesh_array)
            delete[] mesh_array;
        mesh_array = new float[mesh_size];
        for (int i = 0; i < mesh_size; ++i) {
            if (fscanf(mesh_file, "%f", &val) < 0)
                CAMHAL_LOGE("mesh data error %d", i);
            mesh_array[i] = val;
            //CAMHAL_LOGD("mesh data[%d:%f]", i, mesh_array[i]);
        }
        mesh[0].meshin_data_table = mesh_array;
        fclose(mesh_file);
    }

    win[0].win_start_x = 0;
    win[0].win_end_x = in_params.o_width - 1;
    win[0].win_start_y = 0;
    win[0].win_end_y = in_params.o_height - 1;
    win[0].img_start_x = 0;
    win[0].img_end_x = in_params.o_width - 1;
    win[0].img_start_y = 0;
    win[0].img_end_y = in_params.o_height - 1;
    win[0].mesh_x_len = 64;
    win[0].mesh_y_len = 64;

    dewarp_params->tile_x_step = 32;
    dewarp_params->tile_y_step = 32;
}

void cmos_get_sensor_gdc_parameter_imx577(struct sensorConfig *cfg, GDCInParam in_params,
                                              struct dewarp_params *dewarp_params)
{

    gen_gdc_parameter_default(cfg, in_params, dewarp_params);

    print_gdc_parameter(dewarp_params);

    return;
}

#endif

