/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef __HDMI_STATUS_H
#define __HDMI_STATUS_H

#include <utils/Mutex.h>
#include <utils/Thread.h>
#include <sys/epoll.h>

namespace android {

static const char *HDMI_DETECT_PATH = "/dev/hdmirx0";
static const char *HDMI_VDIN_VIDEO_PATH = "/dev/video70";
static const int DETECT_BITS[] = {0x01, 0x02, 0x04};

typedef enum plug_status {
    HDMI_PLUG_OUT = 0,
    HDMI_PLUG_IN = 1,
} plug_status_e;
class HDMIStatus {
private:
    static HDMIStatus* mInstance;
    static Mutex mLock;
    static int m_hdmi_fd;
private:
    HDMIStatus();
    ~HDMIStatus();
public:
    plug_status_e getHdmiStatus();
    int readHdmiStatus();
    bool isStandardHDMICamera();
    int getHdmiPlugStatus(int old_status, int new_status, int port);
    void startDetectStatus();
    static HDMIStatus* getInstance();
    static void putInstance();
private:
    class HDMIHotplugThread : public Thread {
      public:
        HDMIHotplugThread(HDMIStatus* parent);
        ~HDMIHotplugThread();
        virtual void requestExit();
        virtual status_t requestExitAndWait();
      private:
        virtual status_t readyToRun();
        virtual bool threadLoop();
        int hdmi_detect_bit;
        int epoll_fd = -1;
        epoll_event *backEvents = nullptr;
        int m_hdmi_status = 0;
        HDMIStatus* mParent;
    };
    sp<HDMIHotplugThread> mHDMIHotplugThread;
    bool threadRunning = false;

};

}
#endif

