#include <types.h>
#include <stdio.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include "core/robbit.h"
#include "core/mesh.h"
#include "core/level.h"
#include "vulkan/vulkan.h"

// here's a fun idea: check if the physical device supports 1555 images, and if
// so, upload the images directly into it. if it doesn't, convert and then
// upload.

#define USAGE   "earie <.ear file>\n"

const char* content_strings[] = {
#define O(NAME, Name, name)     [EAR_CONTENT_##NAME] = #Name,
    CONTENTS
};

SDL_Window *window;

Image to_vulkan_image(AlohaTexture *src)
{
    void *bitmap = src->bitmap_node->buf;
    u16 *clut = src->clut_node->buf;
    u8 *p = bitmap;
    u16 w = (p[4] << 8) | (p[5]);
    u16 h = (p[6] << 8) | (p[7]);
    u8 *indices = bitmap + 8;

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

int main(int argc, char **argv)
{    
    if (argc != 2) {
        printf(USAGE);
        return -1;
    }

    TracyCSetThreadName("my own real thread");
    
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

    VertexAttr vert_attrs[] = {
        { 0,                                VK_FORMAT_R16G16B16_SNORM },
        { offsetof(RobbitVertex, col),      VK_FORMAT_R8G8B8_UNORM },
        { offsetof(RobbitVertex, normal),   VK_FORMAT_R16G16B16_UNORM },
        { offsetof(RobbitVertex, tex),      VK_FORMAT_R8G8_UINT },
        { offsetof(RobbitVertex, u),        VK_FORMAT_R16G16_UNORM },
    };
    int nattrs = sizeof(vert_attrs)/sizeof(*vert_attrs);
    Pipeline pipe = create_pipeline(sizeof(RobbitVertex), vert_attrs, nattrs);
    
    VkSampler sampler;
    vkCreateSampler(ldev, &(VkSamplerCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    }, NULL, &sampler);
    
    typedef struct {
        float angle;
        float zoom;
        float ratio;
    } Uniform;

    VkDescriptorSet desc_sets[FRAMES_IN_FLIGHT];
    vkAllocateDescriptorSets(ldev, &(VkDescriptorSetAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descpool,
        .descriptorSetCount = 4,
        .pSetLayouts = (VkDescriptorSetLayout[]) { pipe.set, pipe.set, pipe.set, pipe.set },
    }, desc_sets);

    Buffer uniform_buffers[FRAMES_IN_FLIGHT];
    Uniform *uniforms[FRAMES_IN_FLIGHT];

    // TODO: we should be able to just tell the vulkan module what type of
    // uniform we want and just be given a pointer at present time along
    // with the command buffer. (current frame present context should be
    // returned as a seperate struct?)
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        uniform_buffers[i] = create_buffer(sizeof(Uniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ldev, uniform_buffers[i].mem, 0, sizeof(Uniform), 0, (void**) &uniforms[i]);

        RobbitTexture *textures = &level.objs.texture;

        VkWriteDescriptorSet writes[3] = {
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
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_sets[i],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 1,
                .pImageInfo = (VkDescriptorImageInfo[]) {
                    {
                        .sampler = sampler,
                    },
                },
            },
            {
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = desc_sets[i],
                .dstBinding = 2,
                .dstArrayElement = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = textures->n,
            },
        };

        VkDescriptorImageInfo image_infos[textures->n];
        writes[2].pImageInfo = image_infos;
        
        for (int i = 0; i < textures->n; i++) {
            image_infos[i] = (VkDescriptorImageInfo) {
                .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                .imageView = textures->views[i],
                .sampler = VK_NULL_HANDLE,
            };
        }

        vkUpdateDescriptorSets(ldev, 3, writes, 0, NULL);
    }

    present_init();

    bool running = true;
    float t = 1.0;
    float zoom = 5.f;
    int mesh_id = 14;

    // TODO: enum this
    int view_mode = 0;

    while (running) {
        TracyCFrameMarkStart("Frame");

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = 0;
                } else if (event.key.keysym.sym == SDLK_RIGHT) {
                    do {
                        mesh_id = (mesh_id + 1) % 128;
                    } while (level.objs.normal[mesh_id].empty);
                    printf("MESH %d\n", mesh_id);
                } else if (event.key.keysym.sym == SDLK_TAB) {
                    view_mode = 1 - view_mode;
                    if (view_mode == 0) {
                        zoom = 2.5f;
                    } else {
                        zoom = 5.0f;
                    }
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
                    // works fine for now, don't need this.
                    //framebuffer_resized = false;
                }
                break;

            case SDL_MOUSEWHEEL:
                zoom += 0.18 * event.wheel.preciseY;
                if (zoom < 0.2) zoom = 0.2;
                break;

            case SDL_QUIT:
                running = 0;
                break;

            default:
                //printf("Unhandled Event: %d\n", event.type);
                break;
            }
        }

        t += 0.02;
        // TODO: uniforms should also be managed by the present module?
        uniforms[curr_frame]->angle = t;
        uniforms[curr_frame]->zoom = zoom;

        present_acquire();
        // TODO: current extent should be easier to access
        uniforms[curr_frame]->ratio = 
            (float) surface.cap.currentExtent.height / surface.cap.currentExtent.width;

        VkCommandBuffer cbuf = present_begin_pass();

        // TODO: wrappity wrap wrap
        vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.vk);        
        vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.layout, 0, 1, &desc_sets[curr_frame], 0, NULL);
        // TODO: why do we need to pass the pipeline into this? I just wanna
        // push some numbers
        switch (view_mode) {
        case 0:
            draw_level(cbuf, &pipe, &level);
            break;
        case 1:
            push_const(cbuf, &pipe, (PushConst) { 0, 0, 0 });
            draw_mesh(cbuf, &level.objs.normal[mesh_id]);
        }

        present_end_pass();
        present_submit();

        TracyCFrameMarkEnd("Frame");
    }

    present_terminate();
    vkDestroySampler(ldev, sampler, NULL);

    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
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