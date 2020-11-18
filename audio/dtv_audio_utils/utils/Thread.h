/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#ifndef _LIBS_UTILS_THREAD_H
#define _LIBS_UTILS_THREAD_H

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <Condition.h>
#include <Mutex.h>
#include <RefBase.h>
#include <sys/types.h>
#include <sys/wait.h>

typedef int (*aml_thread_func_t)(void*);
typedef aml_thread_func_t thread_func_t;
typedef void* (*aml_pthread_entry)(void*);

class Thread : virtual public RefBase
{
public:
    // Create a Thread object, but doesn't create or start the associated
    // thread. See the run() method.
    explicit            Thread();
    virtual             ~Thread();

    // Start the thread in threadLoop() which needs to be implemented.
    virtual dp_state    run(    const char* name,
                                size_t stack = 0);

    // Ask this object's thread to exit. This function is asynchronous, when the
    // function returns the thread might still be running. Of course, this
    // function can be called from a different thread.
    virtual void        requestExit();

    // Good place to do one-time initializations
    virtual dp_state    readyToRun();

    // Call requestExit() and wait until this object's thread exits.
    // BE VERY CAREFUL of deadlocks. In particular, it would be silly to call
    // this function from this object's thread. Will return WOULD_BLOCK in
    // that case.
            dp_state    requestExitAndWait();

    // Wait until this object's thread exits. Returns immediately if not yet running.
    // Do not call from this object's thread; will return WOULD_BLOCK in that case.
            dp_state    join();

    // Indicates whether this thread is running or not.
            bool        isRunning() const;

protected:
    // exitPending() returns true if requestExit() has been called.
            bool        exitPending() const;

private:
    // Derived class must implement threadLoop(). The thread starts its life
    // here. There are two ways of using the Thread object:
    // 1) loop: if threadLoop() returns true, it will be called again if
    //          requestExit() wasn't called.
    // 2) once: if threadLoop() returns false, the thread will exit upon return.
    virtual bool        threadLoop() = 0;

private:
    Thread& operator=(const Thread&);
    static  int             _threadLoop(void* user);
    // always hold mLock when reading or writing
    pthread_t       mThread;
    const char *    mThreadName;
    mutable TSPMutex   mLock;
    TSPCondition       mThreadExitedCondition;
    dp_state        mStatus;
    // note that all accesses of mExitPending and mRunning need to hold mLock
    volatile bool           mExitPending;
    volatile bool           mRunning;
    sp<Thread>      mHoldSelf;
};


// ---------------------------------------------------------------------------
#endif // _LIBS_UTILS_THREAD_H
// ---------------------------------------------------------------------------
