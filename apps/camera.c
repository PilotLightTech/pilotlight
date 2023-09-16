/*
   camera.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] helpers
// [SECTION] implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "camera.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] helpers
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

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

plCamera
pl_camera_create(plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ)
{
    plCamera tCamera = {
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
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;    
}
