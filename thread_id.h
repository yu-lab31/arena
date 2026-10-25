#ifndef THREAD_ID_H_
#define THREAD_ID_H_

#include <unistd.h>

#include "arena_layout.h"

#ifdef __linux__

#include <sys/syscall.h>
#include <sys/types.h>

static inline int arena_thread_index(void)
{
    pid_t tid = syscall(SYS_gettid);
    return (int)tid % MAXIMUM_THREAD_NUM;
}

#else

static inline int arena_thread_index(void) { return 0; }

#endif // __linux__

#include <stdatomic.h>

#define ARENA_WORD size_t
#define ATOMIC_ARENA_WORD atomic_size_t
#define ATOMIC_ARENA_WORD_SZ_BYTE (sizeof(ATOMIC_ARENA_WORD))
#define ATOMIC_ARENA_WORD_SZ_BITS (ATOMIC_ARENA_WORD_SZ_BYTE << 3)

#define ARENA_WORD_NUM (MAXIMUM_THREAD_NUM / ATOMIC_ARENA_WORD_SZ_BITS)

// must be initialized to 0
extern atomic_size_t arena_tids[ARENA_WORD_NUM];

static inline int arena_requesttid(void)
{
    size_t times_tried = 0;
    for (int i = arena_thread_index(); times_tried < MAXIMUM_THREAD_NUM;
         times_tried++) {
        int wi = i / ATOMIC_ARENA_WORD_SZ_BITS,
            bi = i % ATOMIC_ARENA_WORD_SZ_BITS;

        ARENA_WORD word = atomic_load(&arena_tids[wi]);
        if (!(word & ((ARENA_WORD)1 << bi))) {
            if (atomic_compare_exchange_weak(&arena_tids[wi], &word,
                                             word | ((ARENA_WORD)1 << bi))) {
                return i;
            } else {
                i--;
                times_tried--;
            }
        }
        i = ((i + 1) % MAXIMUM_THREAD_NUM);
    }
    return -1;
}

static inline void arena_releasetid(int tid)
{
    if (tid < 0 || (size_t)tid >= MAXIMUM_THREAD_NUM)
        return;

    int wi = tid / ATOMIC_ARENA_WORD_SZ_BITS,
        bi = tid % ATOMIC_ARENA_WORD_SZ_BITS;

    atomic_fetch_and(&arena_tids[wi], ~((ARENA_WORD)1 << bi));
}

#endif // THREAD_ID_H_
