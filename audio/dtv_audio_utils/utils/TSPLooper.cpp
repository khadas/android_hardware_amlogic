/*
 * Copyright (C) 2010 The Android Open Source Project
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
 */

//#define LOG_NDEBUG 0
#define LOG_TAG "TSPLooper"

#include "tsp_platform.h"

#include <sys/time.h>

#include "TSPLooper.h"
#include "TSPHandler.h"
#include "TSPLooperRoster.h"
#include "TSPMessage.h"

TSPLooperRoster gTsPlayerLooperRoster;
extern pthread_t AmlGetThreadId();

struct TSPLooper::LooperThread : public Thread {
    LooperThread(TSPLooper *looper)
        : mLooper(looper),
          mThreadId(-1) {
    }

    virtual dp_state readyToRun() {
        mThreadId = AmlGetThreadId();

        return Thread::readyToRun();
    }

    virtual bool threadLoop() {
        return mLooper->loop();
    }

    bool isCurrentThread() const {
        return mThreadId == AmlGetThreadId();
    }

protected:
    virtual ~LooperThread() {}

private:
    TSPLooper *mLooper;
    pthread_t mThreadId;

};

// static
int64_t TSPLooper::GetNowUs() {
    return systemTime(SYSTEM_TIME_MONOTONIC) / 1000ll;
}

TSPLooper::TSPLooper()
    : mRunningLocally(false) {
    // clean up stale AHandlers. Doing it here instead of in the destructor avoids
    // the side effect of objects being deleted from the unregister function recursively.
    gTsPlayerLooperRoster.unregisterStaleHandlers();
}

TSPLooper::~TSPLooper() {
    stop();
}

void TSPLooper::setName(const char *name) {
    mName = name;
}

TSPLooper::handler_id TSPLooper::registerHandler(const sp<TSPHandler> &handler) {
    return gTsPlayerLooperRoster.registerHandler(this, handler);
}

void TSPLooper::unregisterHandler(handler_id handlerID) {
    gTsPlayerLooperRoster.unregisterHandler(handlerID);
}

dp_state TSPLooper::start(bool runOnCallingThread) {
    if (runOnCallingThread) {
        {
            TSPMutex::Autolock autoLock(mLock);

            if (mThread != NULL || mRunningLocally) {
                return INVALID_OPERATION;
            }

            mRunningLocally = true;
        }

        do {
        } while (loop());

        return OK;
    }

    TSPMutex::Autolock autoLock(mLock);

    if (mThread != NULL || mRunningLocally) {
        return INVALID_OPERATION;
    }

    mThread = new LooperThread(this);

    dp_state err = mThread->run(
            (mName == NULL) ? "TSPLooper" : mName);
    if (err != OK) {
        mThread.clear();
    }

    return err;
}

dp_state TSPLooper::stop() {
    sp<LooperThread> thread;
    bool runningLocally;

    {
        TSPMutex::Autolock autoLock(mLock);

        thread = mThread;
        runningLocally = mRunningLocally;
        mThread.clear();
        mRunningLocally = false;
    }

    if (thread == NULL && !runningLocally) {
        return INVALID_OPERATION;
    }

    if (thread != NULL) {
        thread->requestExit();
    }

    mQueueChangedCondition.signal();
    {
        TSPMutex::Autolock autoLock(mRepliesLock);
        mRepliesCondition.broadcast();
    }

    if (!runningLocally && !thread->isCurrentThread()) {
        // If not running locally and this thread _is_ the looper thread,
        // the loop() function will return and never be called again.
        thread->requestExitAndWait();
    }

    return OK;
}

void TSPLooper::post(const sp<TSPMessage> &msg, int64_t delayUs) {
    TSPMutex::Autolock autoLock(mLock);

    int64_t whenUs;
    if (delayUs > 0) {
        whenUs = GetNowUs() + delayUs;
    } else {
        whenUs = GetNowUs();
    }

    List<Event>::iterator it = mEventQueue.begin();
    while (it != mEventQueue.end() && (*it).mWhenUs <= whenUs) {
        ++it;
    }

    Event event;
    event.mWhenUs = whenUs;
    event.mMessage = msg;

    if (it == mEventQueue.begin()) {
        mQueueChangedCondition.signal();
    }

    mEventQueue.insert(it, event);
}

bool TSPLooper::loop() {
    Event event;

    {
        TSPMutex::Autolock autoLock(mLock);
        if (mThread == NULL && !mRunningLocally) {
            return false;
        }
        if (mEventQueue.empty()) {
            mQueueChangedCondition.wait(mLock);
            return true;
        }
        int64_t whenUs = (*mEventQueue.begin()).mWhenUs;
        int64_t nowUs = GetNowUs();

        if (whenUs > nowUs) {
            int64_t delayUs = whenUs - nowUs;
            mQueueChangedCondition.waitRelative(mLock, delayUs * 1000ll);

            return true;
        }

        event = *mEventQueue.begin();
        mEventQueue.erase(mEventQueue.begin());
    }

    event.mMessage->deliver();

    return true;
}

// to be called by AMessage::postAndAwaitResponse only
sp<TSPReplyToken> TSPLooper::createReplyToken() {
    return new TSPReplyToken(this);
}

// to be called by AMessage::postAndAwaitResponse only
dp_state TSPLooper::awaitResponse(const sp<TSPReplyToken> &replyToken, sp<TSPMessage> *response) {
    // return status in case we want to handle an interrupted wait
    ALOGE("awaitResponse autoLock");
    TSPMutex::Autolock autoLock(mRepliesLock);
    if (replyToken == NULL)
        return -NO_MEMORY;
    ALOGE("awaitResponse autoLock while");
    while (!replyToken->retrieveReply(response)) {
        {
            ALOGE("awaitResponse mLock");
            TSPMutex::Autolock autoLock(mLock);
            if (mThread == NULL) {
                return -ENOENT;
            }
        }
        ALOGE("awaitResponse mRepliesCondition.wait\n");
        mRepliesCondition.wait(mRepliesLock);
        ALOGE("awaitResponse mRepliesCondition.wait ok\n");
    }
    return OK;
}

dp_state TSPLooper::postReply(const sp<TSPReplyToken> &replyToken, const sp<TSPMessage> &reply) {
    ALOGE("TSPLooper postReply");
    TSPMutex::Autolock autoLock(mRepliesLock);
    ALOGE("TSPLooper postReply setReply");
    dp_state err = replyToken->setReply(reply);
    if (err == OK) {
        ALOGE("TSPLooper postReply mRepliesCondition broadcast");
        mRepliesCondition.broadcast();
    }
    return err;
}
