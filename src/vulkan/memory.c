//#include <vulkan/vulkan.h>
#include "app.h"

VkDeviceMemory allocate_mem(VkMemoryRequirements req, VkMemoryPropertyFlags flags)
{
    VkPhysicalDeviceMemoryProperties mem_type;
    vkGetPhysicalDeviceMemoryProperties(pdev, &mem_type);
    
    // find a suitable memory type for our buffer
    int mem_type_index = -1;
    for (int i = 0; i < mem_type.memoryTypeCount; i++) {
        if (req.memoryTypeBits & (1 << i) && (mem_type.memoryTypes[i].propertyFlags & flags) == flags) {
            mem_type_index = i;
            break;
        }
    }

    if (mem_type_index == -1) {
        printf("oh no can't allocate mem\n");
        return NULL;
    }

    // found one! (hopefully)
    VkDeviceMemory ret;
    vkAllocateMemory(ldev, &(VkMemoryAllocateInfo) {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = mem_type_index,
    }, NULL, &ret);

    return ret;
}