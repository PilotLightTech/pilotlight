/*
   pl_gizmo_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
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

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGizmoI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// enums
typedef int plGizmoMode;

// external
typedef struct _plTransformComponent plTransformComponent; // pl_ecs_ext.h
typedef struct _plCamera             plCamera;             // pl_ecs_ext.h
typedef struct _plDrawList3D         plDrawList3D;         // pl_draw_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_gizmo_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_gizmo_ext(plApiRegistryI*, bool reload);

PL_API void pl_gizmo_set_mode (plGizmoMode);
PL_API void pl_gizmo_next_mode(void);
PL_API bool pl_gizmo_active   (void);
PL_API void pl_gizmo_gizmo    (plDrawList3D*, plCamera*, plTransformComponent* selectedTransform, plTransformComponent* parentTransform, plVec2 viewOffset, plVec2 viewScale);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGizmoI
{
    void (*set_mode)(plGizmoMode);
    void (*next_mode)(void);
    bool (*active)(void);
    void (*gizmo)(plDrawList3D*, plCamera*, plTransformComponent* selectedTransform, plTransformComponent* parentTransform, plVec2 viewOffset, plVec2 viewScale);
} plGizmoI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

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