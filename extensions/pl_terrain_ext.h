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
typedef struct _plTerrainInit plTerrainInit;
typedef struct _plTerrain     plTerrain;

// enums/flags
typedef int plTerrainFlags;

// external
typedef struct _plDevice           plDevice;           // pl_graphics_ext.h
typedef struct _plRenderEncoder    plRenderEncoder;    // pl_graphics_ext.h
typedef struct _plDynamicDataBlock plDynamicDataBlock; // pl_graphics_ext.h
typedef struct _plDrawLayer2D      plDrawLayer2D;      // pl_draw_ext.h
typedef struct _plCamera           plCamera;           // pl_camera_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plTerrainI
{

    // setup/shutdown
    void (*initialize)(plTerrainInit);
    void (*cleanup)   (void);

    // terrain setup/finalization/shutdown
    plTerrain* (*create_terrain) (void);
    void       (*finalize_terrain)(plTerrain*);
    void       (*cleanup_terrain)(plTerrain*);

    // runtime setup/shutdown
    bool (*load_chunk_file)(plTerrain*, const char* path);

    // per frame
    void (*prepare_terrain)(plTerrain*);
    void (*render_terrain)(plTerrain*, plRenderEncoder*, plCamera*, plDynamicDataBlock*, float tau);
    
    // debugging helpers mostly
    void           (*set_flags)     (plTerrain*, plTerrainFlags);
    plTerrainFlags (*get_flags)     (plTerrain*);
    void           (*reload_shaders)(plTerrain*);
    void           (*draw_residency)(plTerrain* ptTerrain, plDrawLayer2D*, plVec2 origin, float radius);

} plTerrainI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plTerrainInit
{
    plDevice* ptDevice;
} plTerrainInit;

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