#include <types.h>
#include <stdio.h>
#include <GLFW/glfw3.h>

#include "core/common.h"
#include "core/mesh.h"
#include "core/level.h"
#include "vulkan/app.h"


void the_rest(Pipeline *pipe, AlohaVertex *vert_data, int vert_size, u16 *index_data, int index_size);
void the_rest_normal(Pipeline *pipe, AlohaVertex *vert_data, int vert_size, u16 *index_data, int index_size, u16 *texture, int tex_w, int tex_h);
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

GLFWwindow *window;
AlohaMesh meshes_normal[128];
AlohaMesh meshes_lod[128];
u16 *clut_data;

int main(int argc, char **argv)
{
    /*
    uint16_t tri_index_data[] = {
        85,	81,	83,
        80,	84,	82,
        29,	22,	21,
        29,	21,	28,
        16,	13,	15,
        15,	13,	14,
        14,	13,	12,
        29,	28,	27,
        18,	13,	17,
        20,	13,	19,
        19,	13,	18,
        17,	13,	16,
        29,	25,	24,
        ...
    };

    uint16_t quad_index_data[] = {
        83,	82,	84,	85,
        85,	84,	80,	81,
        81,	80,	82,	83,
        ...
    };

    int tri_count = sizeof tri_index_data / (3 * sizeof *tri_index_data);
    int quad_count = sizeof quad_index_data / (4 * sizeof *quad_index_data);

    int prim_count = tri_count + 2 * quad_count;
    u16 index_data[prim_count][3];

    for (int i = 0; i < tri_count; i++) {
        index_data[i][0] = tri_index_data[3*i + 0] - 1;
        index_data[i][1] = tri_index_data[3*i + 1] - 1;
        index_data[i][2] = tri_index_data[3*i + 2] - 1;
    }

    for (int i = 0; i < quad_count; i++) {
        int j = 2*i + tri_count;
        index_data[j][0] = quad_index_data[4*i + 0] - 1;
        index_data[j][1] = quad_index_data[4*i + 1] - 1;
        index_data[j][2] = quad_index_data[4*i + 2] - 1;

        index_data[j+1][0] = quad_index_data[4*i + 0] - 1;
        index_data[j+1][1] = quad_index_data[4*i + 2] - 1;
        index_data[j+1][2] = quad_index_data[4*i + 3] - 1;
    }*/

    
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
    }

    int nobjs = geom_count(geom_node->buf);
    printf("%d objects\n", nobjs);
    Thing objs[nobjs];
    geom_unpack(geom_node->buf, objs);

    printf("??? entities\n");

    AlohaMesh *house = &meshes_normal[66];
    void *x = house->faces;

    const GLFWvidmode *mode;
    GLFWmonitor *monitor;
    

    if (GLFW_FALSE == glfwInit()) return -1;

    // don't create opengl for window, we're gonna be vulkaning
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(640, 480, "cool window", NULL, NULL);

    create_app(window);
    Pipeline points_pipe = create_pipeline_points();
    Pipeline normal_pipe = create_pipeline_normal();
    
    the_rest(&points_pipe, house->verts, house->nverts * sizeof(AlohaVertex), NULL, 0);

    destroy_pipeline(points_pipe);
    //destroy_pipeline(normal_pipe);
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