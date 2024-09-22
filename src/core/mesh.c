// This module parses the binary data for the game's models and draws
// it to an SDL_Renderer. There are multiple models per .vo2 file, and
// the clut, texture, and texture clut are stored in the sibling files
// which are also needed.
// The main program is tasked with figuring out which is which. This
// code simply takes them in as input.

#include <math.h>
#include <types.h>

#include "common.h"
#include "mesh.h"

// there is a raw binary of the vo2 file
// a function takes it in, parses it, and returns an array of
// RobbitMesh structs. this array is all the meshes in the file,
// all 128 of them, even the empty ones. the empty ones
// explicitely say that they are empty (4 bytes, all zero)
// the non-empty ones have pointers to the start of each of their
// sections (verts, section 2, header, face data (should be one?))
// basically only the things we have to calculate anyway just to
// enumerate the meshes and find the pointer to the start of each one

static inline _Color color_15_to_24(u16 col)
{
    return (_Color) {
        .r = (0x1f & (col >> 0)) << 3,
        .g = (0x1f & (col >> 5)) << 3,
        .b = (0x1f & (col >> 10)) << 3,
    };
}

// how many meshes are in this file? (always is 128)
static u32 mesh_file_count(const u8 *data)
{
    return (((u32) data[0])
          | ((u32) data[1] << 8)
          | ((u32) data[2] << 16)
          | ((u32) data[3] << 24));
}

static u32 parse_file(u8 *data, RobbitMesh *out)
{
    u32 count = mesh_file_count(data);
    u32 count_non_empty = 0;
    void *p = data + 4;
    for (uint i = 0; i < count; i++) {
        out[i] = (RobbitMesh) {0};
        out[i].head = p;
        if (*(u32*)p == 0) {
            p += 4;
            out[i].empty = true;
            continue;
        }
        p += 4;
        count_non_empty += 1;

        // TODO: these reads are endian dependant. oh well
        out[i].nverts = *(u32*)p + 1;
        p += 4;
        out[i].verts = p;
        p += out[i].nverts * sizeof(AlohaVertex);

        out[i].nunks = *(u32*)p + 1;
        p += 4;
        // TODO: these sizes shouldn't be hardcoded
        p += out[i].nunks * 4;

        out[i].ngroups = *(u32*)p + 1;
        p += 4;
        // one header per group
        p += out[i].ngroups * 4;
        out[i].faces = p;
        for (uint group = 0; group < out[i].ngroups; group++) {
            // each group has one or more subgroups. each subgroup has some
            // ranges in its header and then a list of its faces.
            // we don't cache subgroup info, just read enough data to skip
            // over them. the encoding actually makes this fast.
            u32 subgroups_count = *(u32*)p + 1;
            p += 4;
            for (uint sub = 0; sub < subgroups_count; sub++) {
                p += 4 + *(u32*)p;
                p += 4 + *(u32*)p;
            }
        }
    }

    return count_non_empty;
}

// ranges of vertecis used in the subset
typedef struct {
    u16 base;
    u16 len;
} SubsetRange;

static u32 translate_index(u8 index, SubsetRange *ranges)
{
    int range = 0;
    int j = index - 1;
    while (j >= ranges[range].len + 1) {
        j -= ranges[range].len + 1;
        range += 1;
    }
    return j + ranges[range].base/8;
}

// this method ONLY extracts an array of faces and translates indices
// if out == NULL, just the count is returned
static int extract_faces(AlohaFace *dst, RobbitMesh *mesh)
{
    int ret = 0;
    void *p = mesh->faces;
    for (uint i = 0; i < mesh->ngroups; i++) {
        u32 nsubgroups = *(u32*)p + 1;
        p += 4;
        for (uint j = 0; j < nsubgroups; j++) {
            u32 nranges = (*(u32*)p)/sizeof(SubsetRange);
            p += 4;
            SubsetRange *ranges = p;
            p += nranges * sizeof(SubsetRange);
            u32 nfaces = (*(u32*)p)/sizeof(AlohaFace);
            ret += nfaces;
            p += 4;
            if (dst != NULL) {
                AlohaFace *faces = p;
                for (AlohaFace *f = &faces[0]; f < &faces[nfaces]; f++) {
                    *dst = *f;
                    // probably precompute the translations?
                    dst->v0 = translate_index(f->v0/3, ranges);
                    dst->v1 = translate_index(f->v1/3, ranges);
                    dst->v2 = translate_index(f->v2/3, ranges);
                    int v3 = f->v3;
                    if (v3 >= 3)
                        v3 = translate_index(f->v3/3, ranges);
                    else
                        v3 = 0; // FIXME: v3 should be retained
                    dst->v3 = v3;
                    dst += 1;
                }
            }
            p += nfaces * sizeof(AlohaFace);
        }
    }
    return ret;
}

void draw_mesh(VkCommandBuffer cbuf, RobbitMesh *mesh)
{
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cbuf, 0, 1, &mesh->vert_buffer.vk, &offset);
    vkCmdDraw(cbuf, mesh->vert_count, 1, 0, 0);
}

// TODO: right now this only converts the LOD
void convert_objset(RobbitObjSet *set, AlohaObjSet *src)
{
    u8 reps[1 << 21] = {0};
    u8 ntex = 2;
    u16 *clut_data = src->clut_node->buf;
    parse_file(src->mesh_nodes[0]->buf, set->normal);
    parse_file(src->mesh_nodes[1]->buf, set->lod);
    
    for (int i = 0; i < 128; i++) {
        RobbitMesh *m = &set->lod[i];
        if (m->empty) continue;
        int nfaces = extract_faces(NULL, m);
        AlohaFace faces[nfaces];
        extract_faces(faces, m);
        
        int nprims = 0;
        RobbitVertex vkverts[2048][3] = {0};

        for (AlohaFace *f = &faces[0]; f < &faces[nfaces]; f++) {
            bool tex = !(f->flags1 & 0x8000);
            bool lit = !(f->flags0 & 0x0001);
            
            vkverts[nprims][0].pos = m->verts[f->v0];
            vkverts[nprims][1].pos = m->verts[f->v1];
            vkverts[nprims][2].pos = m->verts[f->v2];
            
            if (lit) {
                vkverts[nprims][0].normal.x = f->nv0;
                vkverts[nprims][0].normal.y = f->nv1;
                vkverts[nprims][0].normal.z = f->nv2;
                printf("%d %d %d %d\n", f->nv0, f->nv1, f->nv2, f->nv3);
            }

            if (tex) {
                u32 tw = f->unk;
                int page = (f->flags0 & 0x4) >> 2;
                if (tw != 0xE3000000) {
                    u32 xmask = (tw >> 0) & 0x1F;
                    u32 ymask = (tw >> 5) & 0x1F;
                    u32 x = ((tw >> 10) & 0x1F) << 3;
                    u32 y = ((tw >> 15) & 0x1F) << 3;
                    u32 w = ((~(xmask << 3)) + 1) & 0xFF;
                    u32 h = ((~(ymask << 3)) + 1) & 0xFF;
                    

                    u32 key = (tw & 0xFFFFF) | (page << 20);
                    if (reps[key] == 0) {
                        printf("PAGE %d ", page);
                        printf("REP %d\t%d\t%d\t%d\n", x, y, w, h);
                        reps[key] = ntex++;
                        // TODO: create the texture here, and cache it
                    }
                }

                vkverts[nprims][0].u = f->tu0;
                vkverts[nprims][0].v = f->tv0;

                vkverts[nprims][1].u = f->tu1;
                vkverts[nprims][1].v = f->tv1;

                vkverts[nprims][2].u = f->tu2;
                vkverts[nprims][2].v = f->tv2;
            } else {
                vkverts[nprims][0].col = color_15_to_24(clut_data[f->flags0 >> 2]);
            }


            nprims++;

            if (f->v3 != 0) {
                vkverts[nprims][0].pos = m->verts[f->v0];
                vkverts[nprims][1].pos = m->verts[f->v2];
                vkverts[nprims][2].pos = m->verts[f->v3];

                
                if (lit) {
                    vkverts[nprims][0].normal.x = f->nv0;
                    vkverts[nprims][0].normal.y = f->nv1;
                    vkverts[nprims][0].normal.z = f->nv2;
                    printf("%d %d %d %d\n", f->nv0, f->nv1, f->nv2, f->nv3);
                }

                if (tex) {
                    vkverts[nprims][0].u = f->tu0;
                    vkverts[nprims][0].v = f->tv0;

                    vkverts[nprims][1].u = f->tu2;
                    vkverts[nprims][1].v = f->tv2;

                    vkverts[nprims][2].u = f->tu3;
                    vkverts[nprims][2].v = f->tv3;
                } else {
                    vkverts[nprims][0].col = color_15_to_24(clut_data[f->flags0 >> 2]);
                }
                nprims++;
            }
        }

        m->vert_count = 3 * nprims;
        int vert_size = m->vert_count * sizeof(RobbitVertex);
        Buffer vert_buf = create_buffer(vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        buffer_write(vert_buf, vert_size, vkverts);
        m->vert_buffer = vert_buf;
    }
}

void destroy_objset(RobbitObjSet *set)
{
    for (int i = 0; i < 128; i++) {
        if (set->normal[i].empty) continue;
        destroy_buffer(set->normal[i].vert_buffer);
    }

    for (int i = 0; i < 128; i++) {
        if (set->lod[i].empty) continue;
        destroy_buffer(set->lod[i].vert_buffer);
    }
}

void dump_objset(RobbitObjSet *set)
{
    for (int i = 0; i < 128; i++) {
        printf("%03d:\t", i);
        if (set->normal[i].empty)
            printf("---\t");
        else
            printf("%d\t", set->normal[i].nverts);
            
        if (set->lod[i].empty)
            printf("---\t");
        else
            printf("%d\t", set->lod[i].nverts);

        bool normal = !set->normal[i].empty;
        bool lod = !set->lod[i].empty;

        if (normal) {
            if (lod) printf("Exists");
            else printf("Exists (No LOD)");
        } else {
            if (lod) printf("!!! Only LOD");
        }
        printf("\n");
    }
}