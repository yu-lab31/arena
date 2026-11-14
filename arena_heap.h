#ifndef ARENA_HEAP_H_
#define ARENA_HEAP_H_

#include <pthread.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "arena_layout.h"

typedef struct
{
    uint8_t occu_flags[ARENA_BLOCK_CAP >> 3 >> 4]; // 16Byte Aligned
    size_t next;
} ArenaBlockHeader;

#define ARENA_OCCU_FLAG(flags, idx)                                            \
    (flags[(size_t)idx >> 3] & ((uint8_t)1 << ((size_t)idx & (size_t)7)))

#define ARENA_OCCU_RANGE(flags, beg, len)                                      \
    do {                                                                       \
        for (size_t i = beg; i < beg + len; i++) {                             \
            uint8_t *f = &(flags[i >> 3]);                                     \
            if (!(i & (size_t)7) && i + 7 < beg + len) {                       \
                *f = 255;                                                      \
                i += 7;                                                        \
            } else {                                                           \
                *f |= (uint8_t)1 << (i & (size_t)7);                           \
            }                                                                  \
        }                                                                      \
    } while (0)

#define ARENA_UNOCCU_RANGE(flags, beg, len)                                    \
    do {                                                                       \
        for (size_t i = beg; i < beg + len; i++) {                             \
            uint8_t *f = &(flags[i >> 3]);                                     \
            if (!(i & (size_t)7) && i + 7 < beg + len) {                       \
                *f = 0;                                                        \
                i += 7;                                                        \
            } else {                                                           \
                *f &= (~((uint8_t)1)) << (i & (size_t)7);                      \
            }                                                                  \
        }                                                                      \
    } while (0)

typedef struct
{
    ArenaBlockHeader header;
    uint8_t reserved[ARENA_BLOCK_HEADER_SZ - sizeof(ArenaBlockHeader)];
    uint8_t data[ARENA_BLOCK_CAP];
} ArenaBlock;

typedef struct
{
    int16_t owners[ARENA_HEAP_NORMAL_BLOCK_NUM];
    pthread_mutex_t allock;
} ArenaHeapNormalHeader;

typedef struct
{
    ArenaHeapNormalHeader header;
    uint8_t
        reserved[ARENA_HEAP_NORMAL_HEADER_SZ - sizeof(ArenaHeapNormalHeader)];
    ArenaBlock blocks[ARENA_HEAP_NORMAL_BLOCK_NUM];
} ArenaHeapNormal;

typedef struct
{
    void *addr;
    size_t cap;
    size_t next;
    size_t prev;
} ArenaHugeObject;

#define ARENA_HUGE_OBJ_EMPTY(obj) (!(obj.addr || obj.cap))

typedef struct
{
    ArenaHugeObject objects[ARENA_HEAP_HUGE_OBJECT_NUM];
    size_t first;
    size_t last;
} ArenaHeapHugeHeader;

typedef struct
{
    ArenaHeapHugeHeader header;
    uint8_t reserved[ARENA_HEAP_HUGE_HEADER_SZ - sizeof(ArenaHeapHugeHeader)];
    uint8_t data[ARENA_HEAP_HUGE_CAP];
} ArenaHeapHuge;

typedef struct
{
    atomic_int_fast16_t refc[ARENA_SC_PER_SLOT];
} ArenaSlotHeader;

typedef struct
{
    ArenaSlotHeader header; // first chunk for header
    uint8_t reserved[ARENA_SHARED_CHUNK_SZ - sizeof(ArenaSlotHeader)];
    uint8_t data[ARENA_SC_PER_SLOT - 1][ARENA_SHARED_CHUNK_SZ];
} ArenaSlot;

typedef ArenaSlot ArenaHeapShared[ARENA_HEAP_SHARED_SZ / ARENA_SLOT_SZ];

#endif // ARENA_HEAP_H_
