#ifndef HW_EMULATOR_CAMERA3_HDMISENSOR_H
#define HW_EMULATOR_CAMERA3_HDMISENSOR_H

#include "Sensor.h"
#include "CameraDevice.h"
#include "MPlaneCameraIO.h"
#include "ge2d_stream.h"
#include "tvinType.h"

namespace android {

    class HDMISensor:public Sensor        {
        public:
            HDMISensor();
            ~HDMISensor();
            status_t startUp(int idx) override;
            status_t shutDown(void) override;
            int getOutputFormat() override;
            status_t streamOn(channel ch) override;
            status_t streamOff(channel ch) override;
            int getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) override;
            int getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag) override;
            int64_t getMinFrameDuration() override;
            status_t setOutputFormat(int width, int height, int pixelformat, channel ch) override;
            void captureNV21(StreamBuffer b, uint32_t gain) override;
            int captureNewImage() override;
            bool isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch) override;
            int halFormatToSensorFormat(uint32_t pixelfmt) override;

            int getZoom(int *zoomMin, int *zoomMax, int *zoomStep) override;
            int setZoom(int zoomValue) override;
            status_t setEffect(uint8_t effect) override;
            int getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step) override;
            status_t setExposure(int expCmp) override;
            int getAntiBanding(uint8_t *antiBanding, uint8_t maxCont) override;
            status_t setAntiBanding(uint8_t antiBanding) override;
            status_t setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1) override;
            int getAutoFocus(uint8_t *afMode, uint8_t maxCount) override;
            status_t setAutoFocus(uint8_t afMode) override;
            int getAWB(uint8_t *awbMode, uint8_t maxCount) override;
            status_t setAWB(uint8_t awbMode) override;
            void setSensorListener(SensorListener *listener) override;
            uint32_t getStreamUsage(int stream_type) override;
        private:
            MPlaneCameraIO* mMPlaneCameraIO;
            ge2dTransform* mGE2D = NULL;
            int kernel_dma_fd = -1;
            int vdin_fd = -1;
            bool successStreamOn;
            bool isStableSignal();
    };
}
#endif

