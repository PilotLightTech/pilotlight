/*
   pl_gizmo.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GIZMO_H
#define PL_GIZMO_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plGizmoData plGizmoData;

// external
typedef struct _plDrawList3D       plDrawList3D;       // pl_draw_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plCameraComponent  plCameraComponent;  // pl_ecs_ext.h
typedef union  _plEntity           plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plGizmoData* pl_initialize_gizmo_data(void);
void         pl_change_gizmo_mode(plGizmoData*);
void         pl_gizmo(plGizmoData*, plDrawList3D*, plComponentLibrary*, plCameraComponent*, plEntity);

#endif // PL_GIZMO_H