#ifndef _CAMERA_DEVICE_H_
#define _CAMERA_DEVICE_H_
#include <string>
#include <vector>
#include <HDMIStatus.h>

namespace android {
#if BUILD_KERNEL_4_9 == true
#define MAX_USB_CAM_VIDEO_ID  3
#else
#define MAX_USB_CAM_VIDEO_ID  6
#endif

enum deviceStatus_t{
    FREED_VIDEO_DEVICE = 0,
    USED_VIDEO_DEVICE,
    FREED_META_DEVICE,
    USED_META_DEVICE,
    NONE_DEVICE
};

typedef enum  deviceType {
	USB_CAM_DEV = 0,
	MIPI_CAM_DEV,
	V4L2MEDIA_CAM_DEV,
	HDMI_CAM_DEV,
} deviceType_t;

typedef enum videoDevBeginNum {
    ISP_CAM_VIDEO_DEV_BEGIN_NUM = 50,
    MIPI_ONLY_CAM_VIDEO_DEV_BEGIN_NUM = 60,
    HDMI_VDIN_DEV_BEGIN_NUM = 70,
} videoDevBeginNum_t;

struct VirtualDevice {
    char name[64];
    int streamNum;
    deviceStatus_t status[3];
    int cameraId[3];
    int fileDesc[3];
    int deviceID;
    deviceType_t type;
};

class CameraVirtualDevice {
    public:
        int openVirtualDevice(int cam_id);
        struct VirtualDevice* getVirtualDevice(int cam_id);
        int releaseVirtualDevice(int cam_id,int fd);

        static CameraVirtualDevice* getInstance();
        int getCameraNum();

        int checkUsbDeviceExist(char* name);
        int returnUsbDeviceId(char* name);
        void recoverUsbDevicelists(void);
    private:
        CameraVirtualDevice();
        struct VirtualDevice* findVideoDevice(int cam_id);
        struct VirtualDevice* findMipiVideoDevice(int cam_id);
        struct VirtualDevice* findUsbVideoDevice(int cam_id);

        int checkDeviceStatus(struct VirtualDevice* pDev);
        int OpenVideoDevice(struct VirtualDevice* pDev);
        int CloseVideoDevice(struct VirtualDevice* pDev);

        int findUsbCameraID(int cam_id);
        bool isAmlMediaCamera (char *dev_node_name);
        bool isStandardUSBCamera (char *dev_node_name);
    private:
        static struct VirtualDevice usbvideoDevices[5];

        static struct VirtualDevice usbvideoDeviceslists[5];
        static struct VirtualDevice mipivideoDeviceslists[6];

        static CameraVirtualDevice* mInstance;
        int    pluggedMipiCameraNum;
};
}

#endif

