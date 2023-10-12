/*
 * Copyright (c) 2018 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */
#define LOG_TAG "sensorConfig"

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
#include <dlfcn.h>
#include <errno.h>
#include <cutils/properties.h>

#include "CamHalDebugLog.h"

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

#include "sensor_config.h"
#include "sensor_otp.h"

#include "imx290/imx290_api.h"
#include "imx415/imx415_api.h"
#include "ov13b10/ov13b10_api.h"
#include "ov08a10/ov08a10_api.h"
#include "ov13855/ov13855_api.h"
#include "imx378/imx378_api.h"
#include "imx577/imx577_api.h"
#include "ov16a1q/ov16a1q_api.h"

#define ARRAY_SIZE(array)   (sizeof(array) / sizeof((array)[0]))

struct sensorConfig imx290Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx290,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx290,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx290,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx290,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx290,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx290,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx290,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx290,
    .cmos_get_sensor_otp_data = cmos_get_sensor_otp_data_imx290,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_imx290,
#endif
    .sensorWidth      = 1920,
    .sensorHeight     = 1080,
    .sensorName       = "imx290",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .sdrFormat60HZ    = MEDIA_BUS_FMT_SGBRG10_1X10,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};

struct sensorConfig imx415Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx415,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx415,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx415,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx415,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx415,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx415,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx415,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx415,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_imx415,
#endif
    .sensorWidth      = 3840,
    .sensorHeight     = 2160,
    .sensorName       = "imx415",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB12_1X12,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};

struct sensorConfig ov13b10Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_ov13b10,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_ov13b10,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_ov13b10,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_ov13b10,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_ov13b10,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_ov13b10,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_ov13b10,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_ov13b10,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_ov13b10,
#endif
    .sensorWidth      = 4208,
    .sensorHeight     = 3120,
    .sensorName       = "ov13b10",
    .wdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .type             = sensor_raw,
    .otpDevNum        = "/dev/i2c-2",
    .otpDevAddr       = 0x50,
    .otpDevAddrType   = 2,
    .otpDevLscAddr    = 0x0009,
    .otpDevWbAddr     = 0x191b,
};

struct sensorConfig ov16a1qCfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_ov16a1q,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_ov16a1q,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_ov16a1q,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_ov16a1q,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_ov16a1q,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_ov16a1q,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_ov16a1q,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_ov16a1q,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_ov16a1q,
#endif
    .sensorWidth      = 2304,
    .sensorHeight     = 1748,
    .sensorName       = "ov16a1q",
    .wdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat60HZ        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .type             = sensor_raw,
};


struct sensorConfig ov08a10Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_ov08a10,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_ov08a10,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_ov08a10,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_ov08a10,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_ov08a10,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_ov08a10,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_ov08a10,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_ov08a10,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_ov08a10,
#endif
    .sensorWidth      = 3840,
    .sensorHeight     = 2160,
    .sensorName       = "ov08a10",
    .wdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};

struct sensorConfig ov13855Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_ov13855,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_ov13855,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_ov13855,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_ov13855,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_ov13855,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_ov13855,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_ov13855,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_ov13855,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_ov13855,
#endif
    .sensorWidth      = 4224,
    .sensorHeight     = 3136,
    .sensorName       = "ov13855",
    .wdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};

struct sensorConfig ov5640Cfg = {
    .sensorWidth      = 2592,
    .sensorHeight     = 1944,
    .sensorName       = "ov5640",
    .wdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .sdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .type             = sensor_yuv,
    .otpDevAddr       = 0x00,
};

struct sensorConfig lt6911cCfg = {
    .sensorWidth      = 1920,
    .sensorHeight     = 1080,
    .sensorName       = "lt6911c",
    .wdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .sdrFormat        = MEDIA_BUS_FMT_YUYV8_2X8,
    .type             = sensor_yuv,
    .otpDevAddr       = 0x00,
};

struct sensorConfig imx378Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx378,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx378,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx378,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx378,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx378,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx378,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx378,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx378,
    .cmos_get_sensor_otp_data = cmos_get_sensor_otp_data_imx378,
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    .cmos_get_sensor_gdc_parameter = cmos_get_sensor_gdc_parameter_imx378,
#endif
    .sensorWidth      = 3840,
    .sensorHeight     = 2160,
    .sensorName       = "imx378",
    .wdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SRGGB10_1X10,
    .sdrFormat60HZ    = MEDIA_BUS_FMT_SGBRG10_1X10,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};


struct sensorConfig imx577Cfg = {
    .expFunc.pfn_cmos_fps_set = cmos_fps_set_imx577,
    .expFunc.pfn_cmos_get_alg_default = cmos_get_ae_default_imx577,
    .expFunc.pfn_cmos_alg_update = cmos_alg_update_imx577,
    .expFunc.pfn_cmos_again_calc_table = cmos_again_calc_table_imx577,
    .expFunc.pfn_cmos_dgain_calc_table = cmos_dgain_calc_table_imx577,
    .expFunc.pfn_cmos_inttime_calc_table = cmos_inttime_calc_table_imx577,
    .cmos_set_sensor_entity = cmos_set_sensor_entity_imx577,
    .cmos_get_sensor_calibration = cmos_get_sensor_calibration_imx577,

    .sensorWidth      = 4048,
    .sensorHeight     = 3040,
    .sensorName       = "imx577",
    .wdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat        = MEDIA_BUS_FMT_SBGGR10_1X10,
    .sdrFormat60HZ    = MEDIA_BUS_FMT_SBGGR10_1X10,
    .type             = sensor_raw,
    .otpDevAddr       = 0x00,
};

struct sensorConfig *supportedCfgs[] = {
    &imx290Cfg,
    &imx415Cfg,
    &ov13b10Cfg,
    &ov16a1qCfg,
    &ov08a10Cfg,
    &ov5640Cfg,
    &ov13855Cfg,
    &lt6911cCfg,
    &imx378Cfg,
    &imx577Cfg,
};

static int log2file(const char* name, const char* fmt, ...)
{
    va_list ap;
    char buf[1024];

    va_start(ap, fmt);
    int ret = vsnprintf(buf, 1024, fmt, ap);
    va_end(ap);
    buf[ret] = '\0';

    auto fp = fopen(name, "ab+");
    if (!fp) {
        CAMHAL_LOGE("open file %s fail, error: %s !!!", name, strerror(errno));
        return -1;
    }
    fwrite(buf, 1 , ret, fp);
    fclose(fp);
    return 0;
}

LookupTable *GET_LOOKUP_PTR( aisp_calib_info_t *p_cali, uint32_t idx )
{
    LookupTable *result = NULL;
    if ( idx < CALIBRATION_TOTAL_SIZE ) {
        result = p_cali->calibrations[idx];
    } else {
        result = NULL;
        CAMHAL_LOGE("no find current lut\n");
    }
    return result;
}

uint32_t _GET_SIZE( aisp_calib_info_t *p_cali, uint32_t idx )
{
    uint32_t result = 0;
    LookupTable *lut = GET_LOOKUP_PTR( p_cali, idx );
    if ( lut != NULL ) {
        result = lut->cols * lut->rows * lut->width;
    }
    return result;
}

const void *_GET_LUT_PTR( aisp_calib_info_t *p_ctx, uint32_t idx )
{
    const void *result = NULL;
    LookupTable *lut = GET_LOOKUP_PTR( p_ctx, idx );
    if ( lut != NULL ) {
        result = lut->ptr;
    }

    return result;
}

struct sensorConfig *matchSensorConfig(media_stream_t *stream) {
    for (int i = 0; i < ARRAY_SIZE(supportedCfgs); i++) {
        if (strstr(stream->sensor_ent_name, supportedCfgs[i]->sensorName)) {
            return supportedCfgs[i];
        }
    }
    CAMHAL_LOGE("fail to match sensorConfig");
    return nullptr;
}

struct sensorConfig *matchSensorConfig(const char* sensorEntityName) {
    for (int i = 0; i < ARRAY_SIZE(supportedCfgs); i++) {
        if (strstr(sensorEntityName, supportedCfgs[i]->sensorName)) {
            return supportedCfgs[i];
        }
    }
    CAMHAL_LOGE("fail to match sensorConfig %s", sensorEntityName);
    return nullptr;
}

void cmos_sensor_control_cb(struct sensorConfig *cfg, ALG_SENSOR_EXP_FUNC_S *stSnsExp)
{
    stSnsExp->pfn_cmos_alg_update = cfg->expFunc.pfn_cmos_alg_update;
    stSnsExp->pfn_cmos_get_alg_default = cfg->expFunc.pfn_cmos_get_alg_default;
    stSnsExp->pfn_cmos_again_calc_table = cfg->expFunc.pfn_cmos_again_calc_table;
    stSnsExp->pfn_cmos_dgain_calc_table = cfg->expFunc.pfn_cmos_dgain_calc_table;
    stSnsExp->pfn_cmos_inttime_calc_table = cfg->expFunc.pfn_cmos_inttime_calc_table;
    stSnsExp->pfn_cmos_fps_set = cfg->expFunc.pfn_cmos_fps_set;
}

void cmos_set_sensor_entity(struct sensorConfig *cfg, struct media_entity *sensor_ent, int wdr, int fps)
{
    if (cfg->cmos_set_sensor_entity)
        (cfg->cmos_set_sensor_entity)(sensor_ent, wdr, fps);
}

void cmos_get_sensor_calibration(struct sensorConfig *cfg, struct media_entity * sensor_ent, aisp_calib_info_t *calib)
{
    if (cfg->cmos_get_sensor_calibration)
        (cfg->cmos_get_sensor_calibration)(sensor_ent, calib);
}

void cmos_get_sensor_otp_data(struct sensorConfig *cfg, aisp_calib_info_t *otp)
{
//  (cfg->cmos_get_sensor_otp_data)(otp);
    #define LSC_DATA_MARGIN 1536
    #define RADIAL_DATA_MARGIN 255
    typedef int (*fn_aml_mesh_shading_decompress)(int X_node, int Y_node, int *pLSC, unsigned char *pLSC_enc, int size);
    typedef int (*fn_aml_rad_shading_decompress)(int node, int *pLSC, unsigned char *pLSC_enc, int size);

    void *lib = NULL;
    uint8_t flag = 0;
    char path[128];
    sprintf(path, "/data/%s-otp.data", cfg->sensorName);

    if (cfg->otpDevAddr == 0)
        return;
    lib = ::dlopen("libispaml.so", RTLD_NOW);

    if (!lib) {
        char const* err_str = ::dlerror();
        CAMHAL_LOGE("dlopen: error:%s", (err_str ? err_str : "unknown"));
        return;
    }
    auto decompress = (fn_aml_mesh_shading_decompress)::dlsym(lib, "aml_mesh_shading_decompress");
    if (!decompress) {
        char const* err_str = ::dlerror();
        CAMHAL_LOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        dlclose(lib);
        return;
    }
    auto rad_decompress = (fn_aml_rad_shading_decompress)::dlsym(lib, "aml_rad_shading_decompress");
    if (!rad_decompress) {
        char const* err_str = ::dlerror();
        CAMHAL_LOGE("dlsym: error:%s", (err_str ? err_str : "unknown"));
        dlclose(lib);
        return;
    }

    if (i2c_init(cfg->otpDevNum, cfg->otpDevAddr) < 0) {
        CAMHAL_LOGE("i2c init fail");
        return;
    }
    {
        log2file(path, "otp info MIF Flag 0x%x, vendor id 0x%x, module id 0x%x\n",
            i2c_read(0, cfg->otpDevAddrType),
            i2c_read(1, cfg->otpDevAddrType),
            i2c_read(2, cfg->otpDevAddrType));
        log2file(path, "otp info year 0x%x, month 0x%x, day 0x%x, lens id 0x%x, vcm id 0x%x\n",
            i2c_read(3, cfg->otpDevAddrType),
            i2c_read(4, cfg->otpDevAddrType),
            i2c_read(5, cfg->otpDevAddrType),
            i2c_read(6, cfg->otpDevAddrType),
            i2c_read(7, cfg->otpDevAddrType));
    }
    int X_node = 32;//Mesh shading correction horizonal calibration node nums
    int Y_node = 32;//Mesh shading correction Vertical calibration node nums
    int *pLSC_dec = new int[3 * X_node * Y_node];
    int *pLSC_enc = new int[3 * X_node * Y_node];
    unsigned char *buffer = (unsigned char *)pLSC_enc;
    int addr_offset = cfg->otpDevLscAddr;
    for (int i = 0; i < 1; ++i, ++addr_offset) {
        flag = i2c_read(addr_offset, cfg->otpDevAddrType);
        log2file(path, "otp shading valid, addr:0x%x value:0x%x\n", addr_offset, flag);
    }
    do { //valid
        if (flag != 0x01) {
            log2file(path, "Invalid LSC Data\n");
            break;
        }
        // CENTER_OFFSET
        {
            for (int i = 0; i < 4; ++i, ++addr_offset) {
                buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                log2file(path, "otp center offset data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
            }
            static int32_t _CALIBRATION_LENS_OTP_CENTER_OFFSET[2];
            _CALIBRATION_LENS_OTP_CENTER_OFFSET[0] = (int16_t)(buffer[0] << 8 | buffer[1]);
            _CALIBRATION_LENS_OTP_CENTER_OFFSET[1] = (int16_t)(buffer[2] << 8 | buffer[3]);
            static LookupTable calibration_lens_otp_center_offset = {
                .ptr = _CALIBRATION_LENS_OTP_CENTER_OFFSET,
                .rows = 1,
                .cols = sizeof(_CALIBRATION_LENS_OTP_CENTER_OFFSET) / sizeof(_CALIBRATION_LENS_OTP_CENTER_OFFSET[0]),
                .width = sizeof(_CALIBRATION_LENS_OTP_CENTER_OFFSET[0] )
            };
            otp->calibrations[CALIBRATION_LENS_OTP_CENTER_OFFSET] = &calibration_lens_otp_center_offset;
            log2file(path, "otp center offset-x %d, offset-y %d\n",
                _CALIBRATION_LENS_OTP_CENTER_OFFSET[0],
                _CALIBRATION_LENS_OTP_CENTER_OFFSET[1]);
            CAMHAL_LOGD("otp center offset-x %d, offset-y %d\n",
                _CALIBRATION_LENS_OTP_CENTER_OFFSET[0],
                _CALIBRATION_LENS_OTP_CENTER_OFFSET[1]);
        }
        //SHADING_RADIAL
        {
            int data_offset = 0;
            int lens = i2c_read(addr_offset, cfg->otpDevAddrType);
            log2file(path, "SHADING_RADIAL lens 0x%x \n", lens);
            if (lens > RADIAL_DATA_MARGIN) {
                log2file(path, "invalid lens");
                break;
            }
            addr_offset += 1;
            for (int i = 0; i < RADIAL_DATA_MARGIN; ++i, ++addr_offset) {
                if (i < lens) {
                    buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                    log2file(path, "otp lsc data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
                }
            }
            (rad_decompress)(129, pLSC_dec, buffer, 3 * 129 * sizeof(int));
            static uint16_t _CALIBRATION_SHADING_RADIAL_R[129];
            static uint16_t _CALIBRATION_SHADING_RADIAL_G[129];
            static uint16_t _CALIBRATION_SHADING_RADIAL_B[129];
            for (int i = 0; i < 129; ++i, ++data_offset) {
                _CALIBRATION_SHADING_RADIAL_R[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 129; ++i, ++data_offset) {
                _CALIBRATION_SHADING_RADIAL_G[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 129; ++i, ++data_offset) {
                _CALIBRATION_SHADING_RADIAL_B[i] = pLSC_dec[data_offset];
            }
            static LookupTable calibration_shading_radial_r = {
                .ptr = _CALIBRATION_SHADING_RADIAL_R,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_RADIAL_R ) / sizeof( _CALIBRATION_SHADING_RADIAL_R[0] ),
                .width = sizeof( _CALIBRATION_SHADING_RADIAL_R[0] )
            };
            static LookupTable calibration_shading_radial_g = {
                .ptr = _CALIBRATION_SHADING_RADIAL_G,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_RADIAL_G ) / sizeof( _CALIBRATION_SHADING_RADIAL_G[0] ),
                .width = sizeof( _CALIBRATION_SHADING_RADIAL_G[0] )
            };
            static LookupTable calibration_shading_radial_b = {
                .ptr = _CALIBRATION_SHADING_RADIAL_B,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_RADIAL_B ) / sizeof( _CALIBRATION_SHADING_RADIAL_B[0] ),
                .width = sizeof( _CALIBRATION_SHADING_RADIAL_B[0] )
            };
            otp->calibrations[CALIBRATION_SHADING_RADIAL_R] = &calibration_shading_radial_r;
            otp->calibrations[CALIBRATION_SHADING_RADIAL_G] = &calibration_shading_radial_g;
            otp->calibrations[CALIBRATION_SHADING_RADIAL_B] = &calibration_shading_radial_b;
        }
        //SHADING_CTL
        {
            for (int i = 0; i < 4; ++i, ++addr_offset) {
                buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                log2file(path, "otp shading ctl data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
            }
            static uint32_t _CALIBRATION_LENS_SHADING_CTL[4];
            _CALIBRATION_LENS_SHADING_CTL[0] = buffer[0];
            _CALIBRATION_LENS_SHADING_CTL[1] = buffer[1];
            _CALIBRATION_LENS_SHADING_CTL[2] = buffer[2];
            _CALIBRATION_LENS_SHADING_CTL[3] = buffer[3];
            static LookupTable calibration_lens_shading_ctl = {
                .ptr = _CALIBRATION_LENS_SHADING_CTL,
                .rows = 1,
                .cols = sizeof(_CALIBRATION_LENS_SHADING_CTL) / sizeof(_CALIBRATION_LENS_SHADING_CTL[0]),
                .width = sizeof(_CALIBRATION_LENS_SHADING_CTL[0] )
            };
            otp->calibrations[CALIBRATION_LENS_SHADING_CTL] = &calibration_lens_shading_ctl;
        }
        //SHADING_LS_A
        {
            int data_offset = 0;
            int lens = (i2c_read(addr_offset, cfg->otpDevAddrType) << 8) | i2c_read(addr_offset + 1, cfg->otpDevAddrType);
            log2file(path, "SHADING_LS_A lens 0x%x", lens);
            if (lens > LSC_DATA_MARGIN) {
                log2file(path, "invalid lens");
                break;
            }
            addr_offset += 2;
            for (int i = 0; i < LSC_DATA_MARGIN; ++i, ++addr_offset) {
                if (i < lens) {
                    buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                    log2file(path, "otp lsc data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
                }
            }
            (decompress)(X_node, Y_node, pLSC_dec, buffer, 3 * X_node * Y_node * sizeof(int));
            static uint8_t _CALIBRATION_SHADING_LS_A_R[1024];
            static uint8_t _CALIBRATION_SHADING_LS_A_G[1024];
            static uint8_t _CALIBRATION_SHADING_LS_A_B[1024];
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_A_R[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_A_G[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_A_B[i] = pLSC_dec[data_offset];
            }
            static LookupTable calibration_shading_ls_a_r = {
                .ptr = _CALIBRATION_SHADING_LS_A_R,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_A_R ) / sizeof( _CALIBRATION_SHADING_LS_A_R[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_A_R[0] )
            };
            static LookupTable calibration_shading_ls_a_g = {
                .ptr = _CALIBRATION_SHADING_LS_A_G,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_A_G ) / sizeof( _CALIBRATION_SHADING_LS_A_G[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_A_G[0] )
            };
            static LookupTable calibration_shading_ls_a_b = {
                .ptr = _CALIBRATION_SHADING_LS_A_B,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_A_B ) / sizeof( _CALIBRATION_SHADING_LS_A_B[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_A_B[0] )
            };
            otp->calibrations[CALIBRATION_SHADING_LS_A_R] = &calibration_shading_ls_a_r;
            otp->calibrations[CALIBRATION_SHADING_LS_A_G] = &calibration_shading_ls_a_g;
            otp->calibrations[CALIBRATION_SHADING_LS_A_B] = &calibration_shading_ls_a_b;
        }
        //SHADING_LS_TL84
        {
            int data_offset = 0;
            int lens = (i2c_read(addr_offset, cfg->otpDevAddrType) << 8) | i2c_read(addr_offset + 1, cfg->otpDevAddrType);
            log2file(path, "SHADING_LS_TL84 lens 0x%x\n", lens);
            if (lens > LSC_DATA_MARGIN) { // max 8k
                log2file(path, "invalid lens");
                break;
            }
            addr_offset += 2;
            for (int i = 0; i < LSC_DATA_MARGIN; ++i, ++addr_offset) {
                if (i < lens) {
                    buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                    log2file(path, "otp lsc data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
                }
            }
            (decompress)(X_node, Y_node, pLSC_dec, buffer, 3 * X_node * Y_node * sizeof(int));
            static uint8_t _CALIBRATION_SHADING_LS_TL84_R[1024];
            static uint8_t _CALIBRATION_SHADING_LS_TL84_G[1024];
            static uint8_t _CALIBRATION_SHADING_LS_TL84_B[1024];
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_TL84_R[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_TL84_G[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_TL84_B[i] = pLSC_dec[data_offset];
            }
            static LookupTable calibration_shading_ls_tl84_r = {
                .ptr = _CALIBRATION_SHADING_LS_TL84_R,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_R ) / sizeof( _CALIBRATION_SHADING_LS_TL84_R[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_TL84_R[0] )
            };
            static LookupTable calibration_shading_ls_tl84_g = {
                .ptr = _CALIBRATION_SHADING_LS_TL84_G,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_G ) / sizeof( _CALIBRATION_SHADING_LS_TL84_G[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_TL84_G[0] )
            };
            static LookupTable calibration_shading_ls_tl84_b = {
                .ptr = _CALIBRATION_SHADING_LS_TL84_B,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_TL84_B ) / sizeof( _CALIBRATION_SHADING_LS_TL84_B[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_TL84_B[0] )
            };
            otp->calibrations[CALIBRATION_SHADING_LS_TL84_R] = &calibration_shading_ls_tl84_r;
            otp->calibrations[CALIBRATION_SHADING_LS_TL84_G] = &calibration_shading_ls_tl84_g;
            otp->calibrations[CALIBRATION_SHADING_LS_TL84_B] = &calibration_shading_ls_tl84_b;
        }
        //SHADING_LS_D65
        {
            int data_offset = 0;
            int lens = (i2c_read(addr_offset, cfg->otpDevAddrType) << 8) | i2c_read(addr_offset + 1, cfg->otpDevAddrType);
            log2file(path, "SHADING_LS_D65 lens 0x%x\n", lens);
            if (lens > LSC_DATA_MARGIN) { // max 8k
                log2file(path, "invalid lens\n");
                break;
            }
            addr_offset += 2;
            for (int i = 0; i < LSC_DATA_MARGIN; ++i, ++addr_offset) {
                if (i < lens) {
                    buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                    log2file(path, "otp lsc data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
                }
            }
            (decompress)(X_node, Y_node, pLSC_dec, buffer, 3 * X_node * Y_node * sizeof(int));
            static uint8_t _CALIBRATION_SHADING_LS_D65_R[1024];
            static uint8_t _CALIBRATION_SHADING_LS_D65_G[1024];
            static uint8_t _CALIBRATION_SHADING_LS_D65_B[1024];
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_D65_R[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_D65_G[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_D65_B[i] = pLSC_dec[data_offset];
            }
            static LookupTable calibration_shading_ls_d65_r = {
                .ptr = _CALIBRATION_SHADING_LS_D65_R,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_D65_R ) / sizeof( _CALIBRATION_SHADING_LS_D65_R[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_D65_R[0] )
            };
            static LookupTable calibration_shading_ls_d65_g = {
                .ptr = _CALIBRATION_SHADING_LS_D65_G,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_D65_G ) / sizeof( _CALIBRATION_SHADING_LS_D65_G[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_D65_G[0] )
            };
            static LookupTable calibration_shading_ls_d65_b = {
                .ptr = _CALIBRATION_SHADING_LS_D65_B,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_D65_B ) / sizeof( _CALIBRATION_SHADING_LS_D65_B[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_D65_B[0] )
            };
            otp->calibrations[CALIBRATION_SHADING_LS_D65_R] = &calibration_shading_ls_d65_r;
            otp->calibrations[CALIBRATION_SHADING_LS_D65_G] = &calibration_shading_ls_d65_g;
            otp->calibrations[CALIBRATION_SHADING_LS_D65_B] = &calibration_shading_ls_d65_b;
        }
        //SHADING_LS_CWF
        {
            int data_offset = 0;
            int lens = (i2c_read(addr_offset, cfg->otpDevAddrType) << 8) | i2c_read(addr_offset + 1, cfg->otpDevAddrType);
            log2file(path, "SHADING_LS_CWF lens 0x%x\n", lens);
            if (lens > LSC_DATA_MARGIN) { // max 8k
                log2file(path, "invalid lens\n");
                break;
            }
            addr_offset += 2;
            for (int i = 0; i < LSC_DATA_MARGIN; ++i, ++addr_offset) {
                if (i < lens) {
                    buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                    log2file(path, "otp lsc data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
                }
            }
            (decompress)(X_node, Y_node, pLSC_dec, buffer, 3 * X_node * Y_node * sizeof(int));
            static uint8_t _CALIBRATION_SHADING_LS_CWF_R[1024];
            static uint8_t _CALIBRATION_SHADING_LS_CWF_G[1024];
            static uint8_t _CALIBRATION_SHADING_LS_CWF_B[1024];
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_CWF_R[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_CWF_G[i] = pLSC_dec[data_offset];
            }
            for (int i = 0; i < 1024; ++i, ++data_offset) {
                _CALIBRATION_SHADING_LS_CWF_B[i] = pLSC_dec[data_offset];
            }
            static LookupTable calibration_shading_ls_cwf_r = {
                .ptr = _CALIBRATION_SHADING_LS_CWF_R,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_R ) / sizeof( _CALIBRATION_SHADING_LS_CWF_R[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_CWF_R[0] )
            };
            static LookupTable calibration_shading_ls_cwf_g = {
                .ptr = _CALIBRATION_SHADING_LS_CWF_G,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_G ) / sizeof( _CALIBRATION_SHADING_LS_CWF_G[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_CWF_G[0] )
            };
            static LookupTable calibration_shading_ls_cwf_b = {
                .ptr = _CALIBRATION_SHADING_LS_CWF_B,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_SHADING_LS_CWF_B ) / sizeof( _CALIBRATION_SHADING_LS_CWF_B[0] ),
                .width = sizeof( _CALIBRATION_SHADING_LS_CWF_B[0] )
            };
            otp->calibrations[CALIBRATION_SHADING_LS_CWF_R] = &calibration_shading_ls_cwf_r;
            otp->calibrations[CALIBRATION_SHADING_LS_CWF_G] = &calibration_shading_ls_cwf_g;
            otp->calibrations[CALIBRATION_SHADING_LS_CWF_B] = &calibration_shading_ls_cwf_b;
        }
    } while(0);

    addr_offset = cfg->otpDevWbAddr;
    for (int i = 0; i < 1; ++i, ++addr_offset) {
        flag = i2c_read(addr_offset, cfg->otpDevAddrType);
        log2file(path, "otp wb valid, addr:0x%x value:0x%x\n", i, flag);
    }
    if (flag == 0x01) {
        //AWB_WB
        {
            for (int i = 0; i < 8; ++i, ++addr_offset) {
                buffer[i] = i2c_read(addr_offset, cfg->otpDevAddrType);
                log2file(path, "otp wb data, addr:0x%x value:0x%x\n", addr_offset, buffer[i]);
            }
            static int16_t _CALIBRATION_AWB_WB_GOLDEN_D50[2];
            static int16_t _CALIBRATION_AWB_WB_OTP_D50[2];
            _CALIBRATION_AWB_WB_GOLDEN_D50[0] = buffer[4] << 8 | buffer[5];
            _CALIBRATION_AWB_WB_GOLDEN_D50[1] = buffer[6] << 8 | buffer[7];
            _CALIBRATION_AWB_WB_OTP_D50[0] = buffer[0] << 8 | buffer[1];
            _CALIBRATION_AWB_WB_OTP_D50[1] = buffer[2] << 8 | buffer[3];
            static LookupTable calibration_awb_wb_golden_d50 = {
                .ptr = _CALIBRATION_AWB_WB_GOLDEN_D50,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50 ) / sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50[0] ),
                .width = sizeof( _CALIBRATION_AWB_WB_GOLDEN_D50[0] )
            };
            static LookupTable calibration_awb_wb_otp_d50 = {
                .ptr = _CALIBRATION_AWB_WB_OTP_D50,
                .rows = 1,
                .cols = sizeof( _CALIBRATION_AWB_WB_OTP_D50 ) / sizeof( _CALIBRATION_AWB_WB_OTP_D50[0] ),
                .width = sizeof( _CALIBRATION_AWB_WB_OTP_D50[0] )
            };
            otp->calibrations[CALIBRATION_AWB_WB_GOLDEN_D50] = &calibration_awb_wb_golden_d50;
            otp->calibrations[CALIBRATION_AWB_WB_OTP_D50] = &calibration_awb_wb_otp_d50;
        }
    } else {
        log2file(path, "Invalid WB Data\n");
    }

    delete[] pLSC_dec;
    delete[] pLSC_enc;

    if (lib)
        dlclose(lib);
    i2c_exit();
}

