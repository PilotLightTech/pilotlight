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
// [SECTION] structs
// [SECTION] enums
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
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plTerrainExtInit        plTerrainExtInit;
typedef struct _plTerrainInit           plTerrainInit;
typedef struct _plTerrainRuntimeOptions plTerrainRuntimeOptions;
typedef struct _plTerrain               plTerrain;
typedef struct _plTerrainTexture        plTerrainTexture;

// enums/flags
typedef int plTerrainFlags;

// external
typedef struct _plTerrainProcessInfo    plTerrainProcessInfo;     // pl_terrain_processor.h
typedef struct _plDevice                plDevice;                 // pl_graphics_ext.h
typedef struct _plRenderEncoder         plRenderEncoder;          // pl_graphics_ext.h
typedef struct _plDynamicDataBlock      plDynamicDataBlock;       // pl_graphics_ext.h
typedef struct _plCamera                plCamera;                 // pl_camera_ext.h
typedef struct _plCommandBuffer         plCommandBuffer;          // pl_graphics_ext.h
typedef union  plRenderPassLayoutHandle plRenderPassLayoutHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plTerrainI
{

    // setup/shutdown
    void (*initialize)(plTerrainExtInit);
    void (*cleanup)   (void);

    // terrain setup/finalization/shutdown
    plTerrain* (*create_terrain) (plCommandBuffer*, plTerrainInit, plTerrainProcessInfo*);
    void      (*cleanup_terrain)(plTerrain*);

    void (*set_texture)(plTerrain*, plTerrainTexture*);

    // per frame
    void (*prepare)(plTerrain*, plCommandBuffer*);

    // views (share terrain data, separate render targets)
    void              (*render)     (plRenderEncoder*, plTerrain*, plCamera*, plDynamicDataBlock*);

    // debugging helpers mostly
    void                   (*set_runtime_options)(plTerrain*, plTerrainRuntimeOptions);
    plTerrainRuntimeOptions (*get_runtime_options)(plTerrain*);
    void                   (*reload_shaders)     (plTerrain*);
     void                  (*set_shaders)        (plTerrain*, const char* pcVertexShader, const char* pcFragmentShader);

} plTerrainI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainExtInit
{
    plDevice* ptDevice;
    uint32_t uStagingBufferSize; // default: 268435456 bytes
} plTerrainExtInit;

typedef struct _plTerrainTexture
{
    const char* pcPath;
    float       fMetersPerPixel;
    plVec2      tCenter;
} plTerrainTexture;

typedef struct _plTerrainInit
{
    plRenderPassLayoutHandle* ptRenderPassLayoutHandle;

    // memory allocations
    uint32_t uVertexBufferSize;  // default: 268435456 bytes
    uint32_t uIndexBufferSize;   // default: 268435456 bytes

    // shaders
    const char* pcVertexShader;   // default: "pl_terrain.vert"
    const char* pcFragmentShader; // default: "pl_terrain.frag"
} plTerrainInit;

typedef struct _plTerrainRuntimeOptions
{
    plTerrainFlags tFlags;
    float         fTau;
    plVec3        tLightDirection;
} plTerrainRuntimeOptions;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plTerrainFlags
{
    PL_TERRAIN_FLAGS_NONE        = 0,
    PL_TERRAIN_FLAGS_WIREFRAME   = 1 << 0,
    PL_TERRAIN_FLAGS_SHOW_LEVELS = 1 << 1
};

#endif // PL_TERRAIN_EXT_H