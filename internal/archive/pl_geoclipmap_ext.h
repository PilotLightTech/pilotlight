/*
   pl_geoclipmap_ext.h
     - geometry clipmap terrain system
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

#ifndef PL_GEOCLIPMAP_EXT_H
#define PL_GEOCLIPMAP_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plGeoClipMapI_version {0, 1, 0}

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
typedef struct _plGeoClipMapInit       plGeoClipMapInit;
typedef struct _plGeoClipMapTilingInfo plGeoClipMapTilingInfo;

// enums/flags
typedef int plGeoClipMapFlags;

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

typedef struct _plGeoClipMapI
{

    // setup/shutdown
    void (*initialize)     (plCommandBuffer*, plGeoClipMapInit);
    void (*finalize)       (plCommandBuffer*);
    void (*cleanup)        (void);
    void (*load_mesh)      (plCommandBuffer*, const char* file, uint32_t levels, uint32_t meshBaseLodExtentTexels);
    void (*tile_height_map)(uint32_t count, plGeoClipMapTilingInfo*);
    
    // per frame
    void (*prepare)(plCamera*, plCommandBuffer*);
    void (*render) (plCamera*, plRenderEncoder*);

    // debugging helpers mostly
    void              (*set_flags)     (plGeoClipMapFlags);
    plGeoClipMapFlags (*get_flags)     (void);
    void              (*reload_shaders)(void);
} plGeoClipMapI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plGeoClipMapInit
{
    plDevice*                 ptDevice;
    plRenderPassLayoutHandle* ptRenderPassLayoutHandle;
    uint32_t                  uSubpassIndex;
    float                     fMetersPerTexel;
    float                     fMaxElevation;
    float                     fMinElevation;
    plVec2                    tMinPosition;
    plVec2                    tMaxPosition;
} plGeoClipMapInit;

typedef struct _plGeoClipMapTilingInfo
{
    char   pcFile[64];
    plVec2 tOrigin; // world coordinates
    float  fMinHeight;
    float  fMaxHeight;
} plGeoClipMapTilingInfo;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plGeoClipMapFlags
{
    PL_GEOCLIPMAP_FLAGS_NONE           = 0,
    PL_GEOCLIPMAP_FLAGS_WIREFRAME      = 1 << 0,
    PL_GEOCLIPMAP_FLAGS_TILE_STREAMING = 1 << 1,
    PL_GEOCLIPMAP_FLAGS_SHOW_ORIGIN    = 1 << 2,
    PL_GEOCLIPMAP_FLAGS_SHOW_BOUNDARY  = 1 << 3,
    PL_GEOCLIPMAP_FLAGS_SHOW_GRID      = 1 << 4,
    PL_GEOCLIPMAP_FLAGS_CACHE_TILES    = 1 << 5,
    PL_GEOCLIPMAP_FLAGS_DEBUG_TOOLS    = 1 << 6,
    PL_GEOCLIPMAP_FLAGS_HIGH_RES       = 1 << 7,
    PL_GEOCLIPMAP_FLAGS_LOW_RES        = 1 << 8,
};

#endif // PL_GEOCLIPMAP_EXT_H