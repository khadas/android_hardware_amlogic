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

namespace android {

class staticPipe {
  public:
    static std::vector<struct pipe_info*> supportedPipes;
    static int constructStaticPipe();
    static int fetchPipeMaxResolution(int idx, uint32_t& width, uint32_t &height);
};
}
#endif