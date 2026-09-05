/*
   pl_gltf_ext.h
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
// [SECTION] enums
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

#ifndef PL_GLTF_EXT_H
#define PL_GLTF_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_asset_ext.inl"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGltfI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plGltfImportOptions plGltfImportOptions;

// enums/flags
typedef int plGltfImportFlags;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_gltf_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_gltf_ext(plApiRegistryI*, bool reload);

// import
PL_API bool pl_gltf_import(const char* path, const plGltfImportOptions*);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGltfI
{
    bool (*import)(const char* path, const plGltfImportOptions*);
} plGltfI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plGltfImportOptions
{
    plGltfImportFlags eFlags;
} plGltfImportOptions;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plGltfImportFlags
{
    PL_GLTF_IMPORT_FLAGS_NONE                  = 0,
    PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES         = 1 << 0,
    PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS      = 1 << 1,
    PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES       = 1 << 2,
    PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS     = 1 << 3,
    PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS      = 1 << 4,
    PL_GLTF_IMPORT_FLAGS_IMPORT_SCENES         = 1 << 5,
    PL_GLTF_IMPORT_FLAGS_IMPORT_SKINS          = 1 << 6,

    PL_GLTF_IMPORT_FLAGS_IMPORT_ALL_ASSETS     = PL_GLTF_IMPORT_FLAGS_IMPORT_MESHES | PL_GLTF_IMPORT_FLAGS_IMPORT_MATERIALS | PL_GLTF_IMPORT_FLAGS_IMPORT_TEXTURES | PL_GLTF_IMPORT_FLAGS_IMPORT_ANIMATIONS | PL_GLTF_IMPORT_FLAGS_IMPORT_SKELETONS | PL_GLTF_IMPORT_FLAGS_IMPORT_SCENES | PL_GLTF_IMPORT_FLAGS_IMPORT_SKINS,

    // misc
    // PL_GLTF_IMPORT_FLAGS_SPLIT_MESH_PRIMITIVES = 1 << 10,
    // PL_GLTF_IMPORT_FLAGS_GENERATE_TANGENTS     = 1 << 11,
    // PL_GLTF_IMPORT_FLAGS_GENERATE_NORMALS      = 1 << 12,
};

#ifdef __cplusplus
}
#endif

#endif // PL_GLTF_EXT_H