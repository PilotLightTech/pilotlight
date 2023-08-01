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

// helpers
static inline float frandom(float fMax, float fMin){ return fMin + (float)rand()/(float)(RAND_MAX/fMax);}

void pla_add_floor(plMeshComponent* ptMesh)
{
    uint32_t uInitialIndex = pl_sb_size(ptMesh->sbtVertexPositions);

    for(uint32_t i = 0; i < 4; i++)
        pl_sb_push(ptMesh->sbtVertexColors0, ((plVec4){0.10f, 0.35f, 0.10f, frandom(1.0f, 0.85f)}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -300.0f, 0.0f, -300.0f}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -300.0f, 0.0f,  300.0f}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  300.0f, 0.0f,  300.0f}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  300.0f, 0.0f, -300.0f}));

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 1);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 2);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 2);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 3);
}

void pla_add_pine_tree(plMeshComponent* ptMesh, plVec3 tPos, float fRadius, float fHeight, plVec4 tTrunkColor, plVec4 tLeafColor)
{

    uint32_t uInitialIndex = pl_sb_size(ptMesh->sbtVertexPositions);

    // trunk
    for(uint32_t i = 0; i < 12; i++)
        pl_sb_push(ptMesh->sbtVertexColors0, tTrunkColor);

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x, tPos.y,  tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius + tPos.x, tPos.y, -fRadius + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius + tPos.x, tPos.y,  fRadius + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x,           tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){            tPos.x, fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,            tPos.y, -fRadius + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,           tPos.y, -fRadius + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){           tPos.x, fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ fRadius + tPos.x,           tPos.y,  fRadius + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius + tPos.x,           tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius + tPos.x,           tPos.y,  fRadius + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){            tPos.x, fHeight + tPos.y,            tPos.z}));

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 1);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 2);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 3);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 4);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 5);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 6);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 7);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 8);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 9);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 10);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 11);

    // leaves
    for(uint32_t i = 0; i < 12; i++)
        pl_sb_push(ptMesh->sbtVertexColors0, tLeafColor);

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius * 3.0f + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius * 3.0f + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){                   tPos.x,         fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius* 3.0f + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y, -fRadius* 3.0f + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){                  tPos.x,         fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius* 3.0f + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,            tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fRadius * 3.0f + tPos.x, 0.25f * fHeight + tPos.y,  fRadius* 3.0f + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){                   tPos.x,         fHeight + tPos.y,            tPos.z}));

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 12);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 13);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 14);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 15);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 16);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 17);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 18);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 19);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 20);
    
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 21);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 22);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 23);
}

void pla_add_bush(plMeshComponent* ptMesh, plVec3 tPos, float fHeight, plVec4 tColor)
{

    uint32_t uInitialIndex = pl_sb_size(ptMesh->sbtVertexPositions);

    for(uint32_t i = 0; i < 20; i++)
        pl_sb_push(ptMesh->sbtVertexColors0, tColor);

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x,           tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x,           tPos.y,  fHeight + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, tPos.y, -fHeight + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){  fHeight + tPos.x,           tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x,           tPos.y, -fHeight + tPos.z}));

    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x, fHeight * 2.0f + tPos.y,  fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x,           tPos.y, -fHeight + tPos.z}));
    pl_sb_push(ptMesh->sbtVertexPositions, ((plVec3){ -fHeight + tPos.x,           tPos.y,  fHeight + tPos.z}));

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 1);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 2);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 0);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 2);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 3);

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 4);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 6);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 7);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 4);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 7);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 5);

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 8);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 10);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 11);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 8);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 11);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 9);

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 12);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 14);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 15);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 12);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 15);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 13);

    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 16);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 18);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 19);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 16);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 19);
    pl_sb_push(ptMesh->sbuIndices, uInitialIndex + 17);
}

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

typedef struct _plPickInfo
{
    plVec4 tColor;
} plPickInfo;

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

typedef struct _plRenderTargetDesc
{
    uint32_t     uRenderPass;
    plVec2       tSize;
} plRenderTargetDesc;

typedef struct _plRenderTarget
{
    plRenderTargetDesc tDesc;
    uint32_t*          sbuColorTextureViews;
    uint32_t           uDepthTextureView;
    uint32_t*          sbuFrameBuffers;
    bool               bMSAA;
} plRenderTarget;

typedef struct _plScene
{
    plRenderer*              ptRenderer;
    plRenderTarget*          ptRenderTarget;
    const plCameraComponent* ptCamera;
    bool                     bMaterialsNeedUpdate;
    bool                     bMeshesNeedUpdate;
    
    // global data
    plBindGroup     tGlobalBindGroup;
    uint32_t        uGlobalVertexData;
    float*          sbfGlobalVertexData;
    uint32_t        uGlobalMaterialData;
    plMaterialInfo* sbtGlobalMaterialData;

    plBindGroup     tGlobalPickBindGroup;
    uint32_t        uGlobalPickData;
    plPickInfo*     sbtGlobalPickData;

    uint32_t uGlobalVtxDataOffset;

    // global vertex/index buffers
    plVec3*   sbtVertexData;
    uint32_t* sbuIndexData;
    uint32_t  uIndexBuffer;
    uint32_t  uVertexBuffer;
    
    // skybox
    plBindGroup tSkyboxBindGroup0;
    uint32_t    uSkyboxTextureView;
    plMesh      tSkyboxMesh;
    
    uint32_t            uDynamicBuffer0_Offset;
    uint32_t            uDynamicBuffer0;
    plComponentLibrary* ptComponentLibrary;
} plScene;

typedef struct _plRenderer
{
    plGraphics* ptGraphics;

    plEntity*   sbtVisibleMeshes;
    plEntity*   sbtVisibleOutlinedMeshes;

    // material bind groups
    plBindGroup* sbtMaterialBindGroups;
    plHashMap    tMaterialBindGroupdHashMap;

    // object bind groups
    plBindGroup* sbtObjectBindGroups;
    plHashMap    tObjectBindGroupdHashMap;

    // draw stream
    plDraw*     sbtDraws;
    plDrawArea* sbtDrawAreas;

    // shaders
    uint32_t uMainShader;
    uint32_t uOutlineShader;
    uint32_t uSkyboxShader;
    uint32_t uPickShader;

    // picking
    uint32_t         uPickPass;
    plRenderTarget   tPickTarget;
    VkDescriptorSet* sbtTextures;
    uint32_t         uPickMaterial;

} plRenderer;

typedef struct _plAppData
{

    // vulkan
    plRenderBackend tBackend;
    plGraphics      tGraphics;
    plDrawList      drawlist;
    plDrawList      drawlist2;
    plDrawList3D    drawlist3d;
    plDrawLayer*    fgDrawLayer;
    plDrawLayer*    bgDrawLayer;
    plDrawLayer*    offscreenDrawLayer;
    plFontAtlas     fontAtlas;
    bool            bShowUiDemo;
    bool            bShowUiDebug;
    bool            bShowUiStyle;
    bool            bShowEcs;

    // allocators
    plTempAllocator tTempAllocator;
    
    // renderer
    plRenderer         tRenderer;
    plScene            tScene;
    plComponentLibrary tComponentLibrary;

    // cameras
    plEntity tCameraEntity;
    plEntity tOffscreenCameraEntity;

    // new stuff
    uint32_t         uOffscreenPass;
    plRenderTarget   tMainTarget;
    plRenderTarget   tOffscreenTarget;
    VkDescriptorSet* sbtTextures;

    // lights
    plEntity tLightEntity;

    plDebugApiInfo     tDebugInfo;
    int                iSelectedEntity;
    bool               bVSyncChanged;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plLibraryApiI*           gptLibrary           = NULL;
const plFileApiI*              gptFile              = NULL;
const plDeviceApiI*            gptDevice            = NULL;
const plRenderBackendI*        gptBackend           = NULL;
const plImageApiI*             gptImage             = NULL;
const plGraphicsApiI*          gptGfx               = NULL;
const plDrawApiI*              gptDraw              = NULL;
const plVulkanDrawApiI*        gptVulkanDraw        = NULL;
const plUiApiI*                gptUi                = NULL;
const plEcsI*                  gptEcs               = NULL;
const plCameraI*               gptCamera            = NULL;
const plStatsApiI*             gptStats             = NULL;
const plDebugApiI*             gptDebug             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper functions
//-----------------------------------------------------------------------------

void         pl__show_main_window(plAppData* ptAppData);
void         pl__show_ecs_window (plAppData* ptAppData);
void         pl__select_entity   (plAppData* ptAppData);
VkSurfaceKHR pl__create_surface  (VkInstance tInstance, plIOContext* ptIoCtx);

void pl_begin_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget);
void pl_draw_scene        (plScene* ptScene);
void pl_draw_pick_scene   (plScene* ptScene);

// graphics
void pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut);
void pl_create_render_target2(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut, const char* pcName);
void pl_setup_renderer  (const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptComponentLibrary, plGraphics* ptGraphics, plRenderer* ptRenderer);
void pl_resize_renderer (plRenderer* ptRenderer, float fWidth, float fHeight);
void pl_draw_sky        (plScene* ptScene);

// scene
void pl_create_scene      (plRenderer* ptRenderer, plComponentLibrary* ptComponentLibrary, plScene* ptSceneOut);
void pl_scene_bind_camera (plScene* ptScene, const plCameraComponent* ptCamera);

// entity component system
void pl_prepare_gpu_data(plScene* ptScene);
void pl__prepare_material_gpu_data(plScene* ptScene, plComponentManager* ptManager);
void pl__prepare_object_gpu_data(plScene* ptScene, plComponentManager* ptManager);

// helpers
static inline bool
pl__get_free_resource_index(uint32_t* sbuFreeIndices, uint32_t* puIndexOut)
{
    // check if previous index is availble
    if(pl_sb_size(sbuFreeIndices) > 0)
    {
        const uint32_t uFreeIndex = pl_sb_pop(sbuFreeIndices);
        *puIndexOut = uFreeIndex;
        return true;
    }
    return false;    
}

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
        gptLibrary    = ptApiRegistry->first(PL_API_LIBRARY);
        gptFile       = ptApiRegistry->first(PL_API_FILE);
        gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
        gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
        gptImage      = ptApiRegistry->first(PL_API_IMAGE);
        gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDraw       = ptApiRegistry->first(PL_API_DRAW);
        gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
        gptUi         = ptApiRegistry->first(PL_API_UI);
        gptEcs        = ptApiRegistry->first(PL_API_ECS);
        gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
        gptStats      = ptApiRegistry->first(PL_API_STATS);
        gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

        return ptAppData;
    }

    // allocate intial app memory
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // add contexts to data registry (so extensions can use them)
    gptDataRegistry->set_data("profile", pl_create_profile_context());
    gptDataRegistry->set_data("log", pl_create_log_context());
    ptAppData->iSelectedEntity = 1;

    // load extensions
    gptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    gptExtensionRegistry->load_from_config(ptApiRegistry, "../apps/pl_config.json");

    // load global apis
    gptLibrary    = ptApiRegistry->first(PL_API_LIBRARY);
    gptFile       = ptApiRegistry->first(PL_API_FILE);
    gptDevice     = ptApiRegistry->first(PL_API_DEVICE);
    gptBackend    = ptApiRegistry->first(PL_API_BACKEND_VULKAN);
    gptImage      = ptApiRegistry->first(PL_API_IMAGE);
    gptGfx        = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDraw       = ptApiRegistry->first(PL_API_DRAW);
    gptVulkanDraw = ptApiRegistry->first(PL_API_VULKAN_DRAW);
    gptUi         = ptApiRegistry->first(PL_API_UI);
    gptEcs        = ptApiRegistry->first(PL_API_ECS);
    gptCamera     = ptApiRegistry->first(PL_API_CAMERA);
    gptStats      = ptApiRegistry->first(PL_API_STATS);
    gptDebug      = ptApiRegistry->first(PL_API_DEBUG);

    // for convience
    plGraphics*         ptGraphics         = &ptAppData->tGraphics;
    plRenderBackend*    ptBackend          = &ptAppData->tBackend;
    plDevice*           ptDevice           = &ptGraphics->tDevice;
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plScene*            ptScene            = &ptAppData->tScene;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    
    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();

    // add some context to data registry (for reloads)
    gptDataRegistry->set_data("ui", gptUi->create_context());
    gptDataRegistry->set_data(PL_CONTEXT_DRAW_NAME, gptDraw->get_context());
    gptDataRegistry->set_data("device", ptDevice);

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
    gptDraw->register_drawlist(ptDrawCtx, &ptAppData->drawlist2);
    gptDraw->register_3d_drawlist(ptDrawCtx, &ptAppData->drawlist3d);
    ptAppData->bgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = gptDraw->request_layer(&ptAppData->drawlist, "Foreground Layer");
    ptAppData->offscreenDrawLayer = gptDraw->request_layer(&ptAppData->drawlist2, "Foreground Layer");

    // create font atlas
    gptDraw->add_default_font(&ptAppData->fontAtlas);
    gptDraw->build_font_atlas(ptDrawCtx, &ptAppData->fontAtlas);
    gptUi->set_default_font(&ptAppData->fontAtlas.sbFonts[0]);
    
    // renderer
    gptEcs->init_component_library(ptApiRegistry, ptComponentLibrary);
    pl_setup_renderer(ptApiRegistry, ptComponentLibrary, ptGraphics, &ptAppData->tRenderer);
    pl_create_scene(&ptAppData->tRenderer, ptComponentLibrary, &ptAppData->tScene);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~entity IDs~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    
    // cameras
    ptAppData->tOffscreenCameraEntity = gptEcs->create_camera(ptComponentLibrary, "offscreen camera", (plVec3){0.0f, 0.35f, 1.2f}, PL_PI_3, 1280.0f / 720.0f, 0.1f, 10.0f);
    ptAppData->tCameraEntity = gptEcs->create_camera(ptComponentLibrary, "main camera", (plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1], 0.01f, 400.0f);
    plCameraComponent* ptCamera = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptCamera2 = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tOffscreenCameraEntity);
    gptCamera->set_pitch_yaw(ptCamera, -0.244f, 1.488f);
    gptCamera->set_pitch_yaw(ptCamera2, 0.0f, -PL_PI);

    // create trees
    const uint32_t uGrassRows = 10;
    const uint32_t uGrassColumns = 10;
    const float fGrassSpacing = 5.0f;
    const plVec3 tGrassCenterPoint = {(float)uGrassColumns * fGrassSpacing / 2.0f};

    for(uint32_t i = 0; i < uGrassRows; i++)
    {
        for(uint32_t j = 0; j < uGrassColumns; j++)
        {

            plEntity tObject = gptEcs->create_object(ptComponentLibrary, "tree");
            plObjectComponent* ptNewObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
            pl_sb_push(ptRenderer->sbtVisibleMeshes, ptNewObjectComponent->tMesh);

            plObjectComponent* ptObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
            plTransformComponent* ptTransformComponent = gptEcs->get_component(&ptComponentLibrary->tTransformComponentManager, tObject);
            ptTransformComponent->tWorld       = pl_identity_mat4();
            ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
            ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
            ptTransformComponent->tTranslation = (plVec3){0};

            ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;

            plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptComponentLibrary->tMeshComponentManager, tObject);

            ptMeshComponent->tMaterial = gptEcs->create_material(ptComponentLibrary, "tree material");
            plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);
            ptMaterialComponent->bDoubleSided = false;

            pla_add_pine_tree(ptMeshComponent, 
            (plVec3){
                tGrassCenterPoint.x + (float)j * -fGrassSpacing + frandom(fGrassSpacing * 8.0f, -fGrassSpacing * 4.0f) - fGrassSpacing * 4.0f, 
                0.0f,
                tGrassCenterPoint.z + i * fGrassSpacing + frandom(fGrassSpacing * 8.0f, -fGrassSpacing * 4.0f) - fGrassSpacing * 4.0f
                }, frandom(0.3f, 0.05f), frandom(15.0f, 2.0f), 
                (plVec4){0.57f, 0.35f, 0.24f, 1.0f}, 
                (plVec4){frandom(0.10f, 0.05f), 0.35f, frandom(0.10f, 0.05f), frandom(1.0f, 0.85f)});
        }
    }

    for(uint32_t i = 0; i < uGrassRows; i++)
    {
        for(uint32_t j = 0; j < uGrassColumns; j++)
        {
            plEntity tObject = gptEcs->create_object(ptComponentLibrary, "bush");
            plObjectComponent* ptNewObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
            pl_sb_push(ptRenderer->sbtVisibleMeshes, ptNewObjectComponent->tMesh);

            plObjectComponent* ptObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
            plTransformComponent* ptTransformComponent = gptEcs->get_component(&ptComponentLibrary->tTransformComponentManager, tObject);
            ptTransformComponent->tWorld       = pl_identity_mat4();
            ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
            ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
            ptTransformComponent->tTranslation = (plVec3){0};

            ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;

            plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptComponentLibrary->tMeshComponentManager, tObject);

            ptMeshComponent->tMaterial = gptEcs->create_material(ptComponentLibrary, "bush material");
            plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);
            ptMaterialComponent->bDoubleSided = false;

            pla_add_bush(ptMeshComponent, 
            (plVec3){
                tGrassCenterPoint.x + (float)j * -fGrassSpacing + frandom(fGrassSpacing * 8.0f, -fGrassSpacing * 4.0f) - fGrassSpacing * 4.0f, 
                0.0f,
                tGrassCenterPoint.z + i * fGrassSpacing + frandom(fGrassSpacing * 8.0f, -fGrassSpacing * 4.0f) - fGrassSpacing * 4.0f
                }, frandom(0.5f, 0.05f), (plVec4){frandom(0.25f, 0.05f), frandom(1.0f, 0.05f), frandom(0.25f, 0.05f), 1.0f});


        }
    }

    {
        plEntity tObject = gptEcs->create_object(ptComponentLibrary, "floor");
        plObjectComponent* ptNewObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
        pl_sb_push(ptRenderer->sbtVisibleMeshes, ptNewObjectComponent->tMesh);

        plObjectComponent* ptObjectComponent = gptEcs->get_component(&ptComponentLibrary->tObjectComponentManager, tObject);
        plTransformComponent* ptTransformComponent = gptEcs->get_component(&ptComponentLibrary->tTransformComponentManager, tObject);
        ptTransformComponent->tWorld       = pl_identity_mat4();
        ptTransformComponent->tRotation    = (plVec4){.w = 1.0f};
        ptTransformComponent->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
        ptTransformComponent->tTranslation = (plVec3){0};

        ptTransformComponent->tFinalTransform = ptTransformComponent->tWorld;

        plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptComponentLibrary->tMeshComponentManager, tObject);

        ptMeshComponent->tMaterial = gptEcs->create_material(ptComponentLibrary, "floor material");
        plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);
        ptMaterialComponent->bDoubleSided = false;
        pla_add_floor(ptMeshComponent);
    }

    plMeshComponent* sbtMeshes = (plMeshComponent*)ptComponentLibrary->tMeshComponentManager.pComponents;
    gptEcs->calculate_normals(sbtMeshes, pl_sb_size(sbtMeshes));
    gptEcs->calculate_tangents(sbtMeshes, pl_sb_size(sbtMeshes));

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~materials~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = gptDevice->find_depth_stencil_format(ptDevice)
    };
    ptAppData->uOffscreenPass = gptDevice->create_render_pass(ptDevice, &tRenderPassDesc, "offscreen renderpass");

    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptAppData->uOffscreenPass,
        .tSize = {1280.0f, 720.0f},
    };
    pl_create_render_target(ptGraphics, &tRenderTargetDesc, &ptAppData->tOffscreenTarget);

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        plTextureView* ptColorTextureView = &ptDevice->sbtTextureViews[ptAppData->tOffscreenTarget.sbuColorTextureViews[i]];
        pl_sb_push(ptAppData->sbtTextures, gptVulkanDraw->add_texture(ptDrawCtx, ptColorTextureView->_tImageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    // create main render target
    {
        ptAppData->tMainTarget.bMSAA = true;
        ptAppData->tMainTarget.sbuFrameBuffers = ptGraphics->tSwapchain.puFrameBuffers;
        ptAppData->tMainTarget.tDesc.uRenderPass = ptGraphics->uRenderPass;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
        ptAppData->tMainTarget.tDesc.tSize.x = (float)ptGraphics->tSwapchain.tExtent.width;
        ptAppData->tMainTarget.tDesc.tSize.y = (float)ptGraphics->tSwapchain.tExtent.height;
    }

    // lights
    ptAppData->tLightEntity = gptEcs->create_light(ptComponentLibrary, "light", (plVec3){1.0f, 1.0f, 1.0f}, (plVec3){1.0f, 1.0f, 1.0f});

    // prepare scene
    const uint32_t uMeshCount = pl_sb_size(sbtMeshes);
    for(uint32_t uMeshIndex = 0; uMeshIndex < uMeshCount; uMeshIndex++)
    {
        plMeshComponent* ptMesh = &sbtMeshes[uMeshIndex];

        plMaterialComponent* ptMaterial = gptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMesh->tMaterial);

        // update material data
        const plMaterialInfo tMaterialInfo = {
            .tAlbedo = ptMaterial->tAlbedo
        };
        pl_sb_push(ptScene->sbtGlobalMaterialData, tMaterialInfo);
        ptMesh->tInfo.uMaterialIndex = pl_sb_size(ptScene->sbtGlobalMaterialData) - 1;
        
        // update pick data
        const plPickInfo tPickInfo = {
            .tColor = gptEcs->entity_to_color(ptScene->ptComponentLibrary->tMeshComponentManager.sbtEntities[uMeshIndex])
        };
        pl_sb_push(ptScene->sbtGlobalPickData, tPickInfo);
        
        // add mesh to global buffers
        
        // add primary vertex data
        {
            // current location in global buffers
            ptMesh->tMesh.uIndexOffset = pl_sb_size(ptScene->sbuIndexData);
            ptMesh->tMesh.uIndexCount = pl_sb_size(ptMesh->sbuIndices);
            ptMesh->tMesh.uVertexOffset = pl_sb_size(ptScene->sbtVertexData);
            ptMesh->tMesh.uVertexCount = pl_sb_size(ptMesh->sbtVertexPositions);
            ptMesh->tInfo.uVertexPosOffset = ptMesh->tMesh.uVertexOffset;
            
            // copy data to global buffer
            pl_sb_add_n(ptScene->sbuIndexData, ptMesh->tMesh.uIndexCount);
            pl_sb_add_n(ptScene->sbtVertexData, ptMesh->tMesh.uVertexCount);
            memcpy(&ptScene->sbuIndexData[ptMesh->tMesh.uIndexOffset], ptMesh->sbuIndices, sizeof(uint32_t) * ptMesh->tMesh.uIndexCount);
            memcpy(&ptScene->sbtVertexData[ptMesh->tMesh.uVertexOffset], ptMesh->sbtVertexPositions, sizeof(plVec3) * ptMesh->tMesh.uVertexCount); 
        }

        // add secondary vertex data
        {
            // update global vertex buffer offset
            ptMesh->tInfo.uVertexDataOffset = ptScene->uGlobalVtxDataOffset / 4;

            // stride within storage buffer
            uint32_t uStride = 0;

            // calculate vertex stream mask based on provided data
            if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
            if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
            if(pl_sb_size(ptMesh->sbtVertexColors0) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
            if(pl_sb_size(ptMesh->sbtVertexColors1) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
            if(pl_sb_size(ptMesh->sbtVertexWeights0) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
            if(pl_sb_size(ptMesh->sbtVertexWeights1) > 0)            { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
            if(pl_sb_size(ptMesh->sbtVertexJoints0) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
            if(pl_sb_size(ptMesh->sbtVertexJoints1) > 0)             { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }
            if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates0) > 0) { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
            if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates1) > 0) { uStride += 4; ptMesh->tMesh.ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }

            pl_sb_add_n(ptScene->sbfGlobalVertexData, uStride * ptMesh->tMesh.uVertexCount);

            // current attribute offset
            uint32_t uOffset = 0;

            // normals
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexNormals); i++)
            {
                ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
                const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 0] = ptNormal->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 1] = ptNormal->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 2] = ptNormal->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + 3] = 0.0f;
            }

            if(ptMesh->tMesh.ulVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL)
                uOffset += 4;

            // tangents
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexTangents); i++)
            {
                const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTangent->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTangent->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptTangent->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptTangent->w;
            }

            if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)
                uOffset += 4;

            // texture coordinates 0
            if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates0) > 0)
            {
                for(uint32_t i = 0; i < ptMesh->tMesh.uVertexCount; i++)
                {
                    const plVec2* ptTextureCoordinates = &ptMesh->sbtVertexTextureCoordinates0[i];
                    ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
                    ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
                    ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = 0.0f;
                    ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = 0.0f;

                }
                uOffset += 4;
            }

            // texture coordinates 1
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexTextureCoordinates1); i++)
            {
                const plVec2* ptTextureCoordinates = &ptMesh->sbtVertexTextureCoordinates1[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptTextureCoordinates->u;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptTextureCoordinates->v;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = 0.0f;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = 0.0f;
            }

            if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates1) > 0)
                uOffset += 4;

            // color 0
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexColors0); i++)
            {
                const plVec4* ptColor = &ptMesh->sbtVertexColors0[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptColor->r;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptColor->g;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptColor->b;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptColor->a;
            }

            if(pl_sb_size(ptMesh->sbtVertexColors0) > 0)
                uOffset += 4;

            // color 1
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexColors1); i++)
            {
                const plVec4* ptColor = &ptMesh->sbtVertexColors1[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptColor->r;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptColor->g;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptColor->b;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptColor->a;
            }

            if(pl_sb_size(ptMesh->sbtVertexColors1) > 0)
                uOffset += 4;

            // joints 0
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexJoints0); i++)
            {
                const plVec4* ptJoint = &ptMesh->sbtVertexJoints0[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptJoint->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptJoint->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptJoint->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptJoint->w;
            }

            if(pl_sb_size(ptMesh->sbtVertexJoints0) > 0)
                uOffset += 4;

            // joints 1
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexJoints1); i++)
            {
                const plVec4* ptJoint = &ptMesh->sbtVertexJoints1[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptJoint->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptJoint->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptJoint->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptJoint->w;
            }

            if(pl_sb_size(ptMesh->sbtVertexJoints1) > 0)
                uOffset += 4;

            // weights 0
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexWeights0); i++)
            {
                const plVec4* ptWeight = &ptMesh->sbtVertexWeights0[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptWeight->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptWeight->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptWeight->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptWeight->w;
            }

            if(pl_sb_size(ptMesh->sbtVertexWeights0) > 0)
                uOffset += 4;

            // weights 1
            for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexWeights1); i++)
            {
                const plVec4* ptWeight = &ptMesh->sbtVertexWeights1[i];
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 0] = ptWeight->x;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 1] = ptWeight->y;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 2] = ptWeight->z;
                ptScene->sbfGlobalVertexData[ptScene->uGlobalVtxDataOffset + i * uStride + uOffset + 3] = ptWeight->w;
            }

            if(pl_sb_size(ptMesh->sbtVertexWeights1) > 0)
                uOffset += 4;

            PL_ASSERT(uOffset == uStride && "sanity check");

            ptScene->uGlobalVtxDataOffset += uStride * ptMesh->tMesh.uVertexCount;

            // update material vertex stream to match actual mesh
            plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMesh->tMaterial);
            ptMaterialComponent->tGraphicsState.ulVertexStreamMask = ptMesh->tMesh.ulVertexStreamMask;
        }
    }

    // update cpu buffer and upload to cpu buffers

    // submit old global buffers for deletion
    if(ptScene->uGlobalVertexData   != UINT32_MAX) gptDevice->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalVertexData);
    if(ptScene->uGlobalMaterialData != UINT32_MAX) gptDevice->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalMaterialData);
    if(ptScene->uGlobalPickData     != UINT32_MAX) gptDevice->submit_buffer_for_deletion(ptDevice, ptScene->uGlobalPickData);

    // create new storage global buffers
    ptScene->uGlobalVertexData   = gptDevice->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbfGlobalVertexData) * sizeof(float), ptScene->sbfGlobalVertexData, "global vertex data");
    ptScene->uGlobalMaterialData = gptDevice->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbtGlobalMaterialData) * sizeof(plMaterialInfo), ptScene->sbtGlobalMaterialData, "global material data");
    ptScene->uGlobalPickData     = gptDevice->create_storage_buffer(ptDevice, pl_sb_size(ptScene->sbtGlobalPickData) * sizeof(plPickInfo), ptScene->sbtGlobalPickData, "global pick data");

    // update global bind group
    uint32_t atBuffers0[] = {ptScene->uDynamicBuffer0, ptScene->uGlobalVertexData, ptScene->uGlobalMaterialData};
    size_t aszRangeSizes[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE, VK_WHOLE_SIZE};
    gptGfx->update_bind_group(ptGraphics, &ptScene->tGlobalBindGroup, 3, atBuffers0, aszRangeSizes, 0, NULL);

    // update global picking bind group
    uint32_t atBuffers1[] = {ptScene->uDynamicBuffer0, ptScene->uGlobalPickData};
    size_t aszRangeSizes1[] = {sizeof(plGlobalInfo), VK_WHOLE_SIZE};
    gptGfx->update_bind_group(ptGraphics, &ptScene->tGlobalPickBindGroup, 2, atBuffers1, aszRangeSizes1, 0, NULL);

    // create new global index buffer
    ptScene->uIndexBuffer = gptDevice->create_index_buffer(ptDevice, 
        sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexData),
        ptScene->sbuIndexData, "global index buffer");

    // create new global vertex buffer
    ptScene->uVertexBuffer = gptDevice->create_vertex_buffer(ptDevice, 
        sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexData), sizeof(plVec3),
        ptScene->sbtVertexData, "global vertex buffer");

    // reset global cpu side buffers
    pl_sb_reset(ptScene->sbfGlobalVertexData);
    pl_sb_reset(ptScene->sbtGlobalMaterialData);
    pl_sb_reset(ptScene->sbtGlobalPickData);
    pl_sb_reset(ptScene->sbuIndexData);
    pl_sb_reset(ptScene->sbtVertexData);

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
    plRenderer*      ptRenderer = &ptAppData->tRenderer;

    vkDeviceWaitIdle(ptGraphics->tDevice.tLogicalDevice);

    gptDraw->cleanup_font_atlas(&ptAppData->fontAtlas);
    gptDraw->cleanup_context();
    gptUi->destroy_context(NULL);
    
    // cleanup offscreen target
    for (uint32_t i = 0u; i < pl_sb_size(ptAppData->tOffscreenTarget.sbuFrameBuffers); i++)
        gptDevice->submit_frame_buffer_for_deletion(ptDevice, ptAppData->tOffscreenTarget.sbuFrameBuffers[i]);
    pl_sb_free(ptAppData->tOffscreenTarget.sbuFrameBuffers);
    pl_sb_free(ptAppData->tOffscreenTarget.sbuColorTextureViews);

    // cleanup pick target
    for (uint32_t i = 0u; i < pl_sb_size(ptAppData->tRenderer.tPickTarget.sbuFrameBuffers); i++)
        gptDevice->submit_frame_buffer_for_deletion(ptDevice, ptAppData->tRenderer.tPickTarget.sbuFrameBuffers[i]);
    pl_sb_free(ptAppData->tRenderer.tPickTarget.sbuFrameBuffers);
    pl_sb_free(ptAppData->tRenderer.tPickTarget.sbuColorTextureViews);

    // clean up scene
    pl_sb_free(ptAppData->tScene.sbfGlobalVertexData);
    pl_sb_free(ptAppData->tScene.sbtGlobalMaterialData);
    pl_sb_free(ptAppData->tScene.sbtGlobalPickData);
    pl_sb_free(ptAppData->tScene.sbtVertexData);
    pl_sb_free(ptAppData->tScene.sbuIndexData);

    // clean up renderer
    pl_sb_free(ptRenderer->sbtVisibleMeshes);
    pl_sb_free(ptRenderer->sbtVisibleOutlinedMeshes);
    pl_sb_free(ptRenderer->sbtMaterialBindGroups);
    pl_sb_free(ptRenderer->sbtObjectBindGroups);
    pl_sb_free(ptRenderer->sbtDraws);
    pl_sb_free(ptRenderer->sbtDrawAreas);
    pl_sb_free(ptRenderer->sbtTextures);
    gptEcs->cleanup_systems(gptApiRegistry, &ptAppData->tComponentLibrary);
    gptGfx->cleanup(&ptAppData->tGraphics);
    gptBackend->cleanup_swapchain(ptBackend, ptDevice, &ptGraphics->tSwapchain);
    gptBackend->cleanup_device(&ptGraphics->tDevice);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    pl_temp_allocator_free(&ptAppData->tTempAllocator);
    pl_sb_free(ptAppData->sbtTextures);
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
    plRenderer*         ptRenderer         = &ptAppData->tRenderer;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;
    plDevice*           ptDevice           = &ptGraphics->tDevice;

    // contexts
    plIOContext* ptIoCtx = pl_get_io_context();

    gptGfx->resize(ptGraphics);
    {
        for(uint32_t i = 0; i < pl_sb_size(ptRenderer->tPickTarget.sbuColorTextureViews); i++)
        {
            uint32_t uTextureView = ptRenderer->tPickTarget.sbuColorTextureViews[i];
            uint32_t uTexture = ptDevice->sbtTextureViews[uTextureView].uTextureHandle;
            gptDevice->submit_texture_for_deletion(ptDevice, uTexture);
            gptDevice->submit_texture_view_for_deletion(ptDevice, uTextureView);
        }

        // cleanup pick target
        for (uint32_t i = 0u; i < pl_sb_size(ptAppData->tRenderer.tPickTarget.sbuFrameBuffers); i++)
            gptDevice->submit_frame_buffer_for_deletion(ptDevice, ptAppData->tRenderer.tPickTarget.sbuFrameBuffers[i]);
        pl_sb_free(ptAppData->tRenderer.tPickTarget.sbuFrameBuffers);
        pl_sb_free(ptAppData->tRenderer.tPickTarget.sbuColorTextureViews);

        pl_sb_reset(ptRenderer->tPickTarget.sbuColorTextureViews);
        pl_sb_reset(ptRenderer->tPickTarget.sbuFrameBuffers);

        uint32_t uDepthTextureView = ptRenderer->tPickTarget.uDepthTextureView;
        uint32_t uDepthTexture = ptDevice->sbtTextureViews[uDepthTextureView].uTextureHandle;
        gptDevice->submit_texture_for_deletion(ptDevice, uDepthTexture);
        gptDevice->submit_texture_view_for_deletion(ptDevice, uDepthTextureView);
    }
    pl_resize_renderer(ptRenderer, ptIoCtx->afMainViewportSize[0], ptIoCtx->afMainViewportSize[1]);

    plCameraComponent* ptCamera = gptEcs->get_component(&ptComponentLibrary->tCameraComponentManager, ptAppData->tCameraEntity);
    gptCamera->set_aspect(ptCamera, ptIoCtx->afMainViewportSize[0] / ptIoCtx->afMainViewportSize[1]);

    // create main render target
    {
        ptAppData->tMainTarget.bMSAA = true;
        ptAppData->tMainTarget.sbuFrameBuffers = ptGraphics->tSwapchain.puFrameBuffers;
        ptAppData->tMainTarget.tDesc.uRenderPass = ptGraphics->uRenderPass;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
        ptAppData->tMainTarget.tDesc.tSize.x = (float)ptGraphics->tSwapchain.tExtent.width;
        ptAppData->tMainTarget.tDesc.tSize.y = (float)ptGraphics->tSwapchain.tExtent.height;
    }
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
    plRenderer* ptRenderer = &ptAppData->tRenderer;
    plScene*    ptScene    = &ptAppData->tScene;

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

        // create main render target
        ptAppData->tMainTarget.bMSAA = true;
        ptAppData->tMainTarget.sbuFrameBuffers = ptGraphics->tSwapchain.puFrameBuffers;
        ptAppData->tMainTarget.tDesc.uRenderPass = ptGraphics->uRenderPass;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tColorFormat = ptGraphics->tSwapchain.tFormat;
        ptDevice->sbtRenderPasses[ptAppData->tMainTarget.tDesc.uRenderPass].tDesc.tDepthFormat = ptGraphics->tSwapchain.tDepthFormat;
        ptAppData->tMainTarget.tDesc.tSize.x = (float)ptGraphics->tSwapchain.tExtent.width;
        ptAppData->tMainTarget.tDesc.tSize.y = (float)ptGraphics->tSwapchain.tExtent.height;

        ptAppData->bVSyncChanged = false;
    }
    
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const float fCameraTravelSpeed = 8.0f;
    plCameraComponent* ptCamera = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tCameraEntity);
    plCameraComponent* ptOffscreenCamera = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->tOffscreenCameraEntity);

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) gptCamera->translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) gptCamera->translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIoCtx->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) gptCamera->translate(ptCamera, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) gptCamera->translate(ptCamera,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) gptCamera->translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) gptCamera->translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIoCtx->fDeltaTime,  0.0f);

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
            gptCamera->rotate(ptCamera,  -tMouseDelta.y * 0.1f * ptIoCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIoCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
        gptCamera->update(ptCamera);
        gptCamera->update(ptOffscreenCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptDraw->add_3d_transform(&ptAppData->drawlist3d, &ptOffscreenCamera->tTransformMat, 0.2f, 0.02f);
        gptDraw->add_3d_frustum(&ptAppData->drawlist3d, 
            &ptOffscreenCamera->tTransformMat, ptOffscreenCamera->fFieldOfView, ptOffscreenCamera->fAspectRatio, 
            ptOffscreenCamera->fNearZ, ptOffscreenCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);

        const plMat4 tTransform0 = pl_identity_mat4();
        gptDraw->add_3d_transform(&ptAppData->drawlist3d, &tTransform0, 10.0f, 0.02f);

        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_RIGHT, false))
            pl__select_entity(ptAppData);

        // ui
        if(gptUi->begin_window("Offscreen", NULL, true))
        {
            gptUi->layout_static(720.0f / 2.0f, 1280.0f / 2.0f, 1);
            gptUi->image(ptAppData->sbtTextures[ptGraphics->tSwapchain.uCurrentImageIndex], (plVec2){1280.0f / 2.0f, 720.0f / 2.0f});
            gptUi->end_window();
        }

        plLightComponent* ptLightComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tLightComponentManager, ptAppData->tLightEntity);
        if(gptUi->begin_window("Light", NULL, false))
        {
            
            gptUi->layout_dynamic(0.0f, 1);
            gptUi->slider_float("X", &ptLightComponent->tPosition.x, -10.0f, 10.0f);
            gptUi->slider_float("Y", &ptLightComponent->tPosition.y, -10.0f, 10.0f);
            gptUi->slider_float("Z", &ptLightComponent->tPosition.z, -10.0f, 10.0f);
            gptUi->slider_float("R", &ptLightComponent->tColor.r, 0.0f, 1.0f);
            gptUi->slider_float("G", &ptLightComponent->tColor.g, 0.0f, 1.0f);
            gptUi->slider_float("B", &ptLightComponent->tColor.b, 0.0f, 1.0f);
            gptUi->end_window();
        }

        gptDraw->add_3d_point(&ptAppData->drawlist3d, ptLightComponent->tPosition, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, 0.1f, 0.01f);

        if(ptAppData->iSelectedEntity > 1)
        {

            static plVec3 tRotation = {0.0f, 0.0f, 0.0f};

            plTransformComponent* ptSelectedTransform = gptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, ptAppData->iSelectedEntity);

            // ui
            if(gptUi->begin_window("Testing", NULL, false))
            {
                gptUi->layout_dynamic(0.0f, 1);
                gptUi->slider_float("Scale", &ptSelectedTransform->tScale.x, 0.001f, 5.0f);
                gptUi->slider_float("X", &ptSelectedTransform->tTranslation.x, -10.0f, 10.0f);
                gptUi->slider_float("Y", &ptSelectedTransform->tTranslation.y, -10.0f, 10.0f);
                gptUi->slider_float("Z", &ptSelectedTransform->tTranslation.z, -10.0f, 10.0f);
                gptUi->slider_float("RX", &tRotation.x, -PL_PI, PL_PI);
                gptUi->slider_float("RY", &tRotation.y, -PL_PI, PL_PI);
                gptUi->slider_float("RZ", &tRotation.z, -PL_PI, PL_PI);
                gptUi->end_window();
            }

            const plMat4 tScale0 = pl_mat4_scale_xyz(ptSelectedTransform->tScale.x, ptSelectedTransform->tScale.x, ptSelectedTransform->tScale.x);
            const plMat4 tTranslation0 = pl_mat4_translate_vec3(ptSelectedTransform->tTranslation);
            const plMat4 tTransform2X = pl_mat4_rotate_xyz(tRotation.x, 1.0f, 0.0f, 0.0f);
            const plMat4 tTransform2Y = pl_mat4_rotate_xyz(tRotation.y, 0.0f, 1.0f, 0.0f);
            const plMat4 tTransform2Z = pl_mat4_rotate_xyz(tRotation.z, 0.0f, 0.0f, 1.0f);
            
            plMat4 tFinalTransform = pl_mul_mat4(&tTransform2Z, &tScale0);
            tFinalTransform = pl_mul_mat4(&tTransform2X, &tFinalTransform);
            tFinalTransform = pl_mul_mat4(&tTransform2Y, &tFinalTransform);
            tFinalTransform = pl_mul_mat4(&tTranslation0, &tFinalTransform);

            ptSelectedTransform->tWorld = tFinalTransform;
            ptSelectedTransform->tFinalTransform = tFinalTransform;
        }

        pl__show_main_window(ptAppData);

        if(ptAppData->bShowEcs)
            pl__show_ecs_window(ptAppData);

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
        ptScene->uDynamicBuffer0_Offset = 0;
        plObjectSystemData* ptObjectSystemData = ptScene->ptComponentLibrary->tObjectComponentManager.pSystemData;
        ptObjectSystemData->bDirty = true;

        gptEcs->run_hierarchy_update_system(&ptAppData->tComponentLibrary); // calculate final transforms
        gptEcs->run_object_update_system(&ptAppData->tComponentLibrary);    // set final tranforms

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~offscreen target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        pl_begin_render_target(ptGraphics, &ptAppData->tOffscreenTarget);
        ptScene->ptRenderTarget = &ptAppData->tOffscreenTarget;
        pl_prepare_gpu_data(ptScene);
        pl_scene_bind_camera(ptScene, ptOffscreenCamera);
        pl_draw_scene(ptScene);
        pl_draw_sky(ptScene);

        gptDraw->submit_layer(ptAppData->offscreenDrawLayer);
        gptVulkanDraw->submit_drawlist_ex(&ptAppData->drawlist2, 1280.0f, 720.0f, ptCurrentFrame->tCmdBuf, 
            (uint32_t)ptGraphics->szCurrentFrameIndex, 
            ptDevice->sbtRenderPasses[ptAppData->uOffscreenPass]._tRenderPass, 
            VK_SAMPLE_COUNT_1_BIT);
        vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~main target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        
        pl_begin_render_target(ptGraphics, &ptAppData->tMainTarget);
        ptScene->ptRenderTarget = &ptAppData->tMainTarget;
        pl_prepare_gpu_data(ptScene);
        pl_scene_bind_camera(ptScene, ptCamera);
        pl_draw_scene(ptScene);
        pl_draw_sky(ptScene);

        // submit 3D draw list
        const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
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

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~pick target~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_begin_render_target(ptGraphics, &ptAppData->tRenderer.tPickTarget);
        ptScene->ptRenderTarget = &ptAppData->tRenderer.tPickTarget;
        pl_scene_bind_camera(ptScene, ptCamera);
        pl_draw_pick_scene(ptScene);
        vkCmdEndRenderPass(ptCurrentFrame->tCmdBuf);

        gptGfx->end_recording(ptGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        gptGfx->end_frame(ptGraphics);
    } 
    pl_end_profile_sample();
    pl_end_profile_frame();
}

void
pl__show_ecs_window(plAppData* ptAppData)
{
    if(gptUi->begin_window("Components", &ptAppData->bShowEcs, false))
    {
        const float pfRatios[] = {1.0f};
        gptUi->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        if(ptAppData->iSelectedEntity > 0)
        {

            if(gptUi->collapsing_header("Tag"))
            {
                plTagComponent* ptTagComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptAppData->iSelectedEntity);
                gptUi->text("Name: %s", ptTagComponent->acName);
                gptUi->end_collapsing_header();
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(gptUi->collapsing_header("Transform"))
                {
                    plTransformComponent* ptTransformComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTransformComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Rotation: %0.3f, %0.3f, %0.3f, %0.3f", ptTransformComponent->tRotation.x, ptTransformComponent->tRotation.y, ptTransformComponent->tRotation.z, ptTransformComponent->tRotation.w);
                    gptUi->text("Scale: %0.3f, %0.3f, %0.3f", ptTransformComponent->tScale.x, ptTransformComponent->tScale.y, ptTransformComponent->tScale.z);
                    gptUi->text("Translation: %0.3f, %0.3f, %0.3f", ptTransformComponent->tTranslation.x, ptTransformComponent->tTranslation.y, ptTransformComponent->tTranslation.z);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tMeshComponentManager, ptAppData->iSelectedEntity))
            {
                
                if(gptUi->collapsing_header("Mesh"))
                {
                    // plMeshComponent* ptMeshComponent = pl_ecs_get_component(ptScene->ptMeshComponentManager, iSelectedEntity);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Material"))
                {
                    plMaterialComponent* ptMaterialComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tMaterialComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Albedo: %0.3f, %0.3f, %0.3f, %0.3f", ptMaterialComponent->tAlbedo.r, ptMaterialComponent->tAlbedo.g, ptMaterialComponent->tAlbedo.b, ptMaterialComponent->tAlbedo.a);
                    gptUi->text("Alpha Cutoff: %0.3f", ptMaterialComponent->fAlphaCutoff);
                    gptUi->text("Double Sided: %s", ptMaterialComponent->bDoubleSided ? "true" : "false");
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Object"))
                {
                    plObjectComponent* ptObjectComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tObjectComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptTransformTag = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tTransform);
                    plTagComponent* ptMeshTag = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptObjectComponent->tMesh);
                    gptUi->text("Mesh: %s", ptMeshTag->acName);
                    gptUi->text("Transform: %s", ptTransformTag->acName);

                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Camera"))
                {
                    plCameraComponent* ptCameraComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tCameraComponentManager, ptAppData->iSelectedEntity);
                    gptUi->text("Pitch: %0.3f", ptCameraComponent->fPitch);
                    gptUi->text("Yaw: %0.3f", ptCameraComponent->fYaw);
                    gptUi->text("Roll: %0.3f", ptCameraComponent->fRoll);
                    gptUi->text("Near Z: %0.3f", ptCameraComponent->fNearZ);
                    gptUi->text("Far Z: %0.3f", ptCameraComponent->fFarZ);
                    gptUi->text("Y Field Of View: %0.3f", ptCameraComponent->fFieldOfView);
                    gptUi->text("Aspect Ratio: %0.3f", ptCameraComponent->fAspectRatio);
                    gptUi->text("Up Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tUpVec.x, ptCameraComponent->_tUpVec.y, ptCameraComponent->_tUpVec.z);
                    gptUi->text("Forward Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tForwardVec.x, ptCameraComponent->_tForwardVec.y, ptCameraComponent->_tForwardVec.z);
                    gptUi->text("Right Vector: %0.3f, %0.3f, %0.3f", ptCameraComponent->_tRightVec.x, ptCameraComponent->_tRightVec.y, ptCameraComponent->_tRightVec.z);
                    gptUi->end_collapsing_header();
                }  
            }

            if(gptEcs->has_entity(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity))
            {
                if(gptUi->collapsing_header("Hierarchy"))
                {
                    plHierarchyComponent* ptHierarchyComponent = gptEcs->get_component(&ptAppData->tComponentLibrary.tHierarchyComponentManager, ptAppData->iSelectedEntity);
                    plTagComponent* ptParent = gptEcs->get_component(&ptAppData->tComponentLibrary.tTagComponentManager, ptHierarchyComponent->tParent);
                    gptUi->text("Parent: %s", ptParent->acName);
                    gptUi->end_collapsing_header();
                }  
            } 
        }
        gptUi->end_window();
    }
}

void
pl__select_entity(plAppData* ptAppData)
{
    plDevice* ptDevice = &ptAppData->tGraphics.tDevice;
    plIOContext* ptIoCtx = pl_get_io_context();
    plGraphics* ptGraphics = &ptAppData->tGraphics;
    plRenderer* ptRenderer = &ptAppData->tRenderer;
    plComponentLibrary* ptComponentLibrary = &ptAppData->tComponentLibrary;

    uint32_t uReadBackBuffer = gptDevice->create_read_back_buffer(ptDevice, (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, "pick readback");
    gptDevice->transfer_image_to_buffer(ptDevice, ptDevice->sbtBuffers[uReadBackBuffer].tBuffer, 
        (size_t)(ptIoCtx->afMainViewportSize[0] * ptIoCtx->afMainViewportSize[1]) * 4, 
        &ptDevice->sbtTextures[ptDevice->sbtTextureViews[ptRenderer->tPickTarget.sbuColorTextureViews[ptGraphics->tSwapchain.uCurrentImageIndex]].uTextureHandle]);

    unsigned char* mapping = (unsigned char*)ptDevice->sbtBuffers[uReadBackBuffer].tAllocation.pHostMapped;

    const plVec2 tMousePos = pl_get_mouse_pos();
    
    uint32_t uRowWidth = (uint32_t)ptIoCtx->afMainViewportSize[0] * 4;
    uint32_t uPos = uRowWidth * (uint32_t)tMousePos.y + (uint32_t)tMousePos.x * 4;

    static uint32_t uPickedID = 0;
    if(ptAppData->iSelectedEntity > 0)
    {
        gptEcs->remove_mesh_outline(ptComponentLibrary, (plEntity)ptAppData->iSelectedEntity);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
    }

    uPickedID = (uint32_t)gptEcs->color_to_entity((plVec4*)&mapping[uPos]);
    ptAppData->iSelectedEntity = (int)uPickedID;
    if(uPickedID > 0)
    {
        gptEcs->add_mesh_outline(ptComponentLibrary, (plEntity)uPickedID);
        pl_sb_reset(ptRenderer->sbtVisibleOutlinedMeshes);
        pl_sb_push(ptRenderer->sbtVisibleOutlinedMeshes, (plEntity)uPickedID);
    }

    gptDevice->submit_buffer_for_deletion(ptDevice, uReadBackBuffer);
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

        if(gptUi->collapsing_header("Entities"))
        {
            gptUi->checkbox("Show Components", &ptAppData->bShowEcs);
            plTagComponent* sbtTagComponents = ptAppData->tComponentLibrary.tTagComponentManager.pComponents;
            for(uint32_t i = 0; i < pl_sb_size(sbtTagComponents); i++)
            {
                plTagComponent* ptTagComponent = &sbtTagComponents[i];
                gptUi->radio_button(ptTagComponent->acName, &ptAppData->iSelectedEntity, i + 1);
            }

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

void
pl_begin_render_target(plGraphics* ptGraphics, plRenderTarget* ptTarget)
{
    const plFrameContext* ptCurrentFrame = gptGfx->get_frame_resources(ptGraphics);
    plDevice* ptDevice = &ptGraphics->tDevice;

    static const VkClearValue atClearValues[2] = 
    {
        {
            .color.float32[0] = 0.0f,
            .color.float32[1] = 0.0f,
            .color.float32[2] = 0.0f,
            .color.float32[3] = 1.0f
        },
        {
            .depthStencil.depth = 1.0f,
            .depthStencil.stencil = 0
        }    
    };

    // set viewport
    const VkViewport tViewport = {
        .x        = 0.0f,
        .y        = 0.0f,
        .width    = ptTarget->tDesc.tSize.x,
        .height   = ptTarget->tDesc.tSize.y,
        .minDepth = 0.0f,
        .maxDepth = 1.0f
    };
    vkCmdSetViewport(ptCurrentFrame->tCmdBuf, 0, 1, &tViewport);

    // set scissor
    const VkRect2D tDynamicScissor = {
        .extent = {
            .width    = (uint32_t)ptTarget->tDesc.tSize.x,
            .height   = (uint32_t)ptTarget->tDesc.tSize.y,
        }
    };
    vkCmdSetScissor(ptCurrentFrame->tCmdBuf, 0, 1, &tDynamicScissor);

    const VkRenderPassBeginInfo tRenderPassBeginInfo = {
        .sType               = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass          = ptDevice->sbtRenderPasses[ptTarget->tDesc.uRenderPass]._tRenderPass,
        .framebuffer         = ptDevice->sbtFrameBuffers[ptTarget->sbuFrameBuffers[ptGraphics->tSwapchain.uCurrentImageIndex]]._tFrameBuffer,
        .renderArea          = {
                                    .extent = {
                                        .width  = (uint32_t)ptTarget->tDesc.tSize.x,
                                        .height = (uint32_t)ptTarget->tDesc.tSize.y,
                                    }
                                },
        .clearValueCount     = 2,
        .pClearValues        = atClearValues
    };
    vkCmdBeginRenderPass(ptCurrentFrame->tCmdBuf, &tRenderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
}

void
pl_draw_scene(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleMeshes[i]);
        plMaterialComponent* ptMaterial = gptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);

        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptMaterial->uShaderVariant,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups          = {
                &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2]},
            .auDynamicBufferOffset = {0, ptMeshComponent->uBufferOffset}
            }));
    }

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleOutlinedMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleOutlinedMeshes[i]);
        plMaterialComponent* ptMaterial = gptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tMaterial);
        plMaterialComponent* ptOutlineMaterial = gptEcs->get_component(&ptScene->ptComponentLibrary->tMaterialComponentManager, ptMeshComponent->tOutlineMaterial);

        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptOutlineMaterial->uShaderVariant,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups         = { 
                    &ptRenderer->sbtMaterialBindGroups[ptMaterial->uBindGroup1],
                    &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2] },
            .auDynamicBufferOffset = { 0, ptMeshComponent->uBufferOffset }
            }));
    }

    // record draw area
    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    gptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}

void
pl_draw_pick_scene(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;

    const uint32_t uDrawOffset = pl_sb_size(ptRenderer->sbtDraws);

    // record draws
    for(uint32_t i = 0; i < pl_sb_size(ptRenderer->sbtVisibleMeshes); i++)
    {
        plMeshComponent* ptMeshComponent = gptEcs->get_component(&ptScene->ptComponentLibrary->tMeshComponentManager, ptRenderer->sbtVisibleMeshes[i]);
        pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
            .uShaderVariant        = ptRenderer->uPickMaterial,
            .ptMesh                = &ptMeshComponent->tMesh,
            .aptBindGroups         = { &ptRenderer->sbtObjectBindGroups[ptMeshComponent->uBindGroup2], NULL },
            .auDynamicBufferOffset = { ptMeshComponent->uBufferOffset, 0}
            }));
    }

    // record draw area
    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tGlobalPickBindGroup,
        .uDrawOffset           = uDrawOffset,
        .uDrawCount            = pl_sb_size(ptRenderer->sbtDraws),
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    gptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}


void
pl_create_render_target2(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut, const char* pcName)
{
    ptTargetOut->tDesc = *ptDesc;
    plDevice* ptDevice = &ptGraphics->tDevice;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tDepthFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plSampler tColorSampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tColorView = {
        .tFormat     = tColorTextureDesc.tFormat,
        .uLayerCount = tColorTextureDesc.uLayers,
        .uMips       = tColorTextureDesc.uMips
    };

    pl_sb_reset(ptTargetOut->sbuColorTextureViews);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t uColorTexture = gptDevice->create_texture(ptDevice, tColorTextureDesc, 0, NULL, "pick color texture");
        pl_sb_push(ptTargetOut->sbuColorTextureViews, gptDevice->create_texture_view(ptDevice, &tColorView, &tColorSampler, uColorTexture, "offscreen color view"));
    }

    uint32_t uDepthTexture = gptDevice->create_texture(ptDevice, tDepthTextureDesc, 0, NULL, "pick depth texture");

    const plTextureViewDesc tDepthView = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uLayerCount = tDepthTextureDesc.uLayers,
        .uMips       = tDepthTextureDesc.uMips
    };

    ptTargetOut->uDepthTextureView = gptDevice->create_texture_view(ptDevice, &tDepthView, &tColorSampler, uDepthTexture, "offscreen depth view");

    plTextureView* ptDepthTextureView = &ptDevice->sbtTextureViews[ptTargetOut->uDepthTextureView];

    pl_sb_reset(ptTargetOut->sbuFrameBuffers);
    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {

        uint32_t auAttachments[] = {
            ptTargetOut->sbuColorTextureViews[i],
            ptTargetOut->uDepthTextureView
        };

        plFrameBufferDesc tFBDesc = {
            .uAttachmentCount = 2,
            .puAttachments    = auAttachments,
            .uRenderPass      = ptDesc->uRenderPass,
            .uWidth           = (uint32_t)ptDesc->tSize.x,
            .uHeight          = (uint32_t)ptDesc->tSize.y,
        };
        pl_sb_push(ptTargetOut->sbuFrameBuffers, gptDevice->create_frame_buffer(ptDevice, &tFBDesc, pcName));    
    }
}

void
pl_create_render_target(plGraphics* ptGraphics, const plRenderTargetDesc* ptDesc, plRenderTarget* ptTargetOut)
{
    ptTargetOut->tDesc = *ptDesc;
    plDevice* ptDevice = &ptGraphics->tDevice;

    const plTextureDesc tColorTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tColorFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {.x = ptDesc->tSize.x, .y = ptDesc->tSize.y, .z = 1.0f},
        .tFormat     = ptDevice->sbtRenderPasses[ptDesc->uRenderPass].tDesc.tDepthFormat,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plSampler tColorSampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tColorView = {
        .tFormat     = tColorTextureDesc.tFormat,
        .uLayerCount = tColorTextureDesc.uLayers,
        .uMips       = tColorTextureDesc.uMips
    };

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t uColorTexture = gptDevice->create_texture(ptDevice, tColorTextureDesc, 0, NULL, "offscreen color texture");
        pl_sb_push(ptTargetOut->sbuColorTextureViews, gptDevice->create_texture_view(ptDevice, &tColorView, &tColorSampler, uColorTexture, "offscreen color view"));
    }

    uint32_t uDepthTexture = gptDevice->create_texture(ptDevice, tDepthTextureDesc, 0, NULL, "offscreen depth texture");

    const plTextureViewDesc tDepthView = {
        .tFormat     = tDepthTextureDesc.tFormat,
        .uLayerCount = tDepthTextureDesc.uLayers,
        .uMips       = tDepthTextureDesc.uMips
    };

    ptTargetOut->uDepthTextureView = gptDevice->create_texture_view(ptDevice, &tDepthView, &tColorSampler, uDepthTexture, "offscreen depth view");

    for(uint32_t i = 0; i < ptGraphics->tSwapchain.uImageCount; i++)
    {
        uint32_t auAttachments[] = {
            ptTargetOut->sbuColorTextureViews[i],
            ptTargetOut->uDepthTextureView
        };

        plFrameBufferDesc tFrameBufferDesc = {
            .uAttachmentCount = 2,
            .uWidth           = (uint32_t)ptDesc->tSize.x,
            .uHeight          = (uint32_t)ptDesc->tSize.y,
            .uRenderPass      = ptDesc->uRenderPass,
            .puAttachments    = auAttachments
        };
        pl_sb_push(ptTargetOut->sbuFrameBuffers, gptDevice->create_frame_buffer(ptDevice, &tFrameBufferDesc, "offscreen target"));
    }
}

void
pl_setup_renderer(const plApiRegistryApiI* ptApiRegistry, plComponentLibrary* ptComponentLibrary, plGraphics* ptGraphics, plRenderer* ptRenderer)
{
    memset(ptRenderer, 0, sizeof(plRenderer));

    plDevice* ptDevice = &ptGraphics->tDevice;

    ptRenderer->ptGraphics = ptGraphics;

    //~~~~~~~~~~~~~~~~~~~~~~~~create shader descriptions~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    plShaderDesc tMainShaderDesc = {
        .pcPixelShader                       = "phong.frag.spv",
        .pcVertexShader                      = "primitive.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 3,
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

    // skybox
    plShaderDesc tSkyboxShaderDesc = {
        .pcPixelShader                       = "skybox.frag.spv",
        .pcVertexShader                      = "skybox.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ADDITIVE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_NONE,
        .tGraphicsState.ulDepthWriteEnabled  = VK_FALSE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 1,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 1,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                },
                .uTextureCount = 1,
                .aTextures     = {
                    { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
                }
            }
        },   
    };

    // outline
    plShaderDesc tOutlineShaderDesc = {
        .pcPixelShader                       = "outline.frag.spv",
        .pcVertexShader                      = "outline.vert.spv",
        .tGraphicsState.ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_NORMAL,
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_ALWAYS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_FRONT_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 3,
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

    // pick
    plShaderDesc tPickShaderDesc = {
        .pcPixelShader                       = "pick.frag.spv",
        .pcVertexShader                      = "pick.vert.spv",
        .tGraphicsState.ulDepthMode          = PL_DEPTH_MODE_LESS,
        .tGraphicsState.ulBlendMode          = PL_BLEND_MODE_NONE,
        .tGraphicsState.ulCullMode           = VK_CULL_MODE_BACK_BIT,
        .tGraphicsState.ulDepthWriteEnabled  = VK_TRUE,
        .tGraphicsState.ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
        .tGraphicsState.ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
        .uBindGroupLayoutCount               = 2,
        .atBindGroupLayouts                  = {
            {
                .uBufferCount = 2,
                .aBuffers = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT }
                },
            },
            {
                .uBufferCount  = 1,
                .aBuffers      = {
                    { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
                }
            }
        },   
    };

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~create shaders~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    ptRenderer->uMainShader    = gptGfx->create_shader(ptGraphics, &tMainShaderDesc);
    ptRenderer->uOutlineShader = gptGfx->create_shader(ptGraphics, &tOutlineShaderDesc);
    ptRenderer->uSkyboxShader  = gptGfx->create_shader(ptGraphics, &tSkyboxShaderDesc);
    ptRenderer->uPickShader    = gptGfx->create_shader(ptGraphics, &tPickShaderDesc);

    // offscreen
    plRenderPassDesc tRenderPassDesc = {
        .tColorFormat = PL_FORMAT_R8G8B8A8_UNORM,
        .tDepthFormat = gptDevice->find_depth_stencil_format(ptDevice)
    };

    ptRenderer->uPickPass = 0u;
    if(!pl__get_free_resource_index(ptDevice->_sbulFrameBufferFreeIndices, &ptRenderer->uPickPass))
        ptRenderer->uPickPass = pl_sb_add_n(ptDevice->sbtRenderPasses, 1);
    ptDevice->sbtRenderPasses[ptRenderer->uPickPass].tDesc = tRenderPassDesc;
    // pl_create_render_pass(ptGraphics, &tRenderPassDesc, &ptRenderer->tPickPass);

    // create render pass
    VkAttachmentDescription atAttachments[] = {

        // color attachment
        {
            .flags          = VK_ATTACHMENT_DESCRIPTION_MAY_ALIAS_BIT,
            .format         = gptDevice->vulkan_format(tRenderPassDesc.tColorFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        },

        // depth attachment
        {
            .format         = gptDevice->vulkan_format(tRenderPassDesc.tDepthFormat),
            .samples        = VK_SAMPLE_COUNT_1_BIT,
            .loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout  = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            .finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        }
    };

    VkSubpassDependency tSubpassDependencies[] = {

        // color attachment
        {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = 0
        },

        // color attachment out
        {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_SHADER_READ_BIT,
            .dependencyFlags = 0
        },
    };

    VkAttachmentReference atAttachmentReferences[] = {
        {
            .attachment = 0,
            .layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
        },
        {
            .attachment = 1,
            .layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL     
        }
    };

    VkSubpassDescription tSubpass = {
        .pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount    = 1,
        .pColorAttachments       = &atAttachmentReferences[0],
        .pDepthStencilAttachment = &atAttachmentReferences[1]
    };

    VkRenderPassCreateInfo tRenderPassInfo = {
        .sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 2,
        .pAttachments    = atAttachments,
        .subpassCount    = 1,
        .pSubpasses      = &tSubpass,
        .dependencyCount = 2,
        .pDependencies   = tSubpassDependencies
    };
    PL_VULKAN(vkCreateRenderPass(ptGraphics->tDevice.tLogicalDevice, &tRenderPassInfo, NULL, &ptDevice->sbtRenderPasses[ptRenderer->uPickPass]._tRenderPass));

    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptRenderer->uPickPass,
        .tSize = {500.0f, 500.0f}
    };
    pl_create_render_target2(ptGraphics, &tRenderTargetDesc, &ptRenderer->tPickTarget, "pick render target");

    ptRenderer->uPickMaterial = gptGfx->add_shader_variant(ptGraphics, ptRenderer->uPickShader, 
        tPickShaderDesc.tGraphicsState, ptRenderer->tPickTarget.tDesc.uRenderPass, VK_SAMPLE_COUNT_1_BIT);
}

void
pl_resize_renderer(plRenderer* ptRenderer, float fWidth, float fHeight)
{
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plRenderTargetDesc tRenderTargetDesc = {
        .uRenderPass = ptRenderer->uPickPass,
        .tSize = {fWidth, fHeight}
    };
    pl_create_render_target2(ptGraphics, &tRenderTargetDesc, &ptRenderer->tPickTarget, "pick target");
}

void
pl_create_scene(plRenderer* ptRenderer, plComponentLibrary* ptComponentLibrary, plScene* ptSceneOut)
{
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    memset(ptSceneOut, 0, sizeof(plScene));

    ptSceneOut->ptComponentLibrary = ptComponentLibrary;
    ptSceneOut->uGlobalVertexData = UINT32_MAX;
    ptSceneOut->ptRenderer = ptRenderer;
    ptSceneOut->bMaterialsNeedUpdate = true;
    ptSceneOut->bMeshesNeedUpdate = true;

    ptSceneOut->uDynamicBuffer0 = gptDevice->create_constant_buffer(ptDevice, ptDevice->uUniformBufferBlockSize, "renderer dynamic buffer 0");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;
    unsigned char* rawBytes0 = gptImage->load("../data/pilotlight-assets-master/SkyBox/right.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes1 = gptImage->load("../data/pilotlight-assets-master/SkyBox/left.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes2 = gptImage->load("../data/pilotlight-assets-master/SkyBox/top.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes3 = gptImage->load("../data/pilotlight-assets-master/SkyBox/bottom.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes4 = gptImage->load("../data/pilotlight-assets-master/SkyBox/front.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    unsigned char* rawBytes5 = gptImage->load("../data/pilotlight-assets-master/SkyBox/back.png", &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
    PL_ASSERT(rawBytes0);
    PL_ASSERT(rawBytes1);
    PL_ASSERT(rawBytes2);
    PL_ASSERT(rawBytes3);
    PL_ASSERT(rawBytes4);
    PL_ASSERT(rawBytes5);

    unsigned char* rawBytes = PL_ALLOC(texWidth * texHeight * texForceNumChannels * 6);
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 0], rawBytes0, texWidth * texHeight * texForceNumChannels); //-V522 
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 1], rawBytes1, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 2], rawBytes2, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 3], rawBytes3, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 4], rawBytes4, texWidth * texHeight * texForceNumChannels); //-V522
    memcpy(&rawBytes[texWidth * texHeight * texForceNumChannels * 5], rawBytes5, texWidth * texHeight * texForceNumChannels); //-V522

    gptImage->free(rawBytes0);
    gptImage->free(rawBytes1);
    gptImage->free(rawBytes2);
    gptImage->free(rawBytes3);
    gptImage->free(rawBytes4);
    gptImage->free(rawBytes5);

    const plTextureDesc tTextureDesc = {
        .tDimensions = {.x = (float)texWidth, .y = (float)texHeight, .z = 1.0f},
        .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
        .tUsage      = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .uLayers     = 6,
        .uMips       = 1,
        .tType       = VK_IMAGE_TYPE_2D
    };

    const plSampler tSkyboxSampler = 
    {
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tFilter = PL_FILTER_LINEAR
    };

    const plTextureViewDesc tSkyboxView = {
        .tFormat     = tTextureDesc.tFormat,
        .uLayerCount = tTextureDesc.uLayers,
        .uMips       = tTextureDesc.uMips
    };

    uint32_t uSkyboxTexture = gptDevice->create_texture(ptDevice, tTextureDesc, sizeof(unsigned char) * texWidth * texHeight * texForceNumChannels * 6, rawBytes, "skybox texture");
    ptSceneOut->uSkyboxTextureView  = gptDevice->create_texture_view(ptDevice, &tSkyboxView, &tSkyboxSampler, uSkyboxTexture, "skybox texture view");
    PL_FREE(rawBytes);

    const float fCubeSide = 0.5f;
    float acSkyBoxVertices[] = {
        -fCubeSide, -fCubeSide, -fCubeSide,
         fCubeSide, -fCubeSide, -fCubeSide,
        -fCubeSide,  fCubeSide, -fCubeSide,
         fCubeSide,  fCubeSide, -fCubeSide,
        -fCubeSide, -fCubeSide,  fCubeSide,
         fCubeSide, -fCubeSide,  fCubeSide,
        -fCubeSide,  fCubeSide,  fCubeSide,
         fCubeSide,  fCubeSide,  fCubeSide 
    };

    uint32_t acSkyboxIndices[] =
    {
        0, 2, 1, 2, 3, 1,
        1, 3, 5, 3, 7, 5,
        2, 6, 3, 3, 6, 7,
        4, 5, 7, 4, 7, 6,
        0, 4, 2, 2, 4, 6,
        0, 1, 4, 1, 5, 4
    };

    ptSceneOut->tSkyboxMesh = (plMesh) {
        .uIndexCount   = 36,
        .uVertexCount  = 24,
        .uIndexBuffer  = gptDevice->create_index_buffer(ptDevice, sizeof(uint32_t) * 36, acSkyboxIndices, "skybox index buffer"),
        .uVertexBuffer = gptDevice->create_vertex_buffer(ptDevice, sizeof(float) * 24, sizeof(float), acSkyBoxVertices, "skybox vertex buffer"),
        .ulVertexStreamMask = PL_MESH_FORMAT_FLAG_NONE
    };

    plBindGroupLayout tSkyboxGroupLayout0 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType       = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}
        },
        .uTextureCount = 1,
        .aTextures     = {
            { .tType = PL_TEXTURE_BINDING_TYPE_SAMPLED, .uSlot = 1, .tStageFlags = VK_SHADER_STAGE_FRAGMENT_BIT }
        }
    };
    size_t szSkyboxRangeSize = sizeof(plGlobalInfo);
    ptSceneOut->tSkyboxBindGroup0.tLayout = tSkyboxGroupLayout0;
    gptGfx->update_bind_group(ptGraphics, &ptSceneOut->tSkyboxBindGroup0, 1, &ptSceneOut->uDynamicBuffer0, &szSkyboxRangeSize, 1, &ptSceneOut->uSkyboxTextureView);

    ptSceneOut->uGlobalVertexData = UINT32_MAX;
    ptSceneOut->uGlobalMaterialData = UINT32_MAX;
    ptSceneOut->uGlobalPickData = UINT32_MAX;

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
    ptSceneOut->tGlobalBindGroup.tLayout = tGlobalGroupLayout;

    plBindGroupLayout tGlobalPickGroupLayout =
    {
        .uBufferCount = 2,
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
        }
    };

    // create & update global bind group
    ptSceneOut->tGlobalPickBindGroup.tLayout = tGlobalPickGroupLayout;
}

void
pl_prepare_gpu_data(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    pl__prepare_material_gpu_data(ptScene, &ptScene->ptComponentLibrary->tMaterialComponentManager);
    pl__prepare_object_gpu_data(ptScene, &ptScene->ptComponentLibrary->tObjectComponentManager);
    pl_end_profile_sample();
}

void
pl_scene_bind_camera(plScene* ptScene, const plCameraComponent* ptCamera)
{
    ptScene->ptCamera = ptCamera;

    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;

    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->tAllocation.pHostMapped[uBufferFrameOffset0];
    ptGlobalInfo->tAmbientColor   = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptCamera->tPos, .w = 0.0f};
    ptGlobalInfo->tCameraView     = ptCamera->tViewMat;
    ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    ptGlobalInfo->fTime           = (float)pl_get_io_context()->dTime;

    plLightComponent* sbtLights = ptScene->ptComponentLibrary->tLightComponentManager.pComponents;
    if(pl_sb_size(sbtLights) > 0)
    {
        ptGlobalInfo->tLightPos = (plVec4){.x = sbtLights[0].tPosition.x, .y = sbtLights[0].tPosition.y, .z = sbtLights[0].tPosition.z};
        ptGlobalInfo->tLightColor = (plVec4){.rgb = sbtLights[0].tColor};
    }
}

void
pl_draw_sky(plScene* ptScene)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = ptScene->ptRenderer->ptGraphics;
    plRenderer* ptRenderer = ptScene->ptRenderer;
    VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    const plBuffer* ptBuffer0 = &ptGraphics->tDevice.sbtBuffers[ptScene->uDynamicBuffer0];
    const uint32_t uBufferFrameOffset0 = ((uint32_t)ptBuffer0->tAllocation.ulSize / ptGraphics->uFramesInFlight) * (uint32_t)ptGraphics->szCurrentFrameIndex + ptScene->uDynamicBuffer0_Offset;

    plGlobalInfo* ptGlobalInfo    = (plGlobalInfo*)&ptBuffer0->tAllocation.pHostMapped[uBufferFrameOffset0];
    ptGlobalInfo->tCameraPos      = (plVec4){.xyz = ptScene->ptCamera->tPos, .w = 0.0f};
    const plMat4 tRemoveTranslation = pl_mat4_translate_xyz(ptScene->ptCamera->tPos.x, ptScene->ptCamera->tPos.y, ptScene->ptCamera->tPos.z);
    ptGlobalInfo->tCameraView     = pl_mul_mat4(&ptScene->ptCamera->tViewMat, &tRemoveTranslation);
    ptGlobalInfo->tCameraViewProj = pl_mul_mat4(&ptScene->ptCamera->tProjMat, &ptGlobalInfo->tCameraView);

    uint32_t uSkyboxShaderVariant = UINT32_MAX;

    const plShader* ptShader = &ptGraphics->sbtShaders[ptRenderer->uSkyboxShader];   
    const uint32_t uFillVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

    plGraphicsState tFillStateTemplate = {
        .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_NONE,
        .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
        .ulDepthWriteEnabled  = false,
        .ulCullMode           = VK_CULL_MODE_NONE,
        .ulBlendMode          = PL_BLEND_MODE_ALPHA,
        .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
        .ulStencilMode        = PL_STENCIL_MODE_NOT_EQUAL,
        .ulStencilRef         = 0xff,
        .ulStencilMask        = 0xff,
        .ulStencilOpFail      = VK_STENCIL_OP_KEEP,
        .ulStencilOpDepthFail = VK_STENCIL_OP_KEEP,
        .ulStencilOpPass      = VK_STENCIL_OP_KEEP
    };

    for(uint32_t j = 0; j < uFillVariantCount; j++)
    {
        if(ptShader->tDesc.sbtVariants[j].tGraphicsState.ulValue == tFillStateTemplate.ulValue 
            && ptScene->ptRenderTarget->tDesc.uRenderPass == ptShader->tDesc.sbtVariants[j].uRenderPass)
        {
                uSkyboxShaderVariant = ptShader->_sbuVariantPipelines[j];
                break;
        }
    }

    // create variant that matches texture count, vertex stream, and culling
    if(uSkyboxShaderVariant == UINT32_MAX)
    {
        pl_log_debug("adding skybox shader variant");
        uSkyboxShaderVariant = gptGfx->add_shader_variant(ptGraphics, ptRenderer->uSkyboxShader, tFillStateTemplate, ptScene->ptRenderTarget->tDesc.uRenderPass, tMSAASampleCount);
    }

    pl_sb_push(ptRenderer->sbtDrawAreas, ((plDrawArea){
        .ptBindGroup0          = &ptScene->tSkyboxBindGroup0,
        .uDrawOffset           = pl_sb_size(ptRenderer->sbtDraws),
        .uDrawCount            = 1,
        .uDynamicBufferOffset0 = uBufferFrameOffset0
    }));

    pl_sb_push(ptRenderer->sbtDraws, ((plDraw){
        .uShaderVariant        = uSkyboxShaderVariant,
        .ptMesh                = &ptScene->tSkyboxMesh,
        .aptBindGroups         = { NULL, NULL },
        .auDynamicBufferOffset = {0, 0}
        }));

    gptGfx->draw_areas(ptRenderer->ptGraphics, pl_sb_size(ptRenderer->sbtDrawAreas), ptRenderer->sbtDrawAreas, ptRenderer->sbtDraws);

    pl_sb_reset(ptRenderer->sbtDraws);
    pl_sb_reset(ptRenderer->sbtDrawAreas);

    ptScene->uDynamicBuffer0_Offset = (uint32_t)pl_align_up((size_t)ptScene->uDynamicBuffer0_Offset + sizeof(plGlobalInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    pl_end_profile_sample();
}

void
pl__prepare_material_gpu_data(plScene* ptScene, plComponentManager* ptManager)
{
    pl_begin_profile_sample(__FUNCTION__);
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    VkSampleCountFlagBits tMSAASampleCount = ptScene->ptRenderTarget->bMSAA ? ptGraphics->tSwapchain.tMsaaSamples : VK_SAMPLE_COUNT_1_BIT;

    plMaterialComponent* sbtComponents = ptManager->pComponents;
    for(uint32_t i = 0; i < pl_sb_size(sbtComponents); i++)
    {

        plMaterialComponent* ptMaterial = &sbtComponents[i];

        const uint32_t acShaderLookup[] = {
            ptScene->ptRenderer->uMainShader,
            ptScene->ptRenderer->uOutlineShader,
            ptMaterial->uShader
        };

        ptMaterial->uShader = acShaderLookup[ptMaterial->tShaderType];
        uint32_t sbuTextures[16] = {0};
        sbuTextures[0] = ptMaterial->uAlbedoMap;
        sbuTextures[1] = ptMaterial->uNormalMap;
        sbuTextures[2] = ptMaterial->uEmissiveMap;

        if(ptScene->bMaterialsNeedUpdate)
        {
            uint64_t uHashKey = pl_hm_hash(&ptMaterial->uShader, sizeof(uint32_t), 0);
            uHashKey = pl_hm_hash(&ptMaterial->uAlbedoMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->uNormalMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->uEmissiveMap, sizeof(uint32_t), uHashKey);
            uHashKey = pl_hm_hash(&ptMaterial->tGraphicsState, sizeof(uint64_t), uHashKey);

            // check if bind group for this buffer exist
            uint64_t uMaterialBindGroupIndex = pl_hm_lookup(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey);

            if(uMaterialBindGroupIndex == UINT64_MAX) // doesn't exist
            {

                plBindGroup tNewBindGroup = {
                    .tLayout = *gptGfx->get_bind_group_layout(ptGraphics, ptMaterial->uShader, 1)
                };
                gptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 0, NULL, NULL, 3, sbuTextures);

                // check for free index
                uMaterialBindGroupIndex = pl_hm_get_free_index(&ptRenderer->tMaterialBindGroupdHashMap);

                if(uMaterialBindGroupIndex == UINT64_MAX) // no free index
                {
                    pl_sb_push(ptRenderer->sbtMaterialBindGroups, tNewBindGroup);
                    uMaterialBindGroupIndex = pl_sb_size(ptRenderer->sbtMaterialBindGroups) - 1;
                    pl_hm_insert(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey, uMaterialBindGroupIndex);
                }
                else // resuse free index
                {
                    ptRenderer->sbtMaterialBindGroups[uMaterialBindGroupIndex] = tNewBindGroup;
                    pl_hm_insert(&ptRenderer->tMaterialBindGroupdHashMap, uHashKey, uMaterialBindGroupIndex);
                }  
            }
            ptMaterial->uBindGroup1 = uMaterialBindGroupIndex;
        }
            
        // find variants
        ptMaterial->uShaderVariant = UINT32_MAX;

        ptMaterial->tGraphicsState.ulCullMode = ptMaterial->bDoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
        if(ptMaterial->uAlbedoMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_0;
        if(ptMaterial->uNormalMap > 0)   ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_1;
        if(ptMaterial->uEmissiveMap > 0) ptMaterial->tGraphicsState.ulShaderTextureFlags |= PL_SHADER_TEXTURE_FLAG_BINDING_2;

        const plShader* ptShader = &ptGraphics->sbtShaders[ptMaterial->uShader];   
        const uint32_t uVariantCount = pl_sb_size(ptShader->tDesc.sbtVariants);

        for(uint32_t k = 0; k < uVariantCount; k++)
        {
            plGraphicsState ptVariant = ptShader->tDesc.sbtVariants[k].tGraphicsState;
            if(ptVariant.ulValue == ptMaterial->tGraphicsState.ulValue 
                && ptShader->tDesc.sbtVariants[k].uRenderPass == ptScene->ptRenderTarget->tDesc.uRenderPass
                && tMSAASampleCount == ptShader->tDesc.sbtVariants[k].tMSAASampleCount)
            {
                    ptMaterial->uShaderVariant = ptShader->_sbuVariantPipelines[k];
                    break;
            }
        }

        // create variant that matches texture count, vertex stream, and culling
        if(ptMaterial->uShaderVariant == UINT32_MAX)
        {
            ptMaterial->uShaderVariant = gptGfx->add_shader_variant(ptGraphics, ptMaterial->uShader, ptMaterial->tGraphicsState, ptScene->ptRenderTarget->tDesc.uRenderPass, tMSAASampleCount);
        }
    }

    ptScene->bMaterialsNeedUpdate = false;
    pl_end_profile_sample();
}

void
pl__prepare_object_gpu_data(plScene* ptScene, plComponentManager* ptManager)
{
    plRenderer* ptRenderer = ptScene->ptRenderer;
    plGraphics* ptGraphics = ptRenderer->ptGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plObjectSystemData* ptObjectSystemData = ptManager->pSystemData;
    if(!ptObjectSystemData->bDirty)
        return;

    pl_begin_profile_sample(__FUNCTION__);

    const size_t szSizeWithPadding = pl_align_up(sizeof(plObjectInfo), ptGraphics->tDevice.tDeviceProps.limits.minUniformBufferOffsetAlignment);
    plObjectComponent* sbtComponents = ptManager->pComponents;

    const uint32_t uMaxObjectsPerBuffer = (uint32_t)(ptDevice->uUniformBufferBlockSize / (uint32_t)szSizeWithPadding) - 1;
    uint32_t uMeshCount = pl_sb_size(ptObjectSystemData->sbtMeshes);

    const uint32_t uMinBuffersNeeded = (uint32_t)ceilf((float)uMeshCount / (float)uMaxObjectsPerBuffer);
    uint32_t uCurrentObject = 0;

    const plBindGroupLayout tGroupLayout2 = {
        .uBufferCount = 1,
        .aBuffers      = {
            { .tType = PL_BUFFER_BINDING_TYPE_UNIFORM, .uSlot = 0, .tStageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT}  
        }
    };
    size_t szRangeSize2 = sizeof(plObjectInfo);

    for(uint32_t i = 0; i < uMinBuffersNeeded; i++)
    {
        const uint32_t uDynamicBufferIndex = gptDevice->request_dynamic_buffer(ptDevice);
        plDynamicBufferNode* ptDynamicBufferNode = &ptDevice->_sbtDynamicBufferList[uDynamicBufferIndex];
        plBuffer* ptBuffer = &ptDevice->sbtBuffers[ptDynamicBufferNode->uDynamicBuffer];

        const uint64_t uHashKey = pl_hm_hash(&uDynamicBufferIndex, sizeof(uint64_t), 0);

        // check if bind group for this buffer exist
        uint64_t uObjectBindGroupIndex = pl_hm_lookup(&ptRenderer->tObjectBindGroupdHashMap, uHashKey);

        if(uObjectBindGroupIndex == UINT64_MAX) // doesn't exist
        {
            plBindGroup tNewBindGroup = {
                .tLayout = tGroupLayout2
            };
            gptGfx->update_bind_group(ptGraphics, &tNewBindGroup, 1, &ptDynamicBufferNode->uDynamicBuffer, &szRangeSize2, 0, NULL);

            // check for free index
            uObjectBindGroupIndex = pl_hm_get_free_index(&ptRenderer->tObjectBindGroupdHashMap);

            if(uObjectBindGroupIndex == UINT64_MAX) // no free index
            {
                pl_sb_push(ptRenderer->sbtObjectBindGroups, tNewBindGroup);
                uObjectBindGroupIndex = pl_sb_size(ptRenderer->sbtObjectBindGroups) - 1;
                pl_hm_insert(&ptRenderer->tObjectBindGroupdHashMap, uHashKey, uObjectBindGroupIndex);
            }
            else // resuse free index
            {
                ptRenderer->sbtObjectBindGroups[uObjectBindGroupIndex] = tNewBindGroup;
                pl_hm_insert(&ptRenderer->tObjectBindGroupdHashMap, uHashKey, uObjectBindGroupIndex);
            }  
        }

        uint32_t uIterationObjectCount = pl_minu(uMaxObjectsPerBuffer, uMeshCount);
        for(uint32_t j = 0; j < uIterationObjectCount; j++)
        {
            ptObjectSystemData->sbtMeshes[uCurrentObject]->tMesh.uIndexBuffer = ptScene->uIndexBuffer;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->tMesh.uVertexBuffer = ptScene->uVertexBuffer;
            plObjectInfo* ptObjectInfo = (plObjectInfo*)(ptBuffer->tAllocation.pHostMapped + ptDynamicBufferNode->uDynamicBufferOffset);
            *ptObjectInfo = ptObjectSystemData->sbtMeshes[uCurrentObject]->tInfo;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->uBindGroup2 = uObjectBindGroupIndex;
            ptObjectSystemData->sbtMeshes[uCurrentObject]->uBufferOffset = ptDynamicBufferNode->uDynamicBufferOffset;
            ptDynamicBufferNode->uDynamicBufferOffset += (uint32_t)szSizeWithPadding;
            uCurrentObject++;
        }
        uMeshCount = uMeshCount - uIterationObjectCount;

        pl_sb_push(ptDevice->_sbuDynamicBufferDeletionQueue, uDynamicBufferIndex);

        if(uMeshCount == 0)
            break;
    }
    ptObjectSystemData->bDirty = false;
    pl_end_profile_sample();
}