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

struct VirtualDevice {
    char name[64];
    int streamNum;
    deviceStatus_t status[3];
    int cameraId[3];
    int fileDesc[3];
	int deviceID;
};

#define DEVICE_NUM (6)
#define ISP_DEVICE (4)
class CameraVirtualDevice {
    public:
        int openVirtualDevice(int id);
        int releaseVirtualDevice(int id,int fd);
        static CameraVirtualDevice* getInstance();
        int getCameraNum();
        int checkDeviceExist(char* name);
        int returnDeviceId(char* name);
    private:
        CameraVirtualDevice();
        struct VirtualDevice* findVideoDevice(int id);
        int checkDeviceStatus(struct VirtualDevice* pDev);
        int OpenVideoDevice(struct VirtualDevice* pDev);
        int CloseVideoDevice(struct VirtualDevice* pDev);
        int findCameraID(int id);
    private:
        static struct VirtualDevice videoDevices[8];
        static CameraVirtualDevice* mInstance;
};


#endif
