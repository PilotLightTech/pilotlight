/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define PL_MATH_INCLUDE_FUNCTIONS
#include <string.h> // memset

// core
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_os.h"
#include "pl_memory.h"
#include "pl_math.h"

// extensions
#include "pl_image_ext.h"
#include "pl_vulkan_ext.h"
#include "pl_ecs_ext.h"
#include "pl_draw_ext.h"
#include "pl_ui_ext.h"
#include "pl_stats_ext.h"
#include "pl_debug_ext.h"

// backends
#include "pl_vulkan.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// forward declarations
typedef struct _plMaterialInfo  plMaterialInfo;
typedef struct _plGlobalInfo    plGlobalInfo;

// graphics
typedef struct _plRenderTarget     plRenderTarget;
typedef struct _plRenderTargetDesc plRenderTargetDesc;

// renderer
typedef struct _plRenderer plRenderer;
typedef struct _plScene       plScene;

typedef struct _plGlobalInfo
{
    plVec4 tAmbientColor;

    float  fTime;
    int    _unused[3];
    plVec4 tLightColor;
    plVec4 tLightPos;
    
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraViewProj;

} plGlobalInfo;

typedef struct _plMaterialInfo
{
    plVec4 tAlbedo;
} plMaterialInfo;

typedef struct _plAppData
{

    // object
    plBindGroup     tBindGroup1;
    uint32_t        uBindGroup2;
    plMesh          tMesh;
    uint32_t        uShaderVariant;
    plGraphicsState tGraphicsState;
    plObjectInfo    tInfo;

    // mesh component
    plVec3*      sbtVertexPositions;
    plVec3*      sbtVertexNormals;
    plVec4*      sbtVertexTangents;
    plVec4*      sbtVertexColors0;
    uint32_t*    sbuIndices;

    // vulkan
    plRenderBackend tBackend;
    plGraphics      tGraphics;
    plDrawList      drawlist;
    plDrawList3D    drawlist3d;
    plDrawLayer*    fgDrawLayer;
    plDrawLayer*    bgDrawLayer;
    plFontAtlas     fontAtlas;
    bool            bShowUiDemo;
    bool            bShowUiDebug;
    bool            bShowUiStyle;

    // allocators
    plTempAllocator tTempAllocator;
    
    // renderer
    plComponentLibrary tComponentLibrary;

    // cameras
    plCameraComponent tCamera;

    plDebugApiInfo     tDebugInfo;
    bool               bVSyncChanged;
    
    // global data
    plBindGroup     tGlobalBindGroup;
    uint32_t        uGlobalVertexData;
    float*          sbfGlobalVertexData;
    uint32_t        uGlobalMaterialData;
    plMaterialInfo* sbtGlobalMaterialData;

    uint32_t uGlobalVtxDataOffset;

    // global vertex/index buffers
    plVec3*   sbtVertexData;
    uint32_t* sbuIndexData;
    uint32_t  uIndexBuffer;
    uint32_t  uVertexBuffer;
        
    uint32_t uDynamicBuffer0_Offset;
    uint32_t uDynamicBuffer0;

    // renderer

    // object bind groups
    plBindGroup* sbtObjectBindGroups;
    plHashMap    tObjectBindGroupdHashMap;

    // shaders
    uint32_t uMainShader;

} plAppData;

void pla_add_pine_tree(plAppData* ptAppData, plVec3 tPos, float fRadius, float fHeight, plVec4 tTrunkColor, plVec4 tLeafColor)
{

    uint32_t uInitialIndex = pl_sb_size(ptAppData->sbtVertexPositions);

    // trunk
    for(uint32_t i = 0; i < 12; i++)
        pl_sb_push(ptAppData->sbtVertexColors0, tTrunkColor);

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x, tPos.y,  tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius + tPos.x, tPos.y, -fRadius + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius + tPos.x, tPos.y,  fRadius + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x,           tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){            tPos.x, fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,            tPos.y, -fRadius + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,           tPos.y, -fRadius + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){           tPos.x, fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,           tPos.y,  fRadius + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x,           tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius + tPos.x,           tPos.y,  fRadius + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){            tPos.x, fHeight + tPos.y,            tPos.z}));

    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 1);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 2);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 3);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 4);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 5);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 6);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 7);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 8);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 9);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 10);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 11);

    // leaves
    for(uint32_t i = 0; i < 12; i++)
        pl_sb_push(ptAppData->sbtVertexColors0, tLeafColor);

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius * 3.0f + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius * 3.0f + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){                   tPos.x,         fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius* 3.0f + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius* 3.0f + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){                  tPos.x,         fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius* 3.0f + tPos.z}));

    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius* 3.0f + tPos.z}));
    pl_sb_push(ptAppData->sbtVertexPositions, ((plVec3){                   tPos.x,         fHeight + tPos.y,            tPos.z}));

    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 12);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 13);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 14);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 15);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 16);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 17);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 18);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 19);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 20);
    
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 21);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 22);
    pl_sb_push(ptAppData->sbuIndices, uInitialIndex + 23);
}

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plDeviceApiI*            gptDevice            = NULL;
const plRenderBackendI*        gptBackend           = NULL;
const plGraphicsApiI*          gptGfx               = NULL;
const plDrawApiI*              gptDraw              = NULL;
const plVulkanDrawApiI*        gptVulkanDraw        = NULL;
const plUiApiI*                gptUi                = NULL;
const plCameraI*               gptCamera            = NULL;
const plStatsApiI*             gptStats             = NULL;
const plDebugApiI*             gptDebug             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

void         pl__show_main_window(plAppData* ptAppData);
VkSurfaceKHR pl__create_surface  (VkInstance tInstance, plIOContext* ptIoCtx);

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(const plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_io_context(gptDataRegistry->get_data(PL_CONTEXT_IO_NAME));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
        gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
        gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDraw       = ptApiRegistry->first(PL_API_DRAW);
        gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
        gptUi         = ptApiRegistry->first(PL_API_UI);
        gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
        gptStats      = ptApiRegistry->first(PL_API_STATS);
        gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

        return ptAppData;
    }

    // allocate intial app memory
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    ptAppData->tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL;
    ptAppData->tGraphicsState.ulDepthWriteEnabled  = true;
    ptAppData->tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT;
    ptAppData->tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA;
    ptAppData->tGraphicsState.ulShaderTextureFlags = 0;
    ptAppData->tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS;
    ptAppData->tGraphicsState.ulStencilRef         = 0xff;
    ptAppData->tGraphicsState.ulStencilMask        = 0xff;
    ptAppData->tGraphicsState.ulStencilOpFail      = PL_STENCIL_OP_KEEP;
    ptAppData->tGraphicsState.ulStencilOpDepthFail = PL_STENCIL_OP_KEEP;
    ptAppData->tGraphicsState.ulStencilOpPass      = PL_STENCIL_OP_KEEP;

    // add contexts to data registry (so extensions can use them)
    gptDataRegistry->set_data("profile", pl_create_profile_context());
    gptDataRegistry->set_data("log", pl_create_log_context());

    // load extensions
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptExtensionRegistry->load_from_config(ptApiRegistry, "../apps/pl_config.json");

    // load global apis
    gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
    gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDraw       = ptApiRegistry->first(PL_API_DRAW);
    gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
    gptUi         = ptApiRegistry->first(PL_API_UI);
    gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
    gptStats      = ptApiRegistry->first(PL_API_STATS);
    gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    
    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();

    // setup backend
    gptBackend->setup(ptApiRegistry, ptBackend, VK_API_VERSION_1_2, true);

    // create surface
    ptBackend->tSurface = pl__create_surface(ptBackend->tInstance, ptIoCtx);

    // create & init device
    gptBackend->create_device(ptBackend, ptBackend->tSurface, true, ptDevice);
    gptDevice->init(ptApiRegistry, ptDevice, 2);

    // create swapchain
    ptGraphics->tSwapchain.bVSync = true;
    gptBackend->create_swapchain(ptBackend, ptDevice, ptBackend->tSurface, (uint32_t)ptIoCtx->afMainViewportSize[0], (uint32_t)ptIoCtx->afMainViewportSize[1], &ptGraphics->tSwapchain);
    
    // setup graphics
    ptGraphics->ptBackend = ptBackend;
    gptGfx->setup(ptGraphics, ptBackend, ptApiRegistry, &ptAppData->tTempAllocator);
    
    // setup drawing api
    const plVulkanInit tVulkanInit = {
        .tPhysicalDevice  = ptDevice->tPhysicalDevice,
        .tLogicalDevice   = ptDevice->tLogicalDevice,
        .uImageCount      = ptGraphics->tSwapchain.uImageCount,
        .tRenderPass      = ptDevice->sbtRenderPasses[ptGraphics->uRenderPass]._tRenderPass,
        .tMSAASampleCount = ptGraphics->tSwapchain.tMsaaSamples,
        .uFramesInFlight  = ptGraphics->uFramesInFlight
    };
    gptVulkanDraw->initialize_context(&tVulkanInit);
    plDrawContext* ptDrawCtx = gptDraw->get_context();
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist);
    gptDraw->register_3d_drawlist(ptDrawCtx, &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    gptDraw->add_default_font(&ptAppData->fontAtlas);
    gptDraw->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    gptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tCamera = (plCameraComponent){
        .tPos         = (plVec3){-6.211f, 3.647f, 0.827f},
        .fNearZ       = 0.01f,
        .fFarZ        = 400.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1]
    };
    gptCamera->set_pitch_yaw(&ptAppData->tCamera, -0.244f, 1.488f);

    // create tree
    pla_add_pine_tree(ptAppData, (plVec3){0}, 0.25f, 5.0f, (plVec4){0.57f, 0.35f, 0.24f, 1.0f}, (plVec4){0.10f, 0.35f, 0.10f, 1.0f});

    // calculate normals
    pl_sb_resize(ptAppData->sbtVertexNormals, pl_sb_size(ptAppData->sbtVertexPositions));
    for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbuIndices) - 2; i += 3)
    {
        const uint32_t uIndex0 = ptAppData->sbuIndices[i + 0];
        const uint32_t uIndex1 = ptAppData->sbuIndices[i + 1];
        const uint32_t uIndex2 = ptAppData->sbuIndices[i + 2];

        const plVec3 tP0 = ptAppData->sbtVertexPositions[uIndex0];
        const plVec3 tP1 = ptAppData->sbtVertexPositions[uIndex1];
        const plVec3 tP2 = ptAppData->sbtVertexPositions[uIndex2];

        const plVec3 tEdge1 = pl_sub_vec3(tP1, tP0);
        const plVec3 tEdge2 = pl_sub_vec3(tP2, tP0);

        const plVec3 tNorm = pl_cross_vec3(tEdge1, tEdge2);

        ptAppData->sbtVertexNormals[uIndex0] = tNorm;
        ptAppData->sbtVertexNormals[uIndex1] = tNorm;
        ptAppData->sbtVertexNormals[uIndex2] = tNorm;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // prepare scene

    {

        // update material data
        const plMaterialInfo tMaterialInfo = {
            .tAlbedo = {1.0f, 1.0f, 1.0f, 1.0f}
        };
        pl_sb_push(ptAppData->sbtGlobalMaterialData, tMaterialInfo);
        ptAppData->tInfo.uMaterialIndex = pl_sb_size(ptAppData->sbtGlobalMaterialData) - 1;
        
        // add mesh to global buffers
        
        // add primary vertex data
        {
            // current location in global buffers
            ptAppData->tMesh.uIndexOffset = pl_sb_size(ptAppData->sbuIndexData);
            ptAppData->tMesh.uIndexCount = pl_sb_size(ptAppData->sbuIndices);
            ptAppData->tMesh.uVertexOffset = pl_sb_size(ptAppData->sbtVertexData);
            ptAppData->tMesh.uVertexCount = pl_sb_size(ptAppData->sbtVertexPositions);
            ptAppData->tInfo.uVertexPosOffset = ptAppData->tMesh.uVertexOffset;
            
            // copy data to global buffer
            pl_sb_add_n(ptAppData->sbuIndexData, ptAppData->tMesh.uIndexCount);
            pl_sb_add_n(ptAppData->sbtVertexData, ptAppData->tMesh.uVertexCount);
            memcpy(&ptAppData->sbuIndexData[ptAppData->tMesh.uIndexOffset], ptAppData->sbuIndices, sizeof(uint32_t) * ptAppData->tMesh.uIndexCount);
            memcpy(&ptAppData->sbtVertexData[ptAppData->tMesh.uVertexOffset], ptAppData->sbtVertexPositions, sizeof(plVec3) * ptAppData->tMesh.uVertexCount); 
        }

        // add secondary vertex data
        {
            // update global vertex buffer offset
            ptAppData->tInfo.uVertexDataOffset = ptAppData->uGlobalVtxDataOffset / 4;

            // stride within storage buffer
            uint32_t uStride = 0;

            // calculate vertex stream mask based on provided data
            if(pl_sb_size(ptAppData->sbtVertexNormals) > 0)             { uStride += 4; ptAppData->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
            if(pl_sb_size(ptAppData->sbtVertexTangents) > 0)            { uStride += 4; ptAppData->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
            if(pl_sb_size(ptAppData->sbtVertexColors0) > 0)             { uStride += 4; ptAppData->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }

            pl_sb_add_n(ptAppData->sbfGlobalVertexData, uStride * ptAppData->tMesh.uVertexCount);

            // current attribute offset
            uint32_t uOffset = 0;

            // normals
            for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtVertexNormals); i++)
            {
                ptAppData->sbtVertexNormals[i] = pl_norm_vec3(ptAppData->sbtVertexNormals[i]);
                const plVec3* ptNormal = &ptAppData->sbtVertexNormals[i];
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + 0] = ptNormal->x;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + 1] = ptNormal->y;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + 2] = ptNormal->z;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + 3] = 0.0f;
            }

            if(ptAppData->tMesh.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
                uOffset += 4;

            // tangents
            for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtVertexTangents); i++)
            {
                const plVec4* ptTangent = &ptAppData->sbtVertexTangents[i];
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTangent->x;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTangent->y;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptTangent->z;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptTangent->w;
            }

            if(pl_sb_size(ptAppData->sbtVertexTangents) > 0)
                uOffset += 4;

            // tangents
            for(uint32_t i = 0; i < pl_sb_size(ptAppData->sbtVertexColors0); i++)
            {
                const plVec4* ptColor = &ptAppData->sbtVertexColors0[i];
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptColor->x;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptColor->y;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptColor->z;
                ptAppData->sbfGlobalVertexData[ptAppData->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptColor->w;
            }

            if(pl_sb_size(ptAppData->sbtVertexColors0) > 0)
                uOffset += 4;

            PL_ASSERT(uOffset == uStride && "sanity check");

            ptAppData->uGlobalVtxDataOffset += uStride * ptAppData->tMesh.uVertexCount;

            // update material vertex stream to match actual mesh
            ptAppData->tGraphicsState.ulVertexStreamMask = ptAppData->tMesh.ulVertexStreamMask;
        }
    }

    // update cpu buffer and upload to cpu buffers

    // create new storage global & index & vertex buffers
    ptAppData->uGlobalVertexData   = gptDevice->create_storage_buffer(ptDevice, pl_sb_size(ptAppData->sbfGlobalVertexData) * sizeof(float), ptAppData->sbfGlobalVertexData, "global vertex data");
    ptAppData->uGlobalMaterialData = gptDevice->create_storage_buffer(ptDevice, pl_sb_size(ptAppData->sbtGlobalMaterialData) * sizeof(plMaterialInfo), ptAppData->sbtGlobalMaterialData, "global material data");
    ptAppData->uIndexBuffer        = gptDevice->create_index_buffer(ptDevice, sizeof(uint32_t) * pl_sb_size(ptAppData->sbuIndexData), ptAppData->sbuIndexData, "global index buffer");
    ptAppData->uVertexBuffer       = gptDevice->create_vertex_buffer(ptDevice, sizeof(plVec3) * pl_sb_size(ptAppData->sbtVertexData), sizeof(plVec3), ptAppData->sbtVertexData, "global vertex buffer");

    // update global bind group

    plBindGroupLayout tGlobalGroupLayout =
    {
        .uBufferCount = 3,
        .uTextureCount = 0,
        .aBuffers = {
            {
                .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot       = 0,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            },
            {
                .tType       = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot       = 1,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT
            },
            {
                .tType       = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot       = 2,
                .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
            }
        }
    };
        
    // create & update global bind group
    ptAppData->tGlobalBindGroup.tLayout = tGlobalGroupLayout;

    ptAppData->uDynamicBuffer0 = gptDevice->create_constant_buffer(ptDevice, ptDevice->uUniformBufferBlockSize, "renderer dynamic buffer 0");

    uint32_t atBuffers0[] = {ptAppData->uDynamicBuffer0, ptAppData->uGlobalVertexData, ptAppData->uGlobalMaterialData};
    size_t aszRangeSizes[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE, VK_WHOLE_SIZE};
    gptGfx->update_bind_group(ptGraphics, &ptAppData->tGlobalBindGroup, 3, atBuffers0, aszRangeSizes, 0, NULL);

    // prepare material GPU data
    {

        //~~~~~~~~~~~~~~~~~~~~~~~~create shader descriptions~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // main
        plShaderDesc tMainShaderDesc = {
            .pcPixelShader  = "phong.frag.spv",
            .pcVertexShader = "primitive.vert.spv",
            .tGraphicsState = {
                .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
                .ulDepthMode          = PL_DEPTH_MODE_LESS,
                .ulBlendMode          = PL_BLEND_MODE_ALPHA,
                .ulCullMode           = VK_CULL_MODE_NONE,
                .ulDepthWriteEnabled  = VK_TRUE,
                .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
                .ulStencilMode        = PL_STENCIL_MODE_ALWAYS
            },
            .uBindGroupLayoutCount = 3,
            .atBindGroupLayouts                  = {
                {
                    .uBufferCount = 3,
                    .aBuffers = {
                        { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT },
                        { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                    },
                },
                {
                    .uBufferCount = 1,
                    .aBuffers = {
                        { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                    },
                    .uTextureCount = 3,
                    .aTextures     = {
                        { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                        { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 2, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                        { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 3, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT },
                    }
                },
                {
                    .uBufferCount  = 1,
                    .aBuffers      = {
                        { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                    }
                }
            },   
        };
        ptAppData->uMainShader = gptGfx->create_shader(ptGraphics, &tMainShaderDesc);


        VkSampleCountFlagBits tMSAASampleCount = ptGraphics->tSwapchain.tMsaaSamples;

        uint32_t sbuTextures[16] = {0};

        ptAppData->tBindGroup1.tLayout = *gptGfx->get_bind_group_layout(ptGraphics, ptAppData->uMainShader, 1);
        gptGfx->update_bind_group(ptGraphics, &ptAppData->tBindGroup1, 0, NULL, NULL, 3, sbuTextures);
            
        // find variants
        ptAppData->uShaderVariant = UINT32_MAX;

        const plShader* ptShader = &ptGraphics->sbtShaders[ptAppData->uMainShader];   
        const uint32_t uVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

        for(uint32_t k = 0; k < uVariantCount; k++)
        {
            plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[k].tGraphicsState;
            if(ptVariant.ulValue == ptAppData->tGraphicsState.ulValue && ptShader->tDesc.sbtVariants[k].uRenderPass == ptGraphics->uRenderPass && tMSAASampleCount == ptShader->tDesc.sbtVariants[k].tMSAASampleCount)
            {
                    ptAppData->uShaderVariant = ptShader->_sbuVariantPipelines[k];
                    break;
            }
        }

        // create variant that matches texture count, vertex stream, and culling
        if(ptAppData->uShaderVariant == UINT32_MAX)
            ptAppData->uShaderVariant = gptGfx->add_shader_variant(ptGraphics, ptAppData->uMainShader, ptAppData->tGraphicsState, ptGraphics->uRenderPass, tMSAASampleCount);
    }

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{

    plGraphics*      ptGraphics = &ptAppData->tGraphics;
    plRenderBackend* ptBackend  = &ptAppData->tBackend;
    plDevice*        ptDevice   = &ptGraphics->tDevice;

    vkDeviceWaitIdle(ptGraphics->tDevice.tLogicalDevice);

    gptDraw->cleanup_font_atlas(&ptAppData->fontAtlas);
    gptDraw->cleanup_context();
    gptUi->destroy_context(NULL);

    // clean up scene
    pl_sb_free(ptAppData->sbfGlobalVertexData);
    pl_sb_free(ptAppData->sbtGlobalMaterialData);
    pl_sb_free(ptAppData->sbtVertexData);
    pl_sb_free(ptAppData->sbuIndexData);

    // clean up renderer
    pl_sb_free(ptAppData->sbtObjectBindGroups);
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptBackend->cleanup_swapchain(ptBackend, ptDevice, &ptGraphics->tSwapchain);
    gptBackend->cleanup_device(&ptGraphics->tDevice);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    pl_temp_allocator_free(&ptAppData->tTempAllocator);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    plDevice*           ptDevice           = &ptGraphics->tDevice;

    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();
    gptGfx->resize(ptGraphics);
    gptCamera->set_aspect(&ptAppData->tCamera, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1]);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    pl_begin_profile_frame();
    pl_begin_profile_sample(__FUNCTION__);

    // for convience
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plDevice*   ptDevice   = &ptGraphics->tDevice;

    // contexts
    plIOContext*   ptIoCtx      = pl_get_io_context();
    plDrawContext* ptDrawCtx    = gptDraw->get_context();

    gptStats->new_frame();

    {
        static double* pdFrameTimeCounter = NULL;
        if(!pdFrameTimeCounter)
            pdFrameTimeCounter = gptStats->get_counter("frame rate");
        *pdFrameTimeCounter = (double)ptIoCtx->fFrameRate;
    }

    if(ptAppData->bVSyncChanged)
    {
        gptGfx->resize(&ptAppData->tGraphics);
        ptAppData->bVSyncChanged = false;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) gptCamera->translate(&ptAppData->tCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) gptCamera->translate(&ptAppData->tCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) gptCamera->translate(&ptAppData->tCamera, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) gptCamera->translate(&ptAppData->tCamera,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) gptCamera->translate(&ptAppData->tCamera,  0.0f, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) gptCamera->translate(&ptAppData->tCamera,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);

    plFrameContext* ptCurrentFrame = gptGfx->get_frame_resources(ptGraphics);
    
    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("frame");
    *pdFrameTimeCounter = (double)ptDevice->uCurrentFrame;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(gptGfx->begin_frame(ptGraphics))
    {
        pl_begin_profile_sample("process_cleanup_queue");
        gptDevice->process_cleanup_queue(&ptGraphics->tDevice, (uint32_t)ptGraphics->szCurrentFrameIndex);
        pl_end_profile_sample();

        bool bOwnMouse = ptIoCtx->bWantCaptureMouse;
        gptUi->new_frame();

        if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
            gptCamera->rotate(&ptAppData->tCamera,  -tMouseDelta.y * 0.1f * ptIoCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIoCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        gptCamera->update(&ptAppData->tCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        const plMat4 tTransform0 = pl_identity_mat4();
        gptDraw->add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);

        // ui
        gptDraw->add_3d_point(&ptAppData->drawlist3d, (plVec3){-3.0f, 3.0f, 0.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 0.1f, 0.01f);

        pl__show_main_window(ptAppData);

        gptDebug->show_windows(&ptAppData->tDebugInfo);

        if(ptAppData->bShowUiDemo)
        {
            pl_begin_profile_sample("ui demo");
            gptUi->demo(&ptAppData->bShowUiDemo);
            pl_end_profile_sample();
        }
            
        if(ptAppData->bShowUiStyle)
            gptUi->style(&ptAppData->bShowUiStyle);

        if(ptAppData->bShowUiDebug)
            gptUi->debug(&ptAppData->bShowUiDebug);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~renderer begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        gptDraw->submit_layer(ptAppData->bgDrawLayer);
        gptDraw->submit_layer(ptAppData->fgDrawLayer);
        
        gptGfx->begin_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~scene prep~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // reset scene
        ptAppData->uDynamicBuffer0_Offset = 0;
        ptAppData->tInfo.tModel = pl_identity_mat4();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        static const VkClearValue atClearValues[2] = 
        {
            { .color.float32 = { 0.0f, 0.0f, 0.0f, 1.0f}},
            {.depthStencil = { .depth = 1.0f, .stencil = 0}}    
        };

        // set viewport
        const VkViewport tViewport = {
            .x        = 0.0f,
            .y        = 0.0f,
            .width    = (float)ptGraphics->tSwapchain.tExtent.width,
            .height   = (float)ptGraphics->tSwapchain.tExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);

        // set scissor
        const VkRect2D tDynamicScissor = {
            .extent = {
                .width    = ptGraphics->tSwapchain.tExtent.width,
                .height   = ptGraphics->tSwapchain.tExtent.height,
            }
        };
        vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tDynamicScissor);

        const VkRenderPassBeginInfo tRenderPassBeginInfo = {
            .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .renderPass      = ptDevice->sbtRenderPasses[ptGraphics->uRenderPass]._tRenderPass,
            .framebuffer     = ptDevice->sbtFrameBuffers[ptGraphics->tSwapchain.puFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex]]._tFrameBuffer,
            .renderArea      = {
                           .extent = {
                               .width  = ptGraphics->tSwapchain.tExtent.width,
                               .height = ptGraphics->tSwapchain.tExtent.height,
                           }
                       },
            .clearValueCount = 2,
            .pClearValues    = atClearValues
        };
        vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &tRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        // prepare gpu data
        {
            pl_begin_profile_sample("prepare gpu data");

            plObjectSystemData* ptObjectSystemData = ptAppData->tComponentLibrary.tObjectComponentManager.pSystemData;

            pl_begin_profile_sample(__FUNCTION__);
            
            const uint32_t uDynamicBufferIndex = gptDevice->request_dynamic_buffer(ptDevice);
            plDynamicBufferNode* ptDynamicBufferNode = &ptDevice->_sbtDynamicBufferList[uDynamicBufferIndex];
            plBuffer* ptBuffer = &ptDevice->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];

            // check if bind group for this buffer exist
            uint64_t uObjectBindGroupIndex = pl_hm_lookup(&ptAppData->tObjectBindGroupdHashMap, (uint64_t)uDynamicBufferIndex);

            if(uObjectBindGroupIndex == UINT64_MAX) // doesn't exist
            {
                plBindGroup tNewBindGroup = {
                    .tLayout = {
                        .uBufferCount = 1,
                        .aBuffers      = {
                            { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
                        }  
                    }
                };
                size_t szRangeSize2 = sizeof(plObjectInfo);
                gptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 1, &ptDynamicBufferNode->uDynamicBuffer, &szRangeSize2, 0, NULL);

                // check for free index
                uObjectBindGroupIndex = pl_hm_get_free_index(&ptAppData->tObjectBindGroupdHashMap);

                if(uObjectBindGroupIndex == UINT64_MAX) // no free index
                {
                    pl_sb_push(ptAppData->sbtObjectBindGroups, tNewBindGroup);
                    uObjectBindGroupIndex = pl_sb_size(ptAppData->sbtObjectBindGroups) - 1;
                    pl_hm_insert(&ptAppData->tObjectBindGroupdHashMap, (uint64_t)uDynamicBufferIndex, uObjectBindGroupIndex);
                }
                else // resuse free index
                {
                    ptAppData->sbtObjectBindGroups[uObjectBindGroupIndex] = tNewBindGroup;
                    pl_hm_insert(&ptAppData->tObjectBindGroupdHashMap, (uint64_t)uDynamicBufferIndex, uObjectBindGroupIndex);
                }  
            }

            ptAppData->tMesh.uIndexBuffer = ptAppData->uIndexBuffer;
            ptAppData->tMesh.uVertexBuffer = ptAppData->uVertexBuffer;
            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->tAllocation.pHostMapped + ptDynamicBufferNode->uDynamicBufferOffset);
            *ptObjectInfo = ptAppData->tInfo;
            ptAppData->uBindGroup2 = (uint32_t)uObjectBindGroupIndex;

            pl_sb_push(ptDevice->_sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);
            pl_end_profile_sample();
        }

        // bind camera
        {
            const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptAppData->uDynamicBuffer0];
            const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptAppData->uDynamicBuffer0_Offset;

            plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->tAllocation.pHostMapped[uBufferFrameOffset0];
            ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
            ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptAppData->tCamera.tPos, .w = 0.0f};
            ptGlobalInfo->tCameraView     = ptAppData->tCamera.tViewMat;
            ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptAppData->tCamera.tViewMat);
            ptGlobalInfo->fTime           = (float)pl_get_io_context()->dTime;
            ptGlobalInfo->tLightPos = (plVec4){-3.0f, 2.0, 0.0f};
            ptGlobalInfo->tLightColor = (plVec4){1.0f, 1.0f, 1.0f};
        }

        // draw scene
        {
            pl_begin_profile_sample("draw scene");

            // record draw
            plDraw tDraw = {
                .uShaderVariant        = ptAppData->uShaderVariant,
                .ptMesh                = &ptAppData->tMesh,
                .aptBindGroups          = {
                    &ptAppData->tBindGroup1,
                    &ptAppData->sbtObjectBindGroups[ptAppData->uBindGroup2]},
                .auDynamicBufferOffset = {0, 0}
            };

            // record draw area
            const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptAppData->uDynamicBuffer0];
            const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptAppData->uDynamicBuffer0_Offset;

            plDrawArea tArea = {
                .ptBindGroup0          = &ptAppData->tGlobalBindGroup,
                .uDrawOffset           = 0,
                .uDrawCount            = 1,
                .uDynamicBufferOffset0 = uBufferFrameOffset0 
            };

            gptGfx->draw_areas(ptGraphics, 1, &tArea, &tDraw);

            pl_end_profile_sample();
        }

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptAppData->tCamera.tProjMat, &ptAppData->tCamera.tViewMat);
        gptVulkanDraw->submit_3d_drawlist(&ptAppData->drawlist3d, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST);

        ptDrawCtx->tFrameBufferScale.x = ptIoCtx->afMainFramebufferScale[0];
        ptDrawCtx->tFrameBufferScale.y = ptIoCtx->afMainFramebufferScale[1];

        // submit draw lists
        gptVulkanDraw->submit_drawlist(&ptAppData->drawlist, (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);

        // submit ui drawlist
        gptUi->render();

        gptVulkanDraw->submit_drawlist(gptUi->get_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        gptVulkanDraw->submit_drawlist(gptUi->get_debug_draw_list(NULL), (float)ptIoCtx->afMainViewportSize[0], (float)ptIoCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptGraphics->szCurrentFrameIndex);
        vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

        gptGfx->end_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptGfx->end_frame(ptGraphics);
    } 
    pl_end_profile_sample();
    pl_end_profile_frame();
}

void
pl__show_main_window(plAppData* ptAppData)
{
    plGraphics* ptGraphics = &ptAppData->tGraphics;

    gptUi->set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(gptUi->begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(gptUi->collapsing_header("General"))
        {
            if(gptUi->checkbox("VSync", &ptGraphics->tSwapchain.bVSync))
                ptAppData->bVSyncChanged = true;
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("Tools"))
        {
            gptUi->checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            gptUi->checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            gptUi->checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            gptUi->checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            gptUi->checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            gptUi->end_collapsing_header();
        }

        if(gptUi->collapsing_header("User Interface"))
        {
            gptUi->checkbox("UI Debug", &ptAppData->bShowUiDebug);
            gptUi->checkbox("UI Demo", &ptAppData->bShowUiDemo);
            gptUi->checkbox("UI Style", &ptAppData->bShowUiStyle);
            gptUi->end_collapsing_header();
        }

        gptUi->end_window();
    }
}

VkSurfaceKHR
pl__create_surface(VkInstance tInstance, plIOContext* ptIoCtx)
{
    VkSurfaceKHR tSurface = VK_NULL_HANDLE;
    #ifdef _WIN32
        const VkWin32SurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .hinstance = GetModuleHandle(NULL),
            .hwnd = *(HWND*)ptIoCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateWin32SurfaceKHR(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #elif defined(__APPLE__)
        const VkMetalSurfaceCreateInfoEXT tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT,
            .pLayer = (CAMetalLayer*)ptIoCtx->pBackendPlatformData
        };
        PL_VULKAN(vkCreateMetalSurfaceEXT(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #else // linux
        struct tPlatformData { xcb_connection_t* ptConnection; xcb_window_t tWindow;};
        struct tPlatformData* ptPlatformData = (struct tPlatformData*)ptIoCtx->pBackendPlatformData;
        const VkXcbSurfaceCreateInfoKHR tSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR,
            .pNext = NULL,
            .flags = 0,
            .window = ptPlatformData->tWindow,
            .connection = ptPlatformData->ptConnection
        };
        PL_VULKAN(vkCreateXcbSurfaceKHR(tInstance, &tSurfaceCreateInfo, NULL, &tSurface));
    #endif   
    return tSurface; 
}