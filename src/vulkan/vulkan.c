#include <types.h>

#include <stdlib.h>
#include <stdio.h>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <SDL.h>
#include <SDL_vulkan.h>

#include <string.h>

#include "../core/mesh.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

//#include "../core/common.h"
#include "vulkan.h"

VkInstance inst;
VkPhysicalDevice pdev;
VkDevice ldev;
VkQueue queue;
Surface surface;
Swapchain swapchain;
VkCommandPool cmdpool;
VkDescriptorPool descpool;

VkDevice create_vulkan_device(VkInstance inst, VkSurfaceKHR surface, VkPhysicalDevice *pdev, uint32_t *queue_family_idx);

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

void create_app(SDL_Window *window)
{
    VkResult rc;
    uint nexts;
    SDL_Vulkan_GetInstanceExtensions(window, &nexts, NULL);
    const char *exts[nexts];
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
    
    vkCreateInstance(&createinfo, NULL, &inst);
    SDL_Vulkan_CreateSurface(window, inst, &surface.vk);

    uint32_t queue_family_idx;
    ldev = create_vulkan_device(inst, surface.vk, &pdev, &queue_family_idx);
    
    // ok we got a window and a vk instance jesus fuck.
    // and get access to the queue
    vkGetDeviceQueue(ldev, queue_family_idx, 0, &queue);

    swapchain = create_swapchain();
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
        .poolSizeCount = 3,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {
                .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 4,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 4,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = 256,
            },
        },
    }, NULL, &descpool);
}

void destroy_app(void)
{
    vkDestroyCommandPool(ldev, cmdpool, NULL);
    vkDestroyDescriptorPool(ldev, descpool, NULL);
    destroy_swapchain(swapchain);
    vkDestroySurfaceKHR(inst, surface.vk, NULL);
    vkDestroyDevice(ldev, NULL);
    vkDestroyInstance(inst, NULL);
}

Image extract_tile(Image *img, u32 x, u32 y, u32 w, u32 h)
{
    VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    Image ret = create_image(w, h, VK_FORMAT_A1B5G5R5_UNORM_PACK16_KHR, flags);

    image_set_layout(img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    image_set_layout(&ret, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

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

    image_set_layout(&ret, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    image_set_layout(img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    return ret;
}

// this is a part of present_


// basic binary semaphore
VkSemaphore create_semaphore(void)
{
    VkSemaphore ret;
    vkCreateSemaphore(ldev, &(VkSemaphoreCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &(VkSemaphoreTypeCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
            .semaphoreType = VK_SEMAPHORE_TYPE_BINARY,
        },
    }, NULL, &ret);
    return ret;
}

// basic fence. can be initially signaled
VkFence create_fence(bool signaled)
{
    VkFence ret;
    vkCreateFence(ldev, &(VkFenceCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = signaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0,
    }, NULL, &ret);
    return ret;
}

u32 curr_frame;
static u32 curr_image;
static PresentContext present_ctx[FRAMES_IN_FLIGHT];

#define curr    (&present_ctx[curr_frame])

/*
PresentContext *curr(void)
{
    return &present_ctx[curr_frame];
}
*/

//static PresentContext *curr;

void present_init(void)
{
    create_framebuffers(&swapchain);    // this shouldn't be here? go in SC?
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkAllocateCommandBuffers(ldev, &(VkCommandBufferAllocateInfo) {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = cmdpool,
            .commandBufferCount = 1,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        }, &present_ctx[i].cbuf);
        present_ctx[i].image_acquired = create_semaphore();
        present_ctx[i].render_finished = create_semaphore();
        present_ctx[i].queue_done = create_fence(true);
    }
    curr_frame = 0;
}

void present_terminate(void)
{
    vkDeviceWaitIdle(ldev);
    for (int i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(ldev, present_ctx[i].image_acquired, NULL);
        vkDestroySemaphore(ldev, present_ctx[i].render_finished, NULL);
        vkDestroyFence(ldev, present_ctx[i].queue_done, NULL);
    }
    destroy_framebuffers(&swapchain);
}

void destroy_swapchain(Swapchain sc)
{
    vkDestroySwapchainKHR(ldev, sc.vk, NULL);
}

extern SDL_Window *window;

void recreate_swapchain(void)
{
    vkDeviceWaitIdle(ldev);
    destroy_framebuffers(&swapchain);
    destroy_swapchain(swapchain);
    vkDestroySurfaceKHR(inst, surface.vk, NULL);

    SDL_Vulkan_CreateSurface(window, inst, &surface.vk);    
    swapchain = create_swapchain();
    create_framebuffers(&swapchain);
}

void present_acquire(void)
{
    VkResult rc;
    vkWaitForFences(ldev, 1 , &curr->queue_done, VK_TRUE, UINT64_MAX);

again:
    rc = vkAcquireNextImageKHR(ldev, swapchain.vk, UINT64_MAX,
        curr->image_acquired, VK_NULL_HANDLE,
        &curr_image);
    
    switch (rc) {
    case VK_SUCCESS:
        break;
        
    case VK_ERROR_OUT_OF_DATE_KHR:
    case VK_SUBOPTIMAL_KHR:
        vkDestroySemaphore(ldev, curr->image_acquired, NULL);
        curr->image_acquired = create_semaphore();
        recreate_swapchain();
        goto again;
        break;

    default:
        printf("can't acquire. %d\n", rc);
        assert(0);
    }

    vkResetFences(ldev, 1, &curr->queue_done);
	vkBeginCommandBuffer(curr->cbuf, &(VkCommandBufferBeginInfo){
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    });
    vkCmdSetViewport(curr->cbuf, 0, 1, &(VkViewport){
        .x = 0.0,
        .y = 0.0,
        .width = surface.cap.currentExtent.width,
        .height = surface.cap.currentExtent.height,
        .minDepth = 0.0,
        .maxDepth = 1.0,
    });
    vkCmdSetScissor(curr->cbuf, 0, 1, &(VkRect2D) {
        .offset = { 0, 0 },
        .extent = surface.cap.currentExtent,
    });
}

VkCommandBuffer present_begin_pass(void)
{
    vkCmdBeginRenderPass(curr->cbuf, &(VkRenderPassBeginInfo) {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = default_renderpass,
        .framebuffer = framebuffers[curr_image],
        .renderArea.extent = surface.cap.currentExtent,
        .clearValueCount = 2,
        .pClearValues = (VkClearValue[])  {
            { .color = { .float32 = { 0.1f, 0.15f, 0.1f, 0.0f } } },
            { .depthStencil = { .depth = 0.0f } },
        },
    }, VK_SUBPASS_CONTENTS_INLINE);
    return curr->cbuf;
}

void present_end_pass(void)
{
    vkCmdEndRenderPass(curr->cbuf);
    vkEndCommandBuffer(curr->cbuf);
}

void present_submit(void)
{
    VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    vkQueueSubmit(queue, 1, (VkSubmitInfo[]) {{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curr->image_acquired,
        .pWaitDstStageMask = &wait_stage,
        .commandBufferCount = 1,
        .pCommandBuffers = &curr->cbuf,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &curr->render_finished,
    }}, curr->queue_done);

    VkPresentInfoKHR present_info = {
	    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curr->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain.vk,
        .pImageIndices = &curr_image,
    };

    vkQueuePresentKHR(queue, &present_info);
    SDL_GL_SwapWindow(window);
    curr_frame = (curr_frame + 1) % FRAMES_IN_FLIGHT;
}