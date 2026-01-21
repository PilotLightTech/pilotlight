/*
   pl_cdlod_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal helpers
// [SECTION] public api implementation
// [SECTION] internal helpers implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include "pl.h"
#include "pl_cdlod_ext.h"

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#undef pl_vnsprintf
#include "pl_memory.h"

// stable extensions
#include "pl_platform_ext.h"
#include "pl_image_ext.h"

// unstable extensions
#include "pl_mesh_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCdLodMapElement
{
    uint32_t uX;
    uint32_t uZ;
    float    fX;
    float    fY;
    float    fZ;
    float    fError;
    int8_t   iActivationLevel;
} plCdLodMapElement;

typedef struct _plCdLodHeightMap
{
    bool                b3dErrorCalc; // true for ellipsoid
    bool                bEllipsoid;
    int                 iSize;
    int                 iLogSize;
    float               fSampleSpacing;
    float               fMaxBaseError;
    float               fMetersPerPixel;
    float               fMaxHeight;
    float               fMinHeight;
    float               fRadius;
    plVec3              tCenter;
    plCdLodMapElement* atElements;
    const char*         pcHeightMapFile;
    const char*         pcOutputFile;
} plCdLodHeightMap;

typedef union _plEdgeKey
{
    struct
    {
        uint32_t uLeft;
        uint32_t uRight;
    };
    uint64_t uData;
} plEdgeKey;

typedef struct _plEdgeEntry
{
    uint32_t t0;
    uint32_t t1;
} plEdgeEntry;

typedef struct _plTrianglePrimitive
{
    uint32_t iId; // binary-tree id
    int      iLevel;
    uint32_t uApex;
    uint32_t uLeft;
    uint32_t uRight;
} plTrianglePrimitive;

typedef struct _plCdLodContext
{
    float fMaxBaseError;
} plCdLodContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else

    static const plMemoryI*  gptMemory = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif

    // required APIs
    static const plMeshBuilderI* gptMeshBuilder = NULL;
    static const plImageI*       gptImage       = NULL;
    static const plFileI*        gptFile        = NULL;

#endif

#include "pl_ds.h"

// context
static plCdLodContext* gptCdLodCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal helpers
//-----------------------------------------------------------------------------

static inline int
pl__lowest_one(int x)
{

    // Returns the bit position of the lowest 1 bit in the given value.
    // If x == 0, returns the number of bits in an integer.
    //
    // E.g. pl__lowest_one(1) == 0; pl__lowest_one(16) == 4; pl__lowest_one(5) == 0;

	int	intbits = sizeof(x) * 8;
	int	i;
	for (i = 0; i < intbits; i++, x = x >> 1)
    {
		if (x & 1)
            break;
	}
	return i;
}

static inline int
pl__vertex_index(plCdLodHeightMap* ptHeightMap, int x, int z)
{
    if (x < 0 || x >= ptHeightMap->iSize || z < 0 || z >= ptHeightMap->iSize)
        return -1;
    return ptHeightMap->iSize * z + x;
}

static inline void
pl__activate_height_map_element(plCdLodMapElement* ptElement, int iLevel)
{
    if(iLevel > ptElement->iActivationLevel)
        ptElement->iActivationLevel = (int8_t)iLevel;
}

static inline plCdLodMapElement*
pl__get_elem(plCdLodHeightMap* ptHeightMap, int x, int z)
{
    return &ptHeightMap->atElements[x + z * ptHeightMap->iSize];
}

static inline int
pl__node_index(plCdLodHeightMap* ptHeightMap, int x, int z)
{
	// Given the coordinates of the center of a quadtree node, this
	// function returns its node index.  The node index is essentially
	// the node's rank in a breadth-first quadtree traversal.  Assumes
	// a [nw, ne, sw, se] traversal order.
	//
	// If the coordinates don't specify a valid node (e.g. if the coords
	// are outside the heightfield) then returns -1.

    if (x < 0 || x >= ptHeightMap->iSize || z < 0 || z >= ptHeightMap->iSize)
        return -1;

    int	l1 = pl__lowest_one(x | z);
    int	depth = ptHeightMap->iLogSize - l1 - 1;

    int	base = 0x55555555 & ((1 << depth*2) - 1);	// total node count in all levels above ours.
    int	shift = l1 + 1;

    // Effective coords within this node's level.
    int	col = x >> shift;
    int	row = z >> shift;

    return base + (row << depth) + col;
}

static inline uint32_t pl_parent(uint32_t id)  { return id >> 1u; }
static inline uint32_t pl_sibling(uint32_t id) { return id ^ 1u; }

static inline int
pl__mid_activation(plCdLodHeightMap* ptHeightMap, uint32_t uLeft, uint32_t uRight)
{
    plCdLodMapElement* eL = &ptHeightMap->atElements[uLeft];
    plCdLodMapElement* eR = &ptHeightMap->atElements[uRight];

    int iMidX = ((int)eL->uX + (int)eR->uX) / 2;
    int iMidZ = ((int)eL->uZ + (int)eR->uZ) / 2;

    plCdLodMapElement* ptElement = pl__get_elem(ptHeightMap, iMidX, iMidZ);
    return ptElement->iActivationLevel;
}

// Given the triangle, computes an error value and activation level
// for its base vertex, and recurses to child triangles.
static void pl__update(plCdLodHeightMap*, float base_max_error, int ax, int az, int rx, int rz, int lx, int lz);
static void pl__propagate_activation_level(plCdLodHeightMap*, int cx, int cz, int level, int target_level);

// main steps
static void pl__initialize_cdlod_heightmap(plCdLodHeightMap*);
static void pl__terrain_mesh(FILE*, plCdLodHeightMap*, int iStartIndexX, int iStartIndexY, int iLogSize, int iLevel);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_initialize_cdlod(void)
{
}

void
pl_cleanup_cdlod(void)
{
}

void
pl_process_cdlod_heightmap(plCdLodHeightMapInfo tInfo)
{
    plCdLodHeightMap tHeightMap = {
        .b3dErrorCalc    = tInfo.b3dErrorCalc,
        .bEllipsoid      = tInfo.bEllipsoid,
        .fSampleSpacing  = 1.0f,
        .fMaxBaseError   = tInfo.fMaxBaseError,
        .fMetersPerPixel = tInfo.fMetersPerPixel,
        .fMaxHeight      = tInfo.fMaxHeight,
        .fMinHeight      = tInfo.fMinHeight,
        .fRadius         = tInfo.fRadius,
        .tCenter         = tInfo.tCenter,
        .pcHeightMapFile = tInfo.pcHeightMapFile,
        .pcOutputFile    = tInfo.pcOutputFile
    };

    pl__initialize_cdlod_heightmap(&tHeightMap);

    // update step

    // Run a view-independent L-K style BTT update on the heightfield, to generate
	// error and activation_level values for each element.
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, 0, tHeightMap.iSize-1, tHeightMap.iSize-1, tHeightMap.iSize-1, 0, 0);	// sw half of the square
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, tHeightMap.iSize-1, 0, 0, 0, tHeightMap.iSize-1, tHeightMap.iSize-1);	// ne half of the square

    // propogate step

	// Propagate the activation_level values of verts to their
	// uParent verts, quadtree LOD style.  Gives same result as L-K.
	for(int i = 0; i < tHeightMap.iLogSize; i++)
    {
		pl__propagate_activation_level(&tHeightMap, tHeightMap.iSize >> 1, tHeightMap.iSize >> 1, tHeightMap.iLogSize - 1, i);
		pl__propagate_activation_level(&tHeightMap, tHeightMap.iSize >> 1, tHeightMap.iSize >> 1, tHeightMap.iLogSize - 1, i);
	}

    // meshing step

    int iRootLevel = tInfo.iTreeDepth - 1;

    FILE* ptDataFile = fopen(tInfo.pcOutputFile, "wb");

    fwrite(&tInfo.iTreeDepth, 1, sizeof(int), ptDataFile);
    fwrite(&tHeightMap.fMaxBaseError, 1, sizeof(float), ptDataFile);

    uint32_t uChunkCount = 0x55555555 & ((1 << (tInfo.iTreeDepth*2)) - 1);
    fwrite(&uChunkCount, 1, sizeof(uint32_t), ptDataFile);

    pl__terrain_mesh(ptDataFile, &tHeightMap, 0, 0, tHeightMap.iLogSize, iRootLevel);

    PL_FREE(tHeightMap.atElements);

    fclose(ptDataFile);
}

static void
pl__initialize_cdlod_heightmap(plCdLodHeightMap* ptHeightMap)
{
    size_t szFileSize = 0;
    gptFile->binary_read(ptHeightMap->pcHeightMapFile, &szFileSize, NULL);
    uint8_t* puFileData = PL_ALLOC(szFileSize + 1);
    memset(puFileData, 0, szFileSize + 1);
    gptFile->binary_read(ptHeightMap->pcHeightMapFile, &szFileSize, puFileData);
    int iWidth = 0;
    int iHeight = 0;
    int iChannels = 0;
    uint8_t* puData = gptImage->load(puFileData, (int)szFileSize, &iWidth, &iHeight, &iChannels, 1);
    PL_FREE(puFileData);
    puFileData = NULL;

    ptHeightMap->iSize = pl_max(iWidth, iHeight);
    ptHeightMap->iLogSize = (int) (log2(ptHeightMap->iSize - 1) + 0.5f);

    // expand the heightfield dimension to contain the bitmap.
    while (((1 << ptHeightMap->iLogSize) + 1) < ptHeightMap->iSize)
        ptHeightMap->iLogSize++;
    ptHeightMap->iSize = (1 << ptHeightMap->iLogSize) + 1;

    size_t szHeightMapSize = ptHeightMap->iSize * ptHeightMap->iSize * sizeof(uint16_t);
    uint16_t* auHeightMapData = PL_ALLOC(szHeightMapSize);
    memset(auHeightMapData, 0, szHeightMapSize);

    float fMaxHeight = ptHeightMap->fMaxHeight;
    float fMinHeight = ptHeightMap->fMinHeight;
    for(uint32_t i = 0; i < (uint32_t)ptHeightMap->iSize - 1; i++)
    {
        for(uint32_t j = 0; j < (uint32_t)ptHeightMap->iSize - 1; j++)
        {
            if(i < (uint32_t)iWidth && j < (uint32_t)iHeight)
            {
                uint16_t uOldValue = (uint16_t)(((puData[i + j * iWidth] - fMinHeight) / (fMaxHeight - fMinHeight)) * 255.0f);
                uint8_t uRawHeight = puData[i + j * iWidth];
                uint16_t uModHeight = (uint16_t)(((double)uRawHeight / (double)UINT8_MAX) * (double)UINT16_MAX);
                auHeightMapData[j * ptHeightMap->iSize + i] = uModHeight;
            }
        }
    }

    gptImage->free(puData);
    puData = NULL;

    // Allocate storage.
    int	iSampleCount = ptHeightMap->iSize * ptHeightMap->iSize;
    ptHeightMap->atElements = PL_ALLOC(iSampleCount * sizeof(plCdLodMapElement));
    memset(ptHeightMap->atElements, 0, iSampleCount * sizeof(plCdLodMapElement));

    
    if(ptHeightMap->bEllipsoid)
    {

        float fMinExtent = -(float)(ptHeightMap->iSize) * ptHeightMap->fMetersPerPixel * 0.5f;
        float fExtent = (float)(ptHeightMap->iSize - 1) * ptHeightMap->fMetersPerPixel;

        // Initialize the data.
        for (int i = 0; i < ptHeightMap->iSize; i++)
        {
            for (int j = 0; j < ptHeightMap->iSize; j++)
            {
                float fY = 0.0f;

                // Extract a height value from the pixel data.
                uint32_t x = pl_min(i, ptHeightMap->iSize - 1);
                uint32_t y = pl_min(j, ptHeightMap->iSize - 1);

                uint16_t r = 0;
                if(x > (uint32_t)ptHeightMap->iSize - 1 || y > (uint32_t)ptHeightMap->iSize - 1)
                {

                }
                else
                    r = auHeightMapData[y * ptHeightMap->iSize + x];

                // y = (float)r / 65355.0f;	// just using red component for now.
                fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
                fY *= (fMaxHeight - fMinHeight);
                fY += fMinHeight;

                int iElementIndex = i + ptHeightMap->iSize * j;

                ptHeightMap->atElements[iElementIndex].iActivationLevel = -1;
                ptHeightMap->atElements[iElementIndex].uX = i;
                ptHeightMap->atElements[iElementIndex].uZ = j;


                float fX = ((float)i * fExtent / (float)(ptHeightMap->iSize - 1)) + fMinExtent;
                float fZ = ((float)j * fExtent / (float)(ptHeightMap->iSize - 1)) + fMinExtent;

                float fLongitude = atan2f(fX, (fZ - 0.0f));
                float fR = hypotf(fX, fZ);
                float fLatitude = -PL_PI_2 + 2.0f * atan2f(fR, ptHeightMap->fRadius);

                // elliptical
                plVec3 tEllipsePosition = {
                    
                    ptHeightMap->fRadius * cosf(fLatitude) * cosf(fLongitude),
                    ptHeightMap->fRadius * sinf(fLatitude),
                    ptHeightMap->fRadius * cosf(fLatitude) * sinf(fLongitude)
                };

                plVec3 tNormal = pl_norm_vec3(tEllipsePosition);

                ptHeightMap->atElements[iElementIndex].fX = tEllipsePosition.x + tNormal.x * fY;
                ptHeightMap->atElements[iElementIndex].fY = tEllipsePosition.y + tNormal.y * fY;
                ptHeightMap->atElements[iElementIndex].fZ = tEllipsePosition.z + tNormal.z * fY;

            }
        }
    }
    else
    {
        // Initialize the data.
        for (int i = 0; i < ptHeightMap->iSize; i++)
        {
            for (int j = 0; j < ptHeightMap->iSize; j++)
            {
                float fY = 0.0f;

                // Extract a height value from the pixel data.
                uint32_t x = pl_min(i, ptHeightMap->iSize - 1);
                uint32_t y = pl_min(j, ptHeightMap->iSize - 1);

                uint16_t r = 0;
                if(x > (uint32_t)ptHeightMap->iSize - 1 || y > (uint32_t)ptHeightMap->iSize - 1)
                {

                }
                else
                    r = auHeightMapData[y * ptHeightMap->iSize + x];

                // y = (float)r / 65355.0f;	// just using red component for now.
                fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
                fY *= (fMaxHeight - fMinHeight);
                fY += fMinHeight;

                int iElementIndex = i + ptHeightMap->iSize * j;
                ptHeightMap->atElements[iElementIndex].iActivationLevel = -1;
                ptHeightMap->atElements[iElementIndex].uX = i;
                ptHeightMap->atElements[iElementIndex].uZ = j;
                ptHeightMap->atElements[iElementIndex].fX = (float)i * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.x - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f;
                ptHeightMap->atElements[iElementIndex].fY = fY + ptHeightMap->tCenter.y;
                ptHeightMap->atElements[iElementIndex].fZ = (float)j * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.z - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f;
            }
        }
    }
    
    PL_FREE(auHeightMapData);
    auHeightMapData = NULL;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__terrain_mesh(FILE* ptFile, plCdLodHeightMap* ptHeightMap, int iStartIndexX, int iStartIndexY, int iLogSize, int iLevel)
{
    plMeshBuilder* ptMeshBuilder = gptMeshBuilder->create((plMeshBuilderOptions){.fWeldRadius = 0.001f});
    plTrianglePrimitive* sbtPrimitives = NULL;

    plEdgeEntry* sbtEdges = NULL;
    plHashMap tEdgeHash = {0};

    int	iSize = (1 << iLogSize);
    // int iEndIndexY = iStartIndexY + (iSize - 1);
    int iEndIndexY = iStartIndexY + (iSize);
    // int iEndIndexX = iStartIndexX + (iSize - 1);
    int iEndIndexX = iStartIndexX + (iSize);
    int iMaxLevel = iLogSize * 2 + iLevel;

	int	iHalfSize = iSize >> 1;
	int	iCx = iStartIndexX + iHalfSize;
	int	iCz = iStartIndexY + iHalfSize;

    int iChunkLabel = pl__node_index(ptHeightMap, iCx, iCz);

    fwrite(&iChunkLabel, 1, sizeof(int), ptFile);
    
    // chunk address
    fwrite(&iLevel, 1, sizeof(int), ptFile);


    
    const uint32_t uMaxId = (1u << (iMaxLevel + 2)); // plenty

    plTrianglePrimitive* atTriangles = PL_ALLOC(uMaxId * sizeof(plTrianglePrimitive));
    memset(atTriangles, 0, uMaxId * sizeof(plTrianglePrimitive));
    bool* atPresent = PL_ALLOC(uMaxId * sizeof(bool));
    memset(atPresent, 0, uMaxId * sizeof(bool));
    
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iStartIndexX, iStartIndexY), iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iStartIndexX, iEndIndexY), iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iEndIndexX, iEndIndexY), iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iEndIndexX, iStartIndexY), iLevel);

    // implicit super root
    atTriangles[1].iId = 1;
    atTriangles[1].iLevel = iLevel; // or -1 if you store signed

    plTrianglePrimitive tPrimitiveRoot0 = {
        .iId    = 2,
        .iLevel = iLevel,
        .uApex  = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX, iStartIndexY),
        .uLeft  = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iStartIndexY),
        .uRight = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX, iEndIndexY)
    };

    plTrianglePrimitive tPrimitiveRoot1 = {
        .iId    = 3,
        .iLevel = iLevel,
        .uApex  = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iEndIndexY),
        .uLeft  = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX, iEndIndexY),
        .uRight = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iStartIndexY)
    };

    plCdLodMapElement* ptRootElement0 = &(ptHeightMap->atElements[tPrimitiveRoot0.uLeft]);
    plCdLodMapElement* ptRootElement1 = &(ptHeightMap->atElements[tPrimitiveRoot1.uLeft]);

    pl_sb_push(sbtPrimitives, tPrimitiveRoot0);
    pl_sb_push(sbtPrimitives, tPrimitiveRoot1);

    // generate triangles at finest level
    while(pl_sb_size(sbtPrimitives) > 0)
    {

        plTrianglePrimitive tPrimitive = pl_sb_pop(sbtPrimitives);

        // assign triangle based on id
        atTriangles[tPrimitive.iId] = tPrimitive;

        // we've reached target level, no more splitting necessary
        if(tPrimitive.iLevel == iMaxLevel)
        {
            atPresent[tPrimitive.iId] = true;
            continue;
        }

        // split triangle into children

        plCdLodMapElement* ptElement0 = &(ptHeightMap->atElements[tPrimitive.uApex]);
        plCdLodMapElement* ptElement1 = &(ptHeightMap->atElements[tPrimitive.uRight]);
        plCdLodMapElement* ptElement2 = &(ptHeightMap->atElements[tPrimitive.uLeft]);

        const int iChildLevel = tPrimitive.iLevel + 1;

        const int iMidPointX = ((int)ptElement1->uX + (int)ptElement2->uX) / 2;
        const int iMidPointZ = ((int)ptElement1->uZ + (int)ptElement2->uZ) / 2;
        const uint32_t uMidPoint = pl__vertex_index(ptHeightMap, iMidPointX, iMidPointZ);

        plTrianglePrimitive tChild0 = {
            .iId    = tPrimitive.iId << 1,
            .iLevel = iChildLevel,
            .uApex  = uMidPoint,
            .uLeft  = tPrimitive.uApex,
            .uRight = tPrimitive.uLeft
        };

        plTrianglePrimitive tChild1 = {
            .iId    = (tPrimitive.iId << 1) | 1,
            .iLevel = iChildLevel,
            .uApex  = uMidPoint,
            .uLeft  = tPrimitive.uRight,
            .uRight = tPrimitive.uApex
        };
        pl_sb_push(sbtPrimitives, tChild0);
        pl_sb_push(sbtPrimitives, tChild1);
    }
    pl_sb_free(sbtPrimitives);
    sbtPrimitives = NULL;

    // simply mesh starting from finest level
    for(int iCurrentLevel = iMaxLevel - 1; iCurrentLevel >= 0; iCurrentLevel--)
    {
        pl_hm_free(&tEdgeHash);

        // build base-edge adjacency for PARENTS at level iCurrentLevel
        for(uint32_t uParent = 1; uParent < uMaxId; uParent++)
        {
            if(atTriangles[uParent].iLevel != iCurrentLevel)
                continue;

            const uint32_t uChild0 = uParent << 1;
            const uint32_t uChild1 = (uParent << 1) | 1u;

            if(uChild1 >= uMaxId)
                continue;

            // uParent is currently split if BOTH children are present
            if(!atPresent[uChild0] || !atPresent[uChild1])
                continue;

            plTrianglePrimitive tParent = atTriangles[uParent];

            plEdgeKey tEdge = {
                .uLeft  = pl_min(tParent.uLeft, tParent.uRight),
                .uRight = pl_max(tParent.uLeft, tParent.uRight)
            };

            // check if edge mate already registered
            if(pl_hm_has_key(&tEdgeHash, tEdge.uData))
            {
                uint64_t idx = pl_hm_lookup(&tEdgeHash, tEdge.uData);
                sbtEdges[idx].t1 = uParent;
            }
            else // first time edge registering
            {
                uint64_t idx = pl_hm_get_free_index(&tEdgeHash);
                if(idx == PL_DS_HASH_INVALID)
                {
                    idx = pl_sb_size(sbtEdges);
                    pl_sb_add(sbtEdges);
                }

                sbtEdges[idx].t0 = uParent;
                sbtEdges[idx].t1 = UINT32_MAX;

                pl_hm_insert(&tEdgeHash, tEdge.uData, idx);
            }
        }

        // merge triangles
        for(uint32_t i = 1; i < uMaxId; i++)
        {
            // skip if triangle already discarded
            if(!atPresent[i])
                continue;

            if(atTriangles[i].iLevel != iCurrentLevel + 1)
                continue;

            // process each sibling pair once (only even ids)
            if(i & 1u)
                continue;

            const uint32_t uSibling = pl_sibling(i);
            if(uSibling >= uMaxId)
                continue;
            
            if(!atPresent[uSibling])
                continue;

            const uint32_t uParent = pl_parent(i);
            if(uParent == 0 || uParent >= uMaxId)
                continue;

            // if the uParent split midpoint is required at iLevel, you must keep the split
            const int iActivationA = pl__mid_activation(ptHeightMap, atTriangles[uParent].uLeft, atTriangles[uParent].uRight);
            if(iActivationA >= iLevel)
                continue; // keep children

            // Find diamond mate across PARENT base edge (mate is a PARENT id)
            plEdgeKey tEdgeKey = {
                .uLeft  = pl_min(atTriangles[uParent].uLeft, atTriangles[uParent].uRight),
                .uRight = pl_max(atTriangles[uParent].uLeft, atTriangles[uParent].uRight)
            };

            if(!pl_hm_has_key(&tEdgeHash, tEdgeKey.uData))
                continue;

            uint64_t uEdgeIndex = pl_hm_lookup(&tEdgeHash, tEdgeKey.uData);
            plEdgeEntry tEdgeEntry = sbtEdges[uEdgeIndex];

            // pick the other PARENT in the diamond
            uint32_t uMateParent = (tEdgeEntry.t0 == uParent) ? tEdgeEntry.t1 : tEdgeEntry.t0;
            if(uMateParent == UINT32_MAX || uMateParent == 0 || uMateParent >= uMaxId)
                continue; // boundary / missing mate

            // uMateParent must currently be split too
            uint32_t uMateChild0 = uMateParent << 1;
            uint32_t uMateChild1 = (uMateParent << 1) | 1u;
            if(uMateChild1 >= uMaxId)
                continue;
            if(!atPresent[uMateChild0] || !atPresent[uMateChild1])
                continue;

            // mate side midpoint must also allow merge
            int iActivationB = pl__mid_activation(ptHeightMap, atTriangles[uMateParent].uLeft, atTriangles[uMateParent].uRight);
            if(iActivationA >= iLevel)
                continue;

            // avoid double-merging the same diamond
            if(uParent > uMateParent)
                continue;

            // perform diamond merge: remove 4 children, add 2 parents
            atPresent[i] = false;
            atPresent[uSibling] = false;
            atPresent[uParent] = true;

            atPresent[uMateChild0] = false;
            atPresent[uMateChild1] = false;
            atPresent[uMateParent] = true;
        }
    }

    pl_hm_free(&tEdgeHash);
    pl_sb_free(sbtEdges);

    plVec3 tMinBounding = {
        .x = FLT_MAX,
        .y = FLT_MAX,
        .z = FLT_MAX
    };

    plVec3 tMaxBounding = {
        .x = -FLT_MAX,
        .y = -FLT_MAX,
        .z = -FLT_MAX
    };

    // submit present triangles
    for(uint32_t i = 1; i < uMaxId; i++)
    {

        if(!atPresent[i])
            continue;

        plTrianglePrimitive tPrimitive = atTriangles[i];

        plCdLodMapElement* ptElement0 = &(ptHeightMap->atElements[tPrimitive.uApex]);
        plCdLodMapElement* ptElement1 = &(ptHeightMap->atElements[tPrimitive.uRight]);
        plCdLodMapElement* ptElement2 = &(ptHeightMap->atElements[tPrimitive.uLeft]);

        if(ptElement0->fX < tMinBounding.x) tMinBounding.x = ptElement0->fX;
        if(ptElement1->fX < tMinBounding.x) tMinBounding.x = ptElement1->fX;
        if(ptElement2->fX < tMinBounding.x) tMinBounding.x = ptElement2->fX;
        if(ptElement0->fX > tMaxBounding.x) tMaxBounding.x = ptElement0->fX;
        if(ptElement1->fX > tMaxBounding.x) tMaxBounding.x = ptElement1->fX;
        if(ptElement2->fX > tMaxBounding.x) tMaxBounding.x = ptElement2->fX;

        if(ptElement0->fY < tMinBounding.y) tMinBounding.y = ptElement0->fY;
        if(ptElement1->fY < tMinBounding.y) tMinBounding.y = ptElement1->fY;
        if(ptElement2->fY < tMinBounding.y) tMinBounding.y = ptElement2->fY;
        if(ptElement0->fY > tMaxBounding.y) tMaxBounding.y = ptElement0->fY;
        if(ptElement1->fY > tMaxBounding.y) tMaxBounding.y = ptElement1->fY;
        if(ptElement2->fY > tMaxBounding.y) tMaxBounding.y = ptElement2->fY;

        if(ptElement0->fZ < tMinBounding.z) tMinBounding.z = ptElement0->fZ;
        if(ptElement1->fZ < tMinBounding.z) tMinBounding.z = ptElement1->fZ;
        if(ptElement2->fZ < tMinBounding.z) tMinBounding.z = ptElement2->fZ;
        if(ptElement0->fZ > tMaxBounding.z) tMaxBounding.z = ptElement0->fZ;
        if(ptElement1->fZ > tMaxBounding.z) tMaxBounding.z = ptElement1->fZ;
        if(ptElement2->fZ > tMaxBounding.z) tMaxBounding.z = ptElement2->fZ;

        gptMeshBuilder->add_triangle(ptMeshBuilder, 
            (plVec3){
                ptElement0->fX,
                ptElement0->fY,
                ptElement0->fZ
            },
            (plVec3){
                ptElement1->fX,
                ptElement1->fY,
                ptElement1->fZ
            },
            (plVec3){
                ptElement2->fX,
                ptElement2->fY,
                ptElement2->fZ
            }
        );

    }

    PL_FREE(atTriangles);
    PL_FREE(atPresent);
    atTriangles = NULL;
    atPresent = NULL;

    uint32_t uVertexCount = 0;
    uint32_t uIndexCount = 0;
    gptMeshBuilder->commit(ptMeshBuilder, NULL, NULL, &uIndexCount, &uVertexCount);

    plVec3* atVertexData = PL_ALLOC(uVertexCount * sizeof(plVec3));
    uint32_t* atIndexData = PL_ALLOC(uIndexCount * sizeof(uint32_t));
    gptMeshBuilder->commit(ptMeshBuilder, atIndexData, atVertexData, &uIndexCount, &uVertexCount);

    gptMeshBuilder->cleanup(ptMeshBuilder);
    ptMeshBuilder = NULL;

    fwrite(&tMinBounding, 1, sizeof(plVec3), ptFile);
    fwrite(&tMaxBounding, 1, sizeof(plVec3), ptFile);

    fwrite(&uVertexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atVertexData, 1, sizeof(plVec3) * uVertexCount, ptFile);

    fwrite(&uIndexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atIndexData, 1, sizeof(uint32_t) * uIndexCount, ptFile);

    PL_FREE(atVertexData);
    PL_FREE(atIndexData);

    if(iLevel > 0)
    {

        iHalfSize = (1 << (iLogSize - 1));
        // [nw, ne, sw, se]
        pl__terrain_mesh(ptFile, ptHeightMap,                 iStartIndexX,             iStartIndexY + 0, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + iHalfSize,             iStartIndexY + 0, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap,             iStartIndexX + 0, iStartIndexY + iHalfSize, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + iHalfSize, iStartIndexY + iHalfSize, iLogSize - 1, iLevel - 1);
    }
}

static void
pl__update(plCdLodHeightMap* ptHeightMap, float fBaseMaxError, int iApexX, int iApexZ, int iRightX, int iRightZ, int iLeftX, int iLeftZ)
{
	// compute the coordinates of this triangle's base vertex.
	const int iDx = iLeftX - iRightX;
	const int iDz = iLeftZ - iRightZ;
	if (abs(iDx) <= 1 && abs(iDz) <= 1)
    {
		// We've reached the base level.  There's no base
		// vertex to update, and no child triangles to
		// recurse to.
		return;
	}

	// base vert is midway between left and right verts.
	const int iBaseX = iRightX + (iDx >> 1);
	const int iBaseZ = iRightZ + (iDz >> 1);

    plCdLodMapElement* ptBaseElement = pl__get_elem(ptHeightMap, iBaseX, iBaseZ);
    plCdLodMapElement* ptLeftElement = pl__get_elem(ptHeightMap, iLeftX, iLeftZ);
    plCdLodMapElement* ptRightElement = pl__get_elem(ptHeightMap, iRightX, iRightZ);

    float fError = 0.0f;
    if(ptHeightMap->b3dErrorCalc)
    {
        plVec3 tBaseVertex = {ptBaseElement->fX, ptBaseElement->fY, ptBaseElement->fZ};
        plVec3 tLeftVertex = {ptLeftElement->fX, ptLeftElement->fY, ptLeftElement->fZ};
        plVec3 tRightVertex = {ptRightElement->fX, ptRightElement->fY, ptRightElement->fZ};
        fError = pl_length_vec3(tBaseVertex) - (pl_length_vec3(tLeftVertex) + pl_length_vec3(tRightVertex)) / 2.0f;
    }
    else
    {
        fError = ptBaseElement->fY - (ptLeftElement->fY + ptRightElement->fY) / 2.0f;
    }

	pl__get_elem(ptHeightMap, iBaseX, iBaseZ)->fError = fError;	// Set this vert's error value.
	if (fabs(fError) >= fBaseMaxError)
    {
		// Compute the mesh level above which this vertex
		// needs to be included in LOD meshes.
		int	iActivationLevel = (int) floor(log2(fabs(fError) / fBaseMaxError) + 0.5);

		// Force the base vert to at least this activation level.
		plCdLodMapElement* ptElem = pl__get_elem(ptHeightMap, iBaseX, iBaseZ);
        pl__activate_height_map_element(ptElem, iActivationLevel);
	}

	// recurse to child triangles
	pl__update(ptHeightMap, fBaseMaxError, iBaseX, iBaseZ, iApexX, iApexZ, iRightX, iRightZ); // base, apex, right
	pl__update(ptHeightMap, fBaseMaxError, iBaseX, iBaseZ, iLeftX, iLeftZ, iApexX, iApexZ);	  // base, left, apex
}


static void
pl__propagate_activation_level(plCdLodHeightMap* ptHeightMap, int cx, int cz, int level, int target_level)
{

    // Does a quadtree descent through the heightfield, in the square with
    // center at (cx, cz) and size of (2 ^ (level + 1) + 1).  Descends
    // until level == target_level, and then propagates this square's
    // child center verts to the corresponding edge vert, and the edge
    // verts to the center.  Essentially the quadtree meshing update
    // dependency graph as in my Gamasutra article.  Must call this with
    // successively increasing target_level to get correct propagation.

	int	half_size = 1 << level;
	int	quarter_size = half_size >> 1;

	if (level > target_level)
    {
		// Recurse to children.
		for (int j = 0; j < 2; j++)
        {
			for (int i = 0; i < 2; i++)
            {
				pl__propagate_activation_level(ptHeightMap,
							   cx - quarter_size + half_size * i,
							   cz - quarter_size + half_size * j,
							   level - 1, target_level);
			}
		}
		return;
	}

	// We're at the target level.  Do the propagation on this
	// square.

	// ee == east edge, en = north edge, etc
	plCdLodMapElement*	ee = pl__get_elem(ptHeightMap, cx + half_size, cz);
	plCdLodMapElement*	en = pl__get_elem(ptHeightMap, cx, cz - half_size);
	plCdLodMapElement*	ew = pl__get_elem(ptHeightMap, cx - half_size, cz);
	plCdLodMapElement*	es = pl__get_elem(ptHeightMap, cx, cz + half_size);
	
	if (level > 0)
    {
		// Propagate child verts to edge verts.
		int	elev = pl__get_elem(ptHeightMap, cx + quarter_size, cz - quarter_size)->iActivationLevel; // ne
        pl__activate_height_map_element(ee, elev);
        pl__activate_height_map_element(en, elev);

		elev = pl__get_elem(ptHeightMap, cx - quarter_size, cz - quarter_size)->iActivationLevel; // nw
		pl__activate_height_map_element(en, elev);
		pl__activate_height_map_element(ew, elev);

		elev = pl__get_elem(ptHeightMap, cx - quarter_size, cz + quarter_size)->iActivationLevel; // sw
		pl__activate_height_map_element(ew, elev);
		pl__activate_height_map_element(es, elev);

		elev = pl__get_elem(ptHeightMap, cx + quarter_size, cz + quarter_size)->iActivationLevel; // se
		pl__activate_height_map_element(es, elev);
		pl__activate_height_map_element(ee, elev);
	}

	// Propagate edge verts to center.
	plCdLodMapElement*	c = pl__get_elem(ptHeightMap, cx, cz);
	pl__activate_height_map_element(c, ee->iActivationLevel);
	pl__activate_height_map_element(c, en->iActivationLevel);
	pl__activate_height_map_element(c, es->iActivationLevel);
	pl__activate_height_map_element(c, ew->iActivationLevel);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_cdlod_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCdLodI tApi = {
        .initialize        = pl_initialize_cdlod,
        .cleanup           = pl_cleanup_cdlod,
        .process_heightmap = pl_process_cdlod_heightmap
    };
    pl_set_api(ptApiRegistry, plCdLodI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory      = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptMeshBuilder = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
        gptImage       = pl_get_api_latest(ptApiRegistry, plImageI);
        gptFile        = pl_get_api_latest(ptApiRegistry, plFileI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptCdLodCtx = ptDataRegistry->get_data("plCdLodContext");
    }
    else
    {
        static plCdLodContext tCtx = {0};
        gptCdLodCtx = &tCtx;
        ptDataRegistry->set_data("plCdLodContext", gptCdLodCtx);
    }
}

PL_EXPORT void
pl_unload_cdlod_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plCdLodI* ptApi = pl_get_api_latest(ptApiRegistry, plCdLodI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"

#endif