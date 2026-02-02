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
    int16_t  iX;
    int16_t  iZ;
    float    fY;
    float    fError;
    uint8_t  uFrameStamp;
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
    uint8_t  uLevel;
    uint32_t uApex;
    uint32_t uLeft;
    uint32_t uRight;
} plTrianglePrimitive;

typedef struct _plTerrainHeightMap
{
    uint32_t             uRequestedSize;
    int                  iSize;
    int                  iLogSize;
    float                fSampleSpacing;
    float                fMaxBaseError;
    float                fMetersPerPixel;
    float                fMaxHeight;
    float                fMinHeight;
    plVec3               tCenter;
    plTerrainMapElement* atElements;
    const char*          pcOutputFile;
    plEdgeEntry*         sbtEdges;
    plVec3               tMinBounding;
    plVec3               tMaxBounding;
    uint32_t             uChunkCount;
    uint8_t              uFrameStamp;
    plTrianglePrimitive* sbtPrimitives;

    plTerrainMapElement* atHaloElements;
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

    int iMidX = ((int)eL->iX + (int)eR->iX) / 2;
    int iMidZ = ((int)eL->iZ + (int)eR->iZ) / 2;

    plTerrainMapElement* ptElement = pl__get_elem(ptHeightMap, iMidX, iMidZ);
    return ptElement->iActivationLevel;
}

// Given the triangle, computes an error value and activation level
// for its base vertex, and recurses to child triangles.
static void pl__update(plTerrainHeightMap*, float base_max_error, int ax, int az, int rx, int rz, int lx, int lz);
static void pl__propagate_activation_level(plTerrainHeightMap*, int cx, int cz, int level, int target_level);

// main steps
static void pl__initialize_cdlod_heightmap(plTerrainHeightMap*, uint32_t, plTerrainTileInfo*, plTerrainTileInfo*);
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
 
static inline plVec2
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

static plVec3 pl__get_cartesian(plTerrainHeightMap*, plTerrainMapElement*);
static plVec2 pl__get_normal(plTerrainHeightMap*, plTerrainMapElement*);

#define PL_TERRAIN_SET_PRESENT(INDEX) atPresent[INDEX >> 3] |= (uint8_t)(1u << (INDEX & 7))
#define PL_TERRAIN_UNSET_PRESENT(INDEX) atPresent[INDEX >> 3] &= ~(uint8_t)(1u << (INDEX & 7))
#define PL_TERRAIN_PRESENT(INDEX) (bool)(atPresent[INDEX >> 3] & (uint8_t)(1u << (INDEX & 7)))


//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

void
pl_process_cdlod_heightmap(plTerrainHeightMapInfo tInfo)
{
    plTerrainHeightMap tHeightMap = {
        .fSampleSpacing  = 1.0f,
        .fMaxBaseError   = tInfo.fMaxBaseError,
        .fMetersPerPixel = tInfo.fMetersPerPixel,
        .fMaxHeight      = tInfo.fMaxHeight,
        .fMinHeight      = tInfo.fMinHeight,
        .tCenter         = tInfo.tCenter,
        .uRequestedSize  = tInfo.uSize,
        .pcOutputFile    = tInfo.pcOutputFile
    };


    pl__initialize_cdlod_heightmap(&tHeightMap, tInfo.uTileCount, tInfo.atTiles, tInfo.atHaloTiles);

    // update step

    // Run a view-independent L-K style BTT update on the heightfield, to generate
	// error and activation_level values for each element.

    printf("updating 1\n");
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, 0, tHeightMap.iSize-1, tHeightMap.iSize-1, tHeightMap.iSize-1, 0, 0);	// sw half of the square

    printf("updating 2\n");
	pl__update(&tHeightMap, tHeightMap.fMaxBaseError, tHeightMap.iSize-1, 0, 0, 0, tHeightMap.iSize-1, tHeightMap.iSize-1);	// ne half of the square

    // propogate step

	// Propagate the activation_level values of verts to their
	// uParent verts, quadtree LOD style.  Gives same result as L-K.
    printf("propogating\n");
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

    tHeightMap.uChunkCount = 0x55555555 & ((1 << (tInfo.iTreeDepth*2)) - 1);
    fwrite(&tHeightMap.uChunkCount, 1, sizeof(uint32_t), ptDataFile);

    printf("meshing\n");
    pl__terrain_mesh(ptDataFile, &tHeightMap, 0, 0, tHeightMap.iLogSize, iRootLevel);

    PL_FREE(tHeightMap.atElements);
    PL_FREE(tHeightMap.atHaloElements);

    fclose(ptDataFile);
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

    if(uVertexCount == 0 || uIndexCount == 0)
    {
        int a = 5;
    }

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
pl__initialize_cdlod_heightmap(plTerrainHeightMap* ptHeightMap, uint32_t uTileCount, plTerrainTileInfo* atTiles, plTerrainTileInfo* atHaloTiles)
{

    ptHeightMap->uFrameStamp = 0;
    ptHeightMap->iSize = (int)ptHeightMap->uRequestedSize;
    ptHeightMap->iLogSize = (int) (log2(ptHeightMap->iSize - 1) + 0.5f);

    // expand the heightfield dimension to contain the bitmap.
    while (((1 << ptHeightMap->iLogSize) + 1) < ptHeightMap->iSize)
        ptHeightMap->iLogSize++;
    ptHeightMap->iSize = (1 << ptHeightMap->iLogSize) + 1;

    size_t szHeightMapSize = ptHeightMap->iSize * ptHeightMap->iSize * sizeof(uint16_t);

    uint16_t* auHeightMapData = PL_ALLOC(szHeightMapSize);
    memset(auHeightMapData, 0, szHeightMapSize);

    uint16_t* auHaloHeightMapData = PL_ALLOC((4 * (ptHeightMap->iSize - 1) + 2) * sizeof(uint16_t));
    memset(auHaloHeightMapData, 0, (4 * (ptHeightMap->iSize - 1) + 2) * sizeof(uint16_t));

    for(uint32_t uTileIndex = 0; uTileIndex < 7; uTileIndex++)
    {
        if(atHaloTiles[uTileIndex].acHeightMapFile[0] == 0)
            continue;
        
        size_t szFileSize = 0;
        gptFile->binary_read(atHaloTiles[uTileIndex].acHeightMapFile, &szFileSize, NULL);
        uint8_t* puFileData = PL_ALLOC(szFileSize + 1);
        memset(puFileData, 0, szFileSize + 1);
        gptFile->binary_read(atHaloTiles[uTileIndex].acHeightMapFile, &szFileSize, puFileData);

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

        if(uTileIndex == 0) // north
        {
            for(uint32_t i = 0; i < (uint32_t)iImageWidth; i++)
            {
                int x = i;
                int y = (iImageHeight - 1);
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // south edge
                auHaloHeightMapData[i] = uRawValue;
            }
        }
        else if(uTileIndex == 1) // northeast
        {
            int x = 0;
            int y = iImageHeight - 1;
            uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // southwest corner
            auHaloHeightMapData[(ptHeightMap->iSize - 1) * 4 + 0] = uRawValue;
        }
        else if(uTileIndex == 2) // east
        {

            for(uint32_t i = 0; i < (uint32_t)iImageHeight; i++)
            {
                int x = 0;
                int y = i;
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // west edge
                int xdest = ptHeightMap->iSize - 1;
                int ydest = i;
                auHeightMapData[xdest + ydest * ptHeightMap->iSize] = uRawValue;
            }

            for(uint32_t i = 0; i < (uint32_t)iImageHeight; i++)
            {
                int x = 1;
                int y = i;
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // west edge
                auHaloHeightMapData[(ptHeightMap->iSize - 1) * 1 + i] = uRawValue;
            }
        }
        else if(uTileIndex == 3) //  southeast
        {
            int x = 0;
            int y = 0;
            uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // southwest corner
            int xdest = ptHeightMap->iSize - 1;
            int ydest = ptHeightMap->iSize - 1;
            auHeightMapData[xdest + ydest * ptHeightMap->iSize] = uRawValue;
        }
        else if(uTileIndex == 4) // south
        {
            for(uint32_t i = 0; i < (uint32_t)iImageWidth; i++)
            {
                int x = i;
                int y = 0;
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // north edge
                int xdest = i;
                int ydest = ptHeightMap->iSize - 1;
                auHeightMapData[xdest + ydest * ptHeightMap->iSize] = uRawValue;
            }
            for(uint32_t i = 0; i < (uint32_t)iImageWidth; i++)
            {
                int x = i;
                int y = 2;
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // north edge
                int xdest = i;
                int ydest = ptHeightMap->iSize - 1;
                auHaloHeightMapData[(ptHeightMap->iSize - 1) * 2 + i] = uRawValue;
            }
        }
        else if(uTileIndex == 5) //  southwest
        {
            int x = iImageWidth - 1;
            int y = 0;
            uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // northeast corner
            auHaloHeightMapData[(ptHeightMap->iSize - 1) * 4 + 1] = uRawValue;
        }
        else if(uTileIndex == 6) // west
        {
            for(uint32_t i = 0; i < (uint32_t)iImageHeight; i++)
            {
                int x = iImageWidth - 1;
                int y = i;
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(x + y * iImageWidth) * sizeof(uint16_t)]; // east edge
                auHaloHeightMapData[(ptHeightMap->iSize - 1) * 3 + i] = uRawValue;
            }
        }

        if(pImageData)
            gptImage->free(pImageData);
        if(pConvertedData)
        {
            PL_FREE(pConvertedData);
        }
    }

    for(uint32_t uTileIndex = 0; uTileIndex < uTileCount; uTileIndex++)
    {
        size_t szFileSize = 0;
        gptFile->binary_read(atTiles[uTileIndex].acHeightMapFile, &szFileSize, NULL);
        uint8_t* puFileData = PL_ALLOC(szFileSize + 1);
        memset(puFileData, 0, szFileSize + 1);
        gptFile->binary_read(atTiles[uTileIndex].acHeightMapFile, &szFileSize, puFileData);

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

        uint32_t uMaxX = atTiles[uTileIndex].uXOffset + (uint32_t)iImageWidth;
        uint32_t uMaxY = atTiles[uTileIndex].uYOffset + (uint32_t)iImageHeight;

        for(uint32_t i = 0; i < (uint32_t)iImageWidth; i++)
        {
            for(uint32_t j = 0; j < (uint32_t)iImageHeight; j++)
            {
                uint16_t uRawValue = *(uint16_t*)&pucImageData[(i + j * iImageWidth) * sizeof(uint16_t)];

                uint32_t uGlobalXIndex = i + atTiles[uTileIndex].uXOffset;
                uint32_t uGlobalYIndex = j + atTiles[uTileIndex].uYOffset;

                auHeightMapData[uGlobalYIndex * ptHeightMap->iSize + uGlobalXIndex] = uRawValue;
            }
        }

        if(pImageData)
            gptImage->free(pImageData);
        if(pConvertedData)
        {
            PL_FREE(pConvertedData);
        }
    }



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

    printf("Loaded images\n");

    // Allocate storage.
    int	iSampleCount = ptHeightMap->iSize * ptHeightMap->iSize;
    ptHeightMap->atElements = PL_ALLOC(iSampleCount * sizeof(plTerrainMapElement));
    memset(ptHeightMap->atElements, 0, iSampleCount * sizeof(plTerrainMapElement));

    ptHeightMap->atHaloElements = PL_ALLOC((4 * (ptHeightMap->iSize - 1) + 3)  * sizeof(plTerrainMapElement));
    memset(ptHeightMap->atHaloElements, 0, (4 * (ptHeightMap->iSize - 1) + 3) * sizeof(plTerrainMapElement));

    // Initialize the data.
    for (int j = 0; j < ptHeightMap->iSize; j++)
    {
        for (int i = 0; i < ptHeightMap->iSize; i++)
        {
            float fY = 0.0f;

            // Extract a height value from the pixel data.
            uint32_t x = i;
            uint32_t y = j;

            uint16_t r = auHeightMapData[y * ptHeightMap->iSize + x];

            // y = (float)r / 65355.0f;	// just using red component for now.
            fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
            fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
            fY += ptHeightMap->fMinHeight;

            int iElementIndex = i + ptHeightMap->iSize * j;
            ptHeightMap->atElements[iElementIndex].iActivationLevel = -1;
            ptHeightMap->atElements[iElementIndex].iX = (int16_t)i;
            ptHeightMap->atElements[iElementIndex].iZ = (int16_t)j;
            ptHeightMap->atElements[iElementIndex].uVertexBufferIndex = UINT_MAX;
            ptHeightMap->atElements[iElementIndex].fY = fY;
            ptHeightMap->atElements[iElementIndex].uFrameStamp = 0;
        }
    }

    // north
    for (int i = 0; i < ptHeightMap->iSize - 1; i++)
    {
        int iElementIndex = i;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        
        ptHeightMap->atHaloElements[iElementIndex].iX = (int16_t)i;
        ptHeightMap->atHaloElements[iElementIndex].iZ = -1;
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    // east
    for (int i = 0; i < ptHeightMap->iSize - 1; i++)
    {
        int iElementIndex = (ptHeightMap->iSize - 1) * 1 + i;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        ptHeightMap->atHaloElements[iElementIndex].iX = (int16_t)ptHeightMap->iSize + 1;
        ptHeightMap->atHaloElements[iElementIndex].iZ = (int16_t)i;
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    // south
    for (int i = 0; i < ptHeightMap->iSize - 1; i++)
    {
        int iElementIndex = (ptHeightMap->iSize - 1) * 2 + i;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        
        ptHeightMap->atHaloElements[iElementIndex].iX = (int16_t)i;
        ptHeightMap->atHaloElements[iElementIndex].iZ = (int16_t)ptHeightMap->iSize + 1;
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    // west
    for (int i = 0; i < ptHeightMap->iSize - 1; i++)
    {
        int iElementIndex = (ptHeightMap->iSize - 1) * 3 + i;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        ptHeightMap->atHaloElements[iElementIndex].iX = -1;
        ptHeightMap->atHaloElements[iElementIndex].iZ = (int16_t)i;
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    {
        int iElementIndex = (ptHeightMap->iSize - 1) * 4 + 0;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        ptHeightMap->atHaloElements[iElementIndex].iX = (int16_t)(ptHeightMap->iSize - 1);
        ptHeightMap->atHaloElements[iElementIndex].iZ = -1;
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    {
        int iElementIndex = (ptHeightMap->iSize - 1) * 4 + 1;
        uint16_t r = auHaloHeightMapData[iElementIndex];

        // y = (float)r / 65355.0f;	// just using red component for now.
        float fY = (float)r / (float)UINT16_MAX;	// just using red component for now.
        fY *= (ptHeightMap->fMaxHeight - ptHeightMap->fMinHeight);
        fY += ptHeightMap->fMinHeight;

        ptHeightMap->atHaloElements[iElementIndex].iX = -1;
        ptHeightMap->atHaloElements[iElementIndex].iZ = (int16_t)(ptHeightMap->iSize - 1);
        ptHeightMap->atHaloElements[iElementIndex].fY = fY;
    }

    PL_FREE(auHeightMapData);
    auHeightMapData = NULL;

    PL_FREE(auHaloHeightMapData);
    auHaloHeightMapData = NULL;
}

static inline plEdgeKey
pl__base_edge_key(uint8_t uLevel, uint32_t uLeft, uint32_t uRight)
{
    plEdgeKey k;
    k.iLevel = (int)uLevel;
    k.uLeft  = pl_min(uLeft, uRight);
    k.uRight = pl_max(uLeft, uRight);
    return k;
}

static void
pl__terrain_mesh(FILE* ptFile, plTerrainHeightMap* ptHeightMap, int iStartIndexX, int iStartIndexY, int iLogSize, int iLevel)
{
    static int uChunk = 0;
    printf("%d of %u\n", uChunk++, ptHeightMap->uChunkCount);

    ptHeightMap->uFrameStamp++;
    if(ptHeightMap->uFrameStamp == 0) ptHeightMap->uFrameStamp++;

    const int iSize      = (1 << iLogSize);
    const int iEndIndexY = iStartIndexY + iSize;
    const int iEndIndexX = iStartIndexX + iSize;
    const uint8_t uMaxLevel = (uint8_t)(iLogSize * 2);

    const int iHalfSize = iSize >> 1;
    const int iCx = iStartIndexX + iHalfSize;
    const int iCz = iStartIndexY + iHalfSize;

    int iChunkLabel = pl__node_index(ptHeightMap, iCx, iCz);
    fwrite(&iChunkLabel, 1, sizeof(int), ptFile);
    fwrite(&iLevel,      1, sizeof(int), ptFile);

    // activate the 4 corners
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iStartIndexX, iStartIndexY), iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iStartIndexX, iEndIndexY),   iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iEndIndexX,   iEndIndexY),   iLevel);
    pl__activate_height_map_element(pl__get_elem(ptHeightMap, iEndIndexX,   iStartIndexY), iLevel);

    // roots
    plTrianglePrimitive tRoot0 = {
        .uLevel = (uint8_t)iLevel,
        .uApex  = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX,   iStartIndexY),
        .uLeft  = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iStartIndexY),
        .uRight = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX,   iEndIndexY)
    };

    plTrianglePrimitive tRoot1 = {
        .uLevel = (uint8_t)iLevel,
        .uApex  = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iEndIndexY),
        .uLeft  = (uint32_t)pl__vertex_index(ptHeightMap, iEndIndexX,   iEndIndexY),
        .uRight = (uint32_t)pl__vertex_index(ptHeightMap, iStartIndexX, iStartIndexY)
    };

    // top-down refinement -----------------------------

    plTrianglePrimitive* sbtWork   = NULL;
    plTrianglePrimitive* sbtLeaves = NULL;

    pl_sb_push(sbtWork, tRoot0);
    pl_sb_push(sbtWork, tRoot1);

    // “force mate to split” table keyed by base-edge at a given level
    // value is unused; we just need membership
    plHashMap tRequiredSplit = {0};

    while(pl_sb_size(sbtWork) > 0)
    {
        plTrianglePrimitive t = pl_sb_pop(sbtWork);

        // Stop if already at max depth
        if(t.uLevel == uMaxLevel)
        {
            pl_sb_push(sbtLeaves, t);
            continue;
        }

        // If our base-edge was marked as “must split”, force it
        plEdgeKey tBaseKey = pl__base_edge_key(t.uLevel, t.uLeft, t.uRight);
        uint64_t uBaseHash = pl_hm_hash(&tBaseKey, sizeof(plEdgeKey), 0);
        const bool bForced = pl_hm_has_key(&tRequiredSplit, uBaseHash);

        // If midpoint activation is >= iLevel => we MUST keep the split => so top-down we split.
        const int iActivation = pl__mid_activation(ptHeightMap, t.uLeft, t.uRight);
        const bool bShouldSplit = bForced || (iActivation >= iLevel);

        if(!bShouldSplit)
        {
            // keep as leaf
            pl_sb_push(sbtLeaves, t);
            continue;
        }

        // If we split this triangle, its mate across the base must also be split to avoid T-junctions.
        // Mark the base-edge as “required split” at this level. When mate is encountered it will split too.
        // (If the mate never exists because boundary, this is harmless.)
        if(!pl_hm_has_key(&tRequiredSplit, uBaseHash))
            pl_hm_insert(&tRequiredSplit, uBaseHash, 1);

        // split into 2 children
        plTerrainMapElement* ptR = &(ptHeightMap->atElements[t.uRight]);
        plTerrainMapElement* ptL = &(ptHeightMap->atElements[t.uLeft]);

        const uint8_t uChildLevel = t.uLevel + 1;
        const int iMidX = ((int)ptR->iX + (int)ptL->iX) / 2;
        const int iMidZ = ((int)ptR->iZ + (int)ptL->iZ) / 2;
        const uint32_t uMid = pl__vertex_index(ptHeightMap, iMidX, iMidZ);

        plTrianglePrimitive c0 = {
            .uLevel = uChildLevel,
            .uApex  = uMid,
            .uLeft  = t.uApex,
            .uRight = t.uLeft
        };

        plTrianglePrimitive c1 = {
            .uLevel = uChildLevel,
            .uApex  = uMid,
            .uLeft  = t.uRight,
            .uRight = t.uApex
        };

        pl_sb_push(sbtWork, c0);
        pl_sb_push(sbtWork, c1);
    }

    pl_hm_free(&tRequiredSplit);
    pl_sb_free(sbtWork);

    // ------------------------------------------------
    // Unique vertex gathering
    plVec3 tMinBounding = { .x = FLT_MAX,  .y = FLT_MAX,  .z = FLT_MAX };
    plVec3 tMaxBounding = { .x = -FLT_MAX, .y = -FLT_MAX, .z = -FLT_MAX };

    uint32_t uPresentCount = (uint32_t)pl_sb_size(sbtLeaves);

    uint32_t* sbuUniqueVertices = NULL;
    for(uint32_t it = 0; it < uPresentCount; it++)
    {
        plTrianglePrimitive t = sbtLeaves[it];

        plTerrainMapElement* e0 = &(ptHeightMap->atElements[t.uApex]);
        plTerrainMapElement* e1 = &(ptHeightMap->atElements[t.uRight]);
        plTerrainMapElement* e2 = &(ptHeightMap->atElements[t.uLeft]);

        if(e0->uFrameStamp != ptHeightMap->uFrameStamp) { e0->uVertexBufferIndex = pl_sb_size(sbuUniqueVertices); e0->uFrameStamp = ptHeightMap->uFrameStamp; pl_sb_push(sbuUniqueVertices, t.uApex); }
        if(e1->uFrameStamp != ptHeightMap->uFrameStamp) { e1->uVertexBufferIndex = pl_sb_size(sbuUniqueVertices); e1->uFrameStamp = ptHeightMap->uFrameStamp; pl_sb_push(sbuUniqueVertices, t.uRight); }
        if(e2->uFrameStamp != ptHeightMap->uFrameStamp) { e2->uVertexBufferIndex = pl_sb_size(sbuUniqueVertices); e2->uFrameStamp = ptHeightMap->uFrameStamp; pl_sb_push(sbuUniqueVertices, t.uLeft); }
    }

    uint32_t uVertexCount = (uint32_t)pl_sb_size(sbuUniqueVertices);
    plTerrainVertex* atVertexData = PL_ALLOC(uVertexCount * sizeof(plTerrainVertex));

    for(uint32_t i = 0; i < uVertexCount; i++)
    {
        plTerrainMapElement* e = &(ptHeightMap->atElements[sbuUniqueVertices[i]]);
        plTerrainVertex* v = &atVertexData[e->uVertexBufferIndex];

        v->tPosition = pl__get_cartesian(ptHeightMap, e);
        v->tNormal   = pl__get_normal(ptHeightMap, e);

        if(v->tPosition.x < tMinBounding.x) tMinBounding.x = v->tPosition.x;
        if(v->tPosition.x > tMaxBounding.x) tMaxBounding.x = v->tPosition.x;
        if(v->tPosition.y < tMinBounding.y) tMinBounding.y = v->tPosition.y;
        if(v->tPosition.y > tMaxBounding.y) tMaxBounding.y = v->tPosition.y;
        if(v->tPosition.z < tMinBounding.z) tMinBounding.z = v->tPosition.z;
        if(v->tPosition.z > tMaxBounding.z) tMaxBounding.z = v->tPosition.z;
    }
    pl_sb_free(sbuUniqueVertices);

    // Indices from leaves
    uint32_t uIndexCount = 3u * uPresentCount;
    uint32_t* atIndexData = PL_ALLOC(uIndexCount * sizeof(uint32_t));

    uint32_t uCurrentIndex = 0;
    for(uint32_t it = 0; it < uPresentCount; it++)
    {
        plTrianglePrimitive t = sbtLeaves[it];

        plTerrainMapElement* e0 = &(ptHeightMap->atElements[t.uApex]);
        plTerrainMapElement* e1 = &(ptHeightMap->atElements[t.uRight]);
        plTerrainMapElement* e2 = &(ptHeightMap->atElements[t.uLeft]);

        atIndexData[uCurrentIndex + 0] = e0->uVertexBufferIndex;
        atIndexData[uCurrentIndex + 1] = e2->uVertexBufferIndex;
        atIndexData[uCurrentIndex + 2] = e1->uVertexBufferIndex;
        uCurrentIndex += 3;
    }

    pl_sb_free(sbtLeaves);

    // Write out
    fwrite(&tMinBounding, 1, sizeof(plVec3), ptFile);
    fwrite(&tMaxBounding, 1, sizeof(plVec3), ptFile);

    fwrite(&uVertexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atVertexData, 1, sizeof(plTerrainVertex) * uVertexCount, ptFile);

    fwrite(&uIndexCount, 1, sizeof(uint32_t), ptFile);
    fwrite(atIndexData,  1, sizeof(uint32_t) * uIndexCount, ptFile);

    PL_FREE(atVertexData);
    PL_FREE(atIndexData);

    // Recurse into children chunks
    if(iLevel > 0)
    {
        const int childHalf = (1 << (iLogSize - 1));
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX,             iStartIndexY + 0,             iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + childHalf, iStartIndexY + 0,             iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + 0,         iStartIndexY + childHalf,     iLogSize - 1, iLevel - 1);
        pl__terrain_mesh(ptFile, ptHeightMap, iStartIndexX + childHalf, iStartIndexY + childHalf,     iLogSize - 1, iLevel - 1);
    }
}

static plVec3
pl__get_cartesian(plTerrainHeightMap* ptHeightMap, plTerrainMapElement* ptElement)
{
    plVec3 tResult = {
        (float)ptElement->iX * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.x - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f,
        ptElement->fY + ptHeightMap->tCenter.y,
        (float)ptElement->iZ * ptHeightMap->fMetersPerPixel + ptHeightMap->tCenter.z - ptHeightMap->iSize * ptHeightMap->fMetersPerPixel * 0.5f,
    };
    return tResult;
}

static plVec2
pl__get_normal(plTerrainHeightMap* ptHeightMap, plTerrainMapElement* ptElement)
{
    int iL = ptElement->iX - 1;
    int iR = ptElement->iX + 1;
    int jD = ptElement->iZ - 1;
    int jU = ptElement->iZ + 1;

    const int maxHalo = ptHeightMap->iSize - 1;  // last valid slot in a (iSize-1)-length edge
    const int hx = pl_min(pl_max(ptElement->iX, 0), maxHalo);
    const int hz = pl_min(pl_max(ptElement->iZ, 0), maxHalo);

    float hL = 0.0f;
    float hR = 0.0f;
    float hD = 0.0f;
    float hU = 0.0f;

    // west
    if(ptElement->iX == 0 && ptElement->iZ == ptHeightMap->iSize - 1)
        hL = ptHeightMap->atHaloElements[4 * (ptHeightMap->iSize - 1) + 1].fY;
    else if(ptElement->iX == 0)
        hL = ptHeightMap->atHaloElements[3 * (ptHeightMap->iSize - 1) + hz].fY;
    else
        hL = ptHeightMap->atElements[iL + ptHeightMap->iSize*ptElement->iZ].fY;
        
    // north
    if(ptElement->iZ == 0 && ptElement->iX == ptHeightMap->iSize - 1)
        hD = ptHeightMap->atHaloElements[4 * (ptHeightMap->iSize - 1) + 0].fY;
    else if(ptElement->iZ == 0)
        hD = ptHeightMap->atHaloElements[hx].fY;
    else
        hD = ptHeightMap->atElements[ptElement->iX  + ptHeightMap->iSize*jD].fY;

    if(ptElement->iZ == ptHeightMap->iSize - 1)
        hU = ptHeightMap->atHaloElements[2 * (ptHeightMap->iSize - 1) + hx].fY;
    else
        hU = ptHeightMap->atElements[ptElement->iX  + ptHeightMap->iSize*jU].fY;

    // east
    hR = ptElement->iX == ptHeightMap->iSize - 1 ? ptHeightMap->atHaloElements[1 * (ptHeightMap->iSize - 1) + hz].fY : ptHeightMap->atElements[iR + ptHeightMap->iSize*ptElement->iZ].fY;

    float dx = ptHeightMap->fMetersPerPixel;
    float dz = ptHeightMap->fMetersPerPixel;

    // Tangents in world units
    plVec3 tX = { 2.0f * dx, (hR - hL), 0.0f };
    plVec3 tZ = { 0.0f,      (hU - hD), 2.0f * dz };

    // Normal (pick cross order for your winding; this gives +Y up for typical XZ plane)
    plVec3 n = pl_cross_vec3(tZ, tX);
    n = pl_norm_vec3(n);

    return pl__encode(n);

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

    plVec3 tBaseVertex = pl__get_cartesian(ptHeightMap, ptBaseElement);
    plVec3 tLeftVertex = pl__get_cartesian(ptHeightMap, ptLeftElement);
    plVec3 tRightVertex = pl__get_cartesian(ptHeightMap, ptRightElement);

    float fError = tBaseVertex.y - (tLeftVertex.y + tRightVertex.y) / 2.0f;

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
	plTerrainMapElement* ee = pl__get_elem(ptHeightMap, cx + half_size, cz);
	plTerrainMapElement* en = pl__get_elem(ptHeightMap, cx, cz - half_size);
	plTerrainMapElement* ew = pl__get_elem(ptHeightMap, cx - half_size, cz);
	plTerrainMapElement* es = pl__get_elem(ptHeightMap, cx, cz + half_size);
	
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
	plTerrainMapElement* c = pl__get_elem(ptHeightMap, cx, cz);
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
