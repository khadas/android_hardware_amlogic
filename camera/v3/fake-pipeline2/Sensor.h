/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * This class is a simple simulation of a typical CMOS cellphone imager chip,
 * which outputs 12-bit Bayer-mosaic raw images.
 *
 * Unlike most real image sensors, this one's native color space is linear sRGB.
 *
 * The sensor is abstracted as operating as a pipeline 3 stages deep;
 * conceptually, each frame to be captured goes through these three stages. The
 * processing step for the sensor is marked off by vertical sync signals, which
 * indicate the start of readout of the oldest frame. The interval between
 * processing steps depends on the frame duration of the frame currently being
 * captured. The stages are 1) configure, 2) capture, and 3) readout. During
 * configuration, the sensor's registers for settings such as exposure time,
 * frame duration, and gain are set for the next frame to be captured. In stage
 * 2, the image data for the frame is actually captured by the sensor. Finally,
 * in stage 3, the just-captured data is read out and sent to the rest of the
 * system.
 *
 * The sensor is assumed to be rolling-shutter, so low-numbered rows of the
 * sensor are exposed earlier in time than larger-numbered rows, with the time
 * offset between each row being equal to the row readout time.
 *
 * The characteristics of this sensor don't correspond to any actual sensor,
 * but are not far off typical sensors.
 *
 * Example timing diagram, with three frames:
 *  Frame 0-1: Frame duration 50 ms, exposure time 20 ms.
 *  Frame   2: Frame duration 75 ms, exposure time 65 ms.
 * Legend:
 *   C = update sensor registers for frame
 *   v = row in reset (vertical blanking interval)
 *   E = row capturing image data
 *   R = row being read out
 *   | = vertical sync signal
 *time(ms)|   0          55        105       155            230     270
 * Frame 0|   :configure : capture : readout :              :       :
 *  Row # | ..|CCCC______|_________|_________|              :       :
 *      0 |   :\          \vvvvvEEEER         \             :       :
 *    500 |   : \          \vvvvvEEEER         \            :       :
 *   1000 |   :  \          \vvvvvEEEER         \           :       :
 *   1500 |   :   \          \vvvvvEEEER         \          :       :
 *   2000 |   :    \__________\vvvvvEEEER_________\         :       :
 * Frame 1|   :           configure  capture      readout   :       :
 *  Row # |   :          |CCCC_____|_________|______________|       :
 *      0 |   :          :\         \vvvvvEEEER              \      :
 *    500 |   :          : \         \vvvvvEEEER              \     :
 *   1000 |   :          :  \         \vvvvvEEEER              \    :
 *   1500 |   :          :   \         \vvvvvEEEER              \   :
 *   2000 |   :          :    \_________\vvvvvEEEER______________\  :
 * Frame 2|   :          :          configure     capture    readout:
 *  Row # |   :          :         |CCCC_____|______________|_______|...
 *      0 |   :          :         :\         \vEEEEEEEEEEEEER       \
 *    500 |   :          :         : \         \vEEEEEEEEEEEEER       \
 *   1000 |   :          :         :  \         \vEEEEEEEEEEEEER       \
 *   1500 |   :          :         :   \         \vEEEEEEEEEEEEER       \
 *   2000 |   :          :         :    \_________\vEEEEEEEEEEEEER_______\
 */

#ifndef HW_EMULATOR_CAMERA2_SENSOR_H
#define HW_EMULATOR_CAMERA2_SENSOR_H

#include "utils/Thread.h"
#include "utils/Mutex.h"
#include "utils/Timers.h"
#include <utils/String8.h>

#include "Scene.h"
//#include "Base.h"
#include "camera_hw.h"
#include <cstdlib>

#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include <utils/Errors.h>

#include "SensorTypes.h"

namespace android {

#define IOCTL_MASK_ROTATE    (1<<0)

#define MAX_WIDTH  (1920)
#define MAX_HEIGHT (1080)

struct requestParameter {
    nsecs_t requestExposureTime;
    nsecs_t requestFrameDuration;
    uint32_t requestGain;
    Buffers *requestBuffers;
    uint32_t requestFrameNumber;
};

struct streamInfo {
    uint32_t mPixelformat;
    uint32_t mWidth;
    uint32_t mHeight;
    streamInfo(uint32_t pixelformat, uint32_t width, uint32_t height) : mPixelformat(pixelformat), mWidth(width), mHeight(height) {}
};

class Sensor: public Thread, public virtual RefBase {
  public:

    Sensor();
    ~Sensor();

    /*
     * Power control
     */
    void sendExitSingalToSensor();
    virtual status_t startUp(int idx, bool customizationSensor = false);
    virtual status_t shutDown();

    virtual int getOutputFormat();
    virtual int halFormatToSensorFormat(uint32_t pixelfmt);
    virtual status_t setOutputFormat(int width, int height, int pixelformat, channel ch);
    void setPictureRotate(int rotate);
    inline void setFacing(bool __facingback) {     mFacingBack = __facingback; }
    int getPictureRotate();
    virtual uint32_t getStreamUsage(aml_camera_stream_t& stream);
    virtual status_t getSupportChannels(std::vector<channel> &chs);
    virtual status_t streamOn(channel ch);
    virtual status_t streamOff(channel ch);

    virtual int getPictureSizes(int32_t picSizes[], int size, bool preview);
    virtual int getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size);
    virtual int64_t getMinFrameDuration();
    virtual int getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag);
    virtual bool isStreaming();
    virtual bool isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch);
    void dump(int fd);
    /*
     * Access to scene
     */
    Scene &getScene();

    /*
     * Controls that can be updated every frame
     */

    bool isUnpluged();
    virtual int getZoom(int *zoomMin, int *zoomMax, int *zoomStep);
    virtual int setZoom(int zoomValue);
    virtual int getExposure(int *mamExp, int *minExp, int *def, camera_metadata_rational *step);
    virtual status_t setExposure(int expCmp);
    virtual status_t setEffect(uint8_t effect);
    virtual int getAntiBanding(uint8_t *antiBanding, uint8_t maxCont);
    virtual status_t setAntiBanding(uint8_t antiBanding);
    virtual status_t setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1);
    virtual int getAWB(uint8_t *awbMode, uint8_t maxCount);
    virtual status_t setAWB(uint8_t awbMode);
    virtual status_t setAutoFocus(uint8_t afMode);
    virtual int getAutoFocus(uint8_t *afMode, uint8_t maxCount);
    void setRequestParameter(requestParameter &param);
    void setPictureRequest(Request &PicRequest);
    void setTestPatternMode(int32_t testPatternMode);
    void  setFlushFlag(bool flushFlag);
    void setDeviceName(char* name);
    virtual status_t force_reset_sensor();
    bool get_sensor_status();
    virtual bool isNeedDump();
    virtual status_t checkAndRestartStream(
            uint32_t width, uint32_t height,
            uint32_t pixelfmt, channel ch) { return -1; }
    /*
     * Controls that cause reconfiguration delay
     */

    void setBinning(int horizontalFactor, int verticalFactor);

    /*
     * Synchronizing with sensor operation (vertical sync)
     */

    // Wait until the sensor outputs its next vertical sync signal, meaning it
    // is starting readout of its latest frame of data. Returns true if vertical
    // sync is signaled, false if the wait timed out.
    status_t waitForVSync(nsecs_t reltime);

    // Wait until a new frame has been read out, and then return the time
    // capture started.  May return immediately if a new frame has been pushed
    // since the last wait for a new frame. Returns true if new frame is
    // returned, false if timed out.
    status_t waitForNewFrame(nsecs_t reltime,
            nsecs_t *captureTime);

    /*
     * Interrupt event servicing from the sensor. Only triggers for sensor
     * cycles that have valid buffers to write to.
     */
    struct SensorListener {
        enum Event {
            EXPOSURE_START, // Start of exposure
            ERROR_CAMERA_DEVICE,
        };

        virtual void onSensorEvent(uint32_t frameNumber, Event e,
                nsecs_t timestamp, nsecs_t readoutTimestamp) = 0;
        virtual void onSensorPicJpeg(Request &r) = 0;
        virtual ~SensorListener();
    };

    virtual void setSensorListener(SensorListener *listener);

    /**
     * Static sensor characteristics
     */
    static const unsigned int kResolution[2];

    static const nsecs_t kExposureTimeRange[2];
    static const nsecs_t kFrameDurationRange[2];
    static const nsecs_t kMinVerticalBlank;

    static const uint8_t kColorFilterArrangement;

    // Output image data characteristics
    static const uint32_t kMaxRawValue;
    static const uint32_t kBlackLevel;
    // Sensor sensitivity, approximate

    static const float kSaturationVoltage;
    static const uint32_t kSaturationElectrons;
    static const float kVoltsPerLuxSecond;
    static const float kElectronsPerLuxSecond;

    static const float kBaseGainFactor;

    static const float kReadNoiseStddevBeforeGain; // In electrons
    static const float kReadNoiseStddevAfterGain;  // In raw digital units
    static const float kReadNoiseVarBeforeGain;
    static const float kReadNoiseVarAfterGain;

    // While each row has to read out, reset, and then expose, the (reset +
    // expose) sequence can be overlapped by other row readouts, so the final
    // minimum frame duration is purely a function of row readout time, at least
    // if there's a reasonable number of rows.
    static const nsecs_t kRowReadoutTime;

    static const int32_t kSensitivityRange[2];
    static const uint32_t kDefaultSensitivity;

    uint8_t uBuffer[MAX_WIDTH * MAX_HEIGHT /4];
    uint8_t vBuffer[MAX_WIDTH * MAX_HEIGHT /4];

    sensor_type_e getSensorType(void);

    sensor_face_type_e mSensorFace;

  protected:
    Mutex mControlMutex; // Lock before accessing control parameters
    // Start of control parameters
    Condition mVSync;
    bool      mGotVSync;
    uint64_t  mExposureTime;
    uint64_t  mFrameDuration;
    uint32_t  mGainFactor;
    Buffers  *mNextBuffers;
    uint8_t  *mKernelBuffer;
    int mKernelBufferFmt;
    int mTempFD;
    uintptr_t mKernelPhysAddr;
    uint32_t  mFrameNumber;
    int  mRotateValue;
    // End of control parameters

    int mEV;

    Mutex mReadoutMutex; // Lock before accessing readout variables
    // Start of readout variables
    Condition mReadoutAvailable;
    Condition mReadoutComplete;
    Buffers  *mCapturedBuffers;
    nsecs_t   mCaptureTime;
    SensorListener *mListener;
    // End of readout variables

    uint8_t *mTemp_buffer;
    bool mExitSensorThread;

    // Time of sensor startup, used for simulation zero-time point
    nsecs_t mStartupTime;

    //store the v4l2 info
    struct VideoInfo *vinfo;

    struct timeval mTimeStart, mTimeEnd;
    struct timeval mTestStart, mTestEnd;

    uint32_t mFramecount;
    float mCurFps;
    bool mLowLatencyMode;

    struct DecoderTask {
        mutable std::mutex lock;
        std::condition_variable condition;
        uint8_t *inputBuffer = nullptr;
        uint32_t inputWidth, inputHeight, inputBytesused;
        uint32_t outputWidth, outputHeight, outputStride;
        uint8_t *workingBuffer = nullptr;
        uint8_t *validBuffer = nullptr;
        bool taskRunning = false;
        bool exitThread = false;
        bool bDecoderFlag;
    };
    struct DecoderTask mDecoderTask;
    std::thread mDecoderThread;
    uint8_t mInputBuffer[1920*1080*3/2];
    uint8_t mRingBuffer1[1920*1080*3/2];
    uint8_t mRingBuffer2[1920*1080*3/2];
    uint8_t vBuffer2[1920*1080/4];
    uint8_t uBuffer2[1920*1080/4];
    bool needReturnVinfo = true;
    enum sensor_type_e mSensorType;
    unsigned int mIoctlSupport;
    uint32_t mTimeOutCount;
    uint32_t mSelectCount;
    bool mWait;
    uint32_t mPre_width;
    uint32_t mPre_height;
    bool mFlushFlag;
    bool mSensorWorkFlag;
    int mOpenCameraID;
    char mDeviceName[64];
    bool mNeedCheckMjpeg;
    bool mDPTZEnable;
    uint32_t checkFailCount;

    struct PictureThreadCntler {
        std::thread *PictureThread;
        Vector<Request> NextPictureRequest;
        bool PictureThreadExit;
        Mutex requestOperationLock;
        Condition unprocessedRequest;
        static void resetAndInit(struct PictureThreadCntler &c) {
            c.PictureThread = NULL;
            c.PictureThreadExit = false;
            c.NextPictureRequest.setCapacity(30);
        }
        static void stopAndRelease(struct PictureThreadCntler &c) {
            if (c.PictureThread != NULL) {
                c.PictureThreadExit = true;
                c.unprocessedRequest.signal();
                c.PictureThread->join();
                delete c.PictureThread;
                c.PictureThread = NULL;
                if (!c.NextPictureRequest.empty()) {
                  for(auto iter = c.NextPictureRequest.begin();
                        iter != c.NextPictureRequest.end(); iter++) {
                        if (iter->buffers != NULL) {
                            delete iter->buffers;
                            iter->buffers = NULL;
                        }
                        if (iter->sensorBuffers != NULL) {
                            delete iter->sensorBuffers;
                            iter->sensorBuffers = NULL;
                        }
                   }
                   c.NextPictureRequest.clear();
               }
            }
        }
    } mPictureThreadCntler;
    /**
     * Inherited Thread virtual overrides, and members only used by the
     * processing thread
     */
  protected:
    virtual status_t readyToRun();

    virtual bool threadLoop();

    static status_t decoderThread(void* user);
    nsecs_t mNextCaptureTime;
    Buffers *mNextCapturedBuffers;

    Scene mScene;
    bool mUnpluged;
    bool mFacingBack;
    int32_t mTestPatternMode;
    virtual int captureNewImage();
    void captureRaw(uint8_t *img, uint32_t gain, uint32_t stride);
    virtual void captureRGBA(uint8_t *img, uint32_t gain, uint32_t stride);
    virtual void captureRGB(uint8_t *img, uint32_t gain, uint32_t stride);
    virtual void captureNV21(StreamBuffer b, uint32_t gain);
    virtual void captureYV12(StreamBuffer b, uint32_t gain);
    virtual void captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride);
  private:
    void YUYVToNV21(uint8_t *src, uint8_t *dst, int width, int height);
    void YUYVToYV12(uint8_t *src, uint8_t *dst, int width, int height);
};

}

#endif // HW_EMULATOR_CAMERA2_SENSOR_H
