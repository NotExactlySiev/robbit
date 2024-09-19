#include <vulkan/vulkan.h>
#include "app.h"

// creates buffer and its memory
Buffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
{
    Buffer ret = {0};

    // if it's not host visible, we'll be transferring to it (always?)
    if (!(flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
        usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    vkCreateBuffer(ldev, &buf_info, NULL, &ret.vk);
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(ldev, ret.vk, &req);
    
    ret.mem = allocate_mem(req, flags);
    vkBindBufferMemory(ldev, ret.vk, ret.mem, 0);

    return ret;
}

void destroy_buffer(Buffer b)
{
    vkDestroyBuffer(ldev, b.vk, NULL);
    vkFreeMemory(ldev, b.mem, NULL);
}

void buffer_write(Buffer buf, VkDeviceSize size, void *data)
{    
    Buffer stage = create_staging_buffer(data, size);

    // copy to the main vertex buffer
    VkCommandBuffer copy_cb = begin_tmp_cmdbuf();
    vkCmdCopyBuffer(copy_cb, stage.vk, buf.vk, 1, &(VkBufferCopy) {
        .size = size, 
    });
    end_tmp_cmdbuf(copy_cb);

    destroy_buffer(stage);
}

// creates buffer and writes the data to it
Buffer create_staging_buffer(void *data, VkDeviceSize size)
{
    VkResult rc;
    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 
                                | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    Buffer ret = create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, flags);
    void *p;
    rc = vkMapMemory(ldev, ret.mem, 0, size, 0, &p);
    if (VK_SUCCESS != rc)
        printf("can't map mem %d\n", rc);
    memcpy(p, data, size);
    vkUnmapMemory(ldev, ret.mem);
    return ret;
}