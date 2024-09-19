#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "level.h"
#include "../exact/archive.h"
#include "mesh.h"

// hehehe
#include <GL/gl.h>

uint8_t* decomp(const uint8_t*, uint8_t*);

#define USAGE   "earie <.ear file>\n"
// TODO: should later be able to take in multiple related archives, and maybe
// even the world executable file. that would be necessary for level editing.

const char* content_strings[] = {
#define O(NAME, Name, name)     [EAR_CONTENT_##NAME] = #Name,
    CONTENTS
};

void save_to_file(const char* path, EarNode* node)
{
    if (node->type != EAR_NODE_TYPE_FILE) return;

    FILE* fd;
    fd = fopen(path, "w+");
    fwrite(node->buf, node->size, 1, fd);
    fclose(fd);
}

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t pad;
} Vertex;

typedef struct {
    uint8_t v0;
    uint8_t v1;
    uint8_t v2;
    uint8_t v3; // can be something else in tris
    uint16_t flags0;
    uint16_t flags1;
    uint16_t nv0, nv1, nv2, nv3;
    uint8_t tu0, tv0;
    uint8_t tu1, tv1;
    uint8_t tu2, tv2;
    uint8_t tu3, tv3;
    uint8_t unk[0x4];
} Face;

// see what range the index is in, and return the real index based on that
int find_in_range(int i, short* ranges)
{
    int range = 0;
    int j = i-1;
    while (j >= ranges[range*2+1]+1) {
        j -= ranges[range*2+1]+1;
        range += 1;
    }
    return j+1+ranges[range*2]/8;
}


Color image_15_to_24(uint16_t);

// ok so this was entirely pointless, it's just mesh_file_parse in mesh.c
void mesh_enumerate(void *data, int *vcount)
{
    // first, read the big numbers
    uint32_t *p = data;
    uint32_t count = *p++;
    printf("%d meshes\n", count);
    //while (count--) {
    for (int mesh_idx = 0; mesh_idx < count; mesh_idx++) {
        uint32_t head = *p++;
        if (!head) {
            vcount[mesh_idx] = -1;
            continue;
        }
        // TODO: these fucking reads are endian dependent you dumbass

        // (verts)
        int vert_count = *p++ + 1;
        p = (((void*) p) + sizeof(Vertex) * vert_count);
        vcount[mesh_idx] = vert_count;

        int field2_count = *p++ + 1;
        p += field2_count;

        int sets_count = *p++ + 1;
        p += sets_count;

        for (int i = 0; i < sets_count; i++) {
            int subsets_count = *p++ + 1;
            for (int j = 0; j < subsets_count; j++) {
                int ranges_count = *p++ / 4;
                p += ranges_count;
                int faces_count = *p++ / 0x1C;
                p += (faces_count * 0x1C) / 4;
            }
        }
    }
}

// convert to OBJ
void mesh_parse(void* data, uint16_t* clut)
{
    // first, read the big numbers
    FILE* fd = fopen("model.obj", "w+");
    bool colors_used[256] = { false };
    uint32_t* p = data;
    uint32_t count = *p++;
    fprintf(fd, "mtllib model.mtl\n");
    //fprintf(fd, "# %d meshes\n", count);
    int total_verts = 0;
    int vt_count = 0;
    while (count--) {
        uint32_t head = *p++;
        if (!head) continue;
        fprintf(fd, "o POLY%d\n", 127 - count);

        // (verts)
        int vert_count = *p++ + 1;
        //printf("# %d vertices\n", vert_count);
        Vertex* verts = malloc(sizeof(*verts) * vert_count);
        memcpy(verts, p, sizeof(*verts)*vert_count);
        for (int i = 0; i < vert_count; i++) {
            fprintf(fd, "v %f %f %f\n", (verts[i].x)/10.0, verts[i].y/10.0, verts[i].z/10.0);
        }
        p = (((void*) p) + sizeof(*verts) * vert_count);

        // field 2. TODO no idea what this does yet
        int field2_count = *p++ + 1;
        int16_t* q = p;
        for (int i = 0; i < field2_count; i++) {
            //printf("# %d - %d\n", *q++, *q++);
        }
        p += field2_count;

        int sets_count = *p + 1;
        void* face_data = p++;
        // header is 2 shorts. offset to pivot vert, offset to ranges (right before faces)
        short* header = p;
        p += sets_count;

        for (int i = 0; i < sets_count; i++) {
            int subsets_count = *p++ + 1;
            fprintf(fd, "# SET %d (%d, %d) %d SUBSETS\n", i, header[0], header[1], subsets_count);
            for (int j = 0; j < subsets_count; j++) {
                int ranges_count = *p++ / 4;
                short* ranges = p;
                p += ranges_count;
                int faces_count = *p++ / 0x1C;
                fprintf(fd, "#\t\t%d FACES ", faces_count);
                short base, len;
                for (int k = 0; k < ranges_count; k++) {
                    base = ranges[2*k] / 8;
                    len = ranges[2*k+1] + 1;
                    fprintf(fd, "[%d, %d] ", base, len);
                }
                fprintf(fd, "\n");
                
                int voff = total_verts;
                Face* faces = p;
                for (int k = 0; k < faces_count; k++) {
                    if (faces[k].flags1 & 0x8000) {
                        // flat colored
                        colors_used[faces[k].flags0 >> 2] = true;
                        fprintf(fd, "usemtl Color%d\n", faces[k].flags0 >> 2);

                        fprintf(fd, "f %d %d %d ", 
                            find_in_range(faces[k].v0/3, ranges)+voff,
                            find_in_range(faces[k].v1/3, ranges)+voff,
                            find_in_range(faces[k].v2/3, ranges)+voff);
                        if (faces[k].v3 >= 3)
                            fprintf(fd, "%d", find_in_range(faces[k].v3/3, ranges)+voff);
                    } else {
                        // textured
                        int texture_size = 64;

                        fprintf(fd, "vt %f %f\n", (float) faces[k].tu0 / texture_size, (float) (texture_size - faces[k].tv0) / texture_size);
                        fprintf(fd, "vt %f %f\n", (float) faces[k].tu1 / texture_size, (float) (texture_size - faces[k].tv1) / texture_size);
                        fprintf(fd, "vt %f %f\n", (float) faces[k].tu2 / texture_size, (float) (texture_size - faces[k].tv2) / texture_size);
                        if (faces[k].v3 >= 3)
                            fprintf(fd, "vt %f, %f\n", (float) faces[k].tu3 / texture_size, (float) (texture_size - faces[k].tv3) / texture_size); 

                        fprintf(fd, "usemtl Textured\n");
                        fprintf(fd, "f %d/%d %d/%d %d/%d ", 
                            find_in_range(faces[k].v0/3, ranges)+voff, vt_count + 1,
                            find_in_range(faces[k].v1/3, ranges)+voff, vt_count + 2,
                            find_in_range(faces[k].v2/3, ranges)+voff, vt_count + 3
                        );
                        if (faces[k].v3 >= 3)
                            fprintf(fd, "%d/%d", find_in_range(faces[k].v3/3, ranges)+voff, vt_count + 4);

                        vt_count += 3 + (faces[k].v3 >= 3);
                    }
                    
                    

                    
                    fprintf(fd, "\n");                    
                }
                p += (faces_count * 0x1C) / 4;
            }
            header += 2;
        }

        total_verts += vert_count;
    }
    fclose(fd);

    fd = fopen("model.mtl", "w+");
    for (int i = 0; i < 256; i++)
        if (colors_used[i]) {
            Color col = image_15_to_24(clut[i]);
            fprintf(fd, "newmtl Color%d\n", i);
            fprintf(fd, "Ka 1.000000 1.000000 1.000000\n");
            fprintf(fd, "Kd %f %f %f\n", ((float)col.r)/255, ((float)col.g)/255, ((float)col.b)/255); 
        }
    fprintf(fd, "newmtl Textured\nmap_Kd texture.png\n");

    fclose(fd);
}

void ui_init(SDL_Window *win, SDL_GLContext glctx);
int ui_run(EarNode *ear);

AlohaMetadata *aloha(EarNode *n);

void print_content(EarNode *n)
{
    char *str = content_strings[aloha(n)->content];
    if (str)
        printf("\t[%s]", str);
}

AlohaMesh meshes_normal[128];
AlohaMesh meshes_lod[128];
u16 *clut_data;
GLuint texid[2];

GLfloat i16tof(i16 x)
{
    return ((float) x) / INT16_MAX;
}

void draw_mesh(Thing t, void **pixels)
{
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(i16tof(t.x), i16tof(t.y), i16tof(t.z));
    mesh_render_opengl(&meshes_normal[t.id], clut_data, pixels);
}

int main(int argc, char **argv)
{
    FILE* fd;
    size_t filesize;
    uint8_t* buffer;

    if (argc != 2) {
        printf(USAGE);
        return -1;
    }

    // read entire file
    fd = fopen(argv[1], "r");
    fseek(fd, 0, SEEK_END);
    filesize = ftell(fd);
    buffer = malloc(filesize);
    fseek(fd, 0, SEEK_SET);
    fread(buffer, 1, filesize, fd);
    fclose(fd);

    Ear *ear = ear_decode(buffer, 0);
    // one step is missing here. parsing the semantics of the tree
    // and connecting the related nodes, adding metadata

    // so this expects to get a fully parsed tree with metadata
    // and converted stuff to sdl format?
    // who converts to sdl?
    F(ear);
    ear_print(ear->nodes, print_content);
    
    // find the objects list and level data nodes
    // just fucking terrible code
    EarNode *objs_node = NULL;
    for (EarNode *np = ear->nodes->sub + 4; np->type != EAR_NODE_TYPE_SEPARATOR; np++) {
        if (np->type == EAR_NODE_TYPE_DIR && aloha(np)->content == EAR_CONTENT_ENTITY) {
            objs_node = np;
            break;
        }
    }

    EarNode *stage_node = NULL;
    for (EarNode *np = ear->nodes->sub + 4; np; np++) {
        if (np->type == EAR_NODE_TYPE_DIR && aloha(np)->content == EAR_CONTENT_STAGE) {
            stage_node = np;
            break;
        }
    }

    EarNode *geom_node = &stage_node->sub[2];

    printf("objs node is %d\n", objs_node - ear->nodes);
    //printf("geom node is %d\n", stage_node - ear->nodes);

    // parsing and converting objs. first: is lod a subset of normal?
    
    mesh_file_parse(objs_node->sub[2].buf, meshes_normal);
    mesh_file_parse(objs_node->sub[3].buf, meshes_lod);

    for (int i = 0; i < 128; i++) {
        printf("%03d:\t", i);
        if (meshes_normal[i].empty)
            printf("---\t");
        else
            printf("%d\t", meshes_normal[i].verts_count);
            
        if (meshes_lod[i].empty)
            printf("---\t");
        else
            printf("%d\t", meshes_lod[i].verts_count);

        bool normal = !meshes_normal[i].empty;
        bool lod = !meshes_lod[i].empty;

        if (normal) {
            if (lod) printf("Exists");
            else printf("Exists (No LOD)");
        } else {
            if (lod) printf("!!! Only LOD");
        }
        printf("\n");
    }

    int nobjs = geom_count(geom_node->buf);
    printf("%d objects\n", nobjs);
    Thing objs[nobjs];
    geom_unpack(geom_node->buf, objs);

    printf("??? entities\n");

    AlohaMesh *house = &meshes_normal[66];
    void *x = house->faces;
    for (uint i = 0; i < house->groups_count; i++) {
        u32 subgroups_count = *(u32*)x + 1;
        x += 4;
        for (uint j = 0; j < subgroups_count; j++) {
            u32 ranges_count = (*(u32*)x)/4;
            x += 4;
            //SubsetRange *ranges = p;
            x += ranges_count * 4;
            u32 faces_count = (*(u32*)x)/sizeof(AlohaFace);
            x += 4;
            AlohaFace *faces = x;
            for (uint k = 0; k < faces_count; k++) {
                AlohaFace *face = &faces[k];
                if (faces[k].v3 >= 3) {
                    printf("QUAD\t");
                } else {
                    printf("TRI\t");
                }

                //printf("%04X %04X\t", face->flags0, face->flags1);
                if (!(face->flags1 & 0x8000)) {
                    printf("TEXTURED\t");
                    u32 tw = face->unk;
                    if (tw != 0xE2000000) {
                        printf("%08X\t", face->unk);
                        int xmask = tw & 0x1F;
                        int ymask = (tw >> 5) & 0x1F;
                        int xoff = (tw >> 10) & 0x1F;
                        int yoff = (tw >> 15) & 0x1F;

                        int rx = xoff << 3;
                        int ry = yoff << 3;
                        int rw = ((~(xmask << 3)) + 1) & 0xFF;
                        int rh = ((~(ymask << 3)) + 1) & 0xFF;
                        printf("WINDOW: %d\t%d\t(%dx%d)", rx, ry, rw, rh);
                    }
                }

                printf("\n");
            }
            x += faces_count * sizeof(AlohaFace);
        }
    }


    // ok now we're gonna be cheeky
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_OPENGL);
    SDL_Window *window = SDL_CreateWindow("Robbit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext glctx = SDL_GL_CreateContext(window);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    ui_init(window, glctx);

    bool running = true;

    

    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    //glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
    
    glGenTextures(2, &texid);

    EarNode *texture_clut = &objs_node->sub[1];
    EarNode *texture0 = &objs_node->sub[5];
    EarNode *texture1 = &objs_node->sub[6];

    uint8_t *p = (uint8_t*) texture0->buf;
    uint16_t w = (p[4] << 8) | (p[5]);
    uint16_t h = (p[6] << 8) | (p[7]);
    printf("%d %d\n", w, h);
    uint8_t *indexes0 = texture0->buf + 8;
    u16 pixels0[w*h];
    u16 *clut = texture_clut->buf;
    for (int i = 0; i < w*h; i++) {
        pixels0[i] = set_alpha(clut[indexes0[i]]);
    }
    
    glBindTexture(GL_TEXTURE_2D, texid[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, pixels0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    p = (uint8_t*) texture1->buf;
    w = (p[4] << 8) | (p[5]);
    h = (p[6] << 8) | (p[7]);
    printf("%d %d\n", w, h);
    uint8_t *indexes1 = texture1->buf + 8;
    u16 pixels1[w*h];
    clut = texture_clut->buf;
    for (int i = 0; i < w*h; i++) {
        pixels1[i] = set_alpha(clut[indexes1[i]]);
    }

    glBindTexture(GL_TEXTURE_2D, texid[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB5_A1, w, h, 0, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, pixels1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);

    glEnable(GL_DEPTH_TEST);
    glPointSize(2.f);
    glViewport(0, 0, 1280, 720);


    float r = 0.0;
    float zoom = 3.f;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ui_process_event(&event);
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;
            } else if (event.type == SDL_MOUSEWHEEL) {
                zoom += 0.07 * event.wheel.preciseY;
                if (zoom < 0.2) zoom = 0.2;                
            }
        }

        

        glClearColor(0.1f, 0.4f, 0.1f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glClear(GL_DEPTH_BUFFER_BIT);

        
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glColor3f(1, 1, 1);
        glBindTexture(GL_TEXTURE_2D, texid[1]);
        glBegin(GL_QUADS);
            glTexCoord2f(2.0, 0.0);
            glVertex2f(1.0, 1.0);
            
            glTexCoord2f(0.0, 0.0);
            glVertex2f(0.0, 1.0);

            glTexCoord2f(0.0, 2.0);
            glVertex2f(0.0, 0.0);

            glTexCoord2f(2.0, 2.0);
            glVertex2f(1.0, 0.0);
        glEnd();
        
        //goto over;

        glClear(GL_DEPTH_BUFFER_BIT);
        
        

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glScalef(zoom, -2*zoom, zoom);
        glRotatef(25.0, 1.0, 0.0, 0.0);
        glRotatef(r, 0.0, 1.0, 0.0);
        r += 0.1;

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glColor3f(1.0f, 1.0f, 1.0f);

        void *pixels[2] = { pixels0, pixels1 };
        clut_data = aloha(objs_node)->model.mesh_clut->buf;
        for (int i = 0; i < nobjs; i++) {
            draw_mesh(objs[i], pixels);
        }
        //draw_mesh((Thing) { 67, 0, 0, 0 }, pixels);

        // axes
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glBegin(GL_LINES);
            glColor3f(1.0, 0.0, 0.0);
            glVertex3f(0.0, 0.0, 0.0);
            glVertex3f(1.0, 0.0, 0.0);

            glColor3f(0.0, 1.0, 0.0);
            glVertex3f(0.0, 0.0, 0.0);
            glVertex3f(0.0, 1.0, 0.0);

            glColor3f(0.0, 0.0, 1.0);
            glVertex3f(0.0, 0.0, 0.0);
            glVertex3f(0.0, 0.0, 1.0);
        glEnd();

        ui_run(ear->nodes);

over:
        SDL_GL_SwapWindow(window);        
    }

    


    //mesh_parse(objs_node->sub[2].buf, objs_node->sub[0].buf);

    // just show meshes (both normal and lod)

    //AlohaMetadata *kiwi = aloha(&ear->nodes[3]);    // md[2]
    //mesh_parse(kiwi->related.mesh->buf, kiwi->related.mesh_clut->buf);

    //ui_run(ear->nodes);

    //ear_free(ear);
    return 0;
}
