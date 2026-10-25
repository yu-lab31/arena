#ifndef ARENA_STACK_H_
#define ARENA_STACK_H_

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "arena_alloc.h"
#include "arena_layout.h"
#include "fat_pointer.h"
#include "thread_id.h"

typedef struct
{
    size_t local_pointer_offset;
    size_t thread_pointer_offset;
    FatPointer *local_pointers;
    FatPointer *thread_pointers;
    int arena_tid;
} ArenaStack;

typedef struct
{
    ArenaStack stack;
    uint8_t reserved[TH_STACK_HEADER_SZ - sizeof(ArenaStack)];
} ArenaStackHeader;

typedef struct
{
    ArenaStackHeader stack_header;
    uint8_t local_pointer_data[LOCAL_POINTER_RESERVED];
    uint8_t thread_pointer_data[THREAD_POINTER_RESERVED];
} ArenaStackAdapter;

extern _Thread_local ArenaStackAdapter *p_arena_stack;

extern pthread_key_t arena_stack_key;
extern pthread_once_t arena_stack_once;

#define arena_stack (p_arena_stack->stack_header.stack)

#define ARENA_STACK_PUSH_FRAME() (arena_stack.local_pointer_offset)

#define ARENA_STACK_POP_FRAME(frame)                                           \
    do {                                                                       \
        size_t prev_frame = frame, lp_idx = prev_frame;                        \
        if (lp_idx >= arena_stack.local_pointer_offset)                        \
            break;                                                             \
        for (; lp_idx < arena_stack.local_pointer_offset; lp_idx++) {          \
            if (fat_pointer_escaped_stack(                                     \
                    arena_stack.local_pointers[lp_idx]) ||                     \
                fat_pointer_escaped_thread(                                    \
                    arena_stack.local_pointers[lp_idx])) {                     \
                if (arena_stack.thread_pointer_offset >=                       \
                    MAXIMUM_THREAD_POINTER_NUMBER)                             \
                    abort();                                                   \
                arena_stack                                                    \
                    .thread_pointers[arena_stack.thread_pointer_offset++] =    \
                    arena_stack.local_pointers[lp_idx];                        \
            } else if (fat_pointer_escaped_stack_frame(                        \
                           arena_stack.local_pointers[lp_idx])) {              \
                fat_pointer_catch(&(arena_stack.local_pointers[lp_idx]));      \
                arena_stack.local_pointers[prev_frame++] =                     \
                    arena_stack.local_pointers[lp_idx];                        \
            } else                                                             \
                assert(0 /* TODO: free heap memory */);                        \
        }                                                                      \
        arena_stack.local_pointer_offset = prev_frame;                         \
    } while (0)

#define ARENA_STACK_ACCEPT(ptr)                                                \
    do {                                                                       \
        if (arena_stack.local_pointer_offset >= MAXIMUM_LOCAL_POINTER_NUMBER)  \
            abort();                                                           \
        FatPointer *pfp =                                                      \
            &(arena_stack.local_pointers[arena_stack.local_pointer_offset]);   \
        pfp->escape_flags = (uint8_t)0;                                        \
        pfp->addr = ptr;                                                       \
        arena_stack.local_pointer_offset++;                                    \
    } while (0)

#define ARENA_STACK_FRAME_SEARCH(ptr, frame, pos)                              \
    for (pos = frame; pos < arena_stack.local_pointer_offset; pos++)

static inline void arena_thread_exit(void *unused)
{
    (void)unused;
    // TODO: free heap memory
    for (size_t tp_idx = 0; tp_idx < arena_stack.thread_pointer_offset;
         tp_idx++);

    int tid = arena_stack.arena_tid;

    madvise(p_arena_stack, TH_STACK_CAPACITY, MADV_DONTNEED);

    arena_releasetid(tid);
    p_arena_stack = NULL;
}

static inline void arena_stack_key_init(void)
{
    arena_mem_format();
    pthread_key_create(&arena_stack_key, arena_thread_exit);
}

static inline void arena_stack_init(void)
{
    static_assert(sizeof(FatPointer) == FAT_POINTER_SIZE,
                  "fat pointer type must be 16byte-sized");
    static_assert(sizeof(ArenaStackAdapter) == TH_STACK_CAPACITY,
                  "bad aligned stack");
    if (!p_arena_stack) {
        pthread_once(&arena_stack_once, arena_stack_key_init);
        int tid = arena_requesttid();
        if (tid < 0)
            abort();

        ArenaStackAdapter *stacks = locate_stack();
        p_arena_stack = &stacks[tid];
        arena_stack.arena_tid = tid;
        arena_stack.local_pointer_offset = 0;
        arena_stack.thread_pointer_offset = 0;

        arena_stack.local_pointers =
            (FatPointer *)p_arena_stack->local_pointer_data;
        arena_stack.thread_pointers =
            (FatPointer *)p_arena_stack->thread_pointer_data;

        if (pthread_setspecific(arena_stack_key, &arena_stack) != 0) {
            p_arena_stack = NULL;
            abort();
        }
    }
}

#endif // ARENA_STACK_H_
