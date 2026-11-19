/*
   pl_scene_tools_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs
// [SECTION] global data
// [SECTION] navigation
// [SECTION] terrain and lighting
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <string.h>
#include "pl.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_scene_tools_ext.h"
#include "pl_ecs_ext.h"
#include "pl_ui_ext.h"
#include "pl_draw_ext.h"
#include "pl_renderer_ext.h"
#include "pl_terrain_ext.h"
#include "pl_freelist_ext.h"
#include "pl_string.h"
#include "pl_voxel_ext.h"
#include "pl_path_ext.h"

#ifndef PL_UNITY_BUILD
static const plUiI*       gptUI       = NULL;
static const plEcsI*      gptEcs      = NULL;
static const plDrawI*     gptDraw     = NULL;
static const plRendererI* gptRenderer = NULL;
static const plVoxelI*    gptVoxel    = NULL;
static const plPathI*     gptPath     = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plNavigationDebug
{
    const plComponentLibrary* ptLibrary;
    plEntityId                tVolumeId;
    plEntityId                tAgentId;
    bool                      bShow;
    bool                      bVoxels;
    bool                      bPaths;
    bool                      bAllPaths;
    bool                      bWaypoints;
    bool                      bXray;
    bool                      bFollowAgent;
    int                       iSlice;
    uint32_t                  uMaxVoxels;
    uint32_t                  uDrawn;
    plVec3                    tStart;
    plVec3                    tGoal;
    plVoxelCoord              tStartCell;
    plVoxelCoord              tGoalCell;
    plPathSettings            tCheckedSettings;
    uint64_t                  uCheckedRevision;
    bool                      bChecked;
    bool                      bStartInGrid;
    bool                      bGoalInGrid;
    bool                      bStartWalkable;
    bool                      bGoalWalkable;
    bool                      bSegmentClear;
} plNavigationDebug;

typedef struct _plTerrainDebug
{
    bool     bShow;
    bool     bBounds;
    bool     bHighlight;
    bool     bXray;
    bool     bResidentOnly;
    float    fLineWidth;
    int      iLevel;
    uint32_t uFile;
    uint32_t uChunk;
    uint32_t uPage;
    uint32_t uMaxBoxes;
    uint32_t uDrawn;
} plTerrainDebug;

typedef struct _plTerrainDebugStats
{
    uint64_t uChunks;
    uint64_t uResident;
    uint64_t uTriangles;
    uint64_t uVertexBytes;
    uint64_t uIndexBytes;
    uint32_t auLevels[256];
    uint32_t auResidentLevels[256];
} plTerrainDebugStats;

typedef struct _plSceneToolsContext
{
    const plComponentLibrary* ptLibrary;
    const plScene*            ptScene;
    int                       iTool;
    plNavigationDebug         tNavigationDebug;
    plTerrainDebug            tTerrainDebug;
    plDrawList3D*             ptVoxelDrawlist;
    const plVoxelGrid*        ptCachedVoxelGrid; // identity only; never dereferenced
    plEntityId                tCachedVolumeId;
    uint64_t                  uCachedVoxelRevision;
    int                       iCachedVoxelSlice;
    uint32_t                  uCachedMaxVoxels;
} plSceneToolsContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plSceneToolsContext* gptSceneToolsCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] navigation
//-----------------------------------------------------------------------------

static bool
pl__scene_tools_navigation_available(void)
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

static bool
pl__scene_tools_entity_combo(const char* pcLabel, plComponentLibrary* ptLibrary, plEcsTypeKey tType, plEntityId* ptId, const char* pcNone)
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
pl__scene_tools_voxel_properties(const plVoxelGrid* ptGrid)
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

static inline bool
pl__scene_tools_path_settings_equal(const plPathSettings* a, const plPathSettings* b)
{
    return a->bFlying == b->bFlying && a->bSimplify == b->bSimplify &&
        a->iAgentHeight == b->iAgentHeight && a->iAgentRadius == b->iAgentRadius &&
        a->iMaxStep == b->iMaxStep && a->iSnapRadius == b->iSnapRadius &&
        a->uMaxNodes == b->uMaxNodes && a->bAllowDiagonal == b->bAllowDiagonal &&
        a->tGroundSupport == b->tGroundSupport;
}

static void
pl__scene_tools_check_navigation(plNavigationDebug* ptDebug, const plVoxelGrid* ptGrid, uint64_t uRevision, const plPathSettings* ptSettings)
{
    ptDebug->tCheckedSettings = *ptSettings;
    ptDebug->uCheckedRevision = uRevision;
    ptDebug->bStartInGrid = gptVoxel->world_to_coord(ptGrid, ptDebug->tStart, &ptDebug->tStartCell);
    ptDebug->bGoalInGrid = gptVoxel->world_to_coord(ptGrid, ptDebug->tGoal, &ptDebug->tGoalCell);
    ptDebug->bStartWalkable = ptDebug->bStartInGrid && gptPath->is_walkable(ptGrid, ptDebug->tStartCell, ptSettings);
    ptDebug->bGoalWalkable = ptDebug->bGoalInGrid && gptPath->is_walkable(ptGrid, ptDebug->tGoalCell, ptSettings);
    ptDebug->bSegmentClear = ptDebug->bStartInGrid && ptDebug->bGoalInGrid && gptPath->is_segment_traversable(ptGrid, ptDebug->tStartCell, ptDebug->tGoalCell, ptSettings);
    ptDebug->bChecked = ptGrid != NULL;
}

static void
pl__scene_tools_navigation(plComponentLibrary* ptLibrary, plEntity tSelected, bool* pbOpen)
{
    plNavigationDebug* ptDebug = &gptSceneToolsCtx->tNavigationDebug;
    if(!ptLibrary || !pbOpen || !*pbOpen || !pl__scene_tools_navigation_available())
    {
        ptDebug->bShow = false;
        return;
    }
    if(ptDebug->ptLibrary != ptLibrary)
    {
        *ptDebug = (plNavigationDebug){
            .ptLibrary    = ptLibrary,
            .bPaths       = true,
            .bAllPaths    = true,
            .bWaypoints   = true,
            .bXray        = true,
            .bFollowAgent = true,
            .iSlice       = -1,
            .uMaxVoxels   = 25000
        };
        const plEntity* atVolumes = NULL;
        if(gptEcs->get_components(ptLibrary, gptVoxel->get_ecs_type_key_volume(), NULL, &atVolumes))
            ptDebug->tVolumeId = gptEcs->get_entity_id(ptLibrary, atVolumes[0]);
    }
    ptDebug->bShow = *pbOpen;
    gptUI->layout_dynamic(0, 1);
    gptUI->text("Live ECS navigation");
    gptUI->text("Edit settings, rebuild and bake in ECS.");

    plPathDebugInfo tInfo = gptPath->get_debug_info(ptLibrary);
    gptUI->text("Pending requests: %u / worker: %s", tInfo.uPendingCount, tInfo.bActive ? "active" : "idle");
    if(tInfo.tAgent.uData)
    {
        const plTagComponent* ptTag = gptEcs->get_component(ptLibrary, gptEcs->get_ecs_type_key_tag(), tInfo.tAgent);
        gptUI->text("Last worker query: %s", ptTag && ptTag->pcName ? ptTag->pcName : "removed / unnamed agent");
        gptUI->text("%s / %u visited / %u expanded", gptPath->status_string(tInfo.tStatus), tInfo.uVisitedNodes, tInfo.uExpandedNodes);
    }
    if(tInfo.bTimingAvailable)
        gptUI->text("Last path-system update: %.3f ms", tInfo.dLastUpdateMilliseconds);

    bool bChanged = false;
    if(gptUI->button("Inspect selected entity"))
    {
        if(gptEcs->has_component(ptLibrary, gptPath->get_ecs_type_key_agent(), tSelected))
        {
            ptDebug->tAgentId = gptEcs->get_entity_id(ptLibrary, tSelected);
            ptDebug->bFollowAgent = true;
            ptDebug->bAllPaths = false;
        }
        else if(gptEcs->has_component(ptLibrary, gptVoxel->get_ecs_type_key_volume(), tSelected))
        {
            ptDebug->tVolumeId = gptEcs->get_entity_id(ptLibrary, tSelected);
            ptDebug->bFollowAgent = false;
            ptDebug->tAgentId = 0;
            ptDebug->bAllPaths = true;
        }
        bChanged = true;
    }
    if(pl__scene_tools_entity_combo("Agent", ptLibrary, gptPath->get_ecs_type_key_agent(), &ptDebug->tAgentId, "None"))
    {
        ptDebug->bFollowAgent = true;
        ptDebug->bAllPaths = !ptDebug->tAgentId;
        bChanged = true;
    }
    const plPathAgentComponent* ptAgent = gptEcs->get_component(ptLibrary, gptPath->get_ecs_type_key_agent(), gptEcs->get_entity_by_id(ptLibrary, ptDebug->tAgentId));
    if(ptAgent)
    {
        bChanged |= gptUI->checkbox("Follow agent volume", &ptDebug->bFollowAgent);
        if(ptDebug->bFollowAgent && ptDebug->tVolumeId != ptAgent->tVolumeId)
        {
            ptDebug->tVolumeId = ptAgent->tVolumeId;
            bChanged = true;
        }
        gptUI->text("Agent: %s / %u waypoints / %.2f m", gptPath->status_string(ptAgent->tStatus), ptAgent->uWaypointCount, ptAgent->fLength);
        if(gptUI->button("Use last request endpoints"))
        {
            ptDebug->tStart = ptAgent->_tStart;
            ptDebug->tGoal = ptAgent->_tGoal;
            bChanged = true;
        }
    }

    if(pl__scene_tools_entity_combo("Volume", ptLibrary, gptVoxel->get_ecs_type_key_volume(), &ptDebug->tVolumeId, "None"))
    {
        ptDebug->bFollowAgent = false;
        bChanged = true;
    }

    plEntity tVolumeEntity = gptEcs->get_entity_by_id(ptLibrary, ptDebug->tVolumeId);
    uint64_t uRevision = 0;
    const plVoxelGrid* ptGrid = gptVoxel->get_volume_grid(ptLibrary, tVolumeEntity, &uRevision);
    const plVoxelVolumeComponent* ptVolume = gptEcs->get_component(ptLibrary, gptVoxel->get_ecs_type_key_volume(), tVolumeEntity);
    if(ptVolume)
        gptUI->text("Volume revision: %" PRIu64 " / %s", uRevision, ptVolume->bRebuild ? "rebuild requested" : ptVolume->bBuildFailed ? "build failed; using previous grid" : "ready");
    pl__scene_tools_voxel_properties(ptGrid);
    gptUI->checkbox("Show occupied voxels", &ptDebug->bVoxels);
    if(ptGrid)
    {
        if(ptDebug->iSlice >= (int)ptGrid->auResolution[1])
            ptDebug->iSlice = -1;
        gptUI->slider_int("Y slice (-1 = all)", &ptDebug->iSlice, -1, (int)ptGrid->auResolution[1] - 1, 0);
    }
    gptUI->slider_uint("Voxel box limit", &ptDebug->uMaxVoxels, 0, 25000, 0);
    gptUI->text("Last draw: %u occupied boxes", ptDebug->uDrawn);
    gptUI->checkbox("Show agent routes", &ptDebug->bPaths);
    gptUI->checkbox("All agents in volume", &ptDebug->bAllPaths);
    gptUI->checkbox("Waypoint markers", &ptDebug->bWaypoints);
    gptUI->checkbox("Draw through geometry", &ptDebug->bXray);
    gptUI->text("Yellow routes; display limit 10000 segments.");

    gptUI->separator_text("Read-only traversal checks");
    plPathSettings tSettings = ptAgent ? ptAgent->tSettings : gptPath->default_settings();
    gptUI->text("Uses %s settings.", ptAgent ? "selected agent" : "default agent");
    bChanged |= gptUI->input_float3("Check start", ptDebug->tStart.d, "%g", 0);
    bChanged |= gptUI->input_float3("Check goal", ptDebug->tGoal.d, "%g", 0);
    if(bChanged || uRevision != ptDebug->uCheckedRevision || !pl__scene_tools_path_settings_equal(&tSettings, &ptDebug->tCheckedSettings))
        ptDebug->bChecked = false;
    if(gptUI->button("Check endpoints and segment"))
        pl__scene_tools_check_navigation(ptDebug, ptGrid, uRevision, &tSettings);
    if(ptDebug->bChecked)
    {
        gptUI->text("Start: %s / goal: %s", ptDebug->bStartWalkable ? "walkable" : "invalid", ptDebug->bGoalWalkable ? "walkable" : "invalid");
        gptUI->text("Cell-center segment: %s", !ptDebug->bStartInGrid || !ptDebug->bGoalInGrid ? "outside grid" : ptDebug->bSegmentClear ? "clear" : "blocked");
    }
    else
        gptUI->text("Run the check for the current grid and settings.");
    gptUI->text("Ground checks require equal endpoint cell heights.");
    gptUI->text("These checks do not submit or modify agent requests.");
    ptDebug->bShow = *pbOpen;
}

static plDrawList3D*
pl__scene_tools_cache_voxels(const plVoxelGrid* ptGrid, uint64_t uRevision)
{
    plNavigationDebug* ptDebug = &gptSceneToolsCtx->tNavigationDebug;
    if(!ptGrid)
    {
        if(gptSceneToolsCtx->ptCachedVoxelGrid && gptSceneToolsCtx->ptVoxelDrawlist)
            gptDraw->clear_3d_drawlist(gptSceneToolsCtx->ptVoxelDrawlist);
        gptSceneToolsCtx->ptCachedVoxelGrid = NULL;
        ptDebug->uDrawn = 0;
        return NULL;
    }
    if(!gptSceneToolsCtx->ptVoxelDrawlist)
        gptSceneToolsCtx->ptVoxelDrawlist = gptDraw->request_3d_drawlist(PL_DRAW_LIST_3D_FLAG_PERSISTENT);
    if(!gptSceneToolsCtx->ptVoxelDrawlist)
        return NULL;

    const uint32_t uMaxVoxels = ptDebug->bVoxels ? ptDebug->uMaxVoxels : 0;
    if(gptSceneToolsCtx->ptCachedVoxelGrid != ptGrid || gptSceneToolsCtx->tCachedVolumeId != ptDebug->tVolumeId || gptSceneToolsCtx->uCachedVoxelRevision != uRevision || gptSceneToolsCtx->iCachedVoxelSlice != ptDebug->iSlice || gptSceneToolsCtx->uCachedMaxVoxels != uMaxVoxels)
    {
        gptDraw->clear_3d_drawlist(gptSceneToolsCtx->ptVoxelDrawlist);
        ptDebug->uDrawn = gptVoxel->debug_draw(ptGrid, gptSceneToolsCtx->ptVoxelDrawlist, PL_COLOR_32_GREEN, ptDebug->iSlice, uMaxVoxels);
        gptSceneToolsCtx->ptCachedVoxelGrid = ptGrid;
        gptSceneToolsCtx->tCachedVolumeId = ptDebug->tVolumeId;
        gptSceneToolsCtx->uCachedVoxelRevision = uRevision;
        gptSceneToolsCtx->iCachedVoxelSlice = ptDebug->iSlice;
        gptSceneToolsCtx->uCachedMaxVoxels = uMaxVoxels;
    }
    return gptSceneToolsCtx->ptVoxelDrawlist;
}

static void
pl__scene_tools_draw_navigation(const plComponentLibrary* ptLibrary, plView* ptView, plDrawList3D* ptDrawlist, plDrawList3D* ptOverlay)
{
    plNavigationDebug* ptDebug = &gptSceneToolsCtx->tNavigationDebug;
    if(!ptDebug->bShow || ptDebug->ptLibrary != ptLibrary || !ptDrawlist || !pl__scene_tools_navigation_available())
        return;
    if(ptDebug->bXray && ptOverlay)
        ptDrawlist = ptOverlay;
    uint64_t uRevision = 0;
    const plVoxelGrid* ptGrid = gptVoxel->get_volume_grid(ptLibrary, gptEcs->get_entity_by_id((plComponentLibrary*)ptLibrary, ptDebug->tVolumeId), &uRevision);
    plDrawList3D* ptVoxelDrawlist = pl__scene_tools_cache_voxels(ptGrid, uRevision);
    if(ptVoxelDrawlist)
        gptRenderer->queue_drawlist(ptView, ptVoxelDrawlist, ptDebug->bXray);
    const plPathAgentComponent* atAgents = NULL;
    const plEntity* atEntities = NULL;
    uint32_t uCount = gptEcs->get_components(ptLibrary, gptPath->get_ecs_type_key_agent(), (void**)&atAgents, &atEntities);
    uint32_t uSegments = 0;
    for(uint32_t i = 0; ptDebug->bPaths && i < uCount && uSegments < 10000; i++)
    {
        const plPathAgentComponent* ptAgent = &atAgents[i];
        if(ptAgent->tStatus != PL_PATH_STATUS_SUCCESS || !ptAgent->atWaypoints || (ptDebug->bAllPaths && ptDebug->tVolumeId && ptAgent->tVolumeId != ptDebug->tVolumeId) || (!ptDebug->bAllPaths && gptEcs->get_entity_id(ptLibrary, atEntities[i]) != ptDebug->tAgentId))
            continue;
        uint64_t uAgentRevision = 0;
        const plVoxelGrid* ptAgentGrid = gptVoxel->get_volume_grid(ptLibrary, gptEcs->get_entity_by_id((plComponentLibrary*)ptLibrary, ptAgent->tVolumeId), &uAgentRevision);
        if(!ptAgentGrid || uAgentRevision != ptAgent->uVolumeRevision || ptAgent->tVolumeId != ptAgent->_tRequestVolumeId)
            continue;
        for(uint32_t j = 0; j < ptAgent->uWaypointCount && uSegments < 10000; j++)
        {
            if(j)
            {
                gptDraw->add_3d_line(ptDrawlist, ptAgent->atWaypoints[j - 1], ptAgent->atWaypoints[j], (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.04f});
                uSegments++;
            }
            if(ptDebug->bWaypoints)
                gptDraw->add_3d_cross(ptDrawlist, ptAgent->atWaypoints[j], ptAgentGrid->fVoxelSize * 0.15f, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = 0.02f});
        }
    }
    const plPathAgentComponent* ptAgent = gptEcs->get_component(ptLibrary, gptPath->get_ecs_type_key_agent(), gptEcs->get_entity_by_id((plComponentLibrary*)ptLibrary, ptDebug->tAgentId));
    plPathSettings tSettings = ptAgent ? ptAgent->tSettings : gptPath->default_settings();
    if(ptGrid && ptDebug->bChecked && ptDebug->uCheckedRevision == uRevision && pl__scene_tools_path_settings_equal(&tSettings, &ptDebug->tCheckedSettings) && ptDebug->bStartInGrid && ptDebug->bGoalInGrid)
    {
        plVec3 tStart = gptVoxel->coord_to_world(ptGrid, ptDebug->tStartCell);
        plVec3 tGoal = gptVoxel->coord_to_world(ptGrid, ptDebug->tGoalCell);
        gptDraw->add_3d_line(ptDrawlist, tStart, tGoal, (plDrawLineOptions){.uColor = ptDebug->bSegmentClear ? PL_COLOR_32_GREEN : PL_COLOR_32_RED, .fThickness = 0.025f});
        gptDraw->add_3d_cross(ptDrawlist, tStart, ptGrid->fVoxelSize * 0.3f, (plDrawLineOptions){.uColor = PL_COLOR_32_GREEN, .fThickness = 0.03f});
        gptDraw->add_3d_cross(ptDrawlist, tGoal, ptGrid->fVoxelSize * 0.3f, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.03f});
    }
}

//-----------------------------------------------------------------------------
// [SECTION] terrain and lighting
//-----------------------------------------------------------------------------

static bool
pl__scene_tools_terrain_resident(const plTerrainChunk* ptChunk)
{
    return ptChunk->ptIndexHole && ptChunk->ptVertexHole && ptChunk->uIndexCount != 0;
}

static bool
pl__scene_tools_terrain_matches(const plTerrainChunk* ptChunk, const plTerrainDebug* ptDebug)
{
    return (!ptDebug->bResidentOnly || pl__scene_tools_terrain_resident(ptChunk)) && (ptDebug->iLevel < 0 || ptChunk->uLevel == ptDebug->iLevel);
}

static void
pl__scene_tools_terrain_stats(const plTerrainChunkFile* ptFile, plTerrainDebugStats* ptStats)
{
    for(uint32_t i = 0; i < ptFile->uChunkCount; i++)
    {
        const plTerrainChunk* ptChunk = &ptFile->atChunks[i];
        ptStats->uChunks++;
        ptStats->auLevels[ptChunk->uLevel]++;
        if(ptChunk->ptVertexHole)
            ptStats->uVertexBytes += ptChunk->ptVertexHole->uSize;
        if(ptChunk->ptIndexHole)
            ptStats->uIndexBytes += ptChunk->ptIndexHole->uSize;
        if(pl__scene_tools_terrain_resident(ptChunk))
        {
            ptStats->uResident++;
            ptStats->auResidentLevels[ptChunk->uLevel]++;
            ptStats->uTriangles += ptChunk->uIndexCount / 3;
        }
    }
}

static void
pl__scene_tools_draw_terrain(plDrawList3D* ptDrawlist, plDrawList3D* ptOverlay)
{
    plTerrainDebug* ptDebug = &gptSceneToolsCtx->tTerrainDebug;
    ptDebug->uDrawn = 0;
    if(!ptDebug->bShow || !ptDrawlist || !gptSceneToolsCtx->ptScene)
        return;
    const plTerrainChunkFile* ptFile = gptRenderer->get_terrain_chunk_file(gptSceneToolsCtx->ptScene, ptDebug->uFile);
    if(!ptFile)
        return;
    if(ptDebug->bXray && ptOverlay)
        ptDrawlist = ptOverlay;
    for(uint32_t i = 0; ptDebug->bBounds && i < ptFile->uChunkCount && ptDebug->uDrawn < ptDebug->uMaxBoxes; i++)
    {
        const plTerrainChunk* ptChunk = &ptFile->atChunks[i];
        if(!pl__scene_tools_terrain_matches(ptChunk, ptDebug) || (ptDebug->bHighlight && i == ptDebug->uChunk))
            continue;
        gptDraw->add_3d_aabb(ptDrawlist, ptChunk->tMinBound, ptChunk->tMaxBound, (plDrawLineOptions){.uColor = pl__scene_tools_terrain_resident(ptChunk) ? PL_COLOR_32_GREEN : PL_COLOR_32_RED, .fThickness = ptDebug->fLineWidth});
        ptDebug->uDrawn++;
    }
    // Selection remains visible even when excluded by the list/LOD filter.
    if(ptDebug->bHighlight && ptDebug->uChunk < ptFile->uChunkCount)
    {
        const plTerrainChunk* ptChunk = &ptFile->atChunks[ptDebug->uChunk];
        gptDraw->add_3d_aabb(ptDrawlist, ptChunk->tMinBound, ptChunk->tMaxBound, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = ptDebug->fLineWidth * 2.0f});
    }
}

static void
pl__scene_tools_terrain_chunk(const plTerrainChunkFile* ptFile, plTerrainDebug* ptDebug)
{
    gptUI->separator_text("Inspected chunk");
    if(gptUI->button("Inspect root") && ptFile->uChunkCount)
        ptDebug->uChunk = 0;
    if(ptDebug->uChunk >= ptFile->uChunkCount)
    {
        gptUI->text("Select a chunk to inspect its bounds and LOD relationships.");
        return;
    }
    const plTerrainChunk* ptChunk = &ptFile->atChunks[ptDebug->uChunk];
    gptUI->text("Chunk %u / level %u / %s", ptDebug->uChunk, (uint32_t)ptChunk->uLevel, pl__scene_tools_terrain_resident(ptChunk) ? "resident" : "not resident");
    gptUI->text("Min: %g, %g, %g", ptChunk->tMinBound.x, ptChunk->tMinBound.y, ptChunk->tMinBound.z);
    gptUI->text("Max: %g, %g, %g", ptChunk->tMaxBound.x, ptChunk->tMaxBound.y, ptChunk->tMaxBound.z);
    plVec3 tSize = pl_sub_vec3(ptChunk->tMaxBound, ptChunk->tMinBound);
    gptUI->text("Size: %g, %g, %g", tSize.x, tSize.y, tSize.z);
    gptUI->text("Geometric error: %g", ptFile->fMaxBaseError * ptChunk->uLevel);
    gptUI->text("Resident triangles: %u", pl__scene_tools_terrain_resident(ptChunk) ? ptChunk->uIndexCount / 3 : 0);
    gptUI->text("Allocated: %.2f KiB vertices / %.2f KiB indices", ptChunk->ptVertexHole ? (double)ptChunk->ptVertexHole->uSize / 1024.0 : 0.0, ptChunk->ptIndexHole ? (double)ptChunk->ptIndexHole->uSize / 1024.0 : 0.0);
    gptUI->text("Last used frame: %" PRIu64 " (includes renderer passes)", ptChunk->uLastFrameUsed);
    if(ptChunk->ptParent && gptUI->button("Inspect parent"))
        ptDebug->uChunk = ptChunk->ptParent->uIndex;
    uint32_t uChildren = 0;
    uint32_t uResidentChildren = 0;
    for(uint32_t i = 0; i < 4; i++)
    {
        if(!ptChunk->aptChildren[i])
            continue;
        uChildren++;
        uResidentChildren += pl__scene_tools_terrain_resident(ptChunk->aptChildren[i]) ? 1 : 0;
        char acLabel[64];
        pl_snprintf(acLabel, sizeof(acLabel), "Inspect child %u", i);
        if(gptUI->button(acLabel))
            ptDebug->uChunk = ptChunk->aptChildren[i]->uIndex;
    }
    if(uChildren)
        gptUI->text("Children resident: %u / %u; %s", uResidentChildren, uChildren, uResidentChildren == 4 ? "ready for finer LOD" : "finer LOD needs child residency");
    else
        gptUI->text("Leaf chunk (finest available LOD).");
}

static void
pl__scene_tools_terrain(plScene* ptScene)
{
    plTerrainDebug* ptDebug = &gptSceneToolsCtx->tTerrainDebug;
    gptUI->text("Edit terrain assets and components in Assets / ECS.");
    const plTerrainChunkFile* ptFile = ptScene ? gptRenderer->get_terrain_chunk_file(ptScene, 0) : NULL;
    if(!ptFile)
    {
        gptUI->text("No terrain loaded in this scene.");
        return;
    }
    plTerrainDebugStats tTotal = {0};
    uint32_t uFileCount = 0;
    for(; ptFile; ptFile = gptRenderer->get_terrain_chunk_file(ptScene, ++uFileCount))
        pl__scene_tools_terrain_stats(ptFile, &tTotal);
    gptUI->text("%u tiles / %" PRIu64 " chunks / %" PRIu64 " resident", uFileCount, tTotal.uChunks, tTotal.uResident);
    gptUI->text("Allocated geometry: %.2f MiB vertices / %.2f MiB indices", (double)tTotal.uVertexBytes / 1048576.0, (double)tTotal.uIndexBytes / 1048576.0);
    gptUI->text("Resident triangles: %" PRIu64, tTotal.uTriangles);
    gptUI->text("Resident totals include overlapping LODs, not just rendered chunks.");
    if(ptDebug->uFile >= uFileCount)
    {
        ptDebug->uFile = 0;
        ptDebug->uChunk = UINT32_MAX;
        ptDebug->uPage = 0;
    }
    ptFile = gptRenderer->get_terrain_chunk_file(ptScene, ptDebug->uFile);
    if(gptUI->begin_combo("Tile", ptFile->acFile, PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
    {
        for(uint32_t i = 0; i < uFileCount; i++)
        {
            const plTerrainChunkFile* ptOption = gptRenderer->get_terrain_chunk_file(ptScene, i);
            bool bSelected = ptDebug->uFile == i;
            gptUI->push_id_uint(i);
            if(gptUI->selectable(ptOption->acFile, &bSelected, 0))
            {
                ptDebug->uFile = i;
                ptDebug->uChunk = UINT32_MAX;
                ptDebug->uPage = 0;
                ptDebug->iLevel = -1;
                gptUI->close_current_popup();
            }
            gptUI->pop_id();
        }
        gptUI->end_combo();
    }
    ptFile = gptRenderer->get_terrain_chunk_file(ptScene, ptDebug->uFile);
    plTerrainDebugStats tStats = {0};
    pl__scene_tools_terrain_stats(ptFile, &tStats);
    gptUI->text("Tree depth: %d / base error: %g", ptFile->iTreeDepth, ptFile->fMaxBaseError);
    gptUI->text("Tile: %" PRIu64 " / %" PRIu64 " resident / %" PRIu64 " triangles", tStats.uResident, tStats.uChunks, tStats.uTriangles);
    if(gptUI->collapsing_header("LOD residency", 0))
    {
        gptUI->text("Level 0 is finest. Higher levels are coarser.");
        for(uint32_t i = 0; i < PL_ARRAYSIZE(tStats.auLevels); i++)
        {
            if(tStats.auLevels[i])
                gptUI->text("Level %u: %u / %u resident", i, tStats.auResidentLevels[i], tStats.auLevels[i]);
        }
    }
    gptUI->separator_text("Viewport bounds");
    gptUI->checkbox("Show filtered chunk bounds", &ptDebug->bBounds);
    gptUI->checkbox("Highlight inspected chunk", &ptDebug->bHighlight);
    gptUI->checkbox("Draw terrain bounds through geometry", &ptDebug->bXray);
    gptUI->input_float("Bounds line width", &ptDebug->fLineWidth, "%g", 0);
    ptDebug->fLineWidth = pl_maxf(0.001f, ptDebug->fLineWidth);
    gptUI->slider_uint("Chunk box limit", &ptDebug->uMaxBoxes, 0, 4096, 0);
    gptUI->text("Green: resident / red: not resident / yellow: inspected");
    gptUI->text("Last draw: %u filtered boxes (plus inspected chunk)", ptDebug->uDrawn);

    pl__scene_tools_terrain_chunk(ptFile, ptDebug);
    gptUI->separator_text("Chunk browser");
    bool bChanged = gptUI->checkbox("Resident chunks only", &ptDebug->bResidentOnly);
    int iMaxLevel = 0;
    for(int i = 0; i < 256; i++)
    {
        if(tStats.auLevels[i])
            iMaxLevel = i;
    }
    if(ptDebug->iLevel > iMaxLevel)
        ptDebug->iLevel = -1;
    bChanged |= gptUI->slider_int("Level (-1 = all)", &ptDebug->iLevel, -1, iMaxLevel, 0);
    if(bChanged)
        ptDebug->uPage = 0;
    uint32_t uMatches = 0;
    for(uint32_t i = 0; i < ptFile->uChunkCount; i++)
        uMatches += pl__scene_tools_terrain_matches(&ptFile->atChunks[i], ptDebug) ? 1 : 0;
    uint32_t uLastPage = uMatches ? (uMatches - 1) / 16 : 0;
    if(ptDebug->uPage > uLastPage)
        ptDebug->uPage = uLastPage;
    gptUI->text("%u matches / page %u of %u (16 per page)", uMatches, ptDebug->uPage + 1, uLastPage + 1);
    if(gptUI->button("Previous page") && ptDebug->uPage)
        ptDebug->uPage--;
    if(gptUI->button("Next page") && ptDebug->uPage < uLastPage)
        ptDebug->uPage++;
    uint32_t uMatch = 0;
    uint32_t uFirst = ptDebug->uPage * 16;
    for(uint32_t i = 0; i < ptFile->uChunkCount && uMatch < uFirst + 16; i++)
    {
        const plTerrainChunk* ptChunk = &ptFile->atChunks[i];
        if(!pl__scene_tools_terrain_matches(ptChunk, ptDebug))
            continue;
        if(uMatch++ < uFirst)
            continue;
        char acLabel[128];
        pl_snprintf(acLabel, sizeof(acLabel), "Chunk %u / L%u / %g, %g / %s", i, (uint32_t)ptChunk->uLevel, ptChunk->fX, ptChunk->fY, pl__scene_tools_terrain_resident(ptChunk) ? "resident" : "not resident");
        bool bSelected = ptDebug->uChunk == i;
        if(gptUI->selectable(acLabel, &bSelected, 0))
            ptDebug->uChunk = i;
    }
}

static void
pl__scene_tools_lighting(plScene* ptScene)
{
    gptUI->text("Edit lights, probes and environment settings in Assets / ECS.");
    if(!ptScene)
    {
        gptUI->text("No renderer scene available.");
        return;
    }
    if(gptUI->button("Update Environment"))
        gptRenderer->set_scene_flags(ptScene, gptRenderer->get_scene_flags(ptScene) | PL_RENDERER_SCENE_FLAGS_ALL_PROBES_DIRTY | PL_RENDERER_SCENE_FLAGS_SKY_LUTS_DIRTY);
    gptUI->text("Refreshes scene probes and sky lookup tables.");
}

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_scene_tools_cleanup(void)
{
    if(gptSceneToolsCtx->ptVoxelDrawlist)
        gptDraw->return_3d_drawlist(gptSceneToolsCtx->ptVoxelDrawlist);
    *gptSceneToolsCtx = (plSceneToolsContext){0};
}

void
pl_scene_tools_draw(const plComponentLibrary* ptLibrary, plView* ptView)
{
    if(gptSceneToolsCtx->ptLibrary != ptLibrary || !ptView)
        return;
    plDrawList3D* ptDrawlist = gptRenderer->get_drawlist(ptView);
    plDrawList3D* ptOverlay = gptRenderer->get_gizmo_drawlist(ptView);
    pl__scene_tools_draw_terrain(ptDrawlist, ptOverlay);
    pl__scene_tools_draw_navigation(ptLibrary, ptView, ptDrawlist, ptOverlay);
}

void
pl_scene_tools_run(plComponentLibrary* ptLibrary, plScene* ptScene, plEntity tSelected, bool* pbOpen)
{
    if(gptSceneToolsCtx->ptLibrary != ptLibrary || gptSceneToolsCtx->ptScene != ptScene)
    {
        pl_scene_tools_cleanup();
        gptSceneToolsCtx->ptLibrary = ptLibrary;
        gptSceneToolsCtx->ptScene = ptScene;
        gptSceneToolsCtx->tTerrainDebug = (plTerrainDebug){
            .bHighlight    = true,
            .bXray         = true,
            .bResidentOnly = true,
            .fLineWidth    = 2.0f,
            .iLevel        = -1,
            .uChunk        = UINT32_MAX,
            .uMaxBoxes     = 512
        };
    }
    gptSceneToolsCtx->tNavigationDebug.bShow = false;
    gptSceneToolsCtx->tTerrainDebug.bShow = false;
    if(!pbOpen || !*pbOpen)
        return;
    if(!gptUI->begin_window("Scene Tools", pbOpen, 0))
        return;
    gptUI->layout_dynamic(0, 1);
    if(!ptLibrary)
        gptUI->text("Load a scene to use scene tools.");
    else
    {
        static const char* apcTools[] = {"Navigation", "Terrain", "Lighting"};
        if(gptUI->begin_combo("Tool", apcTools[gptSceneToolsCtx->iTool], PL_UI_COMBO_FLAGS_HEIGHT_REGULAR))
        {
            for(int i = 0; i < 3; i++)
            {
                bool bSelected = gptSceneToolsCtx->iTool == i;
                if(gptUI->selectable(apcTools[i], &bSelected, 0))
                {
                    gptSceneToolsCtx->iTool = i;
                    gptUI->close_current_popup();
                }
            }
            gptUI->end_combo();
        }
        gptUI->separator_text(apcTools[gptSceneToolsCtx->iTool]);
        switch(gptSceneToolsCtx->iTool)
        {
            case 0:
                if(pl__scene_tools_navigation_available())
                    pl__scene_tools_navigation(ptLibrary, tSelected, pbOpen);
                else
                    gptUI->text("Navigation components are not registered.");
                break;
            case 1:
                pl__scene_tools_terrain(ptScene);
                gptSceneToolsCtx->tTerrainDebug.bShow = *pbOpen;
                break;
            case 2:
                pl__scene_tools_lighting(ptScene);
                break;
        }
    }
    gptUI->end_window();
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_scene_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plSceneToolsI tApi = {
        .run     = pl_scene_tools_run,
        .draw    = pl_scene_tools_draw,
        .cleanup = pl_scene_tools_cleanup
    };
    pl_set_api(ptApiRegistry, plSceneToolsI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptUI       = pl_get_api_latest(ptApiRegistry, plUiI);
        gptEcs      = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptDraw     = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptRenderer = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptVoxel    = pl_get_api_latest(ptApiRegistry, plVoxelI);
        gptPath     = pl_get_api_latest(ptApiRegistry, plPathI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    if(bReload)
        gptSceneToolsCtx = ptDataRegistry->get_data("plSceneToolsContext");
    else
    {
        static plSceneToolsContext tContext = {0};
        gptSceneToolsCtx = &tContext;
        ptDataRegistry->set_data("plSceneToolsContext", gptSceneToolsCtx);
    }
}

PL_EXPORT void
pl_unload_scene_tools_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(!bReload)
        ptApiRegistry->remove_api(pl_get_api_latest(ptApiRegistry, plSceneToolsI));
}

#ifndef PL_UNITY_BUILD

    #define PL_STRING_IMPLEMENTATION
    #include "pl_string.h"
    #undef PL_STRING_IMPLEMENTATION

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"
    #undef PL_MEMORY_IMPLEMENTATION

    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif

#endif
