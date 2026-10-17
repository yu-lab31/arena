#ifndef FAT_POINTER_H_
#define FAT_POINTER_H_

#include <stdint.h>
#define FAT_POINTER_SIZE 16
typedef struct
{
    void *addr;
    // bit map: high [ 7 6 5 4 3 2 1 0 ] low
    // 0: escaped from current stack frame
    // 1: escaped from current stack
    // 7: escaped from current thread
    // others: reserved
    uint8_t escape_flags;
    uint8_t reserved[7];
} FatPointer;

#define fat_pointer_escape_stack_frame(pfp)                                    \
    do {                                                                       \
        pfp->escape_flags |= 1;                                                \
    } while (0)

#define fat_pointer_escaped_stack_frame(fp) ((fp).escape_flags & 1)

#define fat_pointer_catch(pfp)                                                 \
    do {                                                                       \
        (pfp)->escape_flags &= ~((uint8_t)1);                                  \
    } while (0)

#define fat_pointer_escape_stack(pfp)                                          \
    do {                                                                       \
        pfp->escape_flags |= (1 << 1);                                         \
    } while (0)

#define fat_pointer_escaped_stack(fp) ((fp).escape_flags & (1 << 1))

#define fat_pointer_escape_thread(pfp)                                         \
    do {                                                                       \
        pfp->escape_flags |= (1 << 7);                                         \
    } while (0)

#define fat_pointer_escaped_thread(fp) ((fp).escape_flags & (1 << 7))

#endif // FAT_POINTER_H_
