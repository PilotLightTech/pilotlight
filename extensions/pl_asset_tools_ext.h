/*
   pl_asset_tools_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plAnimationI (v0.1.x)
        * plAssetI (v1.x)
        * plCameraEcsI (v0.1.x)
        * plCameraI (v1.x)
        * plConsoleI (v2.x)
        * plDataRegistryI (v1.x)
        * plEcsI (v3.x)
        * plGraphicsI (v2.x)
        * plIOI (v1.x)
        * plIkI (v0.3.x)
        * plMaterialI (v0.5.x)
        * plMemoryI (v1.x)
        * plMeshI (v0.3.x)
        * plPhysicsI (v0.2.x)
        * plRendererI (v0.3.x)
        * plResourceI (v1.x)
        * plScriptI (v0.1.x)
        * plSkeletonI (v0.1.x)
        * plTerrainI (v0.1.x)
        * plTextureI (v0.1.x)
        * plTransformI (v1.x)
        * plUiI (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ASSET_TOOLS_EXT_H
#define PL_ASSET_TOOLS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plAssetToolsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_ecs_ext.inl"
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_asset_tools_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_asset_tools_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plAssetToolsI
{
    void (*initialize) (void);
    void (*cleanup)    (void);
    void (*run)        (void);
} plAssetToolsI;

#ifdef __cplusplus
}
#endif

#endif // PL_ASSET_TOOLS_EXT_H