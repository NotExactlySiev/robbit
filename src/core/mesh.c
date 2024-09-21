// This module parses the binary data for the game's models and draws
// it to an SDL_Renderer. There are multiple models per .vo2 file, and
// the clut, texture, and texture clut are stored in the sibling files
// which are also needed.
// The main program is tasked with figuring out which is which. This
// code simply takes them in as input.

#include <math.h>
#include <types.h>

//#include "image.h"
#include "mesh.h"

// The interface I want is these functions:
// - how many meshes are in this file?
// - draw mesh number i to SDL_Surface (what is that?)

// there is a raw binary, of the vo2 file
// a function takes it in, parses it, and returns an array of
// RobbitMesh structs. this array is all the meshes in the file,
// all 128 of them, even the empty ones. the empty ones
// explicitely say that they are empty (4 bytes, all zero)
// the non-empty ones have pointers to the start of each of their
// sections (verts, section 2, header, face data (should be one?))
// basically only the things we have to calculate anyway just to
// enumerate the meshes and find the pointer to the start of each one
// u32 mesh_count(u8 *raw)  // just return the top number
// mesh_parse(u8 *raw, RobbitMesh *out)
// mesh_draw(SDL_Renderer *rend, RobbitMesh *mesh, u16 *clut, [texture])

// how many meshes are in this file? (always is 128)
u32 mesh_file_count(const u8 *data)
{
    return (((u32) data[0])
          | ((u32) data[1] << 8)
          | ((u32) data[2] << 16)
          | ((u32) data[3] << 24));
}

u32 mesh_file_parse(const u8 *data, RobbitMesh *out)
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

// temp
/*
#include <GL/gl.h>

static GLfloat i16tof(i16 x)
{
    return ((float) x) / INT16_MAX;
}

static void aloha2glvert(AlohaVertex t)
{
    glVertex3f(i16tof(t.x), i16tof(t.y), i16tof(t.z));
}

extern GLuint texid[2];

typedef struct {
    GLuint tex;
    float xf, yf;
} RepTex;


GLuint reps[1 << 21] = {0}; // lol

// TODO: the main image should store w and h
// caller should delete texture (TODO: why not keep it cached?)
static GLuint repeat_texpage(u32 tw, u16 *pixels, int main_w, int main_h, float *xf, float *yf, int page)
{
    

    // find the rectangle
    int xmask = (tw >> 0) & 0x1F;
    int ymask = (tw >> 5) & 0x1F;
    int x = ((tw >> 10) & 0x1F) << 3;
    int y = ((tw >> 15) & 0x1F) << 3;
    int w = ((~(xmask << 3)) + 1) & 0xFF;
    int h = ((~(ymask << 3)) + 1) & 0xFF;
    pixels += y*main_w + x;

    *xf = (float) main_w / w;
    *yf = (float) main_h / h;

    u32 key = (tw & 0xFFFFF);
    key = (key << 1) | page;
    if (reps[key] != 0) return reps[key];
    printf("no exist! %08X\n", key);

    GLuint tex;
    glGenTextures(1, &tex);
    
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, main_w);
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    reps[key] = tex;
    return tex;
}

// this should just receive images and make the textures by itself
void mesh_render_opengl(RobbitMesh *mesh, u16 *clut, void **textures)
{   
    void *p = mesh->faces;
    for (uint i = 0; i < mesh->groups_count; i++) {
        u32 subgroups_count = *(u32*)p + 1;
        p += 4;
        for (uint j = 0; j < subgroups_count; j++) {
            u32 ranges_count = (*(u32*)p)/sizeof(SubsetRange);
            p += 4;
            SubsetRange *ranges = p;
            p += ranges_count * sizeof(SubsetRange);
            u32 faces_count = (*(u32*)p)/sizeof(AlohaFace);
            p += 4;
            AlohaFace *faces = p;
            for (uint k = 0; k < faces_count; k++) {
                AlohaFace *face = &faces[k];
                // probably precompute the translations?
                int v0 = translate_index(faces[k].v0/3, ranges);
                int v1 = translate_index(faces[k].v1/3, ranges);
                int v2 = translate_index(faces[k].v2/3, ranges);
                Color col = image_15_to_24(clut[faces[k].flags0 >> 2]);
                bool tex = !(faces[k].flags1 & 0x8000);
                GLuint rep = 0;

                float fu0;
                float fv0;
                float fu1;
                float fv1;
                float fu2;
                float fv2;
                float fu3;
                float fv3;
                if (tex) {
                    int page = (face->flags0 & 0x4) >> 2; 
                    glColor3f(1.0f, 1.0f, 1.0f);
                    fu0 = (float) face->tu0 / UINT8_MAX;
                    fv0 = (float) face->tv0 / UINT8_MAX;
                    fu1 = (float) face->tu1 / UINT8_MAX;
                    fv1 = (float) face->tv1 / UINT8_MAX;
                    fu2 = (float) face->tu2 / UINT8_MAX;
                    fv2 = (float) face->tv2 / UINT8_MAX;
                    fu3 = (float) face->tu3 / UINT8_MAX;
                    fv3 = (float) face->tv3 / UINT8_MAX;

                    u32 tw = face->unk;
                    if (tw == 0xE2000000) {
                        glBindTexture(GL_TEXTURE_2D, texid[page]);
                    } else {
                        float xf, yf;
                        rep = repeat_texpage(tw, textures[page], 256, 256, &xf, &yf, page);
                        fu0 *= xf; fu1 *= xf; fu2 *= xf; fu3 *= xf;
                        fv0 *= yf; fv1 *= yf; fv2 *= yf; fv3 *= yf;
                        glBindTexture(GL_TEXTURE_2D, rep);
                    }
                } else {
                    glColor4ub(col.r, col.g, col.b, 0);
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                glBegin(GL_TRIANGLE_STRIP);
                    if (tex) glTexCoord2f(fu1, fv1);
                    aloha2glvert(mesh->verts[v1]);
                    
                    if (tex) glTexCoord2f(fu2, fv2);
                    aloha2glvert(mesh->verts[v2]);

                    if (tex) glTexCoord2f(fu0, fv0);
                    aloha2glvert(mesh->verts[v0]);
                    if (faces[k].v3 >= 3) {
                        int v3 = translate_index(faces[k].v3/3, ranges);
                        //if (tex) glTexCoord2f((float) face->tu3 / UINT8_MAX, (float) face->tv3 / UINT8_MAX);
                        if (tex) glTexCoord2f(fu3, fv3);
                        aloha2glvert(mesh->verts[v3]);
                    }
                glEnd();
            }
            p += faces_count * sizeof(AlohaFace);
        }
    }
}
*/

/*
void mesh_render_vulkan(RobbitMesh *mesh, u16 *clut, void **textures)
{   
}
*/

// this method ONLY extracts an array of faces and translate indices
// only returns count if out == NULL
int mesh_faces(AlohaFace *dst, RobbitMesh *mesh)
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