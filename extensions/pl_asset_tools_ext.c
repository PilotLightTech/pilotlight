/*
   pl_asset_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] public api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include "pl.h"
#include "pl_asset_tools_ext.h"
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

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

static const plMemoryI*   gptMemory = NULL;
static const plEcsI*      gptEcs      = NULL;
static const plAnimationI*  gptAnimation = NULL;
static const plUiI*       gptUI       = NULL;
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
};

static const char* apcAssetIcons[] = {
    "None",
    PL_ICON_FA_PALETTE,
    PL_ICON_FA_CHESS_BOARD,
    PL_ICON_FA_DRAW_POLYGON,
    PL_ICON_FA_FOLDER_OPEN,
    PL_ICON_FA_FILM,
    PL_ICON_FA_CLOUD_SUN,
    PL_ICON_FA_GLOBE,
    PL_ICON_FA_PLAY,
    PL_ICON_FA_USER_INJURED,
    PL_ICON_FA_BONE,
};

static const char* apcAssetNames[] = {
    "None",
    PL_ICON_FA_PALETTE " material",
    PL_ICON_FA_CHESS_BOARD " texture",
    PL_ICON_FA_DRAW_POLYGON " mesh",
    PL_ICON_FA_FOLDER_OPEN " library",
    PL_ICON_FA_FILM " renderer",
    PL_ICON_FA_CLOUD_SUN " environment",
    PL_ICON_FA_GLOBE " terrain",
    PL_ICON_FA_PLAY " animation",
    PL_ICON_FA_USER_INJURED " skin",
    PL_ICON_FA_BONE " skeleton",
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plLibraryToolHelper
{
    plUiTextFilter tFilter;
    uint32_t uComponentFilter;
} plLibraryToolHelper;

typedef struct _plAssetToolSelectData
{
    bool bSelected;
    plEntity tSelectedEntity;
} plAssetToolSelectData;

typedef struct _plEcsToolsContext
{
    char* sbcBuffer;
    bool  bShowTool;

    // asset data
    plUiTextFilter tAssetFilter;

    plHashMap tLibraryHashmap;
    plLibraryToolHelper* sbtLibraryData;

    plAssetToolSelectData* sbtSelectionData;
    uint32_t* sbuSelectionData;

} plEcsToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plEcsToolsContext* gptAssetToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static bool
pl__asset_tools_blah(plComponentLibrary* ptLibrary, plEntity* ptSelectedEntity, plEntity tEntity, const char* pcName)
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


    pl_sb_reset(gptAssetToolsCtx->sbcBuffer);
    if(ptHierarchyComp) { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_SITEMAP);                    pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptTransformComp) { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT);  pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptObjectComp)    { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_GHOST);                      pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptSkinComp)      { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_USER_INJURED);               pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptCameraComp)    { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CAMERA);                     pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptAnimationComp) { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_PLAY);                       pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptIKComp)        { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_DRAW_POLYGON);               pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptLightComp)     { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_LIGHTBULB);                  pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptProbeComp)     { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_MAP_PIN);                    pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptHumanComp)     { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_PERSON);                     pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptScriptComp)    { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CODE);                       pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptRigidComp)     { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_BOXES_STACKED);              pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptForceField)    { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_WIND);                       pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptEnvironment)   { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_CLOUD_SUN);                  pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptRenderer)      { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_FILM);                       pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptLibraryComp)   { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_FOLDER_OPEN);                pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    if(ptTerrain)       { pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", PL_ICON_FA_GLOBE);                      pl_sb_pop(gptAssetToolsCtx->sbcBuffer); }
    pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, " %s", pcName);

    // gptUI->push_id_uint(i);
    if(gptUI->selectable(gptAssetToolsCtx->sbcBuffer, &bSelected, 0))
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
    // gptUI->pop_id();
    return bResult;
}

bool
pl_asset_tools_show_window(plAssetHandle tAssetHandle, plEntity* ptSelectedEntity, bool* pbShowWindow)
{

    plComponentLibrary* ptLibrary = gptAsset->get_data(tAssetHandle);
    const char* pcLibraryName = gptAsset->get_path(tAssetHandle);

    bool bResult = false;

    if(!pl_hm_has_key(&gptAssetToolsCtx->tLibraryHashmap, (uint64_t)ptLibrary))
    {
        uint64_t uFreeIndex = pl_hm_get_free_index(&gptAssetToolsCtx->tLibraryHashmap);
        if(uFreeIndex == PL_DS_HASH_INVALID)
        {
            uFreeIndex = pl_sb_size(gptAssetToolsCtx->sbtLibraryData);
            pl_sb_add(gptAssetToolsCtx->sbtLibraryData);
        }
        gptAssetToolsCtx->sbtLibraryData[uFreeIndex] = (plLibraryToolHelper){0};
        pl_hm_insert(&gptAssetToolsCtx->tLibraryHashmap, (uint64_t)ptLibrary, uFreeIndex);
    }
    plLibraryToolHelper* ptLibraryHelper = &gptAssetToolsCtx->sbtLibraryData[pl_hm_lookup(&gptAssetToolsCtx->tLibraryHashmap, (uint64_t)ptLibrary)];

    if(gptUI->begin_window(pcLibraryName, pbShowWindow, false))
    {
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
        };

        bool abCombo[PL_ARRAYSIZE(atComponentTypes)] = {0};
        abCombo[ptLibraryHelper->uComponentFilter] = true;
        if(gptUI->begin_combo(PL_ICON_FA_FILTER, apcComponentNames[ptLibraryHelper->uComponentFilter], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
        {
            for(uint32_t i = 0; i < PL_ARRAYSIZE(atComponentTypes); i++)
            {
                if(gptUI->selectable(apcComponentNames[i], &abCombo[i], 0))
                {
                    ptLibraryHelper->uComponentFilter = i;
                    gptUI->close_current_popup();
                } 
            }
            gptUI->end_combo();
        }

        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 105.0f, 2, pfRatios);

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
                        pl__asset_tools_blah(ptLibrary, ptSelectedEntity, ptEntities[i], ptTags[i].pcName);
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
                        pl__asset_tools_blah(ptLibrary, ptSelectedEntity, ptEntities[i], ptTags[i].pcName);
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

            if(ptSelectedEntity->uData != UINT64_MAX)
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

                static char acEntityIdBuffer[64] = {0};
                snprintf(acEntityIdBuffer, 64, "%" PRIu64, gptEcs->get_entity_id(ptLibrary, *ptSelectedEntity));
                gptUI->input_text("ID", acEntityIdBuffer, 64, PL_UI_INPUT_TEXT_FLAGS_READ_ONLY);
                gptUI->text("Entity: {i-%u, g-%u}", ptSelectedEntity->uIndex, ptSelectedEntity->uGeneration);
                
                gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatiosInner);

                if(ptTagComp && gptUI->begin_collapsing_header("Tag", 0))
                {
                    gptUI->text("Name: %s", ptTagComp->pcName);
                    gptUI->end_collapsing_header();
                }

                if(ptLibraryComp && gptUI->begin_collapsing_header("Library", 0))
                {
                    gptUI->text("Library: %s", gptAsset->get_path(ptLibraryComp->tSourceLibrary));
                    gptUI->end_collapsing_header();
                }

                if(ptEnvironment && gptUI->begin_collapsing_header("Environment", 0))
                {
                    gptUI->text("Environment: %s", gptAsset->get_path(ptEnvironment->tEnvironment));
                    
                    gptUI->end_collapsing_header();
                }

                if(ptRenderer && gptUI->begin_collapsing_header("Renderer", 0))
                {
                    gptUI->text("Renderer: %s", gptAsset->get_path(ptRenderer->tRenderer));
                    gptUI->end_collapsing_header();
                }

                if(ptTerrain && gptUI->begin_collapsing_header("Terrain", 0))
                {
                    gptUI->text("Terrain: %s", gptAsset->get_path(ptTerrain->tTerrain));
                    gptUI->slider_float("fTau", &ptTerrain->fTau, 0.0f, 1.0f, 0);

                    gptUI->checkbox_flags("Wireframe", &ptTerrain->tFlags, PL_TERRAIN_FLAGS_WIREFRAME);
                    gptUI->checkbox_flags("Show Levels", &ptTerrain->tFlags, PL_TERRAIN_FLAGS_SHOW_LEVELS);

                    gptUI->slider_float("fSlopeStart", &ptTerrain->fSlopeStart, 0.0f, 1.0f, 0);
                    gptUI->slider_float("fSlopeEnd", &ptTerrain->fSlopeEnd, 0.0f, 1.0f, 0);

                    gptUI->input_float("Terrain Depth Bias", &ptTerrain->fTerrainShadowConstantDepthBias, "%g", 0);
                    gptUI->input_float("Terrain Slope Depth Bias", &ptTerrain->fTerrainShadowSlopeDepthBias, "%g", 0);

                    gptUI->end_collapsing_header();
                }

                if(ptScriptComp && gptUI->begin_collapsing_header("Script", 0))
                {
                    gptUI->text("File: %s", ptScriptComp->pcPath);

                    gptUI->checkbox_flags("Playing", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_PLAYING);
                    gptUI->checkbox_flags("Play Once", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_PLAY_ONCE);
                    gptUI->checkbox_flags("Reloadable", &ptScriptComp->tFlags, PL_SCRIPT_FLAG_RELOADABLE);
                    gptUI->end_collapsing_header();
                }

                if(ptHumanComp && gptUI->begin_collapsing_header("Humanoid", 0))
                {
                    gptUI->end_collapsing_header();
                }

                if(ptRigidComp && gptUI->begin_collapsing_header("Rigid Body Physics", 0))
                {
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

                    gptUI->end_collapsing_header();
                }

                if(ptProbeComp && gptUI->begin_collapsing_header("Environment Probe", 0))
                {
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

                    gptUI->separator_text("Intervals");

                    int iSelection0 = (int)ptProbeComp->uInterval;
                    gptUI->radio_button("1", &iSelection0, 1);
                    gptUI->radio_button("2", &iSelection0, 2);
                    gptUI->radio_button("3", &iSelection0, 3);
                    gptUI->radio_button("4", &iSelection0, 4);
                    gptUI->radio_button("5", &iSelection0, 5);
                    gptUI->radio_button("6", &iSelection0, 6);

                    ptProbeComp->uInterval = (uint32_t)iSelection0;

                    gptUI->end_collapsing_header();
                }

                if(ptTransformComp && gptUI->begin_collapsing_header("Transform", 0))
                {
                    gptUI->text("Scale:       (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tScale.x, ptTransformComp->tScale.y, ptTransformComp->tScale.z);
                    gptUI->text("Translation: (%+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tTranslation.x, ptTransformComp->tTranslation.y, ptTransformComp->tTranslation.z);
                    gptUI->text("Rotation:    (%+0.3f, %+0.3f, %+0.3f, %+0.3f)", ptTransformComp->tRotation.x, ptTransformComp->tRotation.y, ptTransformComp->tRotation.z, ptTransformComp->tRotation.w);
                    gptUI->vertical_spacing();
                    gptUI->text("Local World: |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].x, ptTransformComp->tWorld.col[1].x, ptTransformComp->tWorld.col[2].x, ptTransformComp->tWorld.col[3].x);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].y, ptTransformComp->tWorld.col[1].y, ptTransformComp->tWorld.col[2].y, ptTransformComp->tWorld.col[3].y);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].z, ptTransformComp->tWorld.col[1].z, ptTransformComp->tWorld.col[2].z, ptTransformComp->tWorld.col[3].z);
                    gptUI->text("            |%+0.3f, %+0.3f, %+0.3f, %+0.3f|", ptTransformComp->tWorld.col[0].w, ptTransformComp->tWorld.col[1].w, ptTransformComp->tWorld.col[2].w, ptTransformComp->tWorld.col[3].w);
                    gptUI->end_collapsing_header();
                }


                if(ptForceField && gptUI->begin_collapsing_header("Force Field", 0))
                {
                    gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_POINT", &ptForceField->tType, PL_FORCE_FIELD_TYPE_POINT);
                    gptUI->radio_button("Type: PL_FORCE_FIELD_TYPE_PLANE", &ptForceField->tType, PL_FORCE_FIELD_TYPE_PLANE);
                    gptUI->input_float("Gravity", &ptForceField->fGravity, NULL, 0);
                    gptUI->input_float("Range", &ptForceField->fRange, NULL, 0);
                    gptUI->end_collapsing_header();
                }

                if(ptObjectComp && gptUI->begin_collapsing_header("Object", 0))
                {
                    plTagComponent* ptTransformTagComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptObjectComp->tTransform);

                    plMesh* ptMesh = gptAsset->get_data(ptObjectComp->tMesh);
                    gptUI->text("Mesh Asset:       %s", gptAsset->get_path(ptObjectComp->tMesh));
                    gptUI->text("Submeshes:       %u", ptMesh->uSubmeshCount);
                    gptUI->text("Transform Entity: %s, %u", ptTransformTagComp->pcName, ptObjectComp->tTransform.uIndex);

                    bool bObjectRenderable = ptObjectComp->tFlags & PL_OBJECT_FLAGS_RENDERABLE;
                    bool bObjectCastShadow = ptObjectComp->tFlags & PL_OBJECT_FLAGS_CAST_SHADOW;
                    bool bObjectDynamic = ptObjectComp->tFlags & PL_OBJECT_FLAGS_DYNAMIC;
                    bool bObjectForeground = ptObjectComp->tFlags & PL_OBJECT_FLAGS_FOREGROUND;
                    bool bObjectUpdateRequired = false;

                    if(gptUI->checkbox_flags("Renderable", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_RENDERABLE))
                        bObjectUpdateRequired = true;

                    if(gptUI->checkbox_flags("Cast Shadow", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_CAST_SHADOW))
                        bObjectUpdateRequired = true;

                    if(gptUI->checkbox_flags("Dynamic", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_DYNAMIC))
                        bObjectUpdateRequired = true;

                    if(gptUI->checkbox_flags("Foreground", &ptObjectComp->tFlags, PL_OBJECT_FLAGS_FOREGROUND))
                        bObjectUpdateRequired = true;
                    // if(bObjectUpdateRequired)
                    //     gptRenderer->update_scene_objects(ptScene, 1, ptSelectedEntity);
                    gptUI->end_collapsing_header();
                }

                if(ptHierarchyComp && gptUI->begin_collapsing_header("Hierarchy", 0))
                {
                    plTagComponent* ptParentTagComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptHierarchyComp->tParent);
                    gptUI->text("Parent Entity: %s , %u", ptParentTagComp->pcName, ptHierarchyComp->tParent.uIndex);
                    gptUI->end_collapsing_header();
                }

                if(ptLightComp && gptUI->begin_collapsing_header("Light", 0))
                {
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
                    gptUI->end_collapsing_header();
                }

                if(ptSkinComp && gptUI->begin_collapsing_header("Skin", 0))
                {
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
                    gptUI->end_collapsing_header();
                }

                if(ptCameraComp && gptUI->begin_collapsing_header("Camera", 0))
                { 
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
                    
                    
                    gptUI->end_collapsing_header();
                }

                if(ptAnimationComp && gptUI->begin_collapsing_header("Animation", 0))
                { 
                    gptUI->checkbox_flags("Playing", &ptAnimationComp->tFlags, PL_ANIMATION_FLAG_PLAYING);
                    gptUI->checkbox_flags("Looped", &ptAnimationComp->tFlags, PL_ANIMATION_FLAG_LOOPED);
                    plAnimation* ptAnimation = gptAsset->get_data(ptAnimationComp->tAnimation);
                    gptUI->labeled_text("Start", "%0.3f s", ptAnimation->fStart);
                    gptUI->labeled_text("End", "%0.3f s", ptAnimation->fEnd);
                    // gptUI->labeled_text("Speed", "%0.3f s", ptAnimationComp->fSpeed);
                    gptUI->slider_float("Speed", &ptAnimationComp->fSpeed, 0.0f, 2.0f, 0);
                    gptUI->slider_float("Time", &ptAnimationComp->fTimer, ptAnimation->fStart, ptAnimation->fEnd, 0);
                    gptUI->progress_bar(ptAnimationComp->fTimer / (ptAnimation->fEnd - ptAnimation->fStart), (plVec2){-1.0f, 0.0f}, NULL);
                    gptUI->end_collapsing_header();
                }

                if(ptIKComp && gptUI->begin_collapsing_header("Inverse Kinematics", 0))
                { 
                    plTagComponent* ptTargetComp = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), ptIKComp->tTarget);
                    gptUI->text("Target Entity: %s , %u", ptTargetComp->pcName, ptIKComp->tTarget.uIndex);
                    gptUI->slider_uint("Chain Length", &ptIKComp->uChainLength, 1, 5, 0);
                    gptUI->text("Iterations: %u", ptIKComp->uIterationCount);

                    gptUI->checkbox("Enabled", &ptIKComp->bEnabled);
                    gptUI->end_collapsing_header();
                }

                gptUI->pop_id();
            }
            
            gptUI->end_child();
        }

        gptUI->end_window();
    }

    return bResult;
}

bool
pl_asset_tools_show_assets(bool* bValue)
{
    bool bResult = false;
    const plAssetTypeDesc* ptAssetTypes = NULL;
    uint32_t uAssetTypeCount = gptAsset->get_type_descriptions(&ptAssetTypes);

    uint32_t uAssetCount = 0;
    const plAssetHandle* atAssetHandles = gptAsset->get_assets(&uAssetCount);
    pl_sb_resize(gptAssetToolsCtx->sbtSelectionData, uAssetCount);

    if(gptUI->begin_window("Assets", bValue, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();

        gptUI->layout_dynamic(0.0f, 2);

        if(gptUI->input_text_hint("Asset Filter", "Filter (inc,-exc)", gptAssetToolsCtx->tAssetFilter.acInputBuffer, 256, 0))
        {
            gptUI->text_filter_build(&gptAssetToolsCtx->tAssetFilter);
        }

        
        plAssetTypeKey atAssetTypesTypes[] = {
            INT32_MAX,
            gptMaterial->get_asset_type_key(),
            gptTexture->get_asset_type_key(),
            gptMesh->get_asset_type_key(),
            gptEcs->get_asset_type_key(),
            gptRenderer->get_asset_type_key_settings(),
            gptRenderer->get_asset_type_key_environment(),
            gptTerrain->get_asset_type_key(),
            gptAnimation->get_asset_type_key(),
            gptSkeleton->get_asset_type_key_skin(),
            gptSkeleton->get_asset_type_key_skeleton(),
        };

        static uint32_t uAssetFilter = 0;
        bool abCombo[PL_ARRAYSIZE(atAssetTypesTypes)] = {0};
        if(gptUI->begin_combo(PL_ICON_FA_FILTER, apcAssetNames[uAssetFilter], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
        {
            for(uint32_t i = 0; i < PL_ARRAYSIZE(atAssetTypesTypes); i++)
            {
                if(gptUI->selectable(apcAssetNames[i], &abCombo[i], 0))
                {
                    uAssetFilter = i;
                    gptUI->close_current_popup();
                } 
            }
            gptUI->end_combo();
        }

        gptUI->layout_dynamic(tWindowSize.y - 80.0f, 1);

        if(gptUI->begin_child("Assets Left", 0, 0))
        {
            gptUI->layout_dynamic(0.0f, 1);

            if(uAssetFilter != 0 || gptUI->text_filter_active(&gptAssetToolsCtx->tAssetFilter))
            {
                for(uint32_t i = 0; i < uAssetCount; i++)
                {
                    size_t szUnused = 0;
                    plAssetHandle tAssetHandle = atAssetHandles[i];
                    plAssetTypeKey tAssetType = gptAsset->get_type_key(tAssetHandle);
                    if(uAssetFilter != 0)
                    {
                        if(atAssetTypesTypes[uAssetFilter] != tAssetType)
                            continue;
                    }
                    const char* pcAssetPath = gptAsset->get_path(tAssetHandle);
                    if(gptUI->text_filter_pass(&gptAssetToolsCtx->tAssetFilter, pcAssetPath, NULL))
                    {
                        pl_sb_reset(gptAssetToolsCtx->sbcBuffer);
                        for(uint32_t j = 0; j < PL_ARRAYSIZE(atAssetTypesTypes); j++)
                        {
                            if(atAssetTypesTypes[j] == tAssetType)
                            {
                                pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s ", apcAssetIcons[j]);
                                pl_sb_pop(gptAssetToolsCtx->sbcBuffer);
                                break;
                            }
                        }

                        pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", pcAssetPath);
                        pl_sb_pop(gptAssetToolsCtx->sbcBuffer);
                        if(gptUI->selectable(gptAssetToolsCtx->sbcBuffer, &gptAssetToolsCtx->sbtSelectionData[i].bSelected, 0))
                        {
                            if(gptAssetToolsCtx->sbtSelectionData[i].bSelected)
                            {
                                pl_sb_push(gptAssetToolsCtx->sbuSelectionData, i);
                            }
                            else
                            {
                                for(uint32_t j = 0; j < pl_sb_size(gptAssetToolsCtx->sbuSelectionData); j++)
                                {
                                    if(gptAssetToolsCtx->sbuSelectionData[j] == i)
                                    {
                                        pl_sb_del_swap(gptAssetToolsCtx->sbuSelectionData, j);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                } 
            }
            else
            {
                plUiClipper tClipper = {uAssetCount};
                while(gptUI->step_clipper(&tClipper))
                {
                    for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                    {
                        size_t szUnused = 0;
                        plAssetHandle tAssetHandle = atAssetHandles[i];
                        plAssetTypeKey tAssetType = gptAsset->get_type_key(tAssetHandle);
                        const char* pcAssetPath = gptAsset->get_path(tAssetHandle);

                        pl_sb_reset(gptAssetToolsCtx->sbcBuffer);
                        for(uint32_t j = 0; j < PL_ARRAYSIZE(atAssetTypesTypes); j++)
                        {
                            if(atAssetTypesTypes[j] == tAssetType)
                            {
                                pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s ", apcAssetIcons[j]);
                                pl_sb_pop(gptAssetToolsCtx->sbcBuffer);
                                break;
                            }
                        }

                        pl_sb_sprintf(gptAssetToolsCtx->sbcBuffer, "%s", pcAssetPath);
                        pl_sb_pop(gptAssetToolsCtx->sbcBuffer);

                        if(gptUI->selectable(gptAssetToolsCtx->sbcBuffer, &gptAssetToolsCtx->sbtSelectionData[i].bSelected, 0))
                        {
                            if(gptAssetToolsCtx->sbtSelectionData[i].bSelected)
                            {
                                pl_sb_push(gptAssetToolsCtx->sbuSelectionData, i);
                            }
                            else
                            {
                                for(uint32_t j = 0; j < pl_sb_size(gptAssetToolsCtx->sbuSelectionData); j++)
                                {
                                    if(gptAssetToolsCtx->sbuSelectionData[j] == i)
                                    {
                                        pl_sb_del_swap(gptAssetToolsCtx->sbuSelectionData, j);
                                        break;
                                    }
                                }
                            }
                        }
                    } 
                }
            }
            gptUI->end_child();
        }
        gptUI->end_window();
    }

    // pl_sb_resize(gptAssetToolsCtx->sbtSelectionData, uAssetCount);
    uint32_t uSelectedCount = pl_sb_size(gptAssetToolsCtx->sbuSelectionData);
    for(uint32_t iSelectionIndex = 0; iSelectionIndex < uSelectedCount; iSelectionIndex++)
    {
        plAssetToolSelectData* ptSelectionData = &gptAssetToolsCtx->sbtSelectionData[gptAssetToolsCtx->sbuSelectionData[iSelectionIndex]];

        plAssetHandle tAssetHandle = atAssetHandles[gptAssetToolsCtx->sbuSelectionData[iSelectionIndex]];
        plAssetTypeKey tAssetType = gptAsset->get_type_key(tAssetHandle);

        if(!ptSelectionData->bSelected)
        {
            pl_sb_del_swap(gptAssetToolsCtx->sbuSelectionData, iSelectionIndex);
            uSelectedCount--;
            continue;
        }

        if(tAssetType != gptEcs->get_asset_type_key() && gptUI->begin_window(gptAsset->get_path(tAssetHandle), &ptSelectionData->bSelected, 0))
        {
            const plVec2 tWindowSize = gptUI->get_window_size();
            const plVec2 tWindowPos = gptUI->get_window_pos();

            gptUI->layout_dynamic(0.0f, 1);

            gptUI->text("Properties");
            gptUI->separator();

            gptUI->layout_dynamic(tWindowSize.y - 80.0f, 1);
            if(gptUI->begin_child("Assets Right", 0, 0))
            {
                gptUI->layout_dynamic(0.0f, 1);
                gptUI->labeled_text("Path", "%s", gptAsset->get_path(tAssetHandle));
                gptUI->labeled_text("Type", "%u", gptAsset->get_type_key(tAssetHandle));

                gptUI->layout_dynamic(0.0f, 2);
                if(gptUI->button("Load"))          gptAsset->load(gptAsset->get_path(tAssetHandle));
                if(gptUI->button("Save"))          gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_AUTO);
                if(gptUI->button("Save (text)"))   gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_TEXT);
                if(gptUI->button("Save (binary)")) gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_BINARY);
                gptUI->layout_dynamic(0.0f, 1);

                if(tAssetType == gptMaterial->get_asset_type_key())
                {
                    plMaterial* ptMaterial = gptAsset->get_data(tAssetHandle);

                    gptUI->separator_text("base");
                    gptUI->text("NOTE: edits don't work at the moment");
                    gptUI->labeled_text("material model", "PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS");

                    const char* apcAlphaMode[] = {
                        "PL_MATERIAL_ALPHA_MODE_OPAQUE",
                        "PL_MATERIAL_ALPHA_MODE_MASK",
                        "PL_MATERIAL_ALPHA_MODE_BLEND"
                    };

                    gptUI->labeled_text("alpha mode", apcAlphaMode[ptMaterial->eAlphaMode]);
                    gptUI->input_float("alpha cutoff", &ptMaterial->fAlphaCutoff, "%g", 0);
                    gptUI->checkbox_flags("double sided", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DOUBLE_SIDED);
                    gptUI->input_float4("base color", ptMaterial->tBaseColor.d, "%g", 0);
                    gptUI->input_float("metalness", &ptMaterial->fMetalness, "%g", 0);
                    gptUI->input_float("roughness", &ptMaterial->fRoughness, "%g", 0);
                    gptUI->input_float("normal map strength", &ptMaterial->fNormalMapStrength, "%g", 0);
                    gptUI->input_float("occlusion strength", &ptMaterial->fOcclusionStrength, "%g", 0);
                    gptUI->input_float3("emissive color", ptMaterial->tEmissiveColor.d, "%g", 0);
                    gptUI->input_float("emissive strength", &ptMaterial->fEmissiveStrength, "%g", 0);
                    gptUI->input_float("ior", &ptMaterial->fIor, "%g", 0);

                    gptUI->separator_text("Advanced Properties");

                    gptUI->layout_dynamic(0.0f, 2);

                    gptUI->checkbox_flags("clearcoat", &ptMaterial->eFlags, PL_MATERIAL_FLAG_CLEARCOAT);
                    gptUI->checkbox_flags("sheen", &ptMaterial->eFlags, PL_MATERIAL_FLAG_SHEEN);
                    gptUI->checkbox_flags("iridescence", &ptMaterial->eFlags, PL_MATERIAL_FLAG_IRIDESCENCE);
                    gptUI->checkbox_flags("anisotropy", &ptMaterial->eFlags, PL_MATERIAL_FLAG_ANISOTROPY);
                    gptUI->checkbox_flags("transmission", &ptMaterial->eFlags, PL_MATERIAL_FLAG_TRANSMISSION);
                    gptUI->checkbox_flags("volume", &ptMaterial->eFlags, PL_MATERIAL_FLAG_VOLUME);
                    gptUI->checkbox_flags("dispersion", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DISPERSION);
                    gptUI->checkbox_flags("diffuse transmission", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION);

                    gptUI->layout_dynamic(0.0f, 1);

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT)
                    {
                        if(gptUI->begin_collapsing_header("Clearcoat", 0))
                        {
                            gptUI->input_float("factor", &ptMaterial->tClearcoat.fFactor, "%g", 0);
                            gptUI->input_float("roughness", &ptMaterial->tClearcoat.fRoughness, "%g", 0);
                            gptUI->input_float("normal map strength", &ptMaterial->tClearcoat.fNormalMapStrength, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN)
                    {
                        if(gptUI->begin_collapsing_header("Sheen", 0))
                        {
                            gptUI->input_float3("color", ptMaterial->tSheen.tColor.d, "%g", 0);
                            gptUI->input_float("roughness", &ptMaterial->tSheen.fRoughness, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE)
                    {
                        if(gptUI->begin_collapsing_header("Iridescence", 0))
                        {
                            gptUI->input_float("factor", &ptMaterial->tIridescence.fFactor, "%g", 0);
                            gptUI->input_float("ior", &ptMaterial->tIridescence.fIor, "%g", 0);
                            gptUI->input_float("max. thickness", &ptMaterial->tIridescence.fThicknessMax, "%g", 0);
                            gptUI->input_float("min. thickness", &ptMaterial->tIridescence.fThicknessMin, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY)
                    {
                        if(gptUI->begin_collapsing_header("Anisotropy", 0))
                        {
                            gptUI->input_float("strength", &ptMaterial->tAnisotropy.fStrength, "%g", 0);
                            gptUI->input_float("rotation", &ptMaterial->tAnisotropy.fRotation, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION)
                    {
                        if(gptUI->begin_collapsing_header("Transmission", 0))
                        {
                            gptUI->input_float("factor", &ptMaterial->tTransmission.fFactor, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME)
                    {
                        if(gptUI->begin_collapsing_header("Volume", 0))
                        {
                            gptUI->input_float3("attenuation color", ptMaterial->tVolume.tAttenuationColor.d, "%g", 0);
                            gptUI->input_float("thickness", &ptMaterial->tVolume.fThickness, "%g", 0);
                            gptUI->input_float("attenuation distance", &ptMaterial->tVolume.fAttenuationDistance, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION)
                    {
                        if(gptUI->begin_collapsing_header("Dispersion", 0))
                        {
                            gptUI->input_float("dispersion", &ptMaterial->tDispersion.fDispersion, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
                    {
                        if(gptUI->begin_collapsing_header("Diffuse Transmission", 0))
                        {
                            gptUI->input_float3("color", ptMaterial->tDiffuseTransmission.tColor.d, "%g", 0);
                            gptUI->input_float("factor", &ptMaterial->tDiffuseTransmission.fFactor, "%g", 0);
                            gptUI->end_collapsing_header();
                        }
                    }

                    const char* apcTextureSlotNames[] = {
                        "base_color",
                        "normal",
                        "emissive",
                        "occlusion",
                        "metal_roughness",
                        "clearcoat",
                        "clearcoat_roughness",
                        "clearcoat_normal",
                        "sheen_color",
                        "sheen_roughness",
                        "iridescence",
                        "iridescence_thickness",
                        "anisotropy",
                        "transmission_color",
                        "transmission_thickness",
                        "diffuse_transmission",
                        "diffuse_transmission_color"
                    };

                    gptUI->separator_text("Textures");

                    for(uint32_t i = 0; i < PL_MATERIAL_TEXTURE_SLOT_COUNT; i++)
                    {
                        bool bTextureActive = gptAsset->is_valid(ptMaterial->atTextures[i].tTexture);
                        if(!bTextureActive)
                        {
                            bool bPlaceHolder = false;
                            if(gptUI->checkbox(apcTextureSlotNames[i], &bPlaceHolder))
                            {
                                ptMaterial->atTextures[i].tTexture = gptAsset->find("/assets/textures/default.pltexture");
                                ptMaterial->atTextures[i].uUVSet = 0;
                                ptMaterial->atTextures[i].tScale = (plVec2){1.0f, 1.0f};
                                ptMaterial->atTextures[i].tOffset = (plVec2){0.0f, 0.0f};
                                ptMaterial->atTextures[i].fRotation = 0.0f;
                            }
                        }

                        else
                        {
                            if(gptUI->tree_node(apcTextureSlotNames[i], 0))
                            {

                                if(gptUI->button("Select Texture"))
                                {
                                    gptUI->open_popup("Select Texture Popup", 0);
                                }

                                if(gptUI->button("Disable"))
                                {
                                    ptMaterial->atTextures[i].tTexture.uData = 0;
                                }

                                gptUI->labeled_text("Texture", "%s", gptAsset->get_path(ptMaterial->atTextures[i].tTexture));

                                if(gptUI->is_popup_open("Select Texture Popup"))
                                {
                                    // plVec2 tCurrentCursorPos = gptUI->get_cursor_pos();
                                    uint32_t uTypeAssetCount = 0;
                                    const plAssetHandle* atTypeAssetHandles = gptAsset->get_assets_by_type(gptTexture->get_asset_type_key(), &uTypeAssetCount);

                                    gptUI->set_next_window_pos((plVec2){tWindowPos.x + 0.10f * tWindowSize.x, tWindowPos.y + 0.10f * tWindowSize.y}, PL_UI_COND_ALWAYS);
                                    gptUI->set_next_window_size((plVec2){0.80f * tWindowSize.x, 0.80f * tWindowSize.y}, PL_UI_COND_ALWAYS);

                                    if(gptUI->begin_popup("Select Texture Popup", 0))
                                    {
                                        const plVec2 tPopupWindowSize = gptUI->get_window_size();

                                        gptUI->layout_dynamic(tPopupWindowSize.y - 80.0f, 1);

                                        if(gptUI->begin_child("Select Assets", 0, 0))
                                        {
                                            gptUI->layout_dynamic(0.0f, 1);

                                            plUiClipper tClipper = {uTypeAssetCount};
                                            while(gptUI->step_clipper(&tClipper))
                                            {
                                                for(uint32_t j = tClipper.uDisplayStart; j < tClipper.uDisplayEnd; j++)
                                                {
                                                    bool bPlaceholder = false;
                                                    if(gptUI->selectable(gptAsset->get_path(atTypeAssetHandles[j]), &bPlaceholder, 0))
                                                    {
                                                        ptMaterial->atTextures[i].tTexture = atTypeAssetHandles[j];
                                                        gptUI->close_current_popup();
                                                    }
                                                }
                                            }

                                            gptUI->end_child();
                                        }
                                        gptUI->end_popup();
                                    }
                                }

                                gptUI->slider_uint("UV Set", &ptMaterial->atTextures[i].uUVSet, 0, 1, 0);
                                gptUI->input_float2("Offset", ptMaterial->atTextures[i].tOffset.d, "%g", 0);
                                gptUI->input_float2("Scale", ptMaterial->atTextures[i].tScale.d, "%g", 0);
                                gptUI->slider_angle("Scale", &ptMaterial->atTextures[i].fRotation, -360.0f, 360.0f, NULL, 0);
                                gptUI->tree_pop();
                            }
                        }
                    }

                }
                else if(tAssetType == gptTexture->get_asset_type_key())
                {
                    plTextureAsset* ptTexture = gptAsset->get_data(tAssetHandle);
                    gptUI->labeled_text("source", ptTexture->pcSourceFile);
                    gptUI->labeled_text("format", gptGfx->get_format_as_string(ptTexture->eFormat));
                    gptUI->checkbox("generate mips", &ptTexture->bGenerateMips);
                    gptUI->checkbox("compress", &ptTexture->bCompress);
                    gptUI->checkbox("sRGB", &ptTexture->bSRGB);
                }
                else if(tAssetType == gptMesh->get_asset_type_key())
                {
                    plMesh* ptMesh = gptAsset->get_data(tAssetHandle);
                    gptUI->text("AABB Max: (%g, %g)", ptMesh->tAABB.tMax.x, ptMesh->tAABB.tMax.y);
                    gptUI->text("AABB Min: (%g, %g)", ptMesh->tAABB.tMin.x, ptMesh->tAABB.tMin.y);
                    static uint32_t uSelectedSubmesh = 0;
                    uSelectedSubmesh = pl_clampu(0, uSelectedSubmesh, ptMesh->uSubmeshCount - 1);
                    gptUI->slider_uint("Submesh", &uSelectedSubmesh, 0, ptMesh->uSubmeshCount - 1, 0);

                    const plSubmesh* ptSubmesh = &ptMesh->atSubmeshes[uSelectedSubmesh];
                    gptUI->labeled_text("Material", "%s", gptAsset->get_path(ptSubmesh->tMaterial));
                    gptUI->text("AABB Max:     (%g, %g)", ptSubmesh->tAABB.tMax.x, ptSubmesh->tAABB.tMax.y);
                    gptUI->text("AABB Min:     (%g, %g)", ptSubmesh->tAABB.tMin.x, ptSubmesh->tAABB.tMin.y);
                    gptUI->text("Vertex Count: %zu", ptSubmesh->szVertexCount);
                    gptUI->text("Index Count:  %zu", ptSubmesh->szIndexCount);

                    gptUI->separator_text("Vertex Attributes");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_NORMAL) gptUI->text("Normals");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TANGENT) gptUI->text("Tangents");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0) gptUI->text("Texture Coordinates");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_0) gptUI->text("Colors 0");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR_1) gptUI->text("Colors 1");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0) gptUI->text("Joints 0");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1) gptUI->text("Joints 1");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) gptUI->text("Weights 0");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) gptUI->text("Weights 1");
                }
                else if(tAssetType == gptRenderer->get_asset_type_key_settings())
                {
                    plRenderSettings* ptSettings = gptAsset->get_data(tAssetHandle);

                    if(gptUI->begin_collapsing_header("Lighting", 0))
                    {
                        gptUI->checkbox_flags("Image Based Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED);
                        gptUI->checkbox_flags("Punctual Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS);
                        gptUI->checkbox_flags("Normal Mapping", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING);
                        gptUI->checkbox_flags("No Shadows", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS);
                        gptUI->end_collapsing_header();
                    }

                    if(gptUI->begin_collapsing_header("Shadows", 0))
                    {
                        gptUI->checkbox_flags("MultiViewport Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT);
                        gptUI->checkbox_flags("PCF Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_PCF);
                        gptUI->input_float("Depth Bias", &ptSettings->tShadows.fConstantDepthBias, "%g", 0);
                        gptUI->input_float("Slope Depth Bias", &ptSettings->tShadows.fSlopeDepthBias, "%g", 0);
                        gptUI->input_float("Max Shadow Range", &ptSettings->tShadows.fMaxShadowRange, "%g", 0);

                        static int saiSkyLutRes[2] = {0};
                        static int saiTransmissionLutRes[2] = {0};
                        static int saiMultiscatterLutRes[2] = {0};
                        static int saiAerialLutRes[3] = {0};
                        if(saiSkyLutRes[0] == 0) // first run
                        {
                            saiSkyLutRes[0] = (int)ptSettings->tSky.tSkyLutResolution.x;
                            saiSkyLutRes[1] = (int)ptSettings->tSky.tSkyLutResolution.y;

                            saiTransmissionLutRes[0] = (int)ptSettings->tSky.tTransmissionLutResolution.x;
                            saiTransmissionLutRes[1] = (int)ptSettings->tSky.tTransmissionLutResolution.y;

                            saiMultiscatterLutRes[0] = (int)ptSettings->tSky.tMultiscatterLutResolution.x;
                            saiMultiscatterLutRes[1] = (int)ptSettings->tSky.tMultiscatterLutResolution.y;

                            saiAerialLutRes[0] = (int)ptSettings->tSky.tAerialLutResolution.x;
                            saiAerialLutRes[1] = (int)ptSettings->tSky.tAerialLutResolution.y;
                            saiAerialLutRes[2] = (int)ptSettings->tSky.tAerialLutResolution.z;
                        }
                        int iSunResolution = (int)ptSettings->tShadows.uShadowResolution;
                        gptUI->radio_button("Shadow Resolution: Low", &iSunResolution, 1024);
                        gptUI->radio_button("Shadow Resolution: Medium", &iSunResolution, 2048);
                        gptUI->radio_button("Shadow Resolution: High", &iSunResolution, 4096);
                        ptSettings->tShadows.uShadowResolution = (uint32_t)iSunResolution;
                        int iShadowCascadeCount = (int)ptSettings->tShadows.uShadowCascadeCount;
                        gptUI->slider_int("Cascades", &iShadowCascadeCount, 1, 4, 0);
                        ptSettings->tShadows.uShadowCascadeCount = (uint32_t)iShadowCascadeCount;

                        gptUI->input_int2("Sky LUT Res", saiSkyLutRes, 0);
                        gptUI->input_int2("Transmission LUT Res", saiTransmissionLutRes, 0);
                        gptUI->input_int2("Multiscatter LUT Res", saiMultiscatterLutRes, 0);
                        gptUI->input_int3("Aerial LUT Res", saiAerialLutRes, 0);

                        if(gptUI->button("Update LUTS"))
                        {
                            ptSettings->tSky.tSkyLutResolution.x = (float)saiSkyLutRes[0];
                            ptSettings->tSky.tSkyLutResolution.y = (float)saiSkyLutRes[1];
                            ptSettings->tSky.tTransmissionLutResolution.x = (float)saiTransmissionLutRes[0];
                            ptSettings->tSky.tTransmissionLutResolution.y = (float)saiTransmissionLutRes[1];
                            ptSettings->tSky.tMultiscatterLutResolution.x = (float)saiMultiscatterLutRes[0];
                            ptSettings->tSky.tMultiscatterLutResolution.y = (float)saiMultiscatterLutRes[1];
                            ptSettings->tSky.tAerialLutResolution.x = (float)saiAerialLutRes[0];
                            ptSettings->tSky.tAerialLutResolution.y = (float)saiAerialLutRes[1];
                            ptSettings->tSky.tAerialLutResolution.z = (float)saiAerialLutRes[2];
                        }
                        gptUI->end_collapsing_header();
                    }

                    if(gptUI->begin_collapsing_header("Post Process", 0))
                    {
                        static const char* apcTonemapText[] = {
                            "None",
                            "Simple",
                            "ACES Filmic (Narkowicz)",
                            "ACES Filmic (Hill)",
                            "ACES Filmic (Hill Exposure Boost)",
                            "Reinhard",
                            "Khronos PBR Neutral",
                        };
                        if(gptUI->begin_combo("Tonemapping", apcTonemapText[ptSettings->tTonemap.tMode], PL_UI_COMBO_FLAGS_NONE))
                        {
                            for(uint32_t i = 0; i < PL_ARRAYSIZE(apcTonemapText); i++)
                            {
                                bool bPlaceHolder = ptSettings->tTonemap.tMode == (int)i;
                                if(gptUI->selectable(apcTonemapText[i], &bPlaceHolder, 0))
                                {
                                    ptSettings->tTonemap.tMode = (int)i;
                                    gptUI->close_current_popup();
                                }
                            }
                            gptUI->end_combo();
                        }

                        gptUI->slider_float("Exposure", &ptSettings->tTonemap.fExposure, 0.0f, 3.0f, 0);
                        gptUI->slider_float("Brightness", &ptSettings->tTonemap.fBrightness, -1.0f, 1.0f, 0);
                        gptUI->slider_float("Contrast", &ptSettings->tTonemap.fContrast, 0.0f, 2.0f, 0);
                        gptUI->slider_float("Saturation", &ptSettings->tTonemap.fSaturation, 0.0f, 2.0f, 0);

                        gptUI->end_collapsing_header();
                    }

                    if(gptUI->begin_collapsing_header("Bloom", 0))
                    {
                        bool bBloomActive = ptSettings->tBloom.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                        gptUI->checkbox("Bloom", &bBloomActive);

                        if(bBloomActive)
                        {
                            gptUI->slider_float("Bloom Radius", &ptSettings->tBloom.fRadius, 0.0f, 10.0f, 0);
                            gptUI->slider_float("Bloom Strength", &ptSettings->tBloom.fStrength, 0.0f, 1.0f, 0);
                            int iBloomChainLength = (int)ptSettings->tBloom.uChainLength;
                            if(gptUI->slider_int("Bloom Chain", &iBloomChainLength, 2, 10, 0))
                                ptSettings->tBloom.uChainLength = (uint32_t)iBloomChainLength;
                            ptSettings->tBloom.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                        }
                        else
                            ptSettings->tBloom.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;
                        gptUI->end_collapsing_header();
                    }

                    if(gptUI->begin_collapsing_header("Fog", 0))
                    {
                        bool bFog = ptSettings->tFog.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
                        gptUI->checkbox("Fog", &bFog);
                        if(bFog)
                        {
                            ptSettings->tFog.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                            gptUI->radio_button("Linear Fog", &ptSettings->tFog.tMode, 0);
                            gptUI->radio_button("Exponential Fog", &ptSettings->tFog.tMode, 1);
                            gptUI->slider_float("Fog Start", &ptSettings->tFog.fStart, 0.0f, 100.0f, 0);
                            gptUI->slider_float("Fog End", &ptSettings->tFog.fCutOffDistance, 0.0f, 10000.0f, 0);
                            gptUI->input_float3("Fog Color", ptSettings->tFog.tColor.d, "%g", 0);
                            if(ptSettings->tFog.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                            {
                                gptUI->slider_float("Fog Max Opacity", &ptSettings->tFog.fMaxOpacity, 0.0f, 1.0f, 0);
                                gptUI->slider_float("Fog Density", &ptSettings->tFog.fDensity, 0.0f, 1.0f, 0);
                                gptUI->slider_float("Fog Height", &ptSettings->tFog.fHeight, -100.0f, 100.0f, 0);
                                gptUI->slider_float("Fog Height Falloff", &ptSettings->tFog.fHeightFalloff, 0.0f, 1.0f, 0);
                            }  
                        }
                        else
                            ptSettings->tFog.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;
                        gptUI->end_collapsing_header();
                    }

                }
                else if(tAssetType == gptRenderer->get_asset_type_key_environment())
                {
                    plRenderEnvironment* ptEnvironment = gptAsset->get_data(tAssetHandle);

                    gptUI->radio_button("Method: None", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_NONE);
                    gptUI->radio_button("Method: Skybox", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_SKYBOX);
                    gptUI->radio_button("Method: Realistic", &ptEnvironment->eMode, PL_RENDERER_SKY_MODE_REALISTIC);

                    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_SKYBOX)
                    {
                        gptUI->labeled_text("Skybox", "%s", gptAsset->get_path(ptEnvironment->tSkyboxTexture));
                    }

                    if(ptEnvironment->eMode != PL_RENDERER_SKY_MODE_NONE)
                    {
                        gptUI->input_float("Sun Intensity", &ptEnvironment->fSunIntensity, "%g", 0);
                        gptUI->input_float3("Sun Color", ptEnvironment->tSunColor.d, "%g", 0);
                        gptUI->input_float3("Sun Color", ptEnvironment->tSunColor.d, "%g", 0);

                        ptEnvironment->tSunDirection = pl_norm_vec3(ptEnvironment->tSunDirection);
                        float fSunPitch = asinf(pl_clampf(-1.0f, ptEnvironment->tSunDirection.y, 1.0f));
                        float fSunYaw   = atan2f(ptEnvironment->tSunDirection.x, ptEnvironment->tSunDirection.z);

                        bool bChanged = false;
                        bChanged |= gptUI->slider_angle("Sun Pitch", &fSunPitch, -89.9f, 89.9f, NULL, 0);
                        bChanged |= gptUI->slider_angle("Sun Yaw", &fSunYaw, -180.0f, 180.0f, NULL, 0);
                        if(bChanged)
                        {
                            const float fCosPitch = cosf(fSunPitch);
                            ptEnvironment->tSunDirection.x = fCosPitch * sinf(fSunYaw);
                            ptEnvironment->tSunDirection.y = sinf(fSunPitch);
                            ptEnvironment->tSunDirection.z = fCosPitch * cosf(fSunYaw);
                            ptEnvironment->tSunDirection = pl_norm_vec3(ptEnvironment->tSunDirection);
                        }

                        gptUI->checkbox_flags("Shadow Mapping", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_SHADOWS);

                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_SHADOWS)
                        {
                            gptUI->checkbox_flags("Debug Cascades", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_DEBUG_CASCADES);
                        }
                    }

                    if(ptEnvironment->eMode == PL_RENDERER_SKY_MODE_REALISTIC)
                    {
                        gptUI->checkbox_flags("Feature: Visualizer", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_SHOW_VISUALIZER);
                        gptUI->checkbox_flags("Feature: Multiscattering", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_MULTISCATTER);
                        gptUI->checkbox_flags("Feature: Aerial Perspective", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE);
                        
                        gptUI->input_float("Atmosphere Thickness", &ptEnvironment->fAtmosphereHeight, "%0.6f", 0);
                        gptUI->input_float("Atmosphere Conversion", &ptEnvironment->fAtmosphereConversion, "%0.6f", 0);
                        gptUI->input_float("Sun Radius", &ptEnvironment->fSunRadius, "%0.6f", 0);
                        gptUI->input_float("Planet Radius", &ptEnvironment->fPlanetRadius, "%0.6f", 0);
                        gptUI->input_float3("Rayleigh Scattering", ptEnvironment->tScatteringRayleighGround.d, "%0.6f", 0);
                        gptUI->input_float3("Rayleigh Absorption", ptEnvironment->tExtinctionRayleighGround.d, "%0.6f", 0);
                        gptUI->input_float3("Ozone Absorption", ptEnvironment->tOzoneExtinction.d, "%0.6f", 0);
                        gptUI->input_float("Mie Scattering", &ptEnvironment->fScatteringMieGround, "%0.6f", 0);
                        gptUI->input_float("Mie Absorption", &ptEnvironment->fExtinctionMieGround, "%0.6f", 0);
                        gptUI->input_float("Mie Scatter Asymmetry", &ptEnvironment->fMieScatteringExponent, "%0.6f", 0);

                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_AERIAL_PERSPECTIVE)
                        {
                            gptUI->separator_text("Aerial Perspective");
                            gptUI->input_float("Max. Distance", &ptEnvironment->fMaxAerialDistance, "%g", 0);
                            gptUI->input_float("Depth Exponent", &ptEnvironment->fAerialDepthExponent, "%g", 0);
                            int iAerialSamplesPerSlice = ptEnvironment->uAerialSamplesPerSlice;
                            gptUI->input_int("Samples per Slice", &iAerialSamplesPerSlice, 0);
                            ptEnvironment->uAerialSamplesPerSlice = (uint32_t)iAerialSamplesPerSlice;
                        }
                    }
                }
                else if(tAssetType == gptTerrain->get_asset_type_key())
                {
                    plTerrainAsset* ptTerrain = gptAsset->get_data(tAssetHandle);
                    gptUI->input_float("meters per pixel", &ptTerrain->fMetersPerPixel, "%g", 0);
                    gptUI->input_uint("horizontal tiles", &ptTerrain->uHorizontalTiles, 0);
                    gptUI->input_uint("vertical tiles", &ptTerrain->uVerticalTiles, 0);
                    gptUI->labeled_text("size", "%u", ptTerrain->uSize);
                    gptUI->labeled_text("tiles", "%u", ptTerrain->uTileCount);

                    static uint32_t uSelectedTile = 0;
                    uSelectedTile = pl_clampu(0, uSelectedTile, ptTerrain->uTileCount - 1);

                    gptUI->separator_text("Elevation Zones");

                    for(uint32_t i = 0; i < ptTerrain->uElevationZoneCount; i++)
                    {
                        if(gptUI->tree_node_f("Zone: %d", 0, i))
                        {
                            gptUI->labeled_text("flat material", gptAsset->get_path(ptTerrain->atElevationZones[i].tFlatMaterial));
                            gptUI->labeled_text("steep material", gptAsset->get_path(ptTerrain->atElevationZones[i].tSteepMaterial));
                            gptUI->input_float("fMinElevation", &ptTerrain->atElevationZones[i].fMinElevation, "%g", 0);
                            gptUI->input_float("fMaxElevation", &ptTerrain->atElevationZones[i].fMaxElevation, "%g", 0);
                            gptUI->input_float("fBlendSize", &ptTerrain->atElevationZones[i].fBlendSize, "%g", 0);
                            gptUI->tree_pop();
                        }
                    }

                    gptUI->separator_text("Tiles");
                    if(ptTerrain->uTileCount > 1)
                        gptUI->slider_uint("Tile", &uSelectedTile, 0, ptTerrain->uTileCount, 0);

                    plTerrainProcessTileInfo* ptTile = &ptTerrain->atTiles[uSelectedTile];
                    gptUI->labeled_text("height map", gptAsset->get_path(tAssetHandle));
                    gptUI->input_float("max. base error", &ptTile->fMaxBaseError, "%g", 0);
                    gptUI->input_float("max. height", &ptTile->fMaxHeight, "%g", 0);
                    gptUI->input_float("min. height", &ptTile->fMinHeight, "%g", 0);
                    gptUI->input_int("tree depth", &ptTile->iTreeDepth,  0);
                    gptUI->input_float3("center", ptTile->tCenter.d, "%0.6f", 0);


                }
                else if(tAssetType == gptAnimation->get_asset_type_key())
                {
                    static const char* apcPathText[] = {
                        "unknown",
                        "translation",
                        "rotation",
                        "scale",
                        "weights"
                    };

                    static const char* apcModeText[] = {
                        "unknown",
                        "linear",
                        "step",
                        "cubic_spline"
                    };

                    plAnimation* ptAnimation = gptAsset->get_data(tAssetHandle);
                    gptUI->input_float("start", &ptAnimation->fStart, "%g", 0);
                    gptUI->input_float("end", &ptAnimation->fEnd, "%g", 0);

                    gptUI->separator_text("Joints");

                    static uint32_t uSelectedChannel = 0;
                    uSelectedChannel = pl_clampu(0, uSelectedChannel, ptAnimation->uChannelCount - 1);
                    gptUI->slider_uint("channel", &uSelectedChannel, 0, ptAnimation->uChannelCount - 1, 0);

                    plAnimationChannel* ptChannel = &ptAnimation->atChannels[uSelectedChannel];
                    gptUI->labeled_text("mode", apcModeText[ptChannel->tMode]);
                    gptUI->labeled_text("path", apcPathText[ptChannel->tPath]);

                }
                else if(tAssetType == gptSkeleton->get_asset_type_key_skeleton())
                {
                    plSkeleton* ptSkeleton = gptAsset->get_data(tAssetHandle);
                    gptUI->separator_text("Joints");
                    static uint32_t uSelectedJoint = 0;
                    uSelectedJoint = pl_clampu(0, uSelectedJoint, ptSkeleton->uJointCount - 1);
                    gptUI->slider_uint("joint", &uSelectedJoint, 0, ptSkeleton->uJointCount - 1, 0);

                    plSkeletonJoint* ptJoint = &ptSkeleton->atJoints[uSelectedJoint];
                    gptUI->labeled_text("name", ptJoint->pcName);
                    gptUI->labeled_text("parent", "%u", ptJoint->uParent);
                    gptUI->labeled_text("translation", "%g, %g, %g", ptJoint->tTranslation.x, ptJoint->tTranslation.y, ptJoint->tTranslation.z);
                    gptUI->labeled_text("rotation", "%g, %g, %g, %g", ptJoint->tRotation.x, ptJoint->tRotation.y, ptJoint->tRotation.z, ptJoint->tRotation.w);
                    gptUI->labeled_text("scale", "%g, %g, %g", ptJoint->tScale.x, ptJoint->tScale.y, ptJoint->tScale.z);

                }
                else if(tAssetType == gptSkeleton->get_asset_type_key_skin())
                {
                    plSkin* ptSkin = gptAsset->get_data(tAssetHandle);
                    gptUI->labeled_text("skeleton", gptAsset->get_path(ptSkin->tSkeleton));
                    gptUI->separator_text("Joints");
                    for(uint32_t i = 0; i < ptSkin->uJointCount; i++)
                    {
                        gptUI->text("%" PRIu64, ptSkin->atJoints[i]);
                    }
                }
                else if(tAssetType == gptEcs->get_asset_type_key())
                {
                    // separate window below
                }
                gptUI->end_child();
            }
            gptUI->end_window();
        }
    }

    for(uint32_t iSelectionIndex = 0; iSelectionIndex < uSelectedCount; iSelectionIndex++)
    {
        plAssetToolSelectData* ptSelectionData = &gptAssetToolsCtx->sbtSelectionData[gptAssetToolsCtx->sbuSelectionData[iSelectionIndex]];
        plAssetHandle tAssetHandle = atAssetHandles[gptAssetToolsCtx->sbuSelectionData[iSelectionIndex]];
        plAssetTypeKey tAssetType = gptAsset->get_type_key(tAssetHandle);

        if(tAssetType == gptEcs->get_asset_type_key()) // ecs library
        {

            if(pl_asset_tools_show_window(tAssetHandle, &ptSelectionData->tSelectedEntity, NULL))
            {
                bResult = true;
            }
        }
    }

    return bResult;
}

void
pl_asset_tools_initialize(void)
{
    if(gptConsole->add_bool_variable)
    {
        gptConsole->add_toggle_variable("t.Assets", &gptAssetToolsCtx->bShowTool, "shows assets tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    }
}

void
pl_asset_tools_cleanup(void)
{
    for(uint32_t i = 0; i < pl_sb_size(gptAssetToolsCtx->sbtLibraryData); i++)
    {
        gptUI->text_filter_cleanup(&gptAssetToolsCtx->sbtLibraryData[i].tFilter);
    }
    
    gptUI->text_filter_cleanup(&gptAssetToolsCtx->tAssetFilter);
    pl_sb_free(gptAssetToolsCtx->sbtLibraryData);
    pl_sb_free(gptAssetToolsCtx->sbcBuffer);
    pl_sb_free(gptAssetToolsCtx->sbtSelectionData);
    pl_sb_free(gptAssetToolsCtx->sbuSelectionData);
    pl_hm_free(&gptAssetToolsCtx->tLibraryHashmap);
}

void
pl_asset_tools_run(void)
{
    if(gptAssetToolsCtx->bShowTool)
    {
        pl_asset_tools_show_assets(&gptAssetToolsCtx->bShowTool);
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_asset_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plAssetToolsI tApi = {
        .initialize  = pl_asset_tools_initialize,
        .cleanup     = pl_asset_tools_cleanup,
        .run         = pl_asset_tools_run
    };
    pl_set_api(ptApiRegistry, plAssetToolsI, &tApi);

    #ifndef PL_UNITY_BUILD
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
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptAssetToolsCtx = ptDataRegistry->get_data("plEcsToolsContext");
    }
    else
    {
        static plEcsToolsContext gtEcsToolsCtx = {0};
        gptAssetToolsCtx = &gtEcsToolsCtx;
        ptDataRegistry->set_data("plEcsToolsContext", gptAssetToolsCtx);
    }
}

void
pl_unload_asset_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plAssetToolsI* ptApi = pl_get_api_latest(ptApiRegistry, plAssetToolsI);
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
#endif