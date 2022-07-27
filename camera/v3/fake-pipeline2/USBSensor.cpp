#define LOG_NDEBUG  0
#define LOG_NNDEBUG 0

#define LOG_TAG "USBSensor"

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
#include "USBSensor.h"
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

const usb_frmsize_discrete_t kUsbAvailablePictureSize[] = {
        {4128, 3096},
        {3840, 2160},
        {3264, 2448},
        {2592, 1944},
        {2592, 1936},
        {2560, 1920},
        {2688, 1520},
        {2048, 1536},
        {1600, 1200},
        {1920, 1088},
        {1920, 1080},
        {1440, 1080},
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


USBSensor::USBSensor(int type)
{
    mUseHwType = type;
    mDecodeMethod = DECODE_SOFTWARE;
    mImage_buffer = NULL;
#ifdef GE2D_ENABLE
    mION = IONInterface::get_instance();
    mGE2D = new ge2dTransform();
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
    ALOGD("create usbsensor");
}

USBSensor::~USBSensor() {
    ALOGV("%s: E", __FUNCTION__);
    if (mVinfo) {
        delete(mVinfo);
        mVinfo = NULL;
    }
    if (mCameraUtil) {
        delete mCameraUtil;
        mCameraUtil = NULL;
    }
#ifdef GE2D_ENABLE
    if (mION) {
        mION->put_instance();
    }
    if (mGE2D) {
        delete mGE2D;
        mGE2D = nullptr;
    }
#endif
    ALOGD("delete usbsensor");
};

int USBSensor::camera_open(int idx)
{
    char dev_name[128];
    int ret = 0;
    char property[PROPERTY_VALUE_MAX];
    ALOGV("%s: E", __FUNCTION__);


    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();

    mUSBDevicefd = mCameraVirtualDevice->openVirtualDevice(idx);
    if (mUSBDevicefd < 0) {
        DBG_LOGB("open %s failed, errno=%d\n", dev_name, errno);
        ret = -ENOTTY;
    }
    property_get("ro.vendor.platform.omx", property, "false");
    if (strstr(property, "true"))
        mDecodeMethod = DECODE_OMX;
    return ret;
}

void USBSensor::camera_close(void)
{
    ALOGV("%s: E", __FUNCTION__);
    if (mUSBDevicefd < 0)
        return;
    if (mCameraVirtualDevice == nullptr)
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    if (mVinfo != NULL)
        mCameraVirtualDevice->releaseVirtualDevice(mVinfo->idx,mUSBDevicefd);
    mUSBDevicefd = -1;
}

void USBSensor::InitVideoInfo(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    if (mVinfo && mUSBDevicefd >= 0) {
        mVinfo->fd = mUSBDevicefd;
        mVinfo->idx = idx;
    }else
        ALOGD("%s: init fail", __FUNCTION__);
}

int USBSensor::SensorInit(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    int ret = 0;
    if (mVinfo == NULL)
        mVinfo =  new CVideoInfo();
    ret = camera_open(idx);
    if (ret < 0) {
        ALOGE("Unable to open sensor %d, errno=%d\n", mVinfo->idx, ret);
        return ret;
    }
    InitVideoInfo(idx);
    mVinfo->camera_init();
    setIOBufferNum();
    mSensorType = SENSOR_USB;
    return ret;
}

status_t USBSensor::startUp(int idx) {
    ALOGV("%s: E", __FUNCTION__);
    DBG_LOGA("ddd");
    int res;
    mCapturedBuffers = NULL;
    mOpenCameraID = idx;
    res = run("Camera::USBSensor",ANDROID_PRIORITY_URGENT_DISPLAY);
    if (res != OK) {
        ALOGE("Unable to start up sensor capture thread: %d", res);
        return res;
    }
    res = SensorInit(idx);
    if (!mCameraUtil)
        mCameraUtil = new CameraUtil();
    switch (mDecodeMethod) {
        case DECODE_OMX:
            if (!mDecoder)
                mDecoder = new OMXDecoder(true, true);
            break;
        default:
            break;
    }
    return res;
}

uint32_t USBSensor::getStreamUsage(int stream_type){
    ATRACE_CALL();
#if 0
    uint32_t usage = Sensor::getStreamUsage(stream_type);
    usage = (GRALLOC_USAGE_HW_TEXTURE
            | GRALLOC_USAGE_HW_RENDER
            | GRALLOC_USAGE_SW_READ_MASK
            | GRALLOC_USAGE_SW_WRITE_MASK
            );

#if ANDROID_PLATFORM_SDK_VERSION >= 28
        usage = am_gralloc_get_omx_osd_producer_usage();
#else
        usage = GRALLOC_USAGE_HW_VIDEO_ENCODER | GRALLOC_USAGE_AML_DMA_BUFFER;
#endif
#endif
    uint32_t usage = GRALLOC1_PRODUCER_USAGE_CAMERA;
    ALOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;
}

status_t USBSensor::setOutputFormat(int width, int height,
    int pixelformat, bool isjpeg){
    int res;
    mFramecount = 0;
    mCurFps = 0;
    gettimeofday(&mTimeStart, NULL);
    initDecoder(width, height, width, height,4);
    if (isjpeg) {
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
            ALOGE("set buffer failed\n");
            return res;
        }
    }
        if (NULL == mImage_buffer) {
            mPre_width = mVinfo->preview.format.fmt.pix.width;
            mPre_height = mVinfo->preview.format.fmt.pix.height;
            DBG_LOGB("setOutputFormat :: pre_width = %d, pre_height = %d \n" , mPre_width , mPre_height);
            mImage_buffer = new uint8_t[mPre_width * mPre_height * 3 / 2];
            if (mImage_buffer == NULL) {
                ALOGE("first time allocate mTemp_buffer failed !");
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
                    ALOGE("allocate mTemp_buffer failed !");
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

status_t USBSensor::streamOn() {
    return mVinfo->start_capturing();

}
bool USBSensor::isStreaming() {
    return mVinfo->isStreaming;
}


bool USBSensor::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat) {
    if ((mVinfo->preview.format.fmt.pix.width != width)
        ||(mVinfo->preview.format.fmt.pix.height != height)) {
        return true;
    }
    return false;
}

void USBSensor::initDecoder(int in_width, int in_height, int out_width, int out_height, int out_bufferCount) {
    ALOGV("%s: in_width=%d, in_height=%d out_width=%d, out_height=%d",
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
    }
}

status_t USBSensor::shutDown() {
    ALOGV("%s: E", __FUNCTION__);
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
        ALOGE("Unable to shut down sensor capture thread: %d", res);
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
    ALOGD("%s: Exit", __FUNCTION__);
    return res;
}

status_t USBSensor::streamOff(void){
    ALOGV("%s: E", __FUNCTION__);
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
        ALOGD("get property value is %d\n",tmp);
    } else {
        ALOGD("defalut buffer number is %d\n",tmp);
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
    if (ret)
        return ret;

    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret)
        return ret;
    ALOGE("Unable to find a supported sensor format!");
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

    ALOGE("%s, Unable to find a supported sensor format!", __FUNCTION__);
    return BAD_VALUE;
}

void USBSensor::captureNV21UsbSensor(StreamBuffer b, uint32_t gain, bool needSensorOutBuf) {
    ALOGVV("%s: E", __FUNCTION__);
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
                if (mTempFD != -1)
                    mGE2D->ge2d_copy(b.share_fd,mTempFD,b.stride,b.height,ge2dTransform::NV12);
                else
                    memcpy(b.img, src, b.stride * b.height * 3/2);
#else
                memcpy(b.img, src, b.stride * b.height * 3/2);
#endif
            } else {
#ifdef GE2D_ENABLE
                if (mTempFD != -1) {
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
            ALOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        return ;
    }
    while (1) {
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
                ALOGD("select error:%s",strerror(errno));
            }
            if (0 == r)
                ALOGD("select timeout:%s",strerror(errno));
            src = (uint8_t *)mVinfo->get_frame();
            if (NULL == src) {
                if (mVinfo->get_device_status()) {
                    camera_close();
                    break;
                }
                ALOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
                usleep(5000);

                mTimeOutCount++;
                if (mTimeOutCount > 600) {
                    DBG_LOGA("force sensor reset.\n");
                    force_reset_sensor();
                }

                continue;
            }
            mTimeOutCount = 0;
        pixelformat = mVinfo->preview.format.fmt.pix.pixelformat;
        switch (pixelformat) {
            case V4L2_PIX_FMT_NV21:
                if (mVinfo->preview.buf.length == b.width * b.height * 3/2) {
                    memcpy(b.img, src, mVinfo->preview.buf.length);
                } else {
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
                    mCameraUtil->YUYVToNV21(src, mImage_buffer, width, height);
                    if ((width == b.width) && (height == b.height)) {
                        memcpy(b.img, mImage_buffer, b.width * b.height * 3/2);
                    } else {
                        if ((b.height % 2) != 0) {
                            DBG_LOGB("%d , b.height = %d", __LINE__, b.height);
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
                        && ( b.width != mVinfo->preview.format.fmt.pix.width || b.height != mVinfo->preview.format.fmt.pix.width)) {
                          // need store sensor output buffer AND b.size not equal to sensor output size
                          // first - decode to mSensorOutBuf; then resize to b
                        if (mSensorOutBuf.img == NULL || mSensorOutBuf.share_fd < 0) {
                            // here we need mSensorOutBuf has been ready.
                            ALOGE("mSensorOutBuf not allocated.");
                        } else {
                            ret = MJPEGToNV21(src, mSensorOutBuf);
#ifdef GE2D_ENABLE
                            mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          mSensorOutBuf.share_fd, mSensorOutBuf.width, mSensorOutBuf.height);
#else
                            mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, mSensorOutBuf.width, mSensorOutBuf.height);
#endif
                        }
                    } else {
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
            case V4L2_PIX_FMT_H264:
                {
                    int ret = 1;
                    if (needSensorOutBuf
                        && ( b.width != mVinfo->preview.format.fmt.pix.width || b.height != mVinfo->preview.format.fmt.pix.width)) {
                          // need store sensor output buffer AND b.size not equal to sensor output size
                          // first - decode to mSensorOutBuf; then resize to b
                        if (mSensorOutBuf.img == NULL || mSensorOutBuf.share_fd < 0) {
                            // here we need mSensorOutBuf has been ready.
                            ALOGE("mSensorOutBuf not allocated.");
                        } else {
                            ret = H264ToNV21(src, mSensorOutBuf);
#ifdef GE2D_ENABLE
                            mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          mSensorOutBuf.share_fd, mSensorOutBuf.width, mSensorOutBuf.height);
#else
                            mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, mSensorOutBuf.width, mSensorOutBuf.height);
#endif
                        }
                    } else {
                        ret = H264ToNV21(src, b);
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
                ALOGD("not support this format");
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

int USBSensor::MJPEGToNV21(uint8_t* src, StreamBuffer b) {
    ALOGVV("%s: E, src=0x%p b.w=%d", __FUNCTION__, src, b.width);
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
                    ALOGE("software decoder error \n");
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
        case DECODE_OMX:
            {
                int ret = mDecoder->Decode(src, src_length,
                                            b.share_fd, b.img,
                                            src_width, src_height,
                                            b.width, b.height);
                if (!ret) {
                    flag = 1;
                    if (mDecoder->mTimeOut && mIsDecoderInit == true) {
                        mDecoder->deinitialize();
                        mIsDecoderInit = false;
                        initDecoder(src_width, src_height,
                                    b.width, b.height, 4);
                    }
                } else {
                    mDecodedBuffer = b.img;
                    mKernelBuffer = src;
                    mTempFD = b.share_fd;
                }
            }
            break;
        default:
            ALOGD("not support this decode method");
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

int USBSensor::H264ToNV21(uint8_t* src, StreamBuffer b) {
    int flag = 0;
    size_t src_width = mVinfo->preview.format.fmt.pix.width;
    size_t src_height = mVinfo->preview.format.fmt.pix.height;
    size_t src_length = mVinfo->preview.buf.bytesused;

    int ret = mDecoder->Decode(src, src_length,
                                b.share_fd, b.img,
                                src_width, src_height,
                                b.width, b.height);
    if (!ret) {
        flag = 1;
        if (mDecoder->mTimeOut && mIsDecoderInit == true) {
            mDecoder->deinitialize();
            mIsDecoderInit = false;
            initDecoder(src_width, src_height,
                        b.width, b.height, 4);
        }
    } else {
        mDecodedBuffer = b.img;
        mKernelBuffer = src;
        mTempFD = b.share_fd;
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
            ALOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
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
            ALOGD("select error:%s",strerror(errno));
        }
        if (0 == r)
        ALOGD("select timeout:%s",strerror(errno));

        src = (uint8_t *)mVinfo->get_frame();

        if (NULL == src) {
            ALOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
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
                CAMHAL_LOGDB("length=%d, bytesused=%d \n", mVinfo->preview.buf.length, mVinfo->preview.buf.bytesused);
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
            ALOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
        }
        mSensorWorkFlag = true;
        mVinfo->putback_frame();
        if (mFlushFlag)
            break;
        if (mExitSensorThread)
            break;
        break;
    }
    ALOGVV("YV12 sensor image captured");
}
void USBSensor::captureYUYV(uint8_t *img, uint32_t gain, uint32_t stride){
    uint8_t *src;
    if (mKernelBuffer) {
        src = mKernelBuffer;
        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            //TODO YUYV scale
            //memcpy(img, src, vinfo->preview.buf.length);

        } else
            ALOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);

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
            ALOGD("select error:%s",strerror(errno));
        }
        if (0 == r)
        ALOGD("select timeout:%s",strerror(errno));
        src = (uint8_t *)mVinfo->get_frame();
        if (NULL == src) {
            ALOGVV("%s:get frame NULL, sleep 5ms",__FUNCTION__);
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
                CAMHAL_LOGDB("length=%d, bytesused=%d \n", mVinfo->preview.buf.length, mVinfo->preview.buf.bytesused);
                mVinfo->putback_frame();
                continue;
            }
        }

        if (mVinfo->preview.format.fmt.pix.pixelformat == V4L2_PIX_FMT_YUYV) {
            memcpy(img, src, mVinfo->preview.buf.length);
            mKernelBuffer = src;
        } else {
            ALOGE("Unable known sensor format: %d", mVinfo->preview.format.fmt.pix.pixelformat);
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
    ALOGVV("YUYV sensor image captured");
}

void USBSensor::dump(int& frame_index, uint8_t* buf, int length, std::string name) {
    ALOGD("%s:frame_index= %d",__FUNCTION__,frame_index);
    const int frame_num = 10;
    FILE* fp = NULL;
    if (frame_index > frame_num)
        return;
    else if (frame_index == 0) {
        std::string path("/data/vendor/camera/");
        path.append(name);
        ALOGD("full_name:%s",path.c_str());

        fp = fopen(path.c_str(),"ab+");
        if (!fp) {
            ALOGE("open file %s fail, error: %s !!!",
                    path.c_str(),strerror(errno));
            return;
        }
    }
    if (fp == NULL)
        return;
    if (frame_index++ == frame_num) {
        int fd = fileno(fp);
        fsync(fd);
        fclose(fp);
        close(fd);
        return ;
    } else  {
        ALOGE("write frame %d ",frame_index);
        fwrite((void*)buf,1,length,fp);
        fclose(fp);
    }
}

int USBSensor::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    int ret = 0;
    struct v4l2_queryctrl qc;
    memset(&qc, 0, sizeof(qc));
    qc.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ((qc.flags == V4L2_CTRL_FLAG_DISABLED) || ( ret < 0)
        || (qc.type != V4L2_CTRL_TYPE_INTEGER)) {
        ret = -1;
        *zoomMin = 0;
        *zoomMax = 0;
        *zoomStep = 1;
        CAMHAL_LOGDB("%s: Can't get zoom level!\n", __FUNCTION__);
    } else {
        if ((qc.step != 0) && (qc.minimum != 0) &&
            ((qc.minimum/qc.step) > (qc.maximum/qc.minimum))) {
                DBG_LOGA("adjust zoom step. \n");
                qc.step = (qc.minimum * qc.step);
            }
        *zoomMin = qc.minimum;
        *zoomMax = qc.maximum;
        *zoomStep = qc.step;
        DBG_LOGB("zoomMin:%dzoomMax:%dzoomStep:%d\n", *zoomMin, *zoomMax, *zoomStep);
    }
    return ret ;
}

int USBSensor::setZoom(int zoomValue) {
    int ret = 0;
    struct v4l2_control ctl;
    memset( &ctl, 0, sizeof(ctl));
    ctl.value = zoomValue;
    ctl.id = V4L2_CID_ZOOM_ABSOLUTE;
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        ALOGE("%s: Set zoom level failed!\n", __FUNCTION__);
        }
    return ret ;
}

status_t USBSensor::setEffect(uint8_t effect) {
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_COLORFX;
    switch (effect) {
        case ANDROID_CONTROL_EFFECT_MODE_OFF:
        ctl.value= CAM_EFFECT_ENC_NORMAL;
        break;
        case ANDROID_CONTROL_EFFECT_MODE_NEGATIVE:
        ctl.value= CAM_EFFECT_ENC_COLORINV;
        break;
        case ANDROID_CONTROL_EFFECT_MODE_SEPIA:
        ctl.value= CAM_EFFECT_ENC_SEPIA;
        break;
        default:
        ALOGE("%s: Doesn't support effect mode %d",
        __FUNCTION__, effect);
        return BAD_VALUE;
    }
    DBG_LOGB("set effect mode:%d", effect);
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if (ret < 0)
        CAMHAL_LOGDB("Set effect fail: %s. ret=%d", strerror(errno),ret);
    return ret ;
 }


const int MAX_LEVEL_FOR_EXPOSURE = 16;
const int MIN_LEVEL_FOR_EXPOSURE = 3;

void USBSensor::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}

int USBSensor::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step)
{
    struct v4l2_queryctrl qc;
    int ret=0;
    int level = 0;
    int middle = 0;

    memset( &qc, 0, sizeof(qc));

        DBG_LOGA("getExposure\n");
    qc.id = V4L2_CID_EXPOSURE;
    ret = ioctl(mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if (ret < 0) {
        CAMHAL_LOGDB("QUERYCTRL failed, errno=%d\n", errno);
        *minExp = -4;
        *maxExp = 4;
        *def = 0;
        step->numerator = 1;
        step->denominator = 1;
        return ret;
    }

    if (0 < qc.step)
        level = ( qc.maximum - qc.minimum + 1 )/qc.step;

    if ((level > MAX_LEVEL_FOR_EXPOSURE)
      || (level < MIN_LEVEL_FOR_EXPOSURE)) {
        *minExp = -4;
        *maxExp = 4;
        *def = 0;
        step->numerator = 1;
        step->denominator = 1;
        DBG_LOGB("not in[min,max], min=%d, max=%d, def=%d\n",
                                        *minExp, *maxExp, *def);
        return true;
    }

    middle = (qc.minimum+qc.maximum)/2;
    *minExp = qc.minimum - middle;
    *maxExp = qc.maximum - middle;
    *def = qc.default_value - middle;
    step->numerator = 1;
    step->denominator = 2;//qc.step;
        DBG_LOGB("min=%d, max=%d, step=%d\n", qc.minimum, qc.maximum, qc.step);
    return ret;
}

status_t USBSensor::setExposure(int expCmp)
{
    int ret = 0;
    struct v4l2_control ctl;
    struct v4l2_queryctrl qc;

    if (mEV == expCmp) {
        return 0;
    }else{
        mEV = expCmp;
    }
    memset(&ctl, 0, sizeof(ctl));
    memset(&qc, 0, sizeof(qc));

    qc.id = V4L2_CID_EXPOSURE;

    ret = ioctl(mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if (ret < 0) {
        CAMHAL_LOGDB("AMLOGIC CAMERA get Exposure fail: %s. ret=%d", strerror(errno),ret);
    }

    ctl.id = V4L2_CID_EXPOSURE;
    ctl.value = expCmp + (qc.maximum - qc.minimum) / 2;

    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if (ret < 0) {
        CAMHAL_LOGDB("AMLOGIC CAMERA Set Exposure fail: %s. ret=%d", strerror(errno),ret);
    }
        DBG_LOGB("setExposure value%d mEVmin%d mEVmax%d\n",ctl.value, qc.minimum, qc.maximum);
    return ret ;
}

int USBSensor::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_POWER_LINE_FREQUENCY;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->fd);
    } else if ( qc.type != V4L2_CTRL_TYPE_INTEGER) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        antiBanding[0] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCont)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_POWER_LINE_FREQUENCY;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"50hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"60hz") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"auto") == 0) {
                    antiBanding[mode_count] = ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensor::setAntiBanding(uint8_t antiBanding)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_POWER_LINE_FREQUENCY;

    switch (antiBanding) {
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_OFF:
        ctl.value= CAM_ANTIBANDING_OFF;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_50HZ:
        ctl.value= CAM_ANTIBANDING_50HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_60HZ:
        ctl.value= CAM_ANTIBANDING_60HZ;
        break;
    case ANDROID_CONTROL_AE_ANTIBANDING_MODE_AUTO:
        ctl.value= CAM_ANTIBANDING_AUTO;
        break;
    default:
            ALOGE("%s: Doesn't support ANTIBANDING mode %d",
                    __FUNCTION__, antiBanding);
            return BAD_VALUE;
    }

    DBG_LOGB("anti banding mode:%d", antiBanding);
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    if ( ret < 0) {
        CAMHAL_LOGDA("failed to set anti banding mode!\n");
        return BAD_VALUE;
    }
    return ret;
}

status_t USBSensor::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_ABSOLUTE;
    ctl.value = ((x0 + x1) / 2 + 1000) << 16;
    ctl.value |= ((y0 + y1) / 2 + 1000) & 0xffff;

    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    return ret;
}


int USBSensor::getAutoFocus(uint8_t *afMode, uint8_t maxCount)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_FOCUS_AUTO;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->fd);
    }else if( qc.type != V4L2_CTRL_TYPE_MENU) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    }else{
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        afMode[0] = ANDROID_CONTROL_AF_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_FOCUS_AUTO;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-video") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"continuous-picture") == 0) {
                    afMode[mode_count] = ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensor::setAutoFocus(uint8_t afMode)
{
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_FOCUS_AUTO;

    switch (afMode) {
        case ANDROID_CONTROL_AF_MODE_AUTO:
            ctl.value = CAM_FOCUS_MODE_AUTO;
            break;
        case ANDROID_CONTROL_AF_MODE_MACRO:
            ctl.value = CAM_FOCUS_MODE_MACRO;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO:
            ctl.value = CAM_FOCUS_MODE_CONTI_VID;
            break;
        case ANDROID_CONTROL_AF_MODE_CONTINUOUS_PICTURE:
            ctl.value = CAM_FOCUS_MODE_CONTI_PIC;
            break;
        default:
            ALOGE("%s: Emulator doesn't support AF mode %d",
                    __FUNCTION__, afMode);
            return BAD_VALUE;
    }

    if (ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl) < 0) {
        CAMHAL_LOGDA("failed to set camera focuas mode!\n");
        return BAD_VALUE;
    }

    return OK;
}

int USBSensor::getAWB(uint8_t *awbMode, uint8_t maxCount)
{
    struct v4l2_queryctrl qc;
    struct v4l2_querymenu qm;
    int ret;
    int mode_count = -1;

    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_CID_DO_WHITE_BALANCE;
    ret = ioctl (mVinfo->fd, VIDIOC_QUERYCTRL, &qc);
    if ( (ret<0) || (qc.flags == V4L2_CTRL_FLAG_DISABLED)) {
        DBG_LOGB("camera handle %d can't support this ctrl",mVinfo->fd);
    } else if( qc.type != V4L2_CTRL_TYPE_MENU) {
        DBG_LOGB("this ctrl of camera handle %d can't support menu type",mVinfo->fd);
    } else {
        memset(&qm, 0, sizeof(qm));

        int index = 0;
        mode_count = 1;
        awbMode[0] = ANDROID_CONTROL_AWB_MODE_OFF;

        for (index = qc.minimum; index <= qc.maximum; index+= qc.step) {
            if (mode_count >= maxCount)
                break;

            memset(&qm, 0, sizeof(struct v4l2_querymenu));
            qm.id = V4L2_CID_DO_WHITE_BALANCE;
            qm.index = index;
            if (ioctl (mVinfo->fd, VIDIOC_QUERYMENU, &qm) < 0) {
                continue;
            } else {
                if (strcmp((char*)qm.name,"auto") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_AUTO;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"incandescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_INCANDESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"warm-fluorescent") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_WARM_FLUORESCENT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"cloudy-daylight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_CLOUDY_DAYLIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"twilight") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_TWILIGHT;
                    mode_count++;
                } else if (strcmp((char*)qm.name,"shade") == 0) {
                    awbMode[mode_count] = ANDROID_CONTROL_AWB_MODE_SHADE;
                    mode_count++;
                }

            }
        }
    }

    return mode_count;
}

status_t USBSensor::setAWB(uint8_t awbMode)
{
    int ret = 0;
    struct v4l2_control ctl;
    ctl.id = V4L2_CID_DO_WHITE_BALANCE;

    switch (awbMode) {
        case ANDROID_CONTROL_AWB_MODE_AUTO:
            ctl.value = CAM_WB_AUTO;
            break;
        case ANDROID_CONTROL_AWB_MODE_INCANDESCENT:
            ctl.value = CAM_WB_INCANDESCENCE;
            break;
        case ANDROID_CONTROL_AWB_MODE_FLUORESCENT:
            ctl.value = CAM_WB_FLUORESCENT;
            break;
        case ANDROID_CONTROL_AWB_MODE_DAYLIGHT:
            ctl.value = CAM_WB_DAYLIGHT;
            break;
        case ANDROID_CONTROL_AWB_MODE_SHADE:
            ctl.value = CAM_WB_SHADE;
            break;
        default:
            ALOGE("%s: Emulator doesn't support AWB mode %d",
                    __FUNCTION__, awbMode);
            return BAD_VALUE;
    }
    ret = ioctl(mVinfo->fd, VIDIOC_S_CTRL, &ctl);
    return ret;
}

const char* USBSensor::getformt(int id) {
    static char str[128];
    memset(str,0,sizeof(str));

    switch (id) {
        case V4L2_PIX_FMT_MJPEG:
            sprintf(str,"%s","V4L2_PIX_FMT_MJPEG");
            break;
        case V4L2_PIX_FMT_H264:
            sprintf(str,"%s","V4L2_PIX_FMT_H264");
            break;
        case V4L2_PIX_FMT_RGB24:
            sprintf(str,"%s","V4L2_PIX_FMT_RGB24");
            break;
        case V4L2_PIX_FMT_YUYV:
            sprintf(str,"%s","V4L2_PIX_FMT_YUYV");
            break;
        default:
            sprintf(str,"%s","not support");
            break;
    };
    return str;
}

int USBSensor::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size) {
    int res;
    int i, j, k, START;
    int count = 0;
    //int pixelfmt;
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
        res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
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

            picSizes[count+0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format=%s\n",
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

            picSizes[count+0] = HAL_PIXEL_FORMAT_YCbCr_420_888;
            picSizes[count+1] = frmsize.discrete.width;
            picSizes[count+2] = frmsize.discrete.height;
            picSizes[count+3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

            DBG_LOGB("get output width=%d, height=%d, format=HAL_PIXEL_FORMAT_YCbCr_420_888\n",
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

    uint32_t jpgSrcfmt[] = {
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_YUYV,
    };

    START = count;
    for (j = 0; j<(int)(sizeof(jpgSrcfmt)/sizeof(jpgSrcfmt[0])); j++) {
        memset(&frmsize,0,sizeof(frmsize));
        frmsize.pixel_format = jpgSrcfmt[j];

        for (i = 0; ; i++) {
            frmsize.index = i;
            res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
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

                if ((frmsize.pixel_format == V4L2_PIX_FMT_MJPEG)
                    || (frmsize.pixel_format == V4L2_PIX_FMT_YUYV)) {
                    if (!IsUsbAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
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

int USBSensor::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag)
{
     int ret=0; int framerate=0; int temp_rate=0;
    struct v4l2_frmivalenum fival;
    int i,j=0;
    int count = 0;
    int tmp_size = size;
    memset(duration, 0 ,sizeof(int64_t) * size);
    int pixelfmt_tbl[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_YVU420,
        V4L2_PIX_FMT_NV21,
        V4L2_PIX_FMT_RGB24,
        V4L2_PIX_FMT_YUYV,
        //V4L2_PIX_FMT_YVU420
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
                if ((ret = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
                    if (fival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
                        if ( fival.discrete.numerator != 0 ) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)(picSizes[size-4]);
                        duration[count+1] = (int64_t)(picSizes[size-3]);
                        duration[count+2] = (int64_t)(picSizes[size-2]);
                        if ( framerate != 0 ) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
                        j++;
                    } else if (fival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
                        if ( fival.discrete.numerator != 0 ) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)picSizes[size-4];
                        duration[count+1] = (int64_t)picSizes[size-3];
                        duration[count+2] = (int64_t)picSizes[size-2];
                        if ( framerate != 0 ) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
                        j++;
                    } else if (fival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
                        if ( fival.discrete.numerator != 0 ) temp_rate = fival.discrete.denominator/fival.discrete.numerator;
                        if (framerate < temp_rate)
                            framerate = temp_rate;
                        duration[count+0] = (int64_t)picSizes[size-4];
                        duration[count+1] = (int64_t)picSizes[size-3];
                        duration[count+2] = (int64_t)picSizes[size-2];
                        if ( framerate != 0 ) duration[count+3] = (int64_t)((1.0/framerate) * 1000000000);
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
                            else
                                duration[count+3] = (int64_t)33333333L;
                        } else if (framerate == 60) {
                            if ((!flag) && ((duration[count+0] == HAL_PIXEL_FORMAT_YCbCr_420_888)
                                || (duration[count+0] == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)))
                                duration[count+3] = 0;
                            else {
                                if (mSensorType == SENSOR_USB)
                                    duration[count+3] = (int64_t)33333333L;
                                else
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

    //CAMHAL_LOGDB("enum frameDuration=%lld\n", frameDuration);
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
        CAMHAL_LOGDB("support Max Preview Size :%s",property);
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
    DBG_LOGA("force_reset_sensor");
    status_t ret;
    mTimeOutCount = 0;
    ret = streamOff();
    ret = mVinfo->setBuffersFormat();
    ret = streamOn();
    DBG_LOGB("%s , ret = %d", __FUNCTION__, ret);
    return ret;
}

int USBSensor::captureNewImage() {
    uint32_t gain = mGainFactor;
    mKernelBuffer = NULL;
    mTempFD = -1;
    mDecodedBuffer = NULL;
    mIsRequestFinished = false;

    bool needSensorOutBuffer = false;

    size_t buffer_num = mNextCapturedBuffers->size();
    // Might be adding more buffers, so size isn't constant
    ALOGVV("%s:buffer size=%d\n",__FUNCTION__,buffer_num);
    if (buffer_num > 1) {
        needSensorOutBuffer = true;
    }
    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
        const StreamBuffer &b = (*mNextCapturedBuffers)[i];
        ALOGVV("Sensor capturing buffer %d: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                i, b.streamId, b.width, b.height, b.format, b.stride,
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
                // Add auxillary buffer of the right size
                // Assumes only one BLOB (JPEG) buffer in
                // mNextCapturedBuffers
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
                ALOGE("%s: Unknown format %x, no output", __FUNCTION__,
                        b.format);
                break;
        }
    }
    return 0;
}

}

