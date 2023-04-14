/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: c++ file
 */

#define LOG_TAG "HDMIStatus"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <errno.h>
#include <utils/Mutex.h>

#include <log/log.h>
#include <cutils/properties.h>
#include <HDMIStatus.h>
#include "EmulatedCameraFactory.h"


namespace android {
HDMIStatus* HDMIStatus::mInstance = nullptr;
Mutex HDMIStatus::mLock;
int HDMIStatus::m_hdmi_fd = -1;

HDMIStatus::HDMIStatus() {
    m_hdmi_fd = open(HDMI_DETECT_PATH, O_RDWR);
    if (m_hdmi_fd < 0 )
        ALOGW("open file(%s) fail: %s", HDMI_DETECT_PATH, strerror(errno));
    threadRunning = false;
}

HDMIStatus::~HDMIStatus() {
    if (mHDMIHotplugThread) {
        if (threadRunning) {
            threadRunning = false;
            mHDMIHotplugThread->join();
        }
        mHDMIHotplugThread.clear();
        mHDMIHotplugThread = nullptr;
    }
}

HDMIStatus* HDMIStatus::getInstance() {
    ALOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&mLock);
    if (mInstance != nullptr)
        return mInstance;
    ALOGD("%s: create new ion object \n", __FUNCTION__);
    mInstance = new HDMIStatus;
    return mInstance;
}

void HDMIStatus::putInstance() {
    ALOGD("%s\n", __FUNCTION__);
    Mutex::Autolock lock(&mLock);
    if (mInstance != nullptr) {
        delete mInstance;
        mInstance = nullptr;
    }
}

int HDMIStatus::readHdmiStatus() {
    int status = -1;
    if (m_hdmi_fd > 0) {
        read(m_hdmi_fd, (void *)(&status), sizeof(int));
        if (status < 0)
            status = 0;
    }
    return status;
}

plug_status_e HDMIStatus::getHdmiStatus() {
    return readHdmiStatus() > 0 ? HDMI_PLUG_IN : HDMI_PLUG_OUT;
}

bool HDMIStatus::isStandardHDMICamera() {
    char property[PROPERTY_VALUE_MAX];
    property_get("vendor.media.hdmi.vdin.enable", property, "false");
    if (strstr(property, "false"))
        return false;
    return (getHdmiStatus() == HDMI_PLUG_IN);
}

int HDMIStatus::getHdmiPlugStatus(int old_status, int new_status, int port) {
    int plug = -1;
    if ((new_status & port) != (old_status  & port) ) {
        if ((new_status & port) == port) {
            plug = 1;
        } else {
            plug = 0;
        }
    }
    return plug;
}

void HDMIStatus::startDetectStatus() {
    mHDMIHotplugThread = new HDMIHotplugThread(this);
    mHDMIHotplugThread->run("");
    threadRunning = true;
}

HDMIStatus::HDMIHotplugThread::HDMIHotplugThread(HDMIStatus* status_instance)         :
        Thread(false), mParent(status_instance) {
    epoll_fd = ::epoll_create(30);
    if (epoll_fd > 0) {
        backEvents = new epoll_event[20];
    }

    if (mParent->m_hdmi_fd < 0) {
        ALOGW("invalid hdmirx0 fd");
    } else {
        epoll_event event;
        event.data.fd = mParent->m_hdmi_fd;
        event.events = EPOLLIN | EPOLLET;
        ::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, mParent->m_hdmi_fd, &event);
        char property[256];
        property_get("vendor.media.hdmi.vdin.port", property, "1");
        int detect_index = atoi(property) - 1;
        if (detect_index < 0 || detect_index > 2) {
            ALOGE("invalid index set default detect bit rx1");
            hdmi_detect_bit = DETECT_BITS[0];
        } else {
            hdmi_detect_bit = DETECT_BITS[detect_index];
        }
        m_hdmi_status = mParent->readHdmiStatus();
    }
}

HDMIStatus::HDMIHotplugThread::~HDMIHotplugThread() {
    if (epoll_fd > 0) {
        close(epoll_fd);
        epoll_fd = -1;
    }
    if (backEvents) {
        delete[] backEvents;
        backEvents = nullptr;
    }
}

void HDMIStatus::HDMIHotplugThread::requestExit() {

    ALOGV("%s: Requesting thread exit", __FUNCTION__);
}

status_t HDMIStatus::HDMIHotplugThread::requestExitAndWait() {
    ALOGE("%s: Not implemented. Use requestExit + join instead",
          __FUNCTION__);
    return INVALID_OPERATION;
}


status_t HDMIStatus::HDMIHotplugThread::readyToRun() {
    return OK;
}

bool HDMIStatus::HDMIHotplugThread::threadLoop() {
    if (exitPending())
        return false;
    int num = ::epoll_wait(epoll_fd, backEvents, 20, -1);
    ALOGE("epoll wait %d fds", num);
    for (int i = 0; i < num; ++i) {
        int fd = backEvents[i].data.fd;
        if (backEvents[i].events & EPOLLIN) {
            if (fd == mParent->m_hdmi_fd) {
                int hdmi_status = mParent->readHdmiStatus();
                int plug = mParent->getHdmiPlugStatus(m_hdmi_status, hdmi_status, hdmi_detect_bit);
                m_hdmi_status = hdmi_status;
                if (plug >= 0)
                    gEmulatedCameraFactory.onStatusChanged(HDMI_VDIN_DEV_BEGIN_NUM, plug);
            }
        }
    }
    return true;
}

}
