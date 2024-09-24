#ifndef _COMMON_H
#define _COMMON_H

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

static inline u32 u32le(void *_p)
{
    u8 *p = _p;
    return (p[0] << 0)
         | (p[1] << 8)
         | (p[2] << 16)
         | (p[3] << 24);
}


typedef struct {
    u8 r, g, b;
} _Color;

static inline _Color color_15_to_24(u16 col)
{
    return (_Color) {
        .r = (0x1f & (col >> 0)) << 3,
        .g = (0x1f & (col >> 5)) << 3,
        .b = (0x1f & (col >> 10)) << 3,
    };
}

#endif