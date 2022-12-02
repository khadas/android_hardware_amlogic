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

#ifndef __HW_MIPI_BASE_IO2_H__
#define __HW_MIPI_BASE_IO2_H__
#include <linux/videodev2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <stdbool.h>

#include <linux/videodev2.h>
#include <DebugUtils.h>
#include <vector>
#include "CameraIO.h"

#define NUM_PICTURE_BUFFER (4)
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define V4L2_ROTATE_ID 0x980922  //V4L2_CID_ROTATE

namespace android {
class VideoInfoUseTowFd {
    public:
        VideoInfoUseTowFd();
        virtual ~VideoInfoUseTowFd(){};
        struct    v4l2_capability cap;
        FrameV4L2Info preview;
        FrameV4L2Info picture;
        std::vector<struct VideoInfoBuffer> mem;
        struct VideoInfoBuffer mem_pic[NUM_PICTURE_BUFFER];
        //unsigned int canvas[IO_PREVIEW_BUFFER];
        bool isStreaming;
        bool mIsPicture;
        bool canvas_mode;
        int width;
        int height;
        int formatIn;
        int framesizeIn;
        uint32_t idVendor;
        uint32_t idProduct;

        int idx;
        int mPreviewFd;
        int mSnapShotFd;

        int tempbuflen;
        int dev_status;
        char sensor_type[64];
        const int POLL_TIMEOUT = 2000;
    public:
        int camera_init(void);
        int setBuffersFormat(void);
        int start_capturing(void);
        int start_picture(int rotate);
        void stop_picture();
        void releasebuf_and_stop_picture();
        int stop_capturing();
        int releasebuf_and_stop_capturing();
        uintptr_t get_frame_phys();
        void set_device_status();
        int get_device_status();
        void *get_frame();
        void *get_picture();
        int putback_frame();
        int putback_picture_frame();
        int EnumerateFormat(uint32_t pixelformat);
        bool IsSupportRotation();
        void set_buffer_numbers(int io_buffer);
        int get_frame_buffer(struct VideoInfoBuffer* b);
        int export_dmabuf_fd(int v4lfd, int index, int* dmafd);
    private:
        int set_rotate(int camera_fd, int value);
        int IO_PREVIEW_BUFFER;
};
}
#endif

