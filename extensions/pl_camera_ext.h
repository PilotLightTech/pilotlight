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

        * plEcsI     (v1.x) (only if using ECS integration)
        * plProfileI (v1.x) (only if using ECS integration)
        * plLogI     (v1.x) (only if using ECS integration)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CAMERA_EXT_H
#define PL_CAMERA_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plCameraI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h" // plVec3, plMat4

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plCamera plCamera;

// enums & flags
typedef int plCameraType;

// external
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef uint32_t                   plEcsTypeKey;       // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plCameraI
{
    // operations
    void (*set_fov)        (plCamera*, float fYFov);
    void (*set_clip_planes)(plCamera*, float fNearZ, float fFarZ);
    void (*set_aspect)     (plCamera*, float fAspect);
    void (*set_pos)        (plCamera*, double x, double y, double z);
    void (*set_pitch_yaw)  (plCamera*, float fPitch, float fYaw);
    void (*translate)      (plCamera*, double dX, double dY, double dZ);
    void (*rotate)         (plCamera*, float fDPitch, float fDYaw);
    void (*look_at)        (plCamera*, plDVec3 tEye, plDVec3 tTarget);
    void (*update)         (plCamera*);

    //----------------------------ECS INTEGRATION----------------------------------

    // entity helpers
    plEntity (*create_perspective) (plComponentLibrary*, const char* pcName, plDVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ, bool bReverseZ, plCamera**);
    plEntity (*create_orthographic)(plComponentLibrary*, const char* pcName, plDVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ, plCamera**);

    // system setup/shutdown/etc
    void         (*register_ecs_system)(void);
    void         (*run_ecs)            (plComponentLibrary*);
    plEcsTypeKey (*get_ecs_type_key)   (void);

} plCameraI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plCameraType tType;
    plVec3       tPos;
    plDVec3      tPosDouble;
    float        fNearZ;
    float        fFarZ;
    float        fFieldOfView;
    float        fAspectRatio;        // width/height
    float        fWidth;              // for orthographic
    float        fHeight;             // for orthographic
    plMat4       tViewMat;            // cached
    plMat4       tProjMat;            // cached
    plMat4       tTransformMat;       // cached
    plMat4       tViewMatDouble;      // cached
    plMat4       tTransformMatDouble; // cached

    // rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // [INTERNAL]
    
    // direction vectors
    plVec3       _tUpVec;
    plVec3       _tForwardVec;
    plVec3       _tRightVec;
} plCamera;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plCameraType
{
    PL_CAMERA_TYPE_PERSPECTIVE,
    PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z,
    PL_CAMERA_TYPE_ORTHOGRAPHIC,

    PL_CAMERA_TYPE_COUNT
};

#endif // PL_CAMERA_EXT_H