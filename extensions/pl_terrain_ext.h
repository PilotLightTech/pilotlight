/*
   pl_terrain_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plIOI            (v1.1)
        * plGraphicsI      (v1.5)
        * plFileI          (v1.1)
        * plAtomicsI       (v1.x)
        * plVfsI           (v1.x)
        * plShaderI        (v1.2)
        * plImageI         (v1.x)
        * plUiI            (v1.x)
        * plGPUAllocatorsI (v1.x)
        * plDrawI          (v2.0)
        * plMeshI          (v0.1)
        * plMeshBuilderI   (v0.1)
        * plCameraI        (v0.2)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_TERRAIN_EXT_H
#define PL_TERRAIN_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plTerrainI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainInit       plTerrainInit;
typedef struct _plTerrainTilingInfo plTerrainTilingInfo;

// enums/flags
typedef int plTerrainFlags;

// external
typedef struct _plDevice                plDevice;                 // pl_graphics_ext.h
typedef struct _plCommandBuffer         plCommandBuffer;          // pl_graphics_ext.h
typedef union  plBindGroupHandle        plBindGroupHandle;        // pl_graphics_ext.h
typedef union  plRenderPassLayoutHandle plRenderPassLayoutHandle; // pl_graphics_ext.h
typedef struct _plRenderEncoder         plRenderEncoder;          // pl_graphics_ext.h
typedef struct _plCamera                plCamera;                 // pl_camera_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plTerrainI
{

    // setup/shutdown
    void (*initialize)     (plCommandBuffer*, plTerrainInit);
    void (*finalize)       (plCommandBuffer*);
    void (*cleanup)        (void);
    void (*load_mesh)      (plCommandBuffer*, const char* file, uint32_t levels, uint32_t meshBaseLodExtentTexels);
    void (*tile_height_map)(uint32_t count, plTerrainTilingInfo*);
    
    // per frame
    void (*prepare)(plCamera*, plCommandBuffer*);
    void (*render) (plCamera*, plRenderEncoder*);

    // debugging helpers mostly
    void           (*set_flags)     (plTerrainFlags);
    plTerrainFlags (*get_flags)     (void);
    void           (*reload_shaders)(void);
} plTerrainI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainInit
{
    plDevice*                 ptDevice;
    plRenderPassLayoutHandle* ptRenderPassLayoutHandle;
    uint32_t                  uSubpassIndex;
    float                     fMetersPerTexel;
    float                     fMaxElevation;
    float                     fMinElevation;
    plVec2                    tMinPosition;
    plVec2                    tMaxPosition;
} plTerrainInit;

typedef struct _plTerrainTilingInfo
{
    char   pcFile[64];
    plVec2 tOrigin; // world coordinates
    float  fMinHeight;
    float  fMaxHeight;
} plTerrainTilingInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTerrainFlags
{
    PL_TERRAIN_FLAGS_NONE           = 0,
    PL_TERRAIN_FLAGS_WIREFRAME      = 1 << 0,
    PL_TERRAIN_FLAGS_TILE_STREAMING = 1 << 1,
    PL_TERRAIN_FLAGS_SHOW_ORIGIN    = 1 << 2,
    PL_TERRAIN_FLAGS_SHOW_BOUNDARY  = 1 << 3,
    PL_TERRAIN_FLAGS_SHOW_GRID      = 1 << 4,
    PL_TERRAIN_FLAGS_CACHE_TILES    = 1 << 5,
    PL_TERRAIN_FLAGS_DEBUG_TOOLS    = 1 << 6,
    PL_TERRAIN_FLAGS_HIGH_RES       = 1 << 7,
    PL_TERRAIN_FLAGS_LOW_RES        = 1 << 8,
};

#endif // PL_TERRAIN_EXT_H