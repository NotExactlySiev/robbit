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

// section 1
typedef struct {
    u16 count;
    LoDetailObj objs[];
} OtherBlock;

static int geom_hi_count(void *data)
{
    // error checking? validation?
    int ret = 0;
    u32 *chunks = data;
    for (int i = 0; i < 32*32; i++) {
        u32 off = chunks[i];
        if (off == 0) continue;
        u16 *p = data + off;
        ret += *p;
    }
    return ret;
}

static void geom_hi_unpack(void *data, LoDetailObj *out)
{
    int n = 0;
    u32 *chunks = data;
    for (int i = 0; i < 32*32; i++) {
        u32 off = chunks[i];
        if (off == 0) continue;
        OtherBlock *p = data + off;
        u16 count = p->count;
        for (int j = 0; j < count; j++) {
            out[n++] = p->objs[j];
        }
    }
}

// for section 2 (geom) only. TODO: take out common code with other sections
static int geom_lo_count(void *data)
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

static void geom_lo_unpack(void *data, HiDetailObj *out)
{
    typedef struct {
        u16 count;
        HiDetailObj objs[];
    } ObjBlock;

    int n = 0;
    u16 *chunks = data;
    for (int i = 0; i < 64*64; i++) {
        u16 off = chunks[i];
        if (off == 0) continue;
        ObjBlock *p = data + off;
        u16 count = p->count;
        for (int j = 0; j < count; j++) {
            out[n++] = p->objs[j];
        }
    }
}

void convert_geom_hi(RobbitGeomLo *dst, AlohaGeomHi *src)
{
    int nobjs = geom_hi_count(src->node->buf);
    assert(nobjs <= STAGE_MAX_GEOM);
    dst->n = nobjs;
    geom_hi_unpack(src->node->buf, dst->objs);
}

void convert_geom_lo(RobbitGeomHi *dst, AlohaGeomLo *src)
{
    int nobjs = geom_lo_count(src->node->buf);
    assert(nobjs <= STAGE_MAX_GEOM);
    dst->n = nobjs;
    geom_lo_unpack(src->node->buf, dst->objs);
}

void destroy_level(RobbitLevel *level)
{
    destroy_objset(&level->objs);
    //destroy_stage(level->stage);  // Not needed
}


void convert_entities(RobbitEntities *dst, EarNode *src)
{
    // skip the header
    u8 *p = src->buf;
    printf("header is %d\n", u16le(p));
    p += u16le(p) + 4;

    int n = 0;
    u32 *chunks = (u32*) p;
    for (int i = 0; i < 32*32; i++) {
        u32 off = chunks[i];
        if (off == 0) continue;
        u16 count = *(u16*)(p + off);
        RobbitEntity *e = p + off + 2;
        for (int j = 0; j < count; j++) {
            dst->ents[n++] = e[j];
        }
    }
    dst->n = n;
}

void convert_stage(RobbitStage *dst, AlohaStage *src)
{
    // TODO: other parts
    convert_entities(&dst->ent, src->entities_node);
    convert_geom_hi(&dst->geom_hi, &src->geom_hi);
    convert_geom_lo(&dst->geom_lo, &src->geom_lo);
    // section 3
    // section 4 (?)
}

void convert_level(RobbitLevel *dst, AlohaLevel *src)
{
    convert_objset(&dst->objs, &src->objs);
    convert_stage(&dst->stage, &src->stage);
}

static RobbitVertex ground[6] = {
    
    { .pos = {  INT16_MAX, 1, -INT16_MAX }, .col = 0x7FF0, .u = 255, .v = 0 },
    { .pos = {  INT16_MAX, 1,  INT16_MAX }, .col = 0x7FF0, .u = 255, .v = 255 },
    { .pos = { -INT16_MAX, 1, -INT16_MAX }, .col = 0x7FF0, .u = 0, .v = 0 },

    { .pos = { -INT16_MAX, 1, -INT16_MAX }, .col = 0x7FF0, .u = 0, .v = 0 },
    { .pos = {  INT16_MAX, 1,  INT16_MAX }, .col = 0x7FF0, .u = 255, .v = 255 },
    { .pos = { -INT16_MAX, 1,  INT16_MAX }, .col = 0x7FF0, .u = 0, .v = 255 },
};

void draw_level(VkCommandBuffer cbuf, Pipeline *pipe, RobbitLevel *level)
{
    static bool ground_set = false;
    static Buffer ground_verts;
    
    if (!ground_set) {
        // this is just horrible
        for (int i = 0; i < 6; i++) {
            ground[i].tex = true;
            ground[i].texid = 0;
            ground[i].texwin = (Rect8) { 0, 0, 256, 256 };
        }

        ground_verts = create_buffer(sizeof(ground), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buffer_write(ground_verts, sizeof(ground), ground);
        ground_set = true;
    }

    push_const(cbuf, pipe, (PushConst) { 0, 0, 0 });
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cbuf, 0, 1, &ground_verts.vk, &offset);
    vkCmdDraw(cbuf, 6, 1, 0, 0);

    int total_tri_count = 0;
    for (int i = 0; i < level->stage.geom_hi.n; i++) {
        LoDetailObj *obj = &level->stage.geom_hi.objs[i];
        if (level->objs.lod[0][obj->id].empty) continue;
        ZONE(LevelObject)
        push_const(cbuf, pipe, (PushConst) {
            .x = ((float) obj->x) / INT16_MAX,
            .y = ((float) obj->y) / INT16_MAX,
            .z = ((float) obj->z) / INT16_MAX,
        });
        /*for (int j = 0; j < level->objs.nlod; j++) {
            if (!level->objs.lod[j][obj->id].empty) {
                draw_mesh(cbuf, &level->objs.lod[j][obj->id]);
                break;
            }
        }*/
        total_tri_count += level->objs.lod[0][obj->id].vert_count / 3;
        draw_mesh(cbuf, &level->objs.lod[0][obj->id]);
        UNZONE(LevelObject)
    }
    printf("%d tris\n", total_tri_count);

    for (int i = 0; i < level->stage.ent.n; i++) {
        RobbitEntity *e = &level->stage.ent.ents[i];
        ZONE(LevelEntity)
        push_const(cbuf, pipe, (PushConst) {
            .x = ((float) e->x) / INT16_MAX,
            .y = ((float) e->y) / INT16_MAX,
            .z = ((float) e->z) / INT16_MAX,
        });
        draw_mesh(cbuf, &level->objs.lod[0][63]);
        UNZONE(LevelEntity)

    }
}