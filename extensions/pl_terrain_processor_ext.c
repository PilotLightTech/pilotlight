/*
   pl_terrain_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] defines
// [SECTION] forward declarations
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal helpers (preprocessing)
// [SECTION] internal helpers (rendering)
// [SECTION] public api implementation
// [SECTION] internal helpers implementation (preprocessing)
// [SECTION] internal helpers implementation (rendering)
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include <limits.h> // UINT_MAX
#include "pl.h"
#include "pl_terrain_processor_ext.h"

// libs
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#undef pl_vnsprintf
#include "pl_memory.h"

// stable extensions
#include "pl_platform_ext.h"
#include "pl_image_ext.h"
#include "pl_profile_ext.h"
#include "pl_graphics_ext.h"
#include "pl_starter_ext.h"
#include "pl_shader_ext.h"
#include "pl_screen_log_ext.h"
#include "pl_draw_ext.h"

// unstable extensions
#include "pl_collision_ext.h"
#include "pl_freelist_ext.h"
#include "pl_camera_ext.h"

// shader interop
#include "pl_shader_interop_terrain.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types for preprocessing
typedef struct  _plEdgeKey          plEdgeKey;
typedef struct _plTerrainMapElement plTerrainMapElement;
typedef struct _plEdgeEntry         plEdgeEntry;
typedef struct _plTrianglePrimitive plTrianglePrimitive;
typedef struct _plTerrainHeightMap  plTerrainHeightMap;

// basic types for rendering
typedef struct _plTerrainChunk           plTerrainChunk;
typedef struct _plTerrainChunkFile       plTerrainChunkFile;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainMapElement
{
    uint32_t uX;
    uint32_t uZ;
    float    fX;
    float    fY;
    float    fZ;
    float    fN0;
    float    fN1;
    float    fError;
    int8_t   iActivationLevel;
    uint32_t uVertexBufferIndex;
} plTerrainMapElement;

typedef struct _plEdgeKey
{
    uint32_t uLeft;
    uint32_t uRight;
    int iLevel;
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

typedef struct _plTerrainHeightMap
{
    bool                 b3dErrorCalc; // true for ellipsoid
    bool                 bEllipsoid;
    int                  iSize;
    int                  iLogSize;
    float                fSampleSpacing;
    float                fMaxBaseError;
    float                fMetersPerPixel;
    float                fMaxHeight;
    float                fMinHeight;
    float                fRadius;
    plVec3               tCenter;
    plTerrainMapElement* atElements;
    const char*          pcHeightMapFile;
    const char*          pcOutputFile;
    plEdgeEntry*         sbtEdges;
    plVec3               tMinBounding;
    plVec3               tMaxBounding;
    uint32_t             uChunkCount;
    plTrianglePrimitive* sbtPrimitives;
} plTerrainHeightMap;

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
    static const plImageI*   gptImage   = NULL;
    static const plFileI*    gptFile    = NULL;
    static const plProfileI* gptProfile = NULL;
    

#endif

#include "pl_ds.h"

// context

//-----------------------------------------------------------------------------
// [SECTION] internal helpers (preprocessing)
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
pl__vertex_index(plTerrainHeightMap* ptHeightMap, int x, int z)
{
    if (x < 0 || x >= ptHeightMap->iSize || z < 0 || z >= ptHeightMap->iSize)
        return -1;
    return ptHeightMap->iSize * z + x;
}

static inline void
pl__activate_height_map_element(plTerrainMapElement* ptElement, int iLevel)
{
    if(iLevel > ptElement->iActivationLevel)
        ptElement->iActivationLevel = (int8_t)iLevel;
}

static inline plTerrainMapElement*
pl__get_elem(plTerrainHeightMap* ptHeightMap, int x, int z)
{
    return &ptHeightMap->atElements[x + z * ptHeightMap->iSize];
}

static inline int
pl__node_index(plTerrainHeightMap* ptHeightMap, int x, int z)
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
pl__mid_activation(plTerrainHeightMap* ptHeightMap, uint32_t uLeft, uint32_t uRight)
{
    plTerrainMapElement* eL = &ptHeightMap->atElements[uLeft];
    plTerrainMapElement* eR = &ptHeightMap->atElements[uRight];

    int iMidX = ((int)eL->uX + (int)eR->uX) / 2;
    int iMidZ = ((int)eL->uZ + (int)eR->uZ) / 2;

    plTerrainMapElement* ptElement = pl__get_elem(ptHeightMap, iMidX, iMidZ);
    return ptElement->iActivationLevel;
}

// Given the triangle, computes an error value and activation level
// for its base vertex, and recurses to child triangles.
static void pl__update(plTerrainHeightMap*, float base_max_error, int ax, int az, int rx, int rz, int lx, int lz);
static void pl__propagate_activation_level(plTerrainHeightMap*, int cx, int cz, int level, int target_level);

// main steps
static void pl__initialize_cdlod_heightmap(plTerrainHeightMap*);
static void pl__terrain_mesh(FILE*, plTerrainHeightMap*, int iStartIndexX, int iStartIndexY, int iLogSize, int iLevel);

static inline plVec2
pl__oct_wrap( plVec2 v )
{
    plVec2 w = {
        .x = 1.0f - fabsf( v.y ),
        .y = 1.0f - fabsf( v.x ),
    };
    if (v.x < 0.0f) w.x = -w.x;
    if (v.y < 0.0f) w.y = -w.y;
    return w;
}
 
plVec2
pl__encode( plVec3 n )
{
    n = pl_div_vec3_scalarf(n, ( fabsf( n.x ) + fabsf( n.y ) + fabsf( n.z ) ));
    n.xy = n.z > 0.0f ? n.xy : pl__oct_wrap( n.xy );
    // n.xy = n.xy * 0.5 + 0.5;
    n.xy = pl_mul_vec2_scalarf(n.xy, 0.5f);
    n.x += 0.5f;
    n.y += 0.5f;
    return n.xy;
}


//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_process_cdlod_heightmap(plTerrainHeightMapInfo tInfo)
{
    plTerrainHeightMap tHeightMap = {
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

    pl_begin_cpu_sample(gptProfile, 0, "pl__initialize_cdlod_heightmap");
    pl__initialize_cdlod_heightmap(&tHeightMap);
    pl_end_cpu_sample(gptProfile, 0);


    // update step

    // Run a view-independent L-K style BTT update on the heightfield, to generate
	// error and activation_level values for each element.

    pl_begin_cpu_sample(gptProfile, 0, "updating");
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, 0, tHeightMap.iSize-1, tHeightMap.iSize-1, tHeightMap.iSize-1, 0, 0);	// sw half of the square
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, tHeightMap.iSize-1, 0, 0, 0, tHeightMap.iSize-1, tHeightMap.iSize-1);	// ne half of the square
    pl_end_cpu_sample(gptProfile, 0);

    // propogate step

	// Propagate the activation_level values of verts to their
	// uParent verts, quadtree LOD style.  Gives same result as L-K.
    pl_begin_cpu_sample(gptProfile, 0, "propogation");
	for(int i = 0; i < tHeightMap.iLogSize; i++)
    {
		pl__propagate_activation_level(&tHeightMap, tHeightMap.iSize >> 1, tHeightMap.iSize >> 1, tHeightMap.iLogSize - 1, i);
		pl__propagate_activation_level(&tHeightMap, tHeightMap.iSize >> 1, tHeightMap.iSize >> 1, tHeightMap.iLogSize - 1, i);
	}
    pl_end_cpu_sample(gptProfile, 0);

    // meshing step

    pl_begin_cpu_sample(gptProfile, 0, "meshing");
    int iRootLevel = tInfo.iTreeDepth - 1;

    FILE* ptDataFile = fopen(tInfo.pcOutputFile, "wb");

    fwrite(&tInfo.iTreeDepth, 1, sizeof(int), ptDataFile);
    fwrite(&tHeightMap.fMaxBaseError, 1, sizeof(float), ptDataFile);

    tHeightMap.uChunkCount = 0x55555555 & ((1 << (tInfo.iTreeDepth*2)) - 1);
    fwrite(&tHeightMap.uChunkCount, 1, sizeof(uint32_t), ptDataFile);

    pl_sb_reserve(tHeightMap.sbtEdges, tHeightMap.iSize * tHeightMap.iSize);

    pl_sb_reserve(tHeightMap.sbtPrimitives, tHeightMap.iSize * tHeightMap.iSize * 2);

    pl__terrain_mesh(ptDataFile, &tHeightMap, 0, 0, tHeightMap.iLogSize, iRootLevel);
    pl_sb_free(tHeightMap.sbtEdges);
    pl_sb_free(tHeightMap.sbtPrimitives);

    PL_FREE(tHeightMap.atElements);

    fclose(ptDataFile);
    pl_end_cpu_sample(gptProfile, 0);
}

static void pl__chlod_read_chunk(plTerrainChunkFile* ptFileOut, int iRecurseCount, FILE* ptDataFile, uint32_t* puCurrentChunk);


bool
pl_terrain_load_chunk_file(const char* pcPath, plTerrainChunkFile* ptFile, uint32_t uFileID)
{
    FILE* ptDataFile = fopen(pcPath, "rb");
    strncpy(ptFile->acFile, pcPath, 128);
    
    if(ptDataFile == NULL)
    {
        return false;
    }
    
    fread(&ptFile->iTreeDepth, 1, sizeof(int), ptDataFile);
    fread(&ptFile->fMaxBaseError, 1, sizeof(float), ptDataFile);
    fread(&ptFile->uChunkCount, 1, sizeof(uint32_t), ptDataFile);

    ptFile->atChunks = PL_ALLOC(ptFile->uChunkCount * sizeof(plTerrainChunk));
    memset(ptFile->atChunks, 0, ptFile->uChunkCount * sizeof(plTerrainChunk));

    uint32_t uCurrentChunk = 0;
    ptFile->atChunks[0].uFileID = uFileID;
    pl__chlod_read_chunk(ptFile, ptFile->iTreeDepth - 1, ptDataFile, &uCurrentChunk);

    fclose(ptDataFile);
    return true;
}

static void
pl__chlod_read_chunk(plTerrainChunkFile* ptFileOut, int iRecurseCount, FILE* ptDataFile, uint32_t* puCurrentChunk)
{
    plTerrainChunk* ptChunk = &ptFileOut->atChunks[*puCurrentChunk];
    ptChunk->szFileLocation = (size_t)ftell(ptDataFile);

    int iChunkLabel = 0;
    fread(&iChunkLabel, 1, sizeof(int), ptDataFile);

    int iLevel = 0;
    fread(&iLevel, 1, sizeof(int), ptDataFile);
    ptChunk->uLevel = (uint8_t)iLevel;

    fread(&ptChunk->tMinBound, 1, sizeof(plVec3), ptDataFile);
    fread(&ptChunk->tMaxBound, 1, sizeof(plVec3), ptDataFile);

    uint32_t uVertexCount = 0;
    fread(&uVertexCount, 1, sizeof(uint32_t), ptDataFile);
    fseek(ptDataFile, sizeof(plTerrainVertex) * uVertexCount, SEEK_CUR);

    uint32_t uIndexCount = 0;
    fread(&uIndexCount, 1, sizeof(uint32_t), ptDataFile);
    fseek(ptDataFile, sizeof(uint32_t) * uIndexCount, SEEK_CUR);

    if(iRecurseCount > 0)
    {
        for(uint32_t i = 0; i < 4; i++)
        {
            ptChunk->aptChildren[i] = &ptFileOut->atChunks[++(*puCurrentChunk)];
            ptChunk->aptChildren[i]->ptParent = ptChunk;
            ptChunk->aptChildren[i]->uFileID = ptChunk->uFileID;
            pl__chlod_read_chunk(ptFileOut, iRecurseCount - 1, ptDataFile, puCurrentChunk);
        }
    }
    else
    {
        for(uint32_t i = 0; i < 4; i++)
        {
            ptChunk->aptChildren[i] = NULL;
        }
    }
}


//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static void
pl__initialize_cdlod_heightmap(plTerrainHeightMap* ptHeightMap)
{
    size_t szFileSize = 0;
    gptFile->binary_read(ptHeightMap->pcHeightMapFile, &szFileSize, NULL);
    uint8_t* puFileData = PL_ALLOC(szFileSize + 1);
    memset(puFileData, 0, szFileSize + 1);
    gptFile->binary_read(ptHeightMap->pcHeightMapFile, &szFileSize, puFileData);

    // load image info
    plImageInfo tImageInfo = {0};
    gptImage->get_info(puFileData, (int)szFileSize, &tImageInfo);
    int iImageWidth = tImageInfo.iWidth;
    int iImageHeight = tImageInfo.iHeight;

    void*          pImageData     = NULL;
    void*          pConvertedData = NULL; // if not loaded as 16 bit
    unsigned char* pucImageData   = NULL; // could be converted or not (aliased)

    int _unused = 0;
    if(tImageInfo.b16Bit)
    {
        uint16_t* puImageData = gptImage->load_16bit(puFileData, (int)szFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
        pucImageData = (unsigned char*)puImageData;
        pImageData = puImageData;
    }
    else if(tImageInfo.bHDR)
    {
        float* pufImageData = gptImage->load_hdr(puFileData, (int)szFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
        uint32_t uPixelCount = (uint32_t)(iImageWidth * iImageHeight);
        uint16_t* puConvertedData = PL_ALLOC(sizeof(uint32_t) * uPixelCount);

        // scale for 16 bit
        for(uint32_t i = 0; i < uPixelCount; i++)
            puConvertedData[i] = (uint16_t)(pufImageData[i] * 65535.0f);

        gptImage->free(pufImageData);
        pucImageData = (unsigned char*)puConvertedData;
        pConvertedData = puConvertedData;

    }
    else
    {
        uint8_t* puImageData = gptImage->load(puFileData, (int)szFileSize, &iImageWidth, &iImageHeight, &_unused, 1);
        uint32_t uPixelCount = (uint32_t)(iImageWidth * iImageHeight);
        uint16_t* puConvertedData = PL_ALLOC(sizeof(uint32_t) * uPixelCount);

        // scale for 16 bit
        for(uint32_t i = 0; i < uPixelCount; i++)
            puConvertedData[i] = (uint16_t)(((float)puImageData[i] / 255.0f) * 65535.0f);

        gptImage->free(puImageData);
        pucImageData = (unsigned char*)puConvertedData;
        pConvertedData = puConvertedData;
    }

    PL_FREE(puFileData);
    puFileData = NULL;

    ptHeightMap->iSize = pl_max(iImageWidth, iImageHeight);
    ptHeightMap->iLogSize = (int) (log2(ptHeightMap->iSize - 1) + 0.5f);

   ptHeightMap->tMinBounding = (plVec3){
        .x = FLT_MAX,
        .y = FLT_MAX,
        .z = FLT_MAX
    };

    ptHeightMap->tMaxBounding = (plVec3){
        .x = -FLT_MAX,
        .y = -FLT_MAX,
        .z = -FLT_MAX
    };

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
            if(i < (uint32_t)iImageWidth && j < (uint32_t)iImageHeight)
            {
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(i + j * iImageWidth) * sizeof(uint16_t)];
                // float fPercentage = (float)uRawValue / (float)UINT16_MAX;
                // float fHeight = (fMaxHeight - fMinHeight) * fPercentage + fMinHeight;
                auHeightMapData[j * ptHeightMap->iSize + i] = uRawValue;
            }
        }
    }

    // Allocate storage.
    int	iSampleCount = ptHeightMap->iSize * ptHeightMap->iSize;
    ptHeightMap->atElements = PL_ALLOC(iSampleCount * sizeof(plTerrainMapElement));
    memset(ptHeightMap->atElements, 0, iSampleCount * sizeof(plTerrainMapElement));

    
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
                uint32_t x = pl_min(i, ptHeightMap->iSize - 2);
                uint32_t y = pl_min(j, ptHeightMap->iSize - 2);

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
                ptHeightMap->atElements[iElementIndex].uVertexBufferIndex = UINT_MAX;


                float fX = ((float)i * fExtent / (float)(ptHeightMap->iSize - 1)) + fMinExtent;
                float fZ = ((float)j * fExtent / (float)(ptHeightMap->iSize - 1)) + fMinExtent;

                fX += ptHeightMap->tCenter.x;
                fZ += ptHeightMap->tCenter.z;

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
                uint32_t x = pl_min(i, ptHeightMap->iSize - 2);
                uint32_t y = pl_min(j, ptHeightMap->iSize - 2);

                uint16_t r = auHeightMapData[y * ptHeightMap->iSize + x];

                // y = (float)r / 65355.0f;	// just using red component for now.
                fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
                fY *= (fMaxHeight - fMinHeight);
                fY += fMinHeight;

                int iElementIndex = i + ptHeightMap->iSize * j;
                ptHeightMap->atElements[iElementIndex].iActivationLevel = -1;
                ptHeightMap->atElements[iElementIndex].uX = i;
                ptHeightMap->atElements[iElementIndex].uZ = j;
                ptHeightMap->atElements[iElementIndex].uVertexBufferIndex = UINT_MAX;
                ptHeightMap->atElements[iElementIndex].fX = (float)i * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.x - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f;
                ptHeightMap->atElements[iElementIndex].fY = fY + ptHeightMap->tCenter.y;
                ptHeightMap->atElements[iElementIndex].fZ = (float)j * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.z - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f;

                if(ptHeightMap->atElements[iElementIndex].fX < ptHeightMap->tMinBounding.x) ptHeightMap->tMinBounding.x = ptHeightMap->atElements[iElementIndex].fX;
                if(ptHeightMap->atElements[iElementIndex].fY < ptHeightMap->tMinBounding.y) ptHeightMap->tMinBounding.y = ptHeightMap->atElements[iElementIndex].fY;
                if(ptHeightMap->atElements[iElementIndex].fZ < ptHeightMap->tMinBounding.z) ptHeightMap->tMinBounding.z = ptHeightMap->atElements[iElementIndex].fZ;
                if(ptHeightMap->atElements[iElementIndex].fX > ptHeightMap->tMaxBounding.x) ptHeightMap->tMaxBounding.x = ptHeightMap->atElements[iElementIndex].fX;
                if(ptHeightMap->atElements[iElementIndex].fY > ptHeightMap->tMaxBounding.y) ptHeightMap->tMaxBounding.y = ptHeightMap->atElements[iElementIndex].fY;
                if(ptHeightMap->atElements[iElementIndex].fZ > ptHeightMap->tMaxBounding.z) ptHeightMap->tMaxBounding.z = ptHeightMap->atElements[iElementIndex].fZ;

            }
        }
    }

    int N = ptHeightMap->iSize;

    if(!ptHeightMap->bEllipsoid)
    {
        float dx = ptHeightMap->fMetersPerPixel;
        float dz = ptHeightMap->fMetersPerPixel;

        for(int j = 0; j < N; ++j)
        {
            for(int i = 0; i < N; ++i)
            {
                int iL = (i > 0)     ? i - 1 : i;
                int iR = (i < N - 1) ? i + 1 : i;
                int jD = (j > 0)     ? j - 1 : j;
                int jU = (j < N - 1) ? j + 1 : j;

                float hL = ptHeightMap->atElements[iL + N*j].fY;
                float hR = ptHeightMap->atElements[iR + N*j].fY;
                float hD = ptHeightMap->atElements[i + N*jD].fY;
                float hU = ptHeightMap->atElements[i + N*jU].fY;

                // Tangents in world units
                plVec3 tX = { 2.0f * dx, (hR - hL), 0.0f };
                plVec3 tZ = { 0.0f,      (hU - hD), 2.0f * dz };

                // Normal (pick cross order for your winding; this gives +Y up for typical XZ plane)
                plVec3 n = pl_cross_vec3(tZ, tX);
                n = pl_norm_vec3(n);

                plVec2 tEncodedN = pl__encode(n);

                int idx = i + N*j;
                ptHeightMap->atElements[idx].fN0 = tEncodedN.x;
                ptHeightMap->atElements[idx].fN1 = tEncodedN.y;
            }
        }
    }

    else
    {
        for(int j = 0; j < N; ++j)
        {
            for(int i = 0; i < N; ++i)
            {
                int iL = (i > 0)     ? i - 1 : i;
                int iR = (i < N - 1) ? i + 1 : i;
                int jD = (j > 0)     ? j - 1 : j;
                int jU = (j < N - 1) ? j + 1 : j;

                plTerrainMapElement* eL = &ptHeightMap->atElements[iL + N*j];
                plTerrainMapElement* eR = &ptHeightMap->atElements[iR + N*j];
                plTerrainMapElement* eD = &ptHeightMap->atElements[i  + N*jD];
                plTerrainMapElement* eU = &ptHeightMap->atElements[i  + N*jU];

                plVec3 pL = { eL->fX, eL->fY, eL->fZ };
                plVec3 pR = { eR->fX, eR->fY, eR->fZ };
                plVec3 pD = { eD->fX, eD->fY, eD->fZ };
                plVec3 pU = { eU->fX, eU->fY, eU->fZ };

                // Tangents are now true 3D directions along the surface sampling grid
                plVec3 tX = pl_sub_vec3(pR, pL);
                plVec3 tZ = pl_sub_vec3(pU, pD);

                plVec3 n = pl_cross_vec3(tZ, tX);   // swap order if flipped
                n = pl_norm_vec3(n);

                // Optional: enforce outward normal for a planet centered at ptHeightMap->tCenter
                // (prevents occasional flips near steep/degenerate spots)
                // plVec3 center = ptHeightMap->tCenter;
                // plVec3 pC = { ptHeightMap->atElements[i + N*j].fX,
                //             ptHeightMap->atElements[i + N*j].fY,
                //             ptHeightMap->atElements[i + N*j].fZ };
                // plVec3 out = pl_norm_vec3(pl_sub_vec3(pC, center));
                // if(pl_dot_vec3(n, out) < 0.0f)
                //     n = pl_mul_vec3_scalarf(n, -1.0f);

                plVec2 tEncodedN = pl__encode(n);

                int idx = i + N*j;
                ptHeightMap->atElements[idx].fN0 = tEncodedN.x;
                ptHeightMap->atElements[idx].fN1 = tEncodedN.y;
            }
        }
    }
    
    PL_FREE(auHeightMapData);
    auHeightMapData = NULL;

    if(pImageData)
        gptImage->free(pImageData);
    if(pConvertedData)
    {
        PL_FREE(pConvertedData);
    }
}

static void
pl__terrain_mesh(FILE* ptFile, plTerrainHeightMap* ptHeightMap, int iStartIndexX, int iStartIndexY, int iLogSize, int iLevel)
{
    static int uChunk = 0;
    printf("%d of %u\n", uChunk++, ptHeightMap->uChunkCount);

    plHashMap tEdgeHash = {0};

    int	iSize = (1 << iLogSize);
    int iEndIndexY = iStartIndexY + iSize;
    int iEndIndexX = iStartIndexX + iSize;
    int iMaxLevel = iLogSize * 2;

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

    plTerrainMapElement* ptRootElement0 = &(ptHeightMap->atElements[tPrimitiveRoot0.uLeft]);
    plTerrainMapElement* ptRootElement1 = &(ptHeightMap->atElements[tPrimitiveRoot1.uLeft]);

    pl_sb_push(ptHeightMap->sbtPrimitives, tPrimitiveRoot0);
    pl_sb_push(ptHeightMap->sbtPrimitives, tPrimitiveRoot1);

    gptProfile->begin_sample(0, "generate triangles at finest level");
    
    // generate triangles at finest level
    while(pl_sb_size(ptHeightMap->sbtPrimitives) > 0)
    {

        plTrianglePrimitive tPrimitive = pl_sb_pop(ptHeightMap->sbtPrimitives);

        // assign triangle based on id
        atTriangles[tPrimitive.iId] = tPrimitive;

        plTerrainMapElement* ptElement0 = &(ptHeightMap->atElements[tPrimitive.uApex]);
        plTerrainMapElement* ptElement1 = &(ptHeightMap->atElements[tPrimitive.uRight]);
        plTerrainMapElement* ptElement2 = &(ptHeightMap->atElements[tPrimitive.uLeft]);
        ptElement0->uVertexBufferIndex = UINT32_MAX;
        ptElement1->uVertexBufferIndex = UINT32_MAX;
        ptElement2->uVertexBufferIndex = UINT32_MAX;

        // we've reached target level, no more splitting necessary
        if(tPrimitive.iLevel == iMaxLevel)
        {
            atPresent[tPrimitive.iId] = true;
            continue;
        }

        // split triangle into children

        const int iChildLevel = tPrimitive.iLevel + 1;

        const int iMidPointX = ((int)ptElement1->uX + (int)ptElement2->uX) / 2;
        const int iMidPointZ = ((int)ptElement1->uZ + (int)ptElement2->uZ) / 2;
        const uint32_t uMidPoint = pl__vertex_index(ptHeightMap, iMidPointX, iMidPointZ);

        plTrianglePrimitive tChild0 = {
            .iId    = tPrimitive.iId << 1u,
            .iLevel = iChildLevel,
            .uApex  = uMidPoint,
            .uLeft  = tPrimitive.uApex,
            .uRight = tPrimitive.uLeft
        };

        plTrianglePrimitive tChild1 = {
            .iId    = (tPrimitive.iId << 1u) | 1u,
            .iLevel = iChildLevel,
            .uApex  = uMidPoint,
            .uLeft  = tPrimitive.uRight,
            .uRight = tPrimitive.uApex
        };
        pl_sb_push(ptHeightMap->sbtPrimitives, tChild0);
        pl_sb_push(ptHeightMap->sbtPrimitives, tChild1);
    }
    pl_sb_reset(ptHeightMap->sbtPrimitives);

    gptProfile->end_sample(0);

    gptProfile->begin_sample(0, "simplify mesh");

    // simply mesh starting from finest level
    pl_hm_free(&tEdgeHash);
    pl_sb_reset(ptHeightMap->sbtEdges);
    for(int iCurrentLevel = iMaxLevel - 1; iCurrentLevel >= 0; iCurrentLevel--)
    {

        // build base-edge adjacency for PARENTS at level iCurrentLevel
        for(uint32_t uParent = 1; uParent < uMaxId; uParent++)
        {
            if(atTriangles[uParent].iLevel != iCurrentLevel)
                continue;

            const uint32_t uChild0 = uParent << 1u;
            const uint32_t uChild1 = (uParent << 1u) | 1u;

            if(uChild1 >= uMaxId)
                continue;

            // uParent is currently split if BOTH children are present
            if(!atPresent[uChild0] || !atPresent[uChild1])
                continue;

            plTrianglePrimitive tParent = atTriangles[uParent];

            plEdgeKey tEdge = {
                .iLevel = iCurrentLevel,
                .uLeft  = pl_min(tParent.uLeft, tParent.uRight),
                .uRight = pl_max(tParent.uLeft, tParent.uRight)
            };

            // check if edge mate already registered
            uint64_t uHash = pl_hm_hash(&tEdge, sizeof(plEdgeKey), 0);
            if(pl_hm_has_key(&tEdgeHash, uHash))
            {
                uint64_t idx = pl_hm_lookup(&tEdgeHash, uHash);
                ptHeightMap->sbtEdges[idx].t1 = uParent;
            }
            else // first time edge registering
            {
                uint64_t idx = pl_sb_size(ptHeightMap->sbtEdges);
                pl_sb_add(ptHeightMap->sbtEdges);
                ptHeightMap->sbtEdges[idx].t0 = uParent;
                ptHeightMap->sbtEdges[idx].t1 = UINT32_MAX;

                pl_hm_insert(&tEdgeHash, uHash, idx);
            }
        }

        // merge triangles
        for(uint32_t i = 1; i < uMaxId; i++)
        {
            // skip if triangle already discarded
            if(!atPresent[i])
                continue;

            // only processing children
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
                .iLevel = iCurrentLevel,
                .uLeft  = pl_min(atTriangles[uParent].uLeft, atTriangles[uParent].uRight),
                .uRight = pl_max(atTriangles[uParent].uLeft, atTriangles[uParent].uRight)
            };
            uint64_t uHash = pl_hm_hash(&tEdgeKey, sizeof(plEdgeKey), 0);

            if(!pl_hm_has_key(&tEdgeHash, uHash))
            {
                // TODO: add flag for border options
                // atPresent[i] = false;
                // atPresent[uSibling] = false;
                // atPresent[uParent] = true;
                continue;
            }

            uint64_t uEdgeIndex = pl_hm_lookup(&tEdgeHash, uHash);
            plEdgeEntry tEdgeEntry = ptHeightMap->sbtEdges[uEdgeIndex];

            // pick the other PARENT in the diamond
            uint32_t uMateParent = (tEdgeEntry.t0 == uParent) ? tEdgeEntry.t1 : tEdgeEntry.t0;
            if(uMateParent == UINT32_MAX || uMateParent == 0 || uMateParent >= uMaxId)
            {
                // TODO: add flag for border options
                atPresent[i] = false;
                atPresent[uSibling] = false;
                atPresent[uParent] = true;
                continue; // boundary / missing mate
            }

            // uMateParent must currently be split too
            uint32_t uMateChild0 = uMateParent << 1u;
            uint32_t uMateChild1 = (uMateParent << 1u) | 1u;
            if(uMateChild1 >= uMaxId)
                continue;
            if(!atPresent[uMateChild0] || !atPresent[uMateChild1])
                continue;

            // mate side midpoint must also allow merge
            int iActivationB = pl__mid_activation(ptHeightMap, atTriangles[uMateParent].uLeft, atTriangles[uMateParent].uRight);
            if(iActivationB >= iLevel)
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
    pl_sb_reset(ptHeightMap->sbtEdges);

    gptProfile->end_sample(0);

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

    gptProfile->begin_sample(0, "submit");

    uint32_t uPresentCount = 0;
    uint32_t uVertexCount = 0;
    for(uint32_t i = 1; i < uMaxId; i++)
    {

        if(!atPresent[i])
            continue;
        uPresentCount++;

        plTrianglePrimitive tPrimitive = atTriangles[i];

        plTerrainMapElement* ptElement0 = &(ptHeightMap->atElements[tPrimitive.uApex]);
        plTerrainMapElement* ptElement1 = &(ptHeightMap->atElements[tPrimitive.uRight]);
        plTerrainMapElement* ptElement2 = &(ptHeightMap->atElements[tPrimitive.uLeft]);

        if(ptElement0->uVertexBufferIndex == UINT32_MAX)
        {
            ptElement0->uVertexBufferIndex = uVertexCount++;
        }
        if(ptElement1->uVertexBufferIndex == UINT32_MAX)
        {
            ptElement1->uVertexBufferIndex = uVertexCount++;
        }
        if(ptElement2->uVertexBufferIndex == UINT32_MAX)
        {
            ptElement2->uVertexBufferIndex = uVertexCount++;
        }

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
    }

    plTerrainVertex* atVertexData = PL_ALLOC(uVertexCount * sizeof(plTerrainVertex));
    uint32_t uIndexCount = 3 * uPresentCount;
    uint32_t* atIndexData = PL_ALLOC(uIndexCount * sizeof(uint32_t));

    uint32_t uCurrentIndex = 0;

    // submit present triangles
    for(uint32_t i = 1; i < uMaxId; i++)
    {

        if(!atPresent[i])
            continue;

        plTrianglePrimitive tPrimitive = atTriangles[i];

        plTerrainMapElement* ptElement0 = &(ptHeightMap->atElements[tPrimitive.uApex]);
        plTerrainMapElement* ptElement1 = &(ptHeightMap->atElements[tPrimitive.uRight]);
        plTerrainMapElement* ptElement2 = &(ptHeightMap->atElements[tPrimitive.uLeft]);

        atVertexData[ptElement0->uVertexBufferIndex].tPosition.x = ptElement0->fX;
        atVertexData[ptElement0->uVertexBufferIndex].tPosition.y = ptElement0->fY;
        atVertexData[ptElement0->uVertexBufferIndex].tPosition.z = ptElement0->fZ;

        atVertexData[ptElement1->uVertexBufferIndex].tPosition.x = ptElement1->fX;
        atVertexData[ptElement1->uVertexBufferIndex].tPosition.y = ptElement1->fY;
        atVertexData[ptElement1->uVertexBufferIndex].tPosition.z = ptElement1->fZ;

        atVertexData[ptElement2->uVertexBufferIndex].tPosition.x = ptElement2->fX;
        atVertexData[ptElement2->uVertexBufferIndex].tPosition.y = ptElement2->fY;
        atVertexData[ptElement2->uVertexBufferIndex].tPosition.z = ptElement2->fZ;

        atVertexData[ptElement0->uVertexBufferIndex].tNormal.x = ptElement0->fN0;
        atVertexData[ptElement0->uVertexBufferIndex].tNormal.y = ptElement0->fN1;

        atVertexData[ptElement1->uVertexBufferIndex].tNormal.x = ptElement1->fN0;
        atVertexData[ptElement1->uVertexBufferIndex].tNormal.y = ptElement1->fN1;

        atVertexData[ptElement2->uVertexBufferIndex].tNormal.x = ptElement2->fN0;
        atVertexData[ptElement2->uVertexBufferIndex].tNormal.y = ptElement2->fN1;

        uint32_t uIndex0 = ptElement0->uVertexBufferIndex;
        uint32_t uIndex1 = ptElement1->uVertexBufferIndex;
        uint32_t uIndex2 = ptElement2->uVertexBufferIndex;
        atIndexData[uCurrentIndex + 0] = uIndex0;
        atIndexData[uCurrentIndex + 1] = uIndex2;
        atIndexData[uCurrentIndex + 2] = uIndex1;
        uCurrentIndex += 3;
    }

    gptProfile->end_sample(0);

    PL_FREE(atTriangles);
    PL_FREE(atPresent);
    atTriangles = NULL;
    atPresent = NULL;

    fwrite(&tMinBounding, 1, sizeof(plVec3), ptFile);
    fwrite(&tMaxBounding, 1, sizeof(plVec3), ptFile);

    fwrite(&uVertexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atVertexData, 1, sizeof(plTerrainVertex) * uVertexCount, ptFile);

    fwrite(&uIndexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atIndexData, 1, sizeof(uint32_t) * uIndexCount, ptFile);

    PL_FREE(atVertexData);
    PL_FREE(atIndexData);

    if(iLevel > 0)
    {

        iHalfSize = (1 << (iLogSize - 1));
        // [nw, ne, sw, se]
        pl__terrain_mesh(ptFile, ptHeightMap,             iStartIndexX,         iStartIndexY + 0, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + iHalfSize,         iStartIndexY + 0, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap,         iStartIndexX + 0, iStartIndexY + iHalfSize, iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + iHalfSize, iStartIndexY + iHalfSize, iLogSize - 1, iLevel - 1);
    }
}

static void
pl__update(plTerrainHeightMap* ptHeightMap, float fBaseMaxError, int iApexX, int iApexZ, int iRightX, int iRightZ, int iLeftX, int iLeftZ)
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

    plTerrainMapElement* ptBaseElement = pl__get_elem(ptHeightMap, iBaseX, iBaseZ);
    plTerrainMapElement* ptLeftElement = pl__get_elem(ptHeightMap, iLeftX, iLeftZ);
    plTerrainMapElement* ptRightElement = pl__get_elem(ptHeightMap, iRightX, iRightZ);

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
		plTerrainMapElement* ptElem = pl__get_elem(ptHeightMap, iBaseX, iBaseZ);
        pl__activate_height_map_element(ptElem, iActivationLevel);
	}

	// recurse to child triangles
	pl__update(ptHeightMap, fBaseMaxError, iBaseX, iBaseZ, iApexX, iApexZ, iRightX, iRightZ); // base, apex, right
	pl__update(ptHeightMap, fBaseMaxError, iBaseX, iBaseZ, iLeftX, iLeftZ, iApexX, iApexZ);	  // base, left, apex
}


static void
pl__propagate_activation_level(plTerrainHeightMap* ptHeightMap, int cx, int cz, int level, int target_level)
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
	plTerrainMapElement*	ee = pl__get_elem(ptHeightMap, cx + half_size, cz);
	plTerrainMapElement*	en = pl__get_elem(ptHeightMap, cx, cz - half_size);
	plTerrainMapElement*	ew = pl__get_elem(ptHeightMap, cx - half_size, cz);
	plTerrainMapElement*	es = pl__get_elem(ptHeightMap, cx, cz + half_size);
	
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
	plTerrainMapElement*	c = pl__get_elem(ptHeightMap, cx, cz);
	pl__activate_height_map_element(c, ee->iActivationLevel);
	pl__activate_height_map_element(c, en->iActivationLevel);
	pl__activate_height_map_element(c, es->iActivationLevel);
	pl__activate_height_map_element(c, ew->iActivationLevel);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_terrain_processor_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plTerrainProcessorI tApi = {
        .process_heightmap = pl_process_cdlod_heightmap,
        .load_chunk_file = pl_terrain_load_chunk_file,
    };
    pl_set_api(ptApiRegistry, plTerrainProcessorI, &tApi);

    #ifndef PL_UNITY_BUILD
        gptMemory  = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptImage   = pl_get_api_latest(ptApiRegistry, plImageI);
        gptFile    = pl_get_api_latest(ptApiRegistry, plFileI);
        gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    #endif

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
}

PL_EXPORT void
pl_unload_terrain_processor_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plTerrainProcessorI* ptApi = pl_get_api_latest(ptApiRegistry, plTerrainProcessorI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD

    #define PL_MEMORY_IMPLEMENTATION
    #include "pl_memory.h"

#endif
