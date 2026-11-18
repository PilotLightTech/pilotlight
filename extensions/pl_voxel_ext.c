/*
   pl_voxel_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#include <string.h>

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

// extensions
#include "pl_voxel_ext.h"
#include "pl_mesh_ext.h"
#include "pl_ecs_ext.h"
#include "pl_asset_ext.h"
#include "pl_transform_ext.h"
#include "pl_renderer_ext.h"
#include "pl_draw_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*    gptMemory    = NULL;
    static const plEcsI*       gptEcs       = NULL;
    static const plAssetI*     gptAsset     = NULL;
    static const plTransformI* gptTransform = NULL;
    static const plRendererI*  gptRenderer  = NULL;
    static const plDrawI*      gptDraw      = NULL;

    #define PL_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline uint32_t
pl__voxel_index(const plVoxelGrid* ptGrid, plVoxelCoord tC)
{
    return ((uint32_t)tC.z * ptGrid->auResolution[1] + (uint32_t)tC.y) * ptGrid->auResolution[0] + (uint32_t)tC.x;
}

static inline plVoxelCoord
pl__voxel_coord(const plVoxelGrid* ptGrid, uint32_t uIndex)
{
    plVoxelCoord tC;
    tC.x = (int)(uIndex % ptGrid->auResolution[0]);
    uIndex /= ptGrid->auResolution[0];
    tC.y = (int)(uIndex % ptGrid->auResolution[1]);
    tC.z = (int)(uIndex / ptGrid->auResolution[1]);
    return tC;
}

static inline plVec3d
pl__voxel_cross(plVec3d tA, plVec3d tB)
{
    return (plVec3d){tA.y * tB.z - tA.z * tB.y, tA.z * tB.x - tA.x * tB.z, tA.x * tB.y - tA.y * tB.x};
}

static bool pl__voxel_range    (const plVoxelGrid* ptGrid, const double adMin[3], const double adMax[3], int aiLo[3], int aiHi[3]);
static bool pl__voxel_separated(plVec3d tAxis, const plVec3d atV[3]);
static bool pl__voxel_ray_cells(const plVoxelGrid* ptGrid, const double adPoint[3], plVoxelCoord* ptCell);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_voxel_is_valid(const plVoxelGrid* ptGrid, plVoxelCoord tC)
{
    return ptGrid && ptGrid->_puBits && tC.x >= 0 && tC.y >= 0 && tC.z >= 0 && (uint32_t)tC.x < ptGrid->auResolution[0] && (uint32_t)tC.y < ptGrid->auResolution[1] && (uint32_t)tC.z < ptGrid->auResolution[2];
}

bool
pl_voxel_check(const plVoxelGrid* ptGrid, plVoxelCoord tC)
{
    if(!pl_voxel_is_valid(ptGrid, tC))
        return false;
    uint32_t uIndex = pl__voxel_index(ptGrid, tC);
    return (ptGrid->_puBits[uIndex >> 6] & (UINT64_C(1) << (uIndex & 63))) != 0;
}

void
pl_voxel_set(plVoxelGrid* ptGrid, plVoxelCoord tC, bool bValue)
{
    if(!pl_voxel_is_valid(ptGrid, tC))
        return;
    uint32_t uIndex = pl__voxel_index(ptGrid, tC);
    uint64_t uMask = UINT64_C(1) << (uIndex & 63);
    if(bValue)
        ptGrid->_puBits[uIndex >> 6] |= uMask;
    else
        ptGrid->_puBits[uIndex >> 6] &= ~uMask;
}

void
pl_voxel_cleanup(plVoxelGrid* ptGrid)
{
    if(!ptGrid)
        return;

    if(ptGrid->_puBits)
    {
        PL_FREE(ptGrid->_puBits);
    }
    memset(ptGrid, 0, sizeof(*ptGrid));
}

bool
pl_voxel_init(plVoxelGrid* ptGrid, plAABB tBounds, float fSize)
{
    if(!ptGrid || fSize <= 0.0f)
        return false;
    plVoxelGrid tNew = {0};
    tNew.tMin = tBounds.tMin;
    tNew.fVoxelSize = fSize;
    uint64_t uCount = 1;
    for(int i = 0; i < 3; i++)
    {
        double dCells = ceil(((double)tBounds.tMax.d[i] - tBounds.tMin.d[i]) / fSize);
        if(dCells < 1 || dCells > INT_MAX)
            return false;
        double dMax = (double)tNew.tMin.d[i] + dCells * fSize;
        if(dMax > FLT_MAX || dMax < -FLT_MAX)
            return false;
        tNew.auResolution[i] = (uint32_t)dCells;
        uCount *= tNew.auResolution[i];
        if(uCount > UINT32_MAX)
            return false;
    }
    tNew.uCellCount = (uint32_t)uCount;
    tNew.szMemory = (size_t)((uCount + 63) / 64) * sizeof(uint64_t);
    tNew._puBits = PL_ALLOC(tNew.szMemory);
    if(!tNew._puBits)
        return false;
    memset(tNew._puBits, 0, tNew.szMemory);
    pl_voxel_cleanup(ptGrid);
    *ptGrid = tNew;
    return true;
}

void
pl_voxel_clear(plVoxelGrid* ptGrid)
{
    if(ptGrid && ptGrid->_puBits)
        memset(ptGrid->_puBits, 0, ptGrid->szMemory);
}

bool
pl_voxel_world_to_coord(const plVoxelGrid* ptGrid, plVec3 tP, plVoxelCoord* ptOut)
{
    if(!ptGrid || !ptGrid->_puBits || !ptOut)
        return false;
    int aiC[3] = {0};
    for(int i = 0; i < 3; i++)
    {
        double dC = floor(((double)tP.d[i] - ptGrid->tMin.d[i]) / ptGrid->fVoxelSize);
        if(dC < 0 || dC >= ptGrid->auResolution[i])
            return false;
        aiC[i] = (int)dC;
    }
    *ptOut = (plVoxelCoord){aiC[0], aiC[1], aiC[2]};
    return true;
}

plVec3
pl_voxel_coord_to_world(const plVoxelGrid* ptGrid, plVoxelCoord tC)
{
    if(!pl_voxel_is_valid(ptGrid, tC))
        return (plVec3){0};

    return (plVec3){(float)((double)ptGrid->tMin.x + ((double)tC.x + 0.5) * ptGrid->fVoxelSize),
                    (float)((double)ptGrid->tMin.y + ((double)tC.y + 0.5) * ptGrid->fVoxelSize),
                    (float)((double)ptGrid->tMin.z + ((double)tC.z + 0.5) * ptGrid->fVoxelSize)};
}

bool
pl_voxel_copy(plVoxelGrid* ptDestination, const plVoxelGrid* ptSource)
{
    if(!ptDestination || !ptSource || !ptSource->_puBits)
        return false;
    if(ptDestination == ptSource)
        return true;

    plVoxelGrid tNew = *ptSource;
    tNew._puBits = PL_ALLOC(tNew.szMemory);
    if(!tNew._puBits)
        return false;

    memcpy(tNew._puBits, ptSource->_puBits, tNew.szMemory);
    pl_voxel_cleanup(ptDestination);
    *ptDestination = tNew;
    return true;
}

plAABB
pl_voxel_get_bounds(const plVoxelGrid* ptGrid)
{
    if(!ptGrid || !ptGrid->_puBits)
        return (plAABB){0};

    plAABB tBounds = {.tMin = ptGrid->tMin};
    for(int i = 0; i < 3; i++)
    {
        tBounds.tMax.d[i] = (float)((double)ptGrid->tMin.d[i] + (double)ptGrid->auResolution[i] * ptGrid->fVoxelSize);
    }
    return tBounds;
}

uint32_t
pl_voxel_count_occupied(const plVoxelGrid* ptGrid)
{
    if(!ptGrid || !ptGrid->_puBits)
        return 0;

    uint32_t uCount = 0;
    size_t szWords = ptGrid->szMemory / sizeof(uint64_t);
    for(size_t i = 0; i < szWords; i++)
    {
        uint64_t uBits = ptGrid->_puBits[i];
        if(i + 1 == szWords && (ptGrid->uCellCount & 63))
            uBits &= (UINT64_C(1) << (ptGrid->uCellCount & 63)) - 1;

        // Portable word popcount, independent of compiler intrinsics.
        uBits -= (uBits >> 1) & UINT64_C(0x5555555555555555);
        uBits = (uBits & UINT64_C(0x3333333333333333)) + ((uBits >> 2) & UINT64_C(0x3333333333333333));
        uBits = (uBits + (uBits >> 4)) & UINT64_C(0x0f0f0f0f0f0f0f0f);
        uCount += (uint32_t)((uBits * UINT64_C(0x0101010101010101)) >> 56);
    }
    return uCount;
}

bool
pl_voxel_raycast(const plVoxelGrid* ptGrid, plVec3 tStart, plVec3 tEnd, plVoxelRayHit* ptHit)
{
    if(!ptGrid || !ptGrid->_puBits)
        return false;

    double adOrigin[3] = {0};
    double adDirection[3] = {0};
    double dEnter = 0;
    double dExit = 1;
    for(int i = 0; i < 3; i++)
    {
        adOrigin[i] = ((double)tStart.d[i] - ptGrid->tMin.d[i]) / ptGrid->fVoxelSize;
        adDirection[i] = ((double)tEnd.d[i] - tStart.d[i]) / ptGrid->fVoxelSize;
        if(adDirection[i] == 0)
        {
            if(adOrigin[i] < 0 || adOrigin[i] > ptGrid->auResolution[i])
                return false;
        }
        else
        {
            double dA = -adOrigin[i] / adDirection[i];
            double dB = ((double)ptGrid->auResolution[i] - adOrigin[i]) / adDirection[i];
            dEnter = fmax(dEnter, fmin(dA, dB));
            dExit = fmin(dExit, fmax(dA, dB));
            if(dEnter > dExit)
                return false;
        }
    }

    double adPoint[3] = {0};
    double adBoundary[3] = {0};
    double adNext[3] = {0};
    for(int i = 0; i < 3; i++)
    {
        adPoint[i] = fma(adDirection[i], dEnter, adOrigin[i]);
        adPoint[i] = fmax(0, fmin(ptGrid->auResolution[i], adPoint[i]));
        double dRounded = round(adPoint[i]);
        if(fabs(adPoint[i] - dRounded) <= 8 * DBL_EPSILON * fmax(1, fabs(adPoint[i])))
            adPoint[i] = dRounded;

        adBoundary[i] = adDirection[i] > 0 ? floor(adPoint[i]) + 1 : ceil(adPoint[i]) - 1;
        adNext[i] = adDirection[i] == 0 ? DBL_MAX : (adBoundary[i] - adOrigin[i]) / adDirection[i];
    }
    plVoxelCoord tCell = {0};
    double dFraction = dEnter;
    for(;;)
    {
        // At each crossing inspect all incident cells. This also handles a ray
        // lying on a face/edge, and both cells touching either endpoint.
        if(pl__voxel_ray_cells(ptGrid, adPoint, &tCell))
        {
            if(ptHit)
            {
                ptHit->tCoord = tCell;
                ptHit->dFraction = dFraction;
                for(int i = 0; i < 3; i++)
                {
                    ptHit->tPosition.d[i] = (float)fma((double)tEnd.d[i] - tStart.d[i], dFraction, tStart.d[i]);
                }
            }
            return true;
        }
        if(dFraction >= dExit)
            return false;

        double dNext = fmin(adNext[0], fmin(adNext[1], adNext[2]));
        if(dNext > dExit)
            return false;

        dFraction = fmax(dEnter, dNext);
        for(int i = 0; i < 3; i++)
        {
            adPoint[i] = fma(adDirection[i], dFraction, adOrigin[i]);
            if(adNext[i] <= dNext + 8 * DBL_EPSILON * fabs(dNext))
            {
                adPoint[i] = adBoundary[i];
                adBoundary[i] += adDirection[i] > 0 ? 1 : -1;
                adNext[i] = (adBoundary[i] - adOrigin[i]) / adDirection[i];
            }
        }
    }
}

void
pl_voxel_inject_aabb(plVoxelGrid* ptGrid, plAABB tBounds, bool bSubtract)
{
    double adMin[3] = {tBounds.tMin.x, tBounds.tMin.y, tBounds.tMin.z};
    double adMax[3] = {tBounds.tMax.x, tBounds.tMax.y, tBounds.tMax.z};
    int aiLo[3] = {0};
    int aiHi[3] = {0};

    if(!pl__voxel_range(ptGrid, adMin, adMax, aiLo, aiHi))
        return;

    for(int z = aiLo[2]; z <= aiHi[2]; z++)
    {
        for(int y = aiLo[1]; y <= aiHi[1]; y++)
        {
            for(int x = aiLo[0]; x <= aiHi[0]; x++)
            {
                pl_voxel_set(ptGrid, (plVoxelCoord){x, y, z}, !bSubtract);
            }
        }
    }
}

void
pl_voxel_inject_triangle(plVoxelGrid* ptGrid, plVec3 tA, plVec3 tB, plVec3 tC, bool bSubtract)
{
    double adMin[3] = {0};
    double adMax[3] = {0};
    for(int i = 0; i < 3; i++)
    {
        adMin[i] = fmin(tA.d[i], fmin(tB.d[i], tC.d[i]));
        adMax[i] = fmax(tA.d[i], fmax(tB.d[i], tC.d[i]));
    }
    int aiLo[3], aiHi[3];
    if(!pl__voxel_range(ptGrid, adMin, adMax, aiLo, aiHi))
        return;

    // Grid-local doubles avoid overflow in cross products of finite float inputs.
    const plVec3 atInput[3] = {tA, tB, tC};
    plVec3d atLocal[3] = {0};
    plVec3d atEdges[3] = {0};
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            atLocal[i].d[j] = ((double)atInput[i].d[j] - ptGrid->tMin.d[j]) / ptGrid->fVoxelSize;
        }
    }
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            atEdges[i].d[j] = atLocal[(i + 1) % 3].d[j] - atLocal[i].d[j];
        }
    }
    const plVec3d atBasis[3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    plVec3d atAxes[13] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    atAxes[3] = pl__voxel_cross(atEdges[0], atEdges[1]);
    for(int i = 0; i < 3; i++)
    {
        for(int j = 0; j < 3; j++)
        {
            atAxes[4 + i * 3 + j] = pl__voxel_cross(atEdges[i], atBasis[j]);
        }
    }
    for(int z = aiLo[2]; z <= aiHi[2]; z++)
    {
        for(int y = aiLo[1]; y <= aiHi[1]; y++)
        {
            for(int x = aiLo[0]; x <= aiHi[0]; x++)
            {
                plVec3d atV[3] = {0};
                for(int i = 0; i < 3; i++)
                {
                    atV[i] = (plVec3d){atLocal[i].x - (x + 0.5), atLocal[i].y - (y + 0.5), atLocal[i].z - (z + 0.5)};
                }
                bool bHit = true;
                for(int i = 0; i < 13; i++)
                {
                    if(pl__voxel_separated(atAxes[i], atV))
                    {
                        bHit = false;
                        break;
                    }
                }
                if(bHit)
                    pl_voxel_set(ptGrid, (plVoxelCoord){x, y, z}, !bSubtract);
            }
        }
    }
}

void
pl_voxel_inject_sphere(plVoxelGrid* ptGrid, plVec3 tCenter, float fRadius, bool bSubtract)
{
    if(fRadius < 0.0f)
        return;

    double adMin[3] = {0};
    double adMax[3] = {0};
    for(int i = 0; i < 3; i++)
    {
        adMin[i] = (double)tCenter.d[i] - fRadius;
        adMax[i] = (double)tCenter.d[i] + fRadius;
    }
    int aiLo[3] = {0};
    int aiHi[3] = {0};
    if(!pl__voxel_range(ptGrid, adMin, adMax, aiLo, aiHi))
        return;

    for(int z = aiLo[2]; z <= aiHi[2]; z++)
    {
        for(int y = aiLo[1]; y <= aiHi[1]; y++)
        {
            for(int x = aiLo[0]; x <= aiHi[0]; x++)
            {
                const int aiCell[3] = {x, y, z};
                double dDistance = 0;
                for(int i = 0; i < 3; i++)
                {
                    double dMin = (double)ptGrid->tMin.d[i] + (double)aiCell[i] * ptGrid->fVoxelSize;
                    double dMax = dMin + ptGrid->fVoxelSize;
                    double d = fmax(0, fmax(dMin - tCenter.d[i], tCenter.d[i] - dMax));
                    dDistance += d * d;
                }
                if(dDistance <= (double)fRadius * fRadius)
                    pl_voxel_set(ptGrid, (plVoxelCoord){x, y, z}, !bSubtract);
            }
        }
    }
}

uint32_t
pl_voxel_inject_mesh(plVoxelGrid* ptGrid, const plMesh* ptMesh, const plMat4* ptWorld, uint32_t uFirst, uint32_t uCount)
{
    if(!ptGrid || !ptGrid->_puBits || !ptMesh || !ptMesh->atSubmeshes || uFirst >= ptMesh->uSubmeshCount)
        return 0;

    if(ptWorld)
    {
        if(ptWorld->x41 != 0 || ptWorld->x42 != 0 || ptWorld->x43 != 0 || ptWorld->x44 != 1)
            return 0;
    }
    uint32_t uTriangles = 0;
    if(uCount > ptMesh->uSubmeshCount - uFirst)
        uCount = ptMesh->uSubmeshCount - uFirst;
    for(uint32_t s = uFirst; s < uFirst + uCount; s++)
    {
        const plSubmesh* ptSub = &ptMesh->atSubmeshes[s];
        if(!ptSub->ptVertexPositions)
            continue;
        size_t szCount = ptSub->puIndices ? ptSub->szIndexCount : ptSub->szVertexCount;
        for(size_t i = 0; i < szCount - szCount % 3; i += 3)
        {
            plVec3 atV[3] = {0};
            bool bValid = true;
            for(int j = 0; j < 3; j++)
            {
                size_t szIndex = ptSub->puIndices ? ptSub->puIndices[i + j] : i + j;
                if(szIndex >= ptSub->szVertexCount)
                {
                    bValid = false;
                    break;
                }
                atV[j] = ptSub->ptVertexPositions[szIndex];
                if(ptWorld)
                    atV[j] = pl_mul_mat4_vec3(ptWorld, atV[j]);
            }
            if(!bValid)
                continue;
            pl_voxel_inject_triangle(ptGrid, atV[0], atV[1], atV[2], false);
            if(uTriangles != UINT32_MAX)
                uTriangles++;
        }
    }
    return uTriangles;
}

uint32_t
pl_voxel_voxelize_scene(plVoxelGrid* ptGrid, const plComponentLibrary* ptLibrary)
{
    if(!ptGrid || !ptGrid->_puBits || !ptLibrary)
        return 0;
    plObjectComponent* atObjects = NULL;
    uint32_t uCount = gptEcs->get_components(ptLibrary, gptRenderer->get_ecs_type_key_object(), (void**)&atObjects, NULL);
    uint32_t uTriangles = 0;
    for(uint32_t i = 0; i < uCount; i++)
    {
        const plObjectComponent* ptObject = &atObjects[i];
        if(!(ptObject->tFlags & PL_OBJECT_FLAGS_RENDERABLE))
            continue;
        const plMesh* ptMesh = gptAsset->get_data(ptObject->tMesh);
        const plTransformComponent* ptTransform = gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), ptObject->tTransform);
        if(!ptMesh || !ptTransform)
            continue;
        uint32_t uAdded = pl_voxel_inject_mesh(ptGrid, ptMesh, &ptTransform->tWorld, ptObject->uFirstSubmesh, ptObject->uSubmeshCount);
        uTriangles = uAdded > UINT32_MAX - uTriangles ? UINT32_MAX : uTriangles + uAdded;
    }
    return uTriangles;
}

bool
pl_voxel_fill_interior(plVoxelGrid* ptGrid)
{
    if(!ptGrid || !ptGrid->_puBits || (uint64_t)ptGrid->uCellCount * sizeof(uint32_t) > SIZE_MAX)
        return false;
    uint64_t* puVisited = PL_ALLOC(ptGrid->szMemory);
    uint32_t* puQueue = PL_ALLOC((size_t)ptGrid->uCellCount * sizeof(uint32_t));
    if(!puVisited || !puQueue)
    {
        if(puVisited)
            PL_FREE(puVisited);
        if(puQueue)
            PL_FREE(puQueue);
        return false;
    }
    memset(puVisited, 0, ptGrid->szMemory);
    uint32_t uHead = 0;
    uint32_t uTail = 0;
    for(uint32_t i = 0; i < ptGrid->uCellCount; i++)
    {
        plVoxelCoord tC = pl__voxel_coord(ptGrid, i);
        bool bBoundary = !tC.x || !tC.y || !tC.z || (uint32_t)tC.x == ptGrid->auResolution[0] - 1 || (uint32_t)tC.y == ptGrid->auResolution[1] - 1 || (uint32_t)tC.z == ptGrid->auResolution[2] - 1;
        if(bBoundary && !pl_voxel_check(ptGrid, tC))
        {
            puVisited[i >> 6] |= UINT64_C(1) << (i & 63);
            puQueue[uTail++] = i;
        }
    }
    const plVoxelCoord atOffsets[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    while(uHead < uTail)
    {
        plVoxelCoord tC = pl__voxel_coord(ptGrid, puQueue[uHead++]);
        for(int j = 0; j < 6; j++)
        {
            plVoxelCoord tN = {tC.x + atOffsets[j].x, tC.y + atOffsets[j].y, tC.z + atOffsets[j].z};
            if(!pl_voxel_is_valid(ptGrid, tN) || pl_voxel_check(ptGrid, tN))
                continue;
            uint32_t uIndex = pl__voxel_index(ptGrid, tN);
            uint64_t uMask = UINT64_C(1) << (uIndex & 63);
            if(puVisited[uIndex >> 6] & uMask)
                continue;
            puVisited[uIndex >> 6] |= uMask;
            puQueue[uTail++] = uIndex;
        }
    }
    for(uint32_t i = 0; i < ptGrid->uCellCount; i++)
    {
        if(!(puVisited[i >> 6] & (UINT64_C(1) << (i & 63))))
            ptGrid->_puBits[i >> 6] |= UINT64_C(1) << (i & 63);
    }
    PL_FREE(puVisited);
    PL_FREE(puQueue);
    return true;
}

uint32_t
pl_voxel_debug_draw(const plVoxelGrid* ptGrid, plDrawList3D* ptDrawlist, uint32_t uColor, int iSlice, uint32_t uMax)
{
    if(!ptGrid || !ptGrid->_puBits || !ptDrawlist)
        return 0;
    plVec3 tMax = pl_voxel_get_bounds(ptGrid).tMax;
    gptDraw->add_3d_aabb(ptDrawlist, ptGrid->tMin, tMax, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = ptGrid->fVoxelSize * 0.08f});
    uint32_t uDrawn = 0;
    for(uint32_t i = 0; i < ptGrid->uCellCount && uDrawn < uMax; i++)
    {
        if(!(ptGrid->_puBits[i >> 6] & (UINT64_C(1) << (i & 63))))
            continue;
        plVoxelCoord tC = pl__voxel_coord(ptGrid, i);
        if(iSlice >= 0 && tC.y != iSlice)
            continue;
        float fSize = ptGrid->fVoxelSize * 0.95f;
        gptDraw->add_3d_centered_box(ptDrawlist, pl_voxel_coord_to_world(ptGrid, tC), fSize, fSize, fSize, (plDrawLineOptions){.uColor = uColor, .fThickness = ptGrid->fVoxelSize * 0.04f});
        uDrawn++;
    }
    return uDrawn;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static bool
pl__voxel_range(const plVoxelGrid* ptGrid, const double adMin[3], const double adMax[3], int aiLo[3], int aiHi[3])
{
    if(!ptGrid || !ptGrid->_puBits)
        return false;

    for(int i = 0; i < 3; i++)
    {
        if(adMin[i] > adMax[i])
            return false;
        double dLo = (adMin[i] - ptGrid->tMin.d[i]) / ptGrid->fVoxelSize;
        double dHi = (adMax[i] - ptGrid->tMin.d[i]) / ptGrid->fVoxelSize;
        if(dHi < 0 || dLo > ptGrid->auResolution[i])
            return false;

        // Closed overlap includes both cells touching an exact boundary.
        // Clip before integer conversion, including enormous off-grid shapes.
        aiLo[i] = (int)fmin(ptGrid->auResolution[i] - 1.0, fmax(0, ceil(dLo) - 1));
        aiHi[i] = (int)fmin(ptGrid->auResolution[i] - 1.0, floor(dHi));
    }
    return true;
}

static bool
pl__voxel_separated(plVec3d tAxis, const plVec3d atV[3])
{
    double dA = tAxis.x * atV[0].x + tAxis.y * atV[0].y + tAxis.z * atV[0].z;
    double dB = tAxis.x * atV[1].x + tAxis.y * atV[1].y + tAxis.z * atV[1].z;
    double dC = tAxis.x * atV[2].x + tAxis.y * atV[2].y + tAxis.z * atV[2].z;
    double dRadius = 0.500000000001 * (fabs(tAxis.x) + fabs(tAxis.y) + fabs(tAxis.z));
    return fmin(dA, fmin(dB, dC)) > dRadius || fmax(dA, fmax(dB, dC)) < -dRadius;
}

static bool
pl__voxel_ray_cells(const plVoxelGrid* ptGrid, const double adPoint[3], plVoxelCoord* ptCell)
{
    int aiLo[3], aiHi[3];
    for(int i = 0; i < 3; i++)
    {
        double dPoint = adPoint[i];
        double dRounded = round(dPoint);
        if(fabs(dPoint - dRounded) <= 8 * DBL_EPSILON * fmax(1, fabs(dPoint)))
            dPoint = dRounded;
        if(dPoint < 0 || dPoint > ptGrid->auResolution[i])
            return false;

        aiLo[i] = (int)fmax(0, ceil(dPoint) - 1);
        aiHi[i] = (int)fmin(ptGrid->auResolution[i] - 1.0, floor(dPoint));
    }
    for(int z = aiLo[2]; z <= aiHi[2]; z++)
    {
        for(int y = aiLo[1]; y <= aiHi[1]; y++)
        {
            for(int x = aiLo[0]; x <= aiHi[0]; x++)
            {
                plVoxelCoord tCell = {x, y, z};
                if(pl_voxel_check(ptGrid, tCell))
                {
                    *ptCell = tCell;
                    return true;
                }
            }
        }
    }
    return false;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_voxel_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plVoxelI tApi = {
        .init            = pl_voxel_init,
        .cleanup         = pl_voxel_cleanup,
        .clear           = pl_voxel_clear,
        .world_to_coord  = pl_voxel_world_to_coord,
        .coord_to_world  = pl_voxel_coord_to_world,
        .is_valid        = pl_voxel_is_valid,
        .check           = pl_voxel_check,
        .set             = pl_voxel_set,
        .inject_aabb     = pl_voxel_inject_aabb,
        .inject_triangle = pl_voxel_inject_triangle,
        .inject_sphere   = pl_voxel_inject_sphere,
        .inject_mesh     = pl_voxel_inject_mesh,
        .voxelize_scene  = pl_voxel_voxelize_scene,
        .fill_interior   = pl_voxel_fill_interior,
        .debug_draw      = pl_voxel_debug_draw,
        .copy            = pl_voxel_copy,
        .get_bounds      = pl_voxel_get_bounds,
        .count_occupied  = pl_voxel_count_occupied,
        .raycast         = pl_voxel_raycast
    };
    pl_set_api(ptApiRegistry, plVoxelI, &tApi);

    gptMemory    = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptEcs       = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptAsset     = pl_get_api_latest(ptApiRegistry, plAssetI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptRenderer  = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptDraw      = pl_get_api_latest(ptApiRegistry, plDrawI);
}

PL_EXPORT void
pl_unload_voxel_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(!bReload)
        ptApiRegistry->remove_api(pl_get_api_latest(ptApiRegistry, plVoxelI));
}
