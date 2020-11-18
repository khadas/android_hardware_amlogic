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

#ifndef TSP_LOOPER_ROSTER_H_

#define TSP_LOOPER_ROSTER_H_

#include <TSPLooper.h>
#include <KeyedVector.h>
#include <String16.h>

struct TSPLooperRoster {
    TSPLooperRoster();

    TSPLooper::handler_id registerHandler(
            const sp<TSPLooper> &looper, const sp<TSPHandler> &handler);

    void unregisterHandler(TSPLooper::handler_id handlerID);
    void unregisterStaleHandlers();

    void dump(int fd, const Vector<String16>& args);

private:
    struct HandlerInfo {
        wp<TSPLooper> mLooper;
        wp<TSPHandler> mHandler;
    };

    TSPMutex mLock;
    KeyedVector<TSPLooper::handler_id, HandlerInfo> mHandlers;
    TSPLooper::handler_id mNextHandlerID;
};

#endif  // A_LOOPER_ROSTER_H_
