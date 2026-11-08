#ifndef ARENA_ALLOCATOR_
#define ARENA_ALLOCATOR_

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "arena_alloc.h"
#include "arena_layout.h"
#include "arena_stack.h"

#ifdef ARENA_IMPL
_Thread_local ArenaStackAdapter *p_arena_stack = NULL;

pthread_key_t arena_stack_key;
pthread_once_t arena_stack_once = PTHREAD_ONCE_INIT;

// id zero reserved
ATOMIC_ARENA_WORD arena_tids[ARENA_WORD_NUM] = {1};

void *arena_memory = NULL;
#endif

static inline void arena_frame_pop(size_t frame)
{
    ARENA_STACK_POP_FRAME(frame);
}

#define WITH_ARENA()                                                           \
    for (size_t                                                                \
             _current_frame = (arena_stack_init(), ARENA_STACK_PUSH_FRAME()),  \
             _arena_frame = 1;                                                 \
         _arena_frame; _arena_frame = (arena_frame_pop(_current_frame), 0))

/**
 * Allocate size bytes of uninitialized memory and align automatically, return
 * NULL when no space left. Cannot be called outside WITH_ARENA scope.
 * */
static inline void *arenalloc(size_t size);
void *arenarealloc(void *ptr, size_t size);
void arenafree(void *ptr);
void arenaescapeframe(void *ptr);
void arenaescapestack(void *ptr);
void *arenaescape(void *ptr);

static inline void *arenalloc(size_t size)
{
    void *res = NULL;
    int tid = arena_stack.arena_tid;

    if (size <= ARENA_BLOCK_CAP) {
        res = arena_alloc_normal(size, (int16_t)tid, tid);
    } else {
        res = arena_alloc_huge(size);
    }

    if (!res)
        return NULL;

    ARENA_STACK_ACCEPT(res, size);
    return res;
}

#endif // ARENA_ALLOCATOR_
