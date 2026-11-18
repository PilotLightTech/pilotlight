/*
   pl_scene_tools_ext.h
     - scene workflows and diagnostics (ui extension based)
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

#ifndef PL_SCENE_TOOLS_EXT_H
#define PL_SCENE_TOOLS_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plSceneToolsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include "pl_ecs_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plScene            plScene;            // pl_renderer_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

PL_API void pl_load_scene_tools_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_scene_tools_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plSceneToolsI
{
    // One active library. Call during the UI frame, including when closed.
    // Scene may be NULL for navigation-only hosts. Settings remain in Assets.
    void (*run)(plComponentLibrary*, plScene*, plEntity selected, bool* open);

    // After scene systems, once per view. Borrows live terrain/grids/routes; owns none.
    void (*draw)(const plComponentLibrary*, plDrawList3D*, plDrawList3D* overlay);

    // Reset borrowed scene references and tool state before scene unload.
    void (*cleanup)(void);
} plSceneToolsI;

#ifdef __cplusplus
}
#endif
#endif // PL_SCENE_TOOLS_EXT_H
