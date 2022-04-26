#ifndef __MIPI_BASE_IO_3_H__
#define __MIPI_BASE_IO_3_H__
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
//#include "KeyEvent.h"

#define NUM_PICTURE_BUFFER (4)
#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define V4L2_ROTATE_ID 0x980922  //V4L2_CID_ROTATE

namespace android {
class VideoInfoUsePictureScaler {
    public:
        VideoInfoUsePictureScaler();
        virtual ~VideoInfoUsePictureScaler(){};
        struct    v4l2_capability cap;
        FrameV4L2Info preview;
        FrameV4L2Info record;
        FrameV4L2Info picture;
        FrameV4L2Info picture_config;
        std::vector<struct VideoInfoBuffer> mem;
	std::vector<struct VideoInfoBuffer> mem_rec;
        struct VideoInfoBuffer mem_pic[NUM_PICTURE_BUFFER];
        //unsigned int canvas[IO_PREVIEW_BUFFER];
        bool isStreaming;
        bool mIsRecording;
        bool mIsPicture;
        bool canvas_mode;
        //KeyEvent mMuteKeyStatus;//mute/unmute
        int width;
        int height;
        int formatIn;
        int framesizeIn;
        uint32_t idVendor;
        uint32_t idProduct;

        int idx;
        int mPreviewFd;
        int mRecordFd;
        int mSnapShotFd;

        int tempbuflen;
        int dev_status;
        char sensor_type[64];
        const int POLL_TIMEOUT = 2000;
    public:
        int camera_init(void);
        int setBuffersFormat(void);
        int start_capturing(void);
        int start_recording(void);
        int start_picture(int rotate);
        void stop_picture();
        void releasebuf_and_stop_picture();
        int stop_capturing();
        int stop_recording(void);
        int releasebuf_and_stop_capturing();
        uintptr_t get_frame_phys();
        void set_device_status();
        int get_device_status();
        void *get_frame();
        void *get_record_frame();
        void *get_picture();
        int putback_frame();
        int putback_record_frame();
        int putback_picture_frame();
        int EnumerateFormat(uint32_t pixelformat);
        bool IsSupportRotation();
        void set_buffer_numbers(int io_buffer);
        int get_frame_buffer(struct VideoInfoBuffer* b);
        int get_record_buffer(struct VideoInfoBuffer* b);
        int export_dmabuf_fd(int v4lfd, int index, int* dmafd);
        int get_picture_buffer(struct VideoInfoBuffer* b);
        //void setMuteKeyStatus(KeyEvent muteKeyStatus);
    private:
        int set_rotate(int camera_fd, int value);
        int IO_PREVIEW_BUFFER;
        int dump(void* addr, int size);
};
}



#endif
