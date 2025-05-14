/*
   pl_ecs_tools_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ECS_TOOLS_EXT_H
#define PL_ECS_TOOLS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plEcsToolsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plEcsToolsI
{
    void (*initialize)     (void);
    void (*cleanup)        (void);
    bool (*show_ecs_window)(plComponentLibrary*, plEntity* ptSelectedEntity, plScene*, bool*);
} plEcsToolsI;

#endif // PL_ECS_TOOLS_EXT_H