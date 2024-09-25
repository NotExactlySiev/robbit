#include "vulkan.h"

// abstract swapchain and ALL behind the framebuffer module...? present doesn't
// need to directly worry about the images even.

u32 curr_frame;
static u32 curr_image;
static PresentContext present_ctx[FRAMES_IN_FLIGHT];

#define curr    (&present_ctx[curr_frame])

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

VkCommandBuffer present_acquire(void)
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

void present_submit(void)
{
    ZONE(QueueSubmit);
    vkCmdEndRenderPass(curr->cbuf);
    vkEndCommandBuffer(curr->cbuf);

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
    UNZONE(QueueSubmit);

    ZONE(QueuePresent)
    VkPresentInfoKHR present_info = {
	    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &curr->render_finished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain.vk,
        .pImageIndices = &curr_image,
    };
    vkQueuePresentKHR(queue, &present_info);
    UNZONE(QueuePresent)

    ZONE(Swap)
    SDL_GL_SwapWindow(window);
    UNZONE(Swap)
    TracyCFrameMark
    curr_frame = (curr_frame + 1) % FRAMES_IN_FLIGHT;
}