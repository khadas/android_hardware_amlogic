
#define LOG_TAG "VirtualCameraDevice"


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

#include "CamHalDebugLog.h"

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))
namespace android {
CameraVirtualDevice* CameraVirtualDevice::mInstance = nullptr;
struct VirtualDevice CameraVirtualDevice::usbvideoDevices[5];

#if BUILD_KERNEL_4_9 == true

#define USB_DEVICE_NUM  4
#define MIPI_DEVICE_NUM 2
#define MAX_CAMERA_NUM 6

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
#define USB_DEVICE_NUM  5
#define MIPI_DEVICE_NUM 6
#define MAX_CAMERA_NUM 11

struct VirtualDevice CameraVirtualDevice::mipivideoDeviceslists[] = {
    {"/dev/video50",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},4, MIPI_CAM_DEV},
    {"/dev/video51",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},5, MIPI_CAM_DEV},

    {"/dev/media0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},60, V4L2MEDIA_CAM_DEV},
    {"/dev/media1",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},61, V4L2MEDIA_CAM_DEV},
    {"/dev/media2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},62, V4L2MEDIA_CAM_DEV},
    {"/dev/media3",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},63, V4L2MEDIA_CAM_DEV}
};

struct VirtualDevice CameraVirtualDevice::usbvideoDeviceslists[] = {
    {"/dev/video0",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},0, USB_CAM_DEV},
    {"/dev/video2",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},1, USB_CAM_DEV},
    {"/dev/video4",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},2, USB_CAM_DEV},
    {"/dev/video6",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},3, USB_CAM_DEV},
    {"/dev/video70",1,{FREED_VIDEO_DEVICE,NONE_DEVICE,NONE_DEVICE},{-1,-1,-1},{-1,-1,-1},4, USB_CAM_DEV}
};

#endif

CameraVirtualDevice::CameraVirtualDevice(): pluggedMipiCameraNum(0) {
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
    CAMHAL_LOGD("%s: recoverUsbDevicelists", __FUNCTION__);
    memcpy(&usbvideoDevices, &usbvideoDeviceslists, sizeof(usbvideoDeviceslists));
}

struct VirtualDevice* CameraVirtualDevice::findMipiVideoDevice(int cam_id) {
    int video_device_count = 0;

    for (size_t i = 0; i < MIPI_DEVICE_NUM; i++) {
        struct VirtualDevice* pDev = &mipivideoDeviceslists[i];
        if ( 0 != access(pDev->name, F_OK | R_OK | W_OK)) {
            CAMHAL_LOGD("%s: device %s access fail", __FUNCTION__,pDev->name);
            continue;
        }

        if (pDev->type == V4L2MEDIA_CAM_DEV) {
            // for media device. skip usb cameras' media dev node.
            if ( false == isAmlMediaCamera(pDev->name) ) {
#ifdef MAINTAIN_FD_ENABLE
                closeVideoDeviceFd(pDev->name);
#endif
                // skip
                continue;
            }
        }

        for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
            if (NONE_DEVICE != pDev->status[stream_idx]) {
                if (video_device_count != cam_id) {
                    video_device_count++;
                } else {
                    CAMHAL_LOGD("%s: devname  %s stream index %d map to camera id %d sta %d", __FUNCTION__,
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
    int preferred_usb_device_idx = cam_id - pluggedMipiCameraNum;

    if (cam_id < pluggedMipiCameraNum) {
        CAMHAL_LOGE("bad args. cam id for usb cam should >= %d", pluggedMipiCameraNum);
        return nullptr;
    }

    for (int i = 0; i < USB_DEVICE_NUM; i++) {
        if (usbvideoDevices[i].deviceID == preferred_usb_device_idx) {
            std::string devName = usbvideoDevices[i].name;
            if (0 != videoMap.count(devName)) {
                struct VirtualDevice* pDev = &usbvideoDevices[i];
                if ( 0 != access(pDev->name, F_OK | R_OK | W_OK)) {
                    CAMHAL_LOGD("%s: device %s access fail", __FUNCTION__,pDev->name);
                    return nullptr;
                } else {
                    CAMHAL_LOGD("%s: device %s access success", __FUNCTION__,pDev->name);
                }
                if (pDev->type == USB_CAM_DEV) {
                    bool bypass = false;
                    if (!strcmp(pDev->name, HDMI_VDIN_VIDEO_PATH)) {
                        if (!(HDMIStatus::getInstance()->isStandardHDMICamera()))
                            bypass = true;
                    } else {
                        if (!isStandardUSBCamera(pDev->name))
                            bypass = true;
                    }
                    if (bypass) {
#ifdef MAINTAIN_FD_ENABLE
                        if (strcmp(pDev->name, HDMI_VDIN_VIDEO_PATH) != 0) {
                            closeVideoDeviceFd(pDev->name);
                        }
#endif
                        CAMHAL_LOGD("%s is not a valid usb camera", pDev->name);
                        continue;
                    }
                }

                for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
                    if ( NONE_DEVICE != pDev->status[stream_idx]) {
                        CAMHAL_LOGD("%s: devname  %s stream index %d map to camera id %d sta %d", __FUNCTION__,
                                    pDev->name,stream_idx,cam_id, pDev->status[stream_idx]);

                        usbvideoDevices[i].cameraId[stream_idx] = cam_id;
                        return &usbvideoDevices[i];

                    }
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
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        if (pDev->status[i] == USED_VIDEO_DEVICE
            && pDev->cameraId[i] != -1) {
            CAMHAL_LOGD("%s: device is busy!", __FUNCTION__);
            ret = 1;  //busy
            break;
        }
    }
    return ret;
}

// open all streams of given device.
int CameraVirtualDevice::OpenVideoDevice(struct VirtualDevice* pDev) {
    int fd = -1;
    CAMHAL_LOGD("%s: camera id %d E", __FUNCTION__, pDev->cameraId[0]);
    if (pDev == nullptr) {
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        fd = getVideoDeviceFd(pDev->name);
        if (fd < 0) {
            CAMHAL_LOGE("open device %s , the %dth stream fail!",pDev->name,i);
            CAMHAL_LOGE("the reason is %s",strerror(errno));
            return -1;
        } else {
            if (pDev->status[i] == FREED_VIDEO_DEVICE) {
                // only transfer FREED to USED. do nothing to NONE_DEVICE.
                pDev->status[i] = USED_VIDEO_DEVICE;
            }
            pDev->fileDesc[i] = fd;
        }
        CAMHAL_LOGD("%s: stream = %d ,fd = %d, status = %d!", __FUNCTION__,i,fd,pDev->status[i]);
    }
    return 0;
}

// open all streams of given device.
int CameraVirtualDevice::CloseVideoDevice(struct VirtualDevice* pDev) {
    CAMHAL_LOGD("%s: E", __FUNCTION__);
    if (pDev == nullptr) {
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        if (pDev->fileDesc[i] >= 0) {
            closeVideoDeviceFd(pDev->name);
        } else {
            CAMHAL_LOGE("close fd is invalid. has been closed before !!?");
        }
        if (pDev->status[i] == USED_VIDEO_DEVICE) {
            // only transfer USED to FREED. do nothing to NONE_DEVICE
            pDev->status[i] = FREED_VIDEO_DEVICE ;
        }
    }
    return 0;
}

int CameraVirtualDevice::CloseVideoDeviceWoFd(struct VirtualDevice* pDev) {
    CAMHAL_LOGD("%s: E", __FUNCTION__);
    if (pDev == nullptr) {
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }
    for (int i = 0; i < pDev->streamNum; i++) {
        if (pDev->status[i] == USED_VIDEO_DEVICE) {
            // only transfer USED to FREED. do nothing to NONE_DEVICE
            pDev->status[i] = FREED_VIDEO_DEVICE ;
        }
    }
    return 0;
}

/*
only the first time to do open operation.
for multi stream devices, we open all streams.
*/
int CameraVirtualDevice::openVirtualDevice(int cam_id) {

    CAMHAL_LOGD("%s: cam_id = %d E", __FUNCTION__,cam_id);

    struct VirtualDevice* pDevice = findVideoDevice(cam_id);

    if (pDevice == nullptr) {
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }

    CAMHAL_LOGD("%s: device name is %s", __FUNCTION__,pDevice->name);
    int DeviceStatus = checkDeviceStatus(pDevice);
    if (0 == DeviceStatus) {
        CAMHAL_LOGD("%s: device %s, all streams are free, open them", __FUNCTION__,pDevice->name);
        OpenVideoDevice(pDevice);
    }

    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == cam_id) {
            CAMHAL_LOGD("%s: camera id:%d  fd = %d",
                __FUNCTION__,cam_id, pDevice->fileDesc[i]);
            pDevice->status[i] = USED_VIDEO_DEVICE;
            return pDevice->fileDesc[i];
        }
    }
    return -1;
}

struct VirtualDevice* CameraVirtualDevice::getVirtualDevice(int cam_id)
{
    CAMHAL_LOGD("%s: cam id = %d E", __FUNCTION__,cam_id);
    return findVideoDevice(cam_id);
}

/*
for multi stream device. we close all streams
when all streams are in FREED state.
*/
int CameraVirtualDevice::releaseVirtualDevice(int cam_id, int fd) {
    CAMHAL_LOGD("%s: id = %d, fd = %d", __FUNCTION__, cam_id, fd);
    struct VirtualDevice* pDevice = NULL;

    if (cam_id >= pluggedMipiCameraNum) {
        pDevice = findUsbVideoDevice(cam_id);
    } else {
        // release a mipi camera
        pDevice = findMipiVideoDevice (cam_id);
    }

    if (pDevice == nullptr) {
        CAMHAL_LOGD("%s: device is null!", __FUNCTION__);
        return -1;
    }

    CAMHAL_LOGD("%s: device name %s", __FUNCTION__, pDevice->name);
    /*set correspond stream to free*/
    for (int i = 0; i < pDevice->streamNum; i++) {
        if (pDevice->cameraId[i] == cam_id) {
            switch (pDevice->status[i]) {
                case USED_VIDEO_DEVICE:
                    pDevice->status[i] = FREED_VIDEO_DEVICE;
                    pDevice->cameraId[i] = -1;
                    break;
                default:
                    break;
            }
        }
        CAMHAL_LOGD("%s: status=%d, id =%d,fd =%d ",
        __FUNCTION__, pDevice->status[i], pDevice->cameraId[i], pDevice->fileDesc[i]);
    }

    int DeviceStatus = checkDeviceStatus(pDevice);
    if (0 == DeviceStatus) {
        // all streams are FREED. close all streams.
#ifdef MAINTAIN_FD_ENABLE
        if (!strcmp(pDevice->name, HDMI_VDIN_VIDEO_PATH)) {
            CloseVideoDevice(pDevice);
        } else {
            CloseVideoDeviceWoFd(pDevice);
        }
#else
        CloseVideoDevice(pDevice);
#endif
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

int CameraVirtualDevice::getDeviceNameIdbyIndex(int index) {
    std::string devName = usbvideoDevices[index].name;
    return stoi(devName.substr(devName.find("o") + 1));
}

bool CameraVirtualDevice::isNormalExternalCameraByIndex(int index) {
    if (index < 0 || index >= USB_DEVICE_NUM) {
        CAMHAL_LOGE("%s the USB camera index  %d is invalid for USB External Camera",  __FUNCTION__,index);
        return false;
    }
    struct VirtualDevice* pDev = &usbvideoDevices[index];
    if (0 != access(pDev->name, F_OK | R_OK | W_OK)) {
        ALOGD("%s: device %s is invalid", __FUNCTION__,pDev->name);
        return false;
    }
    return true;
}

bool CameraVirtualDevice::isNormalExternalCameraByName(char *dev_name) {
    struct VirtualDevice* pDev = nullptr;
    for (int i = 0; i < USB_DEVICE_NUM; i++) {
        if (!strcmp(usbvideoDevices[i].name, dev_name)) {
            pDev = &usbvideoDevices[i];
            break;
        }
    }

    if (pDev == nullptr) {
        CAMHAL_LOGD("device %s in not in list", dev_name);
        return false;
    }
    int count = 0;
    while (count < 20) {
        if (0 == access(pDev->name, F_OK | R_OK | W_OK)) {
            CAMHAL_LOGD("access %s success\n", pDev->name);
            break;
        } else {
            CAMHAL_LOGD("access %s fail , i = %d .\n", pDev->name,count);
            usleep(50000);
            count++;
        }
    }

    if (pDev->type == USB_CAM_DEV) {
        bool bypass = false;
        if (!strcmp(pDev->name, HDMI_VDIN_VIDEO_PATH)) {
            if (!(HDMIStatus::getInstance()->isStandardHDMICamera()))
                bypass = true;
        } else {
            if (!isStandardUSBCamera(pDev->name))
                bypass = true;
        }
        if (bypass) {
#ifdef MAINTAIN_FD_ENABLE
            if (strcmp(pDev->name, HDMI_VDIN_VIDEO_PATH) != 0) {
                closeVideoDeviceFd(pDev->name);
            }
#endif
            CAMHAL_LOGD("%s is not a valid usb camera", pDev->name);
            return false;
        }
    }
    return true;

}

void CameraVirtualDevice::addUsbDevice(char * dev_name) {
    std::string devName = dev_name;
    bool searchFlag = false;
    if (videoMap.count(devName) == 0) {
        /*search which camera id has not been register*/
        for (int i = 0; i < USB_DEVICE_NUM; i++) {
            searchFlag = false;
            /*search camera id i weather has not been register*/
            for (auto iter : videoMap) {
                if (iter.second == i) {
                    searchFlag = true;
                    break;
                }
            }
            /*if camera camera id i has not been register, register here*/
            if (!searchFlag) {
                videoMap.insert(std::pair<std::string, int>(devName, i));
                for (int index = 0; index < USB_DEVICE_NUM; index++) {
                    if (strcmp(usbvideoDevices[index].name, dev_name) == 0)
                        usbvideoDevices[index].deviceID = i;
                }
                return;
            }
        }
    } else {
        CAMHAL_LOGD("%s already has been plugged", dev_name);
    }
}

void CameraVirtualDevice::deleteUsbDevice(char * dev_name) {
    std::string devName = dev_name;
    videoMap.erase(devName);
#ifdef MAINTAIN_FD_ENABLE
    if (strcmp(devName.c_str(), HDMI_VDIN_VIDEO_PATH) != 0) {
        closeVideoDeviceFd(devName.data());
    }
#endif
}

bool CameraVirtualDevice::isStandardUSBCamera(char * dev_node_name)
{
    int ret = -1;
    bool result   = false;
    bool skipH264 = true;
    int  loopSize = 2;
    struct v4l2_frmsizeenum frmsize;
    uint32_t srcfmt[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_H264,
    };
    if (property_get_bool("ro.vendor.platform.usehwh264", false) ||
        property_get_bool("vendor.media.camera.dec.mediahalsdk", false)) {
        skipH264 = false;
        loopSize = 3;
        ALOGI("H264 is enabled, do not skip check");
    }
    int fd = getVideoDeviceFd(dev_node_name);
    if (fd < 0) {
        CAMHAL_LOGE("%s open USB %s fd error %s", __FUNCTION__, dev_node_name, strerror(errno));
        return result;
    }
    for (int j = 0; j < loopSize; j++) {
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = srcfmt[j];
        frmsize.index = 0;
        frmsize.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        ret = ioctl(fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
         if (ret >= 0) {
             result = true;
             break;
         }
    }
#ifndef MAINTAIN_FD_ENABLE
    closeVideoDeviceFd(dev_node_name);
#endif
    return result;
}

bool CameraVirtualDevice::isAmlMediaCamera (char* dev_node_name)
{
    int ret = -1;
    bool result = false;
    struct media_device_info mdi;
    /* Open Media device and keep it open */
    int fd = getVideoDeviceFd(dev_node_name);
    if (fd == -1) {
        CAMHAL_LOGE("Media Device open errno %s\n", strerror(errno));
        return false;
    } else {
        ret = ioctl(fd, MEDIA_IOC_DEVICE_INFO, &mdi);
        if (ret < 0) {
            CAMHAL_LOGI("Media Device Info errno %s\n", strerror(errno));
            result = false;
        } else {
            CAMHAL_LOGI("Media device info: model %s driver %s serial %s bus_info %s \n",
                mdi.model, mdi.driver, mdi.serial, mdi.bus_info );
            if ( 0 == strncmp(mdi.driver, "t7-cam", 6) ||
                 0 == strncmp(mdi.driver, "t7c-cam", 7)) {
                result = true;
            } else if (0 == strncmp(mdi.driver, "aml-cam", 7)) {
                CAMHAL_LOGD("start check for for aml-cam");
                void* mediaStream = malloc(sizeof( struct media_stream));
                if (mediaStream == NULL) {
                    CAMHAL_LOGE("alloc media stream mem fail\n");
                    result = false;
                } else {
                    struct media_device * media_dev = media_device_new_with_fd(fd);
                    if (media_dev == NULL) {
                        CAMHAL_LOGE("new media device failed \n");
                        result = false;
                    } else if (0 == mediaStreamInit((media_stream_t *)mediaStream, media_dev)) {
                        CAMHAL_LOGD("mediaStreamInit succeeded");
                        result = true;
                    } else {
                        CAMHAL_LOGD("mediaStreamInit failed");
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
#ifndef MAINTAIN_FD_ENABLE
        closeVideoDeviceFd(dev_node_name);
#endif
    }
    return result;
}

int CameraVirtualDevice::getVideoDeviceFd(char* dev_node_name) {
    CAMHAL_LOGD("get %s node fd", dev_node_name);
    int fd = -1;
    if (strcmp(dev_node_name, HDMI_VDIN_VIDEO_PATH) == 0) {
        fd = open(dev_node_name, O_RDWR | O_NONBLOCK);
        CAMHAL_LOGD("line: %d open hdmi node, fd = %d", __LINE__, fd);
    } else {
        auto it = std::find_if(std::begin(mipivideoDeviceslists), std::end(mipivideoDeviceslists),
            [&dev_node_name](const VirtualDevice mipi_dev) {
                if (strcmp(dev_node_name, mipi_dev.name) == 0)
                    return true; else return false;});
        if (it != std::end(mipivideoDeviceslists)) {
            if (it->fileDesc[0] == -1) {
                it->fileDesc[0] = open(dev_node_name, O_RDWR);
                CAMHAL_LOGD("line: %d open %s node, fd = %d", __LINE__, dev_node_name, it->fileDesc[0]);
            }
            fd = it->fileDesc[0];
        } else {
            auto it = std::find_if(std::begin(usbvideoDevices), std::end(usbvideoDevices),
                [&dev_node_name](const VirtualDevice usb_dev) {
                    if (strcmp(dev_node_name, usb_dev.name) == 0)
                        return true; else return false;});
            if (it != std::end(usbvideoDevices)) {
                if (it->fileDesc[0] == -1) {
                    it->fileDesc[0] = open(dev_node_name, O_RDWR);
                    CAMHAL_LOGD("line: %d open %s node, fd = %d", __LINE__, dev_node_name, it->fileDesc[0]);
                }
                fd = it->fileDesc[0];
            } else {
                CAMHAL_LOGD("line: %d invalid dev name", __LINE__);
            }
        }
    }
    return fd;
}

void CameraVirtualDevice::closeVideoDeviceFd(char* dev_name) {
    CAMHAL_LOGD("delete %s node", dev_name);
    /*close mipi fd*/
    auto it = std::find_if(std::begin(mipivideoDeviceslists), std::end(mipivideoDeviceslists),
        [&dev_name](const VirtualDevice mipi_dev) {
        if (strcmp(dev_name, mipi_dev.name) == 0)
            return true; else return false;});
    if (it != std::end(mipivideoDeviceslists)) {
        CAMHAL_LOGD("line %d close %s node fd", __LINE__, dev_name);
        close(it->fileDesc[0]);
        it->fileDesc[0] = -1;
        return;
    } else {
        /*close usb fd*/
        auto it = std::find_if(std::begin(usbvideoDevices), std::end(usbvideoDevices),
            [&dev_name](const VirtualDevice usb_dev) {
            if (strcmp(dev_name, usb_dev.name) == 0)
                return true; else return false;});
        if (it != std::end(usbvideoDevices)) {
            CAMHAL_LOGD("line: %d close %s node fd", __LINE__, dev_name);
            close(it->fileDesc[0]);
            it->fileDesc[0] = -1;
            return;
        } else {
            CAMHAL_LOGD("line: %d invalid dev name", __LINE__);
        }
    }
}

int CameraVirtualDevice::getLegacyCameraNum() {
    int iCamerasNum = 0;

    for (int i = 0; i < MIPI_DEVICE_NUM; i++ ) {
        struct VirtualDevice* pDev = &mipivideoDeviceslists[i];
        int ret = access(pDev->name, F_OK | R_OK | W_OK);
        if ( 0 == ret)
        {
            CAMHAL_LOGD("access %s success\n", pDev->name);
            if (pDev->type == V4L2MEDIA_CAM_DEV) {
                // for media device. skip usb cameras' media dev node.
                if ( false == isAmlMediaCamera(pDev->name) ) {
                    // skip
#ifdef MAINTAIN_FD_ENABLE
                    closeVideoDeviceFd(pDev->name);
#endif
                    continue;
                }
            }

            for (int stream_idx = 0; stream_idx < pDev->streamNum; stream_idx++) {
                if (pDev->status[stream_idx] != NONE_DEVICE) {
                    CAMHAL_LOGD("device %s stream %d \n", pDev->name,stream_idx);
                    iCamerasNum++;
                }
            }
        } else {
            CAMHAL_LOGD(" %s, access failed. ret %d \n", pDev->name, ret);
        }
    }

    pluggedMipiCameraNum = iCamerasNum;
    return iCamerasNum;
}
}

