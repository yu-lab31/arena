#ifndef STACK_H_
#define STACK_H_

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "fat_pointer.h"

// One stack can hold up to 4MB of pointer data
#define MAXIMUM_STACK_DEPTH ((1 << 22) / FAT_POINTER_SIZE)
// One quarter of the stack is reserved for thread pointers.
#define MAXIMUM_THREAD_POINTER_NUMBER (MAXIMUM_STACK_DEPTH >> 2)
#define THREAD_POINTER_RESERVED                                                \
    (MAXIMUM_THREAD_POINTER_NUMBER * FAT_POINTER_SIZE)
// Other 3/4 is reserved for local pointers.
#define MAXIMUM_LOCAL_POINTER_NUMBER                                           \
    (MAXIMUM_STACK_DEPTH - MAXIMUM_THREAD_POINTER_NUMBER)
#define LOCAL_POINTER_RESERVED (MAXIMUM_LOCAL_POINTER_NUMBER * FAT_POINTER_SIZE)

typedef struct
{
    size_t local_pointer_offset;
    size_t thread_pointer_offset;
    FatPointer *local_pointers;
    FatPointer *thread_pointers;
} ArenaStack;

extern _Thread_local ArenaStack arena_stack;

extern pthread_key_t arena_stack_key;
extern pthread_once_t arena_stack_once;

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
                if (arena_stack.thread_pointer_offset >                        \
                    MAXIMUM_THREAD_POINTER_NUMBER)                             \
                    abort();                                                   \
                arena_stack                                                    \
                    .thread_pointers[arena_stack.thread_pointer_offset++] =    \
                    arena_stack.local_pointers[lp_idx];                        \
            } else if (fat_pointer_escaped_stack_frame(                        \
                           arena_stack.local_pointers[lp_idx])) {              \
                arena_stack.local_pointers[prev_frame++] =                     \
                    arena_stack.local_pointers[lp_idx];                        \
                fat_pointer_catch(&(arena_stack.local_pointers[lp_idx]));      \
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
    munmap(arena_stack.thread_pointers, THREAD_POINTER_RESERVED);

    munmap(arena_stack.local_pointers, LOCAL_POINTER_RESERVED);

    arena_stack.local_pointers = NULL;
    arena_stack.thread_pointers = NULL;

    arena_stack.local_pointer_offset = 0;
    arena_stack.thread_pointer_offset = 0;
}

static inline void arena_stack_key_init(void)
{
    pthread_key_create(&arena_stack_key, arena_thread_exit);
}

static inline void arena_stack_init(void)
{
    static_assert(sizeof(FatPointer) == FAT_POINTER_SIZE,
                  "fat pointer type must be 16byte-sized");
    if (!(arena_stack.local_pointers || arena_stack.thread_pointers)) {
        arena_stack.local_pointer_offset = 0;
        arena_stack.thread_pointer_offset = 0;

        arena_stack.local_pointers =
            mmap(NULL, LOCAL_POINTER_RESERVED, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (arena_stack.local_pointers == MAP_FAILED)
            abort();

        arena_stack.thread_pointers =
            mmap(NULL, THREAD_POINTER_RESERVED, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (arena_stack.thread_pointers == MAP_FAILED)
            abort();

        pthread_once(&arena_stack_once, arena_stack_key_init);
        if (pthread_setspecific(arena_stack_key, &arena_stack) != 0) {
            munmap(arena_stack.thread_pointers, THREAD_POINTER_RESERVED);
            munmap(arena_stack.local_pointers, LOCAL_POINTER_RESERVED);
            abort();
        }
    }
}

#endif // STACK_H_
