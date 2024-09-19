#include <vulkan/vulkan.h>
#include "app.h"

/*

Set
    Allocated from Pool
        Sizes for each type
    Using Layout

*/

void tmp_descriptor_set(void)
{
    VkDescriptorPool pool;
    //
    vkCreateDescriptorPool(ldev, &(VkDescriptorPoolCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 5,
        .poolSizeCount = 4,
        .pPoolSizes = (VkDescriptorPoolSize[]) {
            {   .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 10 },
            {   .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 10 },
            {   .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 10 },
            {   .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 10 },
        }
    }, NULL, &pool);

    VkDescriptorSetLayout layout;
    vkCreateDescriptorSetLayout(ldev, &(VkDescriptorSetLayoutCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 2,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            {
                .binding = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
            {
                .binding = 1,
                .descriptorCount = 30,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            },
        }
    }, NULL, &layout);

    VkDescriptorSet set;
    vkAllocateDescriptorSets(ldev, &(VkDescriptorSetAllocateInfo){
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = pool,
        .descriptorSetCount = 1,
        .pSetLayouts = (VkDescriptorSetLayout[]) { layout },
    }, &set);
}