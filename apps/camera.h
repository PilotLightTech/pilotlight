/*
   camera.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef CAMERA_H
#define CAMERA_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

typedef struct _plCamera plCamera;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plCamera pl_camera_create         (plVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ);
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

#endif // CAMERA_H