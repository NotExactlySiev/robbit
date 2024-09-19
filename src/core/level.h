#ifndef _LEVEL_H
#define _LEVEL_H

#include "common.h"
//#include "image.h"

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
    uint16_t id;
    short x,y,z;
} Thing;

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

Level* level_make(EarNode* node);
int find_first_offset(void* data, int offset_size);
Tiled* read_tiled_data(Tiled*, void* data, int offset_size);
//Tiled* read_tiled_data16(void* data);
Tiled* read_tiled_data16(void *data, void (*draw_cb)(int));
//Image* tiled_to_image(Tiled* tiled);

#endif