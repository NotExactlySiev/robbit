#include "../core/mesh.h"
#include "vulkan.h"

VkInstance inst;
VkPhysicalDevice pdev;
VkDevice ldev;
u32 queue_family;
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

    ldev = create_vulkan_device(inst, surface.vk, &pdev, &queue_family);
    
    vkGetDeviceQueue(ldev, queue_family, 0, &queue);

    swapchain = create_swapchain();
    
    VkCommandPoolCreateInfo command_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family,
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
                .descriptorCount = 16,
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