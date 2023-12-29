

#define LOG_TAG "USBSensor"

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "../EmulatedFakeCamera3.h"
#include "Sensor.h"
#include "USBSensor.h"
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

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))

namespace android {

static const usb_frmsize_discrete_t kUsbAvailablePictureSize[] = {
        {4128, 3096},
        {3840, 2160},
        {3264, 2448},
        //{2592, 1944},
        {2592, 1936},
        {2560, 1920},
        {2688, 1520},
        {2048, 1536},
        {1600, 1200},
        {1920, 1088},
        {1920, 1080},
        //{1440, 1080},
        {1280, 960},
        {1280, 720},
        {1024, 768},
        {960, 720},
        {720, 480},
        {640, 480},
        {352, 288},
        {320, 240},
};

static bool IsUsbAvailablePictureSize(const usb_frmsize_discrete_t AvailablePictureSize[], uint32_t width, uint32_t height)
{
    int i;
    bool ret = false;
    int count = sizeof(kUsbAvailablePictureSize)/sizeof(kUsbAvailablePictureSize[0]);
    for (i = 0; i < count; i++) {
        if ((width == AvailablePictureSize[i].width) && (height == AvailablePictureSize[i].height)) {
            ret = true;
        } else {
            continue;
        }
    }
    return ret;
}

static bool isIDR(int UseHwType, uint8_t* in_src, uint32_t in_size)
{
    if (in_size < 8) {
        ALOGE("%s leave, bad len %d", __FUNCTION__, in_size );
        return false;
    }

    if ( in_src[0] != 0x00 ||  in_src[1] != 0x00 ||  in_src[2] != 0x00 || in_src[3] != 0x01) {
        ALOGE("%s leave, not start with 00 00 00 01", __FUNCTION__ );
        return false;
    }
    if (UseHwType  == USBSensor::HW_H264) {
        // nal_ref_idc bit 6:5  nal_unit_type bit 4:0
        int nal_ref_idc_shifted = in_src[4] & 0x60;
        int nal_unit_type = in_src[4] & 0x1f;
        if (nal_ref_idc_shifted > 0)
            if (nal_unit_type >= 5 && nal_unit_type <= 8)
                return true;

    } else {
        if (in_src[4] == 0x40 || in_src[4] == 0x42 || in_src[4] == 0x44 || in_src[4] == 0x4E || in_src[4] == 0x26) {
            return true;
        }
    }

    return false;
}


USBSensor::USBSensor(int type)
{
    mUseHwType = type;
    mDecodeMethod = DECODE_SOFTWARE;
    mImage_buffer = NULL;
    fp = NULL;
#ifdef GE2D_ENABLE
    mION = IONInterface::get_instance();
    mGE2D = new ge2dTransform();
#ifdef VICP_ENABLE
    mVICP = new vicpTransform();
#endif
#endif
    mSensorOutBuf.img = NULL;
    mSensorOutBuf.share_fd = -1;

    mDecoder = NULL;
    mCurrentFormat = 0;
    mIsDecoderInit = false;
    mUSBDevicefd = -1;
    mCameraVirtualDevice = nullptr;
    mVinfo = NULL;
    mCameraUtil = NULL;
    mTempFD = -1;
    mDecodedBuffer = NULL;
    mIsRequestFinished = false;

    mVICPEnable = false;
    mAsyncEnable = false;
    mDecFillThreadNeedReset=false;
    mHwDecoderSensor = false;
    mUsbSensorUtils = NULL;
    memset(&mSensorOutBuf, 0, sizeof(struct StreamBuffer));
    CAMHAL_LOGD("create usbsensor");
}

USBSensor::~USBSensor() {
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    if (mUsbSensorUtils) {
        delete(mUsbSensorUtils);
        mUsbSensorUtils = NULL;
    }
    if (mVinfo) {
        delete(mVinfo);
        mVinfo = NULL;
    }
    if (mCameraUtil) {
        delete mCameraUtil;
        mCameraUtil = NULL;
    }
    if (fp) {
        fclose(fp);
        fp = NULL;
    }
#ifdef GE2D_ENABLE
    if (mION) {
        mION->put_instance();
    }
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#ifdef VICP_ENABLE
    if (mVICP) {
        delete mVICP;
        mVICP = nullptr;
    }
#endif
#endif
    CAMHAL_LOGD("delete usbsensor");
};

int USBSensor::camera_open(int idx)
{
    char dev_name[128];
    int ret = 0;
    char property[PROPERTY_VALUE_MAX];
    CAMHAL_LOGV("%s: E", __FUNCTION__);


    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();

    mUSBDevicefd = mCameraVirtualDevice->openVirtualDevice(idx);
    if (mUSBDevicefd < 0) {
        CAMHAL_LOGD("open %s failed, errno=%d\n", dev_name, errno);
        ret = -ENOTTY;
    }
    property_get("ro.vendor.platform.omx", property, "false");
    if (strstr(property, "true"))
        mDecodeMethod = DECODE_OMX;
    return ret;
}

void USBSensor::camera_close(void)
{
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    if (mUSBDevicefd < 0)
        return;
    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    if (mVinfo != NULL)
        mCameraVirtualDevice->releaseVirtualDevice(mVinfo->idx,mUSBDevicefd);
    mUSBDevicefd = -1;
}

void USBSensor::InitVideoInfo(int idx) {
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    if (mVinfo && mUSBDevicefd >= 0) {
        mVinfo->fd = mUSBDevicefd;
        mVinfo->idx = idx;
    }else
        CAMHAL_LOGD("%s: init fail", __FUNCTION__);
}

int USBSensor::SensorInit(int idx) {
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    int ret = 0;
    if (mVinfo == NULL)
        mVinfo =  new CVideoInfo();
    if (nullptr == mUsbSensorUtils) {
        mUsbSensorUtils = new USBSensorUtils(mVinfo);
    }
    ret = camera_open(idx);
    if (ret < 0) {
        CAMHAL_LOGE("Unable to open sensor %d, errno=%d\n", mVinfo->idx, ret);
        return ret;
    }
    InitVideoInfo(idx);
    mVinfo->camera_init();
    setIOBufferNum();
    mSensorType = SENSOR_USB;
    ret = getOutputFormat();
    if (ret < 0) {
        CAMHAL_LOGE("get format fail, errno=%d\n", ret);
        return ret;
    }

    if (mUseHwType == HW_H264 && mDecodeMethod == DECODE_OMX) {
        mAsyncEnable = true;
        mHwDecoderSensor = true;
    } else if (mUseHwType == HW_MJPEG && mDecodeMethod == DECODE_OMX) {
        mAsyncEnable = false;
        mHwDecoderSensor = true;
    }

#ifdef VICP_ENABLE
    char property[PROPERTY_VALUE_MAX];
    property_get("ro.vendor.platform.vicp", property, "false");
    if (strstr(property, "true")) {
        CAMHAL_LOGD("VICP is enable");
        mVICPEnable = true;
    }
#endif
    return ret;
}

status_t USBSensor::startUp(int idx, bool customizationSensor) {
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    CAMHAL_LOGD("ddd");
    int res;
    mCapturedBuffers = NULL;
    mOpenCameraID = idx;

    res = run("Camera::USBSensor", ANDROID_PRIORITY_URGENT_AUDIO);
    if (res != OK) {
        CAMHAL_LOGE("Unable to start up sensor capture thread: %d", res);
        return res;
    }
    res = SensorInit(idx);
    if (!mCameraUtil)
        mCameraUtil = new CameraUtil();
    switch (mDecodeMethod) {
        case DECODE_OMX:
            if (!mDecoder) {
                mDecoder = new OMXDecoder(true, true);
            }
            break;
        default:
            break;
    }
    return res;
}

uint32_t USBSensor::getStreamUsage(aml_camera_stream_t& stream){
    ATRACE_CALL();

    uint32_t usage = (GRALLOC_USAGE_HW_TEXTURE
            | GRALLOC_USAGE_HW_RENDER
            | GRALLOC_USAGE_SW_READ_MASK
            | GRALLOC_USAGE_SW_WRITE_MASK
            );
#if 0
#if ANDROID_PLATFORM_SDK_VERSION >= 28
        usage = am_gralloc_get_omx_osd_producer_usage();
#else
        usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_AML_DMA_BUFFER;
#endif
#endif
    usage = GRALLOC1_PRODUCER_USAGE_CAMERA | usage;
    CAMHAL_LOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;
}

status_t USBSensor::setOutputFormat(int width, int height,
    int pixelformat, channel ch) {
    int res;
    mFramecount = 0;
    mCurFps = 0;
    gettimeofday(&mTimeStart, NULL);
    initDecoder(width, height, width, height, 4);
    if (ch == channel_capture) {
        mVinfo->picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVinfo->picture.format.fmt.pix.width = width;
        mVinfo->picture.format.fmt.pix.height = height;
        mVinfo->picture.format.fmt.pix.pixelformat = pixelformat;
    } else {
        mVinfo->preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        mVinfo->preview.format.fmt.pix.width = width;
        mVinfo->preview.format.fmt.pix.height = height;
        mVinfo->preview.format.fmt.pix.pixelformat = pixelformat;
        res = mVinfo->setBuffersFormat();
        if (res < 0) {
            CAMHAL_LOGE("set buffer failed\n");
            return res;
        }
    }
        if (NULL == mImage_buffer) {
            mPre_width = mVinfo->preview.format.fmt.pix.width;
            mPre_height = mVinfo->preview.format.fmt.pix.height;
            CAMHAL_LOGD("setOutputFormat :: pre_width = %d, pre_height = %d \n" , mPre_width , mPre_height);
            mImage_buffer = new uint8_t[mPre_width * mPre_height * 3 / 2];
            if (mImage_buffer == NULL) {
                CAMHAL_LOGE("first time allocate mTemp_buffer failed !");
                return -1;
            }
        }
        mSensorOutBuf.streamId = 0;
        mSensorOutBuf.buffer = NULL;
        mSensorOutBuf.format = pixelformat;

        if (NULL == mSensorOutBuf.img) {
            mSensorOutBuf.width = mPre_width;
            mSensorOutBuf.height = mPre_height;
            mSensorOutBuf.stride = mPre_width;
#ifdef GE2D_ENABLE
            mSensorOutBuf.img = mION->alloc_buffer(mPre_width * mPre_height * 3, &mSensorOutBuf.share_fd);
#else
            mSensorOutBuf.img = new uint8_t[mPre_width * mPre_height * 3];
#endif
        }

        if ((mPre_width != mVinfo->preview.format.fmt.pix.width)
            && (mPre_height != mVinfo->preview.format.fmt.pix.height)) {
                if (mImage_buffer) {
                    delete [] mImage_buffer;
                    mImage_buffer = NULL;
                }
                if (mSensorOutBuf.img) {
#ifdef GE2D_ENABLE
                    mION->free_buffer(mSensorOutBuf.share_fd);
#else
                    delete[] mSensorOutBuf.img;
#endif
                    mSensorOutBuf.img = NULL;
                }
                mPre_width = mVinfo->preview.format.fmt.pix.width;
                mPre_height = mVinfo->preview.format.fmt.pix.height;

                mImage_buffer = new uint8_t[mPre_width * mPre_height * 3 / 2];
                if (mImage_buffer == NULL) {
                    CAMHAL_LOGE("allocate mTemp_buffer failed !");
                    return -1;
                }

                mSensorOutBuf.width = mPre_width;
                mSensorOutBuf.height = mPre_height;
                mSensorOutBuf.stride = mPre_width;
#ifdef GE2D_ENABLE
                mSensorOutBuf.img = mION->alloc_buffer(mPre_width * mPre_height * 3, &mSensorOutBuf.share_fd);
#else
                mSensorOutBuf.img = new uint8_t[mPre_width * mPre_height * 3];
#endif
        }
        return OK;
}

status_t USBSensor::streamOn(channel ch) {
    int ret = mVinfo->start_capturing();
    if (mVinfo->get_device_status() == 0) {
        if (mAsyncEnable) {
            ret = DecFillBufThreadStart();
        }
    } else {
        mDecFillBufThreadNeedStop.store(true);
    }
    return ret;

}
bool USBSensor::isStreaming() {
    return mVinfo->isStreaming;
}

bool USBSensor::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch) {
    if ((mVinfo->preview.format.fmt.pix.width != width)
        ||(mVinfo->preview.format.fmt.pix.height != height)) {
        return true;
    }
    return false;
}

void USBSensor::initDecoder(int in_width, int in_height, int out_width, int out_height, int out_bufferCount) {
    CAMHAL_LOGV("%s: in_width=%d, in_height=%d out_width=%d, out_height=%d",
         __FUNCTION__, in_width, in_height, out_width, out_height);
    if (mDecoder != NULL && mIsDecoderInit == false) {
        mDecoder->setParameters(in_width, in_height, out_width, out_height, out_bufferCount + 2);
        if (mUseHwType == HW_MJPEG)
            mDecoder->initialize("mjpeg");
        else if (mUseHwType == HW_H264)
            mDecoder->initialize("h264");
        else
            mDecoder->initialize("mjpeg");
        mDecoder->prepareBuffers();
        mDecoder->start();
        mIsDecoderInit=true;
        mDecoder->VICPEnable = mVICPEnable;
    }
}

status_t USBSensor::shutDown() {
    CAMHAL_LOGV("%s: E", __FUNCTION__);

    if (mAsyncEnable) {
        DecFillBufThreadStop();
    }

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    DeWarp::putInstance();
#endif

    if (mDecoder && mIsDecoderInit == true) {
        mDecoder->deinitialize();
        delete mDecoder;
        mDecoder = NULL;
        mIsDecoderInit = false;
    }
    //return Sensor::shutDown();
    int res;
    mTimeOutCount = 0;
    res = requestExitAndWait();
    if (res != OK) {
        CAMHAL_LOGE("Unable to shut down sensor capture thread: %d", res);
    }
    if (mVinfo != NULL)
        mVinfo->releasebuf_and_stop_capturing();

    camera_close();

    if (mImage_buffer) {
        delete [] mImage_buffer;
        mImage_buffer = NULL;
    }
    if (mSensorOutBuf.img && mSensorOutBuf.share_fd >= 0) {
#ifdef GE2D_ENABLE
        mION->free_buffer(mSensorOutBuf.share_fd);
#else
        delete[] mSensorOutBuf.img;
#endif
        mSensorOutBuf.img = NULL;
        mSensorOutBuf.share_fd = -1;
    }
    mSensorWorkFlag = false;
    CAMHAL_LOGD("%s: Exit", __FUNCTION__);
    return res;
}

status_t USBSensor::streamOff(channel ch) {
    CAMHAL_LOGV("%s: E", __FUNCTION__);

    if (mAsyncEnable) {
        DecFillBufThreadStop();
    }

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    DeWarp::putInstance();
#endif

    if (mDecoder && mIsDecoderInit == true) {
        mDecoder->deinitialize();
        mIsDecoderInit = false;
    }

    return mVinfo->releasebuf_and_stop_capturing();

}
void USBSensor::setIOBufferNum()
{
    char buffer_number[128];
    int tmp = 4;
    if (property_get("ro.vendor.usbcamera.iobuffer", buffer_number, NULL) > 0) {
        sscanf(buffer_number, "%d", &tmp);
        CAMHAL_LOGD("get property value is %d\n",tmp);
    } else {
        CAMHAL_LOGD("default buffer number is %d\n",tmp);
    }
    mVinfo->set_buffer_numbers(tmp);
}

status_t USBSensor::getOutputFormat(void){
    uint32_t ret = 0;

    if (mUseHwType == HW_H264) {
        ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_H264);
        if (ret)
            return ret;
        mUseHwType = HW_MJPEG;
    }

    if (mUseHwType == HW_MJPEG || mUseHwType == HW_NONE) {
        ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_MJPEG);
        if (ret)
            return ret;
    }

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_NV21);
    if (ret) {
        mUseHwType = HW_NONE;
        return ret;
    }

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret) {
        mUseHwType = HW_NONE;
        return ret;
    }
    CAMHAL_LOGE("Unable to find a supported sensor format!");
    return BAD_VALUE;
}

int USBSensor::halFormatToSensorFormat(uint32_t pixelfmt){
    uint32_t ret = 0;
    uint32_t fmt = 0;
    if (mUseHwType == HW_H264) {
        ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_H264);
        if (ret)
            return ret;
        mUseHwType = HW_MJPEG;
    }

    if  (mUseHwType == HW_MJPEG || mUseHwType == HW_NONE) {
        ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_MJPEG);
        if (ret)
            return ret;
    }

    switch (pixelfmt) {
        case HAL_PIXEL_FORMAT_YV12:
            fmt = V4L2_PIX_FMT_YVU420;
            break;
        case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            fmt = V4L2_PIX_FMT_NV21;
            break;
        case HAL_PIXEL_FORMAT_YCbCr_422_I:
            fmt = V4L2_PIX_FMT_YUYV;
            break;
        default:
            fmt = V4L2_PIX_FMT_NV21;
            break;
        break;
    }
    ret = mVinfo->EnumerateFormat(fmt);
    if (ret)
        return ret;

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret)
        return ret;

    CAMHAL_LOGE("%s, Unable to find a supported sensor format!", __FUNCTION__);
    return BAD_VALUE;
}

void USBSensor::captureNV21UsbSensor(StreamBuffer b, uint32_t gain, bool needSensorOutBuf) {
    CAMHAL_LOGVV("%s: E old",__FUNCTION__);
    uint8_t *src;
    int pixelformat;

    if (mDecodedBuffer) {
        src = mDecodedBuffer;
        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_NV21
                ||mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV
                ||mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG
                ||mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_H264) {

            uint32_t width = mVinfo->preview.format.fmt.pix.width;
            uint32_t height = mVinfo->preview.format.fmt.pix.height;
            if ((width == b.width) && (height == b.height)) {
               //copy the first buffer to new buffer to do recording
#ifdef GE2D_ENABLE
                if (mTempFD != -1) {
#ifdef VICP_ENABLE
                    if (mVICPEnable) {
                        mVICP->vicp_copy(b.share_fd,mTempFD,b.stride,b.height,VICP_COLOR_FMT_YCrCb_420_SP_NV21);
                    }
                    else
#endif
                        mGE2D->ge2d_copy(b.share_fd,mTempFD,b.stride,b.height,ge2dTransform::NV12);
                }
                else
                    memcpy(b.img, src, b.stride * b.height * 3/2);
#else
                memcpy(b.img, src, b.stride * b.height * 3/2);
#endif
            } else {
#ifdef GE2D_ENABLE
                if (mTempFD != -1) {
#ifdef VICP_ENABLE
                    if (mVICPEnable) {
                        mVICP->vicp_keep_ration_scale(b.share_fd, VICP_COLOR_FMT_YCrCb_420_SP_NV21, b.width, b.height,
                                              mTempFD, width, height);
                    }
                    else
#endif
                        mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                              mTempFD, width, height);
                } else {
                    mCameraUtil->ReSizeNV21(src, b.img, b.width, b.height, b.stride, width, height);
                }
#else
                mCameraUtil->ReSizeNV21(src, b.img, b.width, b.height, b.stride, width, height);
#endif
            }
        }  else {
            CAMHAL_LOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        return ;
    }
    while (1) {
//        if (mAsyncEnable) {
//            src = nullptr;
//            if (mVinfo->get_device_status()) {
//                mUnpluged = true;
//                camera_close();
//                CAMHAL_LOGD("%s: error status close camera\n", __FUNCTION__);
//                break;
//            }
//            if (mVinfo->fd <= 0)
//                break;
//        } else {
            fd_set fds;
            struct timeval tv;
            int r;
            if (mVinfo->fd <= 0)
                break;
            FD_ZERO(&fds);
            FD_SET(mVinfo->fd, &fds);
            /*2s Timeout*/
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                CAMHAL_LOGD("select error:%s",strerror(errno));
            }
            if (0 == r) {
                int ret = ResetSensorAndDecoder();
                if (ret != 0) {
                    CAMHAL_LOGE("can't reset");
                }
                CAMHAL_LOGD("select timeout:%s",strerror(errno));
            }
            src = (uint8_t *)mVinfo->get_frame();
            if (NULL == src) {
                if (mVinfo->get_device_status()) {
                    mUnpluged = true;
                    camera_close();
                    break;
                }
                CAMHAL_LOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
                usleep(5000);

                mTimeOutCount++;
                if (mTimeOutCount > 600) {
                    CAMHAL_LOGD("force sensor reset.\n");
                    force_reset_sensor();
                }

                continue;
            }
            mTimeOutCount = 0;
        //}
        pixelformat = mVinfo->preview.format.fmt.pix.pixelformat;
        switch (pixelformat) {
            case V4L2_PIX_FMT_NV21:
                if (mVinfo->preview.buf.length == b.width * b.height * 3/2) {
                    if (src != nullptr)
                        memcpy(b.img, src, mVinfo->preview.buf.length);
                } else {
                    if (src != nullptr)
                        mCameraUtil->nv21_memcpy_align32 (b.img, src, b.width, b.height);
                }
#ifdef GE2D_ENABLE
                mGE2D->doRotationAndMirror(b);
#endif
                mDecodedBuffer = b.img;
                mKernelBuffer = src;
                mVinfo->putback_frame();
                break;
            case V4L2_PIX_FMT_YUYV:
                {
                    uint32_t width = mVinfo->preview.format.fmt.pix.width;
                    uint32_t height = mVinfo->preview.format.fmt.pix.height;

                    memset(mImage_buffer, 0 , width * height * 3/2);

                    if (src != nullptr)
                        mCameraUtil->YUYVToNV21(src, mImage_buffer, width, height);

                    if ((width == b.width) && (height == b.height)) {
                        memcpy(b.img, mImage_buffer, b.width * b.height * 3/2);
                    } else {
                        if ((b.height % 2) != 0) {
                            CAMHAL_LOGD("%d , b.height = %d", __LINE__, b.height);
                            b.height = b.height - 1;
                        }
                        mCameraUtil->ReSizeNV21(mImage_buffer, b.img, b.width, b.height, b.stride,width,height);
                    }
#ifdef GE2D_ENABLE
                    mGE2D->doRotationAndMirror(b);
#endif
                    mDecodedBuffer = mImage_buffer;
                    mKernelBuffer = src;
                    mVinfo->putback_frame();
                }
                break;
            case V4L2_PIX_FMT_MJPEG:
                {
                    int ret = 1;
                    if (needSensorOutBuf
                        && ( b.width != mVinfo->preview.format.fmt.pix.width || b.height != mVinfo->preview.format.fmt.pix.height)) {
                          // need store sensor output buffer AND b.size not equal to sensor output size
                          // first - decode to mSensorOutBuf; then resize to b
                        if (mSensorOutBuf.img == NULL || mSensorOutBuf.share_fd < 0) {
                            // here we need mSensorOutBuf has been ready.
                            CAMHAL_LOGE("mSensorOutBuf not allocated.");
                        } else {
                            if (src != NULL)
                                ret = MJPEGToNV21(src, mSensorOutBuf);
#ifdef GE2D_ENABLE
#ifdef VICP_ENABLE
                            if (mVICPEnable) {
                                mVICP->vicp_keep_ration_scale(b.share_fd, VICP_COLOR_FMT_YCrCb_420_SP_NV21, b.width, b.height,
                                            mSensorOutBuf.share_fd, mSensorOutBuf.width, mSensorOutBuf.height);
                            } else
#endif
                                mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                              mSensorOutBuf.share_fd, mSensorOutBuf.width, mSensorOutBuf.height);

#else
                            mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, mSensorOutBuf.width, mSensorOutBuf.height);
#endif
                        }
                    } else {
                        if (src != NULL)
                            ret = MJPEGToNV21(src, b);
                    }
                    if (ret == 1) {
                        mVinfo->putback_frame();
                        continue;
                    }
#ifdef GE2D_ENABLE
                    mGE2D->doRotationAndMirror(b);
#endif
                    mVinfo->putback_frame();

                }
                break;
            default:
                CAMHAL_LOGD("not support this format");
                break;
        }
        mSensorWorkFlag = true;
        if (mFlushFlag)
            break;


        if (mExitSensorThread)
            break;

        break;
    }
}

void USBSensor::captureNV21UsbSensor(Vector<StreamBuffer> &b, uint32_t gain, bool isJpegRequest) {
    ATRACE_CALL();
    CAMHAL_LOGVV("%s: E new", __FUNCTION__);

    uint8_t *src;
    int pixelformat;
    while (1) {
        if (mAsyncEnable) {
            src = nullptr;
            if (mVinfo->get_device_status()) {
                mUnpluged = true;
                camera_close();
                CAMHAL_LOGD("%s: error status close camera\n", __FUNCTION__);
                break;
            }
            if (mVinfo->fd <= 0)
                break;
        } else {
            fd_set fds;
            struct timeval tv;
            int r;
            if (mVinfo->fd <= 0)
                break;

read_queue:
            FD_ZERO(&fds);
            FD_SET(mVinfo->fd, &fds);
            /*2s Timeout*/
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
            if (-1 == r) {
                if (EINTR == errno)
                    continue;
                CAMHAL_LOGD("select error:%s",strerror(errno));
            }
            if (0 == r) {
                int ret = ResetSensorAndDecoder();
                if (ret != 0) {
                    CAMHAL_LOGE("can't reset");
                }
                CAMHAL_LOGD("select timeout:%s",strerror(errno));
            }
            src = (uint8_t *)mVinfo->get_frame();
            if (NULL == src) {
                if (mVinfo->get_device_status()) {
                    camera_close();
                    break;
                }
                CAMHAL_LOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
                usleep(5000);

                mTimeOutCount++;
                if (mTimeOutCount > 600) {
                    CAMHAL_LOGD("force sensor reset.\n");
                    force_reset_sensor();
                }

                continue;
            }
            // here. src is valid.
            // check if this src is the latest src.
            FD_ZERO(&fds);
            FD_SET(mVinfo->fd, &fds);
            /*no block select, just check if there are more filled buffers*/
            tv.tv_sec = 0;
            tv.tv_usec = 0;
            r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
            if (r > 0) {
                // yes, there are more filled buffers. queue this one, dq next;
                if ( 0 > mVinfo->putback_frame() ) {
                    CAMHAL_LOGE("%s: VIDIOC_QBUF/flush failed, errno=%d\n", __func__, errno);
                    break;
                }
                goto read_queue;
            }

            mTimeOutCount = 0;
        }
        pixelformat = mVinfo->preview.format.fmt.pix.pixelformat;
        switch (pixelformat) {
            case V4L2_PIX_FMT_MJPEG:
            case V4L2_PIX_FMT_H264:
                {
                    int ret = OMXToNV21(src, b, isJpegRequest);
                    if (ret == 1) {
                        if (!mAsyncEnable) {
                            mVinfo->putback_frame();
                        }
                        continue;
                    }

                    if (!mAsyncEnable) {
                        mVinfo->putback_frame();
                    }
                }
                break;
            default:
                CAMHAL_LOGD("not support this format");
                break;
        }
        mSensorWorkFlag = true;
        if (mFlushFlag)
            break;

        if (mExitSensorThread)
            break;

        break;
    }

}

int USBSensor::OMXToNV21(uint8_t* src, Vector<StreamBuffer>& b, bool isJpegRequest) {
    ATRACE_CALL();
    int flag = 0;
    int ret = 0;
    size_t src_width = mVinfo->preview.format.fmt.pix.width;
    size_t src_height = mVinfo->preview.format.fmt.pix.height;
    size_t src_length = mVinfo->preview.buf.bytesused;

    switch (mDecodeMethod) {
        case DECODE_OMX:
            {
                {
                    AutoMutex l(mDecFillThreadResetLock);
                    if (mDecFillThreadNeedReset) {
                        ret = ResetSensorAndDecoder();
                        if (ret < 0) {
                        ALOGE("reset Sensor and Decoder fail!");
                        }
                    }
                    mDecFillThreadNeedReset = false;
                }
                if (mAsyncEnable) {
                    ret = mDecoder->DecodeAsync(src, src_length, b, isJpegRequest);
                } else {
                    ret = mDecoder->Decode(src, src_length, b, isJpegRequest);
                }
                if (!ret) {
                    flag = 1;
                    if (mDecoder->mTimeOut && mIsDecoderInit == true) {
                        AutoMutex l(mDecFillThreadWaitLock);
                        mDecoder->deinitialize();
                        mIsDecoderInit = false;
                        initDecoder(src_width, src_height,
                                    src_width, src_height, 4);
                    }
                }
            }
            break;
        default:
            ALOGD("not support this decode method");
            break;
    }
    return flag;
}



int USBSensor::MJPEGToNV21(uint8_t* src, StreamBuffer b) {
    CAMHAL_LOGVV("%s: E, src=0x%p b.w=%d", __FUNCTION__, src, b.width);
    int flag = 0;
    size_t src_width = mVinfo->preview.format.fmt.pix.width;
    size_t src_height = mVinfo->preview.format.fmt.pix.height;
    size_t src_length = mVinfo->preview.buf.bytesused;

    char property[PROPERTY_VALUE_MAX];
    property_get("camera.debug.dump.device", property, "false");
    if (strstr(property, "true")) {
        static int src_index = 0;
        dump(src_index,src, src_length, "src.mjpg");
    }


    switch (mDecodeMethod) {
        case DECODE_SOFTWARE:
            {
                int result = 0;
                memset(mImage_buffer, 0, src_width * src_height * 3/2);
                result = mCameraUtil->MJPEGToNV21(src, src_length,
                                                src_width, src_height,
                                                b.img, b.width, b.height, b.stride,
                                                mImage_buffer);
                if (result != 0) {
                    CAMHAL_LOGE("software decoder error \n");
                    flag = 1;
                } else {
                    if (src_width == b.width && src_height == b.height) {
                        mDecodedBuffer = b.img;
                        mTempFD = b.share_fd;
                    }else {
                        mDecodedBuffer = mImage_buffer;
                    }
                    mKernelBuffer = src;
                }
            }
            break;
        default:
            CAMHAL_LOGD("not support this decode method");
            break;
    }
    property_get("camera.debug.dump.decoder", property, "false");
    if (strstr(property, "true")) {
        static int dst_index = 0;
        size_t size = b.width*b.height*3/2;
        dump(dst_index,b.img, size ,"dst.yuv");
    }
    return flag;
}

void USBSensor::captureYV12(StreamBuffer b, uint32_t gain){
    uint8_t *src;
    if (mKernelBuffer) {
        src = mKernelBuffer;
        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420) {
            int width = mVinfo->preview.format.fmt.pix.width;
            int height = mVinfo->preview.format.fmt.pix.height;
            mCameraUtil->ScaleYV12(src,width,height,b.img,b.width,b.height);

        } else if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            int width = mVinfo->preview.format.fmt.pix.width;
            int height = mVinfo->preview.format.fmt.pix.height;
            mCameraUtil->YUYVScaleYV12(src,width,height,b.img,b.width,b.height);

        } else if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            int width = mVinfo->preview.format.fmt.pix.width;
            int height = mVinfo->preview.format.fmt.pix.height;
            int length = mVinfo->preview.buf.bytesused;
            mCameraUtil->MJPEGScaleYV12(src,length,width,height,
                    b.img,b.width,b.height,true);

        } else {
            CAMHAL_LOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        return ;
    }
    while (1) {
        fd_set fds;
        struct timeval tv;
        int r;
        FD_ZERO(&fds);
        FD_SET(mVinfo->fd, &fds);
        /*2s Timeout*/
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
        if (-1 == r) {
            if (EINTR == errno)
                continue;
            CAMHAL_LOGD("select error:%s",strerror(errno));
        }
        if (0 == r)
        CAMHAL_LOGD("select timeout:%s",strerror(errno));

        src = (uint8_t *)mVinfo->get_frame();

        if (NULL == src) {
            CAMHAL_LOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
            usleep(5000);
            mTimeOutCount++;
            if (mTimeOutCount > 600) {
                force_reset_sensor();
            }
            continue;
        }
        mTimeOutCount = 0;

        if (mVinfo->preview.format.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
            if (mVinfo->preview.buf.length != mVinfo->preview.buf.bytesused) {
                CAMHAL_LOGD("length=%d, bytesused=%d \n", mVinfo->preview.buf.length, mVinfo->preview.buf.bytesused);
                mVinfo->putback_frame();
                continue;
            }
        }

        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YVU420) {
            if (mVinfo->preview.buf.length == b.width * b.height * 3/2) {
                memcpy(b.img, src, mVinfo->preview.buf.length);
            } else {
                mCameraUtil->yv12_memcpy_align32 (b.img, src, b.width, b.height);
            }
            mKernelBuffer = b.img;
        } else if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            int width = mVinfo->preview.format.fmt.pix.width;
            int height = mVinfo->preview.format.fmt.pix.height;
            mCameraUtil->YUYVToYV12(src, b.img, width, height);
            mKernelBuffer = b.img;
        } else if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_MJPEG) {
            int width = mVinfo->preview.format.fmt.pix.width;
            int height = mVinfo->preview.format.fmt.pix.height;
            int length = mVinfo->preview.buf.bytesused;
            int ret = 0;
            ret = mCameraUtil->MJPEGScaleYV12(src,length,width,height,
                    b.img,b.width,b.height,false);
            if (ret < 0) {
                mVinfo->putback_frame();
                continue;
            }else{
                mKernelBuffer = b.img;
            }
        } else {
            CAMHAL_LOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        mSensorWorkFlag = true;
        mVinfo->putback_frame();
        if (mFlushFlag)
            break;
        if (mExitSensorThread)
            break;
        break;
    }
    CAMHAL_LOGVV("YV12 sensor image captured");
}
void USBSensor::captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride){
    uint8_t *src;
    if (mKernelBuffer) {
        src = mKernelBuffer;
        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            //TODO YUYV scale
            //memcpy(img, src, vinfo->preview.buf.length);

        } else
            CAMHAL_LOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);

        return ;
    }

    while (1) {
        fd_set fds;
        struct timeval tv;
        int r;
        FD_ZERO(&fds);
        FD_SET(mVinfo->fd, &fds);
        /*2s Timeout*/
        tv.tv_sec = 2;
        tv.tv_usec = 0;
        r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
        if (-1 == r) {
            if (EINTR == errno)
                continue;
            CAMHAL_LOGD("select error:%s",strerror(errno));
        }
        if (0 == r)
        CAMHAL_LOGD("select timeout:%s",strerror(errno));
        src = (uint8_t *)mVinfo->get_frame();
        if (NULL == src) {
            CAMHAL_LOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
            usleep(5000);
            mTimeOutCount++;
            if (mTimeOutCount > 600) {
                force_reset_sensor();
            }
            continue;
        }
        mTimeOutCount = 0;

        if (mVinfo->preview.format.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
            if (mVinfo->preview.buf.length != mVinfo->preview.buf.bytesused) {
                CAMHAL_LOGD("length=%d, bytesused=%d \n", mVinfo->preview.buf.length, mVinfo->preview.buf.bytesused);
                mVinfo->putback_frame();
                continue;
            }
        }

        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            memcpy(img, src, mVinfo->preview.buf.length);
            mKernelBuffer = src;
        } else {
            CAMHAL_LOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        mSensorWorkFlag = true;
        mVinfo->putback_frame();

        if (mFlushFlag)
            break;
        if (mExitSensorThread)
            break;

        break;
    }
    //mKernelBuffer = src;
    CAMHAL_LOGVV("YUYV sensor image captured");
}

void USBSensor::dump(int& frame_index, uint8_t* buf, int length, std::string name) {
    int frame_num = 0;
    std::string path("/data/vendor/camera/");
    path.append(name);
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.camera.dump.forever", property, "false");
    if (strstr(property, "true")) {
        CAMHAL_LOGD("full_name:%s",path.c_str());
        fp = fopen(path.c_str(),"ab+");
        if (!fp) {
            CAMHAL_LOGE("open file %s fail, error: %s !!!",
                path.c_str(),strerror(errno));
            return;
        } else {
            fwrite((void*)buf,1,length,fp);
            fclose(fp);
            fp = NULL;
            return;
       }
    } else {
        property_get("vendor.camera.dump.num", property, "0");
        frame_num = atoi(property);
        if (frame_index == frame_num)
            return;
        path = path + "-" + std :: to_string(frame_index);
        fp = fopen(path.c_str(),"wb+");
        if (!fp) {
        CAMHAL_LOGE("open file %s fail, error: %s !!!",
            path.c_str(),strerror(errno));
        return;
        }
        CAMHAL_LOGE("write frame %d ",frame_index);
        fwrite((void*)buf,1,length,fp);
        fclose(fp);
        fp = NULL;
        frame_index++;
        return;
    }
}

int USBSensor::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    return mUsbSensorUtils->getZoom( zoomMin, zoomMax, zoomStep);
}

int USBSensor::setZoom(int zoomValue) {
    return mUsbSensorUtils->setZoom( zoomValue) ;
}

status_t USBSensor::setEffect(uint8_t effect) {
    return mUsbSensorUtils->setEffect( effect);
}

void USBSensor::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}

int USBSensor::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step)
{
    return mUsbSensorUtils->getExposure( maxExp,  minExp,  def, step);
}

status_t USBSensor::setExposure(int expCmp)
{
    return mUsbSensorUtils->setExposure( expCmp);
}

int USBSensor::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont)
{
    return mUsbSensorUtils->getAntiBanding(antiBanding, maxCont);
}

status_t USBSensor::setAntiBanding(uint8_t antiBanding)
{
    return mUsbSensorUtils->setAntiBanding(antiBanding);

}

status_t USBSensor::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return mUsbSensorUtils->setFocusArea( x0,  y0, x1, y1);
}


int USBSensor::getAutoFocus(uint8_t *afMode, uint8_t maxCount)
{
    return mUsbSensorUtils->getAutoFocus( afMode, maxCount);
}

status_t USBSensor::setAutoFocus(uint8_t afMode)
{
    return mUsbSensorUtils->setAutoFocus(afMode);
}

int USBSensor::getAWB(uint8_t *awbMode, uint8_t maxCount)
{
    return mUsbSensorUtils->getAWB( awbMode, maxCount);
}

status_t USBSensor::setAWB(uint8_t awbMode)
{
    return mUsbSensorUtils->setAWB(awbMode);
}

const char* USBSensor::getformt(int id) {
    return mUsbSensorUtils->getformtStr(id);
}

int USBSensor::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) {
    int res;
    int i, k, START;
    int count = 0;
    //int pixelfmt;
    struct v4l2_frmsizeenum frmsize;
    char property[PROPERTY_VALUE_MAX];
    unsigned int support_w,support_h;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera_preview.maxsize", property, NULL) > 0) {
        CAMHAL_LOGD("support Max Preview Size :%s",property);
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
        res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            CAMHAL_LOGD("index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if (frmsize.pixel_format != V4L2_PIX_FMT_H264) {
                if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                    continue;
            }

            if (count >= size)
                break;

            if (!IsUsbAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                continue;

            picSizes[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            CAMHAL_LOGD("get output width=%d, height=%d, format=%s\n",
                                    frmsize.discrete.width,
                                    frmsize.discrete.height,
                                    getformt(frmsize.pixel_format));

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
        res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            CAMHAL_LOGD("index=%d, break\n", i);
            break;
        }

        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if (frmsize.pixel_format != V4L2_PIX_FMT_H264) {
                if ((frmsize.discrete.width * frmsize.discrete.height) > (support_w * support_h))
                    continue;
            }

            if (count >= size)
                break;

            if (!IsUsbAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                continue;

            picSizes[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            CAMHAL_LOGD("get output width=%d, height=%d, format=HAL_PIXEL_FORMAT_YCbCr_420_888\n",
                                                    frmsize.discrete.width,
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

//    uint32_t jpgSrcfmt[] = {
//        V4L2_PIX_FMT_RGB24,
//        V4L2_PIX_FMT_MJPEG,
//        V4L2_PIX_FMT_H264,
//        V4L2_PIX_FMT_YUYV,
//    };

    START = count;
    memset(&frmsize,0,sizeof(frmsize));
    frmsize.pixel_format = getOutputFormat();

    for (i = 0; ; i++) {
        frmsize.index = i;
        res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            CAMHAL_LOGD("index=%d, break\n", i);
            break;
        }
        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if (frmsize.pixel_format != V4L2_PIX_FMT_H264) {
                if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height >support_h))
                    continue;
            }

            if (count >= size)
                break;

            if (!IsUsbAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                continue;

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

    if (frmsize.index == 0)
        CAMHAL_LOGD("no support pixel fmt for jpeg");

    return count;

}

int USBSensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag)
{
    int count = 0;
    int tmp_size = size;
    memset(duration, 0 ,sizeof(int64_t) * size);

    for ( ; size > 0; size-=4)
    {
        duration[count+0] = (int64_t)(picSizes[size-4]);
        duration[count+1] = (int64_t)(picSizes[size-3]);
        duration[count+2] = (int64_t)(picSizes[size-2]);
        if (!flag && picSizes[size-4] != HAL_PIXEL_FORMAT_BLOB)
            duration[count+3] = 0;
        else
            duration[count+3] = (int64_t)FRAME_DURATION;
        count+=4;
    }
    size = tmp_size;
    return count;

}

int64_t USBSensor::getMinFrameDuration()
{
    int64_t tmpDuration =  66666666L; // 1/15 s
    int64_t frameDuration =  66666666L; // 1/15 s
    struct v4l2_frmivalenum fival;
    int i,j;

    uint32_t pixelfmt_tbl[]={
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_NV21,
    };
    struct v4l2_frmsize_discrete resolution_tbl[]={
        {3840,2160},
        {1920, 1080},
        {1280, 960},
        {640, 480},
        {352, 288},
        {320, 240},
    };

    for (i = 0; i < (int)ARRAY_SIZE(pixelfmt_tbl); i++) {
        for (j = 0; j < (int) ARRAY_SIZE(resolution_tbl); j++) {
            memset(&fival, 0, sizeof(fival));
            fival.index = 0;
            fival.pixel_format = pixelfmt_tbl[i];
            fival.width = resolution_tbl[j].width;
            fival.height = resolution_tbl[j].height;

            while (ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival) == 0) {
                if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                    tmpDuration =
                        (int64_t) fival.discrete.numerator * 1000000000L / fival.discrete.denominator;

                    if (frameDuration > tmpDuration)
                        frameDuration = tmpDuration;
                } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                    frameDuration =
                        (int64_t) fival.stepwise.max.numerator * 1000000000L / fival.stepwise.max.denominator;
                    break;
                } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                    frameDuration =
                        (int64_t) fival.stepwise.max.numerator * 1000000000L / fival.stepwise.max.denominator;
                    break;
                }
                fival.index++;
            }
        }

        if (fival.index > 0) {
            break;
        }
    }

    //CAMHAL_LOGD("enum frameDuration=%lld\n", frameDuration);
    return frameDuration;
}

int USBSensor::getPictureSizes(int32_t picSizes[], int size, bool preview) {
    int res;
    int i;
    int count = 0;
    struct v4l2_frmsizeenum frmsize;
    char property[PROPERTY_VALUE_MAX];
    unsigned int support_w,support_h;
    int preview_fmt;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera_preview.maxsize", property, NULL) > 0) {
        CAMHAL_LOGD("support Max Preview Size :%s",property);
        if (sscanf(property,"%dx%d",&support_w,&support_h) !=2) {
            support_w = 10000;
            support_h = 10000;
        }
    }


    memset(&frmsize,0,sizeof(frmsize));
    preview_fmt = V4L2_PIX_FMT_NV21;//getOutputFormat();

    if (preview == true)
        frmsize.pixel_format = V4L2_PIX_FMT_NV21;
    else
        frmsize.pixel_format = V4L2_PIX_FMT_RGB24;
/*
    if (preview_fmt == V4L2_PIX_FMT_MJPEG)
        frmsize.pixel_format = V4L2_PIX_FMT_MJPEG;
    else if (preview_fmt == V4L2_PIX_FMT_NV21) {
        if (preview == true)
            frmsize.pixel_format = V4L2_PIX_FMT_NV21;
        else
            frmsize.pixel_format = V4L2_PIX_FMT_RGB24;
    } else if (preview_fmt == V4L2_PIX_FMT_YVU420) {
        if (preview == true)
            frmsize.pixel_format = V4L2_PIX_FMT_YVU420;
        else
            frmsize.pixel_format = V4L2_PIX_FMT_RGB24;
    } else if (preview_fmt == V4L2_PIX_FMT_YUYV)
        frmsize.pixel_format = V4L2_PIX_FMT_YUYV;
*/
    for (i = 0; ; i++) {
        frmsize.index = i;
        res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
        if (res < 0) {
            CAMHAL_LOGD("index=%d, break\n", i);
            break;
        }


        if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE) { //only support this type

            if (0 != (frmsize.discrete.width%16))
                continue;

            if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height >support_h))
                continue;

            if (count >= size)
                break;

            picSizes[count] = frmsize.discrete.width;
            picSizes[count+1]  =  frmsize.discrete.height;

            if (0 == i) {
                count += 2;
                continue;
            }

            //TODO insert in descend order
            if (picSizes[count + 0] * picSizes[count + 1] > picSizes[count - 1] * picSizes[count - 2]) {
                picSizes[count + 0] = picSizes[count - 2];
                picSizes[count + 1] = picSizes[count - 1];

                picSizes[count - 2] = frmsize.discrete.width;
                picSizes[count - 1] = frmsize.discrete.height;
            }

            count += 2;
        }
    }

    return count;

}

status_t USBSensor::force_reset_sensor() {
    CAMHAL_LOGD("force_reset_sensor");
    status_t ret;
    mTimeOutCount = 0;
    ret = streamOff(channel_preview);
    ret = mVinfo->setBuffersFormat();
    ret = streamOn(channel_preview);
    CAMHAL_LOGD("%s , ret = %d", __FUNCTION__, ret);
    return ret;
}

int USBSensor::captureNewImage() {
    uint32_t gain = mGainFactor;
    if (mHwDecoderSensor) {
        bool isJpegRequest = false;
        for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
            const StreamBuffer &b = (*mNextCapturedBuffers)[i];
            CAMHAL_LOGVV("Sensor capturing buffer %zu: stream %d,"
                    " %d x %d, format %x, stride %d, buf %p, img %p",
                    i, b.streamId, b.width, b.height, b.format, b.stride,
                    b.buffer, b.img);
            if  (b.format == HAL_PIXEL_FORMAT_BLOB) {
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                ALOGD("bAux orientation=%d",orientation);
                uint32_t pixelfmt;
                if (1) {
                    pixelfmt = getOutputFormat();
                    if (pixelfmt == V4L2_PIX_FMT_YVU420) {
                        pixelfmt = HAL_PIXEL_FORMAT_YV12;
                    } else if (pixelfmt == V4L2_PIX_FMT_NV21) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    } else if (pixelfmt == V4L2_PIX_FMT_YUYV) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
                    } else {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    }
                }

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = pixelfmt;
                bAux.stride = b.width;
                bAux.buffer = NULL;
#ifdef GE2D_ENABLE
                bAux.img = mION->alloc_buffer(b.width * b.height * 3,&bAux.share_fd);
#else
                bAux.img = new uint8_t[b.width * b.height * 3];
#endif
                mNextCapturedBuffers->push_back(bAux);
                isJpegRequest = true;
            }
        }
        //capture nv21 vector
        captureNV21UsbSensor(*mNextCapturedBuffers, gain, isJpegRequest);
        if (mUnpluged)
            return -1;
        return 0;
    }
    mKernelBuffer = NULL;
    mTempFD = -1;
    mDecodedBuffer = NULL;
    mIsRequestFinished = false;
    bool needSensorOutBuffer = false;

    size_t buffer_num = mNextCapturedBuffers->size();
    // Might be adding more buffers, so size isn't constant
    CAMHAL_LOGVV("%s:buffer size=%zu\n",__FUNCTION__,buffer_num);
    if (buffer_num > 1) {
        needSensorOutBuffer = true;
    }
    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
        const StreamBuffer &b = (*mNextCapturedBuffers)[i];
        CAMHAL_LOGVV("Sensor capturing buffer %d: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                (int)i, b.streamId, b.width, b.height, b.format, b.stride,
                b.buffer, b.img);
        if (i == buffer_num - 1) {
                mIsRequestFinished = true;
        }
        switch (b.format) {
#if PLATFORM_SDK_VERSION <= 22
            case HAL_PIXEL_FORMAT_RAW_SENSOR:
                captureRaw(b.img, gain, b.stride);
                break;
#endif
            case HAL_PIXEL_FORMAT_RGBA_8888:
                captureRGBA(b.img, gain, b.stride);
                break;
            case HAL_PIXEL_FORMAT_BLOB:
                // Add auxiliary buffer of the right size
                // Assumes only one BLOB (JPEG) buffer in
                // mNextCapturedBuffers
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                CAMHAL_LOGD("bAux orientation=%d",orientation);
                uint32_t pixelfmt;
                if (1) {
                    pixelfmt = getOutputFormat();
                    if (pixelfmt == V4L2_PIX_FMT_YVU420) {
                        pixelfmt = HAL_PIXEL_FORMAT_YV12;
                    } else if (pixelfmt == V4L2_PIX_FMT_NV21) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    } else if (pixelfmt == V4L2_PIX_FMT_YUYV) {
                        pixelfmt = HAL_PIXEL_FORMAT_YCbCr_422_I;
                    } else {
                        pixelfmt = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                    }
                }

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = pixelfmt;
                bAux.stride = b.width;
                bAux.buffer = NULL;
#ifdef GE2D_ENABLE
                bAux.img = mION->alloc_buffer(b.width * b.height * 3,&bAux.share_fd);
#else
                bAux.img = new uint8_t[b.width * b.height * 3];
#endif
                mNextCapturedBuffers->push_back(bAux);
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                captureNV21UsbSensor(b, gain, needSensorOutBuffer);
                break;
            case HAL_PIXEL_FORMAT_YV12:
                captureYV12(b, gain);
                break;
            case HAL_PIXEL_FORMAT_YCbCr_422_I:
                captureYUYV(b.img, gain, b.stride);
                break;
            default:
                CAMHAL_LOGE("%s: Unknown format %x, no output", __FUNCTION__,
                        b.format);
                break;
        }
    }
    if (mUnpluged)
        return -1;
    return 0;
}

status_t USBSensor::readyToRun() {
    //int res;
    ATRACE_CALL();
    CAMHAL_LOGV("Starting up usb sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = NULL;
    CAMHAL_LOGD("");

    return OK;
}

void *USBSensor::DecFillBufThread(void *sensor){
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    uint8_t *src;
    USBSensor *usbSensor = (USBSensor *)sensor;
    CVideoInfo *Vinfo = usbSensor->mVinfo;
    OMXDecoder *decoder = usbSensor->mDecoder;
    uint32_t DeqCounts = 0;
    float DeqFps = 0;
    struct timeval DeqTimeEnd, DeqTimeStart;
    memset(&DeqTimeStart, 0, sizeof(struct timeval));
    memset(&DeqTimeEnd, 0, sizeof(struct timeval));
    gettimeofday(&DeqTimeStart, NULL);
    bool firstIDRfilled = false;

    while (1) {
        if (usbSensor->mDecFillBufThreadNeedStop.load())
            break;

        fd_set fds;
        struct timeval tv;
        int r;
        if (Vinfo->fd <= 0)
            break;
        FD_ZERO(&fds);
        FD_SET(Vinfo->fd, &fds);
        /*2s Timeout*/
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        r = select(Vinfo->fd + 1, &fds, NULL, NULL, &tv);
        if (-1 == r) {
            if (EINTR == errno)
                continue;
            CAMHAL_LOGD("select error:%s",strerror(errno));
        }

        if (0 == r) {
            CAMHAL_LOGD("select timeout:%s",strerror(errno));
            if (usbSensor->mUseHwType == HW_H264) {
                AutoMutex l(usbSensor->mDecFillThreadResetLock);
                if (!usbSensor->mDecFillThreadNeedReset) {
                    usbSensor->mDecFillThreadNeedReset = true;
                    break;
                }
            } else {
                if (usbSensor->mSelectCount++ < 4) {
                    continue;
                }
            }
        }

        src = (uint8_t *)Vinfo->get_frame();
        usbSensor->mSelectCount = 0;
        DeqCounts ++;
        if (DeqCounts == 30) {
            gettimeofday(&DeqTimeEnd, NULL);
            int64_t interval = ( int64_t )(DeqTimeEnd.tv_sec - DeqTimeStart.tv_sec) * 1000000L
                          + (DeqTimeEnd.tv_usec - DeqTimeStart.tv_usec);
            DeqFps = DeqCounts/(interval/1000000.0f);
            memcpy(&DeqTimeStart, &DeqTimeEnd, sizeof(DeqTimeEnd));
            DeqCounts = 0;
            CAMHAL_LOGI("async dq buffer interval(%" PRId64"), interval=%f, mDeqFps=%f\n", interval, interval/1000000.0f, DeqFps);
        }

        if (NULL == src) {
            if (Vinfo->get_device_status()) {
                break;
            }
            AutoMutex l(usbSensor->mDecFillThreadResetLock);
            if (!usbSensor->mDecFillThreadNeedReset) {
                usbSensor->mDecFillThreadNeedReset = true;
                break;
            }
        }
        if (usbSensor->mUseHwType == HW_H264 ) {
            // start with IDR frame. [normally after v4l2 setting. first frame is IDR.]
            // this code segment take effect in case abnormal things occurs.
            if (!firstIDRfilled && !isIDR(usbSensor->mUseHwType, src, Vinfo->preview.buf.bytesused)) {
                // not IDR filled.
                ALOGD("H264 bs not IDR, skip it");
                Vinfo->putback_frame();
                continue;
            } else if (false == firstIDRfilled) {
                ALOGI("h264, first idr queue.");
                firstIDRfilled = true;
            }
        }
        {
            AutoMutex l(usbSensor->mDecFillThreadWaitLock);
            decoder->PutInBuffer(src, Vinfo->preview.buf.bytesused);
        }
        Vinfo->putback_frame();
    }
    return((void *)0);
}
int USBSensor::DecFillBufThreadStart()
{
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    int ret = 0;
    mDecFillBufThreadNeedStop.store(false);
    ret = pthread_create(&mDecFillBufThreadTid, NULL, USBSensor::DecFillBufThread, this);
    if (ret != 0)
        CAMHAL_LOGE("****create thread fail\n");
    return ret;
}

void USBSensor::DecFillBufThreadStop()
{
    if (!(mDecFillBufThreadNeedStop.load())) {
        mDecFillBufThreadNeedStop.store(true);
        pthread_join(mDecFillBufThreadTid, NULL);
    }
}

int USBSensor::ResetSensorAndDecoder()
{
    int ret = 0;
    ret = streamOff(channel_preview);
    if (ret < 0) {
        CAMHAL_LOGE("stream off failed!");
        return ret;
    }
    ret = mVinfo->setBuffersFormat();
    if (ret < 0) {
        CAMHAL_LOGE("set buffer format failed!");
        return ret;
    }
    if (!mIsDecoderInit)
        initDecoder(mVinfo->preview.format.fmt.pix.width, mVinfo->preview.format.fmt.pix.height,
                    mVinfo->preview.format.fmt.pix.width, mVinfo->preview.format.fmt.pix.height, 4);
    ret = streamOn(channel_preview);
    if (ret < 0) {
        CAMHAL_LOGE("stream on failed!");
        return ret;
    }
    return ret;
}

}


