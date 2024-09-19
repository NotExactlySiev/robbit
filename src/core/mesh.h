#ifndef _MESH_H
#define _MESH_H

#include <types.h>

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
    u16 nv0, nv1, nv2, nv3;

    // texture coordinates (if textured)
    u8 tu0, tv0;
    u8 tu1, tv1;
    u8 tu2, tv2;
    u8 tu3, tv3;

    // TODO: unknown
    u32 unk;
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
} AlohaMesh;

#endif
