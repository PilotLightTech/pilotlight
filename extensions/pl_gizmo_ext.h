/*
   pl_gizmo_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plCollisionI (v0.2.x)
        * plDataRegistryI (v1.x)
        * plDrawI (v4.x)
        * plEcsI (v3.x)
        * plIOI (v1.x)
        * plTransformI (v1.x)
        * plUiI (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GIZMO_EXT_H
#define PL_GIZMO_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include "pl_ecs_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGizmoI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// enums
typedef int plGizmoMode;
typedef int plGizmoSpace;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCamera           plCamera;           // pl_camera_ext.h
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_gizmo_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_gizmo_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGizmoI
{
    void         (*set_mode) (plGizmoMode);
    void         (*next_mode)(void);
    bool         (*active)   (void);
    void         (*gizmo)    (plComponentLibrary*, plEntity, plDrawList3D*, plCamera*, plVec2 viewOffset, plVec2 viewScale);
    void         (*set_space)(plGizmoSpace); // cancels an active drag
    plGizmoSpace (*get_space)(void);

    // Hold Ctrl to snap drag deltas. Defaults: 1 world unit, 15 degrees, 0.1 scale factor.
    // Nonpositive increments disable snapping for that operation. Escape restores the starting TRS.
    void (*set_snap)(float translation, float rotationDegrees, float scale);
} plGizmoI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plGizmoSpace
{
    PL_GIZMO_SPACE_WORLD = 0,
    PL_GIZMO_SPACE_LOCAL
};

enum _plGizmoMode
{
    PL_GIZMO_MODE_NONE = 0,
    PL_GIZMO_MODE_TRANSLATION,
    PL_GIZMO_MODE_ROTATION,
    PL_GIZMO_MODE_SCALE,

    PL_GIZMO_MODE_COUNT,
};

#ifdef __cplusplus
}
#endif

#endif // PL_GIZMO_EXT_H