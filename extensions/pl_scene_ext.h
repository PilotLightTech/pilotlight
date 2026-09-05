/*
   pl_scene_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
// [SECTION] components
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SCENE_EXT_H
#define PL_SCENE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plSceneI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stddef.h>         // size_t
#include <stdbool.h>        // bool
#include "pl_math.h"
#include "pl_asset_ext.inl" // plAssetHandle
#include "pl_ecs_ext.inl"   // plEcsTypeKey

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plScene plScene;

// ecs components
typedef struct _plSceneInstanceComponent plSceneInstanceComponent;

// external
typedef struct _plComponentLibrary plComponentLibrary; // opaque

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_scene_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_scene_ext(plApiRegistryI*, bool reload);

PL_API void           pl_scene_register_asset_type(void);
PL_API plAssetTypeKey pl_scene_get_asset_type_key(void);

// system setup/shutdown/etc
PL_API void         pl_scene_register_ecs_components        (void);
PL_API plEcsTypeKey pl_scene_get_ecs_type_key_scene_instance(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plSceneI
{

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)(void);

    // ecs
    void         (*register_ecs_components)(void);
    plEcsTypeKey (*get_ecs_type_key_scene_instance)(void);
} plSceneI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plScene
{
    plComponentLibrary* ptLibrary;
    plAssetHandle       tEnvironment;
    plAssetHandle       tRendererSettings;
} plScene;

typedef struct _plSceneInstanceComponent
{
    plAssetHandle tScene;
} plSceneInstanceComponent;

#ifdef __cplusplus
}
#endif

#endif // PL_SCENE_EXT_H