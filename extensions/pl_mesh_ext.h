/*
   pl_mesh_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public apis
// [SECTION] public api structs
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
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MESH_EXT_H
#define PL_MESH_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plMeshI_version {0, 3, 0}
#define plMeshBuilderI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include "pl.inc"
#include "pl_asset_ext.inl" // plAssetHandle
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plMeshBuilder           plMeshBuilder; // opaque
typedef struct _plMeshBuilderOptions    plMeshBuilderOptions;
typedef struct _plMesh                  plMesh;
typedef struct _plSubmeshAllocationDesc plSubmeshAllocationDesc;
typedef struct _plSubmesh               plSubmesh;

// enums & flags
typedef int plMeshBuilderFlags;

//-----------------------------------------------------------------------------
// [SECTION] public apis
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_mesh_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_mesh_ext(plApiRegistryI*, bool reload);

PL_API void           pl_mesh_register_asset_types(void);
PL_API plAssetTypeKey pl_mesh_get_asset_type_key  (void);

// operations
PL_API void pl_mesh_allocate            (plMesh*, const plSubmeshAllocationDesc*, uint32_t count);
PL_API void pl_mesh_cleanup             (plMesh*);
PL_API void pl_mesh_calculate_normals   (plMesh*);
PL_API void pl_mesh_calculate_tangents  (plMesh*);
PL_API void pl_mesh_calculate_bounds    (plMesh*);

// helpers
PL_API void pl_mesh_create_sphere(float radius, uint32_t latitudeBands, uint32_t longitudeBands, plMesh*);
PL_API void pl_mesh_create_cube  (plMesh*);
PL_API void pl_mesh_create_plane (plMesh*);

//------------------------------mesh builder-----------------------------------

// setup/shutdown
PL_API plMeshBuilder* pl_mesh_builder_create(plMeshBuilderOptions);
PL_API void           pl_mesh_builder_cleanup(plMeshBuilder*);

// adding
PL_API void pl_mesh_builder_add_triangle       (plMeshBuilder*, plVec3, plVec3, plVec3);
PL_API void pl_mesh_builder_add_triangle_double(plMeshBuilder*, plVec3d, plVec3d, plVec3d);

// commit
PL_API void pl_mesh_builder_commit       (plMeshBuilder*, uint32_t* indexBuffer, plVec3* vertexBuffer, uint32_t* indexBufferCountOut, uint32_t* vertexBufferCountOut);
PL_API void pl_mesh_builder_commit_double(plMeshBuilder*, uint32_t* indexBuffer, plVec3d* vertexBuffer, uint32_t* indexBufferCountOut, uint32_t* vertexBufferCountOut);


//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plMeshI
{

    // assets
    void           (*register_asset_types)(void);
    plAssetTypeKey (*get_asset_type_key)  (void);

    // operations
    void (*allocate)            (plMesh*, const plSubmeshAllocationDesc*, uint32_t count);
    void (*cleanup)             (plMesh*);
    void (*calculate_normals)   (plMesh*);
    void (*calculate_tangents)  (plMesh*);
    void (*calculate_bounds)    (plMesh*);

    // helpers
    void (*create_sphere) (float radius, uint32_t latitudeBands, uint32_t longitudeBands, plMesh*);
    void (*create_cube)   (plMesh*);
    void (*create_plane)  (plMesh*);

} plMeshI;

typedef struct _plMeshBuilderI
{
    // setup/shutdown
    plMeshBuilder* (*create)(plMeshBuilderOptions);
    void           (*cleanup)(plMeshBuilder*);

    // adding
    void (*add_triangle)       (plMeshBuilder*, plVec3, plVec3, plVec3);
    void (*add_triangle_double)(plMeshBuilder*, plVec3d, plVec3d, plVec3d);

    // commit
    void (*commit)       (plMeshBuilder*, uint32_t* indexBuffer, plVec3* vertexBuffer, uint32_t* indexBufferCountOut, uint32_t* vertexBufferCountOut);
    void (*commit_double)(plMeshBuilder*, uint32_t* indexBuffer, plVec3d* vertexBuffer, uint32_t* indexBufferCountOut, uint32_t* vertexBufferCountOut);

} plMeshBuilderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMeshBuilderOptions
{
    plMeshBuilderFlags tFlags;
    float              fWeldRadius;
} plMeshBuilderOptions;

typedef struct _plSubmeshAllocationDesc
{
    uint64_t uVertexStreamMask;
    size_t   szVertexCount;
    size_t   szIndexCount;
} plSubmeshAllocationDesc;

typedef struct _plSubmesh
{
    uint64_t      uVertexStreamMask;
    size_t        szVertexCount;
    size_t        szIndexCount;
    plVec3*       ptVertexPositions;
    plVec3*       ptVertexNormals;
    plVec4*       ptVertexTangents;
    plVec4*       ptVertexColors[2];
    plVec4*       ptVertexWeights[2];
    plVec4*       ptVertexJoints[2];
    plVec2*       ptVertexTextureCoordinates[2];
    uint32_t*     puIndices; // optional
    plAssetHandle tMaterial;
    plAABB        tAABB;
} plSubmesh;

typedef struct _plMesh
{
    uint8_t*   puRawData;
    size_t     szRawDataSize;
    plAABB     tAABB;
    uint32_t   uSubmeshCount;
    plSubmesh* atSubmeshes;
} plMesh;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plMeshBuilderFlags
{
    PL_MESH_BUILDER_FLAGS_NONE = 0,
    // PL_MESH_BUILDER_FLAGS_DOUBLE_SIDED = 1 << 0,
};

#ifdef __cplusplus
}
#endif

#endif // PL_MESH_EXT_H