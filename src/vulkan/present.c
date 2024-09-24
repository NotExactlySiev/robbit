#include "vulkan.h"

// abstract swapchain and ALL behind the framebuffer module...? present doesn't
// need to directly worry about the images even.

// TODO: why is the framebuffer module here :/
// FIXME: these globals are yucky
static Image zimages[MAX_IMAGES];
static VkImageView colorviews[MAX_IMAGES];
static VkImageView zviews[MAX_IMAGES];
VkFramebuffer framebuffers[MAX_IMAGES];

// void create_framebuffer(VkImage swapchain_image) {...}

// create the framebuffers we need from the swapchain given to this
void create_framebuffers(Swapchain *sc)
{
    VkResult rc;
    // we don't need to keep swapchain images around. they're taken care of
    vkGetSwapchainImagesKHR(ldev, sc->vk, &sc->nimages, NULL);
    VkImage images[sc->nimages];
    vkGetSwapchainImagesKHR(ldev, sc->vk, &sc->nimages, images);
    VK_CHECK_ERR("Can't get images from swapchain");

    VkImageViewCreateInfo view_info = {
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

    for (int i = 0; i < sc->nimages; i++) {
        // abstract this call to create image view
        view_info.image = images[i];
        rc = vkCreateImageView(ldev, &view_info, NULL, &colorviews[i]);
        VK_CHECK_ERR("Can't create image view");

        zimages[i] = create_image(
            // TODO: these should come from the swapchain not outside
            surface.cap.currentExtent.width,
            surface.cap.currentExtent.height,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        zviews[i] = image_create_view(zimages[i], VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = default_renderpass,
            .attachmentCount = 2,
            .pAttachments = (VkImageView[]) {
                colorviews[i],
                zviews[i],
            },
            .width = surface.cap.currentExtent.width,
            .height = surface.cap.currentExtent.height,
            .layers = 1,
        };
        vkCreateFramebuffer(ldev, &framebuffer_info, NULL, &framebuffers[i]);
    }
}

void destroy_framebuffers(Swapchain *sc)
{
    for (int i = 0; i < sc->nimages; i++) {
        vkDestroyFramebuffer(ldev, framebuffers[i], NULL);
        vkDestroyImageView(ldev, colorviews[i], NULL);
        vkDestroyImageView(ldev, zviews[i], NULL);
        destroy_image(zimages[i]);
    }
}