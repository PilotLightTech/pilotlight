/*
   pl_dear_imgui_ext.cpp

   Combined Dear ImGui platform + renderer backend for Pilot Light.
   Multi-viewport/secondary OS windows are intentionally unsupported for now.

   Native Win32/Cocoa/X11 code should feed plIO. This extension consumes plIO
   and does not install or replace native platform callbacks.
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] apis
// [SECTION] structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] platform backend implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

// std libs
#include <float.h>

// pilot light
#include "pl.h"
#include "pl_ds.h"
#include "pl_dear_imgui_ext.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_shader_ext.h"
#include "pl_starter_ext.h"

// imgui/implot
#include "imgui.h"
#include "implot.h"
#include "imgui_internal.h" // ImLerp

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

// apis
static const plGraphicsI*     gptGfx          = nullptr;
static const plDataRegistryI* gptDataRegistry = nullptr;
static const plMemoryI*       gptMemory       = nullptr;
static const plShaderI*       gptShader       = nullptr;
static const plStarterI*      gptStarter      = nullptr;
static const plIOI*           gptIO           = nullptr;

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

#ifndef PL_DS_ALLOC
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

struct plImGuiRenderState
{
    plCommandBuffer* CommandBuffer;
    plShaderHandle Pipeline;
};

// Reusable buffers used for rendering 1 current in-flight frame, for ImGui_ImplVulkan_RenderDrawData()
// [Please zero-clear before use!]
struct plImGuiFrameRenderBuffers
{
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
};

// Each viewport will hold 1 ImGui_ImplVulkanH_WindowRenderBuffers
// [Please zero-clear before use!]
struct plImGuiWindowRenderBuffers
{
    uint32_t Index;
    uint32_t Count;
    ImVector<plImGuiFrameRenderBuffers> FrameRenderBuffers;
};

struct plImGuiTexture
{
    plTextureHandle   tTexture;
    plBindGroupHandle tBindgroup;
    plImGuiTexture() { memset((void*)this, 0, sizeof(*this)); }
};

// Per-viewport renderer storage. Only the main viewport is used by this backend.
struct plImGuiViewportData
{
    plImGuiWindowRenderBuffers RenderBuffers; // Used by all viewports

    plImGuiViewportData() { memset((void*)&RenderBuffers, 0, sizeof(RenderBuffers)); }
    ~plImGuiViewportData() { }
};

struct plImGuiTextureUpload
{
    plBufferHandle    tStagingBuffer;
    plBufferImageCopy tBufferImageCopy;
    plTextureHandle   tTexture;
    size_t            szUploadSize;
};

struct plImGuiGraphicsData
{
    // gfx stuff
    plDevice* ptDevice;
    
    // shaders
    plShaderHandle tMainShader;
    
    // pipeline info
    uint32_t               uImageCount;
    plRenderAttachmentInfo tMainRenderAttachmentInfo;
    plSampleCount          eMainMsaaSamples;

    // texture/samplers management
    plBindGroupPool*        ptBindGroupPool;
    plCommandPool*          ptTextureCommandPool;
    plCommandBuffer*        ptTextureCommandBuffer;
    plSamplerHandle         tTexSamplerLinear;
    plBindGroupLayoutHandle tTextureDescriptorSetLayout;
    plBindGroupLayoutHandle tSamplerDescriptorSetLayout;
    plBindGroupHandle       tSamplerDescriptorSet;
    
    // deferred uploads
    plImGuiTextureUpload* sbtTextureUploads;

    // temporary dynamic block
    plDynamicDataBlock tDynamicDataBlock;
};

struct plImGuiPlatformData
{
    bool abMouseDown[PL_MOUSE_BUTTON_COUNT];
    bool abKeyDown[PL_KEY_COUNT];

    plImGuiPlatformData() { memset((void*)this, 0, sizeof(*this));}
};

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

// platform helpers
static bool          pl__dear_imgui_platform_initialize(void);
static void          pl__dear_imgui_platform_cleanup(void);
static void          pl__dear_imgui_platform_new_frame(void);
static ImGuiKey      pl__dear_imgui_key_to_imgui_key(plKey);
static plMouseCursor pl__dear_imgui_mouse_cursor_to_pl(ImGuiMouseCursor);

// graphics helpers
static void pl__dear_imgui_update_texture(ImTextureData*);
static bool pl__dear_imgui_create_device_objects(void);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_dear_imgui_initialize(plImGuiGraphicsInitInfo* ptInfo)
{

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGuiContext* ptImGuiCtx = ImGui::CreateContext();

    ImGuiMemAllocFunc p_alloc_func = nullptr;
    ImGuiMemFreeFunc p_free_func = nullptr;
    void* p_user_data = nullptr;
    ImGui::GetAllocatorFunctions(&p_alloc_func, &p_free_func, &p_user_data);

    // save to data registry for apps, other extensions, and reloads
    gptDataRegistry->set_data("imgui", ptImGuiCtx);
    gptDataRegistry->set_data("imgui allocate", (void*)p_alloc_func);
    gptDataRegistry->set_data("imgui free", (void*)p_free_func);

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // setup pilot light style
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_Button]                 = ImVec4(0.51f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.61f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.87f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.06f, 0.06f, 0.06f, 0.94f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.25f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.15f, 0.10f, 0.10f, 0.78f);
    colors[ImGuiCol_Border]                 = ImVec4(0.33f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.23f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.59f, 0.98f, 0.40f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.33f, 0.02f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.05f, 0.05f, 0.05f, 0.85f);
    colors[ImGuiCol_ScrollbarGrab]          = colors[ImGuiCol_Button];
    colors[ImGuiCol_ScrollbarGrabHovered]   = colors[ImGuiCol_ButtonHovered];
    colors[ImGuiCol_ScrollbarGrabActive]    = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_CheckMark]              = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_SliderGrab]             = colors[ImGuiCol_Button];
    colors[ImGuiCol_SliderGrabActive]       = colors[ImGuiCol_ButtonActive];
    colors[ImGuiCol_Header]                 = colors[ImGuiCol_Button];
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_Separator]              = colors[ImGuiCol_Border];
    colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
    colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ResizeGrip]             = ImVec4(0.98f, 0.59f, 0.26f, 0.20f);
    colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.98f, 0.59f, 0.26f, 0.67f);
    colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.98f, 0.59f, 0.26f, 0.95f);
    colors[ImGuiCol_InputTextCursor]        = colors[ImGuiCol_Text];
    colors[ImGuiCol_TabHovered]             = colors[ImGuiCol_HeaderHovered];
    colors[ImGuiCol_Tab]                    = ImLerp(colors[ImGuiCol_Header],       colors[ImGuiCol_TitleBgActive], 0.80f);
    colors[ImGuiCol_TabSelected]            = ImLerp(colors[ImGuiCol_HeaderActive], colors[ImGuiCol_TitleBgActive], 0.60f);
    colors[ImGuiCol_TabSelectedOverline]    = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TabDimmed]              = ImLerp(colors[ImGuiCol_Tab],          colors[ImGuiCol_TitleBg], 0.80f);
    colors[ImGuiCol_TabDimmedSelected]      = ImLerp(colors[ImGuiCol_TabSelected],  colors[ImGuiCol_TitleBg], 0.40f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
    colors[ImGuiCol_DockingPreview]         = ImVec4(0.51f, 0.02f, 0.10f, 0.7f);
    colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
    colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
    colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.19f, 0.20f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.23f, 0.25f, 1.00f);   // Prefer using Alpha=1.0 here
    colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
    colors[ImGuiCol_TextLink]               = colors[ImGuiCol_HeaderActive];
    colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
    colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavCursor]              = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);

    // when viewports are enabled we tweak WindowRounding/WindowBg so
    // platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    ImPlotContext *ptImPlotContext = ImPlot::CreateContext();
    gptDataRegistry->set_data("implot", ptImPlotContext);

    IMGUI_CHECKVERSION();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    if(!pl__dear_imgui_platform_initialize())
    {
        IM_ASSERT(0 && "Pilot Light ImGui platform initialization failed!");
    }

    // Setup renderer backend capabilities flags
    plImGuiGraphicsData* bd = IM_NEW(plImGuiGraphicsData)();
    io.BackendRendererUserData = (void*)bd;
    io.BackendRendererName = "imgui_impl_pilotlight_renderer";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;   // We can honor ImGuiPlatformIO::Textures[] requests during render.
    // io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;  // We can create multi-viewports on the Renderer side (optional)

    if(ptInfo)
    {
        bd->tMainRenderAttachmentInfo = *ptInfo->tPipelineInfoMain.ptRenderAttachmentInfo;
        bd->eMainMsaaSamples = ptInfo->tPipelineInfoMain.eMsaaSamples;
        bd->uImageCount = ptInfo->uImageCount;
        bd->ptDevice = ptInfo->ptDevice;
    }
    else // assume user means to use main viewport
    {
        bd->ptDevice = gptStarter->get_device();
        gptStarter->get_render_attachment_info(&bd->tMainRenderAttachmentInfo);
        bd->eMainMsaaSamples = gptGfx->get_swapchain_info(gptStarter->get_swapchain()).eSampleCount;
        gptGfx->get_swapchain_images(gptStarter->get_swapchain(), &bd->uImageCount);
    }

    plBindGroupPoolDesc tDesc = {};
    tDesc.szSampledTextureBindings = 10;
    tDesc.szSamplerBindings = 2;
    bd->ptBindGroupPool = gptGfx->create_bind_group_pool(bd->ptDevice, &tDesc);

    if(!pl__dear_imgui_create_device_objects())
    {
        IM_ASSERT(0 && "ImGui_ImplVulkan_CreateDeviceObjects() failed!"); // <- Can't be hit yet.
    }

    // Our render function expect RendererUserData to be storing the window render buffer we need (for the main viewport we won't use ->Window)
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    main_viewport->RendererUserData = IM_NEW(plImGuiViewportData)();
}

void
pl_dear_imgui_cleanup(void)
{
    ImGuiIO& io = ImGui::GetIO();
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    plImGuiGraphicsData* bd = (plImGuiGraphicsData*)io.BackendRendererUserData;

    if(bd)
    {
        gptGfx->cleanup_bind_group_pool(bd->ptBindGroupPool);
        gptGfx->cleanup_command_pool(bd->ptTextureCommandPool);
    }

    // Only the main viewport has renderer data because multi-viewport is intentionally unsupported.
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    if (plImGuiViewportData* vd = (plImGuiViewportData*)main_viewport->RendererUserData)
        IM_DELETE(vd);
    main_viewport->RendererUserData = nullptr;

    io.BackendRendererName = nullptr;
    io.BackendRendererUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset |
                         ImGuiBackendFlags_RendererHasTextures |
                         ImGuiBackendFlags_RendererHasViewports);
    platform_io.ClearRendererHandlers();
    pl_sb_free(bd->sbtTextureUploads);
    IM_DELETE(bd);

    pl__dear_imgui_platform_cleanup();

    ImPlot::DestroyContext();
    ImGui::DestroyContext();
}

void
pl_dear_imgui_new_frame(plDevice *ptDevice)
{

    plImGuiGraphicsData* bd = (plImGuiGraphicsData*)ImGui::GetIO().BackendRendererUserData;
    IM_ASSERT(bd != nullptr && "Dear ImGui renderer backend is not initialized!");

    if(pl_sb_size(bd->sbtTextureUploads) > 0)
    {
        plCommandBuffer* ptCmdBuffer = gptGfx->request_command_buffer(bd->ptTextureCommandPool, "imgui texture upload");
        gptGfx->begin_command_recording(ptCmdBuffer);
        gptGfx->begin_compute_pass(ptCmdBuffer, NULL);
        for(uint32_t i = 0; i < pl_sb_size(bd->sbtTextureUploads); i++)
        {
            gptGfx->copy_buffer_to_texture(ptCmdBuffer, bd->sbtTextureUploads[i].tStagingBuffer,
                bd->sbtTextureUploads[i].tTexture, 1, &bd->sbtTextureUploads[i].tBufferImageCopy);
        }

        gptGfx->end_compute_pass(ptCmdBuffer);
        gptGfx->end_command_recording(ptCmdBuffer);
        gptGfx->submit_command_buffer(ptCmdBuffer, nullptr);
        gptGfx->wait_on_command_buffer(ptCmdBuffer);
        gptGfx->return_command_buffer(ptCmdBuffer);

        for(uint32_t i = 0; i < pl_sb_size(bd->sbtTextureUploads); i++)
        {
            gptStarter->return_staging_buffer(&bd->sbtTextureUploads[i].tStagingBuffer);
        }
        pl_sb_reset(bd->sbtTextureUploads);
    }

    plDevice* ptFrameDevice = ptDevice ? ptDevice : bd->ptDevice;
    bd->tDynamicDataBlock = gptGfx->allocate_dynamic_data_block(ptFrameDevice);

    // Platform state and input events must be submitted before ImGui::NewFrame().
    pl__dear_imgui_platform_new_frame();
    ImGui::NewFrame();
}

void
pl_dear_imgui_render(plCommandBuffer *ptCommandBuffer)
{
    ImGui::Render();
    ImDrawData* ptMainDrawData = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    int fb_width = (int)(ptMainDrawData->DisplaySize.x * ptMainDrawData->FramebufferScale.x);
    int fb_height = (int)(ptMainDrawData->DisplaySize.y * ptMainDrawData->FramebufferScale.y);
    if (fb_width <= 0 || fb_height <= 0)
        return;

    // Catch up with texture updates. Most of the times, the list will have 1 element with an OK status, aka nothing to do.
    // (This almost always points to ImGui::GetPlatformIO().Textures[] but is part of ImDrawData to allow overriding or disabling texture updates).
    if (ptMainDrawData->Textures != nullptr)
        for (ImTextureData* tex : *ptMainDrawData->Textures)
            if (tex->Status != ImTextureStatus_OK)
                pl__dear_imgui_update_texture(tex);

    plImGuiGraphicsData* bd = (plImGuiGraphicsData*)ImGui::GetIO().BackendRendererUserData;
    plDevice* ptDevice = bd->ptDevice;

    plShaderHandle tShader = bd->tMainShader;
    if (!gptGfx->is_shader_valid(ptDevice, tShader))
        tShader = bd->tMainShader;

    // Allocate array to store enough vertex/index buffers. Each unique viewport gets its own storage.
    plImGuiViewportData* viewport_renderer_data = (plImGuiViewportData*)ptMainDrawData->OwnerViewport->RendererUserData;
    IM_ASSERT(viewport_renderer_data != nullptr);
    plImGuiWindowRenderBuffers* wrb = &viewport_renderer_data->RenderBuffers;
    if (wrb->FrameRenderBuffers.Size == 0)
    {
        wrb->Index = 0;
        wrb->Count = bd->uImageCount;
        wrb->FrameRenderBuffers.resize(wrb->Count);
        memset((void*)wrb->FrameRenderBuffers.Data, 0, wrb->FrameRenderBuffers.size_in_bytes());
    }
    IM_ASSERT(wrb->Count == bd->uImageCount);
    wrb->Index = (wrb->Index + 1) % wrb->Count;
    plImGuiFrameRenderBuffers* rb = &wrb->FrameRenderBuffers[wrb->Index];

    if (ptMainDrawData->TotalVtxCount > 0)
    {
        // Create or resize the vertex/index buffers
        gptStarter->get_staging_buffer(ptMainDrawData->TotalVtxCount * sizeof(ImDrawVert), &rb->tVertexBuffer, "imgui vtx buffer");
        gptStarter->get_staging_buffer(ptMainDrawData->TotalIdxCount * sizeof(ImDrawIdx), &rb->tIndexBuffer, "imgui idx buffer");

        // Upload vertex/index data into a single contiguous GPU buffer
        ImDrawVert* vtx_dst = (ImDrawVert*)gptGfx->get_buffer(ptDevice, rb->tVertexBuffer)->tMemoryAllocation.pHostMapped;
        ImDrawIdx* idx_dst = (ImDrawIdx*)gptGfx->get_buffer(ptDevice, rb->tIndexBuffer)->tMemoryAllocation.pHostMapped;

        for (const ImDrawList* draw_list : ptMainDrawData->CmdLists)
        {
            memcpy(vtx_dst, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(idx_dst, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vtx_dst += draw_list->VtxBuffer.Size;
            idx_dst += draw_list->IdxBuffer.Size;
        }
    }

    typedef struct _plDrawDynamicData
    {
        plVec2 uScale;
        plVec2 uTranslate;
    } plDrawDynamicData;

    // Setup desired graphics state
    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &bd->tDynamicDataBlock, sizeof(plDrawDynamicData));
    // ImGui_ImplVulkan_SetupRenderState(ptMainDrawData, tShader, command_buffer, rb, fb_width, fb_height);
    {
        gptGfx->bind_shader(ptCommandBuffer, tShader);

        // Bind Vertex And Index Buffer:
        if (ptMainDrawData->TotalVtxCount > 0)
        {
            gptGfx->bind_vertex_buffer(ptCommandBuffer, rb->tVertexBuffer);
        }

        // Setup viewport:
        {
            plRenderViewport tViewport = {};
            tViewport.fX = 0.0f;
            tViewport.fY = 0.0f;
            tViewport.fWidth = (float)fb_width;
            tViewport.fHeight = (float)fb_height;
            tViewport.fMinDepth = 0.0f;
            tViewport.fMaxDepth = 1.0f;
            gptGfx->set_viewport(ptCommandBuffer, &tViewport);
        }
        
        // Setup scale and translation:
        // Our visible imgui space lies from draw_data->DisplayPps (top left) to draw_data->DisplayPos+data_data->DisplaySize (bottom right). DisplayPos is (0,0) for single viewport apps.
        {
            plDrawDynamicData* ptDynamicData = (plDrawDynamicData*)tDynamicBinding.pcData;
            ptDynamicData->uScale.x = 2.0f / ptMainDrawData->DisplaySize.x;
            ptDynamicData->uScale.y = 2.0f / ptMainDrawData->DisplaySize.y;
            ptDynamicData->uTranslate.x = -1.0f - ptMainDrawData->DisplayPos.x * ptDynamicData->uScale.x;
            ptDynamicData->uTranslate.y = -1.0f - ptMainDrawData->DisplayPos.y * ptDynamicData->uScale.y;
        }
    }

    // Setup render state structure (for callbacks and custom texture bindings)
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    plImGuiRenderState render_state;
    render_state.CommandBuffer = ptCommandBuffer;
    render_state.Pipeline = tShader;
    platform_io.Renderer_RenderState = &render_state;

    // Will project scissor/clipping rectangles into framebuffer space
    ImVec2 clip_off = ptMainDrawData->DisplayPos;         // (0,0) unless using multi-viewports
    ImVec2 clip_scale = ptMainDrawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    // Render command lists
    // (Because we merged all buffers into a single one, we maintain our own offset into them)
    // VkDescriptorSet last_desc_set = VK_NULL_HANDLE;
    plBindGroupHandle tLastBindGroup = {};
    int global_vtx_offset = 0;
    int global_idx_offset = 0;
    for (const ImDrawList* draw_list : ptMainDrawData->CmdLists)
    {
        for (int cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++)
        {
            const ImDrawCmd* pcmd = &draw_list->CmdBuffer[cmd_i];
            if (pcmd->UserCallback != nullptr)
            {
                // TODO
                // // User callback, registered via ImDrawList::AddCallback()
                // // (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
                // if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                //     ImGui_SetupRenderState(draw_data, pipeline, command_buffer, rb, fb_width, fb_height);
                // else
                //     pcmd->UserCallback(draw_list, pcmd);
                PL_ASSERT(false && "not implmented");
                tLastBindGroup.uData = 0;
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clip_min((pcmd->ClipRect.x - clip_off.x) * clip_scale.x, (pcmd->ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 clip_max((pcmd->ClipRect.z - clip_off.x) * clip_scale.x, (pcmd->ClipRect.w - clip_off.y) * clip_scale.y);

                // Clamp to viewport as set_scissor_region() won't accept values that are off bounds
                if (clip_min.x < 0.0f) { clip_min.x = 0.0f; }
                if (clip_min.y < 0.0f) { clip_min.y = 0.0f; }
                if (clip_max.x > fb_width) { clip_max.x = (float)fb_width; }
                if (clip_max.y > fb_height) { clip_max.y = (float)fb_height; }
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y)
                    continue;

                // Apply scissor/clipping rectangle
                plScissor tScissor = {};
                tScissor.iOffsetX = (int)(clip_min.x);
                tScissor.iOffsetY = (int)(clip_min.y);
                tScissor.uWidth = (uint32_t)(clip_max.x - clip_min.x);
                tScissor.uHeight = (uint32_t)(clip_max.y - clip_min.y);
                gptGfx->set_scissor_region(ptCommandBuffer, &tScissor);

                // Bind DescriptorSet with font or user texture
                plBindGroupHandle tBindGroup = {};
                tBindGroup.uData = pcmd->GetTexID();
                if (tBindGroup.uData != tLastBindGroup.uData)
                {
                    plBindGroupHandle atGroups[] = {bd->tSamplerDescriptorSet, tBindGroup};
                    gptGfx->bind_graphics_bind_groups(ptCommandBuffer, tShader, 0, 2, atGroups, 1, &tDynamicBinding);
                }
                tLastBindGroup.uData = tBindGroup.uData;

                // Draw
                plDrawIndex tDrawIndex = {};
                tDrawIndex.tIndexBuffer = rb->tIndexBuffer;
                tDrawIndex.uIndexStart = pcmd->IdxOffset + global_idx_offset;
                tDrawIndex.uIndexCount = pcmd->ElemCount;
                tDrawIndex.uInstanceCount = 1;
                tDrawIndex.uVertexStart = pcmd->VtxOffset + global_vtx_offset;
                gptGfx->draw_indexed(ptCommandBuffer, 1, &tDrawIndex);
            }
        }
        global_idx_offset += draw_list->IdxBuffer.Size;
        global_vtx_offset += draw_list->VtxBuffer.Size;
    }
    platform_io.Renderer_RenderState = nullptr;

    // Note: at this point both vkCmdSetViewport() and set_scissor_region() have been called.
    // Our last values will leak into user/application rendering IF:
    // - And you forgot to call vkCmdSetViewport() and set_scissor_region() yourself to explicitly set that state.
    // In theory we should aim to backup/restore those values but I am not sure this is possible.
    // We perform a call to set_scissor_region() to set back a full viewport which is likely to fix things for 99% users but technically this is not perfect. (See github #4644)
    plScissor tScissor = {};
    tScissor.iOffsetX = 0;
    tScissor.iOffsetY = 0;
    tScissor.uWidth = (uint32_t)fb_width;
    tScissor.uHeight = (uint32_t)fb_height;
    gptGfx->set_scissor_region(ptCommandBuffer, &tScissor);
}

//-----------------------------------------------------------------------------
// [SECTION] platform backend implementation
//-----------------------------------------------------------------------------

static plImGuiPlatformData*
pl__dear_imgui_get_platform_data(void)
{
    return ImGui::GetCurrentContext() ? (plImGuiPlatformData*)ImGui::GetIO().BackendPlatformUserData : nullptr;
}

static const char*
pl__dear_imgui_get_clipboard_text(ImGuiContext*)
{
    plIO* ptIO = gptIO->get_io();
    if(ptIO->get_clipboard_text_fn)
        return ptIO->get_clipboard_text_fn(ptIO->pClipboardUserData);

    return ptIO->sbcClipboardData ? ptIO->sbcClipboardData : "";
}

static void
pl__dear_imgui_set_clipboard_text(ImGuiContext*, const char* pcText)
{
    plIO* ptIO = gptIO->get_io();
    if(ptIO->set_clipboard_text_fn)
        ptIO->set_clipboard_text_fn(ptIO->pClipboardUserData, pcText);
}

static ImGuiKey
pl__dear_imgui_key_to_imgui_key(plKey tKey)
{
    switch(tKey)
    {
        case PL_KEY_TAB:             return ImGuiKey_Tab;
        case PL_KEY_LEFT_ARROW:      return ImGuiKey_LeftArrow;
        case PL_KEY_RIGHT_ARROW:     return ImGuiKey_RightArrow;
        case PL_KEY_UP_ARROW:        return ImGuiKey_UpArrow;
        case PL_KEY_DOWN_ARROW:      return ImGuiKey_DownArrow;
        case PL_KEY_PAGE_UP:         return ImGuiKey_PageUp;
        case PL_KEY_PAGE_DOWN:       return ImGuiKey_PageDown;
        case PL_KEY_HOME:            return ImGuiKey_Home;
        case PL_KEY_END:             return ImGuiKey_End;
        case PL_KEY_INSERT:          return ImGuiKey_Insert;
        case PL_KEY_DELETE:          return ImGuiKey_Delete;
        case PL_KEY_BACKSPACE:       return ImGuiKey_Backspace;
        case PL_KEY_SPACE:           return ImGuiKey_Space;
        case PL_KEY_ENTER:           return ImGuiKey_Enter;
        case PL_KEY_ESCAPE:          return ImGuiKey_Escape;
        case PL_KEY_LEFT_CTRL:       return ImGuiKey_LeftCtrl;
        case PL_KEY_LEFT_SHIFT:      return ImGuiKey_LeftShift;
        case PL_KEY_LEFT_ALT:        return ImGuiKey_LeftAlt;
        case PL_KEY_LEFT_SUPER:      return ImGuiKey_LeftSuper;
        case PL_KEY_RIGHT_CTRL:      return ImGuiKey_RightCtrl;
        case PL_KEY_RIGHT_SHIFT:     return ImGuiKey_RightShift;
        case PL_KEY_RIGHT_ALT:       return ImGuiKey_RightAlt;
        case PL_KEY_RIGHT_SUPER:     return ImGuiKey_RightSuper;
        case PL_KEY_MENU:            return ImGuiKey_Menu;

        case PL_KEY_0: return ImGuiKey_0;
        case PL_KEY_1: return ImGuiKey_1;
        case PL_KEY_2: return ImGuiKey_2;
        case PL_KEY_3: return ImGuiKey_3;
        case PL_KEY_4: return ImGuiKey_4;
        case PL_KEY_5: return ImGuiKey_5;
        case PL_KEY_6: return ImGuiKey_6;
        case PL_KEY_7: return ImGuiKey_7;
        case PL_KEY_8: return ImGuiKey_8;
        case PL_KEY_9: return ImGuiKey_9;

        case PL_KEY_A: return ImGuiKey_A;
        case PL_KEY_B: return ImGuiKey_B;
        case PL_KEY_C: return ImGuiKey_C;
        case PL_KEY_D: return ImGuiKey_D;
        case PL_KEY_E: return ImGuiKey_E;
        case PL_KEY_F: return ImGuiKey_F;
        case PL_KEY_G: return ImGuiKey_G;
        case PL_KEY_H: return ImGuiKey_H;
        case PL_KEY_I: return ImGuiKey_I;
        case PL_KEY_J: return ImGuiKey_J;
        case PL_KEY_K: return ImGuiKey_K;
        case PL_KEY_L: return ImGuiKey_L;
        case PL_KEY_M: return ImGuiKey_M;
        case PL_KEY_N: return ImGuiKey_N;
        case PL_KEY_O: return ImGuiKey_O;
        case PL_KEY_P: return ImGuiKey_P;
        case PL_KEY_Q: return ImGuiKey_Q;
        case PL_KEY_R: return ImGuiKey_R;
        case PL_KEY_S: return ImGuiKey_S;
        case PL_KEY_T: return ImGuiKey_T;
        case PL_KEY_U: return ImGuiKey_U;
        case PL_KEY_V: return ImGuiKey_V;
        case PL_KEY_W: return ImGuiKey_W;
        case PL_KEY_X: return ImGuiKey_X;
        case PL_KEY_Y: return ImGuiKey_Y;
        case PL_KEY_Z: return ImGuiKey_Z;

        case PL_KEY_F1:  return ImGuiKey_F1;
        case PL_KEY_F2:  return ImGuiKey_F2;
        case PL_KEY_F3:  return ImGuiKey_F3;
        case PL_KEY_F4:  return ImGuiKey_F4;
        case PL_KEY_F5:  return ImGuiKey_F5;
        case PL_KEY_F6:  return ImGuiKey_F6;
        case PL_KEY_F7:  return ImGuiKey_F7;
        case PL_KEY_F8:  return ImGuiKey_F8;
        case PL_KEY_F9:  return ImGuiKey_F9;
        case PL_KEY_F10: return ImGuiKey_F10;
        case PL_KEY_F11: return ImGuiKey_F11;
        case PL_KEY_F12: return ImGuiKey_F12;
        case PL_KEY_F13: return ImGuiKey_F13;
        case PL_KEY_F14: return ImGuiKey_F14;
        case PL_KEY_F15: return ImGuiKey_F15;
        case PL_KEY_F16: return ImGuiKey_F16;
        case PL_KEY_F17: return ImGuiKey_F17;
        case PL_KEY_F18: return ImGuiKey_F18;
        case PL_KEY_F19: return ImGuiKey_F19;
        case PL_KEY_F20: return ImGuiKey_F20;
        case PL_KEY_F21: return ImGuiKey_F21;
        case PL_KEY_F22: return ImGuiKey_F22;
        case PL_KEY_F23: return ImGuiKey_F23;
        case PL_KEY_F24: return ImGuiKey_F24;

        case PL_KEY_APOSTROPHE:    return ImGuiKey_Apostrophe;
        case PL_KEY_COMMA:         return ImGuiKey_Comma;
        case PL_KEY_MINUS:         return ImGuiKey_Minus;
        case PL_KEY_PERIOD:        return ImGuiKey_Period;
        case PL_KEY_SLASH:         return ImGuiKey_Slash;
        case PL_KEY_SEMICOLON:     return ImGuiKey_Semicolon;
        case PL_KEY_EQUAL:         return ImGuiKey_Equal;
        case PL_KEY_LEFT_BRACKET:  return ImGuiKey_LeftBracket;
        case PL_KEY_BACKSLASH:     return ImGuiKey_Backslash;
        case PL_KEY_RIGHT_BRACKET: return ImGuiKey_RightBracket;
        case PL_KEY_GRAVE_ACCENT:  return ImGuiKey_GraveAccent;
        case PL_KEY_CAPS_LOCK:     return ImGuiKey_CapsLock;
        case PL_KEY_SCROLL_LOCK:   return ImGuiKey_ScrollLock;
        case PL_KEY_NUM_LOCK:      return ImGuiKey_NumLock;
        case PL_KEY_PRINT_SCREEN:  return ImGuiKey_PrintScreen;
        case PL_KEY_PAUSE:         return ImGuiKey_Pause;

        case PL_KEY_KEYPAD_0:        return ImGuiKey_Keypad0;
        case PL_KEY_KEYPAD_1:        return ImGuiKey_Keypad1;
        case PL_KEY_KEYPAD_2:        return ImGuiKey_Keypad2;
        case PL_KEY_KEYPAD_3:        return ImGuiKey_Keypad3;
        case PL_KEY_KEYPAD_4:        return ImGuiKey_Keypad4;
        case PL_KEY_KEYPAD_5:        return ImGuiKey_Keypad5;
        case PL_KEY_KEYPAD_6:        return ImGuiKey_Keypad6;
        case PL_KEY_KEYPAD_7:        return ImGuiKey_Keypad7;
        case PL_KEY_KEYPAD_8:        return ImGuiKey_Keypad8;
        case PL_KEY_KEYPAD_9:        return ImGuiKey_Keypad9;
        case PL_KEY_KEYPAD_DECIMAL:  return ImGuiKey_KeypadDecimal;
        case PL_KEY_KEYPAD_DIVIDE:   return ImGuiKey_KeypadDivide;
        case PL_KEY_KEYPAD_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case PL_KEY_KEYPAD_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case PL_KEY_KEYPAD_ADD:      return ImGuiKey_KeypadAdd;
        case PL_KEY_KEYPAD_ENTER:    return ImGuiKey_KeypadEnter;
        case PL_KEY_KEYPAD_EQUAL:    return ImGuiKey_KeypadEqual;

        default: return ImGuiKey_None;
    }
}

static plMouseCursor
pl__dear_imgui_mouse_cursor_to_pl(ImGuiMouseCursor tCursor)
{
    switch(tCursor)
    {
        case ImGuiMouseCursor_Arrow:      return PL_MOUSE_CURSOR_ARROW;
        case ImGuiMouseCursor_TextInput:  return PL_MOUSE_CURSOR_TEXT_INPUT;
        case ImGuiMouseCursor_ResizeAll:  return PL_MOUSE_CURSOR_RESIZE_ALL;
        case ImGuiMouseCursor_ResizeNS:   return PL_MOUSE_CURSOR_RESIZE_NS;
        case ImGuiMouseCursor_ResizeEW:   return PL_MOUSE_CURSOR_RESIZE_EW;
        case ImGuiMouseCursor_ResizeNESW: return PL_MOUSE_CURSOR_RESIZE_NESW;
        case ImGuiMouseCursor_ResizeNWSE: return PL_MOUSE_CURSOR_RESIZE_NWSE;
        case ImGuiMouseCursor_Hand:       return PL_MOUSE_CURSOR_HAND;
        case ImGuiMouseCursor_NotAllowed: return PL_MOUSE_CURSOR_NOT_ALLOWED;
        case ImGuiMouseCursor_None:
        default:                          return PL_MOUSE_CURSOR_NONE;
    }
}

static bool
pl__dear_imgui_platform_initialize(void)
{
    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(gptIO != nullptr && "plIOI is required by the Dear ImGui platform backend!");
    IM_ASSERT(io.BackendPlatformUserData == nullptr && "Already initialized a platform backend!");

    plImGuiPlatformData* bd = IM_NEW(plImGuiPlatformData)();
    io.BackendPlatformUserData = (void*)bd;
    io.BackendPlatformName = "imgui_impl_pilotlight_platform";

    // Pilot Light IO exposes all standard Dear ImGui cursor shapes.
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;

    // Intentionally omitted:
    // - ImGuiBackendFlags_HasSetMousePos: Pilot Light currently has no public cursor-warp API.
    // - ImGuiBackendFlags_PlatformHasViewports: secondary OS windows are not supported here.
    // - ImGuiBackendFlags_HasMouseHoveredViewport: only relevant to multi-viewport operation.

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Platform_GetClipboardTextFn = pl__dear_imgui_get_clipboard_text;
    platform_io.Platform_SetClipboardTextFn = pl__dear_imgui_set_clipboard_text;

    return true;
}

static void
pl__dear_imgui_platform_cleanup(void)
{
    ImGuiIO& io = ImGui::GetIO();
    plImGuiPlatformData* bd = (plImGuiPlatformData*)io.BackendPlatformUserData;
    if(!bd)
        return;

    gptIO->set_mouse_cursor(PL_MOUSE_CURSOR_ARROW);

    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.ClearPlatformHandlers();

    io.BackendPlatformName = nullptr;
    io.BackendPlatformUserData = nullptr;
    io.BackendFlags &= ~(ImGuiBackendFlags_HasMouseCursors |
                         ImGuiBackendFlags_HasSetMousePos |
                         ImGuiBackendFlags_PlatformHasViewports |
                         ImGuiBackendFlags_HasMouseHoveredViewport);

    IM_DELETE(bd);
}

static void
pl__dear_imgui_update_mouse(plImGuiPlatformData* bd, plIO* ptIO)
{
    ImGuiIO& io = ImGui::GetIO();

    const plVec2 tMousePos = gptIO->get_mouse_pos();
    if(gptIO->is_mouse_pos_valid(tMousePos))
        io.AddMousePosEvent(tMousePos.x, tMousePos.y);
    else
        io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);

    for(int iButton = 0; iButton < PL_MOUSE_BUTTON_COUNT; iButton++)
    {
        const plMouseButton tButton = (plMouseButton)iButton;
        const bool bPressed = gptIO->is_mouse_clicked(tButton, false);
        const bool bReleased = gptIO->is_mouse_released(tButton);
        const bool bDown = gptIO->is_mouse_down(tButton);

        // Submit transitions represented by Pilot Light's current per-frame state.
        if(bPressed)
        {
            io.AddMouseButtonEvent(iButton, true);
            bd->abMouseDown[iButton] = true;
        }

        if(bReleased)
        {
            io.AddMouseButtonEvent(iButton, false);
            bd->abMouseDown[iButton] = false;
        }
        else if(!bPressed && bd->abMouseDown[iButton] != bDown)
        {
            io.AddMouseButtonEvent(iButton, bDown);
            bd->abMouseDown[iButton] = bDown;
        }
    }

    // PLATFORM/IO TODO:
    // Add get_mouse_wheel_horizontal() to plIOI and replace the direct internal-field read.
    const float fWheelX = ptIO->_fMouseWheelH;
    const float fWheelY = gptIO->get_mouse_wheel();
    if(fWheelX != 0.0f || fWheelY != 0.0f)
        io.AddMouseWheelEvent(fWheelX, fWheelY);
}

static void
pl__dear_imgui_update_keyboard(plImGuiPlatformData* bd, plIO* ptIO)
{
    ImGuiIO& io = ImGui::GetIO();

    // Submit modifiers before regular keys, matching the official platform backends.
    io.AddKeyEvent(ImGuiMod_Ctrl,  ptIO->bKeyCtrl);
    io.AddKeyEvent(ImGuiMod_Shift, ptIO->bKeyShift);
    io.AddKeyEvent(ImGuiMod_Alt,   ptIO->bKeyAlt);
    io.AddKeyEvent(ImGuiMod_Super, ptIO->bKeySuper);

    for(int iKey = PL_KEY_NAMED_KEY_BEGIN; iKey < PL_KEY_NAMED_KEY_END; iKey++)
    {
        const plKey tKey = (plKey)iKey;
        const ImGuiKey tImGuiKey = pl__dear_imgui_key_to_imgui_key(tKey);
        if(tImGuiKey == ImGuiKey_None)
            continue;

        const int iStateIndex = iKey - PL_KEY_NAMED_KEY_BEGIN;
        const bool bPressed = gptIO->is_key_pressed(tKey, false);
        const bool bReleased = gptIO->is_key_released(tKey);
        const bool bDown = gptIO->is_key_down(tKey);

        // Submit transitions represented by Pilot Light's current per-frame state.
        if(bPressed)
        {
            io.AddKeyEvent(tImGuiKey, true);
            bd->abKeyDown[iStateIndex] = true;
        }

        if(bReleased)
        {
            io.AddKeyEvent(tImGuiKey, false);
            bd->abKeyDown[iStateIndex] = false;
        }
        else if(!bPressed && bd->abKeyDown[iStateIndex] != bDown)
        {
            io.AddKeyEvent(tImGuiKey, bDown);
            bd->abKeyDown[iStateIndex] = bDown;
        }
    }
}

static void
pl__dear_imgui_update_text(plIO* ptIO)
{
    ImGuiIO& io = ImGui::GetIO();

    // PLATFORM/IO TODO:
    // Expose an immutable Unicode codepoint span through plIOI and replace direct access
    // to _sbInputQueueCharacters. Then use io.AddInputCharacter(codepoint) here.
    for(uint32_t i = 0; i < pl_sb_size(ptIO->_sbInputQueueCharacters); i++)
        io.AddInputCharacterUTF16((ImWchar16)ptIO->_sbInputQueueCharacters[i]);
}

static void
pl__dear_imgui_update_mouse_cursor(plImGuiPlatformData* bd)
{
    ImGuiIO& io = ImGui::GetIO();
    if(io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)
        return;

    ImGuiMouseCursor tImGuiCursor = ImGui::GetMouseCursor();
    if(io.MouseDrawCursor || tImGuiCursor == ImGuiMouseCursor_None)
        tImGuiCursor = ImGuiMouseCursor_None;
 
    const plMouseCursor tCursor = pl__dear_imgui_mouse_cursor_to_pl(tImGuiCursor);
    gptIO->set_mouse_cursor(tCursor);
}

static void
pl__dear_imgui_platform_new_frame(void)
{
    plImGuiPlatformData* bd = pl__dear_imgui_get_platform_data();
    IM_ASSERT(bd != nullptr && "Dear ImGui platform backend is not initialized!");

    plIO* ptIO = gptIO->get_io();
    ImGuiIO& io = ImGui::GetIO();

    io.DeltaTime = ptIO->fDeltaTime > 0.0f ? ptIO->fDeltaTime : (1.0f / 60.0f);
    io.DisplaySize = ImVec2(ptIO->tMainViewportSize.x, ptIO->tMainViewportSize.y);
    io.DisplayFramebufferScale = ImVec2(
        ptIO->tMainFramebufferScale.x > 0.0f ? ptIO->tMainFramebufferScale.x : 1.0f,
        ptIO->tMainFramebufferScale.y > 0.0f ? ptIO->tMainFramebufferScale.y : 1.0f);

    // PLATFORM/IO TODO:
    // Expose the ordered plInputEvent stream through plIOI and forward those events
    // directly. Polling state cannot preserve a press+release pair that occurs entirely
    // between two Dear ImGui frames.
    pl__dear_imgui_update_mouse(bd, ptIO);
    pl__dear_imgui_update_keyboard(bd, ptIO);
    pl__dear_imgui_update_text(ptIO);
    pl__dear_imgui_update_mouse_cursor(bd);

    // PLATFORM/IO TODO:
    // Expose main-window focus through plIO or pass the main plWindow to this extension,
    // then call io.AddFocusEvent(focused) on transitions. The platform layer should also
    // release/clear held keys and buttons when focus is lost.
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__dear_imgui_update_texture(ImTextureData* tex)
{
    if (tex->Status == ImTextureStatus_OK)
        return;

    plImGuiGraphicsData* bd = (plImGuiGraphicsData*)ImGui::GetIO().BackendRendererUserData;
    plDevice* ptDevice = bd->ptDevice;

    static bool bFirstRun = true;

    if(bFirstRun)
    {
        bFirstRun = false;
        plBindGroupDesc tBindGroupDesc = {};
        tBindGroupDesc.tLayout = bd->tSamplerDescriptorSetLayout;
        tBindGroupDesc.ptPool = bd->ptBindGroupPool;
        tBindGroupDesc.pcDebugName = "imgui sampler bind group";
        bd->tSamplerDescriptorSet = gptGfx->create_bind_group(ptDevice, &tBindGroupDesc);

        plBindGroupUpdateData tBGData0 = {};
        tBGData0.atSamplerBindings[0].uSlot = 0;
        tBGData0.atSamplerBindings[0].tSampler = bd->tTexSamplerLinear;
        gptGfx->update_bind_group(ptDevice, bd->tSamplerDescriptorSet, &tBGData0);
    }

    if (tex->Status == ImTextureStatus_WantCreate)
    {
        // Create and upload new texture to graphics system
        //IMGUI_DEBUG_LOG("UpdateTexture #%03d: WantCreate %dx%d\n", tex->UniqueID, tex->Width, tex->Height);
        IM_ASSERT(tex->TexID == ImTextureID_Invalid && tex->BackendUserData == nullptr);
        IM_ASSERT(tex->Format == ImTextureFormat_RGBA32);
        plImGuiTexture* backend_tex = IM_NEW(plImGuiTexture)();


        plTextureDesc tTextureDesc = {};
        tTextureDesc.tDimensions.x = (float)tex->Width;
        tTextureDesc.tDimensions.y = (float)tex->Height;
        tTextureDesc.tDimensions.z = 1;
        tTextureDesc.eFormat       = PL_FORMAT_R8G8B8A8_UNORM;
        tTextureDesc.uLayers       = 1;
        tTextureDesc.uMips         = 1;
        tTextureDesc.eType         = PL_TEXTURE_TYPE_2D;
        tTextureDesc.eUsage        = PL_TEXTURE_USAGE_SAMPLED;
        tTextureDesc.pcDebugName   = "imgui texture";

        gptStarter->create_texture(&tTextureDesc, NULL, 0, &backend_tex->tTexture);

        // Create the Descriptor Set
        plBindGroupDesc tBindGroupDesc = {};
        tBindGroupDesc.tLayout = bd->tTextureDescriptorSetLayout;
        tBindGroupDesc.ptPool = bd->ptBindGroupPool;
        tBindGroupDesc.pcDebugName = "imgui texture bind group";
        backend_tex->tBindgroup = gptGfx->create_bind_group(ptDevice, &tBindGroupDesc);

        plBindGroupUpdateData tBGData0 = {};
        tBGData0.atTextureBindings[0].uSlot = 0;
        tBGData0.atTextureBindings[0].eType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
        tBGData0.atTextureBindings[0].tTexture = backend_tex->tTexture;
        gptGfx->update_bind_group(ptDevice, backend_tex->tBindgroup, &tBGData0);
        // backend_tex->DescriptorSet = bd->tFontTextureDescriptorSet;

        // Store identifiers
        tex->SetTexID(backend_tex->tBindgroup.uData);
        tex->BackendUserData = backend_tex;
    }

    if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
    {
        plImGuiTexture* backend_tex = (plImGuiTexture*)tex->BackendUserData;

        plImGuiTextureUpload tUpload = {};
        tUpload.tTexture = backend_tex->tTexture;

        // Update full texture or selected blocks. We only ever write to textures regions which have never been used before!
        // This backend choose to use tex->UpdateRect but you can use tex->Updates[] to upload individual regions.
        // We could use the smaller rect on _WantCreate but using the full rect allows us to clear the texture.
        const int upload_x = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.x;
        const int upload_y = (tex->Status == ImTextureStatus_WantCreate) ? 0 : tex->UpdateRect.y;
        const int upload_w = (tex->Status == ImTextureStatus_WantCreate) ? tex->Width : tex->UpdateRect.w;
        const int upload_h = (tex->Status == ImTextureStatus_WantCreate) ? tex->Height : tex->UpdateRect.h;

        // Create the Upload Buffer:
        int upload_pitch = upload_w * tex->BytesPerPixel;
        int upload_size = upload_h * upload_pitch;
        int stage_pitch = tex->Width * tex->BytesPerPixel;

        // full size of texture
        gptStarter->get_staging_buffer(tex->BytesPerPixel * tex->Width * tex->Height, &tUpload.tStagingBuffer, "imgui texture stage");

        plBuffer* ptStagingBuffer = gptGfx->get_buffer(ptDevice, tUpload.tStagingBuffer);
        char* pcStageLocation = ptStagingBuffer->tMemoryAllocation.pHostMapped + upload_x * tex->BytesPerPixel + stage_pitch * upload_y;
        for (int y = 0; y < upload_h; y++)
            memcpy(pcStageLocation + stage_pitch * y, tex->GetPixelsAt(upload_x, upload_y + y), (size_t)upload_pitch);

        // copy buffer to image
        tUpload.tBufferImageCopy.uImageWidth = (uint32_t)upload_w;
        tUpload.tBufferImageCopy.uImageHeight = (uint32_t)upload_h;
        tUpload.tBufferImageCopy.uImageDepth = 1;
        tUpload.tBufferImageCopy.uLayerCount = 1;
        tUpload.tBufferImageCopy.iImageOffsetX = upload_x;
        tUpload.tBufferImageCopy.iImageOffsetY = upload_y;
        tUpload.tBufferImageCopy.szBufferOffset = upload_x * tex->BytesPerPixel + stage_pitch * upload_y;
        tUpload.tBufferImageCopy.uBufferRowLength = tex->Width;
        tUpload.szUploadSize = upload_size;
        pl_sb_push(bd->sbtTextureUploads, tUpload);
        tex->SetStatus(ImTextureStatus_OK);
    }

    if (tex->Status == ImTextureStatus_WantDestroy)
    {
        // ImGui_ImplVulkan_DestroyTexture(tex);
        plImGuiTexture* backend_tex = (plImGuiTexture*)tex->BackendUserData;
        gptGfx->queue_texture_for_deletion(ptDevice, backend_tex->tTexture);
        gptGfx->queue_bind_group_for_deletion(ptDevice, backend_tex->tBindgroup);
        IM_DELETE(backend_tex);

        // Clear identifiers and mark as destroyed (in order to allow e.g. calling InvalidateDeviceObjects while running)
        tex->SetTexID(ImTextureID_Invalid);
        tex->BackendUserData = nullptr;
        tex->SetStatus(ImTextureStatus_Destroyed);
    }
        
}

static bool
pl__dear_imgui_create_device_objects(void)
{
    plImGuiGraphicsData* bd = (plImGuiGraphicsData*)ImGui::GetIO().BackendRendererUserData;

    plDevice* ptDevice = bd->ptDevice;

    if(!gptGfx->is_sampler_valid(ptDevice, bd->tTexSamplerLinear))
    {
        plSamplerDesc tSamplerDesc = {};
        tSamplerDesc.eMagFilter      = PL_FILTER_LINEAR;
        tSamplerDesc.eMinFilter      = PL_FILTER_LINEAR;
        tSamplerDesc.fMinMip         = -1000.0f;
        tSamplerDesc.fMaxMip         = 1000.0f;
        tSamplerDesc.fMaxAnisotropy  = 1.0f;
        tSamplerDesc.eVAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE;
        tSamplerDesc.eUAddressMode   = PL_ADDRESS_MODE_CLAMP_TO_EDGE;
        tSamplerDesc.eMipmapMode     = PL_MIPMAP_MODE_LINEAR;
        tSamplerDesc.pcDebugName     = "imgui linear sampler";
        bd->tTexSamplerLinear = gptGfx->create_sampler(ptDevice, &tSamplerDesc);
    }

    if(!gptGfx->is_bind_group_layout_valid(ptDevice, bd->tTextureDescriptorSetLayout))
    {
        plBindGroupLayoutDesc tBindGroupLayoutDesc = {};
        tBindGroupLayoutDesc.atTextureBindings[0].uSlot = 0;
        tBindGroupLayoutDesc.atTextureBindings[0].eStages = PL_SHADER_STAGE_FRAGMENT;
        tBindGroupLayoutDesc.atTextureBindings[0].uDescriptorCount = 1;
        tBindGroupLayoutDesc.atTextureBindings[0].eType = PL_TEXTURE_BINDING_TYPE_SAMPLED;
        bd->tTextureDescriptorSetLayout = gptGfx->create_bind_group_layout(ptDevice, &tBindGroupLayoutDesc);
    }

    if(!gptGfx->is_bind_group_layout_valid(ptDevice, bd->tSamplerDescriptorSetLayout))
    {
        plBindGroupLayoutDesc tBindGroupLayoutDesc = {};
        tBindGroupLayoutDesc.atSamplerBindings[0].uSlot = 0;
        tBindGroupLayoutDesc.atSamplerBindings[0].eStages = PL_SHADER_STAGE_FRAGMENT;
        bd->tSamplerDescriptorSetLayout = gptGfx->create_bind_group_layout(ptDevice, &tBindGroupLayoutDesc);
    }

    bool bCreateMainPipeline = !gptGfx->is_shader_valid(ptDevice, bd->tMainShader);

    if(bCreateMainPipeline)
    {
        plShaderDesc tShaderDesc = {0};

        // shaders
        tShaderDesc.tFragmentShader  = gptShader->load_glsl("pl_draw_2d.frag", "main", NULL, NULL);
        tShaderDesc.tVertexShader    = gptShader->load_glsl("pl_draw_2d.vert", "main", NULL, NULL);

        // graphics state
        tShaderDesc.tGraphicsState.bDepthWriteEnabled  = 0;
        tShaderDesc.tGraphicsState.eDepthMode          = PL_COMPARE_MODE_ALWAYS;
        tShaderDesc.tGraphicsState.eCullMode           = PL_CULL_MODE_NONE;
        tShaderDesc.tGraphicsState.bWireframe          = 0;
        tShaderDesc.tGraphicsState.eStencilMode        = PL_COMPARE_MODE_ALWAYS;
        tShaderDesc.tGraphicsState.uStencilRef         = 0xff;
        tShaderDesc.tGraphicsState.uStencilMask        = 0xff;
        tShaderDesc.tGraphicsState.eStencilOpFail      = PL_STENCIL_OP_KEEP;
        tShaderDesc.tGraphicsState.eStencilOpDepthFail = PL_STENCIL_OP_KEEP;
        tShaderDesc.tGraphicsState.eStencilOpPass      = PL_STENCIL_OP_KEEP;

        // vertex buffer
        tShaderDesc.atVertexBufferLayouts[0].uByteStride = sizeof(float) * 5;
        tShaderDesc.atVertexBufferLayouts[0].atAttributes[0].eFormat = PL_VERTEX_FORMAT_FLOAT2;
        tShaderDesc.atVertexBufferLayouts[0].atAttributes[1].eFormat = PL_VERTEX_FORMAT_FLOAT2;
        tShaderDesc.atVertexBufferLayouts[0].atAttributes[2].eFormat = PL_VERTEX_FORMAT_UINT;

        // blend state
        tShaderDesc.atBlendStates[0].bBlendEnabled   = true;
        tShaderDesc.atBlendStates[0].uColorWriteMask = PL_COLOR_WRITE_MASK_ALL;
        tShaderDesc.atBlendStates[0].eSrcColorFactor = PL_BLEND_FACTOR_SRC_ALPHA;
        tShaderDesc.atBlendStates[0].eDstColorFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        tShaderDesc.atBlendStates[0].eColorOp        = PL_BLEND_OP_ADD;
        tShaderDesc.atBlendStates[0].eSrcAlphaFactor = PL_BLEND_FACTOR_SRC_ALPHA;
        tShaderDesc.atBlendStates[0].eDstAlphaFactor = PL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        tShaderDesc.atBlendStates[0].eAlphaOp        = PL_BLEND_OP_ADD;

        // bind group layout
        tShaderDesc.atBindGroupLayouts[0].atSamplerBindings[0].uSlot = 0;
        tShaderDesc.atBindGroupLayouts[0].atSamplerBindings[0].eStages = PL_SHADER_STAGE_FRAGMENT;
        tShaderDesc.atBindGroupLayouts[1].atTextureBindings[0].uSlot = 0;
        tShaderDesc.atBindGroupLayouts[1].atTextureBindings[0].eStages = PL_SHADER_STAGE_FRAGMENT;
        tShaderDesc.atBindGroupLayouts[1].atTextureBindings[0].eType = PL_TEXTURE_BINDING_TYPE_SAMPLED;

        tShaderDesc.eMSAASampleCount = bd->eMainMsaaSamples;
        tShaderDesc.tRenderAttachmentInfo = bd->tMainRenderAttachmentInfo;

        bd->tMainShader = gptGfx->create_shader(ptDevice, &tShaderDesc);
    }

    if(bd->ptTextureCommandPool == nullptr)
    {
        plCommandPoolDesc tDesc = {};
        bd->ptTextureCommandPool = gptGfx->create_command_pool(ptDevice, &tDesc);
    }
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_dear_imgui_ext(const plApiRegistryI *ptApiRegistry, bool bReload)
{
    plDearImGuiI tApi = PL_ZERO_INIT;
    tApi.initialize = pl_dear_imgui_initialize;
    tApi.cleanup    = pl_dear_imgui_cleanup;
    tApi.new_frame  = pl_dear_imgui_new_frame;
    tApi.render     = pl_dear_imgui_render;
    pl_set_api(ptApiRegistry, plDearImGuiI, &tApi);

    gptGfx = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptStarter = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptIO = pl_get_api_latest(ptApiRegistry, plIOI);

    if (bReload)
    {
        ImGuiContext *ptImguiContext = (ImGuiContext*)gptDataRegistry->get_data("imgui");
        ImGui::SetCurrentContext(ptImguiContext);
        ImPlotContext *ptImPlotContext = (ImPlotContext *)gptDataRegistry->get_data("implot");
        ImPlot::SetCurrentContext(ptImPlotContext);

        ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)gptDataRegistry->get_data("imgui allocate");
        ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)gptDataRegistry->get_data("imgui free");
        ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);
    }
}

void
pl_unload_dear_imgui_ext(const plApiRegistryI *ptApiRegistry, bool bReload)
{

    if (bReload)
        return;

    const plDearImGuiI *ptApi = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    ptApiRegistry->remove_api(ptApi);
}
