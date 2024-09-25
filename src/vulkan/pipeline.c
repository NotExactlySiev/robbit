#include "../common.h"
#include "vulkan.h"
#include <stdio.h>

// TODO: renderpass stuff should probably be separate.
static VkAttachmentDescription default_color_attachment_desc = {
    .format = VK_FORMAT_B8G8R8A8_SRGB,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};

static VkAttachmentDescription default_depth_attachment_desc = {
    .format = VK_FORMAT_D32_SFLOAT_S8_UINT,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
};

VkPipelineRasterizationStateCreateInfo default_raster_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode = VK_POLYGON_MODE_FILL,
    .cullMode = VK_CULL_MODE_BACK_BIT,
    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    .lineWidth = 1.0,
};

VkPipelineMultisampleStateCreateInfo default_multisample_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .minSampleShading = 1.0,
};

VkPipelineColorBlendStateCreateInfo default_colorblend_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
    .logicOp = VK_LOGIC_OP_COPY,
    .attachmentCount = 1,
    .pAttachments = &(VkPipelineColorBlendAttachmentState) {
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    },
    .blendConstants = { 0.0, 0.0, 0.0, 0.0 },
};

static VkShaderModule create_shader_module(const char *file)
{
    FILE *fd = fopen(file, "r");
    fseek(fd, 0, SEEK_END);
    size_t fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    // TODO: reading it like this might cause an issue
    u32 *buf = malloc(fsize);
    assert(fsize == fread(buf, 1, fsize, fd));

    VkShaderModule shader;
    vkCreateShaderModule(ldev, &(VkShaderModuleCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fsize,
        .pCode = buf, 
    }, NULL, &shader);

    free(buf);
    return shader;
}

VkRenderPass default_renderpass;

// basic ass renderpass. one color one depth. one subpass.
static VkRenderPass create_basic_renderpass(void)
{
    VkRenderPass ret;
    VkSubpassDescription subp_descp = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = (VkAttachmentReference[]) {
            {
                .attachment = 0,
                .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            },
        },
        .pDepthStencilAttachment = (VkAttachmentReference[]) {
            {
                .attachment = 1,
                .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            }
        },
    };

    // we don't need to define dependencies if we have only a single
    // subpass. the implicit ones used are fine
    VkRenderPassCreateInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments = (VkAttachmentDescription[]) { 
            default_color_attachment_desc,
            default_depth_attachment_desc,
        },
        .subpassCount = 1,
        .pSubpasses = (VkSubpassDescription[]) {
            subp_descp            
        },
    };
    vkCreateRenderPass(ldev, &renderpass_info, NULL, &ret);
    return ret;
}

Pipeline create_pipeline(u32 stride, VertexAttr *attrs, int nattrs)
{
    Pipeline ret = {0};
    
    default_renderpass = create_basic_renderpass();

    VkVertexInputAttributeDescription attr_descs[nattrs];
    for (int i = 0; i < nattrs; i++) {
        attr_descs[i].location = i;
        attr_descs[i].binding = 0;
        attr_descs[i].format = attrs[i].format;
        attr_descs[i].offset = attrs[i].offset;
    }

    VkPipelineVertexInputStateCreateInfo input_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
            .binding = 0,
            .stride = stride,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        },
        .vertexAttributeDescriptionCount = nattrs,
        .pVertexAttributeDescriptions = attr_descs,
    };

    VkPipelineInputAssemblyStateCreateInfo input_asm_info = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	    .primitiveRestartEnable = VK_FALSE,
    };

    // viewport
    VkPipelineViewportStateCreateInfo viewport_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &(VkViewport) {
            .x = 0.0,
            .y = 0.0,
            .width = surface.cap.currentExtent.width,
            .height = surface.cap.currentExtent.height,
            .minDepth = 0.0,
            .maxDepth = 1.0,
        },
        .scissorCount = 1,
        .pScissors = &(VkRect2D) {
            .extent = surface.cap.currentExtent,
        }
    };

    // TODO: shouldn't be hardcoded
    ret.vert_shader = create_shader_module("normal.vert.spv");
    ret.frag_shader = create_shader_module("normal.frag.spv");

    const VkPipelineShaderStageCreateInfo shader_stages[2] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = ret.vert_shader,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = ret.frag_shader,
            .pName = "main",
        }
    };

    // TODO: shouldn't be hardcoded
    vkCreateDescriptorSetLayout(ldev, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
            {
                .binding = 2,
                .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = MAX_TEXTURES,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        },
    }, NULL, &ret.set);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ret.set,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = (VkPushConstantRange[]) {
            {
                .offset = 0,
                .size = sizeof(PushConst),
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            }
        }
    };
    vkCreatePipelineLayout(ldev, &pipeline_layout_info, NULL, &ret.layout);

    vkCreateGraphicsPipelines(ldev, VK_NULL_HANDLE, 1, &(VkGraphicsPipelineCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &input_state_info,
        .pInputAssemblyState = &input_asm_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &default_raster_info,
        .pMultisampleState = &default_multisample_info,
        .pColorBlendState = &default_colorblend_info,
        .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
            .depthCompareOp = VK_COMPARE_OP_LESS,
        },
        .layout = ret.layout,
        .renderPass = default_renderpass,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
        .pDynamicState = &(VkPipelineDynamicStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            .dynamicStateCount = 2,
            .pDynamicStates = (VkDynamicState[]) {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR,
            },
        },
    }, NULL, &ret.vk);

    return ret;
}

void destroy_pipeline(Pipeline pipe)
{
    vkDestroyDescriptorSetLayout(ldev, pipe.set, NULL);
    vkDestroyPipelineLayout(ldev, pipe.layout, NULL);
    vkDestroyPipeline(ldev, pipe.vk, NULL);
    vkDestroyRenderPass(ldev, default_renderpass, NULL);
    vkDestroyShaderModule(ldev, pipe.vert_shader, NULL);
    vkDestroyShaderModule(ldev, pipe.frag_shader, NULL);
}