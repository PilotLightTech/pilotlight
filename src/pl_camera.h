/*
   pl_camera.h
*/

// library version
#define PL_CAMERA_VERSION    "0.1.0"
#define PL_CAMERA_VERSION_NUM 00100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
// [SECTION] enums
// [SECTION] c file
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CAMERA_H
#define PL_CAMERA_H

#ifndef PL_DECLARE_STRUCT
    #define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
PL_DECLARE_STRUCT(plCamera);

// enums
typedef int plCameraType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// creation
plCamera pl_create_perspective_camera(plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);

// operations
void     pl_camera_set_fov        (plCamera* ptCamera, float fYFov);
void     pl_camera_set_clip_planes(plCamera* ptCamera, float fNearZ, float fFarZ);
void     pl_camera_set_aspect     (plCamera* ptCamera, float fAspect);
void     pl_camera_set_pos        (plCamera* ptCamera, float fX, float fY, float fZ);
void     pl_camera_set_pitch_yaw  (plCamera* ptCamera, float fPitch, float fYaw);
void     pl_camera_translate      (plCamera* ptCamera, float fDx, float fDy, float fDz);
void     pl_camera_rotate         (plCamera* ptCamera, float fDPitch, float fDYaw);
void     pl_camera_update         (plCamera* ptCamera);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plCameraType tType;
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
} plCamera;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plCameraType_
{
    PL_CAMERA_TYPE_NONE,
    PL_CAMERA_TYPE_PERSPECTIVE
};

#endif // PL_CAMERA_H

//-----------------------------------------------------------------------------
// [SECTION] c file
//-----------------------------------------------------------------------------

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] internal api
// [SECTION] public implementation
// [SECTION] internal implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifdef PL_CAMERA_IMPLEMENTATION

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static float pl__wrap_angle        (float tTheta);

//-----------------------------------------------------------------------------
// [SECTION] public implementation
//-----------------------------------------------------------------------------

plCamera
pl_create_perspective_camera(plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ)
{
    plCamera tCamera = {
        .tType        = PL_CAMERA_TYPE_PERSPECTIVE,
        .tPos         = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };
    pl_camera_update(&tCamera);
    return tCamera;
}

void
pl_camera_set_fov(plCamera* ptCamera, float fYFov)
{
    ptCamera->fFieldOfView = fYFov;
}

void
pl_camera_set_clip_planes(plCamera* ptCamera, float fNearZ, float fFarZ)
{
    ptCamera->fNearZ = fNearZ;
    ptCamera->fFarZ = fFarZ;
}

void
pl_camera_set_aspect(plCamera* ptCamera, float fAspect)
{
    ptCamera->fAspectRatio = fAspect;
}

void
pl_camera_set_pos(plCamera* ptCamera, float fX, float fY, float fZ)
{
    ptCamera->tPos.x = fX;
    ptCamera->tPos.y = fY;
    ptCamera->tPos.z = fZ;
}

void
pl_camera_set_pitch_yaw(plCamera* ptCamera, float fPitch, float fYaw)
{
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw = fYaw;
}

void
pl_camera_translate(plCamera* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

void
pl_camera_rotate(plCamera* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = pl__wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
pl_camera_update(plCamera* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    static const plVec4 tOriginalUpVec      = {0.0f, -1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, -1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat = pl_mat4_rotate_vec3(ptCamera->fPitch, (plVec3){1.0f, 0.0f, 0.0f});
    const plMat4 tYRotMat = pl_mat4_rotate_vec3(-ptCamera->fYaw, (plVec3){0.0f, 1.0f, 0.0f});
    const plMat4 tZRotMat = pl_mat4_rotate_vec3(0.0f, (plVec3){0.0f, 0.0f, 1.0f});

    // rotZ * rotY * rotX
    const plMat4 tYRotMat2 = pl_mat4_rotate_vec3(ptCamera->fYaw, (plVec3){0.0f, 1.0f, 0.0f});
    plMat4 tOp0            = pl_mul_mat4t(&tYRotMat2, &tXRotMat);
    tOp0                   = pl_mul_mat4t(&tZRotMat, &tOp0);

    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, -ptCamera->tPos.y, -ptCamera->tPos.z});

    // translate * rotZ * rotY * rotX
    ptCamera->tTransformMat = pl_mul_mat4t(&tYRotMat, &tXRotMat);
    ptCamera->tTransformMat = pl_mul_mat4t(&tZRotMat, &ptCamera->tTransformMat);
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &ptCamera->tTransformMat);

    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tOp0, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tOp0, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tOp0, tOriginalForwardVec)).xyz;

    const plMat4 tFlipY = pl_mat4_scale_xyz(1.0f, -1.0f, -1.0f);
    ptCamera->tViewMat  = pl_mat4t_invert(&ptCamera->tTransformMat);
    ptCamera->tViewMat  = pl_mul_mat4t(&ptCamera->tViewMat, &tFlipY);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;     
}

//-----------------------------------------------------------------------------
// [SECTION] internal implementation
//-----------------------------------------------------------------------------

static float
pl__wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

#endif // PL_CAMERA_IMPLEMENTATION
