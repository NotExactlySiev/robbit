#ifndef _ROBBIT_H
#define _ROBBIT_H

#include "../common.h"
#include "../exact/archive.h"
#include "mesh.h"

#define ENE_MAX_OBJS    16
#define STAGE_MAX_GEOM  2048

#define CONTENTS                    \
    O(UNKNOWN, Unk, unk)            \
    O(CLUT, Clut, clut)             \
    O(TEXTURE, Texture, texture)    \
    O(MESH, Mesh, mesh)             \
    O(TILED, Tiled, tiled)          \


//typedef enum EarContent EarContent;
// TODO: these shouldn't be here...
typedef enum EarContent {
    EAR_CONTENT_NONE,       // for seperator
#define O(NAME, Name, name)     EAR_CONTENT_##NAME,
    CONTENTS
#undef O
} EarContent;

extern const char *content_strings[];

typedef struct {
    // doesn't have a node of its own, abstract object
    EarNode *clut_node;
    EarNode *bitmap_node;
} AlohaTexture;

typedef struct {
    EarNode *node;
    EarNode *clut_node;
    EarNode *mesh_nodes[2];
    AlohaTexture tex[2];    // clut ref is duplicated, who cares
} AlohaObjSet;  // TODO: MeshSet is better, object is when it's in a stage

typedef struct {
    EarNode *node;
} AlohaGeom;

typedef struct {
    EarNode *node;
    AlohaGeom geom;
} AlohaStage;

typedef struct {
    EarNode *node;
    AlohaObjSet objs;
    AlohaStage stage;
} AlohaLevel;

typedef struct {
    EarNode *node;
    AlohaTexture env[2];
    AlohaLevel levels[2];
    EarNode *unk_node;       // TODO: what is this?
} DatFile;

typedef struct {
    EarNode *node;
    int nobjs;
    AlohaObjSet objs[ENE_MAX_OBJS];
} EneFile;

// those get converted into these:

typedef struct {
    // holds the Image and all the possibly required subiamges
    uint n;
    Image images[32];
    VkImageView views[32];
} RobbitTexture;

typedef struct {
    u16 id;
    i16 x, y, z;
} GeomObj;

typedef struct {
    uint n;
    GeomObj objs[STAGE_MAX_GEOM];
} RobbitGeom;

typedef struct {
    RobbitGeom geom;
    // RobbitEntity[]
    // RobbitCollisions[]
    // decorations
    // the other thing
} RobbitStage;

typedef struct {
    RobbitMesh normal[128];
    RobbitMesh lod[128];
    RobbitTexture texture;
} RobbitObjSet;

typedef struct {
    RobbitObjSet objs;
    RobbitStage stage;
} RobbitLevel;

// aloha.c
void aloha_parse_dat(DatFile *out, EarNode *node);

// mesh.c
void convert_objset(RobbitObjSet *set, AlohaObjSet *src);
void destroy_objset(RobbitObjSet *set);
void dump_objset(RobbitObjSet *set);
void draw_mesh(VkCommandBuffer cbuf, RobbitMesh *mesh);

// level.c
void convert_level(RobbitLevel *dst, AlohaLevel *src);
void destroy_level(RobbitLevel *level);
void draw_level(VkCommandBuffer cbuf, Pipeline *pipe, RobbitLevel *level);
int find_first_offset(void *data, int offset_size);

#endif
