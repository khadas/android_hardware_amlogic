#define LOG_TAG "ISP Library"

#include "Isp3a.h"
#include <utils/Log.h>
#include "ispaaalib.h"
#include "CamHalDebugLog.h"

namespace android{

isp3a* isp3a::mInstance = nullptr;

isp3a* isp3a::get_instance(void) {
    if (mInstance == nullptr) {
        mInstance = new isp3a();
        return mInstance;
    }
    else
        return mInstance;
}

isp3a::isp3a() {
    CAMHAL_LOGD("%s:create isp3a object",__FUNCTION__);
    mIsOpened = false;
    mOpenCount = 0;
}
isp3a::~isp3a() {
    CAMHAL_LOGD("%s:release isp3a object",__FUNCTION__);
}
void isp3a::open_isp3a_library(int num) {
    CAMHAL_LOGD("%s:E",__FUNCTION__);
#ifdef ISP_ENABLE
    if (mIsOpened == false && mOpenCount == 0) {
        CAMHAL_LOGD("%s:enable isp lib",__FUNCTION__);
        isp_lib_enable(num);
        mIsOpened = true;
    } else {
        CAMHAL_LOGD("%s:isp lib has been opened",__FUNCTION__);
    }
    mOpenCount += 1;
#endif
}

void isp3a::close_isp3a_library() {
     CAMHAL_LOGD("%s:E",__FUNCTION__);
#ifdef ISP_ENABLE
    mOpenCount -= 1;
    if (mIsOpened && mOpenCount == 0) {
        CAMHAL_LOGD("%s:disable isp lib",__FUNCTION__);
        isp_lib_disable();
        mIsOpened = false;
    }
#endif
}

void isp3a::print_status() {
    if (mIsOpened == false) {
        CAMHAL_LOGD("isp lib is closed");
        CAMHAL_LOGD("current open count is %d",mOpenCount);
    } else {
        CAMHAL_LOGD("isp lib has opend");
        CAMHAL_LOGD("current open count is %d",mOpenCount);
    }
}
}
