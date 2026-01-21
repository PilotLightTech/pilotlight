/*
   pl_cdlod_ext.h
     - continuous distance-dependent level of detail terrain system
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

#ifndef PL_CHLOD_EXT_H
#define PL_CHLOD_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plCdLodI_version {0, 1, 0}

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
typedef struct _plCdLodHeightMapInfo plCdLodHeightMapInfo;
typedef struct _plCdLodChunk         plCdLodChunk;
typedef struct _plCdLodChunkData     plCdLodChunkData;
typedef struct _plCdLodChunkFile     plCdLodChunkFile;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plCdLodI
{

    // setup/shutdown
    void (*initialize) (void);
    void (*cleanup)    (void);

    // offline preprocessing
    void (*process_heightmap)(plCdLodHeightMapInfo);

    // runtime
    bool (*load_chunk_file)  (const char* path, plCdLodChunkFile* fileOut);
    void (*unload_chunk_file)(plCdLodChunkFile*);
} plCdLodI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCdLodChunk
{
    plCdLodChunk* ptParent;
    plCdLodChunk* aptChildren[4];

    // chunk address (its position in the quadtree)
    uint16_t uX;
    uint16_t uY;
    uint8_t uLevel;

    // bounds
    plVec3 tMinBound;
    plVec3 tMaxBound;

    // gpu data
    plCdLodChunkData* ptData;

    size_t szFileLocation;
} plCdLodChunk;

typedef struct _plCdLodChunkFile
{
    int           iTreeDepth;
    float         fMaxBaseError;
    uint32_t      uChunkCount;
    plCdLodChunk* atChunks;
} plCdLodChunkFile;

typedef struct _plCdLodHeightMapInfo
{
    float       fMaxBaseError;
    float       fMetersPerPixel;
    float       fMaxHeight;
    float       fMinHeight;
    int         iTreeDepth;
    plVec3      tCenter;
    const char* pcHeightMapFile;
    const char* pcOutputFile;

    // ellipsoid specific settings
    bool  b3dErrorCalc; // true for ellipsoid
    bool  bEllipsoid;
    float fRadius;
} plCdLodHeightMapInfo;

#endif // PL_CHLOD_EXT_H