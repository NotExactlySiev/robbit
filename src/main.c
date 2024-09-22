#include <types.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include "core/robbit.h"
#include "core/mesh.h"
#include "core/level.h"
#include "vulkan/vulkan.h"

void the_rest_normal(Pipeline *pipe, RobbitObjSet *objs);
Pipeline create_pipeline_points();
Pipeline create_pipeline_normal();

#define USAGE   "earie <.ear file>\n"

const char* content_strings[] = {
#define O(NAME, Name, name)     [EAR_CONTENT_##NAME] = #Name,
    CONTENTS
};

SDL_Window *window;

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

void push_const(VkCommandBuffer cbuf, Pipeline *pipe, PushConst p)
{
    vkCmdPushConstants(
        cbuf,
        pipe->layout,
        VK_SHADER_STAGE_VERTEX_BIT,
        0, 
        sizeof(p), 
        &p
    );
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
    assert(fread(buffer, 1, filesize, fd) == filesize);
    fclose(fd);

    Ear *ear = ear_decode(buffer, 0);
    DatFile parsed = {0};
    aloha_parse_dat(&parsed, ear->nodes);

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER);
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_VULKAN;
    window = SDL_CreateWindow("Robbit", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, flags);
    create_app(window);

    RobbitLevel level = {0};
    convert_level(&level, &parsed.levels[0]);
    

    //Image teximg = to_vulkan_image(objs_node->sub[5].buf, objs_node->sub[1].buf);

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
        //float x, y, z;
    } Uniform;

    VkDescriptorSet desc_sets[max_frames];
    vkAllocateDescriptorSets(ldev, &(VkDescriptorSetAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descpool,
        .descriptorSetCount = 4,
        .pSetLayouts = (VkDescriptorSetLayout[]) { pipe.set, pipe.set, pipe.set, pipe.set },
    }, desc_sets);

    Buffer uniform_buffers[max_frames];
    Uniform *uniforms[max_frames];

    // and bind them to the resources. fuck this shit
    for (int i = 0; i < max_frames; i++) {
        uniform_buffers[i] = create_buffer(sizeof(Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ldev, uniform_buffers[i].mem, 0, sizeof(Uniform), 0, (void**) &uniforms[i]);
        VkWriteDescriptorSet writes[2] = {
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_sets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .pBufferInfo = (VkDescriptorBufferInfo[]) {
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

        t += 0.02;

        uniforms[ctx.image_index]->angle = t;

        present_acquire(&ctx);
        VkCommandBuffer cbuf = present_begin_pass(&ctx);

        
        vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.vk);        
        vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout, 0, 1, &desc_sets[ctx.image_index], 0, NULL);
        
        draw_level(cbuf, &pipe, &level);

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

    destroy_level(&level);
    destroy_pipeline(pipe);
    destroy_app();

    SDL_DestroyWindow(window);
    SDL_Vulkan_UnloadLibrary();
    SDL_Quit();
    
    return 0;
}