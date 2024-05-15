/*
   pl_model_loader_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MODEL_LOADER_EXT_H
#define PL_MODEL_LOADER_EXT_H

#define PL_MODEL_LOADER_EXT_VERSION     "0.9.0"
#define PL_MODEL_LOADER_EXT_VERSION_NUM 009000

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_MODEL_LOADER "PL_API_MODEL_LOADER"
typedef struct _plModelLoaderI plModelLoaderI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plModelLoaderData plModelLoaderData;

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plModelLoaderI
{
    bool (*load_stl) (plComponentLibrary* ptLibrary, const char* pcPath, plVec4 tColor, const plMat4* ptTransform, plModelLoaderData* ptDataOut);
    bool (*load_gltf)(plComponentLibrary* ptLibrary, const char* pcPath, const plMat4* ptTransform, plModelLoaderData* ptDataOut);

    void (*free_data)(plModelLoaderData*);
} plModelLoaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plModelLoaderData
{
    uint32_t  uOpaqueCount;
    plEntity* atOpaqueObjects;
    uint32_t  uTransparentCount;
    plEntity* atTransparentObjects;
} plModelLoaderData;

#endif // PL_MODEL_LOADER_EXT_H