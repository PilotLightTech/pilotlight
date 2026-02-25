/*
   pl_terrain_processor_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TERRAIN_PROCESSOR_EXT_H
#define PL_TERRAIN_PROCESSOR_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plTerrainProcessorI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainChunkFile       plTerrainChunkFile;
typedef struct _plTerrainChunk           plTerrainChunk;
typedef struct _plTerrainProcessTileInfo plTerrainProcessTileInfo;
typedef struct _plTerrainProcessInfo     plTerrainProcessInfo;

// external
typedef struct _plFreeListNode plFreeListNode; // pl_freelist_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plTerrainProcessorI
{
    void (*process)         (plTerrainProcessInfo*);
    bool (*load_chunk_file) (const char* path, plTerrainChunkFile* fileOut, uint32_t fileID);
} plTerrainProcessorI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainProcessTileInfo
{
    float       fMaxBaseError;
    float       fMaxHeight;
    float       fMinHeight;
    int         iTreeDepth;
    plVec3      tCenter;
    char        acHeightMapFile[256];
    char        acOutputFile[256];
} plTerrainProcessTileInfo;

typedef struct _plTerrainProcessInfo
{
    float                     fMetersPerPixel;
    uint32_t                  uSize;
    uint32_t                  uTileCount;
    plTerrainProcessTileInfo* atTiles;
    uint32_t                  uHorizontalTiles;
    uint32_t                  uVerticalTiles;
} plTerrainProcessInfo;

typedef struct _plTerrainChunk
{
    plTerrainChunk* ptParent;
    plTerrainChunk* aptChildren[4];

    // chunk address (its position in the quadtree)
    uint16_t uX;
    uint16_t uY;
    uint8_t uLevel;

    // bounds
    plVec3 tMinBound;
    plVec3 tMaxBound;

    // gpu data
    uint32_t        uIndex;
    uint32_t        uIndexCount;
    plFreeListNode* ptVertexHole;
    plFreeListNode* ptIndexHole;
    
    size_t szFileLocation;
    uint32_t uFileID;

    uint64_t       uLastFrameUsed;
    plTerrainChunk* ptNext;
    plTerrainChunk* ptPrev;

    bool bInReplacementList;
    plVec2 tUVOffset;
    plVec2 tUVScale;
} plTerrainChunk;

typedef struct _plTerrainChunkFile
{
    int             iTreeDepth;
    float           fMaxBaseError;
    uint32_t        uChunkCount;
    plTerrainChunk* atChunks;
    char            acFile[128];
} plTerrainChunkFile;

typedef struct _plTerrainVertex
{
    plVec3 tPosition;
    plVec2 tNormal;
    plVec2 tUV;
} plTerrainVertex;

#endif // PL_TERRAIN_PROCESSOR_EXT_H