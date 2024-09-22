#include <types.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static inline void die(const char *msg)
{
    printf("FATAL: %s\n", msg);
    exit(EXIT_FAILURE);
}

static inline u16 u16be(void *_p)
{
    u8 *p = _p;
    return (p[0] << 8)
         | (p[1] << 0);
}

static inline u32 u32be(void *_p)
{
    u8 *p = _p;
    return (p[0] << 24)
         | (p[1] << 16)
         | (p[2] << 8)
         | (p[3] << 0);
}