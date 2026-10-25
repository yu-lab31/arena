#ifndef ARENA_ALLOCATOR_
#define ARENA_ALLOCATOR_

#include <stddef.h>

#include "arena_stack.h"

#ifdef ARENA_IMPL
_Thread_local ArenaStackAdapter *p_arena_stack = NULL;

pthread_key_t arena_stack_key;
pthread_once_t arena_stack_once = PTHREAD_ONCE_INIT;

ATOMIC_ARENA_WORD arena_tids[ARENA_WORD_NUM] = {0};

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

#endif // ARENA_ALLOCATOR_
