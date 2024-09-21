#include <types.h>
#include <stdio.h>
#include <SDL.h>

#include "core/common.h"
#include "core/mesh.h"
#include "core/level.h"
#include "vulkan/app.h"

void the_rest_normal(Pipeline *pipe, RobbitObjSet *objs);
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
    //char *str = content_strings[aloha(n)->content];
    char *str;
    if (str)
        printf("\t[%s]", str);
}

SDL_Window *window;
//RobbitMesh meshes_normal[128];
//RobbitMesh meshes_lod[128];

// this should take in an aloha texture
Image to_vulkan_image(void *data, u16 *clut)
{
    u8 *p = data;
    u16 w = (p[4] << 8) | (p[5]);
    u16 h = (p[6] << 8) | (p[7]);
    u8 *indices = data + 8;

    u16 pixels[w*h];
    for (int i = 0; i < w*h; i++) {
        pixels[i] = clut[indices[i]];
    }

    VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    Image ret = create_image(w, h, VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR, flags);
    image_write(&ret, pixels);
    return ret;
}

Image *reps[1 << 21] = {0};

void draw_mesh(VkCommandBuffer cbuf, RobbitMesh *mesh)
{
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cbuf, 0, 1, &mesh->vert_buffer.vk, &offset);
    vkCmdDraw(cbuf, mesh->vert_count, 1, 0, 0);
}

void convert_objset(RobbitObjSet *set, AlohaObjSet *src)
{
    u16 *clut_data = src->clut_node->buf;
    mesh_file_parse(src->mesh_nodes[0]->buf, &set->normal);
    mesh_file_parse(src->mesh_nodes[1]->buf, &set->lod);
    for (int i = 0; i < 128; i++) {
        RobbitMesh *m = &set->lod[i];
        if (m->empty) continue;
        int nfaces = mesh_faces(NULL, m);
        AlohaFace faces[nfaces];
        printf("%d FACES\n");
        mesh_faces(faces, m);
        
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
                printf("PAGE %d ");
                if (tw != 0xE3000000) {
                    u32 xmask = (tw >> 0) & 0x1F;
                    u32 ymask = (tw >> 5) & 0x1F;
                    u32 x = ((tw >> 10) & 0x1F) << 3;
                    u32 y = ((tw >> 15) & 0x1F) << 3;
                    u32 w = ((~(xmask << 3)) + 1) & 0xFF;
                    u32 h = ((~(ymask << 3)) + 1) & 0xFF;
                    printf("REP %d\t%d\t%d\t%d\t", x, y, w, h);

                    u32 key = (tw & 0xFFFFF) | (page << 20);
                    if (reps[key] == NULL) {
                        printf("NEW!\n");
                        reps[key] = 1;
                    }
                }
                printf("\n");

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

extern uint32_t max_frames;

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
    DatFile parsed = {0};
    if (aloha_parse_dat(&parsed, ear->nodes) != 0) {
        printf("parse failed\n");
        exit(EXIT_FAILURE);
    }

    AlohaLevel *level = &parsed.levels[0];
    // TODO: make these structures consistent so a node is always at .node
    EarNode *objs_node = level->objs.node;
    EarNode *geom_node = level->stage.geom_node;

    printf("objs node is %d\n",  objs_node - ear->nodes);
    printf("stage node is %d\n", geom_node - ear->nodes);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN;
    window = SDL_CreateWindow("Robbit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
    create_app(window);

    RobbitObjSet objset;
    convert_objset(&objset, &level->objs);
    dump_objset(&objset);

    
    int nobjs = geom_count(geom_node->buf);
    printf("%d objects\n", nobjs);
    Thing levelobjs[nobjs];
    geom_unpack(geom_node->buf, levelobjs);

    Image teximg = to_vulkan_image(objs_node->sub[5].buf, objs_node->sub[1].buf);

    // convert to the format vulkan likes
    // TODO: should the conversion be a two step process? first flatten the mesh
    // to one array of AlohaFace without groups/subgroups.
    // then apply other adjustments from there (divide by 3 etc.)

    VertexAttr vert_attrs[] = {
        { 0,                                VK_FORMAT_R16G16B16_SNORM },
        { offsetof(RobbitVertex, col),      VK_FORMAT_R8G8B8_UNORM },
        { offsetof(RobbitVertex, normal),   VK_FORMAT_R16G16B16_UNORM },
        { offsetof(RobbitVertex, u),        VK_FORMAT_R8G8_UNORM },
    };
    int nattrs = sizeof(vert_attrs)/sizeof(*vert_attrs);
    Pipeline pipe = create_pipeline(sizeof(RobbitVertex), vert_attrs, nattrs);
    
    //the_rest_normal(&pipe, &objset);
    /*
    image_set_layout(&teximage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    VkImageView texview = image_create_view(teximage, VK_IMAGE_ASPECT_COLOR_BIT);

    Image subimg = extract_tile(&teximage, 128, 64, 64, 64);
    texview = image_create_view(subimg, VK_IMAGE_ASPECT_COLOR_BIT);

    VkSampler sampler;
    vkCreateSampler(ldev, &(VkSamplerCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    }, NULL, &sampler);
    */
    typedef struct {
        float angle;
        float x, y, z;
    } Uniform;

    VkDescriptorSet desc_sets[max_frames];
    vkAllocateDescriptorSets(ldev, &(VkDescriptorSetAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descpool,
        .descriptorSetCount = 4,
        .pSetLayouts = (VkDescriptorSetLayout[]) { pipe.set, pipe.set, pipe.set, pipe.set },
    }, &desc_sets);

    Buffer uniform_buffers[max_frames];
    Uniform *uniforms[max_frames];

    // and bind them to the resources. fuck this shit
    for (int i = 0; i < max_frames; i++) {
        uniform_buffers[i] = create_buffer(sizeof(Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ldev, uniform_buffers[i].mem, 0, sizeof(Uniform), 0, &uniforms[i]);
        // POINTS
        VkWriteDescriptorSet writes[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = &(VkDescriptorBufferInfo[]) {
                    {
                        .buffer = uniform_buffers[i].vk,
                        .offset = 0,
                        .range = sizeof(Uniform),
                    }
                },
            },
            /*{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = &(VkDescriptorImageInfo[]) {
                    {
                        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        .imageView = texview,
                        .sampler = sampler,
                    },
                },
            },*/
        };

        vkUpdateDescriptorSets(ldev, 1, writes, 0, NULL);
    }

    PresentContext ctx = {0};
    present_init(&ctx, pipe.pass);

    bool running = true;
    float t = 0.0;
    int current_frame = 0;
    float zoom = 3.f;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            //ui_process_event(&event);
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = 0;
            } else if (event.type == SDL_MOUSEWHEEL) {
                zoom += 0.07 * event.wheel.preciseY;
                if (zoom < 0.2) zoom = 0.2;                
            } else if (event.type == SDL_QUIT) {
                running = 0;
            }
        }

        t += 0.05;
        uniforms[ctx.image_index]->angle = t;
        //uniforms[ctx.image_index]->x = t*0.001;

        present_acquire(&ctx);
        VkCommandBuffer cbuf = present_begin_pass(&ctx);

        
        vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.vk);        
        vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout, 0, 1, &desc_sets[ctx.image_index], 0, NULL);
        
        for (int i = 0; i < nobjs; i++) {
            //draw_mesh(objs[i], pixels);
            //draw_mesh(cbuf, &objset.lod[i]);
            //uniforms[ctx.image_index]->x = ((float) levelobjs[i].x) / INT16_MAX;
            //uniforms[ctx.image_index]->y = ((float) levelobjs[i].y) / INT16_MAX;
            //uniforms[ctx.image_index]->z = ((float) levelobjs[i].z) / INT16_MAX;
            draw_mesh(cbuf, &objset.lod[levelobjs[i].id]);
        }
        

        present_end_pass(&ctx);
        present_submit(&ctx);
    }

    present_terminate(&ctx);
    //destroy_image(teximage);
    
    // destroy_swapchain
    for (int i = 0; i < swapchain.nimages; i++) {
        vkDestroyFramebuffer(ldev, ctx.framebuffers[i], NULL);
    }

    for (int i = 0; i < max_frames; i++) {
        destroy_buffer(uniform_buffers[i]);
    }



    destroy_objset(&objset);
    destroy_pipeline(pipe);
    destroy_app();

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    
    return 0;
}