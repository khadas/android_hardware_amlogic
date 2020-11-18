#ifndef _DRMPLAYER_UTILS_MUTEX_H
#define _DRMPLAYER_UTILS_MUTEX_H

#include <stdint.h>
#include <sys/types.h>
#include <time.h>
#include <pthread.h>
#include <DrmpErrors.h>


// Enable thread safety attributes only with clang.
// The attributes can be safely erased when compiling with other compilers.
#if defined(__clang__) && (!defined(SWIG))
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)  // no-op
#endif

#define CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(capability(x))

#define SCOPED_CAPABILITY THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

#define ACQUIRED_BEFORE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

#define ACQUIRED_AFTER(...) THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))

#define REQUIRES(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_capability(__VA_ARGS__))

#define REQUIRES_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(requires_shared_capability(__VA_ARGS__))

#define ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_capability(__VA_ARGS__))

#define ACQUIRE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(acquire_shared_capability(__VA_ARGS__))

#define RELEASE(...) THREAD_ANNOTATION_ATTRIBUTE__(release_capability(__VA_ARGS__))

#define RELEASE_SHARED(...) THREAD_ANNOTATION_ATTRIBUTE__(release_shared_capability(__VA_ARGS__))

#define TRY_ACQUIRE(...) THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_capability(__VA_ARGS__))

#define TRY_ACQUIRE_SHARED(...) \
    THREAD_ANNOTATION_ATTRIBUTE__(try_acquire_shared_capability(__VA_ARGS__))

#define EXCLUDES(...) THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

#define ASSERT_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_capability(x))

#define ASSERT_SHARED_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_capability(x))

#define RETURN_CAPABILITY(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

#define NO_THREAD_SAFETY_ANALYSIS THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

class TSPCondition;

/*
 * NOTE: This class is for code that builds on Win32.  Its usage is
 * deprecated for code which doesn't build for Win32.  New code which
 * doesn't build for Win32 should use std::mutex and std::lock_guard instead.
 *
 * Simple mutex class.  The implementation is system-dependent.
 *
 * The mutex must be unlocked by the thread that locked it.  They are not
 * recursive, i.e. the same thread can't lock it multiple times.
 */
class CAPABILITY("mutex") TSPMutex {
  public:
    enum {
        PRIVATE = 0,
        SHARED = 1
    };

    TSPMutex();
    explicit TSPMutex(const char* name);
    explicit TSPMutex(int type, const char* name = NULL);
    ~TSPMutex();

    // lock or unlock the mutex
    dp_state lock() ACQUIRE();
    void unlock() RELEASE();

    // lock if possible; returns 0 on success, error otherwise
    dp_state tryLock() TRY_ACQUIRE(true);

    // Manages the mutex automatically. It'll be locked when Autolock is
    // constructed and released when Autolock goes out of scope.
    class SCOPED_CAPABILITY Autolock {
      public:
        inline explicit Autolock(TSPMutex& mutex) ACQUIRE(mutex) : mLock(mutex) { mLock.lock(); }
        inline explicit Autolock(TSPMutex* mutex) ACQUIRE(mutex) : mLock(*mutex) { mLock.lock(); }
        inline ~Autolock() RELEASE() { mLock.unlock(); }

      private:
        TSPMutex& mLock;
        // Cannot be copied or moved - declarations only
        Autolock(const Autolock&);
        Autolock& operator=(const Autolock&);
    };

  private:
    friend class TSPCondition;

    // A mutex cannot be copied
    TSPMutex(const TSPMutex&);
    TSPMutex& operator=(const TSPMutex&);
    pthread_mutex_t mMutex;
};

inline TSPMutex::TSPMutex() {
    pthread_mutex_init(&mMutex, NULL);
}
inline TSPMutex::TSPMutex(__attribute__((unused)) const char* name) {
    pthread_mutex_init(&mMutex, NULL);
}
inline TSPMutex::TSPMutex(int type, __attribute__((unused)) const char* name) {
    if (type == SHARED) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mMutex, &attr);
        pthread_mutexattr_destroy(&attr);
    } else {
        pthread_mutex_init(&mMutex, NULL);
    }
}
inline TSPMutex::~TSPMutex() {
    pthread_mutex_destroy(&mMutex);
}
inline dp_state TSPMutex::lock() {
    return -pthread_mutex_lock(&mMutex);
}
inline void TSPMutex::unlock() {
    pthread_mutex_unlock(&mMutex);
}
inline dp_state TSPMutex::tryLock() {
    return -pthread_mutex_trylock(&mMutex);
}


typedef TSPMutex::Autolock AutoMutex;

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------

#endif // _LIBS_UTILS_MUTEX_H
