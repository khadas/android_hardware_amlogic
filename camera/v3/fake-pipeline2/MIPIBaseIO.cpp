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

#ifndef __CAMERA_IO__
#define __CAMERA_IO__

//#define LOG_NDEBUG 0
#define LOG_TAG "MIPI_Base_IO"

#include <errno.h>
#include <cutils/properties.h>
#include "MIPIBaseIO.h"
#include <poll.h>

namespace android {
VideoInfoUseOneFd::VideoInfoUseOneFd(){
    memset(&cap, 0, sizeof(struct v4l2_capability));
    memset(&preview,0,sizeof(FrameV4L2Info));
    memset(&picture,0,sizeof(FrameV4L2Info));
    //memset(mem,0,sizeof(mem));
    memset(mem_pic,0,sizeof(mem_pic));
    //memset(canvas,0,sizeof(canvas));
    isStreaming = false;
    isPicture = false;
    //canvas_mode = false;
    width = 0;
    height = 0;
    formatIn = 0;
    framesizeIn = 0;
    idVendor = 0;
    idProduct = 0;
    idx = 0;
    V4LDevicefd = -1;
    tempbuflen = 0;
    dev_status = 0;
}

VideoInfoUseOneFd::~VideoInfoUseOneFd() {
    for (uint32_t i = 0; i < mem.size(); i++) {
      if (mem[i].dma_fd != -1)
          close(mem[i].dma_fd);
    }
}

//----get dmabuf file descriptor according to index
int VideoInfoUseOneFd::export_dmabuf_fd(int v4lfd, int index, int* dmafd)
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

int VideoInfoUseOneFd::EnumerateFormat(uint32_t pixelformat) {
    struct v4l2_fmtdesc fmt;
    int ret;
    if (V4LDevicefd < 0) {
        ALOGE("camera not be init!");
        return -1;
    }
    memset(&fmt,0,sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.index = 0;
    while ((ret = ioctl(V4LDevicefd, VIDIOC_ENUM_FMT, &fmt)) == 0) {
        if (fmt.pixelformat == pixelformat)
            return pixelformat;
        fmt.index++;
    }
    return 0;
}

bool VideoInfoUseOneFd::IsSupportRotation() {
    struct v4l2_queryctrl qc;
    int ret = 0;
    bool Support = false;
    memset(&qc, 0, sizeof(struct v4l2_queryctrl));
    qc.id = V4L2_ROTATE_ID;
    ret = ioctl (V4LDevicefd, VIDIOC_QUERYCTRL, &qc);
    if ((qc.flags == V4L2_CTRL_FLAG_DISABLED) ||( ret < 0)
        || (qc.type != V4L2_CTRL_TYPE_INTEGER)) {
            Support = false;
    }else{
            Support = true;
    }
    return Support;
}

int VideoInfoUseOneFd::set_rotate(int camera_fd, int value)
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

void VideoInfoUseOneFd::set_device_status(void)
{
    dev_status = -1;
}

int VideoInfoUseOneFd::get_device_status(void)
{
    return dev_status;
}

int VideoInfoUseOneFd::camera_init(void)
{
    ALOGV("%s: E", __FUNCTION__);
    int ret =0 ;
    if (V4LDevicefd < 0) {
          ALOGE("open /dev/video%d failed, errno=%s\n", idx, strerror(errno));
          return -ENOTTY;
    }

    ret = ioctl(V4LDevicefd, VIDIOC_QUERYCAP, &cap);
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


int VideoInfoUseOneFd::setBuffersFormat(void)
{
    int ret = 0;
    if (V4LDevicefd < 0) {
        ALOGE("camera not be init!");
        return -1;
    }
    if ((preview.format.fmt.pix.width != 0) && (preview.format.fmt.pix.height != 0)) {
    int pixelformat = preview.format.fmt.pix.pixelformat;

    ret = ioctl(V4LDevicefd, VIDIOC_S_FMT, &preview.format);
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

void VideoInfoUseOneFd::set_buffer_numbers(int io_buffer) {
    IO_PREVIEW_BUFFER = io_buffer;
}

int VideoInfoUseOneFd::start_capturing(void)
{
        ALOGD("%s: \n",__FUNCTION__);
        int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;

        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (isStreaming)
            ALOGD("already stream on\n");

        //----allocate memory
        CLEAR(preview.rb);
        mem.resize(IO_PREVIEW_BUFFER);
        preview.rb.count = IO_PREVIEW_BUFFER;
        preview.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
            DBG_LOGB("camera idx:%d does not support "
                      "memory mapping, errno=%d\n", idx, errno);
        }
        if (preview.rb.count < 2) {
            DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                            idx, errno);
            return -EINVAL;
        }

        //----map memory to user space
        for (i = 0; i < (int)preview.rb.count; ++i) {
            CLEAR(preview.buf);
            preview.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            preview.buf.memory      = V4L2_MEMORY_MMAP;
            preview.buf.index       = i;

            if (ioctl(V4LDevicefd, VIDIOC_QUERYBUF, &preview.buf) < 0) {
                    DBG_LOGB("VIDIOC_QUERYBUF, errno=%d", errno);
            }

            mem[i].size = preview.buf.length;
            mem[i].addr = mmap(NULL, // start anywhere
                                mem[i].size,
                                PROT_READ | PROT_WRITE, // required
                                MAP_SHARED, // recommended
                                V4LDevicefd,        //video device fd
                                preview.buf.m.offset);

            if (MAP_FAILED == mem[i].addr) {
                ALOGE("mmap failed,%s\n", strerror(errno));
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(V4LDevicefd,i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem[i].dma_fd = dma_fd;
                ALOGD("index = %d, dma_fd = %d \n",i,mem[i].dma_fd);
            }
        }
        //----queue buffer to driver's video buffer queue
        for (i = 0; i < (int)preview.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(V4LDevicefd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }
        //----stream on----
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
             if (ioctl(V4LDevicefd, VIDIOC_STREAMON, &type) < 0)
                  DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        }

        isStreaming = true;
        return 0;
}

int VideoInfoUseOneFd::stop_capturing(void)
{
       ALOGD("%s: \n",__FUNCTION__);
       enum v4l2_buf_type type;
        int res = 0;
        int i;
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (!isStreaming) {
                ALOGE("error:camera already stopped!");
                return -1;
        }

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(V4LDevicefd, VIDIOC_STREAMOFF, &type) < 0) {
                ALOGE("VIDIOC_STREAMOFF, errno=%d", errno);
                res = -1;
        }

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }

        for (i = 0; i < (int)preview.rb.count; ++i) {

                if (munmap(mem[i].addr, mem[i].size) < 0) {
                        ALOGE("munmap failed errno=%d", errno);
                        res = -1;
                }

                if (mem[i].dma_fd != -1 && !res) {
                    close(mem[i].dma_fd);
                    mem[i].dma_fd = -1;
                }
        }

        //if (strstr((const char *)cap.driver, "ARM-camera-isp")) {
            preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            preview.rb.memory = V4L2_MEMORY_MMAP;
            preview.rb.count = 0;

            res = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &preview.rb);
            if (res < 0) {
                ALOGE("VIDIOC_REQBUFS failed: %s", strerror(errno));
            }else{
                DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
            }
        //}

        isStreaming = false;
        return res;
}

int VideoInfoUseOneFd::releasebuf_and_stop_capturing(void)
{
        enum v4l2_buf_type type;
        int res = 0 ,ret;
        int i;

        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (!isStreaming)
                return -1;

        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if ((preview.format.fmt.pix.width != 0) &&
               (preview.format.fmt.pix.height != 0)) {
            if (ioctl(V4LDevicefd, VIDIOC_STREAMOFF, &type) < 0) {
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

                if (mem[i].dma_fd != -1 && !res) {
                    close(mem[i].dma_fd);
                    mem[i].dma_fd = -1;
                }
        }
        isStreaming = false;

        preview.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.rb.memory = V4L2_MEMORY_MMAP;
        preview.rb.count = 0;

        ret = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &preview.rb);
        if (ret < 0) {
           DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
           //return ret;
        }else{
           DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        return res;
}


uintptr_t VideoInfoUseOneFd::get_frame_phys(void)
{
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        CLEAR(preview.buf);

        preview.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        preview.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(V4LDevicefd, VIDIOC_DQBUF, &preview.buf) < 0) {
                switch (errno) {
                        case EAGAIN:
                                return 0;
                        case EIO:
                        default:
                                DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
                                exit(1);
                }
        DBG_LOGB("VIDIOC_DQBUF failed, errno=%d\n", errno);
        }

        return (uintptr_t)preview.buf.m.userptr;
}

int VideoInfoUseOneFd::get_frame_index(FrameV4L2Info& info) {
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        struct pollfd pfds[1];
        int pollret;

        pfds[0].fd = V4LDevicefd;
        pfds[0].events = POLLIN;
        pfds[0].revents = 0;
        pollret = poll(pfds, 1, POLL_TIMEOUT);
        if (pollret == 0) {
            ALOGE ("%s:poll timeout.\n",__FUNCTION__);
            return -1;
        } else if (pollret < 0) {
            ALOGE ("Error: poll error\n");
            return -1;
        }
        CLEAR(info.buf);
        info.buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        info.buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(V4LDevicefd, VIDIOC_DQBUF, &info.buf) < 0) {
            switch (errno) {
                case EAGAIN:
                    return -1;

                case EIO:
                /* Could ignore EIO, see spec. */

                /* fall through */

                default:
                    CAMHAL_LOGDB("VIDIOC_DQBUF failed, errno=%d\n", errno); //CAMHAL_LOGDB
                    //exit(1); /*here will generate crash, so delete.  when ocour error, should break while() loop*/
                    if (errno == ENODEV) {
                        ALOGE("camera device is not exist!");
                        set_device_status();
                        close(V4LDevicefd);
                        V4LDevicefd = -1;
                    }
                    return -1;
            }
        }
        return info.buf.index;
}

int VideoInfoUseOneFd::get_frame_buffer(struct VideoInfoBuffer* b)
{
    int index = get_frame_index(preview);
    ALOGD("%s:index = %d \n",__FUNCTION__,index);
    if (index < 0)
        return -1;
    else {
        index = index % IO_PREVIEW_BUFFER;
        ALOGD("%s: index=%d,dma_fd=%d\n",__FUNCTION__,index,mem[index].dma_fd);
        b->addr = mem[index].addr;
        b->size = mem[index].size;
        b->dma_fd = mem[index].dma_fd;
    }
    return 0;
}

void* VideoInfoUseOneFd::get_frame()
{
    //DBG_LOGA("get frame\n");
    int index = get_frame_index(preview);
    if (index < 0)
        return nullptr;
    return mem[index].addr;
}

int VideoInfoUseOneFd::putback_frame()
{
        if (dev_status == -1)
            return 0;

        if (!preview.buf.length) {
            preview.buf.length = tempbuflen;
        }
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        if (ioctl(V4LDevicefd, VIDIOC_QBUF, &preview.buf) < 0) {
            DBG_LOGB("QBUF failed :%s\n", strerror(errno));
            if (errno == ENODEV) {
                set_device_status();
            }
        }

        return 0;
}

int VideoInfoUseOneFd::putback_picture_frame()
{
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }
        if (ioctl(V4LDevicefd, VIDIOC_QBUF, &picture.buf) < 0)
                DBG_LOGB("QBUF failed error=%d\n", errno);

        return 0;
}

int VideoInfoUseOneFd::start_picture(int rotate)
{
          int ret = 0;
        int i;
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        bool usbcamera = false;

        CLEAR(picture.rb);
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return -1;
        }

        //step 1 : ioctl  VIDIOC_S_FMT
        for (int i = 0; i < 3; i++) {
            ret = ioctl(V4LDevicefd, VIDIOC_S_FMT, &picture.format);
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
        picture.rb.count = IO_PICTURE_BUFFER;
        picture.rb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;

        ret = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0 ) {
                DBG_LOGB("camera idx:%d does not support "
                                "memory mapping, errno=%d\n", idx, errno);
        }

        if (picture.rb.count < 1) {
                DBG_LOGB( "Insufficient buffer memory on /dev/video%d, errno=%d\n",
                                idx, errno);
                return -EINVAL;
        }

        //step 3: mmap buffer
        for (i = 0; i < (int)picture.rb.count; ++i) {
            CLEAR(picture.buf);
            picture.buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            picture.buf.memory      = V4L2_MEMORY_MMAP;
            picture.buf.index       = i;

            if (ioctl(V4LDevicefd, VIDIOC_QUERYBUF, &picture.buf) < 0) {
                    ALOGE("VIDIOC_QUERYBUF, errno=%d", errno);
            }
            mem_pic[i].size = picture.buf.length;
            mem_pic[i].addr = mmap(NULL, // start anywhere
                                    mem_pic[i].size,
                                    PROT_READ | PROT_WRITE, // required
                                    MAP_SHARED, //recommended
                                    V4LDevicefd,
                                    picture.buf.m.offset);

            if (MAP_FAILED == mem_pic[i].addr) {
                    ALOGE("mmap failed, errno=%d\n", errno);
            }
            int dma_fd = -1;
            int ret = export_dmabuf_fd(V4LDevicefd,i, &dma_fd);
            if (ret) {
                ALOGE("export dma fd failed,%s\n", strerror(errno));
            } else {
                mem_pic[i].dma_fd = dma_fd;
            }
        }

        //step 4 : QBUF
        for (i = 0; i < (int)picture.rb.count; ++i) {

                CLEAR(buf);
                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;
                buf.index = i;

                if (ioctl(V4LDevicefd, VIDIOC_QBUF, &buf) < 0)
                        DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);
        }

        if (isPicture) {
                DBG_LOGA("already stream on\n");
        }

        if (strstr((const char *)cap.driver, "uvcvideo")) {
            usbcamera = true;
        }

        //step 5: Stream ON
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(V4LDevicefd, VIDIOC_STREAMON, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMON, errno=%d\n", errno);
        isPicture = true;

        return 0;
}

void* VideoInfoUseOneFd::get_picture()
{
    DBG_LOGA("get picture\n");
    int index = get_frame_index(picture);
    if (index < 0)
        return nullptr;
    return mem_pic[picture.buf.index].addr;
}

int VideoInfoUseOneFd::get_picture_share_fd()
{
    DBG_LOGA("get picture\n");
    int index = get_frame_index(picture);
    if (index < 0)
        return -1;
    return mem_pic[picture.buf.index].dma_fd;
}

void VideoInfoUseOneFd::stop_picture()
{
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        int i;
        int ret;
        if (V4LDevicefd < 0) {
                ALOGE("camera not be init!");
                return ;
            }

        if (!isPicture)
                return ;

        //QBUF
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = picture.buf.index;
        if (ioctl(V4LDevicefd, VIDIOC_QBUF, &buf) < 0)
            DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);

        //stream off and unmap buffer
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(V4LDevicefd, VIDIOC_STREAMOFF, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

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

        //if (strstr((const char *)cap.driver, "ARM-camera-isp")) {
            picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            picture.rb.memory = V4L2_MEMORY_MMAP;
            picture.rb.count = 0;

            ret = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &picture.rb);
            if (ret < 0) {
                DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
            } else {
                DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
            }
        //}

        set_rotate(V4LDevicefd,0);
        isPicture = false;
        setBuffersFormat();
        start_capturing();
}

void VideoInfoUseOneFd::releasebuf_and_stop_picture()
{
        enum v4l2_buf_type type;
        struct  v4l2_buffer buf;
        int i,ret;
        if (V4LDevicefd < 0) {
            ALOGE("camera not be init!");
            return;
        }

        if (!isPicture)
                return ;

        //QBUF
        CLEAR(buf);
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = picture.buf.index;
        if (ioctl(V4LDevicefd, VIDIOC_QBUF, &buf) < 0)
            DBG_LOGB("VIDIOC_QBUF failed, errno=%d\n", errno);

        //stream off and unmap buffer
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(V4LDevicefd, VIDIOC_STREAMOFF, &type) < 0)
                DBG_LOGB("VIDIOC_STREAMOFF, errno=%d", errno);

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

        isPicture = false;

        picture.format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        picture.rb.memory = V4L2_MEMORY_MMAP;
        picture.rb.count = 0;
        ret = ioctl(V4LDevicefd, VIDIOC_REQBUFS, &picture.rb);
        if (ret < 0) {
          DBG_LOGB("VIDIOC_REQBUFS failed: %s", strerror(errno));
          //return ret;
        }else{
          DBG_LOGA("VIDIOC_REQBUFS delete buffer success\n");
        }
        setBuffersFormat();
        start_capturing();
}

}
#endif

