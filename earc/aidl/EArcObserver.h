/*
 * Copyright (c) 2014 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description: header file
 */

#ifndef EARC_OBSERVER_H
#define EARC_OBSERVER_H

#include <utils/Thread.h>
#include <utils/StrongPointer.h>
#include <sys/epoll.h>

namespace android {

#define  EPOLL_MAX_EVENTS  16
#define  INPUT_MAX_EVENTS  128
#define  NO_TIMEOUT  -1

#define UEVENT_MSG_LEN 2048

#define STR_UEVENT_EARC_STATE_UNKNOWN   "EARCTX_ARC_STATE=0"
#define STR_UEVENT_EARC_STATE_ARC       "EARCTX_ARC_STATE=1"
#define STR_UEVENT_EARC_STATE_EARC      "EARCTX_ARC_STATE=2"

#define EARC_STATE_UNKNOWN       0
#define EARC_STATE_ARC           1
#define EARC_STATE_EARC          2

class EArcListener: virtual public RefBase {
public:
    /**
     * eArcState
     * 0:UEVENT_EARC_STATE_UNKNOWN
     * 1:UEVENT_EARC_STATE_ARC
     * 2:UEVENT_EARC_STATE_EARC
     */
    virtual void onEArcEvent(int eArcState) = 0;
};

class EArcObserver: public Thread {
public:
    EArcObserver(std::shared_ptr<EArcListener> listener);
    virtual ~EArcObserver();

private:
    bool threadLoop();

    void processUevent();

    // The array of pending epoll events and the index of the next event to be handled.
    struct epoll_event mPendingEventItems[EPOLL_MAX_EVENTS];

    std::shared_ptr<EArcListener> mEArcListener;

    int mEpollFd;
    int mUeventFd;;

};
} //namespace android
#endif //EARC_OBSERVER_H
