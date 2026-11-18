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
#include "pl_ecs_tools_ext.h"
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

#define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
#define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

static const plEcsToolsI* gptEcsTools = NULL;
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
#define PL_ICON_FA_CUBES "\xef\x86\xb3"	// U+f1b3

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
    PL_ICON_FA_CUBES,
};

static const char* apcAssetNames[] = {
    "All",
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
    PL_ICON_FA_CUBES " voxel",
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plAssetToolSelectData
{
    bool bSelected;
} plAssetToolSelectData;

typedef struct _plAssetToolsContext
{
    char* sbcBuffer;
    bool  bShowTool;

    // asset data
    plUiTextFilter tAssetFilter;


    plAssetToolSelectData* sbtSelectionData;
    uint32_t* sbuSelectionData;

    bool bVoxelAssets;

} plAssetToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plAssetToolsContext* gptAssetToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl__asset_tools_voxel_properties(const plVoxelGrid* ptGrid)
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

bool
pl_asset_tools_show_assets(bool* bValue)
{
    bool bResult = false;
    const plAssetTypeDesc* ptAssetTypes = NULL;
    uint32_t uAssetTypeCount = gptAsset->get_type_descriptions(&ptAssetTypes);
    gptAssetToolsCtx->bVoxelAssets = gptVoxel && gptVoxel->get_asset_type_key &&
        gptVoxel->get_asset_type_key() < uAssetTypeCount &&
        strcmp(ptAssetTypes[gptVoxel->get_asset_type_key()].pcName, "Voxel") == 0;

    uint32_t uAssetCount = 0;
    const plAssetHandle* atAssetHandles = gptAsset->get_assets(&uAssetCount);
    pl_sb_resize(gptAssetToolsCtx->sbtSelectionData, uAssetCount);

    if(gptUI->begin_window("Assets", bValue, false))
    {
        const plVec2 tWindowSize = gptUI->get_window_size();

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
            gptAssetToolsCtx->bVoxelAssets ? gptVoxel->get_asset_type_key() : UINT32_MAX,
        };

        const float afSizes[] = {0.20f, 0.80f};
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, afSizes);

        gptUI->text("Asset Type");

        if(gptUI->input_text_hint("Asset Filter", "Filter (inc,-exc)", gptAssetToolsCtx->tAssetFilter.acInputBuffer, 256, 0))
        {
            gptUI->text_filter_build(&gptAssetToolsCtx->tAssetFilter);
        }
        gptUI->layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, tWindowSize.y - 80.0f, 2, afSizes);

        static int iAssetFilter = 0;
        gptUI->push_theme_color(PL_UI_COLOR_CHILD_BG, (plVec4){0});
        if(gptUI->begin_child("Assets Types Left", 0, 0))
        {
            gptUI->layout_dynamic(0.0f, 1);
            for(int j = 0; j < PL_ARRAYSIZE(atAssetTypesTypes); j++)
            {
                if(atAssetTypesTypes[j] == UINT32_MAX)
                    continue;
                // gptUI->radio_button(apcAssetNames[j], &iAssetFilter, (int)j);
                bool bPlaceHolder = j == iAssetFilter;
                if(gptUI->selectable(apcAssetNames[j], &bPlaceHolder, 0))
                    iAssetFilter = j;
            }
            gptUI->end_child();
        }
        gptUI->pop_theme_color(1);

        if(gptUI->begin_child("Assets Types Right", 0, 0))
        {
            gptUI->layout_dynamic(0.0f, 1);

            if(iAssetFilter != 0 || gptUI->text_filter_active(&gptAssetToolsCtx->tAssetFilter))
            {
                for(uint32_t i = 0; i < uAssetCount; i++)
                {
                    size_t szUnused = 0;
                    plAssetHandle tAssetHandle = atAssetHandles[i];
                    plAssetTypeKey tAssetType = gptAsset->get_type_key(tAssetHandle);
                    if(iAssetFilter != 0)
                    {
                        if(atAssetTypesTypes[iAssetFilter] != tAssetType)
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

        if(gptUI->begin_window(gptAsset->get_path(tAssetHandle), &ptSelectionData->bSelected, 0))
        {
            const plVec2 tWindowSize = gptUI->get_window_size();
            // const plVec2 tWindowPos = gptUI->get_window_pos();

            gptUI->layout_dynamic(0.0f, 1);

            gptUI->text("Properties");
            gptUI->separator();

            gptUI->layout_dynamic(tWindowSize.y - 80.0f, 1);
            if(gptUI->begin_child("Assets Right", 0, 0))
            {
                bool bDestroy = false;
                gptUI->layout_dynamic(0.0f, 1);
                gptUI->labeled_text("Path", "%s", gptAsset->get_path(tAssetHandle));
                gptUI->labeled_text("Type", "%u", gptAsset->get_type_key(tAssetHandle));
                if(tAssetType == gptEcs->get_asset_type_key() && gptUI->button("Inspect entities"))
                    gptEcsTools->open(tAssetHandle);

                if(tAssetType != gptEcs->get_asset_type_key())
                {
                    gptUI->layout_static(0.0f, 150.0f, 4);
                    if(gptUI->button("Load"))   gptAsset->load(gptAsset->get_path(tAssetHandle));
                    if(gptUI->button("Reload")) gptAsset->reload(tAssetHandle);
                    if(gptUI->button("Update")) gptAsset->mark_changed(tAssetHandle);
                    if(gptUI->button("Unload")) bDestroy = true;
                }

                gptUI->layout_static(0.0f, 150.0f, 3);

                if(gptUI->button("Save"))          gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_AUTO);
                if(gptUI->button("Save (text)"))   gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_TEXT);
                if(gptUI->button("Save (binary)")) gptAsset->save(tAssetHandle, PL_ASSET_ENCODING_BINARY);

                gptUI->layout_static(0.0f, 150.0f, 1);
                

                gptUI->layout_dynamic(0.0f, 1);

                if(gptAssetToolsCtx->bVoxelAssets && tAssetType == gptVoxel->get_asset_type_key())
                {
                    gptUI->separator_text("Voxel occupancy grid");
                    gptUI->text("Version: %u", gptAsset->get_version(tAssetHandle));
                    gptUI->text("Binary .plvoxel asset; volumes own independent copies.");
                    pl__asset_tools_voxel_properties(gptAsset->get_data(tAssetHandle));
                }

                if(tAssetType == gptMaterial->get_asset_type_key())
                {
                    plMaterial* ptMaterial = gptAsset->get_data(tAssetHandle);

                    gptUI->separator_text("base");

                    gptUI->layout_dynamic(0.0f, 1);

                    static const char* apcMaterialModelText[] = {
                        "PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS",
                        "PL_MATERIAL_MODEL_PBR_METALLIC_ROUGHNESS_FOLIAGE",
                        "PL_MATERIAL_MODEL_WATER"
                    };
                    if(gptUI->begin_combo("Model", apcMaterialModelText[ptMaterial->eMaterialModel], PL_UI_COMBO_FLAGS_NONE))
                    {
                        for(uint32_t i = 0; i < PL_ARRAYSIZE(apcMaterialModelText); i++)
                        {
                            bool bPlaceHolder = ptMaterial->eMaterialModel == (int)i;
                            if(gptUI->selectable(apcMaterialModelText[i], &bPlaceHolder, 0))
                            {
                                ptMaterial->eMaterialModel = (int)i;
                                gptUI->close_current_popup();
                            }
                        }
                        gptUI->end_combo();
                    }

                    const char* apcAlphaMode[] = {
                        "PL_MATERIAL_ALPHA_MODE_OPAQUE",
                        "PL_MATERIAL_ALPHA_MODE_MASK",
                        "PL_MATERIAL_ALPHA_MODE_BLEND"
                    };

                    gptUI->labeled_text("alpha mode", apcAlphaMode[ptMaterial->eAlphaMode]);
                    gptUI->input_float("alpha cutoff", &ptMaterial->fAlphaCutoff, "%g", 0);
                    gptUI->checkbox_flags("double sided", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DOUBLE_SIDED);
                    gptUI->color_edit4("base color", ptMaterial->tBaseColor.d, PL_UI_COLOR_EDIT_FLAGS_NONE, NULL);
                    gptUI->input_float4("base color##3", ptMaterial->tBaseColor.d, "%g", 0);
                    // gptUI->input_float4("base color2", ptMaterial->tBaseColor.d, "%g", 0);
                    gptUI->input_float("metalness", &ptMaterial->fMetalness, "%g", 0);
                    gptUI->input_float("roughness", &ptMaterial->fRoughness, "%g", 0);
                    gptUI->input_float("normal map strength", &ptMaterial->fNormalMapStrength, "%g", 0);
                    gptUI->input_float("occlusion strength", &ptMaterial->fOcclusionStrength, "%g", 0);
                    gptUI->input_float("ior", &ptMaterial->fIor, "%g", 0);

                    if(ptMaterial->eMaterialModel == PL_MATERIAL_MODEL_WATER && gptUI->collapsing_header("Water", 0))
                    {
                        plMaterialWater* ptWaterSettings = &ptMaterial->tWater;
                        *ptMaterial = gptMaterial->sanitize(*ptMaterial);
                        const char* apcWaveModes[] = {"Legacy swells", "Gerstner waves", "FFT ocean"};
                        if(gptUI->begin_combo("Wave model", apcWaveModes[ptWaterSettings->iWaveMode], 0))
                        {
                            for(int i = 0; i < 3; i++)
                            {
                                bool bSelected = ptWaterSettings->iWaveMode == i;
                                if(gptUI->selectable(apcWaveModes[i], &bSelected, 0))
                                { ptWaterSettings->iWaveMode = i; gptUI->close_current_popup(); }
                            }
                            gptUI->end_combo();
                        }
                        const char* apcOceanSizes[] = {"64", "128", "256"};
                        int iOceanSize = ptWaterSettings->iFftResolution == 64 ? 0 : (ptWaterSettings->iFftResolution == 128 ? 1 : 2);
                        if(gptUI->begin_combo("FFT resolution", apcOceanSizes[iOceanSize], 0))
                        {
                            for(int i = 0; i < 3; i++)
                            {
                                bool bSelected = iOceanSize == i;
                                if(gptUI->selectable(apcOceanSizes[i], &bSelected, 0))
                                { ptWaterSettings->iFftResolution = 64 << i; gptUI->close_current_popup(); }
                            }
                            gptUI->end_combo();
                        }
                        gptUI->input_int("FFT seed", &ptWaterSettings->iFftSeed, 0);
                        gptUI->input_float("gerstner steepness", &ptWaterSettings->fGerstnerSteepness, "%g", 0);
                        gptUI->input_float("fft patch size", &ptWaterSettings->fFftPatchSize, "%g", 0);
                        gptUI->input_float("fft height limit", &ptWaterSettings->fFftHeight, "%g", 0);
                        gptUI->input_float("fft wind speed", &ptWaterSettings->fFftWindSpeed, "%g", 0);
                        gptUI->input_float("fft wind heading degrees", &ptWaterSettings->fFftWindHeading, "%g", 0);
                        gptUI->input_float("fft against wind", &ptWaterSettings->fFftWindDependency, "%g", 0);
                        gptUI->input_float("fft short wave damping", &ptWaterSettings->fFftDamping, "%g", 0);
                        gptUI->input_float("fft choppiness", &ptWaterSettings->fFftChoppiness, "%g", 0);
                        gptUI->input_float("fft speed", &ptWaterSettings->fFftSpeed, "%g", 0);
                        gptUI->separator_text("Primary swell");
                        gptUI->input_float("swell0 amplitude", &ptWaterSettings->fAmplitude0, "%g", 0);
                        gptUI->input_float("swell0 wavelength", &ptWaterSettings->fWavelength0, "%g", 0);
                        gptUI->input_float("swell0 speed", &ptWaterSettings->fSpeed0, "%g", 0);
                        gptUI->input_float("swell0 heading degrees", &ptWaterSettings->fHeading0, "%g", 0);
                        gptUI->separator_text("Secondary swell");
                        gptUI->input_float("swell1 amplitude", &ptWaterSettings->fAmplitude1, "%g", 0);
                        gptUI->input_float("swell1 wavelength", &ptWaterSettings->fWavelength1, "%g", 0);
                        gptUI->input_float("swell1 speed", &ptWaterSettings->fSpeed1, "%g", 0);
                        gptUI->input_float("swell1 heading degrees", &ptWaterSettings->fHeading1, "%g", 0);
                        gptUI->separator_text("Surface detail");
                        gptUI->input_float("detail scale", &ptWaterSettings->fDetailScale, "%g", 0);
                        gptUI->input_float("detail speed", &ptWaterSettings->fDetailSpeed, "%g", 0);
                        gptUI->input_float("foam speed", &ptWaterSettings->fFoamSpeed, "%g", 0);
                        gptUI->input_float("foam scale", &ptWaterSettings->fFoamScale, "%g", 0);
                        gptUI->separator_text("Shore foam");
                        gptUI->input_float("shore strength", &ptWaterSettings->fShoreStrength, "%g", 0);
                        gptUI->input_float("shore depth", &ptWaterSettings->fShoreDepth, "%g", 0);
                        gptUI->input_float("shore ray fade start", &ptWaterSettings->fShoreRayStart, "%g", 0);
                        gptUI->input_float("shore ray fade end", &ptWaterSettings->fShoreRayEnd, "%g", 0);
                        gptUI->separator_text("Whitecaps");
                        gptUI->input_float("whitecap strength", &ptWaterSettings->fWhitecapStrength, "%g", 0);
                        gptUI->input_float("crest start", &ptWaterSettings->fCrestStart, "%g", 0);
                        gptUI->input_float("crest full", &ptWaterSettings->fCrestFull, "%g", 0);
                        gptUI->input_float("slope start", &ptWaterSettings->fSlopeStart, "%g", 0);
                        gptUI->separator_text("Surface and foam");
                        gptUI->input_float("slope full", &ptWaterSettings->fSlopeFull, "%g", 0);
                        gptUI->input_float("minimum roughness", &ptWaterSettings->fMinRoughness, "%g", 0);
                        gptUI->input_float("foam roughness", &ptWaterSettings->fFoamRoughness, "%g", 0);
                        gptUI->input_float("foam normal flatten", &ptWaterSettings->fFoamNormalFlatten, "%g", 0);
                        gptUI->separator_text("Water volume");
                        gptUI->text("Absorption color: light remaining at the reference distance.");
                        gptUI->input_float("absorption red", &ptWaterSettings->fAbsorptionRed, "%g", 0);
                        gptUI->input_float("absorption green", &ptWaterSettings->fAbsorptionGreen, "%g", 0);
                        gptUI->input_float("absorption blue", &ptWaterSettings->fAbsorptionBlue, "%g", 0);
                        gptUI->input_float("absorption distance", &ptWaterSettings->fAbsorptionDistance, "%g", 0);
                        gptUI->input_float("scattering red", &ptWaterSettings->fScatteringRed, "%g", 0);
                        gptUI->input_float("scattering green", &ptWaterSettings->fScatteringGreen, "%g", 0);
                        gptUI->input_float("scattering blue", &ptWaterSettings->fScatteringBlue, "%g", 0);
                        gptUI->input_float("scattering strength", &ptWaterSettings->fScatteringStrength, "%g", 0);
                        gptUI->input_float("scattering direction", &ptWaterSettings->fScatteringDirection, "%g", 0);
                        gptUI->separator_text("Refraction");
                        gptUI->input_float("refraction strength", &ptWaterSettings->fDistortionStrength, "%g", 0);
                        gptUI->input_float("refraction max pixels", &ptWaterSettings->fMaxDistortionPixels, "%g", 0);
                        gptUI->input_float("refraction max distance", &ptWaterSettings->fMaxPath, "%g", 0);
                        gptUI->input_float("refraction edge fade pixels", &ptWaterSettings->fEdgeFade, "%g", 0);
                        gptUI->input_float("refraction intersection fade", &ptWaterSettings->fIntersectionFade, "%g", 0);
                        gptUI->separator_text("Foam color and debug");
                        gptUI->input_float("foam red", &ptWaterSettings->fFoamRed, "%g", 0);
                        gptUI->input_float("foam green", &ptWaterSettings->fFoamGreen, "%g", 0);
                        gptUI->input_float("foam blue", &ptWaterSettings->fFoamBlue, "%g", 0);
                        const char* apcWaterDebug[] = {"Shaded", "Foam coverage", "Water depth", "Without shadows", "Refraction only", "Surface reflection only", "Composite coverage", "Volume transmittance", "Volume scattering"};
                        *ptMaterial = gptMaterial->sanitize(*ptMaterial);
                        int iWaterDebug = (int)ptWaterSettings->fDebugView;
                        if(gptUI->begin_combo("Water debug", apcWaterDebug[iWaterDebug], 0))
                        {
                            for(int i = 0; i < (int)PL_ARRAYSIZE(apcWaterDebug); i++)
                            {
                                bool bSelected = i == iWaterDebug;
                                if(gptUI->selectable(apcWaterDebug[i], &bSelected, 0))
                                {
                                    ptWaterSettings->fDebugView = (float)i;
                                    gptUI->close_current_popup();
                                }
                            }
                            gptUI->end_combo();
                        }
                    }

                    gptUI->separator_text("Advanced Properties");

                    gptUI->layout_dynamic(0.0f, 3);

                    gptUI->checkbox_flags("emissive##2", &ptMaterial->eFlags, PL_MATERIAL_FLAG_EMISSIVE);
                    gptUI->checkbox_flags("clearcoat", &ptMaterial->eFlags, PL_MATERIAL_FLAG_CLEARCOAT);
                    gptUI->checkbox_flags("sheen", &ptMaterial->eFlags, PL_MATERIAL_FLAG_SHEEN);
                    gptUI->checkbox_flags("iridescence", &ptMaterial->eFlags, PL_MATERIAL_FLAG_IRIDESCENCE);
                    gptUI->checkbox_flags("anisotropy", &ptMaterial->eFlags, PL_MATERIAL_FLAG_ANISOTROPY);
                    gptUI->checkbox_flags("transmission", &ptMaterial->eFlags, PL_MATERIAL_FLAG_TRANSMISSION);
                    gptUI->checkbox_flags("volume", &ptMaterial->eFlags, PL_MATERIAL_FLAG_VOLUME);
                    gptUI->checkbox_flags("dispersion", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DISPERSION);
                    gptUI->checkbox_flags("diffuse transmission", &ptMaterial->eFlags, PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION);

                    gptUI->layout_dynamic(0.0f, 1);
                    gptUI->checkbox_flags("foliage normals", &ptMaterial->eFlags, PL_MATERIAL_FLAG_FOLIAGE_NORMALS);

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_EMISSIVE)
                    {
                        if(gptUI->collapsing_header("Emissive", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->color_edit3("emissive color", ptMaterial->tEmissiveColor.d, PL_UI_COLOR_EDIT_FLAGS_NONE, ptMaterial->tEmissiveColor.d);
                            gptUI->input_float3("emissive color#3", ptMaterial->tEmissiveColor.d, "%g", 0);
                            gptUI->input_float("emissive strength", &ptMaterial->fEmissiveStrength, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_CLEARCOAT)
                    {
                        if(gptUI->collapsing_header("Clearcoat", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float("factor", &ptMaterial->tClearcoat.fFactor, "%g", 0);
                            gptUI->input_float("roughness", &ptMaterial->tClearcoat.fRoughness, "%g", 0);
                            gptUI->input_float("normal map strength", &ptMaterial->tClearcoat.fNormalMapStrength, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_SHEEN)
                    {
                        if(gptUI->collapsing_header("Sheen", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float3("color", ptMaterial->tSheen.tColor.d, "%g", 0);
                            gptUI->input_float("roughness", &ptMaterial->tSheen.fRoughness, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_IRIDESCENCE)
                    {
                        if(gptUI->collapsing_header("Iridescence", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float("factor", &ptMaterial->tIridescence.fFactor, "%g", 0);
                            gptUI->input_float("ior", &ptMaterial->tIridescence.fIor, "%g", 0);
                            gptUI->input_float("max. thickness", &ptMaterial->tIridescence.fThicknessMax, "%g", 0);
                            gptUI->input_float("min. thickness", &ptMaterial->tIridescence.fThicknessMin, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_ANISOTROPY)
                    {
                        if(gptUI->collapsing_header("Anisotropy", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float("strength", &ptMaterial->tAnisotropy.fStrength, "%g", 0);
                            gptUI->input_float("rotation", &ptMaterial->tAnisotropy.fRotation, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_TRANSMISSION)
                    {
                        if(gptUI->collapsing_header("Transmission", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float("factor", &ptMaterial->tTransmission.fFactor, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_VOLUME)
                    {
                        if(gptUI->collapsing_header("Volume", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float3("attenuation color", ptMaterial->tVolume.tAttenuationColor.d, "%g", 0);
                            gptUI->input_float("thickness", &ptMaterial->tVolume.fThickness, "%g", 0);
                            gptUI->input_float("attenuation distance", &ptMaterial->tVolume.fAttenuationDistance, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DISPERSION)
                    {
                        if(gptUI->collapsing_header("Dispersion", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float("dispersion", &ptMaterial->tDispersion.fDispersion, "%g", 0);
                            gptUI->apply_layout();
                        }
                    }

                    if(ptMaterial->eFlags & PL_MATERIAL_FLAG_DIFFUSE_TRANSMISSION)
                    {
                        if(gptUI->collapsing_header("Diffuse Transmission", 0))
                        {
                            gptUI->stash_layout();
                            gptUI->input_float3("color", ptMaterial->tDiffuseTransmission.tColor.d, "%g", 0);
                            gptUI->input_float("factor", &ptMaterial->tDiffuseTransmission.fFactor, "%g", 0);
                            gptUI->apply_layout();
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
                            if(gptUI->checkbox(apcTextureSlotNames[i], &bTextureActive))
                            {
                                ptMaterial->atTextures[i].tTexture = gptAsset->load("/assets/textures/default.pltexture");
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

                                    gptUI->set_next_window_pos(gptIOI->get_mouse_pos(), PL_UI_COND_APPEARING);
                                    gptUI->set_next_window_size((plVec2){400.0f, 500.0f}, PL_UI_COND_ALWAYS);

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
                                gptUI->slider_angle("Rotation", &ptMaterial->atTextures[i].fRotation, -360.0f, 360.0f, NULL, 0);
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
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_COLOR) gptUI->text("Colors");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_0) gptUI->text("Joints 0");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_JOINTS_1) gptUI->text("Joints 1");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0) gptUI->text("Weights 0");
                    if(ptSubmesh->uVertexStreamMask & PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1) gptUI->text("Weights 1");

                    if(ptMesh->uSubmeshCount > 1)
                    {
                        if(gptUI->button("Split"))
                        {
                            plMesh* atMeshes = PL_ALLOC(sizeof(plMesh) * ptMesh->uSubmeshCount);
                            memset(atMeshes, 0, sizeof(plMesh) * ptMesh->uSubmeshCount);
                            gptMesh->split(ptMesh, atMeshes);

                            char acFileNameOnly[1024] = {0};
                            pl_str_get_file_name_only(gptAsset->get_path(tAssetHandle), acFileNameOnly, 1024);

                            char* sbcTempBuffer = NULL;
                            for(uint32_t i = 0; i < ptMesh->uSubmeshCount; i++)
                            {
                                pl_sb_reset(sbcTempBuffer);
                                pl_sb_sprintf(sbcTempBuffer, "%s_%u.plmesh", acFileNameOnly, i);
                                plAssetDesc tNewAssetDesc = {
                                    .eFlags = PL_ASSET_FLAG_NONE,
                                    .tType = gptMesh->get_asset_type_key(),
                                    .pcPath = sbcTempBuffer
                                };
                                gptAsset->create(&tNewAssetDesc, &atMeshes[i]);
                            }
                            pl_sb_free(sbcTempBuffer);
                            PL_FREE(atMeshes);
                        }
                    }
                }
                else if(tAssetType == gptRenderer->get_asset_type_key_settings())
                {
                    plRenderSettings* ptSettings = gptAsset->get_data(tAssetHandle);

                    if(gptUI->collapsing_header("Lighting", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox_flags("Image Based Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED);
                        gptUI->checkbox_flags("Punctual Lighting", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS);
                        gptUI->checkbox_flags("Normal Mapping", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING);
                        gptUI->checkbox_flags("No Shadows", &ptSettings->tLighting.tFlags, PL_RENDERER_LIGHTING_FLAGS_NO_SHADOWS);
                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Shadows", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox_flags("MultiViewport Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT);
                        gptUI->checkbox_flags("PCF Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_PCF);
                        gptUI->checkbox_flags("Contact Shadows", &ptSettings->tShadows.tFlags, PL_RENDERER_SHADOW_FLAGS_CONTACT);
                        if(ptSettings->tShadows.tFlags & PL_RENDERER_SHADOW_FLAGS_CONTACT)
                        {
                            gptUI->input_float("Contact Length", &ptSettings->tShadows.fContactShadowLength, "%g", 0);
                            gptUI->input_float("Contact Thickness", &ptSettings->tShadows.fContactShadowThickness, "%g", 0);
                            gptUI->input_float("Contact Bias", &ptSettings->tShadows.fContactShadowBias, "%g", 0);
                            int iContactSteps = (int)ptSettings->tShadows.uContactShadowSteps;
                            gptUI->input_int("Contact Steps", &iContactSteps, 0);
                            ptSettings->tShadows.uContactShadowSteps = (uint32_t)pl_clampi(1, iContactSteps, 64);
                        }
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
                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Post Process", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox("FXAA", &ptSettings->bFXAAEnabled);
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
                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Screen Space GI", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->checkbox("SSGI Enabled", &ptSettings->tSSGI.bEnabled);
                        gptUI->input_float("SSGI Strength", &ptSettings->tSSGI.fStrength, "%g", 0);
                        gptUI->input_float("SSGI Radius", &ptSettings->tSSGI.fRadius, "%g", 0);
                        gptUI->input_float("SSGI Thickness", &ptSettings->tSSGI.fThickness, "%g", 0);
                        int iRays = (int)ptSettings->tSSGI.uRayCount;
                        int iSteps = (int)ptSettings->tSSGI.uStepCount;
                        gptUI->input_int("SSGI Rays", &iRays, 0);
                        gptUI->input_int("SSGI Steps", &iSteps, 0);
                        ptSettings->tSSGI.uRayCount = (uint32_t)pl_clampi(1, iRays, 32);
                        ptSettings->tSSGI.uStepCount = (uint32_t)pl_clampi(1, iSteps, 64);
                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Ambient Occlusion", 0))
                    {
                        gptUI->stash_layout();
                        gptUI->radio_button("AO Off", &ptSettings->tAO.tMode, PL_RENDERER_AO_NONE);
                        gptUI->radio_button("SSAO", &ptSettings->tAO.tMode, PL_RENDERER_AO_SSAO);
                        gptUI->radio_button("HBAO", &ptSettings->tAO.tMode, PL_RENDERER_AO_HBAO);
                        gptUI->radio_button("MSAO", &ptSettings->tAO.tMode, PL_RENDERER_AO_MSAO);
                        gptUI->slider_float("AO Radius", &ptSettings->tAO.fRadius, 0.01f, 10.0f, 0);
                        gptUI->slider_float("AO Strength", &ptSettings->tAO.fStrength, 0.0f, 4.0f, 0);
                        gptUI->slider_float("AO Bias", &ptSettings->tAO.fBias, 0.0f, 0.5f, 0);
                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Bloom", 0))
                    {
                        gptUI->stash_layout();
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

                        gptUI->apply_layout();
                    }

                    if(gptUI->collapsing_header("Fog", 0))
                    {
                        gptUI->stash_layout();
                        bool bFog = ptSettings->tFog.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
                        gptUI->checkbox("Fog", &bFog);
                        if(bFog)
                        {
                            ptSettings->tFog.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                            gptUI->radio_button("Linear Fog", &ptSettings->tFog.tMode, 0);
                            gptUI->radio_button("Exponential Fog", &ptSettings->tFog.tMode, 1);
                            gptUI->slider_float("Fog Start", &ptSettings->tFog.fStart, 0.0f, 100.0f, 0);
                            gptUI->slider_float("Fog End", &ptSettings->tFog.fCutOffDistance, 0.0f, 100000.0f, 0);
                            gptUI->color_edit3("Fog Color", ptSettings->tFog.tColor.d, 0, ptSettings->tFog.tColor.d);
                            if(ptSettings->tFog.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                            {
                                gptUI->slider_float("Fog Max Opacity", &ptSettings->tFog.fMaxOpacity, 0.0f, 1.0f, 0);
                                gptUI->slider_float("Fog Density", &ptSettings->tFog.fDensity, 0.0f, 10.0f, 0);
                                gptUI->slider_float("Fog Height", &ptSettings->tFog.fHeight, -100.0f, 100.0f, 0);
                                gptUI->slider_float("Fog Height Falloff", &ptSettings->tFog.fHeightFalloff, 0.0f, 1.0f, 0);
                            }  
                        }
                        else
                            ptSettings->tFog.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;
                        gptUI->apply_layout();
                    }

                }
                else if(tAssetType == gptRenderer->get_asset_type_key_environment())
                {
                    plRenderEnvironment* ptEnvironment = gptAsset->get_data(tAssetHandle);

                    gptUI->layout_dynamic(0.0f, 1);
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
                        gptUI->color_edit3("Sun Color", ptEnvironment->tSunColor.d, PL_UI_COLOR_EDIT_FLAGS_NONE, ptEnvironment->tSunColor.d);

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
                        bool bSunContactShadows = !(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_NO_CONTACT_SHADOWS);
                        if(gptUI->checkbox("Main Light Contact Shadows", &bSunContactShadows))
                        {
                            if(bSunContactShadows) ptEnvironment->eFlags &= ~PL_RENDERER_SKY_FLAGS_NO_CONTACT_SHADOWS;
                            else ptEnvironment->eFlags |= PL_RENDERER_SKY_FLAGS_NO_CONTACT_SHADOWS;
                        }

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

                        gptUI->checkbox_flags("Feature: Night Sky", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_NIGHT_SKY);
                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_NIGHT_SKY)
                        {
                            gptUI->separator_text("Stars and Moon");
                            gptUI->slider_float("Star Intensity", &ptEnvironment->fStarIntensity, 0.0f, 5.0f, 0);
                            gptUI->slider_angle("Star Rotation", &ptEnvironment->fStarRotation, -180.0f, 180.0f, NULL, 0);
                            gptUI->slider_float("Moon Phase", &ptEnvironment->fMoonPhase, 0.0f, 1.0f, 0);
                            gptUI->text("0/1 New | 0.25 First quarter | 0.5 Full | 0.75 Last quarter");
                            gptUI->text("Moon position follows phase relative to the sun.");
                            float fMoonAngle = 2.0f * PL_PI * pl_clampf(0.0f, ptEnvironment->fMoonPhase, 1.0f);
                            float fMoonIllumination = 0.5f * (1.0f - cosf(fMoonAngle));
                            gptUI->labeled_text("Moon Illumination", "%.1f%%", 100.0f * fMoonIllumination);
                            if(fMoonIllumination < 0.01f)
                                gptUI->text("New moon: near the sun, below the horizon at night.");
                            if(gptUI->button("Use Full Moon"))
                                ptEnvironment->fMoonPhase = 0.5f;
                            gptUI->slider_float("Moon Intensity", &ptEnvironment->fMoonIntensity, 0.0f, 5.0f, 0);
                            gptUI->slider_angle("Moon Angular Radius", &ptEnvironment->fMoonRadius, 0.05f, 2.0f, "%.3f deg", 0);
                            gptUI->checkbox_flags("Moonlight Illuminates Scene", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_MOONLIGHT);
                            if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_MOONLIGHT)
                            {
                                gptUI->slider_float_f("Full Moonlight Intensity", &ptEnvironment->fMoonlightIntensity, 0.0f, 1.0f, "%.4f", 0);
                                gptUI->color_edit3("Moonlight Color", ptEnvironment->tMoonlightColor.d, PL_UI_COLOR_EDIT_FLAGS_NONE, ptEnvironment->tMoonlightColor.d);
                                gptUI->text("Scene lighting follows phase and horizon; disk brightness is independent.");
                            }
                        }

                        gptUI->checkbox_flags("Feature: Volumetric Clouds", &ptEnvironment->eFlags, PL_RENDERER_SKY_FLAGS_CLOUDS);
                        if(ptEnvironment->eFlags & PL_RENDERER_SKY_FLAGS_CLOUDS)
                        {
                            gptUI->separator_text("Clouds (atmosphere units)");
                            gptUI->slider_float("Cloud multiple scattering", &ptEnvironment->fCloudMultiScatter, 0.0f, 1.0f, 0);
                            gptUI->slider_float("Cloud temporal blend (half resolution sky)", &ptEnvironment->fCloudTemporalBlend, 0.0f, 0.95f, 0);
                            gptUI->input_float("Cloud coverage", &ptEnvironment->fCloudCoverage, "%g", 0);
                            gptUI->input_float("Cloud base_height", &ptEnvironment->fCloudBaseHeight, "%g", 0);
                            gptUI->input_float("Cloud thickness", &ptEnvironment->fCloudThickness, "%g", 0);
                            gptUI->input_float("Cloud extinction", &ptEnvironment->fCloudExtinction, "%g", 0);
                            gptUI->input_float("Cloud scale", &ptEnvironment->fCloudScale, "%g", 0);
                            gptUI->input_float2("Cloud Wind", ptEnvironment->tCloudWind.d, "%g", 0);
                            gptUI->input_uint("Cloud Steps (16-128)", &ptEnvironment->uCloudSteps, 0);
                            gptUI->input_uint("Cloud Resolution Divisor (1-2)", &ptEnvironment->uCloudResolutionDivisor, 0);
                        }

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
                    gptUI->input_float3("center", ptTerrain->tCenter.d, "%0.6f", 0);

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

                if(bDestroy)
                    gptAsset->destroy(tAssetHandle);
            }
            gptUI->end_window();
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
    gptUI->text_filter_cleanup(&gptAssetToolsCtx->tAssetFilter);
    pl_sb_free(gptAssetToolsCtx->sbcBuffer);
    pl_sb_free(gptAssetToolsCtx->sbtSelectionData);
    pl_sb_free(gptAssetToolsCtx->sbuSelectionData);
    bool bShowTool = gptAssetToolsCtx->bShowTool;
    *gptAssetToolsCtx = (plAssetToolsContext){ .bShowTool = bShowTool };
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
        gptEcsTools       = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
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
        gptAssetToolsCtx = ptDataRegistry->get_data("plAssetToolsContext");
    }
    else
    { 
        static plAssetToolsContext gtAssetToolsCtx = {0};
        gptAssetToolsCtx = &gtAssetToolsCtx;
        ptDataRegistry->set_data("plAssetToolsContext", gptAssetToolsCtx);
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

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

#endif
