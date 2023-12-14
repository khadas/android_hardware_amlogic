#ifndef HW_EMULATOR_CAMERA3_USBSENSOR_HWDEC_H
#define HW_EMULATOR_CAMERA3_USBSENSOR_HWDEC_H

#include <utils/Thread.h>
#include <utils/Mutex.h>
#include <utils/List.h>

#include "Sensor.h"
#include "CameraUtil.h"
#include "HWVideoDecoder.h"
#include "CameraIO.h"
#include "CameraDevice.h"

#include "USBSensorUtils.h"

#ifdef GE2D_ENABLE
#include "ge2d_stream.h"
#endif

#include "IonIf.h"

#define FRAME_DURATION (33333333L)

namespace android {

    class USBSensorHWDec:public Sensor {
        public:
            enum DecoderStreamType{
                MJPEG_STREAM,
                H264_STREAM,
                HEVC_STREAM,
            };

            enum HWDecoderWorkMode {
                SYNC_DECODE_MODE = 0,
                ASYNC_DECODE_MODE
            };

        public:
            // 0 for all available format.
            USBSensorHWDec(int expectedV4l2OutPixFmt);
            ~USBSensorHWDec();
        public:
            status_t streamOn(channel ch) override;
            status_t streamOff(channel ch) override;
            status_t startUp(int idx, bool customizationSensor) override;
            status_t shutDown(void) override;

            status_t getOutputFormat(void) override;
            status_t getOutputFormat(int width, int height, int pixelformat);
            status_t setOutputFormat(int width, int height, int pixelformat, channel ch) override;
            int halFormatToSensorFormat(uint32_t pixelfmt) override;

            bool isStreaming() override;
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
        private:
            CameraVirtualDevice* mCameraVirtualDevice;
            int mUSBDevicefd;

            FILE *v4l2OutDumpFp;
            FILE *decOutDumpFp;

            enum Decode_Method{
                DECODE_SOFTWARE,
                DECODE_HARDWARE,
                DECODE_MAX,
            };

            int                     mExpectedV4l2OutPixFmt;

            enum DecoderStreamType  mDecoderStreamType;
            enum Decode_Method      mDecoderMethod;
            enum HWDecoderWorkMode  mHWDecoderWorkMode;
            bool mUseStreamBufVecForDecoder;
            HWVideoDecoder*         mHWDecoder;

            CameraUtil* mCameraUtil;
            Vector<uint32_t> mSupportFormat;
            Vector<uint32_t> mTryPixelFormat;

            uint32_t mCurrentFormat;
            bool mIsDecoderInit;

            //store the v4l2 info
            CVideoInfo *mVinfo;
            USBSensorUtils *mUsbSensorUtils;

            struct bufInfo {
                uint8_t*   vaddr;
                uint32_t   width;
                uint32_t   height;
                int        fd;
                uint32_t   fmt;
            };

            bufInfo  mSavedDecodedBuffer;

#ifdef GE2D_ENABLE
            IONInterface* mION;
            ge2dTransform* mGE2D;
#endif

            StreamBuffer mSensorOutBuf;

        protected:
            virtual status_t readyToRun();
        private:
            USBSensorHWDec();
            void dump(int& frame_index,uint8_t* buf, int length, std::string name);
            void initDecoder(int in_width, int in_height,
                             int out_width, int out_height, int out_bufferCount);
            int getDecOut(Vector<StreamBuffer>& b);
            int HWDecodeToNV21(uint8_t* src, uint32_t src_length, Vector<StreamBuffer>& b, bool isJpegRequest);
            void determineDecoderStreamType();
            void determineDecoderWorkMode();
            int SensorInit(int idx);
            void InitVideoInfo(int idx);
            int camera_open(int idx);
            void camera_close(void);
            void force_reset_v4l2_capture();
            const char* getformt(int id);
            void setIOBufferNum();

            int reAllocSoftwareBuffer(int width, int height);
            int checkAndGetNextSensorData(uint8_t **outDataAddr, uint32_t *outDataLen);
            int checkAndGetLatestSensorData(uint8_t **outDataAddr, uint32_t *outDataLen);
            void captureNV21UsbSensor(StreamBuffer b, uint32_t gain, bool needSensorOutBuf);
            void captureNV21UsbSensor(Vector<StreamBuffer>& b, uint32_t gain, bool isJpegRequest);
            int captureNV21UseSavedBuf(StreamBuffer &b, bufInfo * savedBuf);
            void getStreamInfo(std::vector<streamInfo> &streamInfos);
            // ====== begin async decode. used for h264 =========
public:
            void onDecodeOutBufReady(int out_fd, uint8_t* out_buf, uint32_t out_width, uint32_t out_height, uint64_t timestamp);


private:
            Mutex mDecodeOutBufMutex;
            Condition mDecodeOutBufCondition;
            StreamBuffer mDecoderOutBuf;
            // indicate if mSensorOutBuf has fresh data.
            bool   mDecodeOutBufIsFresh;

            // decode fill thread. dequeue h264 bitstream from v4l2
            // and fill it to HWVideoDecode.
            volatile bool mNeedStopDecodeFillThread;

            const static int THREAD_STATE_DEAD = 0;
            const static int THREAD_STATE_CREATED = 1;
            const static int THREAD_STATE_RUNNING = 2;

            int  mDecodeFillThreadState;
            pthread_t mDecodeFillThreadId;
            std::vector<streamInfo> mStreamInfos;
            bool isUseH264;

            int startDecodeFillThread();
            int stopDecodeFillThread();
            static void *decodeFillThreadProc(void *sensor);

            // ====== end async decode. used for h264 =========

    };
}
#endif

