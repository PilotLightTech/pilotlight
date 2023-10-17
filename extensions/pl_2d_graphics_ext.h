/*
   pl_2d_graphics_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_2D_GRAPHICS_EXT_H
#define PL_2D_GRAPHICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_2D_GRAPHICS "PL_API_2D_GRAPHICS"
typedef struct _pl2DGraphicsI pl2DGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// context
typedef struct _pl2DContext   pl2DContext;
typedef struct _plDynamicTile plDynamicTile;

// tile map
typedef struct _plTile    plTile;
typedef struct _plTileMap plTileMap;

// external
typedef struct _plGraphics plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const pl2DGraphicsI* pl_load_2d_graphics_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _pl2DGraphicsI
{
    void (*initialize)(plGraphics* ptGraphics);
    void (*cleanup)(plGraphics* ptGraphics);

    // layers
    uint32_t (*create_layer)  (const char* pcName, uint32_t uTileMap);
    void     (*submit_layer)  (uint32_t uLayerHandle);
    void     (*draw_layers)   (plGraphics* ptGraphics);
    void     (*compile_layers)(plGraphics* ptGraphics);

    uint32_t (*add_duplicated_tiles)(uint32_t uLayerHandle, uint32_t uCount, plTile tTile, const plVec2* ptPositions, plVec2 tSize);
    uint32_t (*add_tile)            (uint32_t uLayerHandle, plTile tTile, plVec2 tPos, plVec2 tSize);
    uint32_t (*add_tiles)           (uint32_t uLayerHandle, uint32_t uCount, plTile* ptTiles, plVec2* ptPositions, plVec2* ptSizes);
    void     (*update_tile)         (uint32_t uLayerHandle, uint32_t uTileHandle, plVec2 tPos);

    // tile maps
    uint32_t (*create_tile_map)(const char* pcName, const char* pcPath, plGraphics* ptGraphics, uint32_t uTileSize, uint32_t uTileSpacing);
    void     (*get_tile)       (uint32_t uMap, uint32_t uX, uint32_t uY, plTile* ptTileOut);
    void*    (*get_ui_texture)(plGraphics* ptGraphics, uint32_t uTileMapHandle);
} pl2DGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTile
{
    plVec2   tTopLeft;
    plVec2   tTopRight;
    plVec2   tBottomLeft;
    plVec2   tBottomRight;
    uint32_t uParentMap;
} plTile;

typedef struct _plTileMap
{
    // inputs
    const char*   pcName;
    const char*   pcImagePath;
    uint32_t      uTileSize;
    uint32_t      uTileSpacing;

    // outputs
    plTile*       sbtTiles;
    uint32_t      uHorizontalTiles;
    uint32_t      uVerticalTiles;

    uint32_t uResourceHandle;
} plTileMap;

#endif // PL_2D_GRAPHICS_EXT_H