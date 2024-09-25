#include <SDL.h>
#include <imgui.h>
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

#include "imgui_memory_editor.h"

#include <tracy/Tracy.hpp>

#include <vector>
#include <string>

extern "C" {
#include <types.h>

VkCommandBuffer begin_tmp_cmdbuf(void);
void end_tmp_cmdbuf(VkCommandBuffer buf);

extern SDL_Window *window;
extern VkInstance inst;
extern VkPhysicalDevice pdev;
extern VkDevice ldev;
extern u32 queue_family;
extern VkQueue queue;
extern VkRenderPass default_renderpass;
extern VkDescriptorPool descpool;
extern VkCommandPool cmdpool;

extern u32 min_image_count;

//#include "../vulkan/vulkan.h"
#include "../exact/archive.h"
/*
u32 mesh_file_count(const u8 *data);
u32 mesh_file_parse(const u8 *data, AlohaMesh *out);
void mesh_render(SDL_Renderer *rend, AlohaMesh *mesh, u16 *clut, SDL_Texture*);
void save_to_file(const char* path, EarNode* node);
*/
}

using namespace ImGui;

static void check_vk_result(VkResult err)
{
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

std::string node_get_string(EarNode *node)
{
    if (node->parent == NULL) {
        return std::string("");
    } else {
        u8 index = (node - node->parent->sub);
        return node_get_string(node->parent) 
            + "/" 
            + std::to_string(index);
    }
}

class AssetViewer
{
public:
    SDL_Texture *preview;
    AssetViewer(EarNode*);
    void show_window(void);
protected:
    EarNode *node;
    std::string path;
    MemoryEditor mem_edit;
    virtual void show(void);
    void show_raw(void);
};

AssetViewer::AssetViewer(EarNode *node)
{
    this->node = node;
    this->path = node_get_string(node);
    this->preview = NULL;
}

void AssetViewer::show(void)
{
    Text("Not Implemented Yet!");
}

void AssetViewer::show_window(void)
{

    Begin(path.c_str());
    
    BeginTable("", 2);
    TableNextRow();
    ImGui::TableSetColumnIndex(0);

    if (this->node->type == EAR_NODE_TYPE_FILE) {
        if (Button("Extract")) {
            //uint16_t* clut = (uint16_t*) node->parent->sub[0].buf;
            //mesh_parse(node->buf, clut);
        }
        /*
        SameLine();
        if (Button("Save Raw")) {
            save_to_file("saved.bin", node);
        }

        
        if (Button("From File")) {
            IGFD::FileDialogConfig config;
                    config.path = ".";
            ImGuiFileDialog::Instance()->OpenDialog("ChooseFileDlgKey", "Choose File", ".*", config);
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlgKey")) {
            if (ImGuiFileDialog::Instance()->IsOk()) { // action if OK
                std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
                std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
                std::string fullPath = filePath + filePathName;
                //ear_node_overwrite_from_file(this->node, filePathName.c_str());
            }
            ImGuiFileDialog::Instance()->Close();
        }
        */
        this->show_raw();
    }

    TableSetColumnIndex(1);
    this->show();
    EndTable();
    End();
   
}

void AssetViewer::show_raw(void)
{
    this->mem_edit.DrawContents(node->buf, node->size, true);
}

/*
class ClutViewer : public AssetViewer
{
public:
    ClutViewer(EarNode*);
protected:
    virtual void show(void);
};
    
ClutViewer::ClutViewer(EarNode *node) : AssetViewer(node)
{
    this->preview = SDL_CreateTexture(renderer,
                    SDL_PIXELFORMAT_ABGR1555,
                    SDL_TEXTUREACCESS_STATIC,
                    16, 16);
}

void ClutViewer::show(void)
{
    SDL_UpdateTexture(this->preview, NULL, node->buf, 2*16);
    ImGui::Image(this->preview, ImVec2(6*64, 6*64));
}
*/

void ui_ear_tree(EarNode *node)
{
    bool open;
    for (int i = 0; i < node->count; i++) {
        EarNode *p = &node->sub[i];
        TableNextRow();
        switch (p->type) {
        case EAR_NODE_TYPE_DIR:
            TableNextColumn();
            open = TreeNodeEx((void*)p, ImGuiTreeNodeFlags_SpanFullWidth, "%s", node_get_string(p).c_str());
            TableNextColumn(); ImGui::TextDisabled("--");
            TableNextColumn(); //Text("%s", content_strings[aloha(p)->content]);
            Text("Nothing");
            if (open) {
                ui_ear_tree(p);
                TreePop();
                /*
                if (aloha(p)->content == EAR_CONTENT_ENTITY) {
                    if (aloha(p)->viewer == NULL) {
                        aloha(p)->viewer = new ModelViewer(p);
                    }
                    aloha(p)->viewer->show_window();
                }
                */
            }
            break;
        case EAR_NODE_TYPE_FILE:
            TableNextColumn();
            if (TreeNodeEx(node_get_string(p).c_str(), ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanFullWidth)) {
                /*
                AlohaMetadata *md = aloha(p);
                if (md->viewer == NULL) {
                    AssetViewer *viewer;
                    switch (aloha(p)->content) {
                    case EAR_CONTENT_CLUT:
                        viewer = new ClutViewer(p);
                        break;
                    case EAR_CONTENT_TEXTURE:
                        viewer = new TextureViewer(p);
                        break;
                    default:
                        viewer = new AssetViewer(p);
                    }

                    md->viewer = viewer;
                }
                md->viewer->show_window();
                */
            }
            TableNextColumn(); Text("%d", p->size);
            //TableNextColumn(); Text("%s", content_strings[aloha(p)->content]);
            TableNextColumn();
            break;
        case EAR_NODE_TYPE_SEPARATOR:
            TableNextColumn(); SeparatorText("---");
            TableNextColumn();
            TableNextColumn();
            break;
        }
    }
}

extern "C"
void ear_save_to_file(const char* filename, EarNode* ear);

static VkDescriptorPool         g_DescriptorPool = VK_NULL_HANDLE;

extern "C"
void ui_init(void)
{
    VkResult err;
    IMGUI_CHECKVERSION();
    CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    ImGui_ImplSDL2_InitForVulkan(window);

    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1;
    pool_info.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    err = vkCreateDescriptorPool(ldev, &pool_info, nullptr, &g_DescriptorPool);
    check_vk_result(err);

    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = inst;
    info.PhysicalDevice = pdev;
    info.Device = ldev;
    info.QueueFamily = queue_family;
    info.Queue = queue;
    info.PipelineCache = VK_NULL_HANDLE;
    info.DescriptorPool = g_DescriptorPool;
    info.Subpass = 0;
    info.MinImageCount = 2;
    info.ImageCount = min_image_count;
    info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    info.Allocator = nullptr;
    info.CheckVkResultFn = check_vk_result;
    if (ImGui_ImplVulkan_Init(&info, default_renderpass)) {
        printf("returned true!!!!\n");
    } else {
        printf("oh no!\n");
    }

    VkCommandBuffer command_buffer = begin_tmp_cmdbuf();
    ImGui_ImplVulkan_CreateFontsTexture(command_buffer);
    end_tmp_cmdbuf(command_buffer);
}

extern "C"
void ui_kill(void)
{
    ImGui_ImplVulkan_Shutdown();
}


extern "C"
bool ui_process_event(SDL_Event *event)
{
    ZoneScoped;
    return ImGui_ImplSDL2_ProcessEvent(event);
}

bool show_demo = false;

void main_window(EarNode *ear)
{
    ZoneScoped;
    Begin("Bruh");
    //if (ImGui::Button("Save As..."))
    //    ear_save_to_file("savedas.ear", ear);

    if (Button("What"))
       show_demo = !show_demo;

    // table of contents
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV 
                                 | ImGuiTableFlags_BordersOuterH 
                                 | ImGuiTableFlags_Resizable 
                                 | ImGuiTableFlags_RowBg 
                                 | ImGuiTableFlags_NoBordersInBody;
    BeginTable("files", 3, flags);
    TableSetupColumn("Hexdump");
    TableSetupColumn("Size");
    TableSetupColumn("Type");
    TableHeadersRow();
    ui_ear_tree(ear);
    EndTable();

    End();
}

extern "C"
void ui_run(EarNode *ear, VkCommandBuffer cbuf, VkPipeline pipe)
{
    {
        ZoneScoped;
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        NewFrame();
    }


    // main window
    main_window(ear);
    
    // demo window
    if (show_demo) {
        ZoneScoped;
        ShowDemoWindow();
    }

    Render();
    ImGui_ImplVulkan_RenderDrawData(GetDrawData(), cbuf);
}
