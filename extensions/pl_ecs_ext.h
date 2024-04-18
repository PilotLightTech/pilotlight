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

#define PL_ECS_EXT_VERSION    "0.9.0"
#define PL_ECS_EXT_VERSION_NUM 000900

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
typedef struct _plTextureMap       plTextureMap;
typedef struct _plAnimationChannel plAnimationChannel;
typedef struct _plAnimationSampler plAnimationSampler;

// ecs components
typedef struct _plTagComponent               plTagComponent;
typedef struct _plMeshComponent              plMeshComponent;
typedef struct _plTransformComponent         plTransformComponent;
typedef struct _plObjectComponent            plObjectComponent;
typedef struct _plHierarchyComponent         plHierarchyComponent;
typedef struct _plMaterialComponent          plMaterialComponent;
typedef struct _plSkinComponent              plSkinComponent;
typedef struct _plCameraComponent            plCameraComponent;
typedef struct _plAnimationComponent         plAnimationComponent;
typedef struct _plAnimationDataComponent     plAnimationDataComponent;
typedef struct _plInverseKinematicsComponent plInverseKinematicsComponent;

// enums
typedef int plShaderType;
typedef int plComponentType;
typedef int plTextureSlot;
typedef int plMaterialFlags;
typedef int plMaterialBlendMode;
typedef int plCameraType;
typedef int plAnimationMode;
typedef int plAnimationPath;
typedef int plAnimationFlags;
typedef int plMeshFormatFlags;

typedef union _plEntity
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t ulData;
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
    plEntity (*create_tag)                (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_mesh)               (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_object)             (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_transform)          (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_material)           (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_skin)               (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_animation)          (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_animation_data)     (plComponentLibrary* ptLibrary, const char* pcName);
    plEntity (*create_perspective_camera) (plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);
    plEntity (*create_orthographic_camera)(plComponentLibrary* ptLibrary, const char* pcName, plVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ);

    // hierarchy
    void (*attach_component)   (plComponentLibrary* ptLibrary, plEntity tEntity, plEntity tParent);
    void (*deattach_component) (plComponentLibrary* ptLibrary, plEntity tEntity);

    // meshes
    void (*calculate_normals) (plMeshComponent* atMeshes, uint32_t uComponentCount);
    void (*calculate_tangents)(plMeshComponent* atMeshes, uint32_t uComponentCount);

    // systems
    void (*run_object_update_system)            (plComponentLibrary* ptLibrary);
    void (*run_transform_update_system)         (plComponentLibrary* ptLibrary);
    void (*run_skin_update_system)              (plComponentLibrary* ptLibrary);
    void (*run_hierarchy_update_system)         (plComponentLibrary* ptLibrary);
    void (*run_animation_update_system)         (plComponentLibrary* ptLibrary, float fDeltaTime);
    void (*run_inverse_kinematics_update_system)(plComponentLibrary* ptLibrary);
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
    PL_COMPONENT_TYPE_ANIMATION,
    PL_COMPONENT_TYPE_ANIMATION_DATA,
    PL_COMPONENT_TYPE_INVERSE_KINEMATICS,
    
    PL_COMPONENT_TYPE_COUNT
};

enum _plTextureSlot
{
    PL_TEXTURE_SLOT_BASE_COLOR_MAP,
    PL_TEXTURE_SLOT_NORMAL_MAP,
    PL_TEXTURE_SLOT_COUNT
};

enum _plShaderType
{
    PL_SHADER_TYPE_PBR,
    PL_SHADER_TYPE_UNLIT,
    PL_SHADER_TYPE_CUSTOM,
    
    PL_SHADER_TYPE_COUNT
};

enum _plMaterialFlags
{
    PL_MATERIAL_FLAG_NONE         = 0,
    PL_MATERIAL_FLAG_DOUBLE_SIDED = 1 << 0,
};

enum _plMaterialBlendMode
{
    PL_MATERIAL_BLEND_MODE_OPAQUE,
    PL_MATERIAL_BLEND_MODE_ALPHA,
    PL_MATERIAL_BLEND_MODE_PREMULTIPLIED,
    PL_MATERIAL_BLEND_MODE_ADDITIVE,
    PL_MATERIAL_BLEND_MODE_MULTIPLY,
    PL_MATERIAL_BLEND_MODE_COUNT
};

enum _plCameraType
{
    PL_CAMERA_TYPE_PERSPECTIVE,
    PL_CAMERA_TYPE_ORTHOGRAPHIC
};

enum _plAnimationMode
{
    PL_ANIMATION_MODE_UNKNOWN,
    PL_ANIMATION_MODE_LINEAR,
    PL_ANIMATION_MODE_STEP,
    PL_ANIMATION_MODE_CUBIC_SPLINE
};

enum _plAnimationPath
{
    PL_ANIMATION_PATH_UNKNOWN,
    PL_ANIMATION_PATH_TRANSLATION,
    PL_ANIMATION_PATH_ROTATION,
    PL_ANIMATION_PATH_SCALE,
    PL_ANIMATION_PATH_WEIGHTS
};

enum _plAnimationFlags
{
    PL_ANIMATION_FLAG_NONE    = 0,
    PL_ANIMATION_FLAG_PLAYING = 1 << 0,
    PL_ANIMATION_FLAG_LOOPED  = 1 << 1
};

enum _plMeshFormatFlags
{
    PL_MESH_FORMAT_FLAG_NONE           = 0,
    PL_MESH_FORMAT_FLAG_HAS_POSITION   = 1 << 0,
    PL_MESH_FORMAT_FLAG_HAS_NORMAL     = 1 << 1,
    PL_MESH_FORMAT_FLAG_HAS_TANGENT    = 1 << 2,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 = 1 << 3,
    PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1 = 1 << 4,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_0    = 1 << 5,
    PL_MESH_FORMAT_FLAG_HAS_COLOR_1    = 1 << 6,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_0   = 1 << 7,
    PL_MESH_FORMAT_FLAG_HAS_JOINTS_1   = 1 << 8,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0  = 1 << 9,
    PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1  = 1 << 10
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_HANDLE_DEFINED
#define PL_RESOURCE_HANDLE_DEFINED
typedef struct _plResourceHandle
{
    uint32_t uIndex;
    uint32_t uGeneration;
} plResourceHandle;
#endif // PL_RESOURCE_HANDLE_DEFINED

typedef struct _plTextureMap
{
    char             acName[PL_MAX_NAME_LENGTH];
    plResourceHandle tResource;
    uint32_t         uUVSet;
    uint32_t         uWidth;
    uint32_t         uHeight;
} plTextureMap;

typedef struct _plAnimationSampler
{
    plAnimationMode tMode;
    plEntity        tData;
} plAnimationSampler;

typedef struct _plAnimationChannel
{
    plAnimationPath tPath;
    plEntity        tTarget;
    uint32_t        uSamplerIndex;
} plAnimationChannel;

typedef struct _plComponentManager
{
    plComponentLibrary* ptParentLibrary;
    plComponentType     tComponentType;
    plHashMap           tHashMap; // map entity -> index in sbtEntities/pComponents
    plEntity*           sbtEntities;
    void*               pComponents;
    size_t              szStride;
} plComponentManager;

typedef struct _plComponentLibrary
{
    uint32_t* sbtEntityGenerations;
    uint32_t* sbtEntityFreeIndices;
    plHashMap tTagHashMap;

    // managers
    plComponentManager tTagComponentManager;
    plComponentManager tTransformComponentManager;
    plComponentManager tMeshComponentManager;
    plComponentManager tObjectComponentManager;
    plComponentManager tHierarchyComponentManager;
    plComponentManager tMaterialComponentManager;
    plComponentManager tSkinComponentManager;
    plComponentManager tCameraComponentManager;
    plComponentManager tAnimationComponentManager;
    plComponentManager tAnimationDataComponentManager;
    plComponentManager tInverseKinematicsComponentManager;

    plComponentManager* _ptManagers[PL_COMPONENT_TYPE_COUNT]; // just for internal convenience
    void*               pInternal;
} plComponentLibrary;

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
    plMat4 tWorld;
} plTransformComponent;

typedef struct _plMaterialComponent
{
    plMaterialBlendMode tBlendMode;
    plMaterialFlags     tFlags;
    plShaderType        tShaderType;
    plVec4              tBaseColor;
    float               fAlphaCutoff;
    plTextureMap        atTextureMaps[PL_TEXTURE_SLOT_COUNT];
} plMaterialComponent;

typedef struct _plMeshComponent
{
    uint64_t     ulVertexStreamMask;
    plEntity     tMaterial;
    plEntity     tSkinComponent;
    plVec3*      sbtVertexPositions;
    plVec3*      sbtVertexNormals;
    plVec4*      sbtVertexTangents;
    plVec4*      sbtVertexColors[2];
    plVec4*      sbtVertexWeights[2];
    plVec4*      sbtVertexJoints[2];
    plVec2*      sbtVertexTextureCoordinates[2];
    uint32_t*    sbuIndices;
    plAABB       tAABB;
    plAABB       tAABBFinal;
} plMeshComponent;

typedef struct _plSkinComponent
{
    plEntity  tMeshNode;
    plMat4*   sbtInverseBindMatrices;
    plEntity* sbtJoints;
    plMat4*   sbtTextureData;
} plSkinComponent;

typedef struct _plCameraComponent
{
    plCameraType tType;
    plVec3       tPos;
    float        fNearZ;
    float        fFarZ;
    float        fFieldOfView;
    float        fAspectRatio;  // width/height
    float        fWidth;        // for orthographic
    float        fHeight;       // for orthographic
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

typedef struct _plAnimationDataComponent
{
    float* sbfKeyFrameTimes;
    float* sbfKeyFrameData;
} plAnimationDataComponent;

typedef struct _plAnimationComponent
{
    plAnimationFlags    tFlags;
    float               fStart;
    float               fEnd;
    float               fTimer;
    float               fSpeed;
    plAnimationChannel* sbtChannels;
    plAnimationSampler* sbtSamplers;
} plAnimationComponent;

typedef struct _plInverseKinematicsComponent
{
    bool     bEnabled;
    plEntity tTarget;
    uint32_t uChainLength;
    uint32_t uIterationCount;
} plInverseKinematicsComponent;

#endif // PL_ECS_EXT_H