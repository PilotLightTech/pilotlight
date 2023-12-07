/*
   pl_ecs_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] public api structs
// [SECTION] enums
// [SECTION] structs
// [SECTION] components
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_ECS_EXT_H
#define PL_ECS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_ECS "PL_API_ECS"
typedef struct _plEcsI plEcsI;

#define PL_API_CAMERA "PL_API_CAMERA"
typedef struct _plCameraI plCameraI;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h"
#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plComponentLibrary plComponentLibrary;
typedef struct _plComponentManager plComponentManager;

// ecs components
typedef struct _plTagComponent       plTagComponent;
typedef struct _plMeshComponent      plMeshComponent;
typedef struct _plTransformComponent plTransformComponent;
typedef struct _plObjectComponent    plObjectComponent;
typedef struct _plHierarchyComponent plHierarchyComponent;
typedef struct _plMaterialComponent  plMaterialComponent;
typedef struct _plSkinComponent      plSkinComponent;
typedef struct _plCameraComponent    plCameraComponent;

// enums
typedef int plComponentType;

typedef struct _plEntity
{
    uint32_t uIndex;
    uint32_t uGeneration;
} plEntity;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

const plEcsI*    pl_load_ecs_api   (void);
const plCameraI* pl_load_camera_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plEcsI
{
    // setup/shutdown
    void     (*init_component_library)   (plComponentLibrary* ptLibrary);
    void     (*cleanup_component_library)(plComponentLibrary* ptLibrary);

    // misc
    plEntity (*create_entity)  (plComponentLibrary* ptLibrary); // prefer entity helpers below
    void     (*remove_entity)  (plComponentLibrary* ptLibrary, plEntity tEntity);
    bool     (*is_entity_valid)(plComponentLibrary* ptLibrary, plEntity tEntity);
    plEntity (*get_entity)     (plComponentLibrary* ptLibrary, const char* pcName);
    void*    (*get_component)  (plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity);
    void*    (*add_component)  (plComponentLibrary* ptLibrary, plComponentType tType, plEntity tEntity);
    size_t   (*get_index)      (plComponentManager* ptManager, plEntity tEntity);
    
    // entity helpers (creates entity and necessary components)
    plEntity (*create_tag)      (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_mesh)     (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_object)   (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_transform)(plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_material) (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_skin)     (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_camera)   (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

    // hierarchy
    void (*attach_component)   (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
    void (*deattach_component) (plComponentLibrary* ptLibrary, plEntity tEntity);

    // meshes
    void (*calculate_normals) (plMeshComponent* atMeshes, uint32_t uComponentCount);
    void (*calculate_tangents)(plMeshComponent* atMeshes, uint32_t uComponentCount);

    // systems
    void (*run_skin_update_system)     (plComponentLibrary* ptLibrary);
    void (*run_hierarchy_update_system)(plComponentLibrary* ptLibrary);
} plEcsI;

typedef struct _plCameraI
{
    void (*set_fov)        (plCameraComponent* ptCamera, float fYFov);
    void (*set_clip_planes)(plCameraComponent* ptCamera, float fNearZ, float fFarZ);
    void (*set_aspect)     (plCameraComponent* ptCamera, float fAspect);
    void (*set_pos)        (plCameraComponent* ptCamera, float fX, float fY, float fZ);
    void (*set_pitch_yaw)  (plCameraComponent* ptCamera, float fPitch, float fYaw);
    void (*translate)      (plCameraComponent* ptCamera, float fDx, float fDy, float fDz);
    void (*rotate)         (plCameraComponent* ptCamera, float fDPitch, float fDYaw);
    void (*update)         (plCameraComponent* ptCamera);
} plCameraI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plComponentType
{
    PL_COMPONENT_TYPE_TAG,
    PL_COMPONENT_TYPE_TRANSFORM,
    PL_COMPONENT_TYPE_MESH,
    PL_COMPONENT_TYPE_OBJECT,
    PL_COMPONENT_TYPE_HIERARCHY,
    PL_COMPONENT_TYPE_MATERIAL,
    PL_COMPONENT_TYPE_SKIN,
    PL_COMPONENT_TYPE_CAMERA,
    
    PL_COMPONENT_TYPE_COUNT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plComponentManager
{
    plComponentLibrary* ptParentLibrary;
    plComponentType     tComponentType;
    plHashMap           tHashMap;
    plEntity*           sbtEntities;
    void*               pComponents;
    size_t              szStride;
} plComponentManager;

typedef struct _plComponentLibrary
{
    uint32_t* sbtEntityGenerations;
    uint32_t* sbtEntityFreeIndices;
    plHashMap tTagHashMap;

    plComponentManager tTagComponentManager;
    plComponentManager tTransformComponentManager;
    plComponentManager tMeshComponentManager;
    plComponentManager tObjectComponentManager;
    plComponentManager tHierarchyComponentManager;
    plComponentManager tMaterialComponentManager;
    plComponentManager tSkinComponentManager;
    plComponentManager tCameraComponentManager;

    plComponentManager* _ptManagers[PL_COMPONENT_TYPE_COUNT]; // just for internal convenience
} plComponentLibrary;

typedef struct _plMesh
{
    uint32_t uDataOffset;
    uint32_t uVertexOffset;
    uint32_t uVertexCount;
    uint32_t uIndexOffset;
    uint32_t uIndexCount;
} plMesh;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plObjectComponent
{
    plEntity tMesh;
    plEntity tTransform;
} plObjectComponent;

typedef struct _plHierarchyComponent
{
    plEntity tParent;
} plHierarchyComponent;

typedef struct _plLightComponent
{
    plVec3 tPosition;
    plVec3 tColor;
} plLightComponent;

typedef struct _plTagComponent
{
    char acName[PL_MAX_NAME_LENGTH];
} plTagComponent;

typedef struct _plTransformComponent
{
    plVec3 tScale;
    plVec4 tRotation;
    plVec3 tTranslation;
    plMat4 tFinalTransform;
    plMat4 tWorld;
} plTransformComponent;

typedef struct _plMaterialComponent
{
    // properties
    plVec4 tAlbedo;
    float  fAlphaCutoff;
    bool   bDoubleSided;

    const char* pcAlbedoMap;
} plMaterialComponent;

typedef struct _plMeshComponent
{
    plMesh       tMesh;
    uint64_t     ulVertexStreamMask;
    plEntity     tMaterial;
    plEntity     tSkinComponent;
    plVec3*      sbtVertexPositions;
    plVec3*      sbtVertexNormals;
    plVec4*      sbtVertexTangents;
    plVec4*      sbtVertexColors0;
    plVec4*      sbtVertexColors1;
    plVec4*      sbtVertexWeights0;
    plVec4*      sbtVertexWeights1;
    plVec4*      sbtVertexJoints0;
    plVec4*      sbtVertexJoints1;
    plVec2*      sbtVertexTextureCoordinates0;
    plVec2*      sbtVertexTextureCoordinates1;
    uint32_t*    sbuIndices;
} plMeshComponent;

typedef struct _plSkinComponent
{
    plEntity  tSkeleton;
    plMat4*   sbtInverseBindMatrices;
    plEntity* sbtJoints;
    plMat4*   sbtTextureData;
} plSkinComponent;

typedef struct _plCameraComponent
{
    plVec3       tPos;
    float        fNearZ;
    float        fFarZ;
    float        fFieldOfView;
    float        fAspectRatio;  // width/height
    plMat4       tViewMat;      // cached
    plMat4       tProjMat;      // cached
    plMat4       tTransformMat; // cached

    // rotations
    float        fPitch; // rotation about right vector
    float        fYaw;   // rotation about up vector
    float        fRoll;  // rotation about forward vector

    // direction vectors
    plVec3       _tUpVec;
    plVec3       _tForwardVec;
    plVec3       _tRightVec;
} plCameraComponent;

#endif // PL_ECS_EXT_H