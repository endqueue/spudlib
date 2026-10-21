#ifndef SPUDTHREAD_H
#define SPUDTHREAD_H

// The mutex SpudLib's own code locks, written once so that every module and
// backend that has to serialize something does it the same way. Private:
// nothing here is in a public header, and SpudLib still starts no thread of
// its own. A spud_mutex is never copied: it lives in an object that is only
// ever reached through a pointer.
//
// On Windows it is an SRWLOCK, which needs no destroy and can't fail to be
// made. Elsewhere it is a pthread mutex, whose init can fail: a create call
// that makes one fails with SPUDRESULT_GENERAL_FAILURE when it does.
//
// Not recursive. Nothing that holds one calls back into the caller.

#include <stdbool.h>

#if SPUDLIB_PLATFORM_WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct spud_mutex {
	SRWLOCK lock;
} spud_mutex;

static inline bool spud_mutex_init(spud_mutex *mutex) {
	InitializeSRWLock(&mutex->lock);
	return true;
}

static inline void spud_mutex_destroy(spud_mutex *mutex) { (void)mutex; }

static inline void spud_mutex_lock(spud_mutex *mutex) { AcquireSRWLockExclusive(&mutex->lock); }

static inline void spud_mutex_unlock(spud_mutex *mutex) { ReleaseSRWLockExclusive(&mutex->lock); }

#else
#include <pthread.h>

typedef struct spud_mutex {
	pthread_mutex_t lock;
} spud_mutex;

static inline bool spud_mutex_init(spud_mutex *mutex) { return pthread_mutex_init(&mutex->lock, NULL) == 0; }

static inline void spud_mutex_destroy(spud_mutex *mutex) { pthread_mutex_destroy(&mutex->lock); }

static inline void spud_mutex_lock(spud_mutex *mutex) { pthread_mutex_lock(&mutex->lock); }

static inline void spud_mutex_unlock(spud_mutex *mutex) { pthread_mutex_unlock(&mutex->lock); }

#endif

#endif // SPUDTHREAD_H
