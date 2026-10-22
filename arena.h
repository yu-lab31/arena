#ifndef ARENA_ALLOCATOR_
#define ARENA_ALLOCATOR_

#include <stddef.h>

#include "arena_stack.h"

#ifdef ARENA_IMPL
_Thread_local ArenaStack arena_stack = {0};

pthread_key_t arena_stack_key;
pthread_once_t arena_stack_once = PTHREAD_ONCE_INIT;
#endif

static inline void arena_frame_pop(size_t frame)
{
    ARENA_STACK_POP_FRAME(frame);
}

#define ARENA_FRAME                                                            \
    for (size_t current_frame = ARENA_STACK_PUSH_FRAME(), _arena_frame = 1;    \
         _arena_frame; arena_frame_pop(current_frame), _arena_frame = 0)

#define ARENA_THREAD                                                           \
    for (int _arena_thread = (arena_stack_init(), 1); _arena_thread;           \
         _arena_thread = 0)

#endif // ARENA_ALLOCATOR_
