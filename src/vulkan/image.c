#include <vulkan/vulkan.h>
#include "app.h"

Image create_image(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags use)
{
    Image ret = { .w = w, .h = h, .format = fmt };
    vkCreateImage(ldev, &(VkImageCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {
            .width = w,
            .height = h,
            .depth = 1,
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .format = fmt,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = use,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    }, NULL, &ret.vk);

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(ldev, ret.vk, &req);
    ret.mem = allocate_mem(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkBindImageMemory(ldev, ret.vk, ret.mem, 0);
    return ret;
}

VkImageView image_create_view(Image img, VkImageAspectFlags aspect)
{
    VkImageView ret;
    vkCreateImageView(ldev, &(VkImageViewCreateInfo){
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img.vk,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = img.format,
        .subresourceRange = {
            .aspectMask = aspect,   // TODO: this should be determined by usage flags
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    }, NULL, &ret);
    
    return ret;
}

/*
VkSampler image_create_sampler(App *app, Image img)
{
    VkSampler ret;
    vkCreateSampler(ldev, &(VkSamplerCreateInfo){
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    }, NULL, &ret);
    return ret;
}*/

//Image texture_from_file()

void destroy_image(Image b)
{
    vkDestroyImage(ldev, b.vk, NULL);
    vkFreeMemory(ldev, b.mem, NULL);
}


void image_set_layout(Image *img, VkImageLayout old, VkImageLayout new)
{
    VkCommandBuffer cb = begin_tmp_cmdbuf();
    VkImageMemoryBarrier barrier = {
        // ASSUME: a lot of assumptions here
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = old,
        .newLayout = new,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = img->vk,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &barrier);
    end_tmp_cmdbuf(cb);
}

void image_write(Image *img, void *data)
{
    VkDeviceSize size = img->w * img->h * 2;   // ASSUME: is 1555
    Buffer stage = create_staging_buffer(data, size);

    image_set_layout(img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // copy to the main vertex buffer
    VkCommandBuffer cb = begin_tmp_cmdbuf();
    vkCmdCopyBufferToImage(cb, stage.vk, img->vk, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkBufferImageCopy) {
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .imageExtent = {
            .width = img->w,
            .height = img->h,
            .depth = 1,
        },
    });
    end_tmp_cmdbuf(cb);

    destroy_buffer(stage);
}
