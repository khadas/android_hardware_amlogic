#define LOG_NDEBUG 0
#define LOG_TAG "CaptureUseGe2d"
#define ATRACE_TAG (ATRACE_TAG_CAMERA | ATRACE_TAG_HAL | ATRACE_TAG_ALWAYS)
#include <utils/Trace.h>
#include "CaptureUseGe2d.h"
#include "ge2d_stream.h"
#if defined(PREVIEW_DEWARP_ENABLE) || defined(PICTURE_DEWARP_ENABLE)
#include "dewarp.h"
#endif
#define GE2D_SCALER

static void dump2File(const char* name, void* src, int length) {
    CAMHAL_LOGD("dump2File full_name:%s", name);
    auto fp = fopen(name, "ab+");
    if (!fp) {
        CAMHAL_LOGE("open file %s fail, error: %s !!!", name, strerror(errno));
        return;
    }
    if (src == nullptr || length <= 0) {
        CAMHAL_LOGE("invalid parameter %p %d !!!", src, length);
        fclose(fp);
        return;
    }
    fwrite(src, 1 ,length, fp);
    fclose(fp);
}

namespace android {
    CaptureUseGe2d::CaptureUseGe2d(MIPIVideoInfo* info) {
        mInfo = info;
        mCameraUtil = new CameraUtil();
        mGE2D = new ge2dTransform();
    }

    CaptureUseGe2d::~CaptureUseGe2d() {
        if (mCameraUtil) {
            delete mCameraUtil;
            mCameraUtil = nullptr;
        }
        if (mGE2D) {
            delete mGE2D;
            mGE2D = nullptr;
        }
    }

int CaptureUseGe2d::getPicture(StreamBuffer b, struct data_in* in, IONInterface *ion) {
    int ret = 0;
    int length;
    struct VideoInfoBuffer vb;
    ret = mInfo->get_picture_buffer(&vb);
    ret = mInfo->putback_picture_frame();//drop one frame
    ret = mInfo->get_picture_buffer(&vb);
    ret = mInfo->putback_picture_frame();//drop one frame
    ret = mInfo->get_picture_buffer(&vb);
    int dmabuf_fd = vb.dma_fd;
    if (-1 == ret || -1 == dmabuf_fd) {
        CAMHAL_LOGE("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
        usleep(5000);
        return ERROR_FRAME; // no frame data
    }

    uint32_t format = mInfo->get_picture_pixelformat();
    uint32_t width = mInfo->get_picture_width();
    uint32_t height = mInfo->get_picture_height();

#ifdef PICTURE_DEWARP_ENABLE
    int outbuf_fd = -1;
    if (format == V4L2_PIX_FMT_NV21) {
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.camhal.use.dewarp.capture", property, "false");
        if (strstr(property, "true")) {
            DeWarp* GDCObj = nullptr;
            property_get("vendor.camhal.use.dewarp.linear", property, "false");
            CameraConfig* config = CameraConfig::getInstance(DEWARP_CAM2PORT_CAPTURE);
            config->setWidth(width);
            config->setHeight(height);
            config->setStride(b.stride);
            ion->alloc_buffer(width  * height * 3/2, &outbuf_fd);
            CAMHAL_LOGV("%s-%d b.width:%d b.height:%d width:%d,height:%d",__FUNCTION__,__LINE__,b.width,b.height,\
                config->getWidth(),config->getHeight());
            if (strstr(property, "true")) {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE,PROJ_MODE_LINEAR,Rotation::ROTATION_0);
            } else {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE,PROJ_MODE_EQUIDISTANCE,Rotation::ROTATION_0);
            }
            if (GDCObj) {
                GDCObj->mInput_fd = dmabuf_fd;
                GDCObj->mOutput_fd = outbuf_fd;
                GDCObj->gdc_do_fisheye_correction();
            }
            dmabuf_fd = outbuf_fd;
        }
    }
#endif

    switch (format) {
        case V4L2_PIX_FMT_RGB24:
            CAMHAL_LOGE("%s:config format is RGB888 ",__FUNCTION__);
            length = width * height * 3;
            memcpy(b.img, vb.addr, length);
            break;
        case V4L2_PIX_FMT_NV21:
            CAMHAL_LOGD("%s:width=%d,height=%d,size=%d",__FUNCTION__,b.width,b.height,vb.size);
            mGE2D->ge2d_keep_ration_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12,
                b.width, b.height, dmabuf_fd, width, height);
            if (property_get_bool("vendor.camhal.dump.capture", false)) {
                char path[256];
                static int index = 0;
                sprintf(path, "/data/vendor/camera/capture-in-%dx%d-%d.yuv", width, height, index);
                dump2File(path, vb.addr, width * height *3/2);
                sprintf(path, "/data/vendor/camera/capture-out-%dx%d-%d.yuv", b.width, b.height, index);
                dump2File(path, b.img, b.width * b.height * 3/2);
                index++;
            }
            break;
        default:
            CAMHAL_LOGE("%s:not support this format",__FUNCTION__);
            break;
    }

#ifdef PICTURE_DEWARP_ENABLE
    if (format == V4L2_PIX_FMT_NV21)
        ion->free_buffer(outbuf_fd);
#endif
    return 0; //get a new frame
}

int CaptureUseGe2d::captureYUYVframe(uint8_t *img, struct data_in* in) {
    uint8_t* src = nullptr;
    uint32_t format = mInfo->get_preview_pixelformat();
    src = in->src;
    if (src) {
        switch (format) {
            case V4L2_PIX_FMT_YUYV:
                break;
            default:
                CAMHAL_LOGE("Unable known sensor format: %d", format);
                break;
        }
        return 0;
    }

    src = (uint8_t *)mInfo->get_frame();
    if (nullptr == src) {
        CAMHAL_LOGV("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }

    if (format == V4L2_PIX_FMT_YUYV)
        memcpy(img, src, mInfo->get_preview_buf_length());

    return 0;
}

int CaptureUseGe2d::captureNV21frame(StreamBuffer b, struct data_in* in) {
    ATRACE_CALL();


    if (in->share_fd > 0  && in->src_fmt > 0) {
        switch (in->src_fmt) {
            case V4L2_PIX_FMT_NV21:
                //  we assume that [in] is always preview stream
                if (b.width < 3840 && b.height < 2160) {
                    if ((in->src_width == b.width) && (in->src_height == b.height)) {
                        mGE2D->ge2d_copy(b.share_fd, in->share_fd, b.stride, b.height, V4L2_PIX_FMT_NV21);
                    } else {
                        if (in->src_width < b.width || in->src_height < b.height) {
                            CAMHAL_LOGW("ge2d scale from small to big; nv21 src w %d h %d fd %d, dst w %d h %d fd %d",
                                   in->src_width, in->src_height, in->share_fd, b.width, b.height, b.share_fd);
                        }
                        mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height, in->share_fd, in->src_width, in->src_height);
                    }
                } else {
                    struct VideoInfoBuffer vb_rec;
                    auto ret = mInfo->get_record_buffer(&vb_rec);
                    int dmabuf_fd_rec = vb_rec.dma_fd;
                    if (-1 == ret || -1 == dmabuf_fd_rec) {
                        CAMHAL_LOGE("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
                        usleep(5000);
                        return ERROR_FRAME; // no frame data
                    }
                    if (mInfo->get_record_width() != b.width || mInfo->get_record_height() != b.height) {
                        //  this is snapshot capture case
                        CAMHAL_LOGW("%s:config miss match src: %dx%d, dst: %dx%d", __FUNCTION__,
                            mInfo->get_record_width(), mInfo->get_record_height(), b.width, b.height);
                        mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          dmabuf_fd_rec, mInfo->get_record_width(), mInfo->get_record_height());
                    } else {
#ifdef PREVIEW_DEWARP_ENABLE
                        char property[PROPERTY_VALUE_MAX];
                        property_get("vendor.camhal.use.dewarp.rec", property, "true");
                        if (strstr(property, "true")) { //dewarp rec
                            DeWarp* GDCObj = nullptr;
                            property_get("vendor.camhal.use.dewarp.linear", property, "true");
                            CameraConfig* config = CameraConfig::getInstance(DEWARP_CAM2PORT_CAPTURE);
                            config->setWidth(b.width);
                            config->setHeight(b.height);
                            config->setStride(b.stride);
                            CAMHAL_LOGV("%s-%d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
                                  __FUNCTION__, __LINE__, b.width, b.height, b.share_fd,
                                  config->getWidth(), config->getHeight(), dmabuf_fd_rec);
                            if (strstr(property, "true")) {
                                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
                            } else {
                                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_CAPTURE, PROJ_MODE_EQUISOLID, Rotation::ROTATION_0);
                            }
                            if (GDCObj) {
                                GDCObj->mInput_fd = dmabuf_fd_rec;
                                GDCObj->mOutput_fd = b.share_fd;
                                GDCObj->gdc_do_fisheye_correction();
                            }
                        }
#else
                        mGE2D->ge2d_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.height,
                                          dmabuf_fd_rec, mInfo->get_record_width(), mInfo->get_record_height());
#endif
                    }
                    ret = mInfo->putback_record_frame();
                    if (property_get_bool("vendor.camhal.dump", false)) {
                        char path[256];
                        static int index = 0;
                        if (index % 10 == 0) {
                            sprintf(path, "/data/vendor/camera/video-in-%dx%d-%d.yuv",
                                mInfo->get_record_width(), mInfo->get_record_height(), index);
                            dump2File(path, vb_rec.addr, mInfo->get_record_buf_length());
                            sprintf(path, "/data/vendor/camera/video-out-%dx%d-%d.yuv", b.width, b.height, index);
                            dump2File(path, b.img, b.width * b.height * 3/2);
                        }
                        index++;
                    }
                }
                break;
            default:
                CAMHAL_LOGE("Unable known sensor format: %d", mInfo->get_preview_pixelformat());
                break;
        }
        return NO_NEW_FRAME;
    }

    int dmabuf_fd = -1;
    uint32_t width = mInfo->get_preview_width();
    uint32_t height = mInfo->get_preview_height();
    uint32_t format = mInfo->get_preview_pixelformat();
    uint32_t stride = mInfo->get_preview_stride();

    struct VideoInfoBuffer vb;
    int ret = mInfo->get_frame_buffer(&vb);
    dmabuf_fd = vb.dma_fd;
    if (-1 == ret || -1 == dmabuf_fd) {
        CAMHAL_LOGV("%s:get frame fd fail!, sleep 5ms",__FUNCTION__);
        usleep(5000);
        return ERROR_FRAME;
    }
#ifdef PREVIEW_DEWARP_ENABLE
        char property[PROPERTY_VALUE_MAX];
        property_get("vendor.camhal.use.dewarp", property, "false");
        if (strstr(property, "true")) {//dewarp
            DeWarp* GDCObj = nullptr;
            property_get("vendor.camhal.use.dewarp.linear", property, "true");
            CameraConfig* config = CameraConfig::getInstance(DEWARP_CAM2PORT_PREVIEW);
            config->setWidth(b.width);
            config->setHeight(b.height);
            config->setStride(b.stride);
            CAMHAL_LOGV("%s-%d b.width:%d b.height:%d b.fd:%d width:%d height:%d fd:%d",
                  __FUNCTION__, __LINE__, b.width, b.height, b.share_fd,
                  config->getWidth(), config->getHeight(), dmabuf_fd);
            if (strstr(property, "true")) {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_PREVIEW, PROJ_MODE_LINEAR, Rotation::ROTATION_0);
            } else {
                GDCObj = DeWarp::getInstance(DEWARP_CAM2PORT_PREVIEW, PROJ_MODE_EQUISOLID, Rotation::ROTATION_0);
            }
            if (GDCObj) {
                GDCObj->mInput_fd = dmabuf_fd;
                GDCObj->mOutput_fd = b.share_fd;
                GDCObj->gdc_do_fisheye_correction();
            }
        }
        else {
            switch (format) {
                case V4L2_PIX_FMT_NV21:
                    if (width == b.width && height == b.height && stride == b.stride) {
                        CAMHAL_LOGV("line %d ge2d copy dmabuf_fd %d  w %d stride %d h %d \n", __LINE__, dmabuf_fd, b.width, b.stride, b.height);
                        mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride,b.height, ge2dTransform::NV12);
                    } else {
                        CAMHAL_LOGV("line %d ge2d scale in w %d stride %d h %d , out w %d stride %d h %d", __LINE__, width, stride, height,
                                  b.width, b.stride, b.height);
                        mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.stride, b.height,
                                                  dmabuf_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, width, stride, height);
                    }

                    break;
                case V4L2_PIX_FMT_UYVY:
                    CAMHAL_LOGV("line %d ge2d_convert_scale b fd %d w %d stride %d h %d; src fd %d w %d h %d \n", __LINE__,
                                   b.share_fd, b.width, b.stride, b.height, dmabuf_fd, width, height);

                    mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCrCb_420_SP, b.width, b.width, b.height,
                                               dmabuf_fd, PIXEL_FORMAT_YCbCr_422_UYVY, width, width * 2, height);

                    break;
                default:
                    break;
            }
        }
        if (property_get_bool("vendor.camhal.dump", false)) {
            char path[256];
            static int index2 = 0;
            if (index2 % 10 == 0) {
                sprintf(path, "/data/vendor/camera/preview-in-%dx%d-%d.yuv",
                    mInfo->get_preview_width(), mInfo->get_preview_height(), index2);
                dump2File(path, vb.addr, mInfo->get_preview_buf_length());
                sprintf(path, "/data/vendor/camera/preview-out-%dx%d-%d.yuv", b.width, b.height, index2);
                dump2File(path, b.img, b.width *  b.height * 3/2);
            }
            index2++;
        }
#else
    switch (format) {
        case V4L2_PIX_FMT_NV21:
            if (width == b.width && height == b.height && stride == b.stride) {
                CAMHAL_LOGV("line %d ge2d copy dmabuf_fd %d  w %d stride %d h %d \n", __LINE__, dmabuf_fd, b.width, b.stride, b.height);
                mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride,b.height, ge2dTransform::NV12);
            } else {
                CAMHAL_LOGV("line %d ge2d scale in w %d stride %d h %d , out w %d stride %d h %d", __LINE__, width, stride, height,
                          b.width, b.stride, b.height);
                mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, b.width, b.stride, b.height,
                                          dmabuf_fd, PIXEL_FORMAT_YCbCr_420_SP_NV12, width, stride, height);
            }
            break;
        case V4L2_PIX_FMT_UYVY:
            CAMHAL_LOGV("line %d ge2d_convert_scale b fd %d w %d stride %d h %d; src fd %d w %d h %d \n", __LINE__,
                           b.share_fd, b.width, b.stride, b.height, dmabuf_fd, width, height);

            mGE2D->ge2d_convert_scale(b.share_fd, PIXEL_FORMAT_YCrCb_420_SP, b.width, b.width, b.height,
                                       dmabuf_fd, PIXEL_FORMAT_YCbCr_422_UYVY, width, width * 2, height);
            break;
        default:
            break;
    }
#endif
    in->dmabuf_fd = dmabuf_fd;
    return NEW_FRAME;
}

int CaptureUseGe2d::captureYV12frame(StreamBuffer b, struct data_in* in) {
        //ATRACE_CALL();
        int dmabuf_fd = -1;
        uint32_t format = mInfo->get_preview_pixelformat();

        struct VideoInfoBuffer vb;
        int ret = mInfo->get_frame_buffer(&vb);
        if (-1 == ret) {
            CAMHAL_LOGV("get frame NULL, sleep 5ms");
            usleep(5000);
            return -1;
        }
        dmabuf_fd = vb.dma_fd;
        switch (format) {
            case V4L2_PIX_FMT_YVU420:
                if (mInfo->get_preview_buf_length() == b.width * b.height * 3/2) {
                    mGE2D->ge2d_copy(b.share_fd, dmabuf_fd, b.stride, b.height, ge2dTransform::NV12);
                }
                break;
            default:
                break;
            }
            return 0;
}

int CaptureUseGe2d::captureRGBAframe(StreamBuffer b, struct data_in* in){
    uint32_t width = mInfo->get_preview_width();
    uint32_t height = mInfo->get_preview_height();
    uint32_t format = mInfo->get_preview_pixelformat();
    uint8_t *src = nullptr;
    int dmabuf_fd = -1;

    src = in->src;
    if (src && in->src_fmt > 0) {
        switch (in->src_fmt) {
            case V4L2_PIX_FMT_NV21:
                break;
                default:
                    CAMHAL_LOGE("Unable known sensor format: %d", mInfo->get_preview_pixelformat());
                    break;
        }
        return NO_NEW_FRAME;
    }
    struct VideoInfoBuffer vb;
    int ret = mInfo->get_frame_buffer(&vb);
    if (-1 == ret) {
        CAMHAL_LOGV("get frame NULL, sleep 5ms");
        usleep(5000);
        return -1;
    }
    dmabuf_fd = vb.dma_fd;
    switch (format) {
        case V4L2_PIX_FMT_NV21:
            mGE2D->ge2d_fmt_convert(b.share_fd, PIXEL_FORMAT_RGBA_8888, b.stride, b.height,
                                                  dmabuf_fd,  PIXEL_FORMAT_YCrCb_420_SP, width, height);
            break;
        default:
            CAMHAL_LOGE("error: only support NV21 -> RGBA");
            break;
    }
    return NEW_FRAME;
}
}
