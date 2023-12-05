#ifndef HW_EMULATOR_CAMERA3_V4L2MEDIASENSOR_H
#define HW_EMULATOR_CAMERA3_V4L2MEDIASENSOR_H

#include "Sensor.h"
#include "MIPICameraIO.h"
#include "CameraUtil.h"

#include "CameraDevice.h"

#include "ICapture.h"
#include "IonIf.h"

#ifdef GDC_ENABLE
#include "gdcUseMemcpy.h"
#include "gdcUseFd.h"
#include "IGdc.h"
#endif

#include "ispMgr/ispMgr.h"

#define FRAME_DURATION (33333333L) // 1/30 s
namespace android {
    class V4l2MediaSensor:public Sensor {
        public:
            V4l2MediaSensor();
            ~V4l2MediaSensor();
        public:
            status_t streamOff(channel ch) override;
            status_t startUp(int idx, bool customizationSensor) override;
            status_t shutDown(void) override;
            //when take picture we may change image format
            void takePicture(StreamBuffer& b, uint32_t gain, uint32_t stride);
            void captureRGB(uint8_t *img, uint32_t gain, uint32_t stride) override;
            void mediaCaptureRGBA(StreamBuffer b, uint32_t gain, uint32_t stride);
            void captureNV21(StreamBuffer b, uint32_t gain) override;
            void captureYV12(StreamBuffer b, uint32_t gain) override;
            void captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride) override;
            status_t getOutputFormat(void) override;
            status_t setOutputFormat(int width, int height, int pixelformat, channel ch) override;
            int halFormatToSensorFormat(uint32_t pixelfmt) override;
            status_t getSupportChannels(std::vector<channel> &chs) override;
            status_t streamOn(channel chn) override;
            bool isStreaming() override;
            bool isPicture() {return mVinfo->Picture_status();}
            bool isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch) override;
            int getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) override;
            int getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag) override;
            int64_t getMinFrameDuration() override;
            int getPictureSizes(int32_t picSizes[], int size, bool preview) override;
            status_t force_reset_sensor() override;
            int captureNewImage() override;
            //-------dummy function-------
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
            uint32_t getStreamUsage(aml_camera_stream_t& stream) override;
            enum StreamState {
                STREAM_NOT_CREATED = 0,
                STREAM_CREATED,
                STREAM_INITED,
                STREAM_CONFIGURED
            };
        private:
            CameraVirtualDevice* mCameraVirtualDevice;
            int mMediaDevicefd;
            void * mMediaStream;
            stream_configuration_t mStreamconfig;
            int mStreamState;
            sp<IspMgr> mIspMgr;
            //store the v4l2 info
            MIPIVideoInfo *mVinfo;
            MIPIVideoInfo *mExtVinfo;
            uint8_t* mImage_buffer;

            uint32_t mFps;

            bool enableZsl;
            int enableHdr;
            ICapture* mCapture;

            struct bufInfo {
                uint8_t*   vaddr;
                uint32_t   width;
                uint32_t   stride;
                uint32_t   height;
                int        fd;
                uint32_t   fmt;
            };

            bufInfo  mSavedDecodedBuffer;

            uint32_t mMaxWidth;
            uint32_t mMaxHeight;
#ifdef GE2D_ENABLE
            IONInterface* mION;
            ge2dTransform* mGE2D;
#endif
#ifdef GDC_ENABLE
            IGdc* mIGdc;
            bool mIsGdcInit;
#endif
        private:
            int camera_open(int idx);
            void camera_close(void);
            void InitVideoInfo(int idx);
            int SensorInit(int idx);
            void setIOBufferNum();
            void captureRAW();

    protected:
            virtual status_t readyToRun();
    };
}

#endif //HW_EMULATOR_CAMERA3_V4L2MEDIASENSOR_H

