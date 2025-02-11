
/*
   pl_ecs_tools.h
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

#ifndef PL_ECS_TOOLS_H
#define PL_ECS_TOOLS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external
typedef struct _plApiRegistryI     plApiRegistryI;     // pl.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

bool pl_show_ecs_window(plEntity* ptSelectedEntity, uint32_t uSceneHandle, bool*);

#endif // PL_ECS_TOOLS_H