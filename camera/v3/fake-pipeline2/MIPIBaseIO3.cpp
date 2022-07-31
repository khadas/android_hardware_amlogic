/*
 * Copyright (C) 2011 The Android Open Source Project
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
 *
 * Description:
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "MIPI_BASE_IO_3"

#include <errno.h>
#include <cutils/properties.h>
#include "MIPIBaseIO3.h"
#include <poll.h>

namespace android {


VideoInfoUsePictureScaler::VideoInfoUsePictureScaler() {
    memset(&cap, 0, sizeof(struct v4l2_capability));
    memset(&preview,0,sizeof(FrameV4L2Info));
    memset(&picture,0,sizeof(FrameV4L2Info));
    memset(&picture_config,0,sizeof(FrameV4L2Info));
    memset(mem_pic,0,sizeof(mem_pic));
    //memset(canvas,0,sizeof(canvas));
    isStreaming = false;
    mIsPicture = false;
    //canvas_mode = false;
    width = 0;
    height = 0;
    formatIn = 0;
    framesizeIn = 0;
    idVendor = 0;
    idProduct = 0;
    idx = 0;
    mPreviewFd = -1;
    mSnapShotFd = -1;
    tempbuflen = 0;
    dev_status = 0;
    //mMuteKeyStatus = EVENT_CAMERA_UNMUTE;
}

int VideoInfoUsePictureScaler::EnumerateFormat(uint32_t pixelformat){
    struct v4l2_fmtdesc fmt;
    int ret;
    if (mPreviewFd < 0) {
        ALOGE("camera not be init!");
        return -1;
    }
    memset(&fmt,0,sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.index = 0;
    while ((ret = ioctl(mPreviewFd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
        if (fmt.pixelformat == pixelformat)
            return pixelformat;
        fmt.index++;
    }
    return 0;
}

bool VideoInfoUsePictureScaler::IsSupportRotation() {
    struct v4l2_queryctrl qc;
    int ret = 0;
    bool Support = false;
    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_ROTATE_ID;
    ret = ioctl (mPreviewFd, VIDIOC_QUERYCTRL, &qc);
    if ((qc.flags == V4L2_CTRL_FLAG_DISABLED) ||( ret < 0)|| (qc.type != V4L2_CTRL_TYPE_INTEGER)) {
            Support = false;
    }else{
            Support = true;
    }
    return Support;
}

int VideoInfoUsePictureScaler::set_rotate(int camera_fd, int value)
{
    int ret = 0;
    struct v4l2_control ctl;
    if (camera_fd < 0)
        return -1;
    if ((value != 0) && (value != 90) && (value != 180) && (value != 270)) {
        CAMHAL_LOGDB("Set rotate value invalid: %d.", value);
        return -1;
    }
    memset( &ctl, 0, sizeof(ctl));
    ctl.value=value;
    ctl.id = V4L2_CID_ROTATE;
    ALOGD("set_rotate:: id =%x , value=%d",ctl.id,ctl.value);
    ret = ioctl(camera_fd, VIDIOC_S_CTRL, &ctl);
    if (ret<0) {
        CAMHAL_LOGDB("Set rotate value fail: %s,errno=%d. ret=%d", strerror(errno),errno,ret);
    }
    return ret ;
}

void VideoInfoUsePictureScaler::set_device_status(void)
{
    dev_status = -1;
}

int VideoInfoUsePictureScaler::get_device_status(void)
{
    return dev_status;
}

int VideoInfoUsePictureScaler::camera_init(void)
{
    ALOGV("%s: E", __FUNCTION__);
    int ret =0 ;
    if (mPreviewFd < 0) {
          ALOGE("open /dev/video%d failed, errno=%s\n", idx, strerror(errno));
          return -ENOTTY;
    }

    ret = ioctl(mPreviewFd, VIDIOC_QUERYCAP, &cap);
    if (ret < 0) {
        ALOGE("VIDIOC_QUERYCAP, errno=%s", strerror(errno));
        return ret;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE))
        ALOGV( "/dev/video%d is not video capture device\n",idx);


    if (!(cap.capabilities & V4L2_CAP_STREAMING))
        ALOGV( "video%d does not support streaming i/o\n",idx);

    if (strstr((const char*)cap.driver,"ARM-camera-isp"))
        sprintf(sensor_type,"%s","mipi");
    else
        sprintf(sensor_type,"%s","usb");

    return ret;
}

int VideoInfoUsePictureScaler::setBuffersFormat(void)
{
        int ret = 0;
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if ((preview.format.fmt.pix.width != 0) && (preview.format.fmt.pix.height != 0)) {
        int pixelformat = preview.format.fmt.pix.pixelformat;

        ret = ioctl(mPreviewFd, VIDIOC_S_FMT, &preview.format);
        if (ret < 0) {
                DBG_LOGB("Open: VIDIOC_S_FMT Failed: %s, ret=%d\n", strerror(errno), ret);
        }

        CAMHAL_LOGIB("Width * Height %d x %d expect pixelfmt:%.4s, get:%.4s\n",
                        preview.format.fmt.pix.width,
                        preview.format.fmt.pix.height,
                        (char*)&pixelformat,
                        (char*)&preview.format.fmt.pix.pixelformat);
        }
        return ret;
}

void VideoInfoUsePictureScaler::set_buffer_numbers(int io_buffer) {
    IO_PREVIEW_BUFFER = io_buffer;
}


int VideoInfoUsePictureScaler::start_capturing(void)
{
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;

        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (isStreaming) {
                DBG_LOGA("already stream on\n");
        }
        CLEAR(preview.rb);
        mem.resize(IO_PREVIEW_BUFFER);
        preview.rb.count = IO_PREVIEW_BUFFER;
        preview.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //TODO DMABUF & ION
        preview.rb.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
                DBG_LOGB("camera idx:%d does not support "
                                "memory mapping, errno=%d\n", idx, errno);
        }
        if (preview.rb.count < 2) {
                DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                                idx, errno);
                return -EINVAL;
        }

        for (i = 0; i < (int)preview.rb.count; ++i) {
            CLEAR(preview.buf);
            preview.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            preview.buf.memory      = V4L2_MEMORY_MMAP;
            preview.buf.index       = i;

            if (ioctl(mPreviewFd, VIDIOC_QUERYBUF, &preview.buf) < 0) {
                    DBG_LOGB("VIDIOC_QUERYBUF, errno=%d", errno);
            }
            /*pluge usb camera when preview,
              vinfo->preview.buf.length value will equal to 0,
              so save this value
            */
            mem[i].size = preview.buf.length;
            mem[i].addr = mmap(NULL /* start anywhere */,
                                mem[i].size,
                                PROT_READ | PROT_WRITE /* required */,
                                MAP_SHARED /* recommended */,
                                mPreviewFd,
                                preview.buf.m.offset);

            if (MAP_FAILED == mem[i].addr) {
                    DBG_LOGB("mmap failed, errno=%d\n", errno);
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(mPreviewFd,i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem[i].dma_fd = dma_fd;
                ALOGD("index = %d, dma_fd = %d \n",i,mem[i].dma_fd);
            }
        }
        ////////////////////////////////
        for (i = 0; i < (int)preview.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(mPreviewFd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
             if (ioctl(mPreviewFd, VIDIOC_STREAMON, &type) < 0)
                  DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        }

        isStreaming = true;
        return 0;
}

int VideoInfoUsePictureScaler::stop_capturing(void)
{
        enum v4l2_buf_type type;
        int res = 0;
        int i;
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (!isStreaming)
                return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mPreviewFd, VIDIOC_STREAMOFF, &type) < 0) {
                ALOGE("VIDIOC_STREAMOFF : %s", strerror(errno));
                res = -1;
        }

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }

        for (i = 0; i < (int)preview.rb.count; ++i) {
                if (munmap(mem[i].addr, mem[i].size) < 0) {
                        DBG_LOGB("munmap failed errno=%d", errno);
                        res = -1;
                }
                if (mem[i].dma_fd != -1 && !res) {
                    close(mem[i].dma_fd);
                    mem[i].dma_fd = -1;
                }
        }

        preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        preview.rb.count = 0;

        res = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
        if (res < 0) {
            ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        }else{
            ALOGE("VIDIOC_REQBUFS delete buffer success\n");
        }

        isStreaming = false;
        return res;
}

int VideoInfoUsePictureScaler::releasebuf_and_stop_capturing(void)
{
#if 0
        enum v4l2_buf_type type;
        int res = 0 ,ret;
        int i;

        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (!isStreaming)
                return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
            if (ioctl(mPreviewFd, VIDIOC_STREAMOFF, &type) < 0) {
                 DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);
                 res = -1;
            }
        }
        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }
        for (i = 0; i < (int)preview.rb.count; ++i) {
                if (munmap(mem[i].addr, mem[i].size) < 0) {
                        DBG_LOGB("munmap failed errno=%d", errno);
                        res = -1;
                }
        }
        isStreaming = false;

        preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        preview.rb.count = 0;

        ret = ioctl(mPreviewFd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
           DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
           //return ret;
        }else{
           DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        return res;
#endif
        return 0;
}

int VideoInfoUsePictureScaler::start_recording(void)
{
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;

        if (mRecordFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (mIsRecording) {
            DBG_LOGA("already stream on\n");
        }

        for (int i = 0; i < 1; i++) {
            ret = ioctl(mRecordFd, VIDIOC_S_FMT, &record.format);
            if (ret < 0 ) {
                 switch (errno) {
                     case  -EBUSY:
                     case  0:
                        usleep(3000); //3ms
                        continue;
                        default:
                        DBG_LOGB("Open: VIDIOC_S_FMT Failed: %s, errno=%d\n", strerror(errno), ret);
                     return ret;
                 }
            }else
                break;
        }

        CLEAR(record.rb);
        mem_rec.resize(IO_PREVIEW_BUFFER);
        record.rb.count = IO_PREVIEW_BUFFER;
        record.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //TODO DMABUF & ION
        record.rb.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(mRecordFd, VIDIOC_REQBUFS, &record.rb);
        if (ret < 0) {
            DBG_LOGB("camera idx:%d does not support "
                            "memory mapping, errno=%d\n", idx, errno);
        }
        if (record.rb.count < 2) {
			DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
							idx, errno);
			return -EINVAL;
        }

        for (i = 0; i < (int)record.rb.count; ++i) {
            CLEAR(record.buf);
            record.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            record.buf.memory      = V4L2_MEMORY_MMAP;
            record.buf.index       = i;

            if (ioctl(mRecordFd, VIDIOC_QUERYBUF, &record.buf) < 0) {
                DBG_LOGB("VIDIOC_QUERYBUF, errno=%d", errno);
            }
            /*pluge usb camera when preview,
              vinfo->preview.buf.length value will equal to 0,
              so save this value
            */
            mem_rec[i].size = record.buf.length;
            mem_rec[i].addr = mmap(NULL /* start anywhere */,
                                mem_rec[i].size,
                                PROT_READ | PROT_WRITE /* required */,
                                MAP_SHARED /* recommended */,
                                mRecordFd,
                                record.buf.m.offset);

            if (MAP_FAILED == mem_rec[i].addr) {
                    DBG_LOGB("mmap failed, errno=%d\n", errno);
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(mRecordFd, i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem_rec[i].dma_fd = dma_fd;
                ALOGD("index = %d, dma_fd = %d \n", i, mem_rec[i].dma_fd);
            }
        }
        ////////////////////////////////
        for (i = 0; i < (int)record.rb.count; ++i) {
                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(mRecordFd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((record.format.fmt.pix.width != 0) &&
               (record.format.fmt.pix.height != 0)) {
             if (ioctl(mRecordFd, VIDIOC_STREAMON, &type) < 0)
                  DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        }

        mIsRecording = true;
        return 0;
}

int VideoInfoUsePictureScaler::stop_recording(void)
{
        enum v4l2_buf_type type;
        int res = 0;
        int i;
        if (mRecordFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (!mIsRecording)
            return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mRecordFd, VIDIOC_STREAMOFF, &type) < 0) {
            ALOGE("VIDIOC_STREAMOFF : %s", strerror(errno));
            res = -1;
        }

        if (!record.buf.length) {
            record.buf.length = tempbuflen;
        }

        for (i = 0; i < (int)record.rb.count; ++i) {
                if (munmap(mem_rec[i].addr, mem_rec[i].size) < 0) {
                        DBG_LOGB("munmap failed errno=%d", errno);
                        res = -1;
                }
                if (mem_rec[i].dma_fd != -1 && !res) {
                    close(mem_rec[i].dma_fd);
                    mem_rec[i].dma_fd = -1;
                }
        }

        record.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        record.rb.memory = V4L2_MEMORY_MMAP;
        record.rb.count = 0;

        res = ioctl(mRecordFd, VIDIOC_REQBUFS, &record.rb);
        if (res < 0) {
            ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
        }else{
            ALOGE("VIDIOC_REQBUFS delete buffer success\n");
        }

        mIsRecording = false;
        return res;
}

uintptr_t VideoInfoUsePictureScaler::get_frame_phys(void)
{
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        CLEAR(preview.buf);

        preview.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(mPreviewFd, VIDIOC_DQBUF, &preview.buf) < 0) {
                switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
                                exit(1);
                }
        DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
        }

        return (uintptr_t)preview.buf.m.userptr;
}

void* VideoInfoUsePictureScaler::get_frame()
{
    if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return nullptr;
        }

    struct pollfd pfds[1];
    int pollret;

    pfds[0].fd = mPreviewFd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    pollret = poll(pfds, 1, POLL_TIMEOUT);
    if (pollret == 0) {
        ALOGE ("%s:poll timeout.\n",__FUNCTION__);
          return nullptr;
    } else if (pollret < 0) {
        ALOGE ("Error: poll error\n");
        return nullptr;
    }

    CLEAR(preview.buf);
    preview.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    preview.buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(mPreviewFd, VIDIOC_DQBUF, &preview.buf) < 0) {
        switch (errno) {
            case EAGAIN:
                return nullptr;

            case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

            default:
                ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                if (errno == ENODEV) {
                    ALOGE("camera device is not exist!");
                    set_device_status();
                    close(mPreviewFd);
                    mPreviewFd = -1;
                }
                return nullptr;
        }
    }

    return mem[preview.buf.index].addr;
}

int VideoInfoUsePictureScaler::putback_frame()
{
        if (dev_status == -1)
            return 0;

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (ioctl(mPreviewFd, VIDIOC_QBUF, &preview.buf) < 0) {
            DBG_LOGB("QBUF failed :%s\n", strerror(errno));
            if (errno == ENODEV) {
                set_device_status();
            }
        }

        return 0;
}

void* VideoInfoUsePictureScaler::get_record_frame()
{
    if (mRecordFd < 0) {
        ALOGE("camera not be init!");
        return nullptr;
    }

    struct pollfd pfds[1];
    int pollret;

    pfds[0].fd = mRecordFd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    pollret = poll(pfds, 1, POLL_TIMEOUT);
    if (pollret == 0) {
        ALOGE ("%s:poll timeout.\n",__FUNCTION__);
          return nullptr;
    } else if (pollret < 0) {
        ALOGE ("Error: poll error\n");
        return nullptr;
    }

    CLEAR(record.buf);
    record.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    record.buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(mRecordFd, VIDIOC_DQBUF, &record.buf) < 0) {
        switch (errno) {
            case EAGAIN:
                return nullptr;

            case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

            default:
                ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                if (errno == ENODEV) {
                    ALOGE("camera device is not exist!");
                    set_device_status();
                    close(mRecordFd);
                    mRecordFd = -1;
                }
                return nullptr;
        }
    }

    return mem[record.buf.index].addr;
}

int VideoInfoUsePictureScaler::putback_record_frame()
{
        if (dev_status == -1)
            return 0;

        if (!record.buf.length) {
            record.buf.length = tempbuflen;
        }
        if (mRecordFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (ioctl(mRecordFd, VIDIOC_QBUF, &record.buf) < 0) {
            DBG_LOGB("QBUF failed :%s\n", strerror(errno));
            if (errno == ENODEV) {
                set_device_status();
            }
        }

        return 0;
}

int VideoInfoUsePictureScaler::putback_picture_frame()
{
        CAMHAL_LOGDB("%s: E",__FUNCTION__);
        if (mSnapShotFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (ioctl(mSnapShotFd, VIDIOC_QBUF, &picture.buf) < 0)
                DBG_LOGB("QBUF failed error=%d\n", errno);

        return 0;
}

int VideoInfoUsePictureScaler::start_picture(int rotate)
{
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;

        ALOGE("%s: width=%d, height=%d E",__FUNCTION__,
            picture.format.fmt.pix.width,picture.format.fmt.pix.height);


        if (mSnapShotFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (mIsPicture) {
            ALOGD("%s:already stream on\n",__FUNCTION__);
            return 0;
        }

        //step 1 : ioctl  VIDIOC_S_FMT
        for (int i = 0; i < 1; i++) {
            ret = ioctl(mSnapShotFd, VIDIOC_S_FMT, &picture.format);
            if (ret < 0 ) {
             switch (errno) {
                 case  -EBUSY:
                 case  0:
                    usleep(3000); //3ms
                    continue;
                    default:
                    DBG_LOGB("Open: VIDIOC_S_FMT Failed: %s, errno=%d\n", strerror(errno), ret);
                 return ret;
             }
            }else
            break;
        }
        //step 2 : request buffer
        CLEAR(picture.rb);
        picture.rb.count = NUM_PICTURE_BUFFER;
        picture.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //TODO DMABUF & ION
        picture.rb.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(mSnapShotFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0 ) {
                ALOGE("camera idx:%d does not support "
                                "memory mapping, errno=%d\n", idx, errno);
        }

        if (picture.rb.count < 1) {
                ALOGE( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                                idx, errno);
                return -EINVAL;
        }

        //step 3: mmap buffer
        for (i = 0; i < (int)picture.rb.count; ++i) {

                CLEAR(picture.buf);

                picture.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                picture.buf.memory      = V4L2_MEMORY_MMAP;
                picture.buf.index       = i;

                if (ioctl(mSnapShotFd, VIDIOC_QUERYBUF, &picture.buf) < 0) {
                        ALOGE("VIDIOC_QUERYBUF, errno=%d", errno);
                }
                mem_pic[i].size = picture.buf.length;
                mem_pic[i].addr = mmap(NULL /* start anywhere */,
                                        mem_pic[i].size,
                                        PROT_READ | PROT_WRITE /* required */,
                                        MAP_SHARED /* recommended */,
                                        mSnapShotFd,
                                        picture.buf.m.offset);

                if (MAP_FAILED == mem_pic[i].addr)
                    ALOGE("mmap failed, errno=%d\n", errno);
                else
                    ALOGE("%s:addr[%d]=%p  \n", __FUNCTION__,i,mem_pic[i].addr);

                int dma_fd = -1;
                int ret = export_dmabuf_fd(mSnapShotFd,i, &dma_fd);
                if (ret) {
                    ALOGE("export dma fd failed,%s\n", strerror(errno));
                } else {
                    mem_pic[i].dma_fd = dma_fd;
                }
        }

        //step 4 : QBUF
                ////////////////////////////////
        for (i = 0; i < (int)picture.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(mSnapShotFd, VIDIOC_QBUF, &buf) < 0)
                        ALOGE("VIDIOC_QBUF failed, errno=%d\n", errno);
        }

        //step 5: Stream ON
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapShotFd, VIDIOC_STREAMON, &type) < 0)
                ALOGE("VIDIOC_STREAMON, errno=%d\n", errno);

        mIsPicture = true;
        ALOGE("%s: OK",__FUNCTION__);
        return 0;
}

void* VideoInfoUsePictureScaler::get_picture()
{
    ALOGD("%s:get picture\n",__FUNCTION__);
    if (mSnapShotFd < 0) {
        ALOGE("camera not be init!");
        return nullptr;
    }
    int pollret;
    const int POLL_TIMEOUT = 2000;
    struct pollfd pfds[1];
    pfds[0].fd = mSnapShotFd;
    pfds[0].events = POLLIN;
    pfds[0].revents = 0;
    CLEAR(picture.buf);
    picture.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    picture.buf.memory = V4L2_MEMORY_MMAP;

    pollret = poll(pfds, 1, POLL_TIMEOUT);
    if (pollret <= 0) {
        ALOGE("%s Error: poll error %d\n",__func__,pollret);
        return nullptr;
    }

    if (ioctl(mSnapShotFd, VIDIOC_DQBUF, &picture.buf) < 0) {
        switch (errno) {
            case EAGAIN:
                //ALOGE("%s:%s",__FUNCTION__,strerror(errno));
                return nullptr;

            case EIO:
            /* Could ignore EIO, see spec. */

            /* fall through */

            default:
                ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                if (errno == ENODEV) {
                    ALOGE("camera device is not exist!");
                    set_device_status();
                    close(mSnapShotFd);
                    mSnapShotFd = -1;
                }
                return nullptr;
        }
    }
    ALOGD("%s:index=%d",__FUNCTION__,picture.buf.index);
    return mem_pic[picture.buf.index].addr;
}

void VideoInfoUsePictureScaler::stop_picture()
{
        enum v4l2_buf_type type;
        int i;
        int ret;
        ALOGE("%s Enter",__FUNCTION__);
        if (mSnapShotFd < 0) {
                ALOGE("camera not be init!");
                return ;
            }

        if (!mIsPicture)
                return ;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapShotFd, VIDIOC_STREAMOFF, &type) < 0)
        DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

        //stream off and unmap buffer

        for (i = 0; i < (int)picture.rb.count; i++)
        {
            if (munmap(mem_pic[i].addr, mem_pic[i].size) < 0)
                DBG_LOGB("munmap failed errno=%d", errno);
            else {
                if (mem_pic[i].dma_fd != -1) {
                    close(mem_pic[i].dma_fd);
                    mem_pic[i].dma_fd = -1;
                 }
            }
        }
        picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;
        picture.rb.count = 0;

        ret = ioctl(mSnapShotFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0) {
            DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
        } else {
            DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        mIsPicture = false;
        ALOGE("%s: exit",__FUNCTION__);
}

void VideoInfoUsePictureScaler::releasebuf_and_stop_picture()
{
#if 0
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        int i,ret;
        if (mSnapShotFd < 0) {
            ALOGE("camera not be init!");
            return;
        }

        if (!mIsPicture)
                return ;

        //QBUF
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = picture.buf.index;
        if (ioctl(mSnapShotFd, VIDIOC_QBUF, &buf) < 0)
            DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);

        //stream off and unmap buffer
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mSnapShotFd, VIDIOC_STREAMOFF, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

        for (i = 0; i < (int)picture.rb.count; i++)
        {
            if (munmap(mem_pic[i].addr, mem_pic[i].size) < 0)
                DBG_LOGB("munmap failed errno=%d", errno);
        }

        mIsPicture = false;

        picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;
        picture.rb.count = 0;
        ret = ioctl(mSnapShotFd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0) {
          DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
          //return ret;
        }else{
          DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        setBuffersFormat();
        start_capturing();
#endif
}

#if 0
void VideoInfoUsePictureScaler::setMuteKeyStatus(KeyEvent muteKeyStatus){
    mMuteKeyStatus = muteKeyStatus;
}
#endif

int VideoInfoUsePictureScaler::get_frame_buffer(struct VideoInfoBuffer* b)
{
        if (mPreviewFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
            /* poll variables */
        int pollret = 0,poll_retry_counter = 0;
        const int POLL_TIMEOUT = 100;//
        const int POLL_TRY_TIMES = 20;//max poll time = POLL_TIMEOUT * POLL_TRY_TIMES
        struct pollfd pfds[1];
        pfds[0].fd = mPreviewFd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        while (pollret == 0 && isStreaming == true/* && mMuteKeyStatus != EVENT_CAMERA_MUTE */
                && poll_retry_counter < POLL_TRY_TIMES) {
            pollret = poll(pfds, 1, POLL_TIMEOUT);
            poll_retry_counter ++;
        }
        if (pollret <= 0) {
            ALOGE("%s Error: poll preview error %d poll_retry_counter:%d\n",__func__,pollret,poll_retry_counter);
            return -1;
        }

        CLEAR(preview.buf);
        preview.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(mPreviewFd, VIDIOC_DQBUF, &preview.buf) < 0) {
            switch (errno) {
                case EAGAIN:
                    return -1;

                case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

                default:
                    ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                    if (errno == ENODEV) {
                        ALOGE("camera device is not exist!");
                        set_device_status();
                        close(mPreviewFd);
                        mPreviewFd = -1;
                    }
                    return -1;
            }
        }

        CAMHAL_LOGDB("%s: index=%d,dma_fd=%d\n",__FUNCTION__,
                    preview.buf.index, mem[preview.buf.index].dma_fd);
        b->addr = mem[preview.buf.index].addr;
        b->size = mem[preview.buf.index].size;
        b->dma_fd = mem[preview.buf.index].dma_fd;

        return 0;
}

int VideoInfoUsePictureScaler::get_record_buffer(struct VideoInfoBuffer* b)
{
        if (mRecordFd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
            /* poll variables */
        int pollret = 0,poll_retry_counter = 0;
        const int POLL_TIMEOUT = 100;//
        const int POLL_TRY_TIMES = 20;//max poll time = POLL_TIMEOUT * POLL_TRY_TIMES
        struct pollfd pfds[1];
        pfds[0].fd = mRecordFd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        while (pollret == 0 && mIsRecording == true/* && mMuteKeyStatus != EVENT_CAMERA_MUTE */
                && poll_retry_counter < POLL_TRY_TIMES) {
            pollret = poll(pfds, 1, POLL_TIMEOUT);
            poll_retry_counter ++;
        }
        if (pollret <= 0) {
            ALOGE("%s Error: poll preview error %d poll_retry_counter:%d\n",__func__,pollret,poll_retry_counter);
            return -1;
        }

        CLEAR(record.buf);
        record.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        record.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(mRecordFd, VIDIOC_DQBUF, &record.buf) < 0) {
            switch (errno) {
                case EAGAIN:
                    return -1;

                case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

                default:
                    ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                    if (errno == ENODEV) {
                        ALOGE("camera device is not exist!");
                        set_device_status();
                        close(mRecordFd);
                        mRecordFd = -1;
                    }
                    return -1;
            }
        }

        CAMHAL_LOGDB("%s: index=%d,dma_fd=%d\n",__FUNCTION__,
                    record.buf.index, mem_rec[record.buf.index].dma_fd);
        b->addr = mem_rec[record.buf.index].addr;
        b->size = mem_rec[record.buf.index].size;
        b->dma_fd = mem_rec[record.buf.index].dma_fd;

        return 0;
}

//----get dmabuf file descriptor according to index
int VideoInfoUsePictureScaler::export_dmabuf_fd(int v4lfd, int index, int* dmafd)
{
    struct v4l2_exportbuffer expbuf;
    memset(&expbuf,0,sizeof(expbuf));
    expbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    expbuf.index = index;
    expbuf.flags = 0;
    expbuf.fd = -1;
    if (ioctl(v4lfd,VIDIOC_EXPBUF,&expbuf) == -1) {
        ALOGE("export buffer fail:%s",strerror(errno));
        return -1;
    } else {
        ALOGD("dma buffer fd = %d \n",expbuf.fd);
        *dmafd = expbuf.fd;
    }
    return 0;
}

int VideoInfoUsePictureScaler::get_picture_buffer(struct VideoInfoBuffer* b)
{
     if (mSnapShotFd < 0) {
         ALOGE("camera not be init!");
         return -1;
     }

     CLEAR(picture.buf);
     picture.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
     picture.buf.memory = V4L2_MEMORY_MMAP;

     for (int i = 0; i < 1; i++) {
        if (ioctl(mSnapShotFd, VIDIOC_DQBUF, &picture.buf) < 0) {
             switch (errno) {
                 case EAGAIN:
                     ALOGE("%s:%s",__FUNCTION__,strerror(errno));
                     return -1;

                 case EIO:
                 /* Could ignore EIO, see spec. */
                 /* fall through */
                 default:
                     ALOGE("VIDIOC_DQBUF failed, %s\n", strerror(errno));
                     if (errno == ENODEV) {
                         ALOGE("camera device is not exist!");
                         set_device_status();
                         close(mSnapShotFd);
                         mSnapShotFd = -1;
                     }
                     return -1;
             }
        }
    }
    CAMHAL_LOGDB("%s:index=%d",__FUNCTION__,picture.buf.index);
    //dump(mem_pic[picture.buf.index].addr,mem_pic[picture.buf.index].size);
    b->addr = mem_pic[picture.buf.index].addr;
    b->size = mem_pic[picture.buf.index].size;
    b->dma_fd = mem_pic[picture.buf.index].dma_fd;
    return 0;
}


int VideoInfoUsePictureScaler::dump(void* addr, int size) {
        ALOGE("%s:E",__FUNCTION__);
        const char* path="/data/vendor/picture.dat";

        int fd = open(path,O_RDWR);
        if (fd < 0) {
            ALOGE("%s:open file error",__FUNCTION__);
            return -1;
        }

        int ret = write(fd,addr,size);
        if (ret < 0) {
            ALOGE("%s:write data error",__FUNCTION__);
            return -1;
        }

        fsync(fd);
        close(fd);
        return 0;
}

}
