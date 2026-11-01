#ifndef ARENA_ALLOC_H_
#define ARENA_ALLOC_H_

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "arena_heap.h"
#include "arena_layout.h"

extern void *arena_memory;

static inline void arena_stack_format(void)
{
    size_t stack = (size_t)arena_memory;
    for (size_t i = 0; i < MAXIMUM_THREAD_NUM; i++) {
        ARENA_NAME_REGION(((void *)(stack + i * TH_STACK_CAPACITY)),
                          TH_STACK_CAPACITY / ARENA_PAGESZ(),
                          "arena_th%zu_stk_resv", i);
    }
}

static inline void arena_heap_format(void)
{
    static_assert(ARENA_BLOCK_SZ == sizeof(ArenaBlock),
                  "unexpected block size");

    size_t heap = (size_t)arena_memory + STACK_CAPACITY;

    static_assert(ARENA_HEAP_NORMAL_SZ == sizeof(ArenaHeapNormal),
                  "bad aligned normal heap");
    size_t normal = heap;
    ARENA_NAME_REGION(((void *)normal), ARENA_HEAP_NORMAL_SZ / ARENA_PAGESZ(),
                      "arena_heap_normal_resv");
    ArenaHeapNormal *space_normal = (void *)normal;
    pthread_mutex_init(&(space_normal->header.allock), NULL);

    static_assert(ARENA_HEAP_HUGE_SZ == sizeof(ArenaHeapHuge),
                  "bad aligned huge heap");
    size_t huge = normal + ARENA_HEAP_NORMAL_SZ;
    ARENA_NAME_REGION(((void *)huge), ARENA_HEAP_HUGE_SZ / ARENA_PAGESZ(),
                      "arena_heap_huge_resv");

    static_assert(ARENA_SLOT_SZ == sizeof(ArenaSlot), "bad aligned slot");
    static_assert(ARENA_HEAP_SHARED_SZ == sizeof(ArenaHeapShared),
                  "bad aligned shared heap");
    size_t shared = huge + ARENA_HEAP_HUGE_SZ;
    for (size_t i = 0; i < MAXIMUM_THREAD_NUM; i++) {
        ARENA_NAME_REGION(((void *)(shared + i * ARENA_SLOT_SZ)), ARENA_SLOT_SZ,
                          "arena_th%zu_hpsh_resv", i);
    }
}

static inline void arena_mem_format(void)
{
    arena_memory = mmap(NULL, ARENA_MEM_SZ, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
    if (arena_memory == MAP_FAILED)
        abort();

    arena_stack_format();
    arena_heap_format();
}

static inline void *locate_stack(void) { return arena_memory; }

#endif // ARENA_ALLOC_H_
