#include <types.h>
#include "robbit.h"
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

static u32 parse_file(u8 *data, RobbitMesh *out)
{
    u32 count = u32le(data);
    assert(count == 128);
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
    assert(mesh->vert_buffer.vk != VK_NULL_HANDLE);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cbuf, 0, 1, &mesh->vert_buffer.vk, &offset);
    vkCmdDraw(cbuf, mesh->vert_count, 1, 0, 0);
}

Image to_vulkan_image(AlohaTexture *src);
Image extract_tile(Image *img, i32 x, i32 y, i32 w, i32 h);

static void set_prim(RobbitVertex *verts, 
    AlohaVertex v0,
    AlohaVertex v1,
    AlohaVertex v2,
    Color color,
    bool tex, u8 texid,
    u8 tu0, u8 tv0,
    u8 tu1, u8 tv1,
    u8 tu2, u8 tv2,
    u16 x, u16 y, u16 w, u16 h)
{
    verts[0].pos = v0;
    verts[1].pos = v1;
    verts[2].pos = v2;
    verts[0].tex = tex;
    if (tex) {
        verts[0].texid = texid;
        verts[0].texwin = (Rect8) { x, y, w, h };
        verts[0].u = tu0; verts[0].v = tv0;
        verts[1].u = tu1; verts[1].v = tv1;
        verts[2].u = tu2; verts[2].v = tv2;
    } else {
        verts[0].col = color;
    }
}

// TODO: right now this only converts one of the sets. normal or lod
// TODO: now that texture window is handled in the shader, we can break this
//       function apart a bit.
void convert_objset(RobbitObjSet *set, AlohaObjSet *src)
{
    u8 ntex = 0;
    // first do the main images (if they exist)
    if (src->tex[0].bitmap_node) {
        Image img = to_vulkan_image(&src->tex[0]);
        image_set_layout(&img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkImageView view = image_create_view(img, VK_IMAGE_ASPECT_COLOR_BIT);
        set->texture.images[ntex] = img;
        set->texture.views[ntex] = view;
        ntex += 1;
    }

    if (src->tex[1].bitmap_node) {
        Image img = to_vulkan_image(&src->tex[1]);
        image_set_layout(&img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        VkImageView view = image_create_view(img, VK_IMAGE_ASPECT_COLOR_BIT);
        set->texture.images[ntex] = img;
        set->texture.views[ntex] = view;
        ntex += 1;
    }
    
    u16 *clut_data = src->clut_node->buf;
    
    set->nlod = 0;
    for (int i = 0; i < OBJSET_MAX_MESH; i++) {
        if (src->mesh_nodes[i] == NULL) break;
        set->nlod += 1;
        parse_file(src->mesh_nodes[i]->buf, set->lod[i]);
    }
    
    for (int i = 0; i < 128; i++) {
        /*RobbitMesh *m;
        for (int j = 0; j < set->nlod; j++) {
            m = &set->lod[j][i];
            if (!m->empty) break;
        }*/
        RobbitMesh *m = &set->lod[0][i];
        if (m->empty) continue;
        int nfaces = extract_faces(NULL, m);
        AlohaFace faces[nfaces];
        extract_faces(faces, m);
        
        int nprims = 0;
        RobbitVertex vkverts[2048][3];

        for (AlohaFace *f = &faces[0]; f < &faces[nfaces]; f++) {
            AlohaVertex v0 = m->verts[f->v0];
            AlohaVertex v1 = m->verts[f->v1];
            AlohaVertex v2 = m->verts[f->v2];
            AlohaVertex v3 = m->verts[f->v3];
            // TODO: take out these magic flags
            bool tex = !(f->flags1 & 0x8000);
            //bool lit = !(f->flags0 & 0x0001);
            u8 texid;
            // 16 bits so we don't overflow
            u8 tu0 = f->tu0, tv0 = f->tv0;
            u8 tu1 = f->tu1, tv1 = f->tv1;
            u8 tu2 = f->tu2, tv2 = f->tv2;
            u8 tu3 = f->tu3, tv3 = f->tv3;
            u16 x = 0;
            u16 y = 0;
            u16 w = 256;
            u16 h = 256;
            Color color = color_15_to_24(clut_data[f->flags0 >> 2]);

            if (tex) {
                u32 tw = f->texwindow;
                int page = (f->flags0 & 0x4) >> 2;

                texid = page;
                // TODO: this test is not adequate, false positives
                if (tw != 0xE2000000) {
                    u32 xmask = (tw >> 0) & 0x1F;
                    u32 ymask = (tw >> 5) & 0x1F;
                    x = ((tw >> 10) & 0x1F) << 3;
                    y = ((tw >> 15) & 0x1F) << 3;
                    w = ((~(xmask << 3)) + 1) & 0xFF;
                    h = ((~(ymask << 3)) + 1) & 0xFF;
                    // I'm pretty sure this is correct...
                    assert(w != 0 || h != 0);
                    //w = w ?: set->texture.images[page].w;
                    //h = h ?: set->texture.images[page].h;
                }
            }

            set_prim(vkverts[nprims++], v0, v1, v2, color,
                     tex, texid, tu0, tv0, tu1, tv1, tu2, tv2, x, y, w, h);

            if (f->v3 != 0) {
                set_prim(vkverts[nprims++], v0, v2, v3, color,
                     tex, texid, tu0, tv0, tu2, tv2, tu3, tv3, x, y, w, h);
            }
        }

        assert(nprims < 2048);
        assert(nprims > 0);

        m->vert_count = 3 * nprims;
        int vert_size = m->vert_count * sizeof(RobbitVertex);
        Buffer vert_buf = create_buffer(vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        assert(vert_buf.vk != VK_NULL_HANDLE);
        buffer_write(vert_buf, vert_size, vkverts);
        
        m->vert_buffer = vert_buf;
    }

    set->texture.n = ntex;
}

void destroy_objset(RobbitObjSet *set)
{
    for (int i = 0; i < 128; i++) {
        if (set->lod[0][i].empty) continue;
        destroy_buffer(set->lod[0][i].vert_buffer);
    }

    for (int i = 0; i < 128; i++) {
        if (set->lod[1][i].empty) continue;
        destroy_buffer(set->lod[1][i].vert_buffer);
    }

    for (int i = 0; i < set->texture.n; i++) {
        vkDestroyImageView(ldev, set->texture.views[i], NULL);
        destroy_image(set->texture.images[i]);
    }
}

void dump_objset(RobbitObjSet *set)
{
    for (int i = 0; i < set->nlod; i++) {
        printf("\tLOD %d", i);
    }
    printf("\n");
    for (int i = 0; i < 128; i++) {
        printf("%03d:\t", i);
        
        for (int j = 0; j < set->nlod; j++) {
            if (set->lod[j][i].empty)
                printf("---\t");
            else
                printf("%d\t", set->lod[j][i].nverts);
        }
        printf("\n");
    }
}