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
#include <linux/media.h>
#include "media-v4l2/mediaApi.h"
#include "media-v4l2/mediactl.h"

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

CameraVirtualDevice* CameraVirtualDevice::mInstance = nullptr;
struct VirtualDevice CameraVirtualDevice::usbvideoDevices[4];

#if BUILD_KERNEL_4_9 == true

#define USB_DEVICE_NUM  (4)
#define MIPI_DEVICE_NUM (2)

struct VirtualDevice CameraVirtualDevice::mipivideoDeviceslists[] = {
    {"/dev/video50",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},4, MIPI_CAM_DEV},
    {"/dev/video51",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},5, MIPI_CAM_DEV}
};

struct VirtualDevice CameraVirtualDevice::usbvideoDeviceslists[] = {
    {"/dev/video0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},0, USB_CAM_DEV},
    {"/dev/video1",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},1, USB_CAM_DEV},
    {"/dev/video2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},2, USB_CAM_DEV},
    {"/dev/video3",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},3, USB_CAM_DEV}
};
#else

#define USB_DEVICE_NUM  (4)
#define MIPI_DEVICE_NUM (7)

struct VirtualDevice CameraVirtualDevice::mipivideoDeviceslists[] = {
    {"/dev/video50",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},4, MIPI_CAM_DEV},
    {"/dev/video51",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},5, MIPI_CAM_DEV},
    {"/dev/video70",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},6, HDMI_CAM_DEV},

    {"/dev/media0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},60, V4L2MEDIA_CAM_DEV},
    {"/dev/media1",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},61, V4L2MEDIA_CAM_DEV},
    {"/dev/media2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},62, V4L2MEDIA_CAM_DEV},
    {"/dev/media3",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},63, V4L2MEDIA_CAM_DEV}
};

struct VirtualDevice CameraVirtualDevice::usbvideoDeviceslists[] = {
    {"/dev/video0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},0, USB_CAM_DEV},
    {"/dev/video2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},1, USB_CAM_DEV},
    {"/dev/video4",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},2, USB_CAM_DEV},
    {"/dev/video6",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},3, USB_CAM_DEV}
};
#endif

CameraVirtualDevice::CameraVirtualDevice() {
    recoverUsbDevicelists();
}

// check if devname is a potential camera device
// return value: 0 - potential camera.
//               otherwise: not a potential camera.
int CameraVirtualDevice::checkUsbDeviceExist(char* devname) {
    for (int i = 0; i < USB_DEVICE_NUM; i++) {
        if (strcmp(devname, usbvideoDevices[i].name) == 0) {
            return 0;
        }
    }
    return -1;
}

// for usb camera. runtime camera id is stored in deviceID.
int CameraVirtualDevice::returnUsbDeviceId(char* name) {
    for (int i = 0; i < USB_DEVICE_NUM; i++) {
        if (strcmp(name, usbvideoDevices[i].name) == 0) {
            return usbvideoDevices[i].deviceID + pluggedMipiCameraNum;
        }
    }
    return -1;
}

void CameraVirtualDevice::recoverUsbDevicelists(void) {
    ALOGD("%s: recoverUsbDevicelists", __FUNCTION__);
    memcpy(&usbvideoDevices, &usbvideoDeviceslists, sizeof(usbvideoDeviceslists));
}

struct VirtualDevice* CameraVirtualDevice::findMipiVideoDevice(int cam_id) {
    int video_device_count = 0;

    for (size_t i = 0; i < MIPI_DEVICE_NUM; i++) {
        struct VirtualDevice* pDev = &mipivideoDeviceslists[i];
        if ( 0 != access(pDev->name, F_OK | R_OK | W_OK)) {
            ALOGD("%s: device %s access fail", __FUNCTION__,pDev->name);
            continue;
        }

        if (pDev->type == V4L2MEDIA_CAM_DEV) {
            // for media device. skip usb cameras' media dev node.
            if ( false == isAmlMediaCamera(pDev->name) ) {
                // skip
                continue;
            }
        }
        if (pDev->type == HDMI_CAM_DEV) {
            if (!isHdmiVdinCameraEnable()) {
                continue;
            }
        }
        for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
            if (NONE_DEVICE != pDev->status[stream_idx]) {
                if (video_device_count != cam_id) {
                    video_device_count++;
                } else {
                    ALOGD("%s: devname  %s stream index %d map to camera id %d sta %d", __FUNCTION__,
                               pDev->name,stream_idx,cam_id, pDev->status[stream_idx]);
                    pDev->cameraId[stream_idx] = cam_id;
                    return pDev;
                }
            }
        }
    }
    return nullptr;

}


struct VirtualDevice* CameraVirtualDevice::findUsbVideoDevice(int cam_id) {
    int video_device_count = pluggedMipiCameraNum;
    int prefered_usb_device_idx = cam_id - pluggedMipiCameraNum;

    char tmp[64];

    if (cam_id < pluggedMipiCameraNum) {
        ALOGE("bad args. cam id for usb cam should >= %d", pluggedMipiCameraNum);
        return nullptr;
    }

    for (size_t i = 0; i < USB_DEVICE_NUM; i++) {
        struct VirtualDevice* pDev = &usbvideoDevices[i];
        if ( 0 != access(pDev->name, F_OK | R_OK | W_OK)) {
            ALOGD("%s: device %s access fail", __FUNCTION__,pDev->name);
            continue;
        }

        for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
            if ( NONE_DEVICE != pDev->status[stream_idx]) {
                if (video_device_count != cam_id) {
                    video_device_count++;
                } else {
                    ALOGD("%s: devname  %s stream index %d map to camera id %d sta %d", __FUNCTION__,
                              pDev->name,stream_idx,cam_id, pDev->status[stream_idx]);

                    if (i != prefered_usb_device_idx) {
                        // swap usb camera's devname. lower index has higher priority.
                        memcpy(tmp, usbvideoDevices[prefered_usb_device_idx].name, 64);
                        memcpy(usbvideoDevices[prefered_usb_device_idx].name, usbvideoDevices[i].name, 64);
                        memcpy(usbvideoDevices[i].name, tmp, 64);
                    }
                    usbvideoDevices[prefered_usb_device_idx].cameraId[stream_idx] = cam_id;
                    return &usbvideoDevices[prefered_usb_device_idx];
                }
           }
        }
    }
    return nullptr;
}

// parameter: cam id
// return value: related VirtualDevice
// scan current videoDevices array. all USED and FREED streams are counted.
struct VirtualDevice* CameraVirtualDevice::findVideoDevice(int cam_id) {
    struct VirtualDevice*  pDev = findMipiVideoDevice(cam_id);
    if (nullptr == pDev) {
        return findUsbVideoDevice( cam_id );
    } else {
        return pDev;
    }
}

// check all streams. if one stream is USED, return 1 (busy state).
// return 0 (free) when all streams are FREED_VIDEO_DEVICE.
// invalid args - return value < 0.
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

// open all streams of given device.
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
                // only transfer FREED to USED. do nothing to NONE_DEVICE.
                pDev->status[i] = USED_VIDEO_DEVICE;
            }
            pDev->fileDesc[i] = fd;
        }
        ALOGD("%s: stream = %d ,fd = %d, status = %d!", __FUNCTION__,i,fd,pDev->status[i]);
    }
    return 0;
}

// open all streams of given device.
int CameraVirtualDevice::CloseVideoDevice(struct VirtualDevice* pDev) {
    ALOGD("%s: E", __FUNCTION__);
    if (pDev == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        if (pDev->fileDesc[i] >= 0) {
            close(pDev->fileDesc[i]);
        } else {
            ALOGE("close fd is invalid. has been closed before !!?");
        }
        if (pDev->status[i] == USED_VIDEO_DEVICE) {
            // only transfer USED to FREED. do nothing to NONE_DEVICE
            pDev->status[i] = FREED_VIDEO_DEVICE ;
        }
        pDev->fileDesc[i] = -1;
    }
    return 0;
}

/*
only the first time to do open operation.
for multi stream devices, we open all streams.
*/
int CameraVirtualDevice::openVirtualDevice(int cam_id) {

    ALOGD("%s: cam_id = %d E", __FUNCTION__,cam_id);

    struct VirtualDevice* pDevice = findVideoDevice(cam_id);

    if (pDevice == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }

    ALOGD("%s: device name is %s", __FUNCTION__,pDevice->name);
    int DeviceStatus = checkDeviceStatus(pDevice);
    if (0 == DeviceStatus) {
        ALOGD("%s: device %s, all streams are free, open them", __FUNCTION__,pDevice->name);
        OpenVideoDevice(pDevice);
    }

    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == cam_id) {
            ALOGD("%s: camera id:%d  fd = %d",
                __FUNCTION__,cam_id, pDevice->fileDesc[i]);
            pDevice->status[i] = USED_VIDEO_DEVICE;
            return pDevice->fileDesc[i];
        }
    }
    return -1;
}

struct VirtualDevice* CameraVirtualDevice::getVirtualDevice(int cam_id)
{
    ALOGD("%s: cam id = %d E", __FUNCTION__,cam_id);
    return findVideoDevice(cam_id);
}


/*
for multi stream device. we close all streams
when all streams are in FREED state.
*/
int CameraVirtualDevice::releaseVirtualDevice(int cam_id, int fd) {
    ALOGD("%s: id =%d, fd = %d", __FUNCTION__, cam_id, fd);
    struct VirtualDevice* pDevice = NULL;

    if (cam_id >= pluggedMipiCameraNum) {
        // release a usb camera device.
        int prefered_usb_device_idx = cam_id - pluggedMipiCameraNum;
        pDevice = &usbvideoDevices[prefered_usb_device_idx];
    } else {
        // release a mipi camera
        pDevice = findMipiVideoDevice (cam_id);
    }

    if (pDevice == nullptr) {
        ALOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }

    ALOGD("%s: device name %s", __FUNCTION__, pDevice->name);
    /*set correspond stream to free*/
    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == cam_id && pDevice->fileDesc[i] == fd) {
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
    if (0 == DeviceStatus) {
        // all streams are FREED. close all streams.
        CloseVideoDevice(pDevice);
    }
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

bool CameraVirtualDevice::isAmlMediaCamera (char *dev_node_name)
{
    int ret = -1;
    bool result = false;
    struct media_device_info mdi;
    /* Open Media device and keep it open */
    int fd = open(dev_node_name, O_RDWR);
    if (fd == -1) {
        ALOGE("Media Device open errno %s\n", strerror(errno));
        return false;
    } else {
        ret = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
        if (ret < 0) {
            ALOGI("Media Device Info errno %s\n", strerror(errno));
            result = false;
        } else {
            ALOGI("Media device info: model %s driver %s serial %s bus_info %s \n",
                mdi.model, mdi.driver, mdi.serial, mdi.bus_info );
            if ( 0 == strncmp(mdi.driver, "t7-cam", 6) ||
                 0 == strncmp(mdi.driver, "t7c-cam", 7)) {
                result = true;
            } else if (0 == strncmp(mdi.driver, "aml-cam", 7)) {
                ALOGD("start check for for aml-cam");
                void* mediaStream = malloc(sizeof( struct media_stream));
                if (mediaStream == NULL) {
                    ALOGE("alloc media stream mem fail\n");
                    result = false;
                } else {
                    struct media_device * media_dev = media_device_new_with_fd(fd);
                    if (media_dev == NULL) {
                        ALOGE("new media device failed \n");
                        result = false;
                    } else if (0 == mediaStreamInit((media_stream_t *)mediaStream, media_dev)) {
                        ALOGD("mediaStreamInit successed");
                        result = true;
                    } else {
                        ALOGD("mediaStreamInit failed");
                    }
                    if (media_dev) {
                        media_device_unref(media_dev);
                    }
                    if (mediaStream) {
                        free(mediaStream);
                    }
                }
            }
        }
        close(fd);
    }
    return result;
}

bool CameraVirtualDevice::isHdmiVdinCameraEnable() {
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.media.hdmi_vdin.enable", property, "false");
    return strstr(property, "true");
}

// scan the videoDevices array.
// enumerate accessable devname as cameras.
// for multistream cameras. one stream is one camera.
int CameraVirtualDevice::getCameraNum() {

    int iCamerasNum = 0;

    for (int i = 0; i < MIPI_DEVICE_NUM; i++ ) {
        struct VirtualDevice* pDev = &mipivideoDeviceslists[i];
        int ret = access(pDev->name, F_OK | R_OK | W_OK);
        if ( 0 == ret)
        {
            ALOGD("access %s success\n", pDev->name);
            if (pDev->type == V4L2MEDIA_CAM_DEV) {
                // for media device. skip usb cameras' media dev node.
                if ( false == isAmlMediaCamera(pDev->name) ) {
                    // skip
                    continue;
                }
            }
            if (pDev->type == HDMI_CAM_DEV) {
                if (!isHdmiVdinCameraEnable()) {
                    continue;
                }
            }
            for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++)
                if (pDev->status[stream_idx] != NONE_DEVICE) {
                    ALOGD("device %s stream %d \n", pDev->name,stream_idx);
                    iCamerasNum++;
                }
        } else {
            ALOGD(" %s, access failed. ret %d \n", pDev->name, ret);
        }
    }

    // save plugged mipi camera number.
    pluggedMipiCameraNum = iCamerasNum;

    for (int i = 0; i < USB_DEVICE_NUM; i++ ) {
        struct VirtualDevice* pDev = &usbvideoDevices[i];
        int ret = access(pDev->name, F_OK | R_OK | W_OK);
        if ( 0 == ret)
        {
            for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
                if (pDev->status[stream_idx] != NONE_DEVICE) {
                    ALOGD("device %s stream %d \n", pDev->name,stream_idx);
                    iCamerasNum++;
                }
            }
        } else {
            ALOGD(" %s, access failed. ret %d \n", pDev->name, ret);
        }
    }

    return iCamerasNum;
}

// for usb camera. deviceID is its camera id.
int CameraVirtualDevice::findUsbCameraID(int cam_id) {

    bool flag = false;

    ALOGD("%s:cam id=%d",__FUNCTION__,cam_id);

    for (int i = 0; i < USB_DEVICE_NUM; i++) {
        if (cam_id == usbvideoDevices[i].deviceID) {
            flag = true;
            break;
        }
    }

    ALOGD("%s:cam_id=%d, ret flag = %d",__FUNCTION__,cam_id, flag);

    return flag;
}

