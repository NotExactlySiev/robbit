#include "../exact/archive.h"
#include "robbit.h"

// Layer 2, node content types and relations
// these are still immutable and are calculated only once
// when the file is loaded.

// this file gets the tree and adds metadata / content types
// we assume the type of a node never changes after the initial scan

// the hierarchical structure of the archive itself is making the parsed data
// hard to work with. this layer should forego that hierarchy and create its
// own tree that makes sense to the program.
// for example, a _DAT file archive tree looks like this:
//  _DAT.EAR
//      CLUT        \ environment texture (opt, or 2 seps)
//      TEX         /
//      CLUT        \ environment texture (opt, or 2 seps)
//      TEX         /
//      OBJSET      main level objects
//          CLUT    mesh clut
//          CLUT    texture clut (opt, or sep)
//          MESH    normal meshes
//          MESH    lod meshes (opt)
//          ---
//          TEX     (opt)
//          TEX     (opt)
//          ---
//      OBJSET      bonus level objects (opt)
//          CLUT    mesh clut
//          CLUT    texture clut (opt, or sep)
//          MESH    normal meshes
//          MESH    lod meshes (opt)
//          ---
//          TEX     (opt)
//          TEX     (opt)
//          ---
//      ---
//      STAGE       main level
//          ???
//          ???
//          GEOM    level geometry
//          ???
//          ???     (opt, or sep)
//      STAGE       bonus level
//          ???
//          ???
//          GEOM    level geometry
//          ???
//          ???     (opt, or sep)
//      ---
//      ???         who knows what this is
//
// TODO: document all repeating structures and define everything in a neatly
//        organized, nested fashion.
//
// but here's how we want the same data to be organized:
//  
//  dat_file
//      env_textures[2]         // either or both could be NULL
//          clut_node
//          bitmap_node
//      levels[2]               // bonus might not exist
//          objects
//              mesh_clut
//              meshes[128]
//              meshes_lod[128]
//              textures[2]     // 0, 1 or 2
//                  clut_node
//                  bitmap_node
//          stage
//
// doesn't follow the file structure exactly. this API needs to cover the
// inconsistencies of how the EAR format is used.
// also: content tagging and parsing the structure are two different things (?)
// we can tag the content of each node pretty much perfectly in isolation.
// (can we?) all this complexity has arisen because I tried to do both of these
// at the same time and keep things in the same structure as the archive.
// the better way of doing this is just:
// 1- tag the binary files (not the directories)
// 2- go over the entire thing and generate our own tree with our types

// first the raw archive tree of EarNodes gets converted to a more organized
// aloha tree using the parse functions in this module, and then those objects
// get converted to their robbit counterparts that hold more information needed
// by the program to render and manipulate them.
// in other words: this module only LOCATES things, it doesn't convert them.

// guess the content of a NODE_TYPE_FILE
EarContent guess_content_base(EarNode *node)
{
    if (node->size == 0x200) return EAR_CONTENT_CLUT;
    
    // check if it could be a texture
    uint8_t* q;
    uint16_t w, h;
    q = node->buf + 4;
    w = u16be(q);
    h = u16be(q + 2);
    q += 4;
    if (w*h + 8 == node->size) return EAR_CONTENT_TEXTURE;

    // now test if it's tiled
    // TODO: move the test to a function in level.c
    for (int i = 1; i <= 64; i <<= 1) {
        int firstoff = find_first_offset(node->buf, i);
        // sometimes it's 64x32 (or 32x64? who knows)
        if (firstoff == 64*64*i || firstoff == 64*32*i) 
            return EAR_CONTENT_TILED;
    }
        
    return EAR_CONTENT_MESH;
}

// TODO: add error handling to these

void aloha_parse_texture(AlohaTexture *out, EarNode *clut_node, EarNode *bitmap_node)
{
    out->clut_node = clut_node;
    out->bitmap_node = bitmap_node;
}

void aloha_parse_objset(AlohaObjSet *out, EarNode *node)
{
    out->node = node;
    EarNode *p = &node->sub[0];
    out->clut_node = p++;
    bool tex = p->type == EAR_NODE_TYPE_FILE;
    EarNode *tex_clut = tex ? p : NULL;
    p++;
    for (int i = 0; i < OBJSET_MAX_MESH; i++) {
        out->mesh_nodes[i] = p++;
        if (p->type == EAR_NODE_TYPE_SEPARATOR) {
            p++;
            break;
        }
    }

    // TODO: these should probably store how many they find so we don't
    // duplicate code
    for (int i = 0; i < 2; i++) {
        aloha_parse_texture(&out->tex[i], tex_clut, p++);
        if (p->type == EAR_NODE_TYPE_SEPARATOR) {
            p++;
            break;
        }
    }
}

void aloha_parse_stage(AlohaStage *out, EarNode *node)
{
    out->node = node;
    // 0 is entities
    out->geom_hi.node = &node->sub[1];
    out->geom_lo.node = &node->sub[2];
    // 3 is ???
    // 4 is ??? (optional)
}

void aloha_parse_level(AlohaLevel *out, EarNode *objs_node, EarNode *stage_node)
{
    aloha_parse_objset(&out->objs, objs_node);
    aloha_parse_stage(&out->stage, stage_node);
}

void aloha_parse_dat(DatFile *out, EarNode *node)
{
    assert(node->type == EAR_NODE_TYPE_DIR);
    out->node = node;
    aloha_parse_texture(&out->env[0], &node->sub[0], &node->sub[1]);
    aloha_parse_texture(&out->env[1], &node->sub[2], &node->sub[3]);

    // TODO: convert both levels
    EarNode *p = &node->sub[4];
    EarNode *objsets = p;
    while ((p++)->type != EAR_NODE_TYPE_SEPARATOR);
    EarNode *stages = p;
    aloha_parse_level(&out->levels[0], &objsets[0], &stages[0]);
    //aloha_parse_level(&out->levels[1], &objsets[1], &stages[1]);
}