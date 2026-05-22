/*
   pl_camera_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
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

        * plEcsI     (v1.x) (only if using ECS integration)
        * plProfileI (v1.x) (only if using ECS integration)
        * plLogI     (v1.x) (only if using ECS integration)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CAMERA_EXT_H
#define PL_CAMERA_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plCameraI_version    {1, 0, 1}
#define plCameraEcsI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h" // plVec3, plMat4, plQuat

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plCamera                 plCamera;
typedef struct _plCameraPerspectiveDesc  plCameraPerspectiveDesc;
typedef struct _plCameraOrthographicDesc plCameraOrthographicDesc;

// enums & flags
typedef int plCameraProjectionType;
typedef int plCameraDepthMode;
typedef int plCameraDirtyFlags;

// external
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef uint32_t                   plEcsTypeKey;       // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_camera_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_camera_ext(plApiRegistryI*, bool reload);

// lifecycle
PL_API void pl_camera_init(plCamera*);

// projection
PL_API void pl_camera_set_perspective (plCamera*, const plCameraPerspectiveDesc*);
PL_API void pl_camera_set_orthographic(plCamera*, const plCameraOrthographicDesc*);
PL_API void pl_camera_set_viewport    (plCamera*, float width, float height);
PL_API void pl_camera_set_y_fov       (plCamera*, float fov);
PL_API void pl_camera_set_clip_planes (plCamera*, float nearZ, float narZ);
PL_API void pl_camera_set_depth_mode  (plCamera*, plCameraDepthMode);

// pose
PL_API void pl_camera_set_position (plCamera*, plVec3d);
PL_API void pl_camera_set_rotation (plCamera*, plQuat);
PL_API void pl_camera_set_transform(plCamera*, plVec3d position, plQuat rotation);

// movement
PL_API void pl_camera_translate      (plCamera*, plVec3d delta);
PL_API void pl_camera_translate_local(plCamera*, plVec3d delta);
PL_API void pl_camera_look_at        (plCamera*, plVec3d eye, plVec3d target, plVec3 up);

// convenience controller helpers
PL_API void pl_camera_rotate_euler_local(plCamera*, float pitch, float yaw, float roll);
PL_API void pl_camera_set_euler         (plCamera*, float pitch, float yaw, float roll);
PL_API void pl_camera_rotate_euler      (plCamera*, float pitch, float yaw, float roll);

// derived data
PL_API void pl_camera_update (plCamera*);

//----------------------------ECS INTEGRATION----------------------------------

// entity helpers
PL_API plEntity pl_camera_ecs_create_perspective (plComponentLibrary*, const char* name, const plCameraPerspectiveDesc*, plCamera**);
PL_API plEntity pl_camera_ecs_create_orthographic(plComponentLibrary*, const char* name, const plCameraOrthographicDesc*, plCamera**);

// system setup/shutdown/etc
PL_API void         pl_camera_ecs_register_ecs_system(void);
PL_API void         pl_camera_ecs_run_ecs            (plComponentLibrary*);
PL_API plEcsTypeKey pl_camera_ecs_get_ecs_type_key   (void);

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plCameraI
{
    // lifecycle
    void (*init)(plCamera*);

    // projection
    void (*set_perspective) (plCamera*, const plCameraPerspectiveDesc*);
    void (*set_orthographic)(plCamera*, const plCameraOrthographicDesc*);
    void (*set_viewport)    (plCamera*, float width, float height);
    void (*set_y_fov)       (plCamera*, float fov);
    void (*set_clip_planes) (plCamera*, float nearZ, float farZ);
    void (*set_depth_mode)  (plCamera*, plCameraDepthMode);

    // pose
    void (*set_position) (plCamera*, plVec3d);
    void (*set_rotation) (plCamera*, plQuat);
    void (*set_transform)(plCamera*, plVec3d position, plQuat rotation);

    // movement
    void (*translate)      (plCamera*, plVec3d delta);
    void (*translate_local)(plCamera*, plVec3d delta);
    void (*look_at)        (plCamera*, plVec3d eye, plVec3d target, plVec3 up);

    // convenience controller helpers
    void (*rotate_euler_local)(plCamera*, float pitch, float yaw, float roll);
    void (*set_euler)         (plCamera*, float pitch, float yaw, float roll);
    void (*rotate_euler)      (plCamera*, float pitch, float yaw, float roll);

    // derived data
    void (*update)(plCamera*);
} plCameraI;

typedef struct _plCameraEcsI
{
    //----------------------------ECS INTEGRATION----------------------------------

    // entity helpers
    plEntity (*create_perspective) (plComponentLibrary*, const char* name, const plCameraPerspectiveDesc*, plCamera**);
    plEntity (*create_orthographic)(plComponentLibrary*, const char* name, const plCameraOrthographicDesc*, plCamera**);

    // system setup/shutdown/etc
    void         (*register_ecs_system)(void);
    void         (*run_ecs)            (plComponentLibrary*);
    plEcsTypeKey (*get_ecs_type_key)   (void);

} plCameraEcsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCameraPerspectiveDesc
{
    plCameraDepthMode eDepthMode;
    float             fYFov;
    float             fAspectRatio;
    float             fNearZ;
    float             fFarZ;
} plCameraPerspectiveDesc;

typedef struct _plCameraOrthographicDesc
{
    plCameraDepthMode eDepthMode;
    float             fWidth;
    float             fHeight;
    float             fNearZ;
    float             fFarZ;
} plCameraOrthographicDesc;

typedef struct _plCamera
{
    // projection
    plCameraProjectionType eProjectionType;
    plCameraDepthMode      eDepthMode;
    float                  fNearZ;
    float                  fFarZ;

    // perspective
    float fYFov;
    float fAspectRatio; // width/height

    // orthographic
    float fWidth;
    float fHeight;

    // pose
    plVec3d tPosition;
    plQuat  tRotation;

    // cached float pose
    plVec3 tPositionF;
    
    // cached matrices
    plMat4 tViewMat;                 // world to camera/view
    plMat4 tProjMat;                 // view to clip
    plMat4 tViewProjMat;             // world to clip
    plMat4 tInvViewMat;              // camera/view to world
    plMat4 tInvProjMat;              // clip to view
    plMat4 tInvViewProjMat;          // clip to world
    plMat4 tViewMatNoTranslation;    // view matrix using camera-relative origin
    plMat4 tInvViewMatNoTranslation; // inverse view matrix using camera-relative origin

    // convenience rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // cached orientation vectors
    plVec3 tUpVec;
    plVec3 tForwardVec;
    plVec3 tRightVec;

    // [INTERNAL]
    plCameraDirtyFlags eDirtyFlags;
} plCamera;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCameraProjectionType
{
    PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE,
    PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC,

    PL_CAMERA_PROJECTION_TYPE_COUNT
};

enum _plCameraDepthMode
{
    PL_CAMERA_DEPTH_MODE_STANDARD,
    PL_CAMERA_DEPTH_MODE_REVERSE_Z,

    PL_CAMERA_DEPTH_MODE_COUNT
};

enum _plCameraDirtyFlags
{
    PL_CAMERA_DIRTY_FLAGS_NONE       = 0,
    PL_CAMERA_DIRTY_FLAGS_VIEW       = 1 << 0,
    PL_CAMERA_DIRTY_FLAGS_PROJECTION = 1 << 1,
    PL_CAMERA_DIRTY_FLAGS_ALL        = PL_CAMERA_DIRTY_FLAGS_VIEW | PL_CAMERA_DIRTY_FLAGS_PROJECTION
};

#ifdef __cplusplus
}
#endif

#endif // PL_CAMERA_EXT_H