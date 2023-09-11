/* Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: C++ file
 */

#define LOG_TAG "hdmicecd"

#include <log/log.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <cutils/uevent.h>
#include "EArcObserver.h"

namespace android {

EArcObserver::EArcObserver(std::shared_ptr<EArcListener> listener) {
    mEArcListener = listener;

    //uevent
    mUeventFd = uevent_open_socket(64 * 1024, true);
    if (mUeventFd < 0) {
        ALOGE("uevent_open_socket failed.");
        return;
    }
    if (fcntl(mUeventFd, F_SETFL, O_NONBLOCK) == -1) {
        ALOGE("fcntl mUeventFd failed.");
        return;
    }

    //epoll
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (mEpollFd < 0) {
        ALOGE("epoll_create failed.");
        return;
    }
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = EPOLLIN;
    ev.data.fd = mUeventFd;
    if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, mUeventFd, &ev) == -1) {
        ALOGE("epoll_ctl failed.");
        return;
    }
}

EArcObserver::~EArcObserver() {
    ALOGD("~EArcObserver");
    close(mUeventFd);
    close(mEpollFd);
}

bool EArcObserver::threadLoop() {
    ALOGI("EArc observation thread start.");
    while (!exitPending()) {
        int eventNum = epoll_wait(mEpollFd, mPendingEventItems, EPOLL_MAX_EVENTS, NO_TIMEOUT);
        if (eventNum <= 0) {
            ALOGE("epoll_wait fails.");
            continue;
        }
        for (int i = 0; i < eventNum; i++) {
            if (mPendingEventItems[i].events & EPOLLIN) {
                processUevent();
            }
        }
    }
    ALOGI("EArcObserver exit.");
    return false;
}

void EArcObserver::processUevent() {
    char msg[UEVENT_MSG_LEN + 2];
    char* cp;
    int n;

    n = uevent_kernel_multicast_recv(mUeventFd, msg, UEVENT_MSG_LEN);
    if (n <= 0) return;
    if (n >= UEVENT_MSG_LEN) /* overflow -- discard */
        return;

    msg[n] = '\0';
    msg[n + 1] = '\0';
    cp = msg;

    while (*cp) {
        if (strstr(cp, STR_UEVENT_EARC_STATE_UNKNOWN)) {
            ALOGD("receive uevent earc state %s", cp);
            if (mEArcListener != NULL) {
                mEArcListener->onEArcEvent(EARC_STATE_UNKNOWN);
            }
            break;
        } else if (!strcmp(cp, STR_UEVENT_EARC_STATE_ARC)) {
            ALOGD("receive uevent earc state %s", cp);
            if (mEArcListener != NULL) {
                mEArcListener->onEArcEvent(EARC_STATE_ARC);
            }
            break;
        } else if (!strcmp(cp, STR_UEVENT_EARC_STATE_EARC)) {
            ALOGD("receive uevent earc state %s", cp);
            if (mEArcListener != NULL) {
                mEArcListener->onEArcEvent(EARC_STATE_EARC);
            }
            break;
        }
        /* advance to after the next \0 */
        while (*cp++);
    }
}

} // namespace android
