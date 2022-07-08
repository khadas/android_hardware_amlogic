#ifndef _CAMERA_DEVICE_H_
#define _CAMERA_DEVICE_H_
#include <string>
#include <vector>

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
	V4L2MEDIA_CAM_DEV
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

#define DEVICE_NUM (10)
#define ISP_DEVICE (4)
class CameraVirtualDevice {
    public:
        int openVirtualDevice(int id);
        struct VirtualDevice* getVirtualDevice(int id);
        int releaseVirtualDevice(int id,int fd);
        static CameraVirtualDevice* getInstance();
        int getCameraNum();
        int checkDeviceExist(char* name);
        int returnDeviceId(char* name);
        void recoverDevicelists(void);
    private:
        CameraVirtualDevice();
        struct VirtualDevice* findVideoDevice(int id);
        int checkDeviceStatus(struct VirtualDevice* pDev);
        int OpenVideoDevice(struct VirtualDevice* pDev);
        int CloseVideoDevice(struct VirtualDevice* pDev);
        int findCameraID(int id);
        bool isAmlMediaCamera (char *dev_node_name);
    private:
        static struct VirtualDevice videoDevices[10];
        static struct VirtualDevice videoDeviceslists[10];
        static CameraVirtualDevice* mInstance;
};


#endif
