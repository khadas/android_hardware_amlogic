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

#ifndef TSPLAYER_A_LOOPER_H_
#define TSPLAYER_A_LOOPER_H_

#include <DrmpErrors.h>
#include <KeyedVector.h>
#include <List.h>
#include <RefBase.h>
#include <Thread.h>
#include <Mutex.h>
#include <Condition.h>


struct TSPHandler;
struct TSPMessage;
struct TSPReplyToken;

struct TSPLooper : public RefBase {
    typedef int32_t event_id;
    typedef int32_t handler_id;

    TSPLooper();

    // Takes effect in a subsequent call to start().
    void setName(const char *name);

    handler_id registerHandler(const sp<TSPHandler> &handler);
    void unregisterHandler(handler_id handlerID);

    dp_state start(bool runOnCallingThread = false);

    dp_state stop();

    static int64_t GetNowUs();

    const char *getName() const {
        return mName;
    }

protected:
    virtual ~TSPLooper();

private:
    friend struct TSPMessage;       // post()

    struct Event {
        int64_t mWhenUs;
        sp<TSPMessage> mMessage;
    };

    TSPMutex mLock;
    TSPCondition mQueueChangedCondition;

    const char * mName;

    List<Event> mEventQueue;

    struct LooperThread;
    sp<LooperThread> mThread;
    bool mRunningLocally;

    // use a separate lock for reply handling, as it is always on another thread
    // use a central lock, however, to avoid creating a mutex for each reply
    TSPMutex mRepliesLock;
    TSPCondition mRepliesCondition;

    // START --- methods used only by TSPMessage

    // posts a message on this looper with the given timeout
    void post(const sp<TSPMessage> &msg, int64_t delayUs);

    // creates a reply token to be used with this looper
    sp<TSPReplyToken> createReplyToken();
    // waits for a response for the reply token.  If status is OK, the response
    // is stored into the supplied variable.  Otherwise, it is unchanged.
    dp_state awaitResponse(const sp<TSPReplyToken> &replyToken, sp<TSPMessage> *response);
    // posts a reply for a reply token.  If the reply could be successfully posted,
    // it returns OK. Otherwise, it returns an error value.
    dp_state postReply(const sp<TSPReplyToken> &replyToken, const sp<TSPMessage> &msg);

    bool loop();
};


#endif  // A_LOOPER_H_
