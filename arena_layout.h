#ifndef ARENA_LAYOUT_H_
#define ARENA_LAYOUT_H_

#include <stddef.h>
#include <unistd.h>

#include "fat_pointer.h"

#define ARENA_PAGESZ() ((size_t)sysconf(_SC_PAGESIZE))

#ifdef __linux__

#include <stdio.h>
#include <sys/prctl.h>

#define ARENA_REGION_NAME_CAP 80
#define ARENA_NAME_REGION(addr, pgnum, fmt, ...)                               \
    do {                                                                       \
        char nambuf[ARENA_REGION_NAME_CAP];                                    \
        snprintf(nambuf, ARENA_REGION_NAME_CAP, fmt, ##__VA_ARGS__);           \
        arena_name_region(addr, (pgnum) * ARENA_PAGESZ(), nambuf);             \
    } while (0)

static inline void arena_name_region(void *addr, size_t len, const char *name)
{
    prctl(PR_SET_VMA, PR_SET_VMA_ANON_NAME, (unsigned long)addr, len,
          (unsigned long)name);
}

#else

#define ARENA_NAME_REGION(addr, pgnum, fmt, ...)                               \
    do {                                                                       \
    } while (0)

#endif // __linux__

#define MAXIMUM_THREAD_NUM ((size_t)1 << 12) // 4K

#define ARENA_MEM_SZ ((size_t)1 << 37) // 128G

#define STACK_CAPACITY ((size_t)1 << 34)                        // 16G
#define TH_STACK_CAPACITY (STACK_CAPACITY / MAXIMUM_THREAD_NUM) // 4M
#define TH_STACK_HEADER_SZ ((size_t)64)                         // 64Byte

#define MAXIMUM_STACK_DEPTH                                                    \
    ((TH_STACK_CAPACITY - TH_STACK_HEADER_SZ) / FAT_POINTER_SIZE)
// One quarter of the stack is reserved for thread pointers.
#define MAXIMUM_THREAD_POINTER_NUMBER (MAXIMUM_STACK_DEPTH >> 2)
#define THREAD_POINTER_RESERVED                                                \
    (MAXIMUM_THREAD_POINTER_NUMBER * FAT_POINTER_SIZE)
// Other 3/4 is reserved for local pointers.
#define MAXIMUM_LOCAL_POINTER_NUMBER                                           \
    (MAXIMUM_STACK_DEPTH - MAXIMUM_THREAD_POINTER_NUMBER)
#define LOCAL_POINTER_RESERVED (MAXIMUM_LOCAL_POINTER_NUMBER * FAT_POINTER_SIZE)

#endif // ARENA_LAYOUT_H_
