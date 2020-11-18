#ifndef _DRMPLAYER_UTILS_CONDITION_H
#define _DRMPLAYER_UTILS_CONDITION_H

#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <Timers.h>
#include <pthread.h>
#include <Mutex.h>

class TSPCondition {
public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

    enum WakeUpType {
        WAKE_UP_ONE = 0,
        WAKE_UP_ALL = 1
    };

    TSPCondition();
    explicit TSPCondition(int type);
    ~TSPCondition();
    // Wait on the condition variable.  Lock the mutex before calling.
    // Note that spurious wake-ups may happen.
    dp_state wait(TSPMutex& mutex);
    // same with relative timeout
    dp_state waitRelative(TSPMutex& mutex, nsecs_t reltime);
    // Signal the condition variable, allowing one thread to continue.
    void signal();
    // Signal the condition variable, allowing one or all threads to continue.
    void signal(WakeUpType type) {
        if (type == WAKE_UP_ONE) {
            signal();
        } else {
            broadcast();
        }
    }
    // Signal the condition variable, allowing all threads to continue.
    void broadcast();

private:
    pthread_cond_t mCond;
};

inline TSPCondition::TSPCondition() : TSPCondition(PRIVATE) {
}
inline TSPCondition::TSPCondition(int type) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);

    if (type == SHARED) {
        pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    }

    pthread_cond_init(&mCond, &attr);
    pthread_condattr_destroy(&attr);

}
inline TSPCondition::~TSPCondition() {
    pthread_cond_destroy(&mCond);
}
inline dp_state TSPCondition::wait(TSPMutex& mutex) {
    return -pthread_cond_wait(&mCond, &mutex.mMutex);
}
inline dp_state TSPCondition::waitRelative(TSPMutex& mutex, nsecs_t reltime) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);


    // On 32-bit devices, tv_sec is 32-bit, but `reltime` is 64-bit.
    int64_t reltime_sec = reltime/1000000000;

    ts.tv_nsec += static_cast<long>(reltime%1000000000);
    if (reltime_sec < INT64_MAX && ts.tv_nsec >= 1000000000) {
        ts.tv_nsec -= 1000000000;
        ++reltime_sec;
    }

    int64_t time_sec = ts.tv_sec;
    if (time_sec > INT64_MAX - reltime_sec) {
        time_sec = INT64_MAX;
    } else {
        time_sec += reltime_sec;
    }

    ts.tv_sec = (time_sec > LONG_MAX) ? LONG_MAX : static_cast<long>(time_sec);

    return -pthread_cond_timedwait(&mCond, &mutex.mMutex, &ts);
}
inline void TSPCondition::signal() {
    pthread_cond_signal(&mCond);
}
inline void TSPCondition::broadcast() {
    pthread_cond_broadcast(&mCond);
}

#endif // _LIBS_UTILS_CONDITON_H
