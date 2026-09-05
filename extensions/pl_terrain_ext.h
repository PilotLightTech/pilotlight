/*
   pl_terrain_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TERRAIN_EXT_H
#define PL_TERRAIN_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plTerrainI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include <stdint.h>
#include "pl_math.h"
#include "pl_asset_ext.inl" // plAssetHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainChunkFile       plTerrainChunkFile;
typedef struct _plTerrainChunk           plTerrainChunk;
typedef struct _plTerrainProcessTileInfo plTerrainProcessTileInfo;
typedef struct _plTerrainAsset           plTerrainAsset;

// external
typedef struct _plFreeListNode plFreeListNode; // pl_freelist_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_terrain_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_terrain_ext(plApiRegistryI*, bool reload);

PL_API void pl_terrain_process         (plTerrainAsset*);
PL_API bool pl_terrain_load_chunk_file (const char* path, plTerrainChunkFile* fileOut, uint32_t fileID);

// assets
PL_API void           pl_terrain_register_asset_type(void);
PL_API plAssetTypeKey pl_terrain_get_asset_type_key(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plTerrainI
{
    void (*process)         (plTerrainAsset*);
    bool (*load_chunk_file) (const char* path, plTerrainChunkFile* fileOut, uint32_t fileID);

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)(void);
} plTerrainI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainProcessTileInfo
{
    float         fMaxBaseError;
    float         fMaxHeight;
    float         fMinHeight;
    int           iTreeDepth;
    plVec3        tCenter;
    plAssetHandle tHeightmap;
} plTerrainProcessTileInfo;

typedef struct _plTerrainChunk
{
    plTerrainChunk* ptParent;
    plTerrainChunk* aptChildren[4];

    // chunk address (its position in the quadtree)
    float fX;
    float fY;
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

typedef struct _plTerrainAsset
{
    float    fMetersPerPixel;
    uint32_t uHorizontalTiles;
    uint32_t uVerticalTiles;
    uint32_t uSize;

    uint32_t                  uTileCount;
    plTerrainProcessTileInfo* atTiles;

} plTerrainAsset;

#ifdef __cplusplus
}
#endif

#endif // PL_TERRAIN_EXT_H