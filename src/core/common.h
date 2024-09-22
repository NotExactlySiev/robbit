#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../exact/archive.h"
#include "mesh.h"

#define ENE_MAX_OBJS    16

// TODO: bring the big endian inline functions here
#define READ_BE16(x,p)  { x = *p++ << 8; x |= *p++ << 0; }

#define CONTENTS                    \
    O(UNKNOWN, Unk, unk)            \
    O(CLUT, Clut, clut)             \
    O(TEXTURE, Texture, texture)    \
    O(MESH, Mesh, mesh)             \
    O(TILED, Tiled, tiled)          \
    O(ENTITY, Entity, entity)       \
    \
    O(STAGE, Stage, stage)       \



//typedef enum EarContent EarContent;
// TODO: these shouldn't be here...
typedef enum EarContent {
    EAR_CONTENT_NONE,       // for seperator
#define O(NAME, Name, name)     EAR_CONTENT_##NAME,
    CONTENTS
#undef O
} EarContent;

extern const char *content_strings[];

EarContent ear_node_guess_content(EarNode* node);

typedef struct AssetViewer AssetViewer;
// userdata struct allocated for each EarNode

/*
typedef struct {
    EarContent  content;
    // gui stuff
    bool show_viewer;
    AssetViewer *viewer;
} AlohaMetadata;
*/

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
} AlohaObjSet;

typedef struct {
    EarNode *node;
} AlohaGeom;

typedef struct {
    EarNode *node;
    AlohaGeom *geom;
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
} RobbitTexture;

typedef struct {
    u16 id;
    i16 x, y, z;
} GeomObj;

#define STAGE_MAX_GEOM  1024

typedef struct {
    GeomObj geom[STAGE_MAX_GEOM];
    // RobbitEntity[]
    // RobbitCollisions[]
    // decorations
    // the other thing
} RobbitStage;

typedef struct {
    RobbitMesh normal[128];
    RobbitMesh lod[128];
} RobbitObjSet;

typedef struct {
    RobbitObjSet objs;
    RobbitStage stage;
} RobbitLevel;


#endif
