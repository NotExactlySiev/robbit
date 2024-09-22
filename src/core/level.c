// parsing and decoding level data
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "robbit.h"

// section0 is tiled but has a header (it's 0x34 bytes in SOU bonus level)
// section1 is tiled
// section2 is tiled
// section3 is a list with lengths

// TODO: do we even need to analyze the binaries anymore? the top down approach
// works much better for figuring out what's where
int find_first_offset(void *data, int offset_size)
{
    uint8_t* p;
    uint32_t offset;    // biggest it could be is 32 bit (i think?)
    p = data;

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

// for section 2 (geom) only. TODO: take out common code with other sections
static int geom_count(void *data)
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

static void geom_unpack(void *data, GeomObj *out)
{
    typedef struct {
        u16 count;
        GeomObj objs[];
    } ObjBlock;

    int n = 0;
    u16 *chunks = data;
    for (int i = 0; i < 64*64; i++) {
        u16 off = chunks[i];
        if (off == 0) continue;
        ObjBlock *p = data + off;
        u16 count = p->count;
        //GeomObj *objs = p + 1;
        for (int j = 0; j < count; j++) {
            out[n++] = p->objs[j];
        }
    }
}

void convert_geom(RobbitGeom *dst, AlohaGeom *src)
{
    int nobjs = geom_count(src->node->buf);
    assert (nobjs <= STAGE_MAX_GEOM);
    dst->n = nobjs;
    printf("%d objects\n", nobjs);
    geom_unpack(src->node->buf, dst->objs);
}

void destroy_level(RobbitLevel *level)
{
    destroy_objset(&level->objs);
    //destroy_stage(level->stage);  // Not needed
}

void convert_stage(RobbitStage *dst, AlohaStage *src)
{
    convert_geom(&dst->geom, &src->geom);
    // TODO: other parts
}

void convert_level(RobbitLevel *dst, AlohaLevel *src)
{
    convert_objset(&dst->objs, &src->objs);
    convert_stage(&dst->stage, &src->stage);
}

void draw_level(VkCommandBuffer cbuf, Pipeline *pipe, RobbitLevel *level)
{
    for (int i = 0; i < level->stage.geom.n; i++) {
        GeomObj *obj = &level->stage.geom.objs[i];
        if (level->objs.lod[obj->id].empty) continue;
        push_const(cbuf, pipe, (PushConst) {
            .x = ((float) obj->x) / INT16_MAX,
            .y = ((float) obj->y) / INT16_MAX,
            .z = ((float) obj->z) / INT16_MAX,
        });
        draw_mesh(cbuf, &level->objs.lod[obj->id]);
    }
}