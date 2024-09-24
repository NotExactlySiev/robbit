#ifndef _VULKAN_H
#define _VULKAN_H

#include "../common.h"
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <SDL_vulkan.h>
#include <tracy/TracyC.h>

#define MAX_IMAGES  4
#define FRAMES_IN_FLIGHT  2

#define VK_CHECK_ERR(error)    { if (rc != VK_SUCCESS) printf(error ": %d\n", rc); }

// TODO: merge these two
typedef struct {
    VkSwapchainKHR vk;
    uint32_t nimages;
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
    VkImageLayout layout;
} Image;

// pipeline and what it runs, give to the present loop (change this)
typedef struct {
    VkPipeline vk;
    VkPipelineLayout layout;
    VkDescriptorSetLayout set;
    VkShaderModule vert_shader;
    VkShaderModule frag_shader;
} Pipeline;

typedef struct {
    u32 offset;
    VkFormat format;
} VertexAttr;

typedef struct {
    VkSemaphore image_acquired;
    VkSemaphore render_finished;
    VkFence queue_done;
    VkCommandBuffer cbuf;
} PresentContext;

extern u32 curr_frame;

extern VkFramebuffer framebuffers[MAX_IMAGES];
void create_framebuffers(Swapchain *sc);
void destroy_framebuffers(Swapchain *sc);

// globals
extern SDL_Window *window;
extern VkInstance inst;
extern VkPhysicalDevice pdev;
extern VkDevice ldev;
extern u32 queue_family;
extern VkQueue queue;
extern Surface surface;
extern Swapchain swapchain;
extern VkRenderPass default_renderpass;

// pools
extern VkDescriptorPool descpool;
extern VkCommandPool cmdpool;

void create_app(SDL_Window *window);
void destroy_app(void);

Pipeline create_pipeline(u32 stride, VertexAttr *attrs, int nattrs);
void destroy_pipeline(Pipeline pipe);

VkSemaphore create_semaphore(void);
VkFence create_fence(bool signaled);
VkCommandBuffer begin_tmp_cmdbuf(void);
void end_tmp_cmdbuf(VkCommandBuffer buf);

VkDeviceMemory allocate_mem(VkMemoryRequirements req, VkMemoryPropertyFlags flags);

Buffer create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags);
void destroy_buffer(Buffer b);
void buffer_write(Buffer buf, VkDeviceSize size, void *data);
Buffer create_staging_buffer(void *data, VkDeviceSize size);

Image create_image(uint32_t w, uint32_t h, VkFormat fmt, VkImageUsageFlags use);
void destroy_image(Image b);
void image_set_layout(Image *img, VkImageLayout layout);
void image_write(Image *img, void *data);
VkImageView image_create_view(Image img, VkImageAspectFlags aspect);

Swapchain create_swapchain(void);
void destroy_swapchain(Swapchain sc);

void present_init(void);
void present_terminate(void);
VkCommandBuffer present_acquire(void);
void present_submit(void);

// redner commands
typedef struct {
    float x, y, z;
} PushConst;

void push_const(VkCommandBuffer cbuf, Pipeline *pipe, PushConst p);

#endif