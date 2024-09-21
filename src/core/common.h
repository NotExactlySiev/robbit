#ifndef _COMMON_H
#define _COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "../exact/archive.h"
//#include <SDL.h>
//#include "image.h"
#include "mesh.h"


#define READ_BE32(x,p)  { (x) = *p++ << 24; x |= *p++ << 16; x |= *p++ << 8; x |= *p++ << 0; }
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

typedef struct {
    EarContent  content;
    // gui stuff
    bool show_viewer;
    AssetViewer *viewer;
} AlohaMetadata;

typedef struct {
    EarNode *clut_node;
    EarNode *bitmap_node;
} AlohaTexture;

typedef struct {
    EarNode *clut_node;
    EarNode *mesh_nodes[2];
    AlohaTexture tex[2];    // clut ref is duplicated, who cares

    EarNode *node;
} AlohaObjSet;

typedef struct {
    EarNode *geom_node;

    EarNode *node;
} AlohaStage;

typedef struct {
    AlohaObjSet objs;
    AlohaStage stage;
} AlohaLevel;

typedef struct {
    AlohaTexture env[2];
    AlohaLevel levels[2];
    EarNode *unk_node;       // TODO: what is this?

    EarNode *node;
} DatFile;

#define ENE_MAX_OBJS    16

typedef struct {
    int nobjs;
    AlohaObjSet objs[ENE_MAX_OBJS];
} EneFile;

// these should be in aloha.h?
AlohaMetadata *aloha(EarNode *node);

// basic types

/*
typedef struct {
    EarNode *section0;
    EarNode *section1;
    EarNode *section2;
    EarNode *section3;
} Level;*/

#endif
