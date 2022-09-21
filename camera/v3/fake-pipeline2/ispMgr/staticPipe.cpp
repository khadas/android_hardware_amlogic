
#define LOG_TAG "staticPipe"

#include <cstdlib>

#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include <vector>

#include "staticPipe.h"
#include "sensor/sensor_config.h"

namespace android {

int staticPipe::fetchPipeMaxResolution(media_stream_t *stream, uint32_t& width, uint32_t &height) {
    auto cfg = matchSensorConfig(stream);
    if (cfg) {
        width = cfg->sensorWidth;
        height = cfg->sensorHeight;
        ALOGI("find matched sensor configs %dx%d", width, height);
        return 0;
    }
    ALOGE("do not find matched sensor configs");
    return -1;
}
}
