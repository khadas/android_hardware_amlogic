
#define LOG_TAG "staticPipe"

#include <cstdlib>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include <vector>
#include "CamHalDebugLog.h"

#include "staticPipe.h"


namespace android {

int staticPipe::fetchPipeMaxResolution(media_stream_t *stream, uint32_t& width, uint32_t &height) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        width = cfg->sensorWidth;
        height = cfg->sensorHeight;
        CAMHAL_LOGI("find matched sensor configs %dx%d", width, height);
        return 0;
    }
    CAMHAL_LOGE("do not find matched sensor configs");
    return -1;
}

int staticPipe::fetchSensorFormat(media_stream_t *stream, int hdrEnable, uint32_t fps) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        return hdrEnable ? cfg->wdrFormat : (fps == 60 ? cfg->sdrFormat60HZ : cfg->sdrFormat);
    }
    CAMHAL_LOGE("do not find matched");
    return -1;
}

sensorType staticPipe::fetchSensorType(media_stream_t * stream) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        return cfg->type;
    }
    return sensor_NULL;
}

int staticPipe::fetchSensorOTP(media_stream_t * stream, aisp_calib_info_t *otp) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        cmos_get_sensor_otp_data(cfg, otp);
        for (int i = 0; i < CALIBRATION_TOTAL_SIZE; i++) {
            LookupTable *src = GET_LOOKUP_PTR(otp, i);
            if (src) {
                if (i == CALIBRATION_AWB_WB_OTP_D50) {
                    for (int j = 0; j < src->cols*src->width; ++j) {
                        CAMHAL_LOGD("WB_OTP value 0x%x", ((uint8_t *)(src->ptr))[j]);
                    }
                } else if (i == CALIBRATION_AWB_WB_GOLDEN_D50) {
                    for (int j = 0; j < src->cols*src->width; ++j) {
                        CAMHAL_LOGD("WB_GOLDEN value 0x%x", ((uint8_t *)(src->ptr))[j]);
                    }
                }
            }
        }
        return 0;
    }
    return -1;
}

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
int staticPipe::fetchSensorGdcParameter(
                        media_stream_t * stream, GDCInParam in_params, struct dewarp_params *dewarp_params) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        cfg->cmos_get_sensor_gdc_parameter(cfg, in_params, dewarp_params);
        return 0;
    }
    return -1;
}
#endif

}

