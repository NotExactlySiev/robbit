#ifndef _EAR_H
#define _EAR_H

#include <types.h>

typedef enum {
    EAR_NODE_TYPE_UNK,
    EAR_NODE_TYPE_SEPARATOR,
    EAR_NODE_TYPE_DIR,
    EAR_NODE_TYPE_FILE,
} EarNodeType;

typedef struct EarNode EarNode;
struct EarNode {
    EarNodeType type;
    EarNode     *parent;    // NULL in root
    union {
        // EAR_NODE_TYPE_DIR
        struct {
            EarNode *sub;
            uint count;
        };
        // EAR_NODE_TYPE_FILE
        struct {
            void *buf;
            u32 size;
        };
    };
};

typedef struct Ear Ear;
struct Ear {
    uint size;
    // stored breadth first, nodes[0] is the root
    EarNode nodes[];
};

// # Main functions
Ear    *ear_decode(void *data, u32 size);
u8     *ear_encode(Ear *ear);
void    ear_free(Ear *ear);

// # Helper functions (do we need these?)
void ear_print(EarNode* ear, void (*cb)(EarNode*));
//void ear_node_overwrite_from_file(EarNode *node, const char *path);

#endif
