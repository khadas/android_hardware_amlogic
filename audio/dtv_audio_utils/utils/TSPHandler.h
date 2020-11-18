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

#ifndef TSPLAYER_A_HANDLER_H_

#define TSPLAYER_A_HANDLER_H_

#include <TSPLooper.h>
#include <KeyedVector.h>
#include <RefBase.h>

struct TSPMessage;

struct TSPHandler : public RefBase {
    TSPHandler()
        : mID(0),
          mVerboseStats(false),
          mMessageCounter(0) {
    }

    TSPLooper::handler_id id() const {
        return mID;
    }

    sp<TSPLooper> looper() const {
        return mLooper.promote();
    }

    wp<TSPLooper> getLooper() const {
        return mLooper;
    }

    wp<TSPHandler> getHandler() const {
        // allow getting a weak reference to a const handler
        return const_cast<TSPHandler *>(this);
    }

protected:
    virtual void onMessageReceived(const sp<TSPMessage> &msg) = 0;

private:
    friend struct TSPMessage;      // deliverMessage()
    friend struct TSPLooperRoster; // setID()

    TSPLooper::handler_id mID;
    wp<TSPLooper> mLooper;

    inline void setID(TSPLooper::handler_id id, const wp<TSPLooper> &looper) {
        mID = id;
        mLooper = looper;
    }

    bool mVerboseStats;
    uint32_t mMessageCounter;
    KeyedVector<uint32_t, uint32_t> mMessages;

    void deliverMessage(const sp<TSPMessage> &msg);
};


#endif  // A_HANDLER_H_
