#ifndef _CAMERA_DEVICE_H_
#define _CAMERA_DEVICE_H_
#include <string>
#include <vector>

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
        bool isHdmiVdinCameraEnable();
    private:
        static struct VirtualDevice usbvideoDevices[4];

        static struct VirtualDevice usbvideoDeviceslists[4];
        static struct VirtualDevice mipivideoDeviceslists[7];

        static CameraVirtualDevice* mInstance;
        int    pluggedMipiCameraNum;
};

#endif

