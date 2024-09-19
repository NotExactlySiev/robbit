// parsing and decoding level data
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "level.h"

#include <GL/gl.h>

/*
 * section0 is tiled but has a header (it's 0x34 bytes in SOU bonus level)
 * section1 is tiled
 * section2 is tiled
 * section3 is a list with lengths
 */

// tiled files (there are other types too)
// section2

int find_first_offset(void* data, int offset_size)
{
    uint8_t* p;
    uint32_t offset;    // biggest it could be is 32 bit (i think?)
    p = data;

    bool not_zero;

    not_zero = false;
    while (true) {
        offset = 0;
        for (int i = offset_size-1; i >= 0 ; i--)
            offset = (offset << 8) | p[i]; // little endian
        if (offset) {
            return offset;
        }
        p += offset_size;
    }
}


#define READ_LE(p,s)    \
    ({  uint8_t* __p = (uint8_t*) (p) ; int __d = 0;    \
        for (int __i = (s)-1; __i >= 0 ; __i--)   \
        __d = (__d << 8) | (__p)[__i];    \
        __d; })

Tiled* read_tiled_data(Tiled* ret, void* data, int offset_size)
{
    
    int off;
    int count;
    int width = offset_size == 4 ? 32 : 64;
    void* p = data;
    void* q;
    Thing* things;
    //Tiled* ret = malloc(sizeof(Tiled));

    ret->w = width;
    ret->h = width;

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < width; j++, p += offset_size) {
            ret->sizes[i][j] = 0;
            off = READ_LE(p, offset_size);

            if (off == 0) continue;
            q = data + off;
            //count = READ_LE(q, offset_size);
            count = *(short*) q;
            things = q + 2;

            ret->ptrs[i][j] = things;
            ret->sizes[i][j] = count;
            if (count != 0) 
                printf("%d - %d:\t%d\n", i, j, count);
            for (int k = 0; k < count; k++) {
                printf("\t%d:\t%d\t%d\t%d\n", things[k].id & 0x7FFF, things[k].x, things[k].y, things[k].z);
            }
        }
    }


    return ret;
}


/*
void aloha2glvert(Thing t)
{
    if (t.id == 8) {
        glColor3f(1.0, 0.0, 0.0);
        glVertex3f(i16tof(t.x), i16tof(t.y), i16tof(t.z));
    }
    else {
        glColor3f(1.0f, 1.0f, 1.0f);
        glVertex3f(i16tof(t.x), i16tof(t.y), i16tof(t.z));
    }
}*/



// for section 2 (geom) only. TODO: take out common code with other sections
int geom_count(void *data)
{
    // error checking? validation?
    int ret = 0;
    u16 *chunks = data;
    for (int i = 0; i < 64*64; i++) {
        u16 off = chunks[i];
        if (off == 0) continue;
        u16 *p = data + off;
        ret += *p;
    }
    return ret;
}

void geom_unpack(void *data, Thing *out)
{
    int n = 0;
    u16 *chunks = data;
    for (int i = 0; i < 64*64; i++) {
        u16 off = chunks[i];
        if (off == 0) continue;
        u16 *p = data + off;
        u16 count = *p;
        Thing *objs = p + 1;
        for (int j = 0; j < count; j++) {
            out[n++] = objs[j];
        }
    }
}

// idk what this contains exactly
Section3* read_section3(void* data)
{
    uint32_t* p = data;
    int count = *p++;
    int size;
    Obj* obj;
    Section3* ret; 
    
    ret = malloc(sizeof(Section3) + count * sizeof(Obj*));
    ret->count = count;

    for (int i = 0; i < count; i++) {
        size = *p++;
        if (size == 0) continue;
        obj = p;
        printf("%d:\t%d\t%d %d\t%d %d\t%d %d\n", i, size, 
            obj->x_a, obj->x_b,
            obj->y_a, obj->y_b,
            obj->z_a, obj->z_b
        );
        ret->ptrs[i] = obj;
        printf("set pointer for %d\n", i);
        p += size/4 - 1;
    }

    return ret;
}


Level* level_make(EarNode* node)
{
    void* section0 = node->sub[0].buf;
    void* section1 = node->sub[1].buf;
    void* section2 = node->sub[2].buf;
    void* section3 = node->sub[3].buf;

    Level level;

    read_tiled_data(&level.section1, section1, 4);
    read_tiled_data(&level.section2, section2, 2);
    level.section3 = read_section3(section3);

    int i,j;
    for (i = 0; i < 64; i++)
        for (j = 0; j < 64; j++) {
            if (level.section2.sizes[i][j] >= 4) goto out;
        }
out:
    printf("\n");
    int count = level.section2.sizes[i][j];
    Thing* things = level.section2.ptrs[i][j];
    Obj* obj;
    printf("%d things:\n", count);
    for (int k = 0; k < count; k++) {
        printf("objid: %d, ", things->id);
        obj = level.section3->ptrs[things->id];
        printf("y = %d\trange: %d - %d\t%d - %d\n", 
            things->y, 
            obj->y_a, obj->y_b,
            things->y + obj->y_a, things->y + obj->y_b
        );

        things++;
    }
}
