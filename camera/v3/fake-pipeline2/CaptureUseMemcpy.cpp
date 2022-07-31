#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>
#include "CaptureUseMemcpy.h"

namespace android {

CaptureUseMemcpy::CaptureUseMemcpy(MIPIVideoInfo* info) {
    mInfo = info;
    mCameraUtil = new CameraUtil();
}

CaptureUseMemcpy::~CaptureUseMemcpy() {
    if (mCameraUtil) {
        delete mCameraUtil;
        mCameraUtil = nullptr;
    }
}

int CaptureUseMemcpy::getPicture(StreamBuffer b, struct data_in* in, IONInterface *ion) {
        uint8_t* src = nullptr;
        uint32_t width = mInfo->get_picture_width();
        uint32_t height = mInfo->get_picture_height();
        uint32_t format = mInfo->get_picture_pixelformat();

        ALOGD("%s:  width=%d, height=%d img %p",__FUNCTION__,width,height,b.img);
        src = (uint8_t *)mInfo->get_picture();
        ALOGD("%s:src=%p",__FUNCTION__,src);
        if (nullptr == src)
            return ERROR_FRAME;

        switch (format) {
            case V4L2_PIX_FMT_YUYV:
                if (mInfo->get_picture_buf_length() == mInfo->get_picture_buf_bytesused())
                    mCameraUtil->yuyv422_to_rgb24(src,b.img,width,height);
                break;
            case V4L2_PIX_FMT_RGB24:
                ALOGD("%s: picture format is RGB",__FUNCTION__);
                if (mInfo->get_picture_buf_length() == width * height * 3) {
                    memcpy(b.img, src, mInfo->get_picture_buf_length());
                } else {
                    mCameraUtil->rgb24_memcpy(b.img, src, width, height);
                }
                break;
            case V4L2_PIX_FMT_NV21:
#ifndef HW_JPEG
                mCameraUtil->nv21_to_rgb24(src, b.img, width,height);
#else
                memcpy(b.img, src, mInfo->get_picture_buf_length());
#endif
                break;

            default:
                break;
        }
    return 0;
}

int CaptureUseMemcpy::captureYUYVframe(uint8_t *img, struct data_in* in) {
        uint8_t* src = nullptr;
        uint32_t format = mInfo->get_preview_pixelformat();
        src = in->src;
        if (src) {
            switch (format) {
                case V4L2_PIX_FMT_YUYV:
                    break;
                default:
                    ALOGE("Unable known sensor format: %d", format);
                    break;
            }
            return 0;
        }

        src = (uint8_t *)mInfo->get_frame();
        if (nullptr == src) {
            ALOGV("get frame NULL, sleep 5ms");
            usleep(5000);
            return -1;
        }

        if (format == V4L2_PIX_FMT_YUYV)
            memcpy(img, src, mInfo->get_preview_buf_length());

        return 0;
}

int CaptureUseMemcpy::captureNV21frame(StreamBuffer b, struct data_in* in) {
        ATRACE_CALL();
        ALOGV("%s ", __FUNCTION__);
        uint8_t *temp_buffer = nullptr;
        uint32_t width = mInfo->get_preview_width();
        uint32_t height = mInfo->get_preview_height();
        uint32_t format = mInfo->get_preview_pixelformat();
        uint8_t *src = nullptr;
        src = in->src;
        if (src) {
            switch (format) {
                case V4L2_PIX_FMT_NV21:
                    if ((width == b.width) && (height == b.height)) {
                        memcpy(b.img, src, b.stride * b.height * 3/2);
                    } else {
                        mCameraUtil->ReSizeNV21(src, b.img, b.width, b.height, b.stride,width,height);
                    }
                    break;
                case V4L2_PIX_FMT_YUYV:
                    if ((width == b.width) && (height == b.height)) {
                        memcpy(b.img, src, b.stride * b.height * 3/2);
                    } else {
                        mCameraUtil->ReSizeNV21(src, b.img, b.width, b.height, b.stride,width,height);
                    }
                    break;
                default:
                    ALOGE("Unable known sensor format: %d", mInfo->get_preview_pixelformat());
                    break;
            }
            return NO_NEW_FRAME;
        }
        struct VideoInfoBuffer vb;
        int ret = mInfo->get_frame_buffer(&vb);
        if (-1 == ret) {
            ALOGE("get frame NULL, sleep 5ms");
            usleep(5000);
            return ERROR_FRAME;
        }
        in->dmabuf_fd = vb.dma_fd;
        switch (format) {
            case V4L2_PIX_FMT_NV21:
                if (mInfo->get_preview_buf_length() == b.width * b.height * 3/2) {
                    memcpy(b.img, vb.addr, mInfo->get_preview_buf_length());
                } else {
                    mCameraUtil->nv21_memcpy_align32 (b.img, (uint8_t*)vb.addr, b.width, b.height);
                }
                break;
            case V4L2_PIX_FMT_YUYV:
                temp_buffer = (uint8_t *)malloc(width * height * 3/2);
                memset(temp_buffer, 0 , width * height * 3/2);
                mCameraUtil->YUYVToNV21((uint8_t*)vb.addr, temp_buffer, width, height);
                memcpy(b.img, temp_buffer, b.width * b.height * 3/2);
                free(temp_buffer);
                break;
               default:
                break;
        }
        return NEW_FRAME;
}

int CaptureUseMemcpy::captureYV12frame(StreamBuffer b, struct data_in* in) {
        uint8_t* src = nullptr;
        uint32_t format = mInfo->get_preview_pixelformat();
        uint32_t width = mInfo->get_preview_width();
        uint32_t height = mInfo->get_preview_height();
        src = in->src;
        if (src) {
            switch (format) {
                case V4L2_PIX_FMT_YVU420:
                    mCameraUtil->ScaleYV12(src,width,height,b.img,b.width,b.height);
                    break;
                case V4L2_PIX_FMT_YUYV:
                    mCameraUtil->YUYVScaleYV12(src,width,height,b.img,b.width,b.height);
                    break;
                default:
                    ALOGE("Unable known sensor format: %d",format);
                    break;
            }
            return 0;
        }

         src = (uint8_t *)mInfo->get_frame();
        if (nullptr == src) {
            ALOGV("get frame NULL, sleep 5ms");
            usleep(5000);
            return -1;
        }

        switch (format) {
            case V4L2_PIX_FMT_YVU420:
                if (mInfo->get_preview_buf_length() == b.width * b.height * 3/2) {
                    memcpy(b.img, src, mInfo->get_preview_buf_length());
                } else {
                    mCameraUtil->yv12_memcpy_align32 (b.img, src, b.width, b.height);
                }
                break;
            case V4L2_PIX_FMT_YUYV:
                mCameraUtil->YUYVToYV12(src, b.img, width, height);
                break;
            default:
                break;
            }
            return 0;
  }
}
