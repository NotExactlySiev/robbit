#include <types.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

#include "core/common.h"
#include "core/mesh.h"
#include "core/level.h"
#include "vulkan/app.h"

void the_rest_normal(Pipeline *pipe, RobbitVertex *vert_data, int vert_size, EarNode *objs_node);
Pipeline create_pipeline_points();
Pipeline create_pipeline_normal();

#define USAGE   "earie <.ear file>\n"

const char* content_strings[] = {
#define O(NAME, Name, name)     [EAR_CONTENT_##NAME] = #Name,
    CONTENTS
};

static void print_content(EarNode *n)
{
    if (n->type == EAR_NODE_TYPE_SEPARATOR) return;
    char *str = content_strings[aloha(n)->content];
    if (str)
        printf("\t[%s]", str);
}

static void nop(EarNode *n) {}

GLFWwindow *window;
AlohaMesh meshes_normal[128];
AlohaMesh meshes_lod[128];
u16 *clut_data;

u16 **tmp_get_level_textures(EarNode *objs_node, int *w, int *h)
{
    EarNode *clut_node = &objs_node->sub[1];
    EarNode *tex_node = &objs_node->sub[5];

    u8 *p = (uint8_t*) tex_node->buf;
    u16 _w = (p[4] << 8) | (p[5]);
    u16 _h = (p[6] << 8) | (p[7]);
    *w = _w;
    *h = _h;
    u8 *indexes = tex_node->buf + 8;
    u16 *pixels = malloc(_w*_h*sizeof(u16));
    u16 *clut = clut_node->buf;
    for (int i = 0; i < _w*_h; i++) {
        pixels[i] = clut[indexes[i]];
    }
    

    return pixels;
}

int main(int argc, char **argv)
{    
    if (argc != 2) {
        printf(USAGE);
        return -1;
    }
    
    FILE *fd = fopen(argv[1], "r");
    fseek(fd, 0, SEEK_END);
    size_t filesize = ftell(fd);
    u8 *buffer = malloc(filesize);
    fseek(fd, 0, SEEK_SET);
    fread(buffer, 1, filesize, fd);
    fclose(fd);

    Ear *ear = ear_decode(buffer, 0);
    //F(ear);
    //ear_print(ear->nodes, print_content);
    ear_print(ear->nodes, nop);
    return 0;

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
    printf("geom node is %d\n", stage_node - ear->nodes);

    // parsing and converting objs. first: is lod a subset of normal?
    clut_data = aloha(objs_node)->model.mesh_clut->buf;
    
    mesh_file_parse(objs_node->sub[2].buf, meshes_normal);
    mesh_file_parse(objs_node->sub[3].buf, meshes_lod);

    /*for (int i = 0; i < 128; i++) {
        printf("%03d:\t", i);
        if (meshes_normal[i].empty)
            printf("---\t");
        else
            printf("%d\t", meshes_normal[i].nverts);
            
        if (meshes_lod[i].empty)
            printf("---\t");
        else
            printf("%d\t", meshes_lod[i].nverts);

        bool normal = !meshes_normal[i].empty;
        bool lod = !meshes_lod[i].empty;

        if (normal) {
            if (lod) printf("Exists");
            else printf("Exists (No LOD)");
        } else {
            if (lod) printf("!!! Only LOD");
        }
        printf("\n");
    }*/

    int nobjs = geom_count(geom_node->buf);
    printf("%d objects\n", nobjs);
    Thing objs[nobjs];
    geom_unpack(geom_node->buf, objs);

    printf("??? entities\n");

    AlohaMesh *house = &meshes_normal[66];
    

    //EarNode *kiwi_node = &ear->nodes->sub[0];
    //mesh_file_parse(kiwi_node->sub[2].buf, meshes_normal);
    //AlohaMesh *house = &meshes_normal[1];
    void *x = house->faces;

    //clut_data = aloha(kiwi_node)->model.mesh_clut->buf;

    int nfaces = mesh_faces(NULL, house);
    AlohaFace faces[nfaces];
    printf("%d FACES\n");
    mesh_faces(faces, house);
    

    int nprims = 0;
    RobbitVertex vkverts[2048][3] = {0};

    for (AlohaFace *f = &faces[0]; f < &faces[nfaces]; f++) {
        //printf("%d %d %d %d\n", f->v0, f->v1, f->v2, f->v3);
        bool tex = !(f->flags1 & 0x8000);
        bool lit = !(f->flags0 & 0x0001);
        
        vkverts[nprims][0].pos = house->verts[f->v0];
        vkverts[nprims][1].pos = house->verts[f->v1];
        vkverts[nprims][2].pos = house->verts[f->v2];
        
        

        if (lit) {
            vkverts[nprims][0].normal.x = f->nv0;
            vkverts[nprims][0].normal.y = f->nv1;
            vkverts[nprims][0].normal.z = f->nv2;
            printf("%d %d %d %d\n", f->nv0, f->nv1, f->nv2, f->nv3);
        }

        if (!tex) {
            vkverts[nprims][0].col = color_15_to_24(clut_data[f->flags0 >> 2]);
        }


        nprims++;

        if (f->v3 != 0) {
            vkverts[nprims][0].pos = house->verts[f->v0];
            vkverts[nprims][1].pos = house->verts[f->v2];
            vkverts[nprims][2].pos = house->verts[f->v3];

            
            if (lit) {
                vkverts[nprims][0].normal.x = f->nv0;
                vkverts[nprims][0].normal.y = f->nv1;
                vkverts[nprims][0].normal.z = f->nv2;
                printf("%d %d %d %d\n", f->nv0, f->nv1, f->nv2, f->nv3);
            }

            if (!tex) {
                vkverts[nprims][0].col = color_15_to_24(clut_data[f->flags0 >> 2]);
            }
            nprims++;
        }
    }
    

    const GLFWvidmode *mode;
    GLFWmonitor *monitor;
    

    if (GLFW_FALSE == glfwInit()) return -1;

    // don't create opengl for window, we're gonna be vulkaning
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(640, 480, "cool window", NULL, NULL);

    create_app(window);

    //Pipeline points_pipe = create_pipeline_points();


    // convert to the format vulkan likes
    // TODO: should the conversion be a two step process? first flatten the mesh
    // to one array of AlohaFace without groups/subgroups.
    // then apply other adjustments from there (divide by 3 etc.)

    
    Pipeline normal_pipe = create_pipeline_normal();
    
    //the_rest(&normal_pipe, house->verts, house->nverts * sizeof(AlohaVertex), NULL, 0);
    //the_rest_normal(&normal_pipe, house->verts, house->nverts * sizeof(AlohaVertex), index_data, 3*sizeof(u16)*nprims);
    the_rest_normal(&normal_pipe, vkverts, 3 * nprims * sizeof(RobbitVertex), objs_node);

    //destroy_pipeline(points_pipe);
    destroy_pipeline(normal_pipe);
    destroy_app();

    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}


/*

things that I had to change to do point drawing

the commands recorded to the buffer
    the buffers I was using

pipeline stuff:
    the descriptor set and layout
        no texture, so that part doesn't exist
    shaders
    input assembly

if I record the command buffer right in the main loop...

*/