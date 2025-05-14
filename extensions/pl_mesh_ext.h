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

        * plEcsI (v1.x) (only if using ECS integration)
        * plLogI (v1.x) (only if using ECS integration)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MESH_EXT_H
#define PL_MESH_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plMeshI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_ecs_ext.inl" // plEntity
#include "pl_math.h"     // plVec3, plMat4

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plMeshComponent plMeshComponent;

// enums & flags
typedef int plMeshFormatFlags;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plMeshI
{
    // operations
    void (*allocate_vertex_data)(plMeshComponent*, size_t vertexCount, uint64_t vertexStreamMask, size_t indexCount);
    void (*calculate_normals)   (plMeshComponent*, uint32_t meshCount);
    void (*calculate_tangents)  (plMeshComponent*, uint32_t meshCount);

    //----------------------------ECS INTEGRATION----------------------------------

    // entity helpers
    plEntity (*create_mesh)         (plComponentLibrary*, const char* name, plMeshComponent**);
    plEntity (*create_sphere_mesh)  (plComponentLibrary*, const char* name, float radius, uint32_t latitudeBands, uint32_t longitudeBands, plMeshComponent**);
    plEntity (*create_cube_mesh)    (plComponentLibrary*, const char* name, plMeshComponent**);
    plEntity (*create_plane_mesh)   (plComponentLibrary*, const char* name, plMeshComponent**);

    // system setup/shutdown/etc
    void (*register_ecs_system)(void);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key_mesh)(void);
} plMeshI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMeshComponent
{
    uint64_t  ulVertexStreamMask;
    size_t    szVertexCount;
    size_t    szIndexCount;
    uint8_t*  puRawData;
    plEntity  tMaterial;
    plEntity  tSkinComponent;
    plVec3*   ptVertexPositions;
    plVec3*   ptVertexNormals;
    plVec4*   ptVertexTangents;
    plVec4*   ptVertexColors[2];
    plVec4*   ptVertexWeights[2];
    plVec4*   ptVertexJoints[2];
    plVec2*   ptVertexTextureCoordinates[8];
    uint32_t* puIndices;
    plAABB    tAABB;
} plMeshComponent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plMeshFormatFlags
{
    PL_MESH_FORMAT_FLAG_NONE           = 0,
    PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0,
    PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1,
    PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_2 = 1 << 5,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_3 = 1 << 6,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 7,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 8,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 9,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 10,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 11,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 12
};

#endif // PL_MESH_EXT_H