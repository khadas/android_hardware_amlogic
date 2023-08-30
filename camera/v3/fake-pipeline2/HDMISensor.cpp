#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "HDMISensor"

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
#include <amlogic/am_gralloc_usage.h>
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

const usb_frmsize_discrete_t kHDMIAvailablePictureSize[] = {
        {1920, 1080},
};

static bool IsHDMIAvailablePictureSize(const usb_frmsize_discrete_t AvailablePictureSize[], uint32_t width, uint32_t height)
{
    int i;
    bool ret = false;
    int count = sizeof(kHDMIAvailablePictureSize)/sizeof(kHDMIAvailablePictureSize[0]);
    for (i = 0; i < count; i++) {
        if ((width == AvailablePictureSize[i].width) && (height == AvailablePictureSize[i].height)) {
            ret = true;
        } else {
            continue;
        }
    }
    return ret;
}

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


uint32_t HDMISensor::getStreamUsage(int stream_type)
{
    ATRACE_CALL();
    uint32_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
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


status_t HDMISensor::startUp(int idx) {
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

    mMPlaneCameraIO = (MPlaneCameraIO *) calloc(1, sizeof(MPlaneCameraIO));
    mMPlaneCameraIO->openIdx = idx;

    res = mMPlaneCameraIO->openCamera();
    if (res < 0) {
        ALOGE("Unable to open sensor %d, errno=%d\n", mMPlaneCameraIO->openIdx, res);
    }

    // set input
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
    }

    mMPlaneCameraIO->closeCamera();

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
    int res;
    int i, j, k, START;
    int count = 0;
    struct v4l2_frmsizeenum frmsize;
    char property[PROPERTY_VALUE_MAX];
    unsigned int support_w,support_h;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera_preview.maxsize", property, NULL) > 0) {
        CAMHAL_LOGDB("support Max Preview Size :%s",property);
        if (sscanf(property,"%dx%d",&support_w,&support_h) != 2) {
            support_w = 10000;
            support_h = 10000;
        }
    }

    memset(&frmsize,0,sizeof(frmsize));
    frmsize.pixel_format = getOutputFormat();

    START = 0;
    for (i = 0; ; i++) {
        frmsize.index = i;
        res = ioctl(mMPlaneCameraIO->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)  { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                continue;
            if (count >= size)
                break;

            if (!IsHDMIAvailablePictureSize(kHDMIAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                continue;

            picSizes[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format=%d\n",
                frmsize.discrete.width, frmsize.discrete.height, frmsize.pixel_format);
            if (0 == i) {
                count += 4;
                continue;
            }

            for (k = count; k > START; k -= 4) {
                if (frmsize.discrete.width * frmsize.discrete.height >
                        picSizes[k - 3] * picSizes[k - 2]) {
                    picSizes[k + 1] = picSizes[k - 3];
                    picSizes[k + 2] = picSizes[k - 2];

                } else {
                    break;
                }
            }
            picSizes[k + 1] = frmsize.discrete.width;
            picSizes[k + 2] = frmsize.discrete.height;

            count += 4;
        }
    }

    START = count;
    for (i = 0; ; i++) {
        frmsize.index = i;
        res = ioctl(mMPlaneCameraIO->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            DBG_LOGB("index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                continue;
            if (count >= size)
                break;
            if (!IsHDMIAvailablePictureSize(kHDMIAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                continue;

            picSizes[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format =\
                HAL_PIXEL_FORMAT_YCbCr_420_888\n", frmsize.discrete.width,
                                                    frmsize.discrete.height);
            if (0 == i) {
                count += 4;
                continue;
            }

            for (k = count; k > START; k -= 4) {
                if (frmsize.discrete.width * frmsize.discrete.height >
                        picSizes[k - 3] * picSizes[k - 2]) {
                    picSizes[k + 1] = picSizes[k - 3];
                    picSizes[k + 2] = picSizes[k - 2];

                } else {
                    break;
                }
            }
            picSizes[k + 1] = frmsize.discrete.width;
            picSizes[k + 2] = frmsize.discrete.height;

            count += 4;
        }
    }

    uint32_t jpgSrcfmt[] = {
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_YUYV,
    };

    START = count;
    for (j = 0; j<(int)(sizeof(jpgSrcfmt)/sizeof(jpgSrcfmt[0])); j++) {
        memset(&frmsize,0,sizeof(frmsize));
        frmsize.pixel_format = jpgSrcfmt[j];

        for (i = 0; ; i++) {
            frmsize.index = i;
            res = ioctl(mMPlaneCameraIO->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (res < 0) {
                DBG_LOGB("index=%d, break\n", i);
                break;
            }

            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

                if (0 != (frmsize.discrete.width%16))
                    continue;

                if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height >support_h))
                    continue;

                if (count >= size)
                    break;

                if ((frmsize.pixel_format == V4L2_PIX_FMT_MJPEG) || (frmsize.pixel_format == V4L2_PIX_FMT_YUYV)
                    || (frmsize.pixel_format == V4L2_PIX_FMT_NV21)) {
                    if (!IsHDMIAvailablePictureSize(kHDMIAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                        continue;
                }

                picSizes[count+0] = HAL_PIXEL_FORMAT_BLOB;
                picSizes[count+1] = frmsize.discrete.width;
                picSizes[count+2] = frmsize.discrete.height;
                picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

                if (0 == i) {
                    count += 4;
                    continue;
                }

                //TODO insert in descend order
                for (k = count; k > START; k -= 4) {
                    if (frmsize.discrete.width * frmsize.discrete.height >
                            picSizes[k - 3] * picSizes[k - 2]) {
                        picSizes[k + 1] = picSizes[k - 3];
                        picSizes[k + 2] = picSizes[k - 2];

                    } else {
                        break;
                    }
                }

                picSizes[k + 1] = frmsize.discrete.width;
                picSizes[k + 2] = frmsize.discrete.height;

                count += 4;
            }
        }

        if (frmsize.index > 0)
            break;
    }

    if (frmsize.index == 0)
        CAMHAL_LOGDA("no support pixel fmt for jpeg");

    return count;
}

int HDMISensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag)
{
    int ret=0; int framerate=0; int temp_rate=0;
    struct v4l2_frmivalenum fival;
    int i,j=0;
    int count = 0;
    int tmp_size = size;
    memset(duration, 0 ,sizeof(int64_t) * size);
    int pixelfmt_tbl[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_YVU420,
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_YUYV,
    };

    for ( i = 0; i < (int) ARRAY_SIZE(pixelfmt_tbl); i++)
    {
        /* we got all duration for each resolution for prev format*/
        if (count >= tmp_size)
            break;

        for ( ; size > 0; size-=4)
        {
            memset(&fival, 0, sizeof(fival));

            for (fival.index = 0;;fival.index++)
            {
                fival.pixel_format = pixelfmt_tbl[i];
                fival.width = picSizes[size-3];
                fival.height = picSizes[size-2];
                if ((ret = ioctl(mMPlaneCameraIO->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
                    if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if ( fival.discrete.numerator != 0) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)(picSizes[size-4]);
                        duration[count+1] = (int64_t)(picSizes[size-3]);
                        duration[count+2] = (int64_t)(picSizes[size-2]);
                        if (framerate != 0) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
                        j++;
                    } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                        if ( fival.discrete.numerator != 0) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)picSizes[size-4];
                        duration[count+1] = (int64_t)picSizes[size-3];
                        duration[count+2] = (int64_t)picSizes[size-2];
                        if (framerate != 0) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
                        j++;
                    } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                        if ( fival.discrete.numerator != 0) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)picSizes[size-4];
                        duration[count+1] = (int64_t)picSizes[size-3];
                        duration[count+2] = (int64_t)picSizes[size-2];
                        if (framerate != 0) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
                        j++;
                    }
                } else {
                    if (j > 0) {
                        if (count >= tmp_size)
                            break;
                        duration[count+0] = (int64_t)(picSizes[size-4]);
                        duration[count+1] = (int64_t)(picSizes[size-3]);
                        duration[count+2] = (int64_t)(picSizes[size-2]);
                        if (framerate == 5) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else
                                duration[count+3] = (int64_t)200000000L;
                        } else if (framerate == 10) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else
                                duration[count+3] = (int64_t)100000000L;
                        } else if (framerate == 15) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else
                                duration[count+3] = (int64_t)66666666L;
                        } else if (framerate == 30) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else {
                                if (fival.width *fival.height >= 1920*1080)
                                    duration[count+3] = (int64_t)66666666L;
                                else
                                    duration[count+3] = (int64_t)33333333L;
                            }
                        } else if (framerate == 60) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else {
                                duration[count+3] = (int64_t)16666666L;
                            }
                        } else {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else
                                duration[count+3] = (int64_t)66666666L;
                        }
                        count += 4;
                        break;
                    } else {
                        break;
                    }
                }
            }
            framerate=0;
            j=0;
        }
        size = tmp_size;
    }

    return count;

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
        //mGE2D->doRotationAndMirror(b, true);
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
                // Add auxillary buffer of the right size
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


}

