#define LOG_TAG "GlobalResource"

#include "GlobalResource.h"
#include "Base.h"
#include <linux/videodev2.h>
#include <log/log.h>

namespace android {
int GlobalResource::mDeviceFd[HAL_MAX_CAM_NUM][HAL_MAX_FD_NUM] = {{-1,-1,-1},{-1,-1,-1},{-1,-1,-1}};
GlobalResource* GlobalResource::mInstance = nullptr;
Mutex GlobalResource::mLock;

GlobalResource* GlobalResource::getInstance() {
     if (mInstance) {
        return mInstance;
     } else {
        Mutex::Autolock lock(&mLock);
        mInstance = new GlobalResource();
        return mInstance;
     }
 }

 void GlobalResource::DeleteInstance() {
    Mutex::Autolock lock(&mLock);
    if (mInstance)
    {
        delete mInstance;
        mInstance = NULL;
    }
 }
#define ISP_V4L2_CID_BASE ( 0x00f00000 | 0xf000 )
#define ISP_V4L2_CID_ISP_MODULES_BYPASS ( ISP_V4L2_CID_BASE + 139 )

#define ACAMERA_ISP_TOP_BYPASS_TEMPER (1LL<<13)

int GlobalResource::getFd(char* device_name, int id, std::vector<int> &fds) {
    bool IsInit = true;
    //enum v4l2_buf_type type;
    //struct    v4l2_requestbuffers rb;
    //struct v4l2_ext_controls ctrls;
    //struct v4l2_ext_control ext_ctrl;
    //unsigned long long bypass_array[2];
    char property[PROPERTY_VALUE_MAX];
    int counter = 2;
    for (int i = 0; i < HAL_MAX_FD_NUM; i++) {
        ALOGD("%s: fd %d", __FUNCTION__, mDeviceFd[id][i]);
        if (mDeviceFd[id][i] == -1) {
            IsInit = false;
            break;
        }
    }
    ALOGD("%s: id %d init? %d E", __FUNCTION__, id, IsInit);
    if (!IsInit) {
        ALOGD("%s:init global resource , name:%s", __FUNCTION__, device_name);
        for (int i = 0; i < HAL_MAX_FD_NUM; i++)
            mDeviceFd[id][i] = open(device_name, O_RDWR);

        memset(property, 0, sizeof(property));
        if (property_get("vendor.media.camera.count", property, NULL) > 0) {
            sscanf(property,"%d",&counter);
            if (counter > 4)
                counter = 2;
        }

        mISP = isp3a::get_instance();
        mISP->open_isp3a_library(counter);
    }

    fds[channel_preview] = mDeviceFd[id][1];// sc0
    fds[channel_capture] = mDeviceFd[id][0];// sc3 no resize
    fds[channel_record] = mDeviceFd[id][2];// sc1

    return 0;
}
}
