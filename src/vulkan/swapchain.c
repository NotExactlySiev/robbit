#include "vulkan.h"

Swapchain create_swapchain(void)
{
    VkResult rc;
    Swapchain ret = {0};

    
    // check swapchain capabilities
    rc = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pdev, surface.vk, &surface.cap);
    VK_CHECK_ERR("can't get surface cap :(");

    VkSwapchainCreateInfoKHR swapchainc = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .flags = 0,
        .surface = surface.vk,
        .minImageCount = surface.cap.minImageCount + 1,
        .imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageExtent = surface.cap.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        // don't have to specify queue families because not sharing
        .preTransform = surface.cap.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    rc = vkCreateSwapchainKHR(ldev, &swapchainc, NULL, &ret.vk);
    VK_CHECK_ERR("Can't create swapchain");

    return ret;
}