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

// ----- HEAP START -----

// HEAP SPACE: 112G

#define ARENA_BLOCK_SZ ((size_t)1 << 18) // 256K, 1/128 of which is header
#define ARENA_BLOCK_HEADER_SZ (ARENA_BLOCK_SZ >> 7) // 2K
#define ARENA_BLOCK_CAP (ARENA_BLOCK_SZ - ARENA_BLOCK_HEADER_SZ)

// Segment for objects smaller than capacity of one block and is local to their
// owners.
#define ARENA_HEAP_NORMAL_SZ (((size_t)1 << 35) + ((size_t)1 << 34)) // 48G
#define ARENA_HEAP_NORMAL_HEADER_SZ (ARENA_BLOCK_SZ << 1)
#define ARENA_HEAP_NORMAL_BLOCK_NUM                                            \
    ((ARENA_HEAP_NORMAL_SZ - ARENA_HEAP_NORMAL_HEADER_SZ) / ARENA_BLOCK_SZ)

// Segment for objects bigger than capacity of one block and is local to their
// owners.
#define ARENA_HEAP_HUGE_SZ ((size_t)1 << 35)                              // 32G
#define ARENA_HEAP_HUGE_HEADER_SZ (((size_t)1 << 22) + ((size_t)1 << 21)) // 6M
#define ARENA_HEAP_HUGE_CAP (ARENA_HEAP_HUGE_SZ - ARENA_HEAP_HUGE_HEADER_SZ)
#define ARENA_HEAP_HUGE_OBJECT_NUM (ARENA_HEAP_HUGE_CAP / ARENA_BLOCK_CAP)
#define ARENA_HUGE_OBJECT_HEADER_SZ (32)

// Segment for shared objects
#define ARENA_HEAP_SHARED_SZ ((size_t)1 << 35) // 32G

#define ARENA_SLOT_SZ (ARENA_HEAP_SHARED_SZ / MAXIMUM_THREAD_NUM) // 8M

#define ARENA_SHARED_CHUNK_SZ ((size_t)1 << 15) // 32K
#define ARENA_SC_PER_SLOT (ARENA_SLOT_SZ / ARENA_SHARED_CHUNK_SZ)

// ----- HEAP   END -----

#endif // ARENA_LAYOUT_H_
