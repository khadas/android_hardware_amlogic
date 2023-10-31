
#ifndef __STATIC_PIPE_H__
#define __STATIC_PIPE_H__

#include <cstdlib>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "media-v4l2/mediactl.h"
#include "media-v4l2/v4l2subdev.h"
#include "media-v4l2/v4l2videodev.h"
#include "media-v4l2/mediaApi.h"
#include "sensor/sensor_config.h"


namespace android {

class staticPipe {
  public:
    static int fetchPipeMaxResolution(media_stream_t *stream, uint32_t& width, uint32_t &height);
    static int fetchSensorFormat(media_stream_t *stream, int hdrEnable, uint32_t fps);
    static sensorType fetchSensorType(media_stream_t *stream);
    static int fetchSensorOTP(media_stream_t * stream, aisp_calib_info_t *otp);

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    static int fetchSensorGdcParameter(media_stream_t * stream, GDCInParam in_params,
                                               struct dewarp_params *dewarp_params);
#endif

};
}
#endif
