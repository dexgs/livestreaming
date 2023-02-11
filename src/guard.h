#ifndef GUARD_H_
#define GUARD_H_

#include <assert.h>
#include <pthread.h>


#define GUARD(lock, block) {\
    int err; \
    pthread_mutex_t * mtx = lock; \
    err = pthread_mutex_lock(mtx); \
    assert(err == 0); \
    block \
    err = pthread_mutex_unlock(mtx); \
    assert(err == 0); \
}

#endif
