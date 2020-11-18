/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * Description:
 */

#define LOG_NDEBUG 0
#define LOG_TAG "libutils.threads"

#include <assert.h>
#include <Thread.h>
#include <sys/resource.h>
#include <sys/prctl.h>



int AmlCreateRawThreadEtc(aml_thread_func_t entryFunction,
                               void *userData,
                               size_t threadStackSize,
                               pthread_t *threadId)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);



    if (threadStackSize) {
        pthread_attr_setstacksize(&attr, threadStackSize);
    }

    errno = 0;
    pthread_t thread;
    int result = pthread_create(&thread, &attr,
                    (aml_pthread_entry)entryFunction, userData);
    pthread_attr_destroy(&attr);
    if (result != 0) {
        /*ALOGE("androidCreateRawThreadEtc failed (entry=%p, res=%d, %s)\n",
			  entryFunction, result, strerror(errno));*/
        return 0;
    }

    // Note that *threadID is directly available to the parent only, as it is
    // assigned after the child starts.  Use memory barrier / lock if the child
    // or other threads also need access.
    if (threadId != NULL) {
        *threadId = thread; // XXX: this is not portable
    }
    return 1;
}

pthread_t AmlGetThreadId()
{
    return (pthread_t)pthread_self();
}

Thread::Thread()
    :   mThread(pthread_t(-1)),
        mLock("Thread::mLock"),
        mStatus(NO_ERROR),
        mExitPending(false), mRunning(false)
{
}

Thread::~Thread()
{
}

dp_state Thread::readyToRun()
{
    return NO_ERROR;
}

dp_state Thread::run(const char* name, size_t stack)
{
    //LOG_ALWAYS_FATAL_IF(name == nullptr, "thread name not provided to Thread::run");

    TSPMutex::Autolock _l(mLock);

    if (mRunning) {
        // thread already started
        return INVALID_OPERATION;
    }

    // reset status and exitPending to their default value, so we can
    // try again after an error happened (either below, or in readyToRun())
    mStatus = NO_ERROR;
    mExitPending = false;
    mThread = pthread_t(-1);
    // hold a strong reference on ourself
    mHoldSelf = this;
    mThreadName = name;
    mRunning = true;

    bool res;
    res = AmlCreateRawThreadEtc(_threadLoop,
          this, stack, &mThread);

    if (res == false) {
        mStatus = UNKNOWN_ERROR;   // something happened!
        mRunning = false;
        mThread = pthread_t(-1);
        mHoldSelf.clear();  // "this" may have gone away after this.

        return UNKNOWN_ERROR;
    }

    // Do not refer to mStatus here: The thread is already running (may, in fact
    // already have exited with a valid mStatus result). The NO_ERROR indication
    // here merely indicates successfully starting the thread and does not
    // imply successful termination/execution.
    return NO_ERROR;

    // Exiting scope of mLock is a memory barrier and allows new thread to run
}

int Thread::_threadLoop(void* user)
{
    Thread* const self = static_cast<Thread*>(user);
    pthread_setname_np(pthread_self(), self->mThreadName);
    sp<Thread> strong(self->mHoldSelf);
    wp<Thread> weak(strong);
    self->mHoldSelf.clear();

    bool first = true;

    do {
        bool result;
        if (first) {
            first = false;
            self->mStatus = self->readyToRun();
            result = (self->mStatus == NO_ERROR);

            if (result && !self->exitPending()) {
                // Binder threads (and maybe others) rely on threadLoop
                // running at least once after a successful ::readyToRun()
                // (unless, of course, the thread has already been asked to exit
                // at that point).
                // This is because threads are essentially used like this:
                //   (new ThreadSubclass())->run();
                // The caller therefore does not retain a strong reference to
                // the thread and the thread would simply disappear after the
                // successful ::readyToRun() call instead of entering the
                // threadLoop at least once.
                result = self->threadLoop();
            }
        } else {
            result = self->threadLoop();
        }

        // establish a scope for mLock
        {
        TSPMutex::Autolock _l(self->mLock);
        if (result == false || self->mExitPending) {
            self->mExitPending = true;
            self->mRunning = false;
            // clear thread ID so that requestExitAndWait() does not exit if
            // called by a new thread using the same thread ID as this one.
            self->mThread = pthread_t(-1);
            // note that interested observers blocked in requestExitAndWait are
            // awoken by broadcast, but blocked on mLock until break exits scope
            self->mThreadExitedCondition.broadcast();
            break;
        }
        }

        // Release our strong reference, to let a chance to the thread
        // to die a peaceful death.
        strong.clear();
        // And immediately, re-acquire a strong reference for the next loop
        strong = weak.promote();
    } while(strong != 0);

    return 0;
}

void Thread::requestExit()
{
    TSPMutex::Autolock _l(mLock);
    mExitPending = true;
}

dp_state Thread::requestExitAndWait()
{
    TSPMutex::Autolock _l(mLock);
    if (mThread == AmlGetThreadId()) {
        /*ALOGW(
        "Thread (this=%p): don't call waitForExit() from this "
        "Thread object's thread. It's a guaranteed deadlock!",
        this);*/

        return WOULD_BLOCK;
    }

    mExitPending = true;

    while (mRunning == true) {
        mThreadExitedCondition.wait(mLock);
    }
    // This next line is probably not needed any more, but is being left for
    // historical reference. Note that each interested party will clear flag.
    mExitPending = false;

    return mStatus;
}

dp_state Thread::join()
{
    TSPMutex::Autolock _l(mLock);
    if (mThread == AmlGetThreadId()) {
        /*ALOGW(
        "Thread (this=%p): don't call join() from this "
        "Thread object's thread. It's a guaranteed deadlock!",
        this);*/

        return WOULD_BLOCK;
    }

    while (mRunning == true) {
        mThreadExitedCondition.wait(mLock);
    }

    return mStatus;
}

bool Thread::isRunning() const {
    TSPMutex::Autolock _l(mLock);
    return mRunning;
}

bool Thread::exitPending() const
{
    TSPMutex::Autolock _l(mLock);
    return mExitPending;
}

