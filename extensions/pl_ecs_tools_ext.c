/*
   pl_ecs_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] viewport helpers
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <float.h>
#include "pl.h"
#include "pl_ecs_tools_ext.h"
#include "pl_draw_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_asset_ext.h"
#include "pl_animation_ext.h"
#include "pl_camera_ext.h"
#include "pl_renderer_ext.h"
#include "pl_ui_ext.h"
#include "pl_physics_ext.h"
#include "pl_mesh_ext.h"
#include "pl_material_ext.h"
#include "pl_script_ext.h"
#include "pl_transform_ext.h"
#include "pl_ik_ext.h"
#include "pl_skeleton_ext.h"
#include "pl_shader_interop_renderer.h"
#include "pl_texture_ext.h"
#include "pl_terrain_ext.h"
#include "pl_graphics_ext.h"
#include "pl_console_ext.h"
#include "pl_voxel_ext.h"
#include "pl_path_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#include "pl_string.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

static const plMemoryI*   gptMemory = NULL;
static const plDrawI*     gptDraw = NULL;
#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

static const plEcsI*      gptEcs      = NULL;
static const plAnimationI*  gptAnimation = NULL;
static const plUiI*       gptUI       = NULL;
static const plIOI*       gptIOI       = NULL;
static const plRendererI* gptRenderer = NULL;
static const plPhysicsI*  gptPhysics = NULL;
static const plCameraI*   gptCamera = NULL;
static const plCameraEcsI*   gptCameraEcs = NULL;
static const plMeshI*     gptMesh = NULL;
static const plMaterialI* gptMaterial = NULL;
static const plScriptI*   gptScript = NULL;
static const plResourceI*   gptResource = NULL;
static const plAssetI*   gptAsset = NULL;
static const plTransformI* gptTransform = NULL;
static const plIkI* gptIk = NULL;
static const plSkeletonI* gptSkeleton = NULL;
static const plTextureI*       gptTexture       = NULL;
static const plTerrainI*       gptTerrain       = NULL;
static const plGraphicsI*       gptGfx       = NULL;
static const plConsoleI*       gptConsole       = NULL;
static const plVoxelI*         gptVoxel         = NULL;
static const plPathI*          gptPath          = NULL;

#ifndef PL_DS_ALLOC
    
    #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
    #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

#include "pl_ds.h"
#endif

#define PL_ICON_FA_FILTER "\xef\x82\xb0"	// U+f0b0
#define PL_ICON_FA_MAGNIFYING_GLASS "\xef\x80\x82"	// U+f002
#define PL_ICON_FA_SITEMAP "\xef\x83\xa8"	// U+f0e8
#define PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT "\xef\x81\x87"	// U+f047
#define PL_ICON_FA_CUBE "\xef\x86\xb2"	// U+f1b2
#define PL_ICON_FA_GHOST "\xef\x9b\xa2"	// U+f6e2
#define PL_ICON_FA_MAP "\xef\x89\xb9"	// U+f279
#define PL_ICON_FA_CAMERA "\xef\x80\xb0"	// U+f030
#define PL_ICON_FA_PLAY "\xef\x81\x8b"	// U+f04b
#define PL_ICON_FA_DRAW_POLYGON "\xef\x97\xae"	// U+f5ee
#define PL_ICON_FA_LIGHTBULB "\xef\x83\xab"	// U+f0eb
#define PL_ICON_FA_MAP_PIN "\xef\x89\xb6"	// U+f276
#define PL_ICON_FA_PERSON "\xef\x86\x83"	// U+f183
#define PL_ICON_FA_CODE "\xef\x84\xa1"	// U+f121
#define PL_ICON_FA_BOXES_STACKED "\xef\x91\xa8"	// U+f468
#define PL_ICON_FA_WIND "\xef\x9c\xae"	// U+f72e
#define PL_ICON_FA_FILM "\xef\x80\x88"	// U+f008
#define PL_ICON_FA_CLOUD_SUN "\xef\x9b\x84"	// U+f6c4
#define PL_ICON_FA_FOLDER_OPEN "\xef\x81\xbc"	// U+f07c
#define PL_ICON_FA_USER_INJURED "\xef\x9c\xa8"	// U+f728
#define PL_ICON_FA_GLOBE "\xef\x82\xac"	// U+f0ac
#define PL_ICON_FA_PALETTE "\xef\x94\xbf"	// U+f53f
#define PL_ICON_FA_CHESS_BOARD "\xef\x90\xbc"	// U+f43c
#define PL_ICON_FA_DRAW_POLYGON "\xef\x97\xae"	// U+f5ee
#define PL_ICON_FA_BONE "\xef\x97\x97"	// U+f5d7
#define PL_ICON_FA_TRASH_CAN "\xef\x8b\xad"	// U+f2ed
#define PL_ICON_FA_STAR "\xef\x80\x85"	// U+f005
#define PL_ICON_FA_ROUTE "\xef\x93\x97"	// U+f4d7
#define PL_ICON_FA_VECTOR_SQUARE "\xef\x97\x8b"	// U+f5cb
#define PL_ICON_FA_SHAPES "\xef\x98\x9f"	// U+f61f

static const char* apcComponentNames[] = {
    "None",
    PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT " Transform",
    PL_ICON_FA_GHOST " Object",
    PL_ICON_FA_SITEMAP " Hierarchy",
    PL_ICON_FA_USER_INJURED " Skin",
    PL_ICON_FA_CAMERA " Camera",
    PL_ICON_FA_PLAY " Animation",
    PL_ICON_FA_DRAW_POLYGON " Inverse Kinematics",
    PL_ICON_FA_LIGHTBULB " Light",
    PL_ICON_FA_MAP_PIN " Environment Probe",
    PL_ICON_FA_PERSON " Humanoid",
    PL_ICON_FA_CODE " Script",
    PL_ICON_FA_BOXES_STACKED " Rigid Body Physics",
    PL_ICON_FA_WIND " Force Field",
    PL_ICON_FA_CLOUD_SUN " Environment",
    PL_ICON_FA_FILM " Renderer",
    PL_ICON_FA_GLOBE " Terrain",
    PL_ICON_FA_FOLDER_OPEN " Library",
    PL_ICON_FA_STAR " Decal",
    PL_ICON_FA_VECTOR_SQUARE " Voxel Volume",
    PL_ICON_FA_SHAPES " Voxel Geometry",
    PL_ICON_FA_ROUTE " Path Agent",
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plLibraryToolHelper
{
    plEntity       tSelectedEntity;
    plUiTextFilter tFilter;
    uint32_t       uComponentFilter;
    plEntity       tNavigationEntity;
    plVec3         tPathStart;
    plVec3         tPathGoal;
    char           acNavigationPath[1024];
    char           acNavigationMeshPath[1024];
} plLibraryToolHelper;

typedef struct _plEcsToolsContext
{
    char*                sbcBuffer;
    bool                 bShowTool;
    plAssetHandle        tLibraryAsset;
    plHashMap            tLibraryHashmap;
    plLibraryToolHelper* sbtLibraryData;
    bool                 bNavigationComponents;
    bool                 bVoxelAssets;
    int                  iNavigationAction;
    plAssetHandle        tNavigationLibrary;
    plEntity             tNavigationEntity;
    char                 acNavigationPath[1024];
    char                 acNavigationMessage[256];
} plEcsToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plEcsToolsContext* gptEcsToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static plLibraryToolHelper*
pl__ecs_tools_library(plAssetHandle tAsset)
{
    if(!gptAsset->is_valid(tAsset) || gptAsset->get_type_key(tAsset) != gptEcs->get_asset_type_key())
        return NULL;
    if(!pl_hm_has_key(&gptEcsToolsCtx->tLibraryHashmap, tAsset.uData))
    {
        uint64_t uIndex = pl_sb_size(gptEcsToolsCtx->sbtLibraryData);
        pl_sb_push(gptEcsToolsCtx->sbtLibraryData, ((plLibraryToolHelper){.tSelectedEntity = {.uData = UINT64_MAX}}));
        pl_hm_insert(&gptEcsToolsCtx->tLibraryHashmap, tAsset.uData, uIndex);
    }
    return &gptEcsToolsCtx->sbtLibraryData[pl_hm_lookup(&gptEcsToolsCtx->tLibraryHashmap, tAsset.uData)];
}

static bool
pl__ecs_tools_navigation_available(void)
{
    const plComponentDesc* atTypes = NULL;
    uint32_t uCount = gptEcs->get_type_descriptions(&atTypes);
    plEcsTypeKey atKeys[] = {gptVoxel->get_ecs_type_key_volume(), gptVoxel->get_ecs_type_key_geometry(), gptPath->get_ecs_type_key_agent()};
    const char* apcNames[] = {"voxel_volume", "voxel_geometry", "path_agent"};
    for(uint32_t i = 0; i < PL_ARRAYSIZE(atKeys); i++)
    {
        if(atKeys[i] >= uCount || strcmp(atTypes[atKeys[i]].pcName, apcNames[i]) != 0)
            return false;
    }
    return true;
}

static void
pl__ecs_tools_dirty_voxels(plComponentLibrary* ptLibrary)
{
    plVoxelVolumeComponent* atVolumes = NULL;
    uint32_t uCount = gptEcs->get_components(ptLibrary, gptVoxel->get_ecs_type_key_volume(), (void**)&atVolumes, NULL);
    for(uint32_t i = 0; i < uCount; i++)
    {
        if(!atVolumes[i].tAsset.uData)
            atVolumes[i].bRebuild = true;
    }
}

static bool
pl__ecs_tools_asset_combo(const char* pcLabel, plAssetTypeKey tType, plAssetHandle* ptAsset)
{
    bool bChanged = false;
    const char* pcPreview = gptAsset->is_valid(*ptAsset) ? gptAsset->get_path(*ptAsset) : "None";
    if(gptUI->begin_combo(pcLabel, pcPreview, PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
    {
        bool bSelected = !ptAsset->uData;
        if(gptUI->selectable("None", &bSelected, 0))
        {
            *ptAsset = (plAssetHandle){0};
            bChanged = true;
            gptUI->close_current_popup();
        }
        uint32_t uCount = 0;
        const plAssetHandle* atAssets = gptAsset->get_assets_by_type(tType, &uCount);
        for(uint32_t i = 0; i < uCount; i++)
        {
            bSelected = ptAsset->uData == atAssets[i].uData;
            if(gptUI->selectable(gptAsset->get_path(atAssets[i]), &bSelected, 0))
            {
                *ptAsset = atAssets[i];
                bChanged = true;
                gptUI->close_current_popup();
            }
        }
        gptUI->end_combo();
    }
    return bChanged;
}

static bool
pl__ecs_tools_entity_combo(const char* pcLabel, plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntityId* ptId, const char* pcNone)
{
    bool bChanged = false;
    plEntity tCurrent = gptEcs->get_entity_by_id(ptLibrary, *ptId);
    const plTagComponent* ptTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tCurrent);
    const char* pcPreview = *ptId ? (ptTag && ptTag->pcName ? ptTag->pcName : "Missing / unnamed entity") : pcNone;
    if(gptUI->begin_combo(pcLabel, pcPreview, PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
    {
        bool bSelected = !*ptId;
        if(gptUI->selectable(pcNone, &bSelected, 0))
        {
            *ptId = 0;
            bChanged = true;
            gptUI->close_current_popup();
        }
        const plEntity* atEntities = NULL;
        uint32_t uCount = gptEcs->get_components(ptLibrary, tType, NULL, &atEntities);
        for(uint32_t i = 0; i < uCount; i++)
        {
            plEntityId tId = gptEcs->get_entity_id(ptLibrary, atEntities[i]);
            ptTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), atEntities[i]);
            bSelected = tId == *ptId;
            gptUI->push_id_uint(atEntities[i].uIndex);
            if(gptUI->selectable(ptTag && ptTag->pcName ? ptTag->pcName : "Unnamed", &bSelected, 0))
            {
                *ptId = tId;
                bChanged = true;
                gptUI->close_current_popup();
            }
            gptUI->pop_id();
        }
        gptUI->end_combo();
    }
    return bChanged;
}

static void
pl__ecs_tools_voxel_properties(const plVoxelGrid* ptGrid)
{
    if(!ptGrid || !ptGrid->_puBits)
    {
        gptUI->text("No grid built");
        return;
    }
    plAABB tBounds = gptVoxel->get_bounds(ptGrid);
    gptUI->text("Resolution: %u x %u x %u", ptGrid->auResolution[0], ptGrid->auResolution[1], ptGrid->auResolution[2]);
    gptUI->text("Cells: %u / occupied: %u", ptGrid->uCellCount, gptVoxel->count_occupied(ptGrid));
    gptUI->text("Occupancy: %.2f KiB", (double)ptGrid->szMemory / 1024.0);
    gptUI->text("Cell size: %g", ptGrid->fVoxelSize);
    gptUI->text("Min: %g, %g, %g", tBounds.tMin.x, tBounds.tMin.y, tBounds.tMin.z);
    gptUI->text("Max: %g, %g, %g", tBounds.tMax.x, tBounds.tMax.y, tBounds.tMax.z);
}

static void
pl__ecs_tools_queue_navigation(int iAction, plAssetHandle tLibrary, plEntity tEntity, const char* pcPath)
{
    gptEcsToolsCtx->iNavigationAction = iAction;
    gptEcsToolsCtx->tNavigationLibrary = tLibrary;
    gptEcsToolsCtx->tNavigationEntity = tEntity;
    pl_snprintf(gptEcsToolsCtx->acNavigationPath, sizeof(gptEcsToolsCtx->acNavigationPath), "%s", pcPath);
    gptEcsToolsCtx->acNavigationMessage[0] = 0;
}

static const char*
pl__ecs_tools_execute_navigation(int iAction)
{
    const char* pcError = "Navigation asset action failed";
    const char* pcPath = gptEcsToolsCtx->acNavigationPath;
    plAssetHandle tLibraryAsset = gptEcsToolsCtx->tNavigationLibrary;
    plEntity tEntity = gptEcsToolsCtx->tNavigationEntity;
    if(!gptAsset->is_valid(tLibraryAsset))
        return pcError;
    plComponentLibrary* ptLibrary = gptAsset->get_data(tLibraryAsset);
    plEcsTypeKey tComponentType = iAction == 3 ? gptVoxel->get_ecs_type_key_geometry() : gptVoxel->get_ecs_type_key_volume();
    void* pComponent = gptEcs->get_component(ptLibrary, tComponentType, tEntity);
    if(!pComponent || !pcPath[0])
        return pcError;
    plAssetTypeKey tAssetType = iAction == 3 ? gptMesh->get_asset_type_key() : gptVoxel->get_asset_type_key();
    plAssetHandle tAsset = {0};
    if(iAction == 1)
    {
        plVoxelVolumeComponent* ptVolume = pComponent;
        const char* pcExtension = strrchr(pcPath, '.');
        if(!pcExtension || strcmp(pcExtension, ".plvoxel") != 0)
            return "Bake path must end in .plvoxel";
        if(ptVolume->bRebuild || ptVolume->bBuildFailed || !ptVolume->_tGrid._puBits)
            return "Rebuild the volume successfully before baking";
        tAsset = gptAsset->find(pcPath);
        if(gptAsset->is_valid(tAsset))
        {
            if(gptAsset->get_type_key(tAsset) != tAssetType || !gptVoxel->copy(gptAsset->get_data(tAsset), &ptVolume->_tGrid))
                return pcError;
            gptAsset->mark_changed(tAsset);
        }
        else
            tAsset = gptVoxel->create_asset(pcPath, &ptVolume->_tGrid);
        if(!gptAsset->is_valid(tAsset))
            return pcError;
        if(!gptAsset->save(tAsset, PL_ASSET_ENCODING_BINARY))
            return "Save failed; the baked asset remains in memory";
    }
    else
        tAsset = gptAsset->load(pcPath);
    if(!gptAsset->is_valid(tAsset) || gptAsset->get_type_key(tAsset) != tAssetType)
        return "Asset could not be loaded or has the wrong type";
    if(iAction == 3)
    {
        ((plVoxelGeometryComponent*)pComponent)->tMesh = tAsset;
        pl__ecs_tools_dirty_voxels(ptLibrary);
    }
    else
    {
        ((plVoxelVolumeComponent*)pComponent)->tAsset = tAsset;
        ((plVoxelVolumeComponent*)pComponent)->bRebuild = true;
    }
    gptEcs->mark_component_changed(ptLibrary, tEntity, tComponentType);
    return iAction == 1 ? "Baked, saved and assigned voxel asset" : "Loaded and assigned asset";
}

static void
pl__ecs_tools_apply_navigation(void)
{
    int iAction = gptEcsToolsCtx->iNavigationAction;
    gptEcsToolsCtx->iNavigationAction = 0;
    if(!iAction)
        return;
    const char* pcMessage = pl__ecs_tools_execute_navigation(iAction);
    pl_snprintf(gptEcsToolsCtx->acNavigationMessage, sizeof(gptEcsToolsCtx->acNavigationMessage), "%s", pcMessage);
}

// Asset loads/bakes are deferred until after asset iteration.
static void
pl__ecs_tools_navigation(plAssetHandle tLibraryAsset, plComponentLibrary* ptLibrary, plEntity tEntity, plLibraryToolHelper* ptHelper)
{
    if(!gptEcsToolsCtx->bNavigationComponents || !gptEcs->is_entity_valid(ptLibrary, tEntity))
        return;
    const plEcsTypeKey tVolumeType = gptVoxel->get_ecs_type_key_volume();
    const plEcsTypeKey tGeometryType = gptVoxel->get_ecs_type_key_geometry();
    const plEcsTypeKey tAgentType = gptPath->get_ecs_type_key_agent();
    gptUI->push_id_string("navigation");
    gptUI->layout_dynamic(0, 1);
    if(ptHelper->tNavigationEntity.uData != tEntity.uData)
    {
        ptHelper->tNavigationEntity = tEntity;
        ptHelper->tPathStart = (plVec3){0};
        ptHelper->tPathGoal = (plVec3){0};
        pl_snprintf(ptHelper->acNavigationPath, sizeof(ptHelper->acNavigationPath), "/cache/navigation_%" PRIu64 ".plvoxel", gptEcs->get_entity_id(ptLibrary, tEntity));
        ptHelper->acNavigationMeshPath[0] = 0;
        const plVoxelVolumeComponent* ptSelectedVolume = gptEcs->get_component(ptLibrary, tVolumeType, tEntity);
        const plVoxelGeometryComponent* ptSelectedGeometry = gptEcs->get_component(ptLibrary, tGeometryType, tEntity);
        const plPathAgentComponent* ptSelectedAgent = gptEcs->get_component(ptLibrary, tAgentType, tEntity);
        if(ptSelectedVolume && gptAsset->is_valid(ptSelectedVolume->tAsset))
            pl_snprintf(ptHelper->acNavigationPath, sizeof(ptHelper->acNavigationPath), "%s", gptAsset->get_path(ptSelectedVolume->tAsset));
        if(ptSelectedGeometry && gptAsset->is_valid(ptSelectedGeometry->tMesh))
            pl_snprintf(ptHelper->acNavigationMeshPath, sizeof(ptHelper->acNavigationMeshPath), "%s", gptAsset->get_path(ptSelectedGeometry->tMesh));
        if(ptSelectedAgent)
        {
            ptHelper->tPathStart = ptSelectedAgent->_tStart;
            ptHelper->tPathGoal = ptSelectedAgent->_tGoal;
        }
    }
    plVoxelVolumeComponent* ptVolume = gptEcs->get_component(ptLibrary, tVolumeType, tEntity);
    if(ptVolume && gptUI->collapsing_header("Voxel Volume", 0))
    {
        bool bChanged = false;
        bChanged |= gptUI->input_float3("World min", ptVolume->tBounds.tMin.d, "%g", 0);
        bChanged |= gptUI->input_float3("World max", ptVolume->tBounds.tMax.d, "%g", 0);
        bChanged |= gptUI->input_float("Voxel size", &ptVolume->fVoxelSize, "%g", 0);
        bChanged |= gptUI->input_uint("Volume layer mask", &ptVolume->uLayerMask, 0);
        bChanged |= gptUI->checkbox("Fill enclosed cells", &ptVolume->bFillInterior);
        if(gptEcsToolsCtx->bVoxelAssets)
        {
            bChanged |= pl__ecs_tools_asset_combo("Baked grid", gptVoxel->get_asset_type_key(), &ptVolume->tAsset);
            gptUI->input_text("Voxel asset path", ptHelper->acNavigationPath, sizeof(ptHelper->acNavigationPath), 0);
            if(gptUI->button("Load voxel asset"))
                pl__ecs_tools_queue_navigation(2, tLibraryAsset, tEntity, ptHelper->acNavigationPath);
            gptUI->text("Baking replaces the asset at this path.");
            if(gptUI->button("Bake, save and assign"))
                pl__ecs_tools_queue_navigation(1, tLibraryAsset, tEntity, ptHelper->acNavigationPath);
        }
        if(ptVolume->tAsset.uData)
            gptUI->text("Baked grid supplies bounds, cell size and occupancy.");
        if(bChanged)
        {
            ptVolume->bRebuild = true;
            gptEcs->mark_component_changed(ptLibrary, tEntity, tVolumeType);
        }
        if(gptUI->button("Rebuild volume"))
            ptVolume->bRebuild = true;
        gptUI->text("Revision: %" PRIu64 " / %s", ptVolume->uRevision, ptVolume->bRebuild ? "rebuild requested" : ptVolume->bBuildFailed ? "build failed; previous grid retained" : "ready");
        pl__ecs_tools_voxel_properties(gptVoxel->get_volume_grid(ptLibrary, tEntity, NULL));
        if(gptUI->button(PL_ICON_FA_TRASH_CAN " Remove voxel volume"))
        {
            gptEcs->remove_component(ptLibrary, tVolumeType, tEntity);
            gptPath->run_update_system(ptLibrary, 0);
        }
    }
    plVoxelGeometryComponent* ptGeometry = gptEcs->get_component(ptLibrary, tGeometryType, tEntity);
    if(ptGeometry && gptUI->collapsing_header("Voxel Geometry", 0))
    {
        bool bChanged = false;
        bChanged |= gptUI->checkbox("Enabled", &ptGeometry->bEnabled);
        bChanged |= pl__ecs_tools_asset_combo("Mesh", gptMesh->get_asset_type_key(), &ptGeometry->tMesh);
        bChanged |= pl__ecs_tools_entity_combo("Geometry transform", ptLibrary, gptTransform->get_ecs_type_key_transform(), &ptGeometry->tTransformId, "This entity / identity");
        bChanged |= gptUI->input_uint("First submesh", &ptGeometry->uFirstSubmesh, 0);
        bChanged |= gptUI->input_uint("Submesh count", &ptGeometry->uSubmeshCount, 0);
        bChanged |= gptUI->input_uint("Geometry layer mask", &ptGeometry->uLayerMask, 0);
        gptUI->text("UINT32_MAX submeshes selects the remaining range.");
        if(gptUI->button("Copy mesh and transform from Object"))
        {
            const plObjectComponent* ptObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), tEntity);
            if(ptObject)
            {
                ptGeometry->tMesh = ptObject->tMesh;
                ptGeometry->tTransformId = gptEcs->get_entity_id(ptLibrary, ptObject->tTransform);
                ptGeometry->uFirstSubmesh = ptObject->uFirstSubmesh;
                ptGeometry->uSubmeshCount = ptObject->uSubmeshCount;
                ptGeometry->bEnabled = true;
                bChanged = true;
            }
        }
        gptUI->input_text("Mesh asset path", ptHelper->acNavigationMeshPath, sizeof(ptHelper->acNavigationMeshPath), 0);
        if(gptUI->button("Load mesh asset"))
            pl__ecs_tools_queue_navigation(3, tLibraryAsset, tEntity, ptHelper->acNavigationMeshPath);
        if(bChanged)
        {
            gptEcs->mark_component_changed(ptLibrary, tEntity, tGeometryType);
            pl__ecs_tools_dirty_voxels(ptLibrary);
        }
        if(gptUI->button("Rebuild scene geometry volumes"))
            pl__ecs_tools_dirty_voxels(ptLibrary);
        if(gptUI->button(PL_ICON_FA_TRASH_CAN " Remove voxel geometry"))
        {
            gptEcs->remove_component(ptLibrary, tGeometryType, tEntity);
            pl__ecs_tools_dirty_voxels(ptLibrary);
        }
    }
    plPathAgentComponent* ptAgent = gptEcs->get_component(ptLibrary, tAgentType, tEntity);
    if(ptAgent && gptUI->collapsing_header("Path Agent", 0))
    {
        bool bChanged = pl__ecs_tools_entity_combo("Navigation volume", ptLibrary, tVolumeType, &ptAgent->tVolumeId, "None");
        plPathSettings* ptSettings = &ptAgent->tSettings;
        bChanged |= gptUI->checkbox("Flying", &ptSettings->bFlying);
        bChanged |= gptUI->checkbox("Simplify", &ptSettings->bSimplify);
        bChanged |= gptUI->checkbox("Diagonals", &ptSettings->bAllowDiagonal);
        if(gptUI->input_int("Height (cells)", &ptSettings->iAgentHeight, 0))
        {
            if(ptSettings->iAgentHeight < 1) ptSettings->iAgentHeight = 1;
            bChanged = true;
        }
        bChanged |= gptUI->slider_int("Radius (cells)", &ptSettings->iAgentRadius, 0, 1024, 0);
        bChanged |= gptUI->slider_int("Max step (cells)", &ptSettings->iMaxStep, 0, 64, 0);
        bChanged |= gptUI->slider_int("Snap radius (cells)", &ptSettings->iSnapRadius, 0, 32, 0);
        bChanged |= gptUI->slider_uint("Max nodes", &ptSettings->uMaxNodes, 1, 1048576, 0);
        bool bCenter = ptSettings->tGroundSupport == PL_PATH_GROUND_SUPPORT_CENTER;
        if(gptUI->checkbox("Center ground support", &bCenter))
        {
            ptSettings->tGroundSupport = bCenter ? PL_PATH_GROUND_SUPPORT_CENTER : PL_PATH_GROUND_SUPPORT_FULL;
            bChanged = true;
        }
        if(bChanged)
        {
            gptPath->cancel_request(ptLibrary, tEntity);
            gptEcs->mark_component_changed(ptLibrary, tEntity, tAgentType);
        }
        gptUI->input_float3("Start", ptHelper->tPathStart.d, "%g", 0);
        gptUI->input_float3("Goal", ptHelper->tPathGoal.d, "%g", 0);
        if(gptUI->button("Start at agent transform"))
        {
            const plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity);
            if(ptTransform)
                ptHelper->tPathStart = ptTransform->tWorld.col[3].xyz;
        }
        gptUI->text("Endpoints are world-space bottom-cell centers.");
        if(gptUI->button("Request path"))
            gptPath->request(ptLibrary, tEntity, ptHelper->tPathStart, ptHelper->tPathGoal);
        if(gptUI->button("Cancel path"))
            gptPath->cancel_request(ptLibrary, tEntity);
        gptUI->text("Status: %s", gptPath->status_string(ptAgent->tStatus));
        gptUI->text("Waypoints: %u / length: %g", ptAgent->uWaypointCount, ptAgent->fLength);
        gptUI->text("Volume revision: %" PRIu64, ptAgent->uVolumeRevision);
        if(gptUI->button(PL_ICON_FA_TRASH_CAN " Remove path agent"))
        {
            gptPath->cancel_request(ptLibrary, tEntity);
            gptEcs->remove_component(ptLibrary, tAgentType, tEntity);
        }
    }
    if(gptEcsToolsCtx->acNavigationMessage[0])
        gptUI->text("%s", gptEcsToolsCtx->acNavigationMessage);
    gptUI->pop_id();
}

static bool
pl__ecs_tools_entity(plComponentLibrary* ptLibrary, plEntity* ptSelectedEntity, plEntity tEntity, const char* pcName)
{
    bool bSelected = ptSelectedEntity->uData == tEntity.uData;

    const plEcsTypeKey tTransformComponentType = gptTransform->get_ecs_type_key_transform();
    const plEcsTypeKey tObjectComponentType = gptRenderer->get_ecs_type_key_object();
    const plEcsTypeKey tHierarchyComponentType = gptTransform->get_ecs_type_key_hierarchy();
    const plEcsTypeKey tSkinComponentType = gptSkeleton->get_ecs_type_key_skin();
    const plEcsTypeKey tCameraComponentType = gptCameraEcs->get_ecs_type_key();
    const plEcsTypeKey tAnimationComponentType = gptAnimation->get_ecs_type_key_animation();
    const plEcsTypeKey tInverseKinematicsComponentType = gptIk->get_ecs_type_key();
    const plEcsTypeKey tLightComponentType = gptRenderer->get_ecs_type_key_light();
    const plEcsTypeKey tEnvironmentProbeComponentType = gptRenderer->get_ecs_type_key_environment_probe();
    const plEcsTypeKey tHumanoidComponentType = gptAnimation->get_ecs_type_key_humanoid();
    const plEcsTypeKey tScriptComponentType = gptScript->get_ecs_type_key();
    const plEcsTypeKey tRigidBodyComponentType = gptPhysics->get_ecs_type_key_rigid_body_physics();
    const plEcsTypeKey tForceFieldComponentType = gptPhysics->get_ecs_type_key_force_field();
    const plEcsTypeKey tEnvironmentComponentType = gptRenderer->get_ecs_type_key_environment();
    const plEcsTypeKey tRendererComponentType = gptRenderer->get_ecs_type_key_renderer();
    const plEcsTypeKey tTerrainComponentType = gptRenderer->get_ecs_type_key_terrain();
    const plEcsTypeKey tDecalComponentType = gptRenderer->get_ecs_type_key_decal();

    bool bResult = false;
    plTagComponent*               ptTagComp           = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tEntity);
    plLibraryComponent*           ptLibraryComp       = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_library(), tEntity);
    plTransformComponent*         ptTransformComp     = gptEcs->get_component(ptLibrary, tTransformComponentType, tEntity);
    plObjectComponent*            ptObjectComp        = gptEcs->get_component(ptLibrary, tObjectComponentType, tEntity);
    plHierarchyComponent*         ptHierarchyComp     = gptEcs->get_component(ptLibrary, tHierarchyComponentType, tEntity);
    plSkinComponent*              ptSkinComp          = gptEcs->get_component(ptLibrary, tSkinComponentType, tEntity);
    plCamera*                     ptCameraComp        = gptEcs->get_component(ptLibrary, tCameraComponentType, tEntity);
    plAnimationComponent*         ptAnimationComp     = gptEcs->get_component(ptLibrary, tAnimationComponentType, tEntity);
    plInverseKinematicsComponent* ptIKComp            = gptEcs->get_component(ptLibrary, tInverseKinematicsComponentType, tEntity);
    plLightComponent*             ptLightComp         = gptEcs->get_component(ptLibrary, tLightComponentType, tEntity);
    plEnvironmentProbeComponent*  ptProbeComp         = gptEcs->get_component(ptLibrary, tEnvironmentProbeComponentType, tEntity);
    plHumanoidComponent*          ptHumanComp         = gptEcs->get_component(ptLibrary, tHumanoidComponentType, tEntity);
    plScriptComponent*            ptScriptComp        = gptEcs->get_component(ptLibrary, tScriptComponentType, tEntity);
    plRigidBodyPhysicsComponent*  ptRigidComp         = gptEcs->get_component(ptLibrary, tRigidBodyComponentType, tEntity);
    plForceFieldComponent*        ptForceField        = gptEcs->get_component(ptLibrary, tForceFieldComponentType, tEntity);
    plEnvironmentComponent*       ptEnvironment       = gptEcs->get_component(ptLibrary, tEnvironmentComponentType, tEntity);
    plRendererComponent*          ptRenderer          = gptEcs->get_component(ptLibrary, tRendererComponentType, tEntity);
    plTerrainComponent*           ptTerrain           = gptEcs->get_component(ptLibrary, tTerrainComponentType, tEntity);
    plDecalComponent*             ptDecal             = gptEcs->get_component(ptLibrary, tDecalComponentType, tEntity);


    pl_sb_reset(gptEcsToolsCtx->sbcBuffer);
    if(ptHierarchyComp) { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_SITEMAP);                    pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptTransformComp) { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT);  pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptObjectComp)    { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_GHOST);                      pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptSkinComp)      { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_USER_INJURED);               pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptCameraComp)    { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CAMERA);                     pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptAnimationComp) { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_PLAY);                       pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptIKComp)        { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_DRAW_POLYGON);               pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptLightComp)     { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_LIGHTBULB);                  pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptProbeComp)     { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_MAP_PIN);                    pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptHumanComp)     { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_PERSON);                     pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptScriptComp)    { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CODE);                       pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptRigidComp)     { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_BOXES_STACKED);              pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptForceField)    { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_WIND);                       pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptEnvironment)   { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CLOUD_SUN);                  pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptRenderer)      { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_FILM);                       pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptLibraryComp)   { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_FOLDER_OPEN);                pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptTerrain)       { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_GLOBE);                      pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(ptDecal)         { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_STAR);                       pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    if(gptEcsToolsCtx->bNavigationComponents)
    {
        if(gptEcs->has_component(ptLibrary, gptVoxel->get_ecs_type_key_volume(), tEntity))
        { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_VECTOR_SQUARE); pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
        if(gptEcs->has_component(ptLibrary, gptVoxel->get_ecs_type_key_geometry(), tEntity))
        { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_SHAPES); pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
        if(gptEcs->has_component(ptLibrary, gptPath->get_ecs_type_key_agent(), tEntity))
        { pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, "%s", PL_ICON_FA_ROUTE); pl_sb_pop(gptEcsToolsCtx->sbcBuffer); }
    }
    pl_sb_sprintf(gptEcsToolsCtx->sbcBuffer, " %s", pcName);

    bool bLocal = gptEcs->is_local(ptLibrary, tEntity);

    gptUI->push_theme_color(PL_UI_COLOR_HEADER, (plVec4){0.0f, 0.51f, 0.0f, 1.00f});

    if(!bLocal)
    {
        gptUI->push_theme_color(PL_UI_COLOR_TEXT, (plVec4){0.4f, 0.9f, 0.4f, 1.00f});
    }

    // gptUI->push_id_uint(i);
    if(gptUI->selectable(gptEcsToolsCtx->sbcBuffer, &bSelected, 0))
    {
        if(bSelected)
        {
            *ptSelectedEntity = tEntity;
            if(ptSelectedEntity->uIndex != UINT32_MAX)
                bResult = true;
        }
        else
        {
            ptSelectedEntity->uIndex = UINT32_MAX;
            ptSelectedEntity->uGeneration = UINT32_MAX;
            bResult = true;
        }
    }

    if(!bLocal)
        gptUI->pop_theme_color(1);
    gptUI->pop_theme_color(1);

    // gptUI->pop_id();
    return bResult;
}

static bool
pl__ecs_tools_split_object(plComponentLibrary* ptLibrary, plEntity tEntity, plEntity* atEntitiesOut, uint32_t* puCount)
{
    if(!puCount)
        return false;

    uint32_t uCapacity = atEntitiesOut ? *puCount : 0;
    *puCount = 0;
    if(!ptLibrary || !gptEcs->is_entity_valid(ptLibrary, tEntity))
        return false;

    plEcsTypeKey tObjectType = gptRenderer->get_ecs_type_key_object();
    plObjectComponent* ptSource = gptEcs->get_component(ptLibrary, tObjectType, tEntity);
    if(!ptSource || !gptAsset->is_valid(ptSource->tMesh) || gptAsset->get_type_key(ptSource->tMesh) != gptMesh->get_asset_type_key() || gptEcs->has_component(ptLibrary, gptSkeleton->get_ecs_type_key_skin(), tEntity))
        return false;

    const plMesh* ptMesh = gptAsset->get_data(ptSource->tMesh);
    uint32_t uCount = ptSource->uSubmeshCount;
    uint32_t uFirst = ptSource->uFirstSubmesh;
    plEntity tParent = gptEcs->get_entity_by_id(ptLibrary, ptSource->tTransformId);
    if(!ptMesh || uCount < 2 || uFirst >= ptMesh->uSubmeshCount || uCount > ptMesh->uSubmeshCount - uFirst || !gptEcs->has_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tParent))
        return false;

    *puCount = uCount;
    if(!atEntitiesOut)
        return true;
    if(uCapacity < uCount)
        return false;

    const plEcsTypeKey tTransformType = gptTransform->get_ecs_type_key_transform();
    const plEcsTypeKey tHierarchyType = gptTransform->get_ecs_type_key_hierarchy();
    const plEntityId tSourceId = gptEcs->get_entity_id(ptLibrary, tEntity);

    // Each piece replaces the source at the same hierarchy level. Copying local
    // transforms preserves nonuniform scale/rotation without decomposing a world matrix.
    // Names are interned; save the string, not a pointer into the movable tag array.
    const plTagComponent* ptTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tEntity);
    const char* pcName = ptTag && ptTag->pcName ? ptTag->pcName : "Object";
    for(uint32_t i = 0; i < uCount; i++)
    {
        char acName[256];
        pl_snprintf(acName, sizeof(acName), "%s / submesh %u", pcName, uFirst + i);
        plEntity tPiece = gptTransform->create_transform(ptLibrary, acName, NULL);
        gptEcs->copy_component(ptLibrary, tTransformType, tParent, tPiece);
        if(gptEcs->has_component(ptLibrary, tHierarchyType, tParent))
            gptEcs->copy_component(ptLibrary, tHierarchyType, tParent, tPiece);
        gptEcs->copy_component(ptLibrary, tObjectType, tEntity, tPiece);
        plObjectComponent* ptPiece = gptEcs->get_component(ptLibrary, tObjectType, tPiece);
        if(ptPiece->atMaterialOverrides)
        {
            ptPiece->atMaterialOverrides[0] = ptPiece->atMaterialOverrides[i];
            ptPiece->atMaterialOverrides = PL_REALLOC(ptPiece->atMaterialOverrides, sizeof(plAssetHandle));
        }
        ptPiece->uFirstSubmesh = uFirst + i;
        ptPiece->uSubmeshCount = 1;
        ptPiece->tTransform = tPiece;
        ptPiece->tTransformId = gptEcs->get_entity_id(ptLibrary, tPiece);
        ptPiece->tFlags = (ptPiece->tFlags & ~PL_OBJECT_FLAGS_OUTLINE) | PL_OBJECT_FLAGS_DIRTY;
        atEntitiesOut[i] = tPiece;
    }

    // The first piece replaces the deleted transform for existing children and
    // objects that referenced it. The other pieces remain independent siblings.
    plEntity tReplacement = atEntitiesOut[0];
    plHierarchyComponent* ptHierarchy = NULL;
    const plEntity* atHierarchyEntities = NULL;
    uint32_t uHierarchyCount = gptEcs->get_components(ptLibrary, tHierarchyType, (void**)&ptHierarchy, &atHierarchyEntities);

    // An object can render using a different entity's transform. If its own
    // transform also has dependents, preserve that distinct transform on a new
    // transform-only entity rather than moving those dependents to the mesh.
    if(tParent.uData != tEntity.uData && gptEcs->has_component(ptLibrary, tTransformType, tEntity))
    {
        bool bReferenced = false;
        for(uint32_t i = 0; i < uHierarchyCount; i++)
            bReferenced |= ptHierarchy[i].tParent.uData == tEntity.uData;
        plObjectComponent* ptReferences = NULL;
        uint32_t uReferenceCount = gptEcs->get_components(ptLibrary, tObjectType, (void**)&ptReferences, NULL);
        for(uint32_t i = 0; i < uReferenceCount; i++)
            bReferenced |= ptReferences[i].tTransformId == tSourceId;
        if(bReferenced)
        {
            tReplacement = gptTransform->create_transform(ptLibrary, "Split reference transform", NULL);
            gptEcs->copy_component(ptLibrary, tTransformType, tEntity, tReplacement);
            if(gptEcs->has_component(ptLibrary, tHierarchyType, tEntity))
                gptEcs->copy_component(ptLibrary, tHierarchyType, tEntity, tReplacement);
            uHierarchyCount = gptEcs->get_components(ptLibrary, tHierarchyType, (void**)&ptHierarchy, &atHierarchyEntities);
        }
    }
    for(uint32_t i = 0; i < uHierarchyCount; i++)
    {
        if(ptHierarchy[i].tParent.uData == tEntity.uData)
        {
            gptTransform->attach_component(ptLibrary, atHierarchyEntities[i], tReplacement);
            gptEcs->mark_component_changed(ptLibrary, atHierarchyEntities[i], tHierarchyType);
        }
    }
    plObjectComponent* ptObjects = NULL;
    const plEntity* atObjectEntities = NULL;
    uint32_t uObjectCount = gptEcs->get_components(ptLibrary, tObjectType, (void**)&ptObjects, &atObjectEntities);
    for(uint32_t i = 0; i < uObjectCount; i++)
    {
        if(atObjectEntities[i].uData != tEntity.uData && ptObjects[i].tTransformId == tSourceId)
        {
            ptObjects[i].tTransform = tReplacement;
            ptObjects[i].tTransformId = gptEcs->get_entity_id(ptLibrary, tReplacement);
            ptObjects[i].tFlags |= PL_OBJECT_FLAGS_DIRTY;
            gptEcs->mark_component_changed(ptLibrary, atObjectEntities[i], tObjectType);
        }
    }
    gptEcs->remove_entity(ptLibrary, tEntity);
    return true;
}

bool
pl__ecs_tools_show_window(plAssetHandle tAssetHandle, plEntity* ptSelectedEntity, bool* pbShowWindow)
{

    plComponentLibrary* ptLibrary = gptAsset->get_data(tAssetHandle);
    const char* pcLibraryName = gptAsset->get_path(tAssetHandle);
    bool bSplitObject = false;

    bool bResult = false;

    plLibraryToolHelper* ptLibraryHelper = pl__ecs_tools_library(tAssetHandle);

    if(gptUI->begin_window("ECS", pbShowWindow, false))
    {
        gptUI->layout_dynamic(0, 1);
        gptUI->text("%s", pcLibraryName);
        const plVec2 tWindowSize = gptUI->get_window_size();
        const float pfRatios[] = {0.5f, 0.5f};

        gptUI->layout_static(0.0f, 100.0f, 1);
        if(gptUI->button("Save"))
        {
            gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_TEXT);
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        gptUI->text("Entities");
        gptUI->text("Components");
        gptUI->layout_dynamic(0.0f, 1);
        gptUI->separator();
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios);
        if(gptUI->input_text_hint(PL_ICON_FA_MAGNIFYING_GLASS, "Filter (inc,-exc)", ptLibraryHelper->tFilter.acInputBuffer, 256, 0))
        {
            gptUI->text_filter_build(&ptLibraryHelper->tFilter);
        }

        const plEcsTypeKey tTransformComponentType = gptTransform->get_ecs_type_key_transform();
        const plEcsTypeKey tObjectComponentType = gptRenderer->get_ecs_type_key_object();
        const plEcsTypeKey tHierarchyComponentType = gptTransform->get_ecs_type_key_hierarchy();
        const plEcsTypeKey tSkinComponentType = gptSkeleton->get_ecs_type_key_skin();
        const plEcsTypeKey tCameraComponentType = gptCameraEcs->get_ecs_type_key();
        const plEcsTypeKey tAnimationComponentType = gptAnimation->get_ecs_type_key_animation();
        const plEcsTypeKey tInverseKinematicsComponentType = gptIk->get_ecs_type_key();
        const plEcsTypeKey tLightComponentType = gptRenderer->get_ecs_type_key_light();
        const plEcsTypeKey tEnvironmentProbeComponentType = gptRenderer->get_ecs_type_key_environment_probe();
        const plEcsTypeKey tHumanoidComponentType = gptAnimation->get_ecs_type_key_humanoid();
        const plEcsTypeKey tScriptComponentType = gptScript->get_ecs_type_key();
        const plEcsTypeKey tRigidBodyComponentType = gptPhysics->get_ecs_type_key_rigid_body_physics();
        const plEcsTypeKey tForceFieldComponentType = gptPhysics->get_ecs_type_key_force_field();
        const plEcsTypeKey tEnvironmentComponentType = gptRenderer->get_ecs_type_key_environment();
        const plEcsTypeKey tRendererComponentType = gptRenderer->get_ecs_type_key_renderer();
        const plEcsTypeKey tTerrainComponentType = gptRenderer->get_ecs_type_key_terrain();
        const plEcsTypeKey tLibraryComponentType = gptEcs->get_ecs_type_key_library();
        const plEcsTypeKey tDecalComponentType = gptRenderer->get_ecs_type_key_decal();

        plEcsTypeKey atComponentTypes[] = {
            INT32_MAX,
            tTransformComponentType,
            tObjectComponentType,
            tHierarchyComponentType,
            tSkinComponentType,
            tCameraComponentType,
            tAnimationComponentType,
            tInverseKinematicsComponentType,
            tLightComponentType,
            tEnvironmentProbeComponentType,
            tHumanoidComponentType,
            tScriptComponentType,
            tRigidBodyComponentType,
            tForceFieldComponentType,
            tEnvironmentComponentType,
            tRendererComponentType,
            tTerrainComponentType,
            tLibraryComponentType,
            tDecalComponentType,
            gptEcsToolsCtx->bNavigationComponents ? gptVoxel->get_ecs_type_key_volume() : UINT32_MAX,
            gptEcsToolsCtx->bNavigationComponents ? gptVoxel->get_ecs_type_key_geometry() : UINT32_MAX,
            gptEcsToolsCtx->bNavigationComponents ? gptPath->get_ecs_type_key_agent() : UINT32_MAX,
        };

        if(ptLibraryHelper->uComponentFilter >= PL_ARRAYSIZE(atComponentTypes) || atComponentTypes[ptLibraryHelper->uComponentFilter] == UINT32_MAX)
            ptLibraryHelper->uComponentFilter = 0;
        bool abCombo[PL_ARRAYSIZE(atComponentTypes)] = {0};
        abCombo[ptLibraryHelper->uComponentFilter] = true;
        if(gptUI->begin_combo(PL_ICON_FA_FILTER, apcComponentNames[ptLibraryHelper->uComponentFilter], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
        {
            for(uint32_t i = 0; i < PL_ARRAYSIZE(atComponentTypes); i++)
            {
                if(atComponentTypes[i] == UINT32_MAX)
                    continue;
                if(gptUI->selectable(apcComponentNames[i], &abCombo[i], 0))
                {
                    ptLibraryHelper->uComponentFilter = i;
                    gptUI->close_current_popup();
                } 
            }
            gptUI->end_combo();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 135.0f, 2, pfRatios);

        if(gptUI->begin_child("Entities", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            plTagComponent* ptTags = NULL;
            const plEntity* ptEntities = NULL;
            const uint32_t uEntityCount = gptEcs->get_components(ptLibrary, gptEcs->get_ecs_type_key_tag(), (void**)&ptTags, &ptEntities);

            if(ptLibraryHelper->uComponentFilter != 0 || gptUI->text_filter_active(&ptLibraryHelper->tFilter))
            {
                for(uint32_t i = 0; i < uEntityCount; i++)
                {
                    if(gptUI->text_filter_pass(&ptLibraryHelper->tFilter, ptTags[i].pcName, NULL))
                    {
                        bool bSelected = ptSelectedEntity->uData == ptEntities[i].uData;

                        if(ptLibraryHelper->uComponentFilter != 0)
                        {
                            void* pComponent = gptEcs->get_component(ptLibrary, atComponentTypes[ptLibraryHelper->uComponentFilter], ptEntities[i]);
                            if(pComponent == NULL)
                                continue;
                        }

                        gptUI->push_id_uint(i);
                        pl__ecs_tools_entity(ptLibrary, ptSelectedEntity, ptEntities[i], ptTags[i].pcName);
                        gptUI->pop_id();
                    }
                }
            }
            else
            {
                plUiClipper tClipper = {(uint32_t)uEntityCount};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        gptUI->push_id_uint(i);
                        pl__ecs_tools_entity(ptLibrary, ptSelectedEntity, ptEntities[i], ptTags[i].pcName);
                        gptUI->pop_id();
                    }
                }
            }

            gptUI->end_child();
        }

        if(gptUI->begin_child("Components", 0, 0))
        {
            const float pfRatiosInner[] = {1.0f};
            gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

            // if(ptSelectedEntity->uData != UINT64_MAX)
            if(gptEcs->is_entity_valid(ptLibrary, *ptSelectedEntity))
            {
                gptUI->push_id_uint(ptSelectedEntity->uIndex);

                plTagComponent*               ptTagComp           = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), *ptSelectedEntity);
                plTransformComponent*         ptTransformComp     = gptEcs->get_component(ptLibrary, tTransformComponentType, *ptSelectedEntity);
                plObjectComponent*            ptObjectComp        = gptEcs->get_component(ptLibrary, tObjectComponentType, *ptSelectedEntity);
                plHierarchyComponent*         ptHierarchyComp     = gptEcs->get_component(ptLibrary, tHierarchyComponentType, *ptSelectedEntity);
                plSkinComponent*              ptSkinComp          = gptEcs->get_component(ptLibrary, tSkinComponentType, *ptSelectedEntity);
                plCamera*                     ptCameraComp        = gptEcs->get_component(ptLibrary, tCameraComponentType, *ptSelectedEntity);
                plAnimationComponent*         ptAnimationComp     = gptEcs->get_component(ptLibrary, tAnimationComponentType, *ptSelectedEntity);
                plInverseKinematicsComponent* ptIKComp            = gptEcs->get_component(ptLibrary, tInverseKinematicsComponentType, *ptSelectedEntity);
                plLightComponent*             ptLightComp         = gptEcs->get_component(ptLibrary, tLightComponentType, *ptSelectedEntity);
                plEnvironmentProbeComponent*  ptProbeComp         = gptEcs->get_component(ptLibrary, tEnvironmentProbeComponentType, *ptSelectedEntity);
                plHumanoidComponent*          ptHumanComp         = gptEcs->get_component(ptLibrary, tHumanoidComponentType, *ptSelectedEntity);
                plScriptComponent*            ptScriptComp        = gptEcs->get_component(ptLibrary, tScriptComponentType, *ptSelectedEntity);
                plRigidBodyPhysicsComponent*  ptRigidComp         = gptEcs->get_component(ptLibrary, tRigidBodyComponentType, *ptSelectedEntity);
                plForceFieldComponent*        ptForceField        = gptEcs->get_component(ptLibrary, tForceFieldComponentType, *ptSelectedEntity);
                plLibraryComponent*           ptLibraryComp       = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_library(), *ptSelectedEntity);
                plEnvironmentComponent*       ptEnvironment       = gptEcs->get_component(ptLibrary, tEnvironmentComponentType, *ptSelectedEntity);
                plRendererComponent*          ptRenderer          = gptEcs->get_component(ptLibrary, tRendererComponentType, *ptSelectedEntity);
                plTerrainComponent*           ptTerrain           = gptEcs->get_component(ptLibrary, tTerrainComponentType, *ptSelectedEntity);
                plDecalComponent*             ptDecal             = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_decal(), *ptSelectedEntity);



                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_push_static(25.0f);
                gptUI->layout_template_end();

                gptUI->text("Entity: {i-%u, g-%u}", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);

                static char acEntityIdBuffer[64] = {0};
                snprintf(acEntityIdBuffer, 64, "%" PRIu64, gptEcs->get_entity_id(ptLibrary, *ptSelectedEntity));

                if(gptUI->button(PL_ICON_FA_TRASH_CAN "##entity"))
                {
                    gptEcs->remove_entity(ptLibrary, *ptSelectedEntity);
                    if(gptEcsToolsCtx->bNavigationComponents)
                    {
                        pl__ecs_tools_dirty_voxels(ptLibrary);
                        gptPath->run_update_system(ptLibrary, 0);
                    }
                    ptTagComp           = NULL;
                    ptTransformComp     = NULL;
                    ptObjectComp        = NULL;
                    ptHierarchyComp     = NULL;
                    ptSkinComp          = NULL;
                    ptCameraComp        = NULL;
                    ptAnimationComp     = NULL;
                    ptIKComp            = NULL;
                    ptLightComp         = NULL;
                    ptProbeComp         = NULL;
                    ptHumanComp         = NULL;
                    ptScriptComp        = NULL;
                    ptRigidComp         = NULL;
                    ptForceField        = NULL;
                    ptLibraryComp       = NULL;
                    ptEnvironment       = NULL;
                    ptRenderer          = NULL;
                    ptTerrain           = NULL;
                    ptDecal             = NULL;
                }

                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);
                gptUI->input_text("ID", acEntityIdBuffer, 64, PL_UI_INPUT_TEXT_FLAGS_READ_ONLY);

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_push_static(25.0f);
                gptUI->layout_template_end();
                
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

                if(ptTagComp)
                {
                    if(gptUI->collapsing_header("Tag", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Name: %s", ptTagComp->pcName);
                        gptUI->apply_layout();
                    }
                }

                gptUI->layout_template_begin(0.0f);
                gptUI->layout_template_push_dynamic();
                gptUI->layout_template_push_static(25.0f);
                gptUI->layout_template_end();

                if(ptLibraryComp)
                {
                    if(gptUI->collapsing_header("Library", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Library: %s", gptAsset->get_path(ptLibraryComp->tSourceLibrary));
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##0"))
                    {
                        gptEcs->remove_component(ptLibrary, gptEcs->get_ecs_type_key_library(), *ptSelectedEntity);
                        ptLibraryComp = NULL;
                    }
                }

                if(ptEnvironment)
                {
                    if(gptUI->collapsing_header("Environment", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Environment: %s", gptAsset->get_path(ptEnvironment->tEnvironment));
                        gptUI->apply_layout();
                    }

                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##1"))
                    {
                        gptEcs->remove_component(ptLibrary, tEnvironmentProbeComponentType, *ptSelectedEntity);
                        ptEnvironment = NULL;
                    }
                }

                if(ptRenderer)
                {
                    if(gptUI->collapsing_header("Renderer", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Renderer: %s", gptAsset->get_path(ptRenderer->tRenderer));
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##2"))
                    {
                        gptEcs->remove_component(ptLibrary, tRendererComponentType, *ptSelectedEntity);
                        ptRenderer = NULL;
                    }
                }

                if(ptDecal)
                {
                    if(gptUI->collapsing_header("Decal", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox("Enabled", &ptDecal->bEnabled);
                        gptUI->slider_float("Opacity", &ptDecal->fOpacity, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Normal Cutoff", &ptDecal->fNormalCutoff, -1.0f, 1.0f, 0);
                        gptUI->slider_float("Edge Fade", &ptDecal->fEdgeFade, 0.0f, 1.0f, 0);
                        gptUI->input_int("Sort Order", &ptDecal->iSortOrder, 0);
                        if(gptAsset->is_valid(ptDecal->tMaterial))
                            gptUI->text("Material: %s", gptAsset->get_path(ptDecal->tMaterial));
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##decal"))
                    {
                        gptEcs->remove_component(ptLibrary, gptRenderer->get_ecs_type_key_decal(), *ptSelectedEntity);
                        ptDecal = NULL;
                    }
                }

                if(ptTerrain)
                {
                    if(gptUI->collapsing_header("Terrain", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Terrain: %s", gptAsset->get_path(ptTerrain->tTerrain));
                        gptUI->slider_float("fTau", &ptTerrain->fTau, 0.0f, 1.0f, 0);

                        gptUI->checkbox_flags("Wireframe", &ptTerrain->tFlags, PL_TERRAIN_FLAGS_WIREFRAME);
                        gptUI->checkbox_flags("Show Levels", &ptTerrain->tFlags, PL_TERRAIN_FLAGS_SHOW_LEVELS);
                        gptUI->checkbox_flags("Show Tiles", &ptTerrain->tFlags, PL_TERRAIN_FLAGS_SHOW_TILES);

                        gptUI->slider_float("fSlopeStart", &ptTerrain->fSlopeStart, 0.0f, 1.0f, 0);
                        gptUI->slider_float("fSlopeEnd", &ptTerrain->fSlopeEnd, 0.0f, 1.0f, 0);

                        gptUI->input_float("Terrain Depth Bias", &ptTerrain->fTerrainShadowConstantDepthBias, "%g", 0);
                        gptUI->input_float("Terrain Slope Depth Bias", &ptTerrain->fTerrainShadowSlopeDepthBias, "%g", 0);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##3"))
                    {
                        gptEcs->remove_component(ptLibrary, tTerrainComponentType, *ptSelectedEntity);
                        ptTerrain = NULL;
                    }

                    
                }

                if(ptScriptComp)
                {
                    if(gptUI->collapsing_header("Script", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("File: %s", ptScriptComp->pcPath);

                        gptUI->checkbox_flags("Playing", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_PLAYING);
                        gptUI->checkbox_flags("Play Once", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_PLAY_ONCE);
                        gptUI->checkbox_flags("Reloadable", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_RELOADABLE);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##4"))
                    {
                        gptEcs->remove_component(ptLibrary, tScriptComponentType, *ptSelectedEntity);
                        ptScriptComp = NULL;
                    }
                }

                if(ptHumanComp)
                {
                    if(gptUI->collapsing_header("Humanoid", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##5"))
                    {
                        gptEcs->remove_component(ptLibrary, tHumanoidComponentType, *ptSelectedEntity);
                        ptHumanComp = NULL;
                    }
                }

                if(ptRigidComp)
                {
                    if(gptUI->collapsing_header("Rigid Body Physics", 0))
                    {
                        gptUI->stash_layout();
                        if(gptUI->checkbox_flags("No Sleeping", &ptRigidComp->tFlags, PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING))
                        {
                            if(ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_NO_SLEEPING)
                                gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                        }

                        if(gptUI->checkbox_flags("Kinematic", &ptRigidComp->tFlags, PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC))
                        {
                            if(ptRigidComp->tFlags & PL_RIGID_BODY_PHYSICS_FLAG_KINEMATIC)
                                gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                        }
                        gptUI->input_float("Mass", &ptRigidComp->fMass, "%g", 0);
                        gptUI->slider_float("Friction", &ptRigidComp->fFriction, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Restitution", &ptRigidComp->fRestitution, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Linear Damping", &ptRigidComp->fLinearDamping, 0.0f, 1.0f, 0);
                        gptUI->slider_float("Angluar Damping", &ptRigidComp->fAngularDamping, 0.0f, 1.0f, 0);
                        gptUI->input_float3("Gravity", ptRigidComp->tGravity.d, NULL, 0);
                        gptUI->input_float3("Local Offset", ptRigidComp->tLocalOffset.d, "%g", 0);

                        gptUI->vertical_spacing();

                        gptUI->separator_text("Collision Shape");
                        gptUI->radio_button("Box", &ptRigidComp->tShape, PL_COLLISION_SHAPE_BOX);
                        gptUI->radio_button("Sphere", &ptRigidComp->tShape, PL_COLLISION_SHAPE_SPHERE);

                        gptUI->vertical_spacing();

                        if(ptRigidComp->tShape == PL_COLLISION_SHAPE_BOX)
                        {
                            gptUI->input_float3("Extents", ptRigidComp->tExtents.d, "%g", 0);
                        }
                        else if(ptRigidComp->tShape == PL_COLLISION_SHAPE_SPHERE)
                        {
                            gptUI->input_float("Radius", &ptRigidComp->fRadius, "%g", 0);
                        }

                        static plVec3 tPoint = {0};
                        static plVec3 tForce = {1000.0f};
                        static plVec3 tTorque = {0.0f, 100.0f, 0.0f};
                        static plVec3 tLinearVelocity = {0.0f, 0.0f, 0.0f};
                        static plVec3 tAngularVelocity = {0.0f, 0.0f, 0.0f};
                        gptUI->input_float3("Point", tPoint.d, NULL, 0);
                        gptUI->input_float3("Force", tForce.d, NULL, 0);
                        gptUI->input_float3("Torque", tTorque.d, NULL, 0);
                        gptUI->input_float3("Velocity", tLinearVelocity.d, NULL, 0);
                        gptUI->input_float3("Angular Velocity", tAngularVelocity.d, NULL, 0);

                        gptUI->push_theme_color(PL_UI_COLOR_BUTTON, (plVec4){0.02f, 0.51f, 0.10f, 1.00f});
                        gptUI->push_theme_color(PL_UI_COLOR_BUTTON_HOVERED, (plVec4){ 0.02f, 0.61f, 0.10f, 1.00f});
                        gptUI->push_theme_color(PL_UI_COLOR_BUTTON_ACTIVE, (plVec4){0.02f, 0.87f, 0.10f, 1.00f});

                        gptUI->layout_dynamic(0.0f, 2);
                        if(gptUI->button("Stop"))
                        {
                            gptPhysics->set_linear_velocity(ptLibrary, *ptSelectedEntity, (plVec3){0});
                            gptPhysics->set_angular_velocity(ptLibrary, *ptSelectedEntity, (plVec3){0});
                        }
                        
                        if(gptUI->button("Set Velocity"))          gptPhysics->set_linear_velocity(ptLibrary, *ptSelectedEntity, tLinearVelocity);
                        if(gptUI->button("Set A. Velocity"))       gptPhysics->set_angular_velocity(ptLibrary, *ptSelectedEntity, tAngularVelocity);
                        if(gptUI->button("torque"))                gptPhysics->apply_torque(ptLibrary, *ptSelectedEntity, tTorque);
                        if(gptUI->button("impulse torque"))        gptPhysics->apply_impulse_torque(ptLibrary, *ptSelectedEntity, tTorque);
                        if(gptUI->button("force"))                 gptPhysics->apply_force(ptLibrary, *ptSelectedEntity, tForce);
                        if(gptUI->button("force at point"))        gptPhysics->apply_force_at_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                        if(gptUI->button("force at body point"))   gptPhysics->apply_force_at_body_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                        if(gptUI->button("impulse"))               gptPhysics->apply_impulse(ptLibrary, *ptSelectedEntity, tForce);
                        if(gptUI->button("impulse at point"))      gptPhysics->apply_impulse_at_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                        if(gptUI->button("impulse at body point")) gptPhysics->apply_impulse_at_body_point(ptLibrary, *ptSelectedEntity, tForce, tPoint);
                        if(gptUI->button("wake up"))               gptPhysics->wake_up_body(ptLibrary, *ptSelectedEntity);
                        if(gptUI->button("sleep"))                 gptPhysics->sleep_body(ptLibrary, *ptSelectedEntity);
                        gptUI->invisible_button("not_used", (plVec2){1.0f, 1.0f});

                        gptUI->pop_theme_color(3);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##5"))
                    {
                        gptEcs->remove_component(ptLibrary, tRigidBodyComponentType, *ptSelectedEntity);
                        ptRigidComp = NULL;
                    }
                }

                if(ptProbeComp)
                {
                    if(gptUI->collapsing_header("Environment Probe", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox_flags("Real Time", &ptProbeComp->tFlags, PL_ENVIRONMENT_PROBE_FLAGS_REALTIME);
                        gptUI->checkbox_flags("Include Sky", &ptProbeComp->tFlags, PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY);
                        gptUI->checkbox_flags("Box Parallax Correction", &ptProbeComp->tFlags, PL_ENVIRONMENT_PROBE_FLAGS_PARALLAX_CORRECTION_BOX);

                        if(gptUI->button("Update"))
                            ptProbeComp->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_DIRTY;
                        gptUI->input_float("Range", &ptProbeComp->fRange, NULL, 0);

                        uint32_t auSamples[] = {
                            32,
                            64,
                            128,
                            256,
                            512,
                            1024,
                            2048,
                            4096,
                        };
                        int iSelection = 0;
                        if(ptProbeComp->uSamples == 32)        iSelection = 0;
                        else if(ptProbeComp->uSamples == 64)   iSelection = 1;
                        else if(ptProbeComp->uSamples == 128)  iSelection = 2;
                        else if(ptProbeComp->uSamples == 256)  iSelection = 3;
                        else if(ptProbeComp->uSamples == 512)  iSelection = 4;
                        else if(ptProbeComp->uSamples == 1024) iSelection = 5;
                        else if(ptProbeComp->uSamples == 2048) iSelection = 6;
                        else if(ptProbeComp->uSamples == 4096) iSelection = 7;
                        gptUI->separator_text("Samples");
                        gptUI->radio_button("32", &iSelection, 0);
                        gptUI->radio_button("64", &iSelection, 1);
                        gptUI->radio_button("128", &iSelection, 2);
                        gptUI->radio_button("256", &iSelection, 3);
                        gptUI->radio_button("512", &iSelection, 4);
                        gptUI->radio_button("1024", &iSelection, 5);
                        gptUI->radio_button("2048", &iSelection, 6);
                        gptUI->radio_button("4096", &iSelection, 7);
                        ptProbeComp->uSamples = auSamples[iSelection];

                        gptUI->separator_text("Capture Faces per Scheduled Turn");

                        int iSelection0 = (int)ptProbeComp->uInterval;
                        gptUI->radio_button("1", &iSelection0, 1);
                        gptUI->radio_button("2", &iSelection0, 2);
                        gptUI->radio_button("3", &iSelection0, 3);
                        gptUI->radio_button("4", &iSelection0, 4);
                        gptUI->radio_button("5", &iSelection0, 5);
                        gptUI->radio_button("6", &iSelection0, 6);

                        ptProbeComp->uInterval = (uint32_t)iSelection0;
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##6"))
                    {
                        gptEcs->remove_component(ptLibrary, tEnvironmentProbeComponentType, *ptSelectedEntity);
                        ptProbeComp = NULL;
                    }
                }

                if(ptTransformComp)
                {
                    if(gptUI->collapsing_header("Transform", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                        gptUI->text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                        gptUI->text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                        gptUI->vertical_spacing();
                        gptUI->text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                        gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                        gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                        gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##7"))
                    {
                        gptEcs->remove_component(ptLibrary, tTransformComponentType, *ptSelectedEntity);
                        ptTransformComp = NULL;
                    }
                }

                if(ptForceField)
                {
                    if(gptUI->collapsing_header("Force Field", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_POINT", &ptForceField->tType, PL_FORCE_FIELD_TYPE_POINT);
                        gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_PLANE", &ptForceField->tType, PL_FORCE_FIELD_TYPE_PLANE);
                        gptUI->input_float("Gravity", &ptForceField->fGravity, NULL, 0);
                        gptUI->input_float("Range", &ptForceField->fRange, NULL, 0);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##8"))
                    {
                        gptEcs->remove_component(ptLibrary, tForceFieldComponentType, *ptSelectedEntity);
                        ptForceField = NULL;
                    }
                }

                if(ptObjectComp)
                {
                    if(gptUI->collapsing_header("Object", 0))
                    {
                        gptUI->stash_layout();
                        plTagComponent* ptTransformTagComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptObjectComp->tTransform);

                        plMesh* ptMesh = gptAsset->get_data(ptObjectComp->tMesh);
                        gptUI->text("Mesh Asset:       %s", gptAsset->get_path(ptObjectComp->tMesh));
                        gptUI->text("First Submesh:    %u", ptObjectComp->uFirstSubmesh);
                        gptUI->text("Submeshes:       %u", ptObjectComp->uSubmeshCount);
                        uint32_t uSplitCount = 0;
                        if(pl__ecs_tools_split_object(ptLibrary, *ptSelectedEntity, NULL, &uSplitCount))
                            bSplitObject = gptUI->button("Split into objects");
                        gptUI->text("Transform Entity: %s, %u", ptTransformTagComp->pcName, ptObjectComp->tTransform.uIndex);

                        if(gptUI->checkbox_flags("Renderable", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_RENDERABLE)) ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DIRTY;
                        if(gptUI->checkbox_flags("Cast Shadow", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_CAST_SHADOW)) ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DIRTY;
                        if(gptUI->checkbox_flags("Receive Shadow", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_RECEIVE_SHADOW)) ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DIRTY;
                        if(gptUI->checkbox_flags("Outline", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_OUTLINE)) ptObjectComp->tFlags |= PL_OBJECT_FLAGS_DIRTY;

                        if(ptObjectComp->atMaterialOverrides)
                        {
                            

                            gptUI->separator_text("Material Overrides");

                            gptUI->layout_dynamic(100.0f, 1);
                            if(gptUI->begin_child("Overrides", 0, 0))
                            {
                                gptUI->layout_dynamic(0.0f, 1);
                                for(uint32_t uMaterialOverride = 0; uMaterialOverride < ptObjectComp->uSubmeshCount; uMaterialOverride++)
                                {
                                    gptUI->text("%s", gptAsset->get_path(ptObjectComp->atMaterialOverrides[uMaterialOverride]));
                                }
                                gptUI->end_child();
                            }
                            // gptUI->text("Renderer: %s", gptAsset->get_path(ptRenderer->tRenderer));
                        }

                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##9"))
                    {
                        gptEcs->remove_component(ptLibrary, tObjectComponentType, *ptSelectedEntity);
                        ptObjectComp = NULL;
                    }
                }

                if(ptHierarchyComp)
                {
                    if(gptUI->collapsing_header("Hierarchy", 0))
                    {
                        gptUI->stash_layout();
                        plTagComponent* ptParentTagComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptHierarchyComp->tParent);
                        gptUI->text("Parent Entity: %s , %u", ptParentTagComp->pcName, ptHierarchyComp->tParent.uIndex);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##10"))
                    {
                        gptEcs->remove_component(ptLibrary, tHierarchyComponentType, *ptSelectedEntity);
                        ptHierarchyComp = NULL;
                    }
                }

                if(ptLightComp)
                {
                    if(gptUI->collapsing_header("Light", 0))
                    {
                        gptUI->stash_layout();
                        static const char* apcLightTypes[] = {
                            "PL_LIGHT_TYPE_DIRECTIONAL",
                            "PL_LIGHT_TYPE_POINT",
                            "PL_LIGHT_TYPE_SPOT",
                        };
                        gptUI->labeled_text("Type", "%s", apcLightTypes[ptLightComp->tType]);

                        gptUI->checkbox_flags("Visualizer", &ptLightComp->tFlags, PL_LIGHT_FLAG_VISUALIZER);

                        gptUI->input_float3("Position", ptLightComp->tPosition.d, NULL, 0);

                        gptUI->separator_text("Color");
                        gptUI->slider_float("r", &ptLightComp->tColor.x, 0.0f, 1.0f, 0);
                        gptUI->slider_float("g", &ptLightComp->tColor.y, 0.0f, 1.0f, 0);
                        gptUI->slider_float("b", &ptLightComp->tColor.z, 0.0f, 1.0f, 0);

                        gptUI->slider_float("Intensity", &ptLightComp->fIntensity, 0.0f, 20.0f, 0);

                        if(ptLightComp->tType != PL_LIGHT_TYPE_DIRECTIONAL)
                        {
                            gptUI->input_float("Radius", &ptLightComp->fRadius, NULL, 0);
                            gptUI->input_float("Range", &ptLightComp->fRange, NULL, 0);
                        }

                        if(ptLightComp->tType == PL_LIGHT_TYPE_SPOT)
                        {
                            gptUI->slider_float("Inner Cone Angle", &ptLightComp->fInnerConeAngle, 0.0f, PL_PI_2, 0);
                            gptUI->slider_float("Outer Cone Angle", &ptLightComp->fOuterConeAngle, 0.0f, PL_PI_2, 0);
                        }


                        if(ptLightComp->tType != PL_LIGHT_TYPE_POINT)
                        {
                            gptUI->separator_text("Direction");
                            gptUI->slider_float("x", &ptLightComp->tDirection.x, -1.0f, 1.0f, 0);
                            gptUI->slider_float("y", &ptLightComp->tDirection.y, -1.0f, 1.0f, 0);
                            gptUI->slider_float("z", &ptLightComp->tDirection.z, -1.0f, 1.0f, 0);
                        }

                        gptUI->separator_text("Shadows");

                        gptUI->checkbox_flags("Cast Shadow", &ptLightComp->tFlags, PL_LIGHT_FLAG_CAST_SHADOW);
                        bool bContactShadows = !(ptLightComp->tFlags & PL_LIGHT_FLAG_NO_CONTACT_SHADOWS);
                        if(gptUI->checkbox("Contact Shadows", &bContactShadows))
                        {
                            if(bContactShadows) ptLightComp->tFlags &= ~PL_LIGHT_FLAG_NO_CONTACT_SHADOWS;
                            else ptLightComp->tFlags |= PL_LIGHT_FLAG_NO_CONTACT_SHADOWS;
                        }

                        if(ptLightComp->tFlags & PL_LIGHT_FLAG_CAST_SHADOW)
                        {
                            uint32_t auResolutions[] = {
                                128,
                                256,
                                512,
                                1024,
                                2048,
                                4096,
                            };
                            int iSelection = 0;
                            if(ptLightComp->uShadowResolution == 128)       iSelection = 0;
                            else if(ptLightComp->uShadowResolution == 256)  iSelection = 1;
                            else if(ptLightComp->uShadowResolution == 512)  iSelection = 2;
                            else if(ptLightComp->uShadowResolution == 1024) iSelection = 3;
                            else if(ptLightComp->uShadowResolution == 2048) iSelection = 4;
                            else if(ptLightComp->uShadowResolution == 4096) iSelection = 5;
                            gptUI->radio_button("Resolution: 128", &iSelection, 0);
                            gptUI->radio_button("Resolution: 256", &iSelection, 1);
                            gptUI->radio_button("Resolution: 512", &iSelection, 2);
                            gptUI->radio_button("Resolution: 1024", &iSelection, 3);
                            gptUI->radio_button("Resolution: 2048", &iSelection, 4);
                            gptUI->radio_button("Resolution: 4096", &iSelection, 5);
                            ptLightComp->uShadowResolution = auResolutions[iSelection];
                        }
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##11"))
                    {
                        gptEcs->remove_component(ptLibrary, tLightComponentType, *ptSelectedEntity);
                        ptLightComp = NULL;
                    }
                }

                if(ptSkinComp)
                {
                    if(gptUI->collapsing_header("Skin", 0))
                    {
                        gptUI->stash_layout();
                        if(gptUI->tree_node("Joints", 0))
                        {
                            plSkin* ptSkin = gptAsset->get_data(ptSkinComp->tSkin);
                            for(uint32_t i = 0; i < ptSkin->uJointCount; i++)
                            {
                                plTagComponent* ptJointTagComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptSkinComp->_atJoints[i]);
                                gptUI->text("%s", ptJointTagComp->pcName);  
                            }
                            gptUI->tree_pop();
                        }
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##12"))
                    {
                        gptEcs->remove_component(ptLibrary, tSkinComponentType, *ptSelectedEntity);
                        ptSkinComp = NULL;
                    }
                }

                if(ptCameraComp)
                { 
                    if(gptUI->collapsing_header("Camera", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->labeled_text("Near Z", "%+0.3f", ptCameraComp->fNearZ);
                        gptUI->labeled_text("Far Z", "%+0.3f", ptCameraComp->fFarZ);
                        if(gptUI->radio_button("Projection: Perspective", &ptCameraComp->eProjectionType, PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE))
                        {
                            ptCameraComp->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_ALL;
                        }
                        if(gptUI->radio_button("Projection: Orthographic", &ptCameraComp->eProjectionType, PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC))
                        {
                            ptCameraComp->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_ALL;
                        }
                        if(gptUI->radio_button("Depth Mode: Standard", &ptCameraComp->eDepthMode, PL_CAMERA_DEPTH_MODE_STANDARD))
                        {
                            gptCamera->set_depth_mode(ptCameraComp, PL_CAMERA_DEPTH_MODE_STANDARD);
                            ptCameraComp->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_ALL;
                            
                        }
                        if(gptUI->radio_button("Depth Mode: Reverse Z", &ptCameraComp->eDepthMode, PL_CAMERA_DEPTH_MODE_REVERSE_Z))
                        {
                            gptCamera->set_depth_mode(ptCameraComp, PL_CAMERA_DEPTH_MODE_REVERSE_Z);
                            ptCameraComp->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_ALL;
                        }
                        gptUI->labeled_text("Aspect Ratio", "%+0.3f", ptCameraComp->fAspectRatio);
                        gptUI->labeled_text("Pitch", "%+0.3f", ptCameraComp->fPitch);
                        gptUI->labeled_text("Yaw", "%+0.3f", ptCameraComp->fYaw);
                        gptUI->labeled_text("Roll", "%+0.3f", ptCameraComp->fRoll);
                        gptUI->labeled_text("Position", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tPositionF.x, ptCameraComp->tPositionF.y, ptCameraComp->tPositionF.z);
                        gptUI->labeled_text("Up", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tUpVec.x, ptCameraComp->tUpVec.y, ptCameraComp->tUpVec.z);
                        gptUI->labeled_text("Forward", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tForwardVec.x, ptCameraComp->tForwardVec.y, ptCameraComp->tForwardVec.z);
                        gptUI->labeled_text("Right", "(%+0.3f, %+0.3f, %+0.3f)", ptCameraComp->tRightVec.x, ptCameraComp->tRightVec.y, ptCameraComp->tRightVec.z);

                        if(ptCameraComp->eProjectionType == PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE)
                        {
                            gptUI->labeled_text("Vertical Field of View", "%+0.3f", ptCameraComp->fYFov);
                            gptUI->labeled_text("Aspect Ratio", "%+0.3f", ptCameraComp->fAspectRatio);
                        }
                        else
                        {
                            gptUI->input_float("Width", &ptCameraComp->fWidth, NULL, 0);
                            gptUI->input_float("Height", &ptCameraComp->fHeight, NULL, 0);
                        }

                        gptUI->input_float3("Position", ptCameraComp->tPositionF.d, NULL, 0);
                        gptUI->input_float("Near Z Plane", &ptCameraComp->fNearZ, NULL, 0);
                        gptUI->input_float("Far Z Plane", &ptCameraComp->fFarZ, NULL, 0);

                        ptCameraComp->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_ALL;
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##13"))
                    {
                        gptEcs->remove_component(ptLibrary, tCameraComponentType, *ptSelectedEntity);
                        ptCameraComp = NULL;
                    }
                }

                if(ptAnimationComp)
                { 
                    if(gptUI->collapsing_header("Animation", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox_flags("Playing", &ptAnimationComp->tFlags, PL_ANIMATION_FLAG_PLAYING);
                        gptUI->checkbox_flags("Looped", &ptAnimationComp->tFlags, PL_ANIMATION_FLAG_LOOPED);
                        plAnimation* ptAnimation = gptAsset->get_data(ptAnimationComp->tAnimation);
                        gptUI->labeled_text("Start", "%0.3f s", ptAnimation->fStart);
                        gptUI->labeled_text("End", "%0.3f s", ptAnimation->fEnd);
                        // gptUI->labeled_text("Speed", "%0.3f s", ptAnimationComp->fSpeed);
                        gptUI->slider_float("Speed", &ptAnimationComp->fSpeed, 0.0f, 2.0f, 0);
                        gptUI->slider_float("Time", &ptAnimationComp->fTimer, ptAnimation->fStart, ptAnimation->fEnd, 0);
                        gptUI->progress_bar(ptAnimationComp->fTimer / (ptAnimation->fEnd - ptAnimation->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##14"))
                    {
                        gptEcs->remove_component(ptLibrary, tAnimationComponentType, *ptSelectedEntity);
                        ptAnimationComp = NULL;
                    }
                }

                if(ptIKComp)
                { 
                    if(gptUI->collapsing_header("Inverse Kinematics", 0))
                    {
                        gptUI->stash_layout();
                        plTagComponent* ptTargetComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptIKComp->tTarget);
                        gptUI->text("Target Entity: %s , %u", ptTargetComp->pcName, ptIKComp->tTarget.uIndex);
                        gptUI->slider_uint("Chain Length", &ptIKComp->uChainLength, 1, 10, 0);
                        gptUI->slider_uint("Iterations", &ptIKComp->uIterationCount, 1, 10, 0);

                        gptUI->checkbox("Enabled", &ptIKComp->bEnabled);
                        gptUI->apply_layout();
                    }
                    if(gptUI->button(PL_ICON_FA_TRASH_CAN "##15"))
                    {
                        gptEcs->remove_component(ptLibrary, tInverseKinematicsComponentType, *ptSelectedEntity);
                        ptIKComp = NULL;
                    }
                }

                pl__ecs_tools_navigation(tAssetHandle, ptLibrary, *ptSelectedEntity, ptLibraryHelper);
                gptUI->pop_id();
            }
            
            gptUI->end_child();
        }

        gptUI->end_window();
    }

    // Defer structural edits until all inspector pointers and tree iterators are finished.
    if(bSplitObject)
    {
        uint32_t uCount = 0;
        if(pl__ecs_tools_split_object(ptLibrary, *ptSelectedEntity, NULL, &uCount))
        {
            plEntity* atPieces = PL_ALLOC(sizeof(plEntity) * uCount);
            if(pl__ecs_tools_split_object(ptLibrary, *ptSelectedEntity, atPieces, &uCount))
            {
                *ptSelectedEntity = atPieces[0];
                bResult = true;
            }
            PL_FREE(atPieces);
        }
    }
    return bResult;
}

void
pl_ecs_tools_initialize(void)
{
    gptConsole->add_toggle_variable("t.ECS", &gptEcsToolsCtx->bShowTool, "shows entity/component inspector", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
}

void
pl_ecs_tools_cleanup(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptEcsToolsCtx->sbtLibraryData); i++)
        gptUI->text_filter_cleanup(&gptEcsToolsCtx->sbtLibraryData[i].tFilter);
    pl_sb_free(gptEcsToolsCtx->sbtLibraryData);
    pl_sb_free(gptEcsToolsCtx->sbcBuffer);
    pl_hm_free(&gptEcsToolsCtx->tLibraryHashmap);
    bool bShow = gptEcsToolsCtx->bShowTool;
    *gptEcsToolsCtx = (plEcsToolsContext){.bShowTool = bShow};
}

void
pl_ecs_tools_open(plAssetHandle tLibrary)
{
    if(pl__ecs_tools_library(tLibrary))
    {
        gptEcsToolsCtx->tLibraryAsset = tLibrary;
        gptEcsToolsCtx->bShowTool = true;
    }
}

void
pl_ecs_tools_select_entity(plAssetHandle tLibrary, plEntity tEntity)
{
    plLibraryToolHelper* ptHelper = pl__ecs_tools_library(tLibrary);
    if(!ptHelper)
        return;
    if(!gptEcs->is_entity_valid(gptAsset->get_data(tLibrary), tEntity))
        tEntity.uData = UINT64_MAX;
    if(ptHelper->tSelectedEntity.uData != tEntity.uData)
    {
        ptHelper->tSelectedEntity = tEntity;
        ptHelper->uComponentFilter = 0;
        ptHelper->tFilter.acInputBuffer[0] = 0;
        gptUI->text_filter_build(&ptHelper->tFilter);
    }
}

plEntity
pl_ecs_tools_get_selected_entity(plAssetHandle tLibrary)
{
    plLibraryToolHelper* ptHelper = pl__ecs_tools_library(tLibrary);
    if(!ptHelper)
        return (plEntity){.uData = UINT64_MAX};
    if(!gptEcs->is_entity_valid(gptAsset->get_data(tLibrary), ptHelper->tSelectedEntity))
        ptHelper->tSelectedEntity.uData = UINT64_MAX;
    return ptHelper->tSelectedEntity;
}

plEntity
pl_ecs_tools_get_transform_entity(const plComponentLibrary* ptLibrary, plEntity tEntity)
{
    if(gptEcs->has_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity))
        return tEntity;
    const plObjectComponent* ptObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), tEntity);
    if(ptObject && gptEcs->has_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptObject->tTransform))
        return ptObject->tTransform;
    return (plEntity){.uData = UINT64_MAX};
}

void
pl_ecs_tools_run(void)
{
    if(!gptEcsToolsCtx->bShowTool)
        return;
    plLibraryToolHelper* ptHelper = pl__ecs_tools_library(gptEcsToolsCtx->tLibraryAsset);
    if(!ptHelper)
    {
        if(gptUI->begin_window("ECS", &gptEcsToolsCtx->bShowTool, 0))
        {
            gptUI->layout_dynamic(0, 1);
            gptUI->text("Load a scene or choose Inspect entities in Assets.");
            gptUI->end_window();
        }
        return;
    }
    const plAssetTypeDesc* atTypes = NULL;
    uint32_t uCount = gptAsset->get_type_descriptions(&atTypes);
    gptEcsToolsCtx->bNavigationComponents = pl__ecs_tools_navigation_available();
    gptEcsToolsCtx->bVoxelAssets = gptVoxel->get_asset_type_key() < uCount && strcmp(atTypes[gptVoxel->get_asset_type_key()].pcName, "Voxel") == 0;
    pl_ecs_tools_get_selected_entity(gptEcsToolsCtx->tLibraryAsset);
    pl__ecs_tools_show_window(gptEcsToolsCtx->tLibraryAsset, &ptHelper->tSelectedEntity, &gptEcsToolsCtx->bShowTool);
    pl__ecs_tools_apply_navigation();
}

//-----------------------------------------------------------------------------
// [SECTION] viewport helpers
//-----------------------------------------------------------------------------

static bool
pl__ecs_tools_marker(const plComponentLibrary* ptLibrary, plEntity tEntity)
{
    const plObjectComponent* ptObject = gptEcs->get_component(ptLibrary, gptRenderer->get_ecs_type_key_object(), tEntity);
    return !ptObject || !(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE) || !gptAsset->is_valid(ptObject->tMesh);
}

static bool
pl__ecs_tools_project(const plCamera* ptCamera, plVec3 tPosition, plVec2 tSize, plVec2* ptScreen, float* pfSize)
{
    if(!ptCamera || tSize.x <= 0 || tSize.y <= 0)
        return false;
    plVec4 tClip = pl_mul_mat4_vec4(&ptCamera->tViewProjMat, (plVec4){tPosition.x, tPosition.y, tPosition.z, 1});
    if(tClip.w <= 0 || tClip.z < 0 || tClip.z > tClip.w || fabsf(tClip.x) > tClip.w || fabsf(tClip.y) > tClip.w)
        return false;
    *ptScreen = (plVec2){(tClip.x / tClip.w + 1) * 0.5f * tSize.x, (tClip.y / tClip.w + 1) * 0.5f * tSize.y};
    *pfSize = 12.0f * tClip.w / (fabsf(ptCamera->tProjMat.x22) * tSize.y);
    return true;
}

void
pl_ecs_tools_draw_transforms(const plComponentLibrary* ptLibrary, plEntity tSelected, const plCamera* ptCamera, plVec2 tSize, plDrawList3D* ptDrawlist)
{
    if(!ptLibrary || !ptCamera || !ptDrawlist)
        return;
    tSelected = pl_ecs_tools_get_transform_entity(ptLibrary, tSelected);
    const plTransformComponent* atTransforms = NULL;
    const plEntity* atEntities = NULL;
    uint32_t uCount = gptEcs->get_components(ptLibrary, gptTransform->get_ecs_type_key_transform(), (void**)&atTransforms, &atEntities);
    for(uint32_t i = 0; i < uCount; i++)
    {
        bool bSelected = atEntities[i].uData == tSelected.uData;
        if(!bSelected && !pl__ecs_tools_marker(ptLibrary, atEntities[i]))
            continue;
        plVec3 tPosition = atTransforms[i].tWorld.col[3].xyz;
        plVec2 tScreen;
        float fSize;
        if(!pl__ecs_tools_project(ptCamera, tPosition, tSize, &tScreen, &fSize))
            continue;
        if(bSelected) fSize *= 1.5f;
        plVec3 tRight = pl_mul_vec3_scalarf(ptCamera->tRightVec, fSize);
        plVec3 tUp = pl_mul_vec3_scalarf(ptCamera->tUpVec, fSize);
        plVec3 atCorners[] = {pl_add_vec3(tPosition, tRight), pl_add_vec3(tPosition, tUp),
            pl_sub_vec3(tPosition, tRight), pl_sub_vec3(tPosition, tUp)};
        plDrawLineOptions tOptions = {.uColor = bSelected ? PL_COLOR_32_YELLOW : PL_COLOR_32_CYAN, .fThickness = fSize * 0.15f};
        for(uint32_t j = 0; j < 4; j++)
            gptDraw->add_3d_line(ptDrawlist, atCorners[j], atCorners[(j + 1) % 4], tOptions);
    }
}

bool
pl_ecs_tools_pick_transform(const plComponentLibrary* ptLibrary, const plCamera* ptCamera, plVec2 tOffset, plVec2 tSize, plVec2 tMouse, float fRadius, plEntity* ptEntity)
{
    if(!ptLibrary || !ptCamera || !ptEntity || fRadius <= 0)
        return false;
    tMouse = pl_sub_vec2(tMouse, tOffset);
    if(tMouse.x < 0 || tMouse.y < 0 || tMouse.x >= tSize.x || tMouse.y >= tSize.y)
        return false;
    const plTransformComponent* atTransforms = NULL;
    const plEntity* atEntities = NULL;
    uint32_t uCount = gptEcs->get_components(ptLibrary, gptTransform->get_ecs_type_key_transform(), (void**)&atTransforms, &atEntities);
    float fBestDistance = fRadius * fRadius;
    float fBestDepth = FLT_MAX;
    bool bFound = false;
    for(uint32_t i = 0; i < uCount; i++)
    {
        if(!pl__ecs_tools_marker(ptLibrary, atEntities[i]))
            continue;
        plVec3 tPosition = atTransforms[i].tWorld.col[3].xyz;
        plVec2 tScreen;
        float fSize;
        if(!pl__ecs_tools_project(ptCamera, tPosition, tSize, &tScreen, &fSize))
            continue;
        plVec2 tDelta = pl_sub_vec2(tScreen, tMouse);
        float fDistance = tDelta.x * tDelta.x + tDelta.y * tDelta.y;
        float fDepth = pl_mul_mat4_vec4(&ptCamera->tViewMat, (plVec4){tPosition.x, tPosition.y, tPosition.z, 1}).z;
        if(fDistance < fBestDistance || (fDistance == fBestDistance && fDepth < fBestDepth))
        {
            fBestDistance = fDistance;
            fBestDepth = fDepth;
            *ptEntity = atEntities[i];
            bFound = true;
        }
    }
    return bFound;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ecs_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plEcsToolsI tApi = {
        .initialize  = pl_ecs_tools_initialize,
        .cleanup     = pl_ecs_tools_cleanup,
        .run         = pl_ecs_tools_run,
        .open        = pl_ecs_tools_open,
        .select_entity = pl_ecs_tools_select_entity,
        .get_selected_entity = pl_ecs_tools_get_selected_entity,
        .get_transform_entity = pl_ecs_tools_get_transform_entity,
        .draw_transforms = pl_ecs_tools_draw_transforms,
        .pick_transform = pl_ecs_tools_pick_transform
    };
    pl_set_api(ptApiRegistry, plEcsToolsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptDraw           = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptMemory         = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptRenderer       = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptUI             = pl_get_api_latest(ptApiRegistry, plUiI);
        gptEcs            = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptPhysics        = pl_get_api_latest(ptApiRegistry, plPhysicsI);
        gptCamera         = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptCameraEcs      = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
        gptAnimation      = pl_get_api_latest(ptApiRegistry, plAnimationI);
        gptMesh           = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptMaterial       = pl_get_api_latest(ptApiRegistry, plMaterialI);
        gptScript         = pl_get_api_latest(ptApiRegistry, plScriptI);
        gptResource       = pl_get_api_latest(ptApiRegistry, plResourceI);
        gptAsset          = pl_get_api_latest(ptApiRegistry, plAssetI);
        gptTransform      = pl_get_api_latest(ptApiRegistry, plTransformI);
        gptIk             = pl_get_api_latest(ptApiRegistry, plIkI);
        gptSkeleton       = pl_get_api_latest(ptApiRegistry, plSkeletonI);
        gptTexture        = pl_get_api_latest(ptApiRegistry, plTextureI);
        gptTerrain        = pl_get_api_latest(ptApiRegistry, plTerrainI);
        gptGfx            = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptConsole        = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptIOI            = pl_get_api_latest(ptApiRegistry, plIOI);
        gptVoxel          = pl_get_api_latest(ptApiRegistry, plVoxelI);
        gptPath           = pl_get_api_latest(ptApiRegistry, plPathI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptEcsToolsCtx = ptDataRegistry->get_data("plEcsToolsContext");
    }
    else
    {
        static plEcsToolsContext gtEcsToolsCtx = {0};
        gptEcsToolsCtx = &gtEcsToolsCtx;
        ptDataRegistry->set_data("plEcsToolsContext", gptEcsToolsCtx);
    }
}

void
pl_unload_ecs_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plEcsToolsI* ptApi = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif
