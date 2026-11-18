/*
   pl_ecs_tools_ext.h
     - entity inspection and selection shared with scene viewports
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ECS_TOOLS_EXT_H
#define PL_ECS_TOOLS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plEcsToolsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include "pl_ecs_ext.inl"
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCamera           plCamera;           // pl_camera_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

PL_API void pl_load_ecs_tools_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ecs_tools_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plEcsToolsI
{
    void (*initialize)(void);
    void (*cleanup)   (void); // before unloading libraries
    void (*open)      (plAssetHandle library); // opens the entity/component inspector
    void (*run)       (void); // once per UI frame, after the asset browser

    // Selection belongs to a library asset, not a renderer view. Invalid entity
    // clears selection. Viewport picks and the inspector use this same state.
    void     (*select_entity)      (plAssetHandle library, plEntity);
    plEntity (*get_selected_entity)(plAssetHandle library);
    plEntity (*get_transform_entity)(const plComponentLibrary*, plEntity);

    // Optional viewport helpers. Markers cover transform entities without a
    // renderable mesh. Overlay picking is intentionally not depth-tested.
    // viewOffset/viewSize/mouse are in UI screen pixels; radius is in pixels.
    void (*draw_transforms)(const plComponentLibrary*, plEntity selected, const plCamera*, plVec2 viewSize, plDrawList3D* overlay);
    bool (*pick_transform) (const plComponentLibrary*, const plCamera*, plVec2 viewOffset, plVec2 viewSize, plVec2 mouse, float radius, plEntity* entityOut);
} plEcsToolsI;

#ifdef __cplusplus
}
#endif
#endif // PL_ECS_TOOLS_EXT_H
