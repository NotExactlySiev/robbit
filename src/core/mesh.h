#ifndef _MESH_H
#define _MESH_H

#include "../vulkan/vulkan.h"

// These are the data structs as found in the file
typedef struct {
    i16 x;
    i16 y;
    i16 z;
    i16 pad;    // is always zero
} AlohaVertex;

typedef struct {
    // vertex indices
    // should be divided by 3
    u8 v0;
    u8 v1;
    u8 v2;
    u8 v3; // can be something else in tris. TODO: what?

    // flags and color. TODO: use a bitmap?
    u16 flags0;
    u16 flags1;

    // normal vector
    i16 nv0, nv1, nv2, nv3;

    // texture coordinates (if textured)
    u8 tu0, tv0;
    u8 tu1, tv1;
    u8 tu2, tv2;
    u8 tu3, tv3;

    // TODO: unknown
    u32 texwindow;
} AlohaFace;

// obscured public struct
typedef struct {
    void *head;   // start of the mesh
    bool empty;
    
    u32 nverts;
    u32 nunks;
    u32 ngroups;

    AlohaVertex *verts;
    void *faces;  // pointer to the start of first group

    // TODO: split this in two?

    Buffer vert_buffer;
    u32 vert_count;
} RobbitMesh;

// TODO: clean this up
typedef struct {
    u8 r, g, b;
} _Color;

typedef struct {
    u16 x, y, z, pad;
} NormalVec;

typedef struct {
    AlohaVertex pos;
    NormalVec normal;
    u8 tex, texid;
    u16 u, v;
    _Color col;
} RobbitVertex;

#endif
