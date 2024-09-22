#ifndef _LEVEL_H
#define _LEVEL_H

#include "robbit.h"

/*
typedef struct {
    short unk0, unk1;
    short x_a, x_b;
    short y_a, y_b;
    short z_a, z_b;
} Obj;

typedef struct {
    int count;
    Obj* ptrs[];
} Section3;

typedef struct {
    int w, h;
    short sizes[64][64];  // i don't think it's ever bigger than this
    void* ptrs[64][64]; // TODO: maybe make this array of structs so I can allocate less space?
} Tiled;

typedef struct {
    void* section0_header;
    Tiled section0;
    Tiled section1;
    Tiled section2;
    Section3* section3;
} Level;
*/

#endif