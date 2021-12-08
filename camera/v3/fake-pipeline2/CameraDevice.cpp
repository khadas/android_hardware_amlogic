#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "VirtualCameraDevice"

#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>
#include "CameraDevice.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

CameraVirtualDevice* CameraVirtualDevice::mInstance = nullptr;

#if BUILD_KERNEL_4_9 == true
struct VirtualDevice CameraVirtualDevice::videoDevices[] = {
        {"/dev/video0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},0},
        {"/dev/video1",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},1},
        {"/dev/video2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},2},
        {"/dev/video3",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},3},
        {"/dev/video50",3,{FREED_VIDEO_DEVICE,FREED_META_DEVICE,FREED_VIDEO_DEVICE},{-1,-1,-1},{-1,-1,-1},50},
        {"/dev/video51",3,{FREED_VIDEO_DEVICE,FREED_META_DEVICE,FREED_VIDEO_DEVICE},{-1,-1,-1},{-1,-1,-1},51}
};
#else
struct VirtualDevice CameraVirtualDevice::videoDevices[] = {
        {"/dev/video0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},0},
        {"/dev/video2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},1},
        {"/dev/video4",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},2},
        {"/dev/video6",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},3},
        {"/dev/video50",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},50},
        {"/dev/video51",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},51}
};
#endif

CameraVirtualDevice::CameraVirtualDevice() {
}

int CameraVirtualDevice::checkDeviceExist(char* name) {
    for (int i = 0; i < DEVICE_NUM; i++) {
        if (strcmp(name, videoDevices[i].name) == 0) {
            return 0;
        }
    }
    return -1;
}

int CameraVirtualDevice::returnDeviceId(char* name) {
    for (int i = 0; i < DEVICE_NUM; i++) {
        if (strcmp(name, videoDevices[i].name) == 0) {
            return videoDevices[i].deviceID;
        }
    }
    return -1;
}


struct VirtualDevice* CameraVirtualDevice::findVideoDevice(int id) {
    int video_device_count = 0;
    char tmp[64];
    /*scan the device name*/
    for (size_t i = 0; i < ARRAY_SIZE(videoDevices); i++) {
        struct VirtualDevice* pDev = &videoDevices[i];
        if (!findCameraID(pDev->deviceID)
            || 0 != access(pDev->name, F_OK | R_OK | W_OK)) {
            ALOGD("%s: device %s is invaild", __FUNCTION__,pDev->name);
            continue;
        }

        for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
            switch (pDev->status[stream_idx])
            {
                case FREED_VIDEO_DEVICE:
                case USED_VIDEO_DEVICE:
                    if (video_device_count != id)
                        video_device_count++;
                    else {
                        ALOGD("%s: device %s stream index %d map to camera id %d", __FUNCTION__,pDev->name,stream_idx,id);
                        if (i >= ISP_DEVICE) {
                            pDev->cameraId[stream_idx] = id;
                            return pDev;
                        } else {
                            if (pDev->deviceID != id) {
                                memcpy(tmp, videoDevices[id].name, 64);
                                memcpy(videoDevices[id].name, pDev->name, 64);
                                memcpy(pDev->name, tmp, 64);
                            }
                            videoDevices[id].cameraId[stream_idx] = id;
                            return &videoDevices[id];
                        }
                    }
                    break;
                default:
                    break;
           }
        }
    }
    return nullptr;
}

int CameraVirtualDevice::checkDeviceStatus(struct VirtualDevice* pDev) {
    int ret = 0; //free
    if (pDev == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        if (pDev->status[i] == USED_VIDEO_DEVICE
            && pDev->cameraId[i] != -1) {

            ALOGD("%s: device is busy!", __FUNCTION__);
            ret = 1;  //busy
            break;
        }
    }
    return ret;
}


int CameraVirtualDevice::OpenVideoDevice(struct VirtualDevice* pDev) {
    ALOGD("%s: E", __FUNCTION__);
    if (pDev == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {

        int fd = open(pDev->name,O_RDWR | O_NONBLOCK);
        if (fd < 0) {
            ALOGE("open device %s , the %dth stream fail!",pDev->name,i);
            ALOGE("the reason is %s",strerror(errno));
            return -1;
        } else {
            if (pDev->status[i] == FREED_VIDEO_DEVICE) {
                pDev->status[i] = USED_VIDEO_DEVICE;
            }
            pDev->fileDesc[i] = fd;
        }
        ALOGD("%s: stream = %d ,fd = %d, status = %d!", __FUNCTION__,i,fd,pDev->status[i]);
    }
    return 0;
}

/*this function not be called*/
int CameraVirtualDevice::CloseVideoDevice(struct VirtualDevice* pDev) {
    ALOGD("%s: E", __FUNCTION__);
    if (pDev == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        close(pDev->fileDesc[i]);
        if (pDev->status[i] == USED_VIDEO_DEVICE) {
            pDev->status[i] = FREED_VIDEO_DEVICE ;
        }
        pDev->fileDesc[i] = -1;
    }
    return 0;
}

/*only the first time to do open operation.
 *anytime, we only return pointed fd.
 */
int CameraVirtualDevice::openVirtualDevice(int id) {
    ALOGD("%s: id = %d E", __FUNCTION__,id);
    struct VirtualDevice* pDevice = findVideoDevice(id);
    if (pDevice == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    ALOGD("%s: device name is %s", __FUNCTION__,pDevice->name);
    int DeviceStatus = checkDeviceStatus(pDevice);
    if (!DeviceStatus) {
        ALOGD("%s: device %s is free,open it", __FUNCTION__,pDevice->name);
        OpenVideoDevice(pDevice);
    }
    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == id) {
            ALOGD("%s: camera id:%d  fd = %d",
                __FUNCTION__,id,pDevice->fileDesc[i]);
            pDevice->status[i] = USED_VIDEO_DEVICE;
            return pDevice->fileDesc[i];
        }
    }
    return -1;
}
/*for mipi camera ,once we has opened all ports,
 *we never closed them.
 */
int CameraVirtualDevice::releaseVirtualDevice(int id,int fd) {
    ALOGD("%s: id =%d, fd = %d", __FUNCTION__,id,fd);
    struct VirtualDevice* pDevice = findVideoDevice(id);
    if (pDevice == nullptr)
        return -1;

    ALOGD("%s: device name %s", __FUNCTION__,pDevice->name);
    /*set correspond stream to free*/
    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == id && pDevice->fileDesc[i] == fd) {
            switch (pDevice->status[i]) {
                case USED_VIDEO_DEVICE:
                    pDevice->status[i] = FREED_VIDEO_DEVICE;
                    pDevice->cameraId[i] = -1;
                    break;
                default:
                    break;
            }
        }
        ALOGD("%s: status=%d, id =%d,fd =%d ",
        __FUNCTION__,pDevice->status[i],pDevice->cameraId[i],pDevice->fileDesc[i]);
    }

    int DeviceStatus = checkDeviceStatus(pDevice);
    if (!DeviceStatus)//free
        CloseVideoDevice(pDevice);
    return 0;
}

CameraVirtualDevice* CameraVirtualDevice::getInstance() {
    if (mInstance != nullptr) {
        return mInstance;
    } else {
        mInstance = new CameraVirtualDevice();
        return mInstance;
    }
}

int CameraVirtualDevice::getCameraNum() {
    int iCamerasNum = 0;
    ATRACE_CALL();
    for (int i = 0; i < (int)ARRAY_SIZE(videoDevices); i++ ) {
        struct VirtualDevice* pDev = &videoDevices[i];
        if (findCameraID(pDev->deviceID)
            && 0 == access(pDev->name, F_OK | R_OK | W_OK))
        {
            ALOGD("access %s success\n", pDev->name);
            for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++)
                if (pDev->status[stream_idx] == FREED_VIDEO_DEVICE) {
                    ALOGD("device %s stream %d \n", pDev->name,stream_idx);
                    iCamerasNum++;
                }
        }
    }
    return iCamerasNum;
}

int CameraVirtualDevice::findCameraID(int id) {
    bool flag = false;
    ALOGD("%s:id=%d",__FUNCTION__,id);
    for (int i = 0; i < DEVICE_NUM; i++) {
        if (id == videoDevices[i].deviceID) {
            flag = true;
            break;
        }
    }
    return flag;
}

