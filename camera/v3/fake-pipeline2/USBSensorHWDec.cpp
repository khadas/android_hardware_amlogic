#define LOG_TAG "USBSensorHWDec"

#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Log.h>
#include <utils/Trace.h>
#include <cutils/properties.h>
#include <android/log.h>

#include "../EmulatedFakeCamera3.h"
#include "Sensor.h"
#include "USBSensorHWDec.h"

#include <gralloc1.h>
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif
#include <string>

#define ARRAY_SIZE(x) (sizeof((x))/sizeof(((x)[0])))
#define ALIGN(x, align) ((x) + (align -1) & (~(align -1)))

namespace android {

static const usb_frmsize_discrete_t kUsbAvailablePictureSize[] = {
        {4128, 3096},
        {3840, 2160},
        {3264, 2448},
#ifndef VICP_ENABLE
        {2592, 1944},
#endif
        {2560, 1920},
        {2048, 1536},
        {1600, 1200},
        {1920, 1080},
#ifndef VICP_ENABLE
        {1440, 1080},
#endif
        {1280, 960},
        {1280, 720},
        {1024, 768},
        {960, 720},
        {640, 480},
#ifndef VICP_ENABLE
        {352, 288},
#endif
        {320, 240},
};

static char property[PROPERTY_VALUE_MAX];

static bool IsAvailablePictureSize(const usb_frmsize_discrete_t AvailablePictureSize[], uint32_t width, uint32_t height)
{
    int i;
    int count = sizeof(kUsbAvailablePictureSize)/sizeof(kUsbAvailablePictureSize[0]);

    if (width > AvailablePictureSize[0].width || width < AvailablePictureSize[count-1].width) {
        return false;
    }

    for (i = 0; i < count; i++) {
        if ((width == AvailablePictureSize[i].width) && (height == AvailablePictureSize[i].height)) {
            return true;
        } else {
            continue;
        }
    }
    return false;
}

static bool determineUseH264(const uint32_t width, const uint32_t height)
{
    uint32_t base_w = property_get_int32("vendor.media.camera.h264.width", 3840);
    uint32_t base_h = property_get_int32("vendor.media.camera.h264.height", 2160);
    CAMHAL_LOGV("base width %d, base height %d", base_w, base_h);
    if (property_get_bool("vendor.media.camera.force.h264", false)) {
        CAMHAL_LOGD("default choose h264");
        return true;
    } else if ((width >= base_w) && (height >= base_h)) {
        return true;
    }
    return false;
}

USBSensorHWDec::USBSensorHWDec(int expectedV4l2OutPixFmt)
{
    mExpectedV4l2OutPixFmt = expectedV4l2OutPixFmt;

    mDecoderMethod = DECODE_HARDWARE;

    v4l2OutDumpFp = NULL;
    decOutDumpFp  = NULL;

#ifdef GE2D_ENABLE
    mION = IONInterface::get_instance();
    mGE2D = new ge2dTransform();
#endif
    mSensorOutBuf.img = NULL;
    mSensorOutBuf.share_fd = -1;

    mHWDecoder = NULL;
    mCurrentFormat = 0;
    mIsDecoderInit = false;
    mUSBDevicefd = -1;
    mCameraVirtualDevice = nullptr;
    mVinfo = NULL;
    mCameraUtil = NULL;
    mTempFD = -1;
    memset(&mSavedDecodedBuffer, 0, sizeof(mSavedDecodedBuffer));
    mSavedDecodedBuffer.fd = -1;

    mDecodeFillThreadState = THREAD_STATE_DEAD;
    mDecodeFillThreadId = 0;

    // decoder can access streambuf vector. for loop to fill streambufs.
    mUseStreamBufVecForDecoder = true;
    mDecoderStreamType = MJPEG_STREAM;
    mHWDecoderWorkMode = ASYNC_DECODE_MODE;
    mUsbSensorUtils = nullptr;
    memset(&mSensorOutBuf, 0, sizeof(mSensorOutBuf));
    memset(&mDecoderOutBuf, 0, sizeof(mDecoderOutBuf));
    mNeedStopDecodeFillThread = false;
    mDecodeOutBufIsFresh = false;
    isUseH264 = false;
    CAMHAL_LOGD("create usbsensorHWDec");
}

USBSensorHWDec::~USBSensorHWDec() {
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
    if (v4l2OutDumpFp) {
        fclose(v4l2OutDumpFp);
        v4l2OutDumpFp = NULL;
    }
    if (decOutDumpFp) {
        fclose(decOutDumpFp);
        decOutDumpFp = NULL;
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
    CAMHAL_LOGD("delete usbsensorHWDec");
};

int USBSensorHWDec::camera_open(int idx)
{

    int ret = 0;

    CAMHAL_LOGV("%s: E", __FUNCTION__);


    if (mCameraVirtualDevice == nullptr) {
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    }

    if (mCameraVirtualDevice == nullptr) {
        CAMHAL_LOGE("get CameraVirtualDevice single instance failed");
        return -ENOTTY;
    }

    mUSBDevicefd = mCameraVirtualDevice->openVirtualDevice(idx);
    if (mUSBDevicefd < 0) {
        CAMHAL_LOGD("open %d failed, errno=%d\n", idx, errno);
        ret = -ENOTTY;
    }

    return ret;
}

void USBSensorHWDec::camera_close(void)
{
    CAMHAL_LOGV("%s: E", __FUNCTION__);

    if (mUSBDevicefd < 0) {
        return;
    }

    if (mCameraVirtualDevice == nullptr) {
        mCameraVirtualDevice = CameraVirtualDevice::getInstance();
    }

    if (mVinfo != NULL) {
        mCameraVirtualDevice->releaseVirtualDevice(mVinfo->idx, mUSBDevicefd);
        mVinfo->fd = -1;
    }

    mUSBDevicefd = -1;
}

void USBSensorHWDec::InitVideoInfo(int idx)
{
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    if (mVinfo && mUSBDevicefd >= 0) {
        mVinfo->fd = mUSBDevicefd;
        mVinfo->idx = idx;
    }else {
        CAMHAL_LOGE("%s: init fail", __FUNCTION__);
    }
}

void USBSensorHWDec::determineDecoderStreamType()
{
    int v4l2OutPixFmt = getOutputFormat();
    if (V4L2_PIX_FMT_H264 == v4l2OutPixFmt) {
        CAMHAL_LOGI("%s: decoder stream type h264 stream", __FUNCTION__);
        mDecoderStreamType = H264_STREAM;
        return ;
    }
    else if (V4L2_PIX_FMT_MJPEG == v4l2OutPixFmt) {
        CAMHAL_LOGE("%s: decoder stream type mjpeg stream", __FUNCTION__);
        mDecoderStreamType = MJPEG_STREAM;
        return ;
    }
    else if (V4L2_PIX_FMT_HEVC == v4l2OutPixFmt) {
        CAMHAL_LOGE("%s: decoder stream type hevc stream", __FUNCTION__);
        mDecoderStreamType = HEVC_STREAM;
        return ;
    }

    CAMHAL_LOGE("%s: can not determin decoder stream type,set to mjpeg", __FUNCTION__);
    mDecoderStreamType = MJPEG_STREAM;

}

void USBSensorHWDec::determineDecoderWorkMode()
{
    int v4l2OutPixFmt = getOutputFormat();

    // if prop is set. follow prop.
    if (property_get_bool("vendor.media.camera.usb.asyncdec", false) && v4l2OutPixFmt != V4L2_PIX_FMT_YUYV) {
        CAMHAL_LOGI("%s: got prop, decoder work mode async", __FUNCTION__);
        mHWDecoderWorkMode = ASYNC_DECODE_MODE;
        return ;
    }

    // default behavior: h264 use async mode; mjpeg use sync mode;
    if (V4L2_PIX_FMT_H264 == v4l2OutPixFmt || V4L2_PIX_FMT_HEVC == v4l2OutPixFmt) {
        CAMHAL_LOGI("%s: decoder work mode async", __FUNCTION__);
        mHWDecoderWorkMode = ASYNC_DECODE_MODE;
        return ;
    } else if (v4l2OutPixFmt == V4L2_PIX_FMT_MJPEG) {
        CAMHAL_LOGI("%s: decoder work mode sync", __FUNCTION__);
        mHWDecoderWorkMode = SYNC_DECODE_MODE;
        return;
    }

    CAMHAL_LOGW("%s: unknown pix fmt. default to decoder work mode sync ", __FUNCTION__);
    mHWDecoderWorkMode = SYNC_DECODE_MODE;
}

int USBSensorHWDec::SensorInit(int idx)
{
    CAMHAL_LOGV("%s: E", __FUNCTION__);
    int ret = 0;

    if (mVinfo == NULL) {
        mVinfo =  new CVideoInfo();
        if (mVinfo == nullptr) {
            CAMHAL_LOGE("new CVideoInfo failed");
            return -1;
        }
    }

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
    determineDecoderStreamType();
    determineDecoderWorkMode();
    getStreamInfo(mStreamInfos);
    mSensorType = SENSOR_USB;
    return ret;
}

status_t USBSensorHWDec::startUp(int idx, bool customizationSensor) {

    CAMHAL_LOGV("%s: E", __FUNCTION__);

    int res;
    mCapturedBuffers = NULL;
    mOpenCameraID = idx;

    res = run("Camera::USBSensorHWDec", ANDROID_PRIORITY_URGENT_DISPLAY);

    if (res != OK) {
        CAMHAL_LOGE("Unable to start up usbsensorhwdec capture thread: %d", res);
        return res;
    }

    res = SensorInit(idx);
    if (nullptr == mCameraUtil) {
        mCameraUtil = new CameraUtil();
    }

    if (nullptr == mHWDecoder) {
        mHWDecoder = new HWVideoDecoder();
        if (nullptr == mHWDecoder) {
            CAMHAL_LOGE("new HWVideoDecoder fail");
        }
    }

    CAMHAL_LOGV("%s: leave ", __FUNCTION__);

    return res;
}

uint32_t USBSensorHWDec::getStreamUsage(aml_camera_stream_t& stream){
    ATRACE_CALL();

    uint32_t usage = (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER);
    if (stream.format == HAL_PIXEL_FORMAT_BLOB || (this -> getOutputFormat() == V4L2_PIX_FMT_YUYV) || this -> isNeedDump())
        usage = (usage | GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK);
    usage = GRALLOC1_PRODUCER_USAGE_CAMERA | usage;
    CAMHAL_LOGV("%s: usage=0x%x", __FUNCTION__,usage);
    return usage;

}

int USBSensorHWDec::reAllocSoftwareBuffer(int width, int height)
{
    int ret = 0;
    if (NULL == mSensorOutBuf.img) {
        mSensorOutBuf.width = width;
        mSensorOutBuf.height = height;
        mSensorOutBuf.img = new uint8_t[mSensorOutBuf.width * mSensorOutBuf.height * 3 / 2];
        if (mSensorOutBuf.img == NULL) {
            CAMHAL_LOGE("first time allocate mTemp_buffer failed !");
            return -1;
        }
        return ret;
    } else {
        if ((mSensorOutBuf.width != width) && (mSensorOutBuf.height != height)) {
            mSensorOutBuf.width = width;
            mSensorOutBuf.height = height;
            if (mSensorOutBuf.img) {
                delete [] mSensorOutBuf.img;
                mSensorOutBuf.img = NULL;
            }
            mSensorOutBuf.img = new uint8_t[mSensorOutBuf.width * mSensorOutBuf.height * 3 / 2];
            if (mSensorOutBuf.img == NULL) {
                CAMHAL_LOGE("allocate mTemp_buffer failed !");
                return -1;
            }
        }
    }
    return ret;
}

status_t USBSensorHWDec::getOutputFormat(int width, int height, int pixelformat) {
    int ret = 0;
    for (auto& mStreamInfo : mStreamInfos) {
        if (mStreamInfo.mPixelformat == pixelformat && width == mStreamInfo.mWidth && height == mStreamInfo.mHeight) {
            return pixelformat;
        }
    }
    return ret;
}

status_t USBSensorHWDec::setOutputFormat(int width, int height,
                                   int pixelformat, channel ch)
{
    int res, ret;
    mFramecount = 0;
    mCurFps = 0;

    do {
        if (isUseH264) {
            ret = getOutputFormat(width, height, V4L2_PIX_FMT_H264);
            if (ret) {
                pixelformat = ret;
                mDecoderStreamType = H264_STREAM;
                mHWDecoderWorkMode = ASYNC_DECODE_MODE;
                CAMHAL_LOGW("%s support 4k, set H264 %dx%d", __FUNCTION__, width, height);
                break;
            }
        }
        ret = getOutputFormat(width, height, V4L2_PIX_FMT_MJPEG);
        if (ret) {
            pixelformat = ret;
            mDecoderStreamType = MJPEG_STREAM;
            mHWDecoderWorkMode = ASYNC_DECODE_MODE;
            CAMHAL_LOGW("%s set mjpeg %dx%d", __FUNCTION__, width, height);
            break;
        }
        ret = getOutputFormat(width, height, V4L2_PIX_FMT_H264);
        if (ret) {
            pixelformat = ret;
            mDecoderStreamType = H264_STREAM;
            mHWDecoderWorkMode = ASYNC_DECODE_MODE;
            CAMHAL_LOGW("%s set H264 %dx%d", __FUNCTION__, width, height);
            break;
        }
        ret = getOutputFormat(width, height, V4L2_PIX_FMT_HEVC);
        if (ret) {
            pixelformat = ret;
            mDecoderStreamType = HEVC_STREAM;
            mHWDecoderWorkMode = ASYNC_DECODE_MODE;
            CAMHAL_LOGW("%s set H265 %dx%d", __FUNCTION__, width, height);
            break;
        }
        ret = getOutputFormat(width, height, V4L2_PIX_FMT_YUYV);
        if (ret) {
            pixelformat = ret;
            mHWDecoderWorkMode = SYNC_DECODE_MODE;
            CAMHAL_LOGW("%s set yuyv %dx%d", __FUNCTION__, width, height);
            break;
        }
        ret = getOutputFormat(width, height, V4L2_PIX_FMT_NV21);
        if (ret) {
            pixelformat = ret;
            mHWDecoderWorkMode = SYNC_DECODE_MODE;
            CAMHAL_LOGW("%s set nv21 %dx%d", __FUNCTION__, width, height);
            break;
        }
    } while (0);

    gettimeofday(&mTimeStart, NULL);
    if (pixelformat != V4L2_PIX_FMT_YUYV && pixelformat != V4L2_PIX_FMT_NV21)
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
    if (pixelformat == V4L2_PIX_FMT_YUYV) {
        reAllocSoftwareBuffer(width, height);
    }

    mPre_width = mVinfo->preview.format.fmt.pix.width;
    mPre_height = mVinfo->preview.format.fmt.pix.height;

    return OK;
}

status_t USBSensorHWDec::streamOn(channel ch) {
    status_t ret = mVinfo->start_capturing();

    if (mHWDecoderWorkMode == ASYNC_DECODE_MODE) {
        ret = startDecodeFillThread();
    }
    return ret;
}

bool USBSensorHWDec::isStreaming() {
    return mVinfo->isStreaming;
}


bool USBSensorHWDec::isNeedRestart(uint32_t width, uint32_t height, uint32_t pixelformat, channel ch) {
    if ((mVinfo->preview.format.fmt.pix.width != width)
        ||(mVinfo->preview.format.fmt.pix.height != height)) {
        return true;
    }
    return false;
}

void USBSensorHWDec::initDecoder(int in_width, int in_height, int out_width, int out_height, int out_bufferCount) {
    CAMHAL_LOGI("%s: in_width=%d, in_height=%d out_width=%d, out_height=%d",
         __FUNCTION__, in_width, in_height, out_width, out_height);

    if (mHWDecoder != NULL && mIsDecoderInit == false) {
        CAMHAL_LOGV("%s: really init decoder", __FUNCTION__);
        uint32_t stream_type = HWVideoDecoder::MJPEG_STREAM;
        if (mDecoderStreamType == H264_STREAM) {
            stream_type = HWVideoDecoder::H264_STREAM;
        }
        if (mDecoderStreamType == HEVC_STREAM) {
            stream_type = HWVideoDecoder::HEVC_STREAM;
        }
        HWVideoDecoder::DecoderMode decoderWorkMode = HWVideoDecoder::SYNC_DECODE_MODE;
        if (mHWDecoderWorkMode == ASYNC_DECODE_MODE) {
            decoderWorkMode = HWVideoDecoder::ASYNC_DECODE_MODE;
            CAMHAL_LOGI("decoder in async mode");
        } else {
            CAMHAL_LOGI("decoder in sync mode");
        }

        uint32_t   default_fps = 30;
        mHWDecoder->initialize(stream_type, in_width, in_height, default_fps, decoderWorkMode);

        mIsDecoderInit=true;
    } else {
        CAMHAL_LOGW("skip decoder initialize. has been inited");
    }
}

status_t USBSensorHWDec::shutDown() {
    CAMHAL_LOGD("%s: E", __FUNCTION__);
    int res;
    mTimeOutCount = 0;

    if (mHWDecoderWorkMode == ASYNC_DECODE_MODE) {
        res = stopDecodeFillThread();
    }

    res = requestExitAndWait();
    if (res != OK) {
        CAMHAL_LOGE("Unable to shut down sensor capture thread: %d", res);
    }

    if (mVinfo != NULL) {
        mVinfo->releasebuf_and_stop_capturing();
    }

    camera_close();

    if (mSensorOutBuf.img ) {
        delete[] mSensorOutBuf.img;
        mSensorOutBuf.img = NULL;
    }
    mSensorWorkFlag = false;
    CAMHAL_LOGD("%s: line %d ", __FUNCTION__, __LINE__);

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    DeWarp::putInstance();
    CameraConfig::deleteInstance();
#endif

    if (mHWDecoder && mIsDecoderInit == true) {
        mHWDecoder->deinitialize();
        mIsDecoderInit = false;
    }
    CAMHAL_LOGD("%s: line %d ", __FUNCTION__, __LINE__);

    if (mHWDecoder) {
        delete mHWDecoder;
        mHWDecoder = NULL;
    }

    CAMHAL_LOGD("%s: Exit", __FUNCTION__);
    return res;
}

status_t USBSensorHWDec::streamOff(channel ch) {
    CAMHAL_LOGV("%s: E", __FUNCTION__);

    if (mHWDecoderWorkMode == ASYNC_DECODE_MODE) {
        stopDecodeFillThread();
    }

    mVinfo->releasebuf_and_stop_capturing();

#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
    DeWarp::putInstance();
#endif

    // streamoff ->configureStreams->streamon; we need deinitialize decoder here.
    // configure streams with different output size;
    if (mHWDecoder && mIsDecoderInit == true) {
        mHWDecoder->deinitialize();
        mIsDecoderInit = false;
        delete mHWDecoder;
        mHWDecoder = new HWVideoDecoder();
    }

    return 0;
}

void USBSensorHWDec::setIOBufferNum()
{
    mVinfo->set_buffer_numbers(property_get_int32("ro.vendor.usbcamera.iobuffer", 4));
}

status_t USBSensorHWDec::getOutputFormat(void)
{
    uint32_t ret = 0;
    if (mExpectedV4l2OutPixFmt != 0x0) {
        ret = mVinfo->EnumerateFormat(mExpectedV4l2OutPixFmt);
        if (ret) {
            return ret;
        }
    }
    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_HEVC);
    if (ret) {
        return ret;
    }

    CAMHAL_LOGW("h265 stream is not supported by hw . fallback to h264 stream");
    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_H264);
    if (ret) {
        return ret;
    }

    CAMHAL_LOGW("h264 stream is not supported by hw . fallback to mjpeg stream");
    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_MJPEG);
    if (ret) {
        return ret;
    }

    CAMHAL_LOGW("h264 & mjpeg stream not supported by hw, try nv21");
    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_NV21);
    if (ret) {
        return ret;
    }

    CAMHAL_LOGW("h264 & mjpeg & nv21 stream not supported by hw, try yuyv");
    ret = mVinfo->EnumerateFormat(V4L2_PIX_FMT_YUYV);
    if (ret) {
        return ret;
    }

    CAMHAL_LOGE("Unable to find a supported v4l2 pix format!");
    return 0;
}

// return
// 0 - ok or retry (if outDataAddr is null)
// -1 - fail.
int USBSensorHWDec::checkAndGetLatestSensorData(uint8_t **outDataAddr, uint32_t *outDataLen )
{
    *outDataLen = 0;
    *outDataAddr = NULL;
    uint8_t * src = NULL;

    fd_set fds;
    struct timeval tv;
    int r;

    if (mVinfo->fd <= 0) {
       CAMHAL_LOGE("invalid fd  %d", mVinfo->fd);
        return -1;
    }

read_queue:
    FD_ZERO(&fds);
    FD_SET(mVinfo->fd, &fds);
    /*2s Timeout*/
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r) {
        if (EINTR == errno)
            return 0;
        CAMHAL_LOGD("select error:%s",strerror(errno));
    }

    if (0 == r) {
        force_reset_v4l2_capture();
        CAMHAL_LOGE("select timeout:%s",strerror(errno));
        return 0;
    }

    // selected ok. get frame.
    src = (uint8_t *)mVinfo->get_frame();
    if (NULL == src) {
        if (mVinfo->get_device_status()) {
            CAMHAL_LOGE("camera device in error state ");
            camera_close();
            return -1;
        }

        CAMHAL_LOGD("%s:get frame NULL, sleep 5ms",__FUNCTION__);
        usleep(5000);

        mTimeOutCount++;
        if (mTimeOutCount > 600) {
            CAMHAL_LOGD("force sensor reset.\n");
            force_reset_v4l2_capture();
        }

        return 0;
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
            return -1;
        }
        goto read_queue;
    }

    // here this src is the latest buffer.
    mTimeOutCount = 0;
    *outDataAddr = src;
    *outDataLen = mVinfo->preview.buf.bytesused;
    return 0;
}

// return
// 0 - ok or retry (if outDataAddr is null)
// -1 - fail.
int USBSensorHWDec::checkAndGetNextSensorData(uint8_t **outDataAddr, uint32_t *outDataLen )
{
    *outDataLen = 0;
    *outDataAddr = NULL;
    uint8_t * src = NULL;

    fd_set fds;
    struct timeval tv;
    int r;

    if (mVinfo->fd <= 0) {
       CAMHAL_LOGE("invalid fd  %d", mVinfo->fd);
        return -1;
    }

    FD_ZERO(&fds);
    FD_SET(mVinfo->fd, &fds);
    /*2s Timeout*/
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    r = select(mVinfo->fd + 1, &fds, NULL, NULL, &tv);
    if (-1 == r) {
        if (EINTR == errno)
            return 0;
        CAMHAL_LOGD("select error:%s",strerror(errno));
    }

    if (0 == r) {
        force_reset_v4l2_capture();
        CAMHAL_LOGE("select timeout:%s",strerror(errno));
        return 0;
    }

    // selected ok. get frame.
    src = (uint8_t *)mVinfo->get_frame();
    if (NULL == src) {
        if (mVinfo->get_device_status()) {
            CAMHAL_LOGE("camera device in error state ");
            camera_close();
            return -1;
        }

        CAMHAL_LOGD("%s:get frame NULL, sleep 5ms",__FUNCTION__);
        usleep(5000);

        mTimeOutCount++;
        if (mTimeOutCount > 600) {
            CAMHAL_LOGD("force sensor reset.\n");
            force_reset_v4l2_capture();
        }

        return 0;
    }

    // here. src is valid.
    mTimeOutCount = 0;
    *outDataAddr = src;
    *outDataLen = mVinfo->preview.buf.bytesused;
    return 0;
}

int USBSensorHWDec::halFormatToSensorFormat(uint32_t pixelfmt){
    return getOutputFormat();
}

int USBSensorHWDec::captureNV21UseSavedBuf(StreamBuffer &b, bufInfo *savedBuffer)
{
    CAMHAL_LOGVV("%s: E", __FUNCTION__);

    if (nullptr == savedBuffer->vaddr && savedBuffer->fd < 0) {
        CAMHAL_LOGE("saved decoded buffer is null");
        return -1;
    }

    switch (savedBuffer->fmt)
    {
        case V4L2_PIX_FMT_NV21:
            if ((savedBuffer->width == b.width) && (savedBuffer->height == b.height)) {
#ifdef GE2D_ENABLE
                if (savedBuffer->fd != -1) {
                    mGE2D->ge2d_copy(b.share_fd, savedBuffer->fd, b.stride, b.height, ge2dTransform::NV12);
                } else {
                    memcpy(b.img, savedBuffer->vaddr, b.stride * b.height * 3/2);
                }
#else
                memcpy(b.img, savedBuffer->vaddr, b.stride * b.height * 3/2);
#endif
            } else {
#ifdef GE2D_ENABLE
                if (savedBuffer->fd != -1) {
                    mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          savedBuffer->fd, savedBuffer->width, savedBuffer->height);
                } else {
                    mCameraUtil->ReSizeNV21(savedBuffer->vaddr, b.img, b.width, b.height, b.stride, savedBuffer->width, savedBuffer->height);
                }
#else
                mCameraUtil->ReSizeNV21(savedBuffer->vaddr, b.img, b.width, b.height, b.stride, savedBuffer->width, savedBuffer->height);
#endif
            }
        return 0;
    }
    return -1;
}

void USBSensorHWDec::captureNV21UsbSensor(StreamBuffer b, uint32_t gain, bool needSensorOutBuf) {
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    uint8_t *src = nullptr;
    uint32_t src_len = 0;
    int pixelformat;

    // if there is saved decoded buffer. use it.
    if (mSavedDecodedBuffer.vaddr || mSavedDecodedBuffer.fd != -1) {
        captureNV21UseSavedBuf(b, &mSavedDecodedBuffer);
#ifdef GE2D_ENABLE
        mGE2D->doRotationAndMirror(b);
#endif
        return;
    }

    while (1) {
        int ret = 0;
        ret = checkAndGetLatestSensorData(&src, &src_len);
        if (nullptr == src && ret == 0) {
            CAMHAL_LOGE("%s, line %d can not get sensor data", __FUNCTION__, __LINE__);
            continue;
        } else if (ret < 0) {
            break;
        }


        mTimeOutCount = 0;
        pixelformat = mVinfo->preview.format.fmt.pix.pixelformat;
        uint32_t width = mVinfo->preview.format.fmt.pix.width;
        uint32_t height = mVinfo->preview.format.fmt.pix.height;

        bool useSensorOutBuf = needSensorOutBuf
            && ( b.width != mVinfo->preview.format.fmt.pix.width
               || b.height != mVinfo->preview.format.fmt.pix.height);

        switch (pixelformat) {
            case V4L2_PIX_FMT_NV21:
            {
                bool  cap_buf_filled = false;
                // step 1 : sensor data process. either to capture buf or to mSensorOutBuf
                if (useSensorOutBuf) {
                    // sensor out buf is always the same size with preview buf.
                    // just copy it.
                    memcpy(mSensorOutBuf.img, src, width * height * 3 / 2);
                } else {
                    if (width == b.width && height == b.height) {
                        memcpy(b.img, src, width * height * 3 / 2);
                    } else {
                        mCameraUtil->ReSizeNV21(src, b.img, b.width, b.height, b.stride, width, height);
                    }
                    cap_buf_filled = true;
                }

                // step 2: if capture buf is not filled in step1. fill it with mSensorOutBuf.
                if (false == cap_buf_filled) {
                    if (mSensorOutBuf.width == b.width && mSensorOutBuf.height == b.height) {
                        memcpy(b.img, mSensorOutBuf.img, width * height * 3 / 2);
                    } else {
                        mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, mSensorOutBuf.width, mSensorOutBuf.height);
                    }
                    cap_buf_filled = true;
                }

#ifdef GE2D_ENABLE
                mGE2D->doRotationAndMirror(b);
#endif
                if (false == useSensorOutBuf) {
                    mSavedDecodedBuffer.vaddr = b.img;
                    mSavedDecodedBuffer.fd = b.share_fd;
                    mSavedDecodedBuffer.width = b.width;
                    mSavedDecodedBuffer.height = b.height;
                    mSavedDecodedBuffer.fmt = V4L2_PIX_FMT_NV21;
                } else {
                    mSavedDecodedBuffer.vaddr = mSensorOutBuf.img;
                    mSavedDecodedBuffer.fd = mSensorOutBuf.share_fd;
                    mSavedDecodedBuffer.width = mSensorOutBuf.width;
                    mSavedDecodedBuffer.height = mSensorOutBuf.height;
                    mSavedDecodedBuffer.fmt = V4L2_PIX_FMT_NV21;
                }
                if (src) {
                    mVinfo->putback_frame();
                }
            } break;
            case V4L2_PIX_FMT_YUYV:
            {
                bool  cap_buf_filled = false;
                // step 1 : sensor data process. either to capture buf or to mSensorOutBuf
                if (useSensorOutBuf) {
                    // sensor out buf is always the same size with preview buf.
                    mCameraUtil->YUYVToNV21(src, mSensorOutBuf.img, width, height);
                } else {
                    if (width == b.width && height == b.height) {
                        mCameraUtil->YUYVToNV21(src, b.img, width, height);
                    } else {
                        // ge2d does not support yuyv. use software.
                        mCameraUtil->YUYVToNV21(src, mSensorOutBuf.img, width, height);
                        mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, width, height);
                    }
                    cap_buf_filled = true;
                }

                // step 2: if capture buf is not filled in step1. fill it with mSensorOutBuf.
                if (false == cap_buf_filled) {
                    if (mSensorOutBuf.width == b.width && mSensorOutBuf.height == b.height) {
                        memcpy(b.img, mSensorOutBuf.img, width * height * 3 / 2);
                    } else {
                        mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b.img, b.width, b.height, b.stride, mSensorOutBuf.width, mSensorOutBuf.height);
                    }
                    cap_buf_filled = true;
                }

#ifdef GE2D_ENABLE
                mGE2D->doRotationAndMirror(b);
#endif
                if (false == useSensorOutBuf) {
                    mSavedDecodedBuffer.vaddr = b.img;
                    mSavedDecodedBuffer.fd = b.share_fd;
                    mSavedDecodedBuffer.width = b.width;
                    mSavedDecodedBuffer.height = b.height;
                    mSavedDecodedBuffer.fmt = V4L2_PIX_FMT_NV21;
                } else {
                    mSavedDecodedBuffer.vaddr = mSensorOutBuf.img;
                    mSavedDecodedBuffer.fd = mSensorOutBuf.share_fd;
                    mSavedDecodedBuffer.width = mSensorOutBuf.width;
                    mSavedDecodedBuffer.height = mSensorOutBuf.height;
                    mSavedDecodedBuffer.fmt = V4L2_PIX_FMT_NV21;
                }
                if (src) {
                    mVinfo->putback_frame();
                }
            } break;
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

void USBSensorHWDec::captureNV21UsbSensor(Vector<StreamBuffer>& b, uint32_t gain, bool isJpegRequest) {
    CAMHAL_LOGVV("%s: E", __FUNCTION__);
    uint8_t *src = nullptr;
    uint32_t src_len = 0;
    int pixelformat;
    uint32_t width;
    uint32_t height;

    while (1) {
        if (mHWDecoderWorkMode == ASYNC_DECODE_MODE) {
            src = nullptr;
            if ( (mVinfo->fd <= 0) || exitPending() )
                break;
        } else {
            int ret = 0;
            ret = checkAndGetLatestSensorData(&src, &src_len);
            if (nullptr == src && ret == 0) {
                CAMHAL_LOGE("%s, line %d can not get sensor data", __FUNCTION__, __LINE__);
                continue;
            } else if (ret < 0) {
                break;
            }
        }

        pixelformat = mVinfo->preview.format.fmt.pix.pixelformat;
        width = mVinfo->preview.format.fmt.pix.width;
        height = mVinfo->preview.format.fmt.pix.height;
        switch (pixelformat) {
            case V4L2_PIX_FMT_H264:
            case V4L2_PIX_FMT_MJPEG:
            case V4L2_PIX_FMT_HEVC:
            {
                int ret = HWDecodeToNV21(src, src_len, b, isJpegRequest);

                if (src && mHWDecoderWorkMode == SYNC_DECODE_MODE) {
                    mVinfo->putback_frame();
                }

                // both sync and async mode; should continue here when fail to fill b;
                if (ret != 0) {
                    continue;
                }

            } break;
            case V4L2_PIX_FMT_YUYV:
            {
                for (size_t i = 0; i < b.size(); i++) {
                    if (b[i].format == HAL_PIXEL_FORMAT_BLOB) {
                        CAMHAL_LOGE("%s:blob buffer bypass",__FUNCTION__);
                    } else {
                        if (src != nullptr) {
                            if (width == b[i].width && height == b[i].height) {
                                mCameraUtil->YUYVToNV21(src, b[i].img, width, height);
                            } else {
                                // ge2d does not support yuyv. use software.
                                mCameraUtil->YUYVToNV21(src, mSensorOutBuf.img, width, height);
                                mCameraUtil->ReSizeNV21(mSensorOutBuf.img, b[i].img, b[i].width, b[i].height, b[i].stride, width, height);
                            }
                        }
                    }
#ifdef GE2D_ENABLE
                    mGE2D->doRotationAndMirror(b[i]);
#endif
                }
                    mVinfo->putback_frame();
            } break;
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

int USBSensorHWDec::HWDecodeToNV21(uint8_t* src, uint32_t src_length, Vector<StreamBuffer>& b, bool isJpegRequest)
{
    CAMHAL_LOGVV("%s: E, src=0x%p", __FUNCTION__, src);
    size_t src_width = mVinfo->preview.format.fmt.pix.width;
    size_t src_height = mVinfo->preview.format.fmt.pix.height;

    int ret = -1;

    if (false == mIsDecoderInit) {
        CAMHAL_LOGE("hw video deocder is not initialized yet");
        return -1;
    }

    HWVideoDecoder::DecoderStatus decoderStatus =  mHWDecoder->getDecoderStatus();

    if (decoderStatus < HWVideoDecoder::INITED || decoderStatus == HWVideoDecoder::RUNTIME_ERROR) {
        CAMHAL_LOGE("hw video deocder bad status %d", decoderStatus);
        usleep(10*1000);
        return -1;
    }

    if (decoderStatus == HWVideoDecoder::DECODE_FAIL_AND_INPUT_FULL) {
        CAMHAL_LOGE("%s: decoder input buffer is full, do decode re-initialize", __FUNCTION__);
        mHWDecoder->deinitialize();
        mIsDecoderInit = false;
        initDecoder(src_width, src_height, src_width, src_height, 4);
    }

    if (mHWDecoderWorkMode == SYNC_DECODE_MODE) {
         ret = mHWDecoder->syncDecode(-1, src, src_length, b, isJpegRequest);
    }
    else {
        ret = mHWDecoder->asyncDecodeDequeueOutput(b, isJpegRequest);
    }


    if (0 != ret) {
        CAMHAL_LOGE(" Decode fail");
    }

    return ret;
}


void USBSensorHWDec::dump(int& frame_index, uint8_t* buf, int length, std::string name) {
    // todo
}

void USBSensorHWDec::setSensorListener(SensorListener *listener) {
    Sensor::setSensorListener(listener);
}

int USBSensorHWDec::getZoom(int *zoomMin, int *zoomMax, int *zoomStep) {
    return mUsbSensorUtils->getZoom( zoomMin, zoomMax, zoomStep);
}

int USBSensorHWDec::setZoom(int zoomValue) {
    return mUsbSensorUtils->setZoom( zoomValue) ;
}

status_t USBSensorHWDec::setEffect(uint8_t effect) {
    return mUsbSensorUtils->setEffect( effect);
}

int USBSensorHWDec::getExposure(int *maxExp, int *minExp, int *def, camera_metadata_rational *step)
{
    return mUsbSensorUtils->getExposure( maxExp,  minExp,  def, step);
}

status_t USBSensorHWDec::setExposure(int expCmp)
{
    return mUsbSensorUtils->setExposure( expCmp);
}

int USBSensorHWDec::getAntiBanding(uint8_t *antiBanding, uint8_t maxCont)
{
    return mUsbSensorUtils->getAntiBanding(antiBanding, maxCont);
}

status_t USBSensorHWDec::setAntiBanding(uint8_t antiBanding)
{
    return mUsbSensorUtils->setAntiBanding(antiBanding);
}

status_t USBSensorHWDec::setFocusArea(int32_t x0, int32_t y0, int32_t x1, int32_t y1)
{
    return mUsbSensorUtils->setFocusArea( x0,  y0, x1, y1);
}


int USBSensorHWDec::getAutoFocus(uint8_t *afMode, uint8_t maxCount)
{
    return mUsbSensorUtils->getAutoFocus( afMode, maxCount);
}

status_t USBSensorHWDec::setAutoFocus(uint8_t afMode)
{
    return mUsbSensorUtils->setAutoFocus(afMode);
}

int USBSensorHWDec::getAWB(uint8_t *awbMode, uint8_t maxCount)
{
    return mUsbSensorUtils->getAWB( awbMode, maxCount);
}

status_t USBSensorHWDec::setAWB(uint8_t awbMode)
{
    return mUsbSensorUtils->setAWB(awbMode);
}

const char* USBSensorHWDec::getformt(int id) {
    return mUsbSensorUtils->getformtStr(id);
}

void USBSensorHWDec::getStreamInfo(std::vector<streamInfo> &streamInfos) {
    int i, j, res, ret;
    int temp_rate, framerate, framerate_min;
    unsigned int support_w, support_h;
    struct v4l2_frmivalenum fival;
    struct v4l2_frmsizeenum frmsize;
    streamInfos.clear();
    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera_preview.maxsize", property, NULL) > 0)
    {
        CAMHAL_LOGV("support Max Preview Size :%s", property);
        if (sscanf(property, "%dx%d", &support_w, &support_h) != 2)
        {
            support_w = 10000;
            support_h = 10000;
        }
    } else {
#if defined(CAMERA_MAX_PREVIEW_WIDTH) && defined(CAMERA_MAX_PREVIEW_HEIGHT)
        support_w = atoi(CAMERA_MAX_PREVIEW_WIDTH);
        support_h = atoi(CAMERA_MAX_PREVIEW_HEIGHT);
#endif
        CAMHAL_LOGV("the configured max preview size :%dx%d", support_w, support_h);
    }
    framerate_min = property_get_int32("vendor.camera.frame.rate.min", 20);
    uint32_t srcfmt[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_HEVC,
    };

    for (j = 0; j < (int)(sizeof(srcfmt) / sizeof(srcfmt[0])); j++)
    {
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = srcfmt[j];
        for (i = 0;; i++)
        {
            frmsize.index = i;

            res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (res < 0)
            {
                CAMHAL_LOGV("index=%d, break\n", i);
                break;
            }

            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            { // only support this type

                memset(&fival, 0, sizeof(fival));
                fival.pixel_format = srcfmt[j];
                fival.width = frmsize.discrete.width;
                fival.height = frmsize.discrete.height;
                fival.index = 0;
                fival.type = V4L2_FRMIVAL_TYPE_DISCRETE;
                temp_rate=0;
                framerate=0;
                while ((ret = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
                    if ( fival.discrete.numerator != 0)
                        temp_rate = fival.discrete.denominator / fival.discrete.numerator;

                    if (framerate < temp_rate)
                        framerate = temp_rate;

                    fival.index++;
                }

                if (framerate < framerate_min)
                    continue;

                if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height > support_h))
                    continue;

                if (!IsAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                    continue;

                streamInfos.emplace_back(srcfmt[j], frmsize.discrete.width, frmsize.discrete.height);
                if ((srcfmt[j] == V4L2_PIX_FMT_H264) && determineUseH264(frmsize.discrete.width, frmsize.discrete.height)) {
                    isUseH264 = true;
                 }
            }
        }
    }
}

int USBSensorHWDec::getStreamConfigurations(uint32_t picSizes[], const int32_t kAvailableFormats[], int size)
{
    int res, ret;
    int i, j, k, START;
    int count = 0;
    int temp_rate, framerate, framerate_min;
    struct v4l2_frmsizeenum frmsize;
    struct v4l2_frmivalenum fival;
    unsigned int support_w, support_h;
    bool isSameSize = false;

    support_w = 10000;
    support_h = 10000;
    memset(property, 0, sizeof(property));
    if (property_get("vendor.media.camera_preview.maxsize", property, NULL) > 0)
    {
        CAMHAL_LOGD("support Max Preview Size :%s", property);
        if (sscanf(property, "%dx%d", &support_w, &support_h) != 2)
        {
            support_w = 10000;
            support_h = 10000;
        }
    } else {
#if defined(CAMERA_MAX_PREVIEW_WIDTH) && defined(CAMERA_MAX_PREVIEW_HEIGHT)
        support_w = atoi(CAMERA_MAX_PREVIEW_WIDTH);
        support_h = atoi(CAMERA_MAX_PREVIEW_HEIGHT);
#endif
        CAMHAL_LOGD("the configured max preview size :%dx%d", support_w, support_h);
    }

    framerate_min = property_get_int32("vendor.camera.frame.rate.min", 20);

    uint32_t srcfmt[] = {
        V4L2_PIX_FMT_MJPEG,
        V4L2_PIX_FMT_H264,
        V4L2_PIX_FMT_YUYV,
        V4L2_PIX_FMT_HEVC,
    };
    uint32_t halPixelFormat[] = {
        HAL_PIXEL_FORMAT_YCbCr_420_888,
        HAL_PIXEL_FORMAT_BLOB,
    };

    START = 0;
    for (j = 0; j < (int)(sizeof(srcfmt) / sizeof(srcfmt[0])); j++)
    {
        memset(&frmsize, 0, sizeof(frmsize));
        frmsize.pixel_format = srcfmt[j];
        for (i = 0;; i++)
        {
            frmsize.index = i;

            res = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMESIZES, &frmsize);
            if (res < 0)
            {
                CAMHAL_LOGD("index=%d, break\n", i);
                break;
            }

            if (frmsize.type == V4L2_FRMSIZE_TYPE_DISCRETE)
            { // only support this type

                if (0 != (frmsize.discrete.width % 16))
                    continue;

                if ((frmsize.discrete.width > support_w) && (frmsize.discrete.height > support_h))
                    continue;


                memset(&fival, 0, sizeof(fival));
                fival.pixel_format = srcfmt[j];
                fival.width = frmsize.discrete.width;
                fival.height = frmsize.discrete.height;
                fival.index = 0;
                fival.type = V4L2_FRMIVAL_TYPE_DISCRETE;
                temp_rate=0;
                framerate=0;
                while ((ret = ioctl(mVinfo->fd, VIDIOC_ENUM_FRAMEINTERVALS, &fival)) == 0) {
                    if ( fival.discrete.numerator != 0)
                        temp_rate = fival.discrete.denominator / fival.discrete.numerator;

                    if (framerate < temp_rate)
                        framerate = temp_rate;

                    fival.index++;
                }

                if (framerate < framerate_min)
                    continue;

                if (count >= size)
                    break;

                if (!IsAvailablePictureSize(kUsbAvailablePictureSize, frmsize.discrete.width, frmsize.discrete.height))
                    continue;

                if (((j != 0) || (i != 0)) && (count >= 4))
                {
                    for (int m = count; m > START; m -= 4)
                    {
                        if ((frmsize.discrete.width == picSizes[m - 3]) && (frmsize.discrete.height == picSizes[m - 2]))
                        {
                            isSameSize = true;
                        }
                    }
                    if (isSameSize)
                    {
                        isSameSize = false;
                        continue;
                    }
                }

                picSizes[count + 0] = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
                picSizes[count + 1] = frmsize.discrete.width;
                picSizes[count + 2] = frmsize.discrete.height;
                picSizes[count + 3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;

                if (0 == i && count == 0)
                {
                    count += 4;
                    continue;
                }

                // TODO insert in descend order
                for (k = count; k > START; k -= 4)
                {
                    if (frmsize.discrete.width * frmsize.discrete.height >
                        picSizes[k - 3] * picSizes[k - 2])
                    {
                        picSizes[k + 1] = picSizes[k - 3];
                        picSizes[k + 2] = picSizes[k - 2];
                    }
                    else
                    {
                        break;
                    }
                }

                picSizes[k + 1] = frmsize.discrete.width;
                picSizes[k + 2] = frmsize.discrete.height;
                CAMHAL_LOGD("get output width=%d, height=%d, format=%s\n",
                                        frmsize.discrete.width,
                                        frmsize.discrete.height,
                                        getformt(frmsize.pixel_format));

                count += 4;
            }
        }
    }
    if (count != 0) {
        START = count;
        for (j = 0; j < (int)(sizeof(halPixelFormat) / sizeof(halPixelFormat[0])); j++) {
            for (i = 0; i < START; i += 4) {
                picSizes[count + 0] = halPixelFormat[j];
                picSizes[count + 1] = picSizes[i+1];
                picSizes[count + 2] = picSizes[i+2];
                picSizes[count + 3] = ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS_OUTPUT;
                CAMHAL_LOGD("get output width=%d, height=%d, hal pixel format=%d\n",
                                        picSizes[count + 1],
                                        picSizes[count + 2],
                                        halPixelFormat[j]);
                count += 4;
            }
        }
    } else {
        CAMHAL_LOGD("no support pixel fmt");
    }
    return count;
}

int USBSensorHWDec::getStreamConfigurationDurations(uint32_t picSizes[], int64_t duration[], int size, bool flag)
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

int64_t USBSensorHWDec::getMinFrameDuration()
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
        V4L2_PIX_FMT_HEVC,
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

int USBSensorHWDec::getPictureSizes(int32_t picSizes[], int size, bool preview) {
    int res;
    int i;
    int count = 0;
    struct v4l2_frmsizeenum frmsize;
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

status_t USBSensorHWDec::force_reset_sensor() {
    CAMHAL_LOGW("force_reset_sensor");
    status_t ret;
    mTimeOutCount = 0;
    ret = streamOff(channel_preview);
    ret = mVinfo->setBuffersFormat();
    ret = streamOn(channel_preview);
    CAMHAL_LOGW("%s , leave, ret = %d", __FUNCTION__, ret);
    return ret;
}

void USBSensorHWDec::force_reset_v4l2_capture()
{
    mVinfo->releasebuf_and_stop_capturing();
    mVinfo->setBuffersFormat();
    mVinfo->start_capturing();
}

int USBSensorHWDec::captureNewImage() {
    uint32_t gain = mGainFactor;
    uint8_t* solidBuffer = nullptr;
    int solidBufferFd = -1;
    if (mUseStreamBufVecForDecoder) {
        bool isJpegRequest = false;
        for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {
            const StreamBuffer &b = (*mNextCapturedBuffers)[i];
            CAMHAL_LOGVV("Sensor capturing buffer %zu: stream %d,"
                " %d x %d, format %x, stride %d, buf %p, img %p",
                i, b.streamId, b.width, b.height, b.format, b.stride,
                b.buffer, b.img);
            if (mTestPatternMode == ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR) {
                mSensorWorkFlag = true;
                if (mFlushFlag)
                    break;
                if (mExitSensorThread)
                    break;
                if (b.img != NULL) {
                    memset(b.img, 0, b.width * b.height);
                    memset(b.img + b.width * b.height, 128, b.width * b.height / 2);
                } else {
                    solidBuffer = mION->alloc_buffer(b.width * b.height * 3 / 2, &solidBufferFd, cache);
                    memset(solidBuffer, 0, b.width * b.height);
                    memset(solidBuffer + b.width * b.height, 128, b.width * b.height / 2);
                    mGE2D->ge2d_copy(b.share_fd, solidBufferFd, b.width, b.height, V4L2_PIX_FMT_NV21);
                    mION->free_buffer(solidBufferFd);
                    solidBuffer = nullptr;
                    solidBufferFd = -1;
                }
                continue;
            }
            if  (b.format == HAL_PIXEL_FORMAT_BLOB) {
                StreamBuffer bAux;
                int orientation;
                orientation = getPictureRotate();
                CAMHAL_LOGD("bAux orientation=%d",orientation);
                uint32_t pixelfmt;
                if (1) {
                    pixelfmt = getOutputFormat();
                    if (pixelfmt == V4L2_PIX_FMT_YVU420) {
                        pixelfmt = HAL_PIXEL_FORMAT_YV12;
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
                bAux.img = NULL;
                bAux.share_fd = -1;
#ifdef GE2D_ENABLE
                if (getOutputFormat() == V4L2_PIX_FMT_YUYV) {
                    bAux.img = mION->alloc_buffer(b.width * b.height * 3,&bAux.share_fd, cache);
                } else {
                    bAux.img = mION->alloc_buffer(b.width * b.height * 3,&bAux.share_fd);
                }
#else
                bAux.img = new uint8_t[b.width * b.height * 3];
#endif
                mNextCapturedBuffers->push_back(bAux);
                isJpegRequest = true;
            }
        }
        if (mTestPatternMode != ANDROID_SENSOR_TEST_PATTERN_MODE_SOLID_COLOR) {
            CAMHAL_LOGVV("%s capture NV21", __FUNCTION__);
            captureNV21UsbSensor(*mNextCapturedBuffers, gain, isJpegRequest);
        }
        return 0;
    }

    memset(&mSavedDecodedBuffer, 0, sizeof (mSavedDecodedBuffer) );
    mSavedDecodedBuffer.fd = -1;
    mSavedDecodedBuffer.vaddr = nullptr;

    bool needSensorOutBuffer = false;

    size_t buffer_num = mNextCapturedBuffers->size();

    // Might be adding more buffers, so size isn't constant
    CAMHAL_LOGVV("%s:buffer size=%zu\n",__FUNCTION__,buffer_num);
    if (buffer_num > 1) {
        needSensorOutBuffer = true;
    }

    for (size_t i = 0; i < mNextCapturedBuffers->size(); i++) {

        const StreamBuffer &b = (*mNextCapturedBuffers)[i];

        CAMHAL_LOGVV("Sensor capturing buffer %zu: stream %d,"
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
                CAMHAL_LOGD("bAux orientation=%d",orientation);

                bAux.streamId = 0;
                bAux.width = b.width;
                bAux.height = b.height;
                bAux.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
                bAux.stride = b.width;
                bAux.buffer = NULL;
#ifdef GE2D_ENABLE
                bAux.img = mION->alloc_buffer(b.width * b.height * 3 / 2,&bAux.share_fd);
#else
                bAux.img = new uint8_t[b.width * b.height * 3 / 2];
#endif
                mNextCapturedBuffers->push_back(bAux);
                break;
            case HAL_PIXEL_FORMAT_YCrCb_420_SP:
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                captureNV21UsbSensor(b, gain, needSensorOutBuffer);
                break;

            default:
                CAMHAL_LOGE("%s: UnSupported format 0x%x, no output", __FUNCTION__,
                        b.format);
                break;
        }
    }
    return 0;
}

status_t USBSensorHWDec::readyToRun() {
    //int res;
    ATRACE_CALL();
    CAMHAL_LOGV("Starting up usb sensor thread");
    mStartupTime = systemTime();
    mNextCaptureTime = 0;
    mNextCapturedBuffers = NULL;
    CAMHAL_LOGD("");

    return OK;
}

static bool isIDR(int decoderStreamType, uint8_t* in_src, uint32_t in_size)
{
    if (in_size < 8) {
        CAMHAL_LOGE("%s leave, bad len %d", __FUNCTION__, in_size );
        return false;
    }

    if ( in_src[0] != 0x00 ||  in_src[1] != 0x00 ||  in_src[2] != 0x00 || in_src[3] != 0x01) {
        CAMHAL_LOGE("%s leave, not start with 00 00 00 01", __FUNCTION__ );
        return false;
    }

    if (decoderStreamType == USBSensorHWDec::H264_STREAM) {
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

void *USBSensorHWDec::decodeFillThreadProc(void *data){
    uint8_t *src = nullptr;
    uint32_t src_len = 0;
    USBSensorHWDec *sensor = static_cast<USBSensorHWDec *>(data);

    CVideoInfo *vinfo = sensor->mVinfo;
    HWVideoDecoder *decoder = sensor->mHWDecoder;

    bool firstIDRfilled = false;

    CAMHAL_LOGI("%s line %d, in", __FUNCTION__, __LINE__);

    while (false == sensor->mNeedStopDecodeFillThread) {
        int ret = 0;

        if (sensor->mDecoderStreamType == H264_STREAM || sensor->mDecoderStreamType == HEVC_STREAM) {
            ret = sensor->checkAndGetNextSensorData(&src, &src_len);
        } else if (sensor->mDecoderStreamType == MJPEG_STREAM) {
            ret = sensor->checkAndGetLatestSensorData(&src, &src_len);
        } else {
            CAMHAL_LOGE("unknown pixel fmt for decoder");
            break;
        }

        if (nullptr == src && ret == 0) {
            CAMHAL_LOGE("%s, line %d can not get sensor data", __FUNCTION__, __LINE__);
            continue;
        } else if (ret < 0) {
            break;
        }

        if (sensor->mDecoderStreamType == H264_STREAM || sensor->mDecoderStreamType == HEVC_STREAM) {
            // start with IDR frame. [normally after v4l2 setting. first frame is IDR.]
            // this code segment take effect in case abnormal things occurs.
            if (!firstIDRfilled && !isIDR(sensor->mDecoderStreamType, src, src_len)) {
                // not IDR filled.
                CAMHAL_LOGD("H264 bs not IDR, skip it");
                vinfo->putback_frame();
                continue;
            } else if (false == firstIDRfilled) {
                CAMHAL_LOGI("h264, first idr queue.");
                firstIDRfilled = true;
            }
        }

        CAMHAL_LOGW("%d, queue input src %p size %d", sensor->mDecoderStreamType, src, src_len);
        decoder->asyncDecodeQueueInput(-1, src, src_len);
        vinfo->putback_frame();
    }

    CAMHAL_LOGI("%s line %d, leave", __FUNCTION__, __LINE__);

    return((void *)0);
}

int USBSensorHWDec::startDecodeFillThread()
{
    int ret = 0;

    CAMHAL_LOGI("%s, line %d, enter ", __FUNCTION__, __LINE__);

    mNeedStopDecodeFillThread = false;

    if ( THREAD_STATE_DEAD == mDecodeFillThreadState ) {
        ret = pthread_create(&mDecodeFillThreadId, NULL, USBSensorHWDec::decodeFillThreadProc, this);
        if (ret != 0) {
            CAMHAL_LOGE("****create thread fail\n");
        }
        mDecodeFillThreadState = THREAD_STATE_CREATED;
    } else {
        CAMHAL_LOGW("line %d, thread already started", __LINE__);
    }
    return ret;
}

int USBSensorHWDec::stopDecodeFillThread()
{
    int ret = 0;

    if ( THREAD_STATE_DEAD != mDecodeFillThreadState ) {
        mNeedStopDecodeFillThread = true;
        pthread_join(mDecodeFillThreadId, NULL);
        mDecodeFillThreadState = THREAD_STATE_DEAD;
        CAMHAL_LOGI("line %d, thread stopped", __LINE__);
    }

    return ret;
}

bool USBSensorHWDec::isNeedDump() {
    if (property_get_bool("camera.debug.dump.decoder", false)) {
        return true;
    }
    return false;
}

}

