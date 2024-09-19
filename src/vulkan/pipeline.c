#include "app.h"
#include "../core/mesh.h"
#include <stdio.h>


static VkAttachmentDescription default_attachment_desc = {
    .format = VK_FORMAT_B8G8R8A8_SRGB,//swapchainc.imageFormat,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
    .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
    .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
};

static VkPipelineVertexInputStateCreateInfo default_vertex_input_state  = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    .vertexBindingDescriptionCount = 1,
    .pVertexBindingDescriptions = &(VkVertexInputBindingDescription) {
        .binding = 0,
        .stride = sizeof(RobbitVertex),  // TODO: don't hardcode
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
    // TODO: we can have per polygon (per instance) color here!
    // this shouldn't be default, should it?
    .vertexAttributeDescriptionCount = 2,
    .pVertexAttributeDescriptions = (VkVertexInputAttributeDescription[]) {
        {
            .location = 0,
            .binding = 0,
            .format = VK_FORMAT_R16G16B16_SNORM,
            .offset = 0,
        },
        {
            .location = 1,
            .binding = 0,
            .format = VK_FORMAT_R8G8B8_UNORM,
            .offset = offsetof(RobbitVertex, col),
        },
    },
};

// rasterizer
VkPipelineRasterizationStateCreateInfo default_raster_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
    .polygonMode=VK_POLYGON_MODE_FILL,
    .cullMode=VK_CULL_MODE_BACK_BIT,
    //.cullMode = VK_CULL_MODE_NONE,
    .frontFace=VK_FRONT_FACE_CLOCKWISE,
    .lineWidth = 1.0,
};

// multisampling
VkPipelineMultisampleStateCreateInfo default_multisample_info = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    .minSampleShading = 1.0,
};

// colorblend
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
    VkResult rc;
    FILE *fd;
    size_t fsize;
    uint32_t *buf;

    fd = fopen(file, "r");
    fseek(fd, 0, SEEK_END);
    fsize = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    buf = malloc(fsize);
    if (fsize != fread(buf, 1, fsize, fd)) {
        return NULL;
    }

    VkShaderModule shader;
    VkShaderModuleCreateInfo shaderc = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fsize,
        .pCode = buf, 
    };

    vkCreateShaderModule(ldev, &shaderc, NULL, &shader);

    free(buf);
    return shader;
}

static create_default_renderpass(VkRenderPass *pass)
{
    // it has a single attachment, the window surface
    // we only have one subpass. it has only a single output
    // attachment which is color
    VkSubpassDescription subp_descp = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &(VkAttachmentReference) {
            .attachment = 0,
            .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        },
    };

    // we don't need to define dependencies if we have only a single
    // subpass. the implicit ones used are fine
    VkRenderPassCreateInfo renderpass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &default_attachment_desc,
        .subpassCount = 1,
        .pSubpasses = &subp_descp,
    };
    vkCreateRenderPass(ldev, &renderpass_info, NULL, pass);
}

Pipeline create_pipeline_points(void)
{
    Pipeline ret = {0};
    VkResult rc;
    
    
    create_default_renderpass(&ret.pass);

    VkPipelineInputAssemblyStateCreateInfo input_asm_info = {
	    .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
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


    // # shader stuff
    ret.vert_shader = create_shader_module("points.vert.spv");
    ret.frag_shader = create_shader_module("points.frag.spv");

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

    // for now have one set that sends a single color
    vkCreateDescriptorSetLayout(ldev, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
        },
    }, NULL, &ret.set);


    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ret.set,
    };
    vkCreatePipelineLayout(ldev, &pipeline_layout_info, NULL, &ret.layout);


    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &default_vertex_input_state,
        .pInputAssemblyState = &input_asm_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &default_raster_info,
        .pMultisampleState = &default_multisample_info,
        .pColorBlendState = &default_colorblend_info,
        .layout = ret.layout,
        .renderPass = ret.pass,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    rc = vkCreateGraphicsPipelines(ldev, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &ret.vk);

    return ret;
}




Pipeline create_pipeline_normal(void)
{
    Pipeline ret = {0};
    VkResult rc;
    
    create_default_renderpass(&ret.pass);

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

    // for now have one set that sends a single color
    vkCreateDescriptorSetLayout(ldev, &(VkDescriptorSetLayoutCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = (VkDescriptorSetLayoutBinding[]) {
            {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            },
            {
                .binding = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            },
        },
    }, NULL, &ret.set);

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &ret.set,
    };
    vkCreatePipelineLayout(ldev, &pipeline_layout_info, NULL, &ret.layout);


    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &default_vertex_input_state,
        .pInputAssemblyState = &input_asm_info,
        .pViewportState = &viewport_info,
        .pRasterizationState = &default_raster_info,
        .pMultisampleState = &default_multisample_info,
        .pColorBlendState = &default_colorblend_info,
        .pDepthStencilState = &(VkPipelineDepthStencilStateCreateInfo) {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_TRUE,
            .depthWriteEnable = VK_TRUE,
        },
        .layout = ret.layout,
        .renderPass = ret.pass,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    rc = vkCreateGraphicsPipelines(ldev, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &ret.vk);

    return ret;
}
