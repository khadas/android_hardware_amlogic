#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "HDMISensor"
#define HDMI_PORT_INDEX 1
#if defined(LOG_NNDEBUG) && LOG_NNDEBUG == 0
#define ALOGVV ALOGV
#else
#define ALOGVV(...) ((void)0)
#endif

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "../EmulatedFakeCamera3.h"
#include "Sensor.h"
#include "HDMISensor.h"
#if ANDROID_PLATFORM_SDK_VERSION >= 24
#if ANDROID_PLATFORM_SDK_VERSION >= 29
#include <gralloc1.h>
#else
#include <gralloc_usage_ext.h>
#endif
#endif

#if ANDROID_PLATFORM_SDK_VERSION >= 28
#include <amlogic/am_gralloc_ext.h>
#endif

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

namespace android {
HDMISensor::HDMISensor() {
    mMPlaneCameraIO = NULL;
    mGE2D = new ge2dTransform();
    kernel_dma_fd = -1;
    successStreamOn = false;
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.media.hdmi.vdin.port", property, "1");
    hdmi_port_index = atoi(property);
    if (hdmi_port_index > 3 || hdmi_port_index <= 0) {
        ALOGE("invalid port set default port1");
        hdmi_port_index = 1;
    }
}
HDMISensor::~HDMISensor() {
    if (mMPlaneCameraIO) {
        free(mMPlaneCameraIO);
        mMPlaneCameraIO = NULL;
    }
    if (mGE2D) {
        delete mGE2D;
        mGE2D = NULL;
    }
}

int HDMISensor::halFormatToSensorFormat(uint32_t pixelfmt)
{
    ALOGD("get sensor output format");
    return V4L2_PIX_FMT_NV21;
}


uint32_t HDMISensor::getStreamUsage(camera3_stream_t& stream)
{
    ATRACE_CALL();
    uint32_t usage = (GRALLOC_USAGE_HW_TEXTURE
            | GRALLOC_USAGE_HW_RENDER
            | GRALLOC_USAGE_SW_READ_MASK
            | GRALLOC_USAGE_SW_WRITE_MASK
            );
    usage = GRALLOC1_PRODUCER_USAGE_CAMERA | usage;
    ALOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;
}

int HDMISensor::getOutputFormat() {
    return V4L2_PIX_FMT_NV21;
}

int HDMISensor::streamOn(channel ch) {
    bool waitStable = true;
    int waitCount = 0;
    while (!isStableSignal()) {
        if (waitCount++ >= 2000) {
            waitStable = false;
            break;
        }
        usleep(5000);
    }
    if ((waitStable && (mMPlaneCameraIO->startCameraIO() < 0))
        || (!waitStable)) {
            successStreamOn = false;
            return -1;
    } else {
        ALOGE("HDMI success streamOn");
        successStreamOn = true;
        return 0;
    }
}

int HDMISensor::streamOff(channel ch) {
    return mMPlaneCameraIO->stopCameraIO();
}

bool HDMISensor::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch)
{
    if ((mMPlaneCameraIO->format.fmt.pix_mp.width != width)
        ||(mMPlaneCameraIO->format.fmt.pix_mp.height != height)
        ) {

        return true;

    }

    return false;
}


status_t HDMISensor::startUp(int idx, bool customizationSensor) {
    ATRACE_CALL();
    ALOGV("%s: E", __FUNCTION__);
    DBG_LOGA("ddd");

    int res;
    mCapturedBuffers = NULL;
    mOpenCameraID = idx;
    res = run("EmulatedFakeCamera3::HDMISensor",
            ANDROID_PRIORITY_URGENT_DISPLAY);

    if (res != OK) {
        ALOGE("Unable to start up sensor capture thread: %d", res);
    }
    if (customizationSensor)
        return res;
    mMPlaneCameraIO = (MPlaneCameraIO *) calloc(1, sizeof(MPlaneCameraIO));
    mMPlaneCameraIO->openIdx = idx;

    res = mMPlaneCameraIO->openCamera();
    if (res < 0) {
        ALOGE("Unable to open sensor %d, errno=%d\n", mMPlaneCameraIO->openIdx, res);
    }

    hdmi_port_index = HDMI_PORT_INDEX;
    res = mMPlaneCameraIO->setInputPort(&hdmi_port_index);
    if (res < 0) {
        ALOGE("Unable set input HDMI3_RX3");
    }

    vdin_fd = open("/dev/vdin0", O_RDWR | O_NONBLOCK);
    if (vdin_fd < 0) {
        ALOGE("HDMISensor open vdin0 fail %s", strerror(errno));
    }

    return res;

}

status_t HDMISensor::shutDown() {
    ALOGV("%s: E", __FUNCTION__);
    int res;
    mTimeOutCount = 0;
    res = requestExitAndWait();
    if (res != OK) {
        ALOGE("Unable to shut down sensor capture thread: %d", res);
    }
    if (mMPlaneCameraIO != NULL) {
        mMPlaneCameraIO->stopCameraIO();
        mMPlaneCameraIO->closeCamera();
    }

    mSensorWorkFlag = false;
    ALOGD("%s: Exit", __FUNCTION__);
    return res;
}

bool HDMISensor::isStableSignal() {
    bool tvin_stable = true;
    tvin_info_s signal_info;
    memset(&signal_info, 0, sizeof(tvin_info_s));
    int ret = ioctl(vdin_fd, TVIN_IOC_G_SIG_INFO, &signal_info);
    if (ret < 0) {
        ALOGE("TVIN_IOC_G_SIG_INFO is not stable %d",ret);
        tvin_stable = false;
    } else {
        tvin_stable = (signal_info.status == TVIN_SIG_STATUS_STABLE);
    }
    return tvin_stable;
}

int HDMISensor::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) {
    uint32_t count = 0;
    picSizes[count++] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    picSizes[count++] = 1920;
    picSizes[count++] = 1080;
    picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    picSizes[count++] = HAL_PIXEL_FORMAT_YCbCr_420_888;
    picSizes[count++] = 1920;
    picSizes[count++] = 1080;
    picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    picSizes[count++] = HAL_PIXEL_FORMAT_BLOB;
    picSizes[count++] = 1920;
    picSizes[count++] = 1080;
    picSizes[count++] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
    return (int)count;
}

int HDMISensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag)
{
    uint32_t count = 0;
    duration[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
    duration[count+1] = 1920;
    duration[count+2] = 1080;
    duration[count+3] = (int64_t)16666666L;
    count += 4;
    duration[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
    duration[count+1] = 1920;
    duration[count+2] = 1080;
    duration[count+3] = (int64_t)16666666L;
    count += 4;
    duration[count+0] = HAL_PIXEL_FORMAT_BLOB;
    duration[count+1] = 1920;
    duration[count+2] = 1080;
    duration[count+3] = (int64_t)16666666L;
    count += 4;
    return (int)count;
}

int64_t HDMISensor::getMinFrameDuration() {
    int64_t minFrameDuration =  1000000000L/60L ; // 30fps
    ALOGW("%s to be implemented, minframeduration  %" PRId64 "\n", __func__, minFrameDuration);
    return minFrameDuration;
}

status_t HDMISensor::setOutputFormat(int width, int height, int pixelformat, channel ch) {
    int res = OK;

    mMPlaneCameraIO->format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    mMPlaneCameraIO->format.fmt.pix_mp.width       = width;
    mMPlaneCameraIO->format.fmt.pix_mp.height      = height;
    mMPlaneCameraIO->format.fmt.pix_mp.pixelformat = pixelformat;
    mMPlaneCameraIO->format.fmt.pix_mp.field       = V4L2_FIELD_ANY;
    mMPlaneCameraIO->format.fmt.pix_mp.num_planes  = 1;

    res = mMPlaneCameraIO->setOutputFormat();
    if (res < 0) {
        ALOGE("set buffer failed\n");
    }
    return res;
}

void HDMISensor::captureNV21(StreamBuffer b, uint32_t gain) {
    ATRACE_CALL();
    uint32_t width = mMPlaneCameraIO->format.fmt.pix_mp.width;
    uint32_t height = mMPlaneCameraIO->format.fmt.pix_mp.height;

    if (kernel_dma_fd != -1) {
        if (mMPlaneCameraIO->format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV21) {
            if ((width == b.width) && (height == b.height)) {
                mGE2D->ge2d_copy(b.share_fd, kernel_dma_fd, b.stride,b.height, ge2dTransform::NV12);
            } else {
                mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height, kernel_dma_fd, width, height);
            }
        }
        return;
    }
    VideoInfo output_info;
    bool dequeSuccess = false;
    while (1) {
        if (!successStreamOn)
            break;
        if (mExitSensorThread || mFlushFlag) {
            break;
        }
        if (mMPlaneCameraIO->fd <= 0)
            break;
        memset(&output_info, 0 , sizeof(output_info));
        int ret = mMPlaneCameraIO->getFrame(output_info);
        if (ret < 0) {
            ALOGE("get frame NULL, sleep 5ms");
            usleep(5000);
            mTimeOutCount++;
            if (mTimeOutCount > 600) {
                ALOGE("retry deque frame");
            }
            continue;
        }
        dequeSuccess = true;
        kernel_dma_fd = output_info.dma_fd;
        mTimeOutCount = 0;
        if (mMPlaneCameraIO->format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV21) {
            if (width == b.width && height == b.height) {
                mGE2D->ge2d_copy(b.share_fd, output_info.dma_fd, b.stride,b.height, ge2dTransform::NV12);
            } else {
                mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height, output_info.dma_fd, width, height);
            }
        }
        mSensorWorkFlag = true;
        break;
    }
    if (dequeSuccess)
        mMPlaneCameraIO->pushbackFrame(output_info.buf_idx);
    ALOGVV("NV21 sensor image captured");
}

int HDMISensor::captureNewImage() {
    uint32_t gain = mGainFactor;
    mKernelBuffer = NULL;
    mKernelBufferFmt = 0;
    mTempFD = -1;
    kernel_dma_fd = -1;
    ALOGVV("%s:buffer size=%zu\n",__FUNCTION__,mNextCapturedBuffers->size());
    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
        const StreamBuffer &b = (*mNextCapturedBuffers)[i];
        ALOGVV("Sensor capturing buffer %zu: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                i, b.streamId, b.width, b.height, b.format, b.stride,
                b.buffer, b.img);
        switch (b.format) {
            case HAL_PIXEL_FORMAT_BLOB:
                // Add auxiliary buffer of the right size
                // Assumes only one BLOB (JPEG) buffer in
                // mNextCapturedBuffers
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                ALOGD("bAux orientation=%d",orientation);

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                bAux.stride = b.width;
                bAux.buffer = NULL;
                bAux.img = IONInterface::get_instance()->alloc_buffer(b.width * b.height * 3, &bAux.share_fd);
                mNextCapturedBuffers->push_back(bAux);
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                captureNV21(b, gain);
                break;
            default:
                ALOGE("%s: Unknown format %x, no output", __FUNCTION__,
                        b.format);
                break;
        }
    }
    return 0;

}

int HDMISensor::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);

    return ret ;
}

int HDMISensor::setZoom(int zoomValue) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);

    return ret ;
}


status_t HDMISensor::setEffect(uint8_t effect) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);
    return ret ;
}

int HDMISensor::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step) {
   int ret=0;
   ALOGVV("%s not implemented yet!", __func__);
   return ret;
}

status_t HDMISensor::setExposure(int expCmp) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);
    return ret ;
}

int HDMISensor::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont) {

    int mode_count = -1;
    ALOGVV("%s not implemented yet!", __func__);

    return mode_count;
}

status_t HDMISensor::setAntiBanding(uint8_t antiBanding) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);
    return ret;
}

status_t HDMISensor::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    int ret = 0;
    ALOGVV("%s not implemented yet!", __func__);
    return ret;
}

int HDMISensor::getAutoFocus(uint8_t *afMode, uint8_t maxCount) {
    int mode_count = -1;
    ALOGVV("%s not implemented yet!", __func__);

    return mode_count;
}

status_t HDMISensor::setAutoFocus(uint8_t afMode) {
    ALOGVV("%s not implemented yet!", __func__);
    return 0;
}

int HDMISensor::getAWB(uint8_t *awbMode, uint8_t maxCount) {
    int mode_count = -1;
    ALOGVV("%s not implemented yet!", __func__);
    return mode_count;
}

status_t HDMISensor::setAWB(uint8_t awbMode) {
    ALOGVV("%s not implemented yet!", __func__);
    return 0;
}

void HDMISensor::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}

status_t HDMISensor::readyToRun() {
    //int res;
    ATRACE_CALL();
    ALOGV("Starting up hdmi sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = NULL;
    DBG_LOGA("");

    return OK;
}


}

