/*
   pl_ecs_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] defines
// [SECTION] forward declarations & basic types
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

#define PL_ECS_EXT_VERSION    "0.10.0"
#define PL_ECS_EXT_VERSION_NUM 001000

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

#define PL_MAX_SHADOW_CASCADES 4

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
typedef struct _plLightComponent             plLightComponent;
typedef struct _plScriptComponent            plScriptComponent;

// enums
typedef int plShaderType;
typedef int plComponentType;
typedef int plTextureSlot;
typedef int plMaterialFlags;
typedef int plScriptFlags;
typedef int plBlendMode;
typedef int plCameraType;
typedef int plAnimationMode;
typedef int plAnimationPath;
typedef int plAnimationFlags;
typedef int plMeshFormatFlags;
typedef int plLightFlags;
typedef int plLightType;

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
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plEcsI
{
    // setup/shutdown
    void     (*init_component_library)   (plComponentLibrary*);
    void     (*cleanup_component_library)(plComponentLibrary*);

    // misc
    plEntity (*create_entity)  (plComponentLibrary*); // prefer entity helpers below
    void     (*remove_entity)  (plComponentLibrary*, plEntity);
    bool     (*is_entity_valid)(plComponentLibrary*, plEntity);
    plEntity (*get_entity)     (plComponentLibrary*, const char* pcName);
    void*    (*get_component)  (plComponentLibrary*, plComponentType, plEntity);
    void*    (*add_component)  (plComponentLibrary*, plComponentType, plEntity);
    size_t   (*get_index)      (plComponentManager*, plEntity);
    
    // entity helpers (creates entity and necessary components)
    //   - do NOT store out parameter; use it immediately
    plEntity (*create_tag)                (plComponentLibrary*, const char* pcName);
    plEntity (*create_mesh)               (plComponentLibrary*, const char* pcName, plMeshComponent**);
    plEntity (*create_object)             (plComponentLibrary*, const char* pcName, plObjectComponent**);
    plEntity (*create_transform)          (plComponentLibrary*, const char* pcName, plTransformComponent**);
    plEntity (*create_material)           (plComponentLibrary*, const char* pcName, plMaterialComponent**);
    plEntity (*create_skin)               (plComponentLibrary*, const char* pcName, plSkinComponent**);
    plEntity (*create_animation)          (plComponentLibrary*, const char* pcName, plAnimationComponent**);
    plEntity (*create_animation_data)     (plComponentLibrary*, const char* pcName, plAnimationDataComponent**);
    plEntity (*create_perspective_camera) (plComponentLibrary*, const char* pcName, plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ, plCameraComponent**);
    plEntity (*create_orthographic_camera)(plComponentLibrary*, const char* pcName, plVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ, plCameraComponent**);
    plEntity (*create_directional_light)  (plComponentLibrary*, const char* pcName, plVec3 tDirection, plLightComponent**);
    plEntity (*create_point_light)        (plComponentLibrary*, const char* pcName, plVec3 tPosition, plLightComponent**);

    // scripts
    plEntity (*create_script)(plComponentLibrary*, const char* pcFile, plScriptFlags, plScriptComponent**);
    void     (*attach_script)(plComponentLibrary*, const char* pcFile, plScriptFlags, plEntity tEntity, plScriptComponent**);

    // hierarchy
    void (*attach_component)   (plComponentLibrary*, plEntity tEntity, plEntity tParent);
    void (*deattach_component) (plComponentLibrary*, plEntity);

    // meshes
    void (*calculate_normals) (plMeshComponent*, uint32_t uMeshCount);
    void (*calculate_tangents)(plMeshComponent*, uint32_t uMeshCount);

    // systems
    void (*run_object_update_system)            (plComponentLibrary*);
    void (*run_transform_update_system)         (plComponentLibrary*);
    void (*run_skin_update_system)              (plComponentLibrary*);
    void (*run_hierarchy_update_system)         (plComponentLibrary*);
    void (*run_animation_update_system)         (plComponentLibrary*, float fDeltaTime);
    void (*run_inverse_kinematics_update_system)(plComponentLibrary*);
    void (*run_script_update_system)            (plComponentLibrary*);
} plEcsI;

typedef struct _plCameraI
{
    void (*set_fov)        (plCameraComponent*, float fYFov);
    void (*set_clip_planes)(plCameraComponent*, float fNearZ, float fFarZ);
    void (*set_aspect)     (plCameraComponent*, float fAspect);
    void (*set_pos)        (plCameraComponent*, float fX, float fY, float fZ);
    void (*set_pitch_yaw)  (plCameraComponent*, float fPitch, float fYaw);
    void (*translate)      (plCameraComponent*, float fDx, float fDy, float fDz);
    void (*rotate)         (plCameraComponent*, float fDPitch, float fDYaw);
    void (*look_at)        (plCameraComponent*, plVec3 tEye, plVec3 tTarget);
    void (*update)         (plCameraComponent*);
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
    PL_COMPONENT_TYPE_LIGHT,
    PL_COMPONENT_TYPE_SCRIPT,
    
    PL_COMPONENT_TYPE_COUNT
};

enum _plTextureSlot
{
    PL_TEXTURE_SLOT_BASE_COLOR_MAP,
    PL_TEXTURE_SLOT_NORMAL_MAP,
    PL_TEXTURE_SLOT_EMISSIVE_MAP,
    PL_TEXTURE_SLOT_OCCLUSION_MAP,
    PL_TEXTURE_SLOT_METAL_ROUGHNESS_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_ROUGHNESS_MAP,
    PL_TEXTURE_SLOT_CLEARCOAT_NORMAL_MAP,
    PL_TEXTURE_SLOT_SPECULAR_MAP,
    PL_TEXTURE_SLOT_SPECULAR_COLOR_MAP,
    PL_TEXTURE_SLOT_IRIDESCENCE_MAP,
    PL_TEXTURE_SLOT_IRIDESCENCE_THICKNESS_MAP,

    PL_TEXTURE_SLOT_COUNT
};

enum _plShaderType
{
    PL_SHADER_TYPE_PBR,
    PL_SHADER_TYPE_CUSTOM,
    
    PL_SHADER_TYPE_COUNT
};

enum _plMaterialFlags
{
    PL_MATERIAL_FLAG_NONE         = 0,
    PL_MATERIAL_FLAG_DOUBLE_SIDED = 1 << 0,
    PL_MATERIAL_FLAG_OUTLINE      = 1 << 1,
};

enum _plBlendMode
{
    PL_BLEND_MODE_OPAQUE,
    PL_BLEND_MODE_ALPHA,
    PL_BLEND_MODE_PREMULTIPLIED,
    PL_BLEND_MODE_ADDITIVE,
    PL_BLEND_MODE_MULTIPLY,
    PL_BLEND_MODE_CLIP_MASK,

    PL_BLEND_MODE_COUNT
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

enum _plScriptFlags
{
    PL_SCRIPT_FLAG_NONE       = 0,
    PL_SCRIPT_FLAG_PLAYING    = 1 << 0,
    PL_SCRIPT_FLAG_PLAY_ONCE  = 1 << 1,
    PL_SCRIPT_FLAG_RELOADABLE = 1 << 2
};

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

enum _plLightFlags
{
    PL_LIGHT_FLAG_NONE            = 0,
    PL_LIGHT_FLAG_CAST_SHADOW = 1 << 0,
};

enum _plLightType
{
    PL_LIGHT_TYPE_DIRECTIONAL,
    PL_LIGHT_TYPE_POINT
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_HANDLE_DEFINED
#define PL_RESOURCE_HANDLE_DEFINED
typedef union _plResourceHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t ulData;
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
    plComponentManager tLightComponentManager;
    plComponentManager tScriptComponentManager;

    plComponentManager* _ptManagers[PL_COMPONENT_TYPE_COUNT]; // just for internal convenience
    void*               pInternal;
} plComponentLibrary;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plLightComponent
{
    plLightType  tType;
    plLightFlags tFlags;
    plVec3       tColor;
    float        fIntensity;
    float        fRange;
    plVec3       tPosition;
    plVec3       tDirection;
    float        afCascadeSplits[PL_MAX_SHADOW_CASCADES];
    uint32_t     uCascadeCount;
} plLightComponent;

typedef struct _plObjectComponent
{
    plEntity tMesh;
    plEntity tTransform;
} plObjectComponent;

typedef struct _plHierarchyComponent
{
    plEntity tParent;
} plHierarchyComponent;

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
    plBlendMode     tBlendMode;
    plMaterialFlags tFlags;
    plShaderType    tShaderType;
    plVec4          tBaseColor;
    plVec4          tSpecularColor;
    plVec4          tEmissiveColor;
    plVec4          tSheenColor;
    float           fAlphaCutoff;
    float           fRoughness;
    float           fReflectance;
    float           fMetalness;
    float           fNormalMapStrength;
    float           fOcclusionMapStrength;
    float           fRefraction;
    float           fTransmission;
    float           fAnisotropyStrength;
    float           fAnisotropyRotation; // radians, counter-clockwise
    float           fSheenRoughness;
    float           fSpecularFactor;
    float           fClearcoatFactor;
    float           fClearcoatRoughness;
    float           fThicknessFactor;
    float           fIridescenceFactor;
    float           fIridescenceIor;
    float           fIridescenceThicknessMinimum;
    float           fIridescenceThicknessMaximum;
    plTextureMap    atTextureMaps[PL_TEXTURE_SLOT_COUNT];
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
    plVec2*      sbtVertexTextureCoordinates[8];
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
    float               fBlendAmount;
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

typedef struct _plScriptComponent
{
    plScriptFlags tFlags;
    char          acFile[PL_MAX_NAME_LENGTH];
    const struct _plScriptI* _ptApi;
} plScriptComponent;

#endif // PL_ECS_EXT_H