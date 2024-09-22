#include "../common.h"
#include "archive.h"
#include "press.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define EAR_MAX_NODES           256
#define EAR_MAX_CHILDREN        16
#define EAR_HEADER_TERMINATOR   -1
#define EAR_HEADER_SEPARATOR    0

// input data should include the header
static void *decompress_data(void* compressed_data, uint32_t* size_out)
{
    u8 *buffer;
    u32 decompressed_size;

    decompressed_size = exact_get_decompressed_size(compressed_data);
    buffer = malloc(decompressed_size);
    exact_decompress(buffer, compressed_data);
    *size_out = decompressed_size;
    return buffer;
}

// does this section have a consistent header structure for a directory?
// TODO: rename and make it return a bool
static EarNodeType find_type(void *data)
{
    // we already know it's a real node (not separator) so simply
    // check if the header has a valid structure for a directory
    u32 *p = data;
    u32 first = 0;
    u32 prev = first;
    for (int i = 0; i < EAR_MAX_CHILDREN; i++) {
        
        u32 offset = u32be(p++);
        if (first == 0) first = offset;
        if (offset == EAR_HEADER_TERMINATOR && (i+1)*4 == first)
            return EAR_NODE_TYPE_DIR;
        if (offset != 0 && offset <= prev)
            break;
    }
    return EAR_NODE_TYPE_FILE;
}

// makes ear by parsing the data. ear must have enough space allocated
Ear *ear_decode(void *data, u32 size)
{
    Ear *ret = malloc(sizeof(u32) + EAR_MAX_NODES*sizeof(EarNode));
    EarNode *tmp = ret->nodes;
    int i,j;
    u32 offset;

    // the root is always a directory
    tmp[0].type = EAR_NODE_TYPE_DIR;
    tmp[0].parent = NULL;
    tmp[0].buf = data;        // temp value, not actually file
    j = 1;

    for (i = 0; i < j; i++) {
        EarNode *np = &tmp[i];
        if (np->type == EAR_NODE_TYPE_SEPARATOR)
            continue;

        void *head = np->buf;
        np->type = find_type(head);

        switch (np->type) {
        case EAR_NODE_TYPE_DIR:
            np->count = 0;
            np->sub = &tmp[j];
            u32 *offp = head;
            while ((offset = u32be(offp++), offset != EAR_HEADER_TERMINATOR)) {
                EarNode *child = &tmp[j++];
                np->count++;
                child->parent = np;
                child->type = offset == EAR_HEADER_SEPARATOR ? EAR_NODE_TYPE_SEPARATOR : EAR_NODE_TYPE_UNK;

                if (child->type == EAR_NODE_TYPE_UNK)
                    // remember to figure it out later
                    child->buf = head + offset;
            }
            break;
        case EAR_NODE_TYPE_FILE:
            np->buf = decompress_data(head, &np->size);
            break;
        default:
            assert(false);
            break;
        }
    }

    ret->size = i;
    return ret;
}

void ear_free(Ear *ear)
{
    EarNode *np;
    // TODO: macro this loop?
    for (int i = 0; i < ear->size; i++)
        if ((np = &ear->nodes[i])->type == EAR_NODE_TYPE_FILE)
            free(np->buf);
    free(ear);
}

static void _ear_print(EarNode* ear, int depth, void (*cb)(EarNode*))
{
    int i;
    EarNode* p;

    for (int i = 0; i < ear->count; i++) {
        p = &ear->sub[i];
        for (int j = 0; j < depth-1; j++) printf("│   ");
        if (depth > 0) printf(i == ear->count-1 ? "└── " : "├── ");

        switch (p->type) {
        case EAR_NODE_TYPE_SEPARATOR:
            printf("---");
            break;
        case EAR_NODE_TYPE_DIR:
            printf("dir");
            break;
        case EAR_NODE_TYPE_FILE:
            printf("file: 0x%X", p->size);
            break;
        }
        cb(p);
        printf("\n");

        if (p->type == EAR_NODE_TYPE_DIR)
            _ear_print(p, depth+1, cb);
    }
}

void ear_print(EarNode* ear, void (*cb)(EarNode*))
{
    _ear_print(ear, 1, cb);
}