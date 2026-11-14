#ifndef ARENA_ALLOC_H_
#define ARENA_ALLOC_H_

#include <assert.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
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
        ARENA_NAME_REGION(((void *)(shared + i * ARENA_SLOT_SZ)),
                          ARENA_SLOT_SZ / ARENA_PAGESZ(),
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

static inline ArenaHeapNormal *locate_heap_normal(void)
{
    return (ArenaHeapNormal *)((size_t)arena_memory + STACK_CAPACITY);
}

static inline ArenaHeapHuge *locate_heap_huge(void)
{
    return (ArenaHeapHuge *)((size_t)locate_heap_normal() +
                             ARENA_HEAP_NORMAL_SZ);
}

static inline size_t arena_alloc_block(int16_t tid)
{
    ArenaHeapNormal *seg = locate_heap_normal();
    size_t i;

    pthread_mutex_lock(&(seg->header.allock));

    for (i = MAXIMUM_THREAD_NUM + 1; i < ARENA_HEAP_NORMAL_BLOCK_NUM; i++)
        if (!(seg->header.owners[i])) {
            seg->header.owners[i] = tid;
            break;
        }

    pthread_mutex_unlock(&(seg->header.allock));

    return i < ARENA_HEAP_NORMAL_BLOCK_NUM ? i : 0;
}

static inline size_t arena_locate_block(void *ptr)
{
    ArenaHeapNormal *seg = locate_heap_normal();
    return ((size_t)ptr - (size_t)seg->blocks) & ~(ARENA_BLOCK_SZ - 1);
}

static inline void *arena_alloc_normal(size_t size, int16_t tid, size_t bid)
{
    if (size > ARENA_BLOCK_CAP)
        return NULL;

    ArenaHeapNormal *seg = locate_heap_normal();

    ArenaBlock *block = &(seg->blocks[bid]);

    size_t aligned = (size + 15) / _Alignof(max_align_t);
    aligned = aligned ? aligned : 1; // support 0-byte calls
    size_t beg = 0, end;

    while (beg <= ARENA_BLOCK_CAP / _Alignof(max_align_t) - aligned) {
        if (ARENA_OCCU_FLAG(block->header.occu_flags, beg)) {
            beg++;
            continue;
        }

        for (end = beg + 1; end < ARENA_BLOCK_CAP / _Alignof(max_align_t) &&
                            !ARENA_OCCU_FLAG(block->header.occu_flags, end);
             end++);

        if (end - beg < aligned) {
            beg = end + 1;
            continue;
        }
        break;
    }

    if (beg <= ARENA_BLOCK_CAP / _Alignof(max_align_t) - aligned) {
        ARENA_OCCU_RANGE(block->header.occu_flags, beg, aligned);
        return &(block->data[beg * _Alignof(max_align_t)]);
    }

    if (block->header.next != 0) {
        return arena_alloc_normal(size, tid, block->header.next);
    }

    size_t next = arena_alloc_block(tid);
    if (!next) {
        return NULL;
    }

    block->header.next = next;
    return arena_alloc_normal(size, tid, next);
}

static inline void arena_free_normal(void *ptr, size_t size)
{
    ArenaHeapNormal *seg = locate_heap_normal();
    size_t bid = arena_locate_block(ptr);

    ArenaBlock *block = &(seg->blocks[bid]);
    size_t beg = ((size_t)ptr - (size_t)block->data) / _Alignof(max_align_t);

    if (!ARENA_OCCU_FLAG(block->header.occu_flags, beg)) {
        return;
    }

    size_t aligned = (size + 15) / _Alignof(max_align_t);
    aligned = aligned ? aligned : 1; // support 0-byte calls

    if (beg <= ARENA_BLOCK_CAP / _Alignof(max_align_t) - aligned) {
        ARENA_UNOCCU_RANGE(block->header.occu_flags, beg, aligned);
    }
}

static inline void allock(void)
{
    ArenaHeapNormal *seg = locate_heap_normal();
    pthread_mutex_lock(&(seg->header.allock));
}

static inline void alunlock(void)
{
    ArenaHeapNormal *seg = locate_heap_normal();
    pthread_mutex_unlock(&(seg->header.allock));
}

#define ARENA_LAST_FRAG_SZ(last)                                               \
    ((size_t)locate_heap_huge() + ARENA_HEAP_HUGE_SZ -                         \
     ((size_t)last->addr - ARENA_HUGE_OBJECT_HEADER_SZ + last->cap))
#define ARENA_FIRST_FRAG_SZ(first)                                             \
    ((size_t)first->addr -                                                     \
     ((size_t)locate_heap_huge() + ARENA_HEAP_HUGE_HEADER_SZ))

#define ARENA_RET_ADDR(addr)                                                   \
    do {                                                                       \
        meta = addr ? (void *)((size_t)addr - ARENA_HUGE_OBJECT_HEADER_SZ)     \
                    : NULL;                                                    \
        res = addr;                                                            \
        goto eocf;                                                             \
    } while (0)

static inline void *arena_alloc_huge(size_t size)
{
    if (size > ARENA_HEAP_HUGE_CAP - ARENA_HUGE_OBJECT_HEADER_SZ)
        return NULL;

    ArenaHeapHuge *seg = locate_heap_huge();

    static_assert(sizeof(ArenaHugeObject) == ARENA_HUGE_OBJECT_HEADER_SZ,
                  "bad aligned huge object");

    size_t block_needed = (size + ARENA_HUGE_OBJECT_HEADER_SZ) / ARENA_BLOCK_SZ;
    // allocate more space to avoid reallocate frequently
    if (block_needed <= 1) {
        block_needed = 2;
    } else if (size <= ((size_t)1 << 22)) {
        block_needed <<= 1;
    } else {
        block_needed += block_needed >> 2;
    }
    size_t size_aligned = block_needed * ARENA_BLOCK_SZ;

    if (size_aligned > ARENA_HEAP_HUGE_CAP) {
        return NULL;
    }

    void *res = NULL;
    ArenaHugeObject *meta = NULL;

    allock();

    ArenaHugeObject *beg = &(seg->header.objects[seg->header.first]);
    ArenaHugeObject *end = &(seg->header.objects[seg->header.last]);

    size_t bid;
    for (bid = 0; bid < ARENA_HEAP_HUGE_OBJECT_NUM; bid++)
        if (ARENA_HUGE_OBJ_EMPTY(seg->header.objects[bid]))
            break;
    ArenaHugeObject *cur = &(seg->header.objects[bid]);

    if (ARENA_HUGE_OBJ_EMPTY((*beg))) {
        cur->addr = (void *)((size_t)seg + ARENA_HEAP_HUGE_HEADER_SZ +
                             ARENA_HUGE_OBJECT_HEADER_SZ);
        cur->cap = size_aligned;
        seg->header.first = seg->header.last = bid;
        ARENA_RET_ADDR(cur->addr);
    }

    if (size_aligned <= ARENA_LAST_FRAG_SZ(end)) {
        cur->addr = (void *)((size_t)end->addr + end->cap);
        cur->cap = size_aligned;

        end->next = bid;
        cur->prev = seg->header.last;

        seg->header.last = bid;

        ARENA_RET_ADDR(cur->addr);
    }

    if (size_aligned <= ARENA_FIRST_FRAG_SZ(beg)) {
        cur->addr = (void *)((size_t)seg + ARENA_HEAP_HUGE_HEADER_SZ +
                             ARENA_HUGE_OBJECT_HEADER_SZ);
        cur->cap = size_aligned;

        cur->next = seg->header.first;
        beg->prev = bid;

        seg->header.first = bid;

        ARENA_RET_ADDR(cur->addr);
    }

    while (beg != end) {
        ArenaHugeObject *next = &(seg->header.objects[beg->next]);
        size_t frag_sz =
            (size_t)next->addr -
            ((size_t)beg->addr - ARENA_HUGE_OBJECT_HEADER_SZ + beg->cap);

        if (size_aligned <= frag_sz) {
            cur->addr = (void *)((size_t)beg->addr + beg->cap);
            cur->cap = size_aligned;
            cur->prev = next->prev;
            cur->next = beg->next;

            beg->next = bid;
            next->prev = bid;
            ARENA_RET_ADDR(cur->addr);
        }

        beg = next;
    }

eocf:
    alunlock();
    if (meta) {
        meta->addr = res;
        meta->cap = size_aligned;
        meta->next = meta->prev = bid;
    }
    return res;
}

#endif // ARENA_ALLOC_H_
