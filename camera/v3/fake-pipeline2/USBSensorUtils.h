
#ifndef HW_EMULATOR_CAMERA3_USBSENSOR_UTILS_H
#define HW_EMULATOR_CAMERA3_USBSENSOR_UTILS_H

#include <stdint.h>

#include <utils/Errors.h>
#include <system/camera_metadata.h>


namespace android {

class CVideoInfo;

class USBSensorUtils final {

public:
	USBSensorUtils(CVideoInfo *vinfo) {mVinfo = vinfo;mEV = 0;}
	~USBSensorUtils(){}

	int getZoom(int *zoomMin, int *zoomMax, int *zoomStep);
	int setZoom(int zoomValue);
	status_t setEffect(uint8_t effect);
	int getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step);
	status_t setExposure(int expCmp);
	int getAntiBanding(uint8_t *antiBanding, uint8_t maxCont);
	status_t setAntiBanding(uint8_t antiBanding);
	status_t setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
	int getAutoFocus(uint8_t *afMode, uint8_t maxCount);
	status_t setAutoFocus(uint8_t afMode);
	int getAWB(uint8_t *awbMode, uint8_t maxCount);
	status_t setAWB(uint8_t awbMode);

	const char* getformtStr(int id);

private:
	CVideoInfo *mVinfo;

private:
	int mEV;
};

}

#endif

