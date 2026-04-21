/*
   pl_ecs_tools_ext.h
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
#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef struct _plScene            plScene;            // pl_renderer_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_ecs_tools_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_ecs_tools_ext(plApiRegistryI*, bool reload);

PL_API void pl_ecs_tools_initialize (void);
PL_API void pl_ecs_tools_cleanup    (void);
PL_API bool pl_ecs_tools_show_window(plComponentLibrary*, plEntity* selectedEntity, plScene*, bool*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plEcsToolsI
{
    void (*initialize)     (void);
    void (*cleanup)        (void);
    bool (*show_window)(plComponentLibrary*, plEntity* selectedEntity, plScene*, bool*);
} plEcsToolsI;

#ifdef __cplusplus
}
#endif

#endif // PL_ECS_TOOLS_EXT_H