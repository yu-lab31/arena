#ifndef ARENA_ALLOC_H_
#define ARENA_ALLOC_H_

#include <stddef.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "arena_layout.h"

extern void *arena_memory;

static inline void arena_stack_format(void)
{
    size_t stack = (size_t)arena_memory;
    for (size_t i = 0; i < MAXIMUM_THREAD_NUM; i++) {
        ARENA_NAME_REGION(((void *)(stack + i * TH_STACK_CAPACITY)),
                          TH_STACK_CAPACITY / ARENA_PAGESZ(),
                          "arena_th%zu_resv", i);
    }
}

static inline void arena_mem_format(void)
{
    arena_memory = mmap(NULL, ARENA_MEM_SZ, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE | MAP_NORESERVE, -1, 0);
    if (arena_memory == MAP_FAILED)
        abort();

    arena_stack_format();
}

static inline void *locate_stack(void) { return arena_memory; }

#endif // ARENA_ALLOC_H_
