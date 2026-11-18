/*
   pl_path_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data
// [SECTION] structs
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_path_ext.h"
#include "pl_draw_ext.h"
#include "pl_voxel_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI* gptMemory = NULL;
    static const plVoxelI*  gptVoxel  = NULL;
    static const plDrawI*   gptDraw   = NULL;

    #define PL_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] internal enums
//-----------------------------------------------------------------------------

typedef int plPathPhase;

enum _plPathPhase
{
    PL_PATH_PHASE_SEARCH = 0,
    PL_PATH_PHASE_TRACE,
    PL_PATH_PHASE_BUILD,
    PL_PATH_PHASE_SMOOTH,
    PL_PATH_PHASE_VISIBILITY
};

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plPathNode
{
    uint32_t uCell;
    uint32_t uParent;
    uint32_t uHeap;
    float    fCost;
    float    fScore;
} plPathNode;

typedef struct _plPathLine
{
    int    aiC[3];
    int    aiEnd[3];
    int    aiStep[3];
    double adNext[3];
    double adDelta[3];
    bool   bDone;
    bool   bVisible;
} plPathLine;

typedef struct _plPathWork
{
    plPathNode*        atNodes;
    uint32_t*          auHeap;
    plHashMapStatic32  tHash;
    uint32_t           uHeapCount;
    uint32_t           uCount;
    uint32_t           uLimit;
    const plVoxelGrid* ptGrid;
    plPathSettings     tSettings;
    plVoxelCoord       tFrom;
    plVoxelCoord       tTo;
    plVoxelCoord       tPrevious;
    uint32_t           uGoal;
    uint32_t           uTrace;
    uint32_t           uRoute;
    uint32_t           uRawCount;
    uint32_t           uPathCount;
    uint32_t           uAnchor;
    uint32_t           uNext;
    uint32_t           uCandidate;
    float              fLength;
    plPathPhase        tPhase;
    plPathLine         tLine;
    bool               bLineActive;
} plPathWork;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline bool
pl__path_settings_valid(const plVoxelGrid* ptGrid, const plPathSettings* ptSettings)
{
    return ptGrid && ptGrid->_puBits && ptSettings && ptSettings->iAgentHeight >= 1 &&
        (uint32_t)ptSettings->iAgentHeight <= ptGrid->auResolution[1] && ptSettings->iAgentRadius >= 0 &&
        ptSettings->iAgentRadius <= 1024 && ptSettings->iMaxStep >= 0 && ptSettings->iMaxStep <= 64 && ptSettings->iSnapRadius >= 0 && ptSettings->iSnapRadius <= 32 &&
        ptSettings->uMaxNodes >= 1 && ptSettings->uMaxNodes <= 1048576 && (ptSettings->tGroundSupport == PL_PATH_GROUND_SUPPORT_FULL || ptSettings->tGroundSupport == PL_PATH_GROUND_SUPPORT_CENTER);
}

static inline uint32_t
pl__path_index(const plVoxelGrid* ptGrid, plVoxelCoord tC)
{
    return ((uint32_t)tC.z * ptGrid->auResolution[1] + (uint32_t)tC.y) * ptGrid->auResolution[0] + (uint32_t)tC.x;
}

static inline plVoxelCoord
pl__path_coord(const plVoxelGrid* ptGrid, uint32_t uIndex)
{
    plVoxelCoord tC = {0};
    tC.x = (int)(uIndex % ptGrid->auResolution[0]);
    uIndex /= ptGrid->auResolution[0];
    tC.y = (int)(uIndex % ptGrid->auResolution[1]);
    tC.z = (int)(uIndex / ptGrid->auResolution[1]);
    return tC;
}

static inline bool
pl__path_less(const plPathWork* ptWork, uint32_t a, uint32_t b)
{
    const plPathNode* p = &ptWork->atNodes[a];
    const plPathNode* q = &ptWork->atNodes[b];
    return p->fScore < q->fScore || (p->fScore == q->fScore && p->fCost > q->fCost);
}

static inline plPathStatus
pl__path_finish(plPathQuery* ptQuery, plPathStatus tStatus)
{
    if(ptQuery->_pScratch)
    {
        ((plPathWork*)ptQuery->_pScratch)->ptGrid = NULL;
    }
    return ptQuery->tStatus = tStatus;
}

static bool     pl__path_visible   (const plVoxelGrid*, plVoxelCoord, plVoxelCoord, const plPathSettings*);
static bool     pl__path_clear     (const plVoxelGrid*, plVoxelCoord, const plPathSettings*);
static bool     pl__path_walkable  (const plVoxelGrid*, plVoxelCoord, const plPathSettings*);
static bool     pl__path_snap      (const plVoxelGrid*, plVec3, const plPathSettings*, plVoxelCoord*);
static float    pl__path_distance  (plVoxelCoord tA, plVoxelCoord tB, const plPathSettings*);
static void     pl__path_heap_up   (plPathWork*, uint32_t);
static uint32_t pl__path_heap_pop  (plPathWork*);
static bool     pl__path_step_clear(const plVoxelGrid*, plVoxelCoord, plVoxelCoord, const plPathSettings*);
static void     pl__path_line_begin(plPathLine*, const plVoxelGrid*, plVoxelCoord, plVoxelCoord, const plPathSettings*);
static void     pl__path_line_step (plPathLine*, const plVoxelGrid*, const plPathSettings*);
static bool     pl__path_visible   (const plVoxelGrid*, plVoxelCoord, plVoxelCoord, const plPathSettings*);
static bool     pl__path_allocate  (plPathQuery*, uint32_t);
static void     pl__path_expand    (plPathQuery*, plPathWork*);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plPathSettings
pl_path_default_settings(void)
{
    return (plPathSettings){
        .bSimplify      = true,
        .iAgentHeight   = 2,
        .iMaxStep       = 1,
        .iSnapRadius    = 0,
        .uMaxNodes      = 65536,
        .bAllowDiagonal = true,
        .tGroundSupport = PL_PATH_GROUND_SUPPORT_FULL
    };
}

bool
pl_path_is_walkable(const plVoxelGrid* ptGrid, plVoxelCoord tC, const plPathSettings* ptSettings)
{
    plPathSettings tSettings = ptSettings ? *ptSettings : pl_path_default_settings();
    return pl__path_settings_valid(ptGrid, &tSettings) && pl__path_walkable(ptGrid, tC, &tSettings);
}

void
pl_path_cleanup(plPathQuery* ptQuery)
{
    if(!ptQuery)
        return;
    
    if(ptQuery->_pScratch)
    {
        PL_FREE(ptQuery->_pScratch);
    }

    memset(ptQuery,0,sizeof(*ptQuery));
}

bool
pl_path_is_segment_traversable(const plVoxelGrid* ptGrid, plVoxelCoord tStart, plVoxelCoord tGoal, const plPathSettings* ptSettings)
{
    plPathSettings tSettings = ptSettings ? *ptSettings : pl_path_default_settings();
    return pl__path_settings_valid(ptGrid, &tSettings) && pl__path_visible(ptGrid, tStart, tGoal, &tSettings);
}

void
pl_path_cancel(plPathQuery* ptQuery)
{
    if(!ptQuery)
        return;
    ptQuery->uWaypointCount = 0;
    ptQuery->uRawWaypointCount = 0;
    ptQuery->fLength = 0;
    ptQuery->tStart = (plVec3){0};
    ptQuery->tGoal = (plVec3){0};
    pl__path_finish(ptQuery, PL_PATH_STATUS_CANCELLED);
}

plPathStatus
pl_path_begin(plPathQuery* ptQuery, const plVoxelGrid* ptGrid, plVec3 tStart, plVec3 tGoal, const plPathSettings* ptInput)
{
    if(!ptQuery)
        return PL_PATH_STATUS_INVALID_INPUT;
    
    pl_path_cancel(ptQuery);
    ptQuery->uVisitedNodes = 0;
    ptQuery->uExpandedNodes = 0;
    ptQuery->tStatus = PL_PATH_STATUS_INVALID_INPUT;
    plPathSettings tSettings = ptInput ? *ptInput : pl_path_default_settings();

    if(!pl__path_settings_valid(ptGrid, &tSettings))
        return ptQuery->tStatus;

    plVoxelCoord tFrom = {0};
    plVoxelCoord tTo = {0};

    if(!pl__path_snap(ptGrid, tStart, &tSettings, &tFrom))
        return ptQuery->tStatus = PL_PATH_STATUS_INVALID_START;

    if(!pl__path_snap(ptGrid, tGoal, &tSettings, &tTo))
        return ptQuery->tStatus = PL_PATH_STATUS_INVALID_GOAL;

    uint32_t uLimit = tSettings.uMaxNodes < ptGrid->uCellCount ? tSettings.uMaxNodes : ptGrid->uCellCount;

    if(!pl__path_allocate(ptQuery, uLimit))
        return ptQuery->tStatus = PL_PATH_STATUS_OUT_OF_MEMORY;
    
    plPathWork* ptWork = ptQuery->_pScratch;
    ptWork->ptGrid = ptGrid;
    ptWork->tSettings = tSettings;
    ptWork->tFrom = tFrom;
    ptWork->tPrevious = tFrom;
    ptWork->tTo = tTo;
    ptWork->uGoal = pl__path_index(ptGrid, tTo);
    uint32_t uStart = pl__path_index(ptGrid, tFrom);
    ptWork->atNodes[0] = (plPathNode){uStart, UINT32_MAX, 0, 0, pl__path_distance(tFrom, tTo, &tSettings)};
    pl_hms_set32(&ptWork->tHash, (uint32_t)(uStart * UINT32_C(2654435761)), 0);
    ptWork->auHeap[0] = 0;
    ptWork->uCount = 1;
    ptQuery->uVisitedNodes = 1;
    ptWork->uHeapCount = 1;
    return ptQuery->tStatus = PL_PATH_STATUS_IN_PROGRESS;
}

plPathStatus
pl_path_step(plPathQuery* ptQuery, uint32_t uMaxWork)
{
    if(!ptQuery)
        return PL_PATH_STATUS_INVALID_INPUT;

    if(ptQuery->tStatus != PL_PATH_STATUS_IN_PROGRESS)
        return ptQuery->tStatus;

    plPathWork* ptWork = ptQuery->_pScratch;
    const plVoxelGrid* ptGrid = ptWork->ptGrid;
    const plPathSettings* ptSettings = &ptWork->tSettings;
    for(uint32_t uWork = 0; uWork < uMaxWork && ptQuery->tStatus == PL_PATH_STATUS_IN_PROGRESS; uWork++)
    {
        switch(ptWork->tPhase)
        {
            case PL_PATH_PHASE_SEARCH:
                pl__path_expand(ptQuery, ptWork);
                break;

            case PL_PATH_PHASE_TRACE:

                // Reuse heap storage to reverse one parent link per unit.
                ptWork->auHeap[ptWork->uRoute++] = ptWork->uTrace;
                ptWork->uTrace = ptWork->atNodes[ptWork->uTrace].uParent;
                if(ptWork->uTrace == UINT32_MAX)
                    ptWork->tPhase = PL_PATH_PHASE_BUILD;
                break;

            case PL_PATH_PHASE_BUILD:
            {
                plVoxelCoord tC = pl__path_coord(ptGrid, ptWork->atNodes[ptWork->auHeap[--ptWork->uRoute]].uCell);
                plVoxelCoord tPrevious = ptWork->tPrevious;
                if(!ptSettings->bFlying && tC.y != tPrevious.y)
                {
                    plVoxelCoord tElbow = tC.y > tPrevious.y ? (plVoxelCoord){tPrevious.x, tC.y, tPrevious.z} : (plVoxelCoord){tC.x, tPrevious.y, tC.z};
                    ptQuery->atRawWaypoints[ptWork->uRawCount++] = gptVoxel->coord_to_world(ptGrid, tElbow);
                }
                ptQuery->atRawWaypoints[ptWork->uRawCount++] = gptVoxel->coord_to_world(ptGrid, tC);
                ptWork->tPrevious = tC;
                if(!ptWork->uRoute)
                    ptWork->tPhase = PL_PATH_PHASE_SMOOTH;
                break;
            }

            case PL_PATH_PHASE_SMOOTH:
            {
                plVec3 tP = ptQuery->atRawWaypoints[ptWork->uAnchor];
                if(ptWork->uPathCount)
                    ptWork->fLength += pl_length_vec3(pl_sub_vec3(tP, ptQuery->atWaypoints[ptWork->uPathCount - 1]));
                ptQuery->atWaypoints[ptWork->uPathCount++] = tP;
                if(ptWork->uAnchor + 1 == ptWork->uRawCount)
                {
                    ptQuery->uWaypointCount = ptWork->uPathCount;
                    ptQuery->uRawWaypointCount = ptWork->uRawCount;
                    ptQuery->fLength = ptWork->fLength;
                    ptQuery->tStart = ptQuery->atWaypoints[0];
                    ptQuery->tGoal = tP;
                    pl__path_finish(ptQuery, PL_PATH_STATUS_SUCCESS);
                    break;
                }
                ptWork->uNext = ptWork->uAnchor + 1;
                ptWork->uCandidate = ptWork->uAnchor + 2;
                ptWork->tPhase = PL_PATH_PHASE_VISIBILITY;
                break;
            }
            case PL_PATH_PHASE_VISIBILITY:
                if(!ptSettings->bSimplify || ptWork->uCandidate >= ptWork->uRawCount)
                {
                    ptWork->uAnchor = ptWork->uNext;
                    ptWork->tPhase = PL_PATH_PHASE_SMOOTH;
                    break;
                }
                if(!ptWork->bLineActive)
                {
                    plVoxelCoord tA, tB;
                    gptVoxel->world_to_coord(ptGrid, ptQuery->atRawWaypoints[ptWork->uAnchor], &tA);
                    gptVoxel->world_to_coord(ptGrid, ptQuery->atRawWaypoints[ptWork->uCandidate], &tB);
                    pl__path_line_begin(&ptWork->tLine, ptGrid, tA, tB, ptSettings);
                    ptWork->bLineActive = true;
                }
                pl__path_line_step(&ptWork->tLine, ptGrid, ptSettings);
                if(ptWork->tLine.bDone)
                {
                    ptWork->bLineActive = false;
                    if(ptWork->tLine.bVisible)
                        ptWork->uNext = ptWork->uCandidate++;
                    else
                    {
                        ptWork->uAnchor = ptWork->uNext;
                        ptWork->tPhase = PL_PATH_PHASE_SMOOTH;
                    }
                }
                break;
        }
    }
    return ptQuery->tStatus;
}

plPathStatus
pl_path_process(plPathQuery* ptQuery, const plVoxelGrid* ptGrid, plVec3 tStart, plVec3 tGoal, const plPathSettings* ptSettings)
{
    plPathStatus tStatus = pl_path_begin(ptQuery, ptGrid, tStart, tGoal, ptSettings);
    while(tStatus == PL_PATH_STATUS_IN_PROGRESS)
        tStatus = pl_path_step(ptQuery, UINT32_MAX);
    return tStatus;
}

const char*
pl_path_status_string(plPathStatus tStatus)
{
    switch(tStatus)
    {
        case PL_PATH_STATUS_NONE:          return "Not queried";
        case PL_PATH_STATUS_SUCCESS:       return "Path found";
        case PL_PATH_STATUS_NO_PATH:       return "No path";
        case PL_PATH_STATUS_INVALID_START: return "Start has no valid cell";
        case PL_PATH_STATUS_INVALID_GOAL:  return "Goal has no valid cell";
        case PL_PATH_STATUS_NODE_LIMIT:    return "Node budget exhausted";
        case PL_PATH_STATUS_OUT_OF_MEMORY: return "Allocation failed";
        case PL_PATH_STATUS_IN_PROGRESS:   return "Searching";
        case PL_PATH_STATUS_CANCELLED:     return "Cancelled";
        default:                           return "Invalid input/settings";
    }
}

void
pl_path_debug_draw(const plPathQuery* ptQuery, plDrawList3D* ptDrawlist, bool bRaw, bool bWaypoints)
{
    if(!ptQuery || !ptDrawlist || !gptDraw->add_3d_line || ptQuery->tStatus != PL_PATH_STATUS_SUCCESS)
        return;

    if(bRaw)
    {
        for(uint32_t i = 1; i < ptQuery->uRawWaypointCount; i++)
            gptDraw->add_3d_line(ptDrawlist, ptQuery->atRawWaypoints[i - 1], ptQuery->atRawWaypoints[i], (plDrawLineOptions){.uColor = PL_COLOR_32_CYAN, .fThickness = 0.04f});
    }
    for(uint32_t i = 0; i < ptQuery->uWaypointCount; i++)
    {
        if(i)
            gptDraw->add_3d_line(ptDrawlist, ptQuery->atWaypoints[i - 1], ptQuery->atWaypoints[i], (plDrawLineOptions){.uColor = PL_COLOR_32_MAGENTA, .fThickness = 0.10f});
        if(bWaypoints)
            gptDraw->add_3d_cross(ptDrawlist, ptQuery->atWaypoints[i], 0.15f, (plDrawLineOptions){.uColor = PL_COLOR_32_MAGENTA, .fThickness = 0.06f});
    }
    gptDraw->add_3d_cross(ptDrawlist,ptQuery->tStart, 0.5f, (plDrawLineOptions){.uColor = PL_COLOR_32_GREEN, .fThickness = 0.08f});
    gptDraw->add_3d_cross(ptDrawlist,ptQuery->tGoal, 0.5f, (plDrawLineOptions){.uColor = PL_COLOR_32_RED, .fThickness = 0.08f});
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static bool
pl__path_clear(const plVoxelGrid* ptGrid, plVoxelCoord tC, const plPathSettings* ptSettings)
{
    const int r = ptSettings->iAgentRadius;

    // Validate before signed additions; boundaries are solid for navigation.
    if(!gptVoxel->is_valid(ptGrid, tC) || tC.x < r || tC.z < r || (uint32_t)r >= ptGrid->auResolution[0] - (uint32_t)tC.x || (uint32_t)r >= ptGrid->auResolution[2] - (uint32_t)tC.z || (uint32_t)ptSettings->iAgentHeight > ptGrid->auResolution[1] - (uint32_t)tC.y)
        return false;

    for(int z = tC.z - r; z <= tC.z + r; z++)
    {
        for(int y = tC.y; y < tC.y + ptSettings->iAgentHeight; y++)
        {
            for(int x = tC.x - r; x <= tC.x + r; x++)
            {
                if(gptVoxel->check(ptGrid, (plVoxelCoord){x, y, z}))
                    return false;
            }
        }
    }
    return true;
}

static bool
pl__path_walkable(const plVoxelGrid* ptGrid, plVoxelCoord tC, const plPathSettings* ptSettings)
{
    if(!pl__path_clear(ptGrid, tC, ptSettings))
        return false;
    
    if(ptSettings->bFlying)
        return true;

    if(tC.y == 0)
        return false;

    if(ptSettings->tGroundSupport == PL_PATH_GROUND_SUPPORT_CENTER)
        return gptVoxel->check(ptGrid, (plVoxelCoord){tC.x, tC.y - 1, tC.z});

    for(int z = tC.z - ptSettings->iAgentRadius; z <= tC.z + ptSettings->iAgentRadius; z++)
    {
        for(int x = tC.x - ptSettings->iAgentRadius; x <= tC.x + ptSettings->iAgentRadius; x++)
        {
            if(!gptVoxel->check(ptGrid, (plVoxelCoord){x, tC.y - 1, z}))
                return false;
        }
    }
    return true;
}

static bool
pl__path_snap(const plVoxelGrid* ptGrid, plVec3 tPosition, const plPathSettings* ptSettings, plVoxelCoord* ptOut)
{
    plVoxelCoord tC;
    if(!gptVoxel->world_to_coord(ptGrid, tPosition, &tC))
        return false;

    if(pl__path_walkable(ptGrid, tC, ptSettings))
    {
        *ptOut = tC;
        return true;
    }
    double dBest = DBL_MAX;
    bool bFound = false;
    int r = ptSettings->iSnapRadius;

    // Clip ranges before adding the radius to prevent overflow on large grids.
    int x0 = tC.x > r ? tC.x - r : 0;
    int y0 = tC.y > r ? tC.y - r : 0;
    int z0 = tC.z > r ? tC.z - r : 0;
    int x1 = tC.x + (int)fmin(r, ptGrid->auResolution[0] - 1u - (uint32_t)tC.x);
    int y1 = tC.y + (int)fmin(r, ptGrid->auResolution[1] - 1u - (uint32_t)tC.y);
    int z1 = tC.z + (int)fmin(r, ptGrid->auResolution[2] - 1u - (uint32_t)tC.z);

    for(int z = z0; z <= z1; z++)
    {
        for(int y = y0; y <= y1; y++)
        {
            for(int x = x0; x <= x1; x++)
            {
                plVoxelCoord tN = {x, y, z};
                
                if(!pl__path_walkable(ptGrid, tN, ptSettings))
                    continue;

                plVec3 tP = gptVoxel->coord_to_world(ptGrid, tN);

                double dx = (double)tP.x - tPosition.x;
                double dy = (double)tP.y - tPosition.y;
                double dz = (double)tP.z - tPosition.z;
                double dDistance = dx * dx + dy * dy + dz * dz;
                if(dDistance < dBest)
                {
                    dBest = dDistance;
                    *ptOut = tN;
                    bFound = true;
                }
            }
        }
    }
    return bFound;
}

// Exact obstacle-free metrics: Manhattan, horizontal octile + step height,
// or 3D diagonal distance. Each is consistent with its respective edge costs.
static float
pl__path_distance(plVoxelCoord tA, plVoxelCoord tB, const plPathSettings* ptSettings)
{
    double x = fabs((double)tA.x - tB.x);
    double y = fabs((double)tA.y - tB.y);
    double z = fabs((double)tA.z - tB.z);

    if(!ptSettings->bAllowDiagonal)
        return (float)(x + y + z);

    if(!ptSettings->bFlying)
        return (float)(fmax(x, z) + (sqrt(2.0) - 1.0) * fmin(x, z) + y);

    double lo = fmin(x, fmin(y, z));
    double hi = fmax(x, fmax(y, z));
    double mid = x + y + z - lo - hi;
    return (float)(hi + (sqrt(2.0) - 1.0) * mid + (sqrt(3.0) - sqrt(2.0)) * lo);
}

static void
pl__path_heap_up(plPathWork* ptWork, uint32_t i)
{
    uint32_t uNode = ptWork->auHeap[i];
    while(i > 0)
    {
        uint32_t uParent = (i - 1) / 2;
        uint32_t uOther = ptWork->auHeap[uParent];
        if(!pl__path_less(ptWork, uNode, uOther))
            break;
        ptWork->auHeap[i] = uOther;
        ptWork->atNodes[uOther].uHeap = i;
        i = uParent;
    }
    ptWork->auHeap[i] = uNode;
    ptWork->atNodes[uNode].uHeap = i;
}

static uint32_t
pl__path_heap_pop(plPathWork* ptWork)
{
    uint32_t uResult = ptWork->auHeap[0];
    uint32_t uLast = ptWork->auHeap[--ptWork->uHeapCount];
    ptWork->atNodes[uResult].uHeap = UINT32_MAX;
    if(!ptWork->uHeapCount)
        return uResult;
    uint32_t i = 0;
    while(i * 2 + 1 < ptWork->uHeapCount)
    {
        uint32_t c = i * 2 + 1;
        if(c + 1 < ptWork->uHeapCount && pl__path_less(ptWork, ptWork->auHeap[c + 1], ptWork->auHeap[c]))
            c++;
        if(!pl__path_less(ptWork, ptWork->auHeap[c], uLast))
            break;
        ptWork->auHeap[i] = ptWork->auHeap[c];
        ptWork->atNodes[ptWork->auHeap[i]].uHeap = i;
        i = c;
    }
    ptWork->auHeap[i] = uLast;
    ptWork->atNodes[uLast].uHeap = i;
    return uResult;
}

static bool
pl__path_step_clear(const plVoxelGrid* ptGrid, plVoxelCoord tA, plVoxelCoord tB, const plPathSettings* ptSettings)
{
    if(ptSettings->bFlying)
        return pl__path_visible(ptGrid, tA, tB, ptSettings);

    if(!pl__path_walkable(ptGrid, tB, ptSettings))
        return false;

    int iHigh = tA.y > tB.y ? tA.y : tB.y;

    // Lift then move for step-up; move then lower for step-down.
    for(int y = tA.y; y <= iHigh; y++)
    {
        if(!pl__path_clear(ptGrid, (plVoxelCoord){tA.x, y, tA.z}, ptSettings))
            return false;
    }
    for(int y = tB.y; y <= iHigh; y++)
    {
        if(!pl__path_clear(ptGrid, (plVoxelCoord){tB.x, y, tB.z}, ptSettings))
            return false;
    }
    if(tA.x != tB.x && tA.z != tB.z)
    {
        // Both side columns touched by the diagonal must admit a cardinal
        // transition at one endpoint elevation. This checks support, the full
        // footprint, and lift/lower clearance, including on diagonal stairs.
        const plVoxelCoord atSides[2] = {{tA.x, 0, tB.z}, {tB.x, 0, tA.z}};
        for(int i = 0; i < 2; i++)
        {
            plVoxelCoord tSide = atSides[i];
            tSide.y = tA.y;
            bool bClear = pl__path_step_clear(ptGrid, tA, tSide, ptSettings) && pl__path_step_clear(ptGrid, tSide, tB, ptSettings);
            if(!bClear && tA.y != tB.y)
            {
                tSide.y = tB.y;
                bClear = pl__path_step_clear(ptGrid, tA, tSide, ptSettings) && pl__path_step_clear(ptGrid, tSide, tB, ptSettings);
            }
            if(!bClear)
                return false;
        }
    }
    return true;
}

// Supercover DDA: at edge/corner crossings check ALL touched cells. Ground
// smoothing stays on one elevation; step elbows are never skipped.
static void
pl__path_line_begin(plPathLine* ptLine, const plVoxelGrid* ptGrid, plVoxelCoord tA, plVoxelCoord tB, const plPathSettings* ptSettings)
{
    *ptLine = (plPathLine){.aiC = {tA.x, tA.y, tA.z}, .aiEnd = {tB.x, tB.y, tB.z}};
    ptLine->bVisible = (ptSettings->bFlying || tA.y == tB.y) && pl__path_walkable(ptGrid, tA, ptSettings) && pl__path_walkable(ptGrid, tB, ptSettings);
    ptLine->bDone = !ptLine->bVisible || (tA.x == tB.x && tA.y == tB.y && tA.z == tB.z);
    for(int i = 0; i < 3; i++)
    {
        double d = (double)ptLine->aiEnd[i]-ptLine->aiC[i];
        ptLine->aiStep[i] = d > 0 ? 1 : d < 0 ? -1 : 0;
        ptLine->adDelta[i] = d == 0 ? DBL_MAX : 1.0 / fabs(d);
        ptLine->adNext[i] = d == 0 ? DBL_MAX : ptLine->adDelta[i] * 0.5;
    }
}

static void
pl__path_line_step(plPathLine* ptLine, const plVoxelGrid* ptGrid, const plPathSettings* ptSettings)
{
    if(ptLine->bDone)
        return;
    double dNext = fmin(ptLine->adNext[0], fmin(ptLine->adNext[1], ptLine->adNext[2]));
    int iMask = 0;
    for(int i = 0; i < 3; i++)
    {
        if(ptLine->adNext[i] <= dNext + 1e-10)
            iMask |= 1 << i;
    }

    for(int s = iMask; s; s = (s - 1) & iMask)
    {
        plVoxelCoord tC = {ptLine->aiC[0] + ((s & 1) ? ptLine->aiStep[0] : 0), ptLine->aiC[1] + ((s & 2) ? ptLine->aiStep[1] : 0), ptLine->aiC[2] + ((s & 4) ? ptLine->aiStep[2] : 0)};
        if(!pl__path_walkable(ptGrid, tC, ptSettings))
        {
            ptLine->bDone = true;
            ptLine->bVisible = false;
            return;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        if(iMask & (1 << i))
        {
            ptLine->aiC[i] += ptLine->aiStep[i];
            ptLine->adNext[i] += ptLine->adDelta[i];
        }
    }
    ptLine->bDone = ptLine->aiC[0] == ptLine->aiEnd[0] && ptLine->aiC[1] == ptLine->aiEnd[1] && ptLine->aiC[2] == ptLine->aiEnd[2];
}

static bool
pl__path_visible(const plVoxelGrid* ptGrid, plVoxelCoord tA, plVoxelCoord tB, const plPathSettings* ptSettings)
{
    plPathLine tLine;
    pl__path_line_begin(&tLine, ptGrid, tA, tB, ptSettings);
    while(!tLine.bDone)
        pl__path_line_step(&tLine, ptGrid, ptSettings);
    return tLine.bVisible;
}

static bool
pl__path_allocate(plPathQuery* ptQuery, uint32_t uLimit)
{
    uint32_t uCapacity = ptQuery->_uCapacity > uLimit ? ptQuery->_uCapacity : uLimit;
    uint32_t uHashSize = 2;
    
    while(uHashSize < uCapacity * 2)
        uHashSize *= 2;

    // Node/heap/hash + two paths (two waypoints per node allow step elbows).
    size_t szBytes = sizeof(plPathWork) + (size_t)uCapacity * (sizeof(plPathNode) + sizeof(uint32_t) + 4 * sizeof(plVec3)) + (size_t)uHashSize * (sizeof(uint64_t) + sizeof(uint32_t));
    if(uCapacity != ptQuery->_uCapacity)
    {
        void* pNew = PL_ALLOC(szBytes);
        if(!pNew)
            return false;
        if(ptQuery->_pScratch)
        {
            PL_FREE(ptQuery->_pScratch);
        }
        ptQuery->_pScratch = pNew;
        ptQuery->_uCapacity = uCapacity;
    }
    plPathWork* ptWork = ptQuery->_pScratch;
    memset(ptWork, 0, sizeof(*ptWork));
    // Place 64-bit keys first to preserve alignment for any node capacity.
    ptWork->tHash.auKeys = (uint64_t*)(ptWork + 1);
    ptWork->atNodes = (plPathNode*)(ptWork->tHash.auKeys + uHashSize);
    ptWork->auHeap = (uint32_t*)(ptWork->atNodes + uCapacity);
    ptWork->tHash.auValueBucket = ptWork->auHeap + uCapacity;
    ptWork->tHash.uBucketCount = uHashSize;
    ptWork->uLimit = uLimit;
    pl_hms_clear32(&ptWork->tHash);
    ptQuery->atRawWaypoints = (plVec3*)(ptWork->tHash.auValueBucket + uHashSize);
    ptQuery->atWaypoints = ptQuery->atRawWaypoints + 2 * uCapacity;
    return true;
}

// One expansion is a work unit. Neighbor tests are bounded by settings, with
// diagonal edge costs matching the actual straight or lift/move/lower route.
static void
pl__path_expand(plPathQuery* ptQuery, plPathWork* ptWork)
{
    if(!ptWork->uHeapCount)
    {
        pl__path_finish(ptQuery, PL_PATH_STATUS_NO_PATH);
        return;
    }
    const plVoxelGrid* ptGrid = ptWork->ptGrid;
    const plPathSettings* ptSettings = &ptWork->tSettings;
    uint32_t uCurrent = pl__path_heap_pop(ptWork);
    plPathNode* ptCurrent = &ptWork->atNodes[uCurrent];
    ptQuery->uExpandedNodes++;
    if(ptCurrent->uCell == ptWork->uGoal)
    {
        ptWork->uTrace = uCurrent;
        ptWork->tPhase = PL_PATH_PHASE_TRACE;
        return;
    }
    plVoxelCoord tC = pl__path_coord(ptGrid,ptCurrent->uCell);
    int iSteps = ptSettings->bFlying ? 1 : ptSettings->iMaxStep;
    for(int dx = -1; dx <= 1; dx++)
    {
        for(int dz = -1; dz <= 1; dz++)
        {
            for(int dy = -iSteps; dy <= iSteps; dy++)
            {
                if(ptSettings->bFlying)
                {
                    int iAxes = (dx != 0) + (dy != 0) + (dz != 0);
                    if(!iAxes || (!ptSettings->bAllowDiagonal && iAxes != 1))
                        continue;
                }
                else if((dx == 0 && dz == 0) || (!ptSettings->bAllowDiagonal && dx != 0 && dz != 0))
                    continue;

                // Bound Y before addition (large legal resolutions can approach INT_MAX).
                if((dy < 0 && tC.y < -dy) || (dy > 0 && (uint32_t)dy >= ptGrid->auResolution[1] - (uint32_t)tC.y))
                    continue;
                plVoxelCoord tN = {tC.x + dx, tC.y + dy, tC.z + dz};

                if(!pl__path_step_clear(ptGrid,tC, tN, ptSettings))
                    continue;
                uint32_t uCell = pl__path_index(ptGrid, tN);
                uint32_t uKey = uCell * UINT32_C(2654435761);
                uint32_t uNode = pl_hms_get32(&ptWork->tHash, uKey);
                float fCost = ptCurrent->fCost + pl__path_distance(tC, tN, ptSettings);
                if(uNode == PL_DS_HASH32_INVALID)
                {
                    if(ptWork->uCount == ptWork->uLimit)
                    {
                        pl__path_finish(ptQuery, PL_PATH_STATUS_NODE_LIMIT);
                        return;
                    }
                    uNode = ptWork->uCount++;
                    ptQuery->uVisitedNodes = ptWork->uCount;
                    pl_hms_set32(&ptWork->tHash, uKey, uNode);
                    ptWork->atNodes[uNode] = (plPathNode){uCell, uCurrent, ptWork->uHeapCount, fCost, fCost + pl__path_distance(tN, ptWork->tTo, ptSettings)};
                    ptWork->auHeap[ptWork->uHeapCount++] = uNode;
                    pl__path_heap_up(ptWork, ptWork->uHeapCount - 1);
                }
                else if(fCost < ptWork->atNodes[uNode].fCost)
                {
                    plPathNode* ptNode = &ptWork->atNodes[uNode];
                    ptNode->fCost = fCost;
                    ptNode->fScore = fCost + pl__path_distance(tN, ptWork->tTo, ptSettings);
                    ptNode->uParent = uCurrent;
                    if(ptNode->uHeap == UINT32_MAX)
                    {
                        ptNode->uHeap = ptWork->uHeapCount;
                        ptWork->auHeap[ptWork->uHeapCount++] = uNode;
                    }
                    pl__path_heap_up(ptWork, ptNode->uHeap);
                }
            }
        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_path_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plPathI tApi = {
        .default_settings       = pl_path_default_settings,
        .cleanup                = pl_path_cleanup,
        .process                = pl_path_process,
        .is_walkable            = pl_path_is_walkable,
        .status_string          = pl_path_status_string,
        .debug_draw             = pl_path_debug_draw,
        .begin                  = pl_path_begin,
        .step                   = pl_path_step,
        .cancel                 = pl_path_cancel,
        .is_segment_traversable = pl_path_is_segment_traversable};

    pl_set_api(ptApiRegistry,plPathI,&tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry,plMemoryI);
    gptVoxel  = pl_get_api_latest(ptApiRegistry,plVoxelI);
    gptDraw   = pl_get_api_latest(ptApiRegistry,plDrawI);
}

PL_EXPORT void
pl_unload_path_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(!bReload)
        ptApiRegistry->remove_api(pl_get_api_latest(ptApiRegistry,plPathI));
}
