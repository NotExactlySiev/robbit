#ifndef _VULKAN_H
#define _VULKAN_H

#include <string.h>
#include <vulkan/vulkan.h>
#include <SDL.h>

#define     MAX_IMAGES  4

typedef struct {
    VkSwapchainKHR vk;
    uint32_t nimages;
    VkImage images[MAX_IMAGES];
    VkImageView views[MAX_IMAGES];
} Swapchain;

typedef struct {
    VkSurfaceKHR vk;
    VkSurfaceCapabilitiesKHR cap;
} Surface;

// buffer with its memory
typedef struct {
    VkBuffer vk;
    VkDeviceMemory mem;
} Buffer;

// image with its memory
typedef struct {
    VkImage vk;
    VkDeviceMemory mem;
    int w, h;
    VkFormat format;
} Image;

// pipeline and what it runs, give to the present loop (change this)
typedef struct {
    VkPipeline vk;
    VkPipelineLayout layout;
    VkDescriptorSetLayout set;
    VkRenderPass pass;  // only one for now
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
} Pipeline;




// globals
extern SDL_Window *window;
extern VkInstance inst;
extern VkPhysicalDevice pdev;
extern VkDevice ldev;
extern VkQueue queue;  // ASSUMES: only one    
extern Surface surface;
extern Swapchain swapchain;

// pools
extern VkCommandPool cmdpool;


VkCommandBuffer begin_tmp_cmdbuf(void);
void end_tmp_cmdbuf(VkCommandBuffer buf);

VkDeviceMemory allocate_mem(VkMemoryRequirements req, VkMemoryPropertyFlags flags);

Buffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags);
void destroy_buffer(Buffer b);
void buffer_write(Buffer buf, VkDeviceSize size, void *data);
Buffer create_staging_buffer(void *data, VkDeviceSize size);

Image create_image(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags use);
void destroy_image(Image b);
void image_set_layout(Image *img, VkImageLayout old, VkImageLayout new);
void image_write(Image *img, void *data);
VkImageView image_create_view(Image img, VkImageAspectFlags aspect);

#endif