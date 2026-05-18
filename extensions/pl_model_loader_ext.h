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
// [SECTION] struct #0
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs #1
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

#define plModelLoaderI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plModelLoaderData plModelLoaderData;
typedef union _plModelInstanceHandle plModelInstanceHandle;

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h


//-----------------------------------------------------------------------------
// [SECTION] struct #0
//-----------------------------------------------------------------------------

typedef union _plModelInstanceHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plModelInstanceHandle;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_model_loader_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_model_loader_ext(plApiRegistryI*, bool reload);

PL_API    plModelInstanceHandle pl_model_loader_load_stl   (plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform);
PL_API    plModelInstanceHandle pl_model_loader_load_gltf  (plComponentLibrary*, const char* pcPath, const plMat4* ptTransform);
PL_API const plModelLoaderData* pl_model_loader_get_objects(plModelInstanceHandle);
PL_API void                     pl_model_loader_free_data  (plModelInstanceHandle);

// just use with GLTF models for now
PL_API bool pl_model_loader_get_node_by_path     (plModelInstanceHandle, const char* path, plEntity* entityOut);
PL_API bool pl_model_loader_get_animation_by_name(plModelInstanceHandle, const char* name, plEntity* entityOut);
PL_API bool pl_model_loader_get_material_by_name (plModelInstanceHandle, const char* name, plEntity* entityOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plModelLoaderI
{
    plModelInstanceHandle    (*load_stl)   (plComponentLibrary*, const char* pcPath, plVec4 tColor, const plMat4* ptTransform);
    plModelInstanceHandle    (*load_gltf)  (plComponentLibrary*, const char* pcPath, const plMat4* ptTransform);
    const plModelLoaderData* (*get_objects)(plModelInstanceHandle);
    void                     (*free_data)  (plModelInstanceHandle);

    // just use with GLTF models for now
    bool (*get_material_by_name) (plModelInstanceHandle, const char* name, plEntity* entityOut);
    bool (*get_animation_by_name)(plModelInstanceHandle, const char* name, plEntity* entityOut);
    bool (*get_node_by_path)     (plModelInstanceHandle, const char* path, plEntity* entityOut);

} plModelLoaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs #1
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