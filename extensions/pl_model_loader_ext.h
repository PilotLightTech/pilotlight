/*
   pl_model_loader_ext.h
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
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plResourceI (v1.x)
        * plEcsI      (v1.x)
        * plFileI     (v1.x)
        * plVfsI      (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MODEL_LOADER_EXT_H
#define PL_MODEL_LOADER_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plModelLoaderI_version {0, 2, 1}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plModelLoaderData plModelLoaderData;

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_model_loader_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_model_loader_ext(plApiRegistryI*, bool reload);

PL_API bool pl_model_loader_load_stl (plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform, plModelLoaderData* ptDataOut);
PL_API bool pl_model_loader_load_gltf(plComponentLibrary*, const char* pcPath, const plMat4* ptTransform, plModelLoaderData* ptDataOut);
PL_API void pl_model_loader_free_data(plModelLoaderData*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plModelLoaderI
{
    bool (*load_stl) (plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform, plModelLoaderData* ptDataOut);
    bool (*load_gltf)(plComponentLibrary*, const char* pcPath, const plMat4* ptTransform, plModelLoaderData* ptDataOut);
    void (*free_data)(plModelLoaderData*);
} plModelLoaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plModelLoaderData
{
    uint32_t  uObjectCount;
    plEntity* atObjects;
} plModelLoaderData;

#ifdef __cplusplus
}
#endif

#endif // PL_MODEL_LOADER_EXT_H