#include "../exact/archive.h"
#include "common.h"

// Layer 2, node content types and relations
// these are still immutable and are calculated only once
// when the file is loaded.

// this file gets the tree and adds metadata / content types
// we assume the type of a node never changes after the initial scan

// TODO: should be robbit_metadata lmao

// CONTENT_COMMON_ASSETS
// CONTENT_LEVEL_MODELS
// CONTENT_LEVEL_DATA
// 
// CONTENT_MODELS
// CONTENT_STAGE
// CONTENT_GROUND
//
// CLUT
// TEXTURE  (IMG?)
// MESH
// SECTION0-3

Ear *ear;
AlohaMetadata *md;

AlohaMetadata *aloha(EarNode *n)
{
    return md + (n - ear->nodes);
}

EarContent guess_content_root(EarNode *node)
{

}


// guess the content of a NODE_TYPE_FILE
EarContent guess_content_base(EarNode *node)
{
    void* buf = node->buf;
    if (node->size == 0x200) return EAR_CONTENT_CLUT;
    
    // check if it could be a texture
    uint8_t* q;
    uint16_t w,h;
    q = buf + 4;
    READ_BE16(w, q);
    READ_BE16(h, q);
    if (w*h + 8 == node->size) return EAR_CONTENT_TEXTURE;
    // the clut for the texture will be found by the parent

    // now test if it's tiled
    // TODO: move the test to a function in level.c
    for (int i = 1; i <= 64; i <<= 1) {
        int firstoff = find_first_offset(buf, i);
        // sometimes it's 64x32 (or 32x64? who knows)
        if (firstoff == 64*64*i || firstoff == 64*32*i) 
            return EAR_CONTENT_TILED;
    }
        
    //printf("offset is %X\n", find_first_offset(buf, 4));
    return EAR_CONTENT_MESH;
}

// tag (just like G, change in place)
EarContent guess_content_composite(EarNode *node)
{
    if (node->count == 5) {
        // are they both tiled? is stage
        if (aloha(&node->sub[1])->content == EAR_CONTENT_TILED
         && aloha(&node->sub[2])->content == EAR_CONTENT_TILED) {
            return EAR_CONTENT_STAGE;
        }
    }

    if (aloha(&node->sub[0])->content == EAR_CONTENT_CLUT) {
        // call set_node_model or whatever
        AlohaMetadata *a = aloha(node);
        
        a->model.mesh_clut = &node->sub[0];
        a->model.mesh = &node->sub[2];
        EarNode *texture_clut = NULL;
        if (aloha(&node->sub[1])->content == EAR_CONTENT_CLUT)
            texture_clut = &node->sub[1];

        // jump over to the textures
        int i;
        for (i = 3; i < node->count; i++) {
            if (node->sub[i].type == EAR_NODE_TYPE_SEPARATOR) break;
        }

        i += 1;
        if (aloha(&node->sub[i])->content == EAR_CONTENT_TEXTURE) {
            a->model.texture[0] = &node->sub[i];
            aloha(a->model.texture[0])->texture.clut = texture_clut;
        }

        i += 1;
        if (aloha(&node->sub[i])->content == EAR_CONTENT_TEXTURE) {
            a->model.texture[1] = &node->sub[i];
            aloha(a->model.texture[1])->texture.clut = texture_clut;
        }

        return EAR_CONTENT_ENTITY;
    }

    return EAR_CONTENT_UNKNOWN;
}

int F(Ear *e)
{
    ear = e;
    md = malloc(e->size * sizeof(AlohaMetadata));
    for (int i = e->size-1; i >= 0; i--) {
        G(&e->nodes[i]);
    }
}

// for DIR nodes, assumes all the children have already been tagged
int G(EarNode *node)
{
        // possible outcomes:
        // - tier 0, root:
        //   - com_dat
        //   - xxx_ene
        //   - xxx_dat
        //   - gra_dat (wait, what was this?)
        // - tier 1, dirs
        //   - model
        //   - level (4 sections)
        //   - texture (no mesh)
        //   - perhaps more  
        // - tier 2, basic
        //   - clut
        //   - texture
        //   - mesh
        //   - level data (4 types)
        //   

    EarContent content = EAR_CONTENT_UNKNOWN;
    
    switch (node->type) {
    case EAR_NODE_TYPE_SEPARATOR:
        return EAR_CONTENT_NONE;
    case EAR_NODE_TYPE_DIR:
        if (!node->parent)
            content = guess_content_root(node);
        else
            content = guess_content_composite(node);
        
        //printf("DIR %d child\n", nchild);
        
        // simple _ENE test. are they all models?
        
        //return EAR_CONTENT_UNKNOWN;
        /*ear_t* sub = ear->sub;
        if (sub[0].content == EAR_CONTENT_CLUT &&
            (sub[1].content == EAR_CONTENT_CLUT || sub[1].content == EAR_CONTENT_NONE)) {
            // this might be an entity. check to see if we have a series of meshes,
            // then a seperator, zero or more textures and then another seperator
            ear_t* p;
            for (p = &sub[2]; p->type != NONE; p++) {
                if (p->content != EAR_CONTENT_MESH || p->type == TERMINATOR) return EAR_CONTENT_UNKNOWN;
            }
            for (p++; p->type != NONE; p++) {
                if (p->content != EAR_CONTENT_TEXTURE || p->type == TERMINATOR) return EAR_CONTENT_UNKNOWN;
            }
            if ((++p)->type == TERMINATOR) return EAR_CONTENT_ENTITY;
        }*/


        break;
    case EAR_NODE_TYPE_FILE:
        content = guess_content_base(node);
        break;
    }

    aloha(node)->content = content;
    return 0;
}