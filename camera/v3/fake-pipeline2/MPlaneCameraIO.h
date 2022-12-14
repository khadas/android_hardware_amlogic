#ifndef __MPLANE_CAMERA_IO_H__
#define __MPLANE_CAMERA_IO_H__

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

#include <DebugUtils.h>
#include <vector>
#include "CameraDevice.h"
#include "IonIf.h"

#define CLEAR(x) memset(&(x), 0, sizeof(x))
#define MAX_V4L2_BUFFER_COUNT 5

namespace android {

    typedef struct PlaneBuffers {
        struct v4l2_plane v4lplane[VIDEO_MAX_PLANES];
    } PlaneBuffers;
    typedef struct VideoInfo {
        void* addr;
        int dma_fd;
        unsigned int buf_idx;
    } VideoInfo;

    class MPlaneCameraIO {
        public:
            MPlaneCameraIO();
            ~MPlaneCameraIO();
            int openCamera();
            void closeCamera();
            int startCameraIO();
            int stopCameraIO();
            int setInputPort(int* port_index);
            int setOutputFormat();
            int getFrame(VideoInfo& info);
            int pushbackFrame(unsigned int index);
        public:
            int fd;
            int openIdx;
            struct v4l2_capability cap;
            struct v4l2_format format;
            struct v4l2_requestbuffers rb;
            struct v4l2_buffer buf;
        private:
            bool isStreaming;
            PlaneBuffers* pPlaneBuffers[MAX_V4L2_BUFFER_COUNT] = {NULL,NULL,NULL,NULL,NULL};
            void* addr[MAX_V4L2_BUFFER_COUNT];
            int dma_fd[MAX_V4L2_BUFFER_COUNT] = {-1,-1,-1,-1,-1};
            IONInterface* mIon = NULL;
            uint32_t buffer_size_allocated;
            void allocatePlaneBuffers();
            void freePlaneBuffers();
    };

}

#endif
