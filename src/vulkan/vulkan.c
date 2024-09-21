#include <types.h>

#include <stdlib.h>
#include <stdio.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL.h>

#include <string.h>

#include "../core/mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "../core/common.h"
#include "app.h"

VkInstance inst;
VkPhysicalDevice pdev;
VkDevice ldev;
VkQueue queue;
Surface surface;
Swapchain swapchain;
VkCommandPool cmdpool;
VkDescriptorPool descpool;

uint32_t max_frames = 4;

void die(const char *msg)
{
    printf("FATAL: %s\n", msg);
    exit(EXIT_FAILURE);
}

VkDevice create_vulkan_device(VkInstance inst, VkSurfaceKHR surface, VkPhysicalDevice *pdev, uint32_t *queue_family_idx);

#define VK_CHECK_ERR(error)    { if (rc != VK_SUCCESS) printf(error ": %d\n", rc); }

VkCommandBuffer begin_tmp_cmdbuf(void)
{
    VkCommandBuffer ret;
    vkAllocateCommandBuffers(ldev, &(VkCommandBufferAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdpool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    }, &ret);
    vkBeginCommandBuffer(ret, &(VkCommandBufferBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,  
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    });

    return ret;
}

void end_tmp_cmdbuf(VkCommandBuffer buf)
{
    vkEndCommandBuffer(buf);
    vkQueueSubmit(queue, 1, &(VkSubmitInfo) {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &buf,
    }, NULL);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(ldev, cmdpool, 1, &buf);
}

Swapchain create_swapchain(Surface surface)
{
    VkResult rc;
    Swapchain ret = {0};

    VkSwapchainCreateInfoKHR swapchainc = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .flags = 0,
        .surface = surface.vk,
        .minImageCount = surface.cap.minImageCount + 1, // what's this for? why?
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = surface.cap.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        // don't have to specify queue families because not sharing
        .preTransform = surface.cap.currentTransform, // TODO: try different transforms
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    rc = vkCreateSwapchainKHR(ldev, &swapchainc, NULL, &ret.vk);
    VK_CHECK_ERR("Can't create swapchain");

    vkGetSwapchainImagesKHR(ldev, ret.vk, &ret.nimages, NULL);
    vkGetSwapchainImagesKHR(ldev, ret.vk, &ret.nimages, ret.images);
    VK_CHECK_ERR("Can't get images from swapchain");

    printf("Retrieved %d images from the swapchain.\n", ret.nimages);

    // TODO: use image_create_view here
    // create a view for each so we can give it to the renderpass
    VkImageViewCreateInfo viewc = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_B8G8R8A8_SRGB,
        .components = { 
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
            VK_COMPONENT_SWIZZLE_IDENTITY,
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    for (int i = 0; i < ret.nimages; i++) {
        viewc.image = ret.images[i];
        rc = vkCreateImageView(ldev, &viewc, NULL, &ret.views[i]);
        VK_CHECK_ERR("Can't create image view");
    }

    return ret;
}

void create_app(SDL_Window *window)
{
    int nexts;
    SDL_Vulkan_GetInstanceExtensions(window, &nexts, NULL);
    char *exts[nexts];
    SDL_Vulkan_GetInstanceExtensions(window, &nexts, exts);

    for (int i = 0; i < nexts; i++)
        printf("%s\n", exts[i]);

    const char *const enabled_layers = "VK_LAYER_KHRONOS_validation";
    VkInstanceCreateInfo createinfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .enabledLayerCount = 1,
        .ppEnabledLayerNames = &enabled_layers,
        .enabledExtensionCount = nexts,
        .ppEnabledExtensionNames = exts,
        .pApplicationInfo = &(VkApplicationInfo) {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .apiVersion = VK_API_VERSION_1_3,
        }
    };
    
    VkResult rc = vkCreateInstance(&createinfo, NULL, &inst);
    if (VK_SUCCESS != rc)
        printf("can't instance uwu %d\n", rc);

    rc = SDL_Vulkan_CreateSurface(window, inst, &surface);
    if (VK_SUCCESS != rc)
        printf("can't surface uwu %d\n", rc);

    uint32_t queue_family_idx;
    ldev = create_vulkan_device(inst, surface.vk, &pdev, &queue_family_idx);
    
    // ok we got a window and a vk instance jesus fuck.
    // and get access to the queue
    vkGetDeviceQueue(ldev, queue_family_idx, 0, &queue);

    // check swapchain capabilities
    uint32_t formats_count;
    uint32_t pmodes_count;

    rc = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdev, surface.vk, &surface.cap);
    VK_CHECK_ERR("can't get surface cap :(");

    swapchain = create_swapchain(surface);
    // command pool and command buffer from the queue
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family_idx,
    };
    
    vkCreateCommandPool(ldev, &command_pool_info, NULL, &cmdpool);

    vkCreateDescriptorPool(ldev, &(VkDescriptorPoolCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 4,
        .poolSizeCount = 2,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 4,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 4,
            },
        },
    }, NULL, &descpool);
}

void destroy_app(void)
{
    vkDestroyCommandPool(ldev, cmdpool, NULL);
    vkDestroyDescriptorPool(ldev, descpool, NULL);

    for (int i = 0; i < swapchain.nimages; i++) {
        vkDestroyImageView(ldev, swapchain.views[i], NULL);
    }

    // call destroy_swapchain and destroy_surface
    vkDestroySwapchainKHR(ldev, swapchain.vk, NULL);
    vkDestroySurfaceKHR(inst, surface.vk, NULL);
    vkDestroyDevice(ldev, NULL);
    vkDestroyInstance(inst, NULL);
}

Image extract_tile(Image *img, i32 x, i32 y, i32 w, i32 h)
{
    VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    Image ret = create_image(w, h, VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR, flags);

    image_set_layout(img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    image_set_layout(&ret, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkCommandBuffer cb = begin_tmp_cmdbuf();
    vkCmdCopyImage(cb, 
        img->vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
        ret.vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 
        1, (VkImageCopy[]) {
            {
                .srcOffset = { x, y, 0 },
                .dstOffset = { 0, 0, 0 },
                .srcSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .dstSubresource = {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .layerCount = 1,
                },
                .extent = { w, h, 1 },
            }
        }
    );
    end_tmp_cmdbuf(cb);

    image_set_layout(&ret, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return ret;
}

typedef struct {
    VkRenderPass pass;
    VkSemaphore semps_img_avl[4];
    VkSemaphore semps_rend_fin[4];
    VkFence queue_fences[4];
    VkFramebuffer framebuffers[4];
    VkCommandBuffer cmdbufs[4];
    VkCommandBuffer current_cmdbuf;
    int current_frame;
    uint32_t image_index;
} PresentContext;

void present_init(PresentContext *ctx, VkRenderPass pass);
void present_terminate(PresentContext *ctx);
void present_acquire(PresentContext *ctx);
void present_submit(PresentContext *ctx);
void present_end_pass(PresentContext *ctx);
VkCommandBuffer present_begin_pass(PresentContext *ctx);


void the_rest_normal(Pipeline *pipe, RobbitObjSet *objs)
{
    //image_set_layout(&teximage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    //VkImageView texview = image_create_view(teximage, VK_IMAGE_ASPECT_COLOR_BIT);

    //Image subimg = extract_tile(&teximage, 128, 64, 64, 64);
    //texview = image_create_view(subimg, VK_IMAGE_ASPECT_COLOR_BIT);


    VkSampler sampler;
    vkCreateSampler(ldev, &(VkSamplerCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    }, NULL, &sampler);

    VkDescriptorSet desc_sets[max_frames];
    vkAllocateDescriptorSets(ldev, &(VkDescriptorSetAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descpool,
        .descriptorSetCount = 4,
        .pSetLayouts = (VkDescriptorSetLayout[]) { pipe->set, pipe->set, pipe->set, pipe->set },
    }, &desc_sets);

    Buffer uniform_buffers[max_frames];
    float *ubo_colors[max_frames];

    // and bind them to the resources. fuck this shit
    for (size_t i = 0; i < max_frames; i++) {
        uniform_buffers[i] = create_buffer(4, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        vkMapMemory(ldev, uniform_buffers[i].mem, 0, 1, 0, &ubo_colors[i]);
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
                        .range = 4,
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


    // ### buffer comes in from the caller, who also records
    //Buffer vert_buf = create_buffer(vert_size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //Buffer index_buf = create_buffer(index_size, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //buffer_write(vert_buf, vert_size, vert_data);
    //buffer_write(index_buf, index_size, index_data);

    PresentContext ctx = {0};
    present_init(&ctx, pipe->pass);

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
        *ubo_colors[ctx.image_index] = t;

        present_acquire(&ctx);
        VkCommandBuffer cbuf = present_begin_pass(&ctx);

        vkCmdBindPipeline(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->vk);        
        vkCmdBindDescriptorSets(cbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->layout, 0, 1, &desc_sets[ctx.image_index], 0, NULL);
        VkDeviceSize offset = 0;
        
        vkCmdBindVertexBuffers(cbuf, 0, 1, &objs->lod[66].vert_buffer.vk, &offset);
        //vkCmdBindVertexBuffers(cbuf, 0, 1, &vert_buf.vk, &offset);
        vkCmdDraw(cbuf, objs->lod[66].vert_count, 1, 0, 0);

        present_end_pass(&ctx);
        present_submit(&ctx);
    }

    present_terminate(&ctx);
    // POINTS
    //destroy_image(teximage);
    //destroy_buffer(vert_buf);
    //destroy_buffer(index_buf);
    
    // destroy_swapchain
    /*for (int i = 0; i < swapchain.nimages; i++) {
        vkDestroyFramebuffer(ldev, framebuffers[i], NULL);
    }*/

    for (int i = 0; i < max_frames; i++) {
        destroy_buffer(uniform_buffers[i]);
    }
}

void destroy_pipeline(Pipeline pipe)
{
    vkDestroyDescriptorSetLayout(ldev, pipe.set, NULL);
    vkDestroyPipelineLayout(ldev, pipe.layout, NULL);
    vkDestroyPipeline(ldev, pipe.vk, NULL);
    vkDestroyRenderPass(ldev, pipe.pass, NULL);
    vkDestroyShaderModule(ldev, pipe.vert_shader, NULL);
    vkDestroyShaderModule(ldev, pipe.frag_shader, NULL);
}

void present_init(PresentContext *ctx, VkRenderPass pass)
{
    ctx->pass = pass;
    Image zimage = create_image(
        surface.cap.currentExtent.width,
        surface.cap.currentExtent.height,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    VkImageView zview = image_create_view(zimage, VK_IMAGE_ASPECT_DEPTH_BIT);
    for (size_t i = 0; i < swapchain.nimages; i++) {
        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = pass,
            .attachmentCount = 2,
            .pAttachments = (VkImageView[]) {
                swapchain.views[i],
                zview,
            },
            .width = surface.cap.currentExtent.width,
            .height = surface.cap.currentExtent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(ldev, &framebuffer_info, NULL, &ctx->framebuffers[i]);
    }

    //VkCommandBuffer command_bufs[swapchain.nimages];
    VkCommandBufferAllocateInfo command_buf_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = cmdpool,
        .commandBufferCount = swapchain.nimages,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    vkAllocateCommandBuffers(ldev, &command_buf_alloc_info, ctx->cmdbufs);

    // make the semaphore
    VkSemaphoreCreateInfo semp_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo queue_fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (int i = 0; i < max_frames; i++) {
        vkCreateSemaphore(ldev, &semp_info, NULL, &ctx->semps_img_avl[i]); 
        vkCreateFence(ldev, &queue_fence_info, NULL, &ctx->queue_fences[i]);
        vkCreateSemaphore(ldev, &semp_info, NULL, &ctx->semps_rend_fin[i]); 
    }
}

void present_terminate(PresentContext *ctx)
{
    vkDeviceWaitIdle(ldev);
    for (int i = 0; i < max_frames; i++) {
        vkDestroySemaphore(ldev, ctx->semps_img_avl[i], NULL);
        vkDestroySemaphore(ldev, ctx->semps_rend_fin[i], NULL);
        vkDestroyFence(ldev, ctx->queue_fences[i], NULL);
    }
}

void present_acquire(PresentContext *ctx)
{
    vkWaitForFences(ldev, 1 ,&ctx->queue_fences[ctx->current_frame], VK_TRUE, UINT64_MAX);
    vkResetFences(ldev, 1, &ctx->queue_fences[ctx->current_frame]);
    vkAcquireNextImageKHR(ldev, swapchain.vk, UINT64_MAX, ctx->semps_img_avl[ctx->current_frame], VK_NULL_HANDLE, &ctx->image_index);

    ctx->current_cmdbuf = ctx->cmdbufs[ctx->image_index];
	vkBeginCommandBuffer(ctx->current_cmdbuf, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    });
}

VkCommandBuffer present_begin_pass(PresentContext *ctx)
{
    vkCmdBeginRenderPass(ctx->current_cmdbuf, &(VkRenderPassBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = ctx->pass,       // begin THIS renderpass
        .framebuffer = ctx->framebuffers[ctx->image_index], // with THESE atachments
        .renderArea.extent = surface.cap.currentExtent,
        .clearValueCount = 2,
        .pClearValues = (VkClearValue[])  {
            {   .color = {0.0f, 0.0f, 0.0f, 0.0f} },
            {   .depthStencil = { .depth = 0.0f } },
        },
    }, VK_SUBPASS_CONTENTS_INLINE);
    return ctx->current_cmdbuf;
}

void present_end_pass(PresentContext *ctx)
{
    vkCmdEndRenderPass(ctx->current_cmdbuf);
    vkEndCommandBuffer(ctx->current_cmdbuf);
}

void present_submit(PresentContext *ctx)
{
    int current_frame = ctx->current_frame;
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = {
	    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx->semps_img_avl[current_frame],
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        //.pCommandBuffers = &cmdbufs[img_index],
        .pCommandBuffers = &ctx->cmdbufs[ctx->image_index],
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &ctx->semps_rend_fin[current_frame],
    };

    vkQueueSubmit(queue, 1, &submit_info, ctx->queue_fences[current_frame]);

    VkPresentInfoKHR present_info = {
	    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &ctx->semps_rend_fin[current_frame],
        .swapchainCount = 1,
        .pSwapchains = &swapchain.vk,
        .pImageIndices = &ctx->image_index,
    };

    vkQueuePresentKHR(queue, &present_info);
    SDL_GL_SwapWindow(window);
    ctx->current_frame = (ctx->current_frame + 1) % max_frames;
}