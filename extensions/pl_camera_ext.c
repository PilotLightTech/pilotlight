/*
   pl_camera_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] global data
// [SECTION] internal api
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl.h"
#include "pl_ecs_ext.h"
#include "pl_animation_ext.h"
#include "pl_camera_ext.h"
#include "pl_math.h"

// extensions
#include "pl_profile_ext.h"
#include "pl_log_ext.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plProfileI* gptProfile = NULL;
    static const plLogI*     gptLog     = NULL;
    static const plEcsI*     gptECS     = NULL;
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCameraContext
{
    plEcsTypeKey uManagerIndex;
} plCameraContext;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plCameraContext* gptCameraCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api
//-----------------------------------------------------------------------------

static inline float
pl__wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

static inline plVec4
pl__quat_from_pitch_yaw_roll(float fPitch, float fYaw, float fRoll)
{
    static const plVec3 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f};
    static const plVec3 tOriginalUpVec      = { 0.0f, 1.0f, 0.0f};
    static const plVec3 tOriginalForwardVec = { 0.0f, 0.0f, 1.0f};

    const plVec4 qPitch = pl_quat_rotation_vec3(fPitch, tOriginalRightVec);
    const plVec4 qYaw   = pl_quat_rotation_vec3(fYaw, tOriginalUpVec);
    const plVec4 qRoll  = pl_quat_rotation_vec3(fRoll, tOriginalForwardVec);

    // Match old matrix order: rotY * rotX * rotZ
    return pl_norm_quat(pl_mul_quat(qYaw, pl_mul_quat(qPitch, qRoll)));
}

static inline plVec4
pl__quat_from_mat3_basis(plVec3 right, plVec3 up, plVec3 forward)
{
    // This assumes the basis is stored as columns:
    //
    // [ right.x   up.x   forward.x ]
    // [ right.y   up.y   forward.y ]
    // [ right.z   up.z   forward.z ]
    //
    // Standard matrix-to-quat conversion.

    const float m00 = right.x;
    const float m01 = up.x;
    const float m02 = forward.x;

    const float m10 = right.y;
    const float m11 = up.y;
    const float m12 = forward.y;

    const float m20 = right.z;
    const float m21 = up.z;
    const float m22 = forward.z;

    plVec4 q = {0};

    const float trace = m00 + m11 + m22;

    if(trace > 0.0f)
    {
        const float s = sqrtf(trace + 1.0f) * 2.0f;
        q.w = 0.25f * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    }
    else if((m00 > m11) && (m00 > m22))
    {
        const float s = sqrtf(1.0f + m00 - m11 - m22) * 2.0f;
        q.w = (m21 - m12) / s;
        q.x = 0.25f * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    }
    else if(m11 > m22)
    {
        const float s = sqrtf(1.0f + m11 - m00 - m22) * 2.0f;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25f * s;
        q.z = (m12 + m21) / s;
    }
    else
    {
        const float s = sqrtf(1.0f + m22 - m00 - m11) * 2.0f;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25f * s;
    }

    return pl_norm_quat(q);
}

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void
pl_camera_update(plCamera* ptCamera)
{
    ptCamera->tPositionF.x = (float)ptCamera->tPosition.x;
    ptCamera->tPositionF.y = (float)ptCamera->tPosition.y;
    ptCamera->tPositionF.z = (float)ptCamera->tPosition.z;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptCamera->eDirtyFlags & PL_CAMERA_DIRTY_FLAGS_VIEW)
    {
    
        static const plVec4 tOriginalXVec       = {1.0f, 0.0f, 0.0f, 0.0f};
        static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
        static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};

        const plMat4 tTranslate       = pl_mat4_translate_vec3(ptCamera->tPositionF);
        const plMat4 tTranslateDouble = pl_identity_mat4();

        ptCamera->tRotation = pl_norm_quat(ptCamera->tRotation);

        // Proper camera/world basis from quaternion.
        const plVec3 tCameraX = pl_norm_vec3(pl_mul_quat_vec3(tOriginalXVec.xyz,       ptCamera->tRotation));
        const plVec3 tCameraY = pl_norm_vec3(pl_mul_quat_vec3(tOriginalUpVec.xyz,      ptCamera->tRotation));
        const plVec3 tCameraZ = pl_norm_vec3(pl_mul_quat_vec3(tOriginalForwardVec.xyz, ptCamera->tRotation));

        // Public/convenience vectors.
        // Preserve your old convention: "right" was rot * (-X), not matrix column 0.
        ptCamera->tRightVec   = pl_mul_vec3_scalarf(tCameraX, -1.0f);
        ptCamera->tUpVec      = tCameraY;
        ptCamera->tForwardVec = tCameraZ;

        plMat4 tRotations = pl_identity_mat4();

        // IMPORTANT:
        // Use +X, +Y, +Z basis here. This keeps determinant +1.
        tRotations.col[0].x = tCameraX.x;
        tRotations.col[0].y = tCameraX.y;
        tRotations.col[0].z = tCameraX.z;
        tRotations.col[0].w = 0.0f;

        tRotations.col[1].x = tCameraY.x;
        tRotations.col[1].y = tCameraY.y;
        tRotations.col[1].z = tCameraY.z;
        tRotations.col[1].w = 0.0f;

        tRotations.col[2].x = tCameraZ.x;
        tRotations.col[2].y = tCameraZ.y;
        tRotations.col[2].z = tCameraZ.z;
        tRotations.col[2].w = 0.0f;

        tRotations.col[3].x = 0.0f;
        tRotations.col[3].y = 0.0f;
        tRotations.col[3].z = 0.0f;
        tRotations.col[3].w = 1.0f;

        ptCamera->tInvViewMat    = pl_mul_mat4t(&tTranslate,       &tRotations);
        ptCamera->tInvViewMatNoTranslation = pl_mul_mat4t(&tTranslateDouble, &tRotations);

        ptCamera->tViewMat    = pl_mat4t_invert(&ptCamera->tInvViewMat);
        ptCamera->tViewMatNoTranslation = pl_mat4t_invert(&ptCamera->tInvViewMatNoTranslation);

        const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);

        ptCamera->tViewMat    = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);
        ptCamera->tViewMatNoTranslation = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMatNoTranslation);
    }

    if(ptCamera->eDirtyFlags & PL_CAMERA_DIRTY_FLAGS_PROJECTION)
    {
        //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        switch(ptCamera->eProjectionType)
        {
            case PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE:
            {
                const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fYFov / 2.0f);
                ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
                ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;

                ptCamera->tProjMat.col[2].w = 1.0f;
                ptCamera->tProjMat.col[3].w = 0.0f;

                if(ptCamera->eDepthMode == PL_CAMERA_DEPTH_MODE_STANDARD)
                {
                    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
                    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
                }
                else if(ptCamera->eDepthMode == PL_CAMERA_DEPTH_MODE_REVERSE_Z)
                {
                    ptCamera->tProjMat.col[2].z = ptCamera->fNearZ / (ptCamera->fNearZ - ptCamera->fFarZ);
                    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fNearZ - ptCamera->fFarZ);
                }
                break;
            }

            case PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC:
            {
                ptCamera->tProjMat.col[0].x = 2.0f / ptCamera->fWidth;
                ptCamera->tProjMat.col[1].y = 2.0f / ptCamera->fHeight;
                ptCamera->tProjMat.col[3].w = 1.0f;

                if(ptCamera->eDepthMode == PL_CAMERA_DEPTH_MODE_STANDARD)
                {
                    ptCamera->tProjMat.col[2].z = 1 / (ptCamera->fFarZ - ptCamera->fNearZ);
                }
                else if(ptCamera->eDepthMode == PL_CAMERA_DEPTH_MODE_REVERSE_Z)
                {
                    ptCamera->tProjMat.col[2].z = 1 / (ptCamera->fFarZ - ptCamera->fNearZ);
                    ptCamera->tProjMat.col[3].z = -ptCamera->fFarZ / (ptCamera->fNearZ - ptCamera->fFarZ);
                }
                break;
            }

            default:
            {
                PL_ASSERT(false && "Unknown camera component type");
                break;
            }
        }

        ptCamera->tInvProjMat = pl_mat4t_invert(&ptCamera->tProjMat);
    }

    if(ptCamera->eDirtyFlags != PL_CAMERA_DIRTY_FLAGS_NONE)
    {
        ptCamera->tViewProjMat = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
        ptCamera->tInvViewProjMat = pl_mat4t_invert(&ptCamera->tViewProjMat);
    }

    ptCamera->eDirtyFlags = PL_CAMERA_DIRTY_FLAGS_NONE;
}

void
pl_camera_init(plCamera* ptCamera)
{
    ptCamera->tRotation       = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptCamera->tPositionF            = (plVec3){0};
    ptCamera->tPosition      = (plVec3d){0};
    ptCamera->eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    ptCamera->eDepthMode      = PL_CAMERA_DEPTH_MODE_REVERSE_Z;
    ptCamera->fNearZ          = 0.1f;
    ptCamera->fFarZ           = 1000.0f;
    ptCamera->fYFov    = PL_PI / 4.0f;
    ptCamera->fAspectRatio    = 16.0f / 9.0f;
    ptCamera->eDirtyFlags     = PL_CAMERA_DIRTY_FLAGS_ALL;
}

void
pl_camera_set_perspective(plCamera* ptCamera, const plCameraPerspectiveDesc* ptDesc)
{
    ptCamera->eProjectionType = PL_CAMERA_PROJECTION_TYPE_PERSPECTIVE;
    ptCamera->eDepthMode      = ptDesc->eDepthMode;
    ptCamera->fNearZ       = ptDesc->fNearZ;
    ptCamera->fFarZ        = ptDesc->fFarZ;
    ptCamera->fYFov = ptDesc->fYFov;
    ptCamera->fAspectRatio = ptDesc->fAspectRatio;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_ALL;
}

void
pl_camera_set_orthographic(plCamera* ptCamera, const plCameraOrthographicDesc* ptDesc)
{
    ptCamera->eProjectionType = PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC;
    ptCamera->eDepthMode      = ptDesc->eDepthMode;
    ptCamera->fNearZ     = ptDesc->fNearZ;
    ptCamera->fFarZ      = ptDesc->fFarZ;
    ptCamera->fWidth     = ptDesc->fWidth;
    ptCamera->fHeight    = ptDesc->fHeight;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_ALL;
}

plEntity
pl_camera_ecs_create_perspective(plComponentLibrary* ptLibrary, const char* pcName, const plCameraPerspectiveDesc* ptDesc, plCamera** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    PL_LOG_DEBUG_API_F(gptLog, gptECS->get_log_channel(), "created camera: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plCamera tCamera = {0};
    pl_camera_init(&tCamera);
    pl_camera_set_perspective(&tCamera, ptDesc);

    plCamera* ptCamera = gptECS->add_component(ptLibrary, gptCameraCtx->uManagerIndex, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity; 
}

plEntity
pl_camera_ecs_create_orthographic(plComponentLibrary* ptLibrary, const char* pcName, const plCameraOrthographicDesc* ptDesc, plCamera** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    PL_LOG_DEBUG_API_F(gptLog, gptECS->get_log_channel(), "created camera: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    plCamera tCamera = {0};
    pl_camera_init(&tCamera);
    pl_camera_set_orthographic(&tCamera, ptDesc);

    plCamera* ptCamera = gptECS->add_component(ptLibrary, gptCameraCtx->uManagerIndex, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity;    
}

void
pl_camera_ecs_run_ecs(plComponentLibrary* ptLibrary)
{
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    plCamera* ptComponents = NULL;
    const plEntity* ptEntities = NULL;

    const uint32_t uComponentCount = gptECS->get_components(ptLibrary, gptCameraCtx->uManagerIndex, (void**)&ptComponents, &ptEntities);
    const plEcsTypeKey tTransformComponentType = gptECS->get_ecs_type_key_transform();

    for(uint32_t i = 0; i < uComponentCount; i++)
    {
        plEntity tEntity = ptEntities[i];
        if(gptECS->has_component(ptLibrary, tTransformComponentType, tEntity))
        {
            plCamera* ptCamera = &ptComponents[i];
            plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, tTransformComponentType, tEntity);
            ptCamera->tPosition.x = (double)ptTransform->tWorld.col[3].x;
            ptCamera->tPosition.y = (double)ptTransform->tWorld.col[3].y;
            ptCamera->tPosition.z = (double)ptTransform->tWorld.col[3].z;
            ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;

            pl_camera_update(ptCamera);
        }
    }
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
}

PL_API void
pl_camera_set_viewport(plCamera* ptCamera, float fWidth, float fHeight)
{
    ptCamera->fWidth = fWidth;
    ptCamera->fHeight = fHeight;
    ptCamera->fAspectRatio = fWidth / fHeight;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_PROJECTION;
}

void
pl_camera_set_y_fov(plCamera* ptCamera, float fYFov)
{
    ptCamera->fYFov = fYFov;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_PROJECTION;
}

void
pl_camera_set_clip_planes(plCamera* ptCamera, float fNearZ, float fFarZ)
{
    ptCamera->fNearZ = fNearZ;
    ptCamera->fFarZ = fFarZ;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_PROJECTION;
}

void
pl_camera_set_depth_mode(plCamera* ptCamera, plCameraDepthMode eMode)
{
    ptCamera->eDepthMode = eMode;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_PROJECTION;
}

void
pl_camera_set_position(plCamera* ptCamera, plVec3d tPositionF)
{
    ptCamera->tPosition = tPositionF;
    ptCamera->tPositionF.x = (float)ptCamera->tPosition.x;
    ptCamera->tPositionF.y = (float)ptCamera->tPosition.y;
    ptCamera->tPositionF.z = (float)ptCamera->tPosition.z;
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_set_rotation(plCamera* ptCamera, plQuat tRotation)
{
    ptCamera->tRotation = pl_norm_quat(tRotation);
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_set_transform(plCamera* ptCamera, plVec3d tPosition, plQuat tRotation)
{
    pl_camera_set_rotation(ptCamera, tRotation);
    pl_camera_set_position(ptCamera, tPosition);
}

void
pl_camera_set_euler(plCamera* ptCamera, float fPitch, float fYaw, float fRoll)
{
    // ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, fPitch, 0.995f * PL_PI_2);
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw   = pl__wrap_angle(fYaw);
    ptCamera->fRoll  = fRoll;

    ptCamera->tRotation = pl__quat_from_pitch_yaw_roll(
        ptCamera->fPitch,
        ptCamera->fYaw,
        ptCamera->fRoll
    );
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_translate(plCamera* ptCamera, plVec3d tDelta)
{
    ptCamera->tPosition = pl_add_vec3_d(ptCamera->tPosition, tDelta);

    ptCamera->tPositionF.x = (float)ptCamera->tPosition.x;
    ptCamera->tPositionF.y = (float)ptCamera->tPosition.y;
    ptCamera->tPositionF.z = (float)ptCamera->tPosition.z;

    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_translate_local(plCamera* ptCamera, plVec3d tDelta)
{
    if(ptCamera->eDirtyFlags & PL_CAMERA_DIRTY_FLAGS_VIEW)
        pl_camera_update(ptCamera);

    plVec3 tRightChange   = pl_mul_vec3_scalarf(ptCamera->tRightVec,   (float)tDelta.x);
    plVec3 tForwardChange = pl_mul_vec3_scalarf(ptCamera->tForwardVec, (float)tDelta.z);
    plVec3 tUpChange      = pl_mul_vec3_scalarf(ptCamera->tUpVec,      (float)tDelta.y);

    ptCamera->tPosition = pl_add_vec3_d(ptCamera->tPosition, (plVec3d){
        .x = (double)tRightChange.x + (double)tForwardChange.x + (double)tUpChange.x,
        .y = (double)tRightChange.y + (double)tForwardChange.y + (double)tUpChange.y,
        .z = (double)tRightChange.z + (double)tForwardChange.z + (double)tUpChange.z
    });

    ptCamera->tPositionF.x = (float)ptCamera->tPosition.x;
    ptCamera->tPositionF.y = (float)ptCamera->tPosition.y;
    ptCamera->tPositionF.z = (float)ptCamera->tPosition.z;

    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_rotate_euler(plCamera* ptCamera, float fDPitch, float fDYaw, float fRoll)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw   += fDYaw;
    ptCamera->fRoll  += fRoll;

    ptCamera->fYaw   = pl__wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);

    ptCamera->tRotation = pl__quat_from_pitch_yaw_roll(
        ptCamera->fPitch,
        ptCamera->fYaw,
        ptCamera->fRoll
    );

    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_rotate_euler_local(plCamera* ptCamera, float fDPitch, float fDYaw, float fRoll)
{
    static const plVec3 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f};
    static const plVec3 tOriginalUpVec      = { 0.0f, 1.0f, 0.0f};
    static const plVec3 tOriginalForwardVec = { 0.0f, 0.0f, 1.0f};

    const plVec4 qPitch = pl_quat_rotation_vec3(fDPitch, tOriginalRightVec);
    const plVec4 qYaw   = pl_quat_rotation_vec3(fDYaw, tOriginalUpVec);
    const plVec4 qRoll  = pl_quat_rotation_vec3(fRoll, tOriginalForwardVec);

    const plVec4 qDelta = pl_norm_quat(pl_mul_quat(qYaw, pl_mul_quat(qPitch, qRoll)));

    ptCamera->tRotation = pl_norm_quat(pl_mul_quat(qDelta, ptCamera->tRotation));
    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_look_at(plCamera* ptCamera, plVec3d tEye, plVec3d tTarget, plVec3 tUp)
{
    // const plVec3d tDirection = pl_norm_vec3_d(pl_sub_vec3_d(tTarget, tEye));
    // ptCamera->fYaw = (float)atan2(tDirection.x, tDirection.z);
    // ptCamera->fPitch = (float)asin(tDirection.y);
    // ptCamera->tPosition = tEye;

    // ptCamera->tPositionF.x = (float)ptCamera->tPosition.x;
    // ptCamera->tPositionF.y = (float)ptCamera->tPosition.y;
    // ptCamera->tPositionF.z = (float)ptCamera->tPosition.z;

    // ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;

    plVec3d tForwardD = pl_norm_vec3_d(pl_sub_vec3_d(tTarget, tEye));

    plVec3 tForward = {
        (float)tForwardD.x,
        (float)tForwardD.y,
        (float)tForwardD.z
    };

    tForward = pl_norm_vec3(tForward);
    tUp = pl_norm_vec3(tUp);

    // Avoid degenerate up vector.
    if(fabsf(pl_dot_vec3(tForward, tUp)) > 0.999f)
        tUp = (plVec3){0.0f, 1.0f, 0.0f};

    // Because your camera's "right" basis is -X, this cross order matters.
    // Test this with identity camera and a simple target.
    plVec3 tRight = pl_norm_vec3(pl_cross_vec3(tUp, tForward));
    tUp = pl_norm_vec3(pl_cross_vec3(tForward, tRight));

    // Your original camera basis has right = -X.
    // If the camera appears mirrored, negate tRight here.
    // tRight = pl_mul_vec3_scalarf(tRight, -1.0f);

    ptCamera->tRotation = pl__quat_from_mat3_basis(tRight, tUp, tForward);

    ptCamera->tPosition = tEye;
    ptCamera->tPositionF.x = (float)tEye.x;
    ptCamera->tPositionF.y = (float)tEye.y;
    ptCamera->tPositionF.z = (float)tEye.z;

    // Optional: keep debug/controller Euler values approximately in sync.
    ptCamera->fYaw   = (float)atan2(tForward.x, tForward.z);
    ptCamera->fPitch = (float)asin(tForward.y);
    ptCamera->fRoll  = 0.0f;

    ptCamera->eDirtyFlags |= PL_CAMERA_DIRTY_FLAGS_VIEW;
}

void
pl_camera_ecs_register_ecs_system(void)
{
    static const plComponentDesc tDesc = {
        .pcName = "Camera",
        .szSize = sizeof(plCamera)
    };
    gptCameraCtx->uManagerIndex = gptECS->register_type(tDesc, NULL);
}

plEcsTypeKey
pl_camera_ecs_get_ecs_type_key(void)
{
    return gptCameraCtx->uManagerIndex;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_camera_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCameraI tApi0 = {
        .init                = pl_camera_init,
        .set_perspective     = pl_camera_set_perspective,
        .set_orthographic    = pl_camera_set_orthographic,
        .set_y_fov           = pl_camera_set_y_fov,
        .set_clip_planes     = pl_camera_set_clip_planes,
        .set_depth_mode      = pl_camera_set_depth_mode,
        .set_viewport        = pl_camera_set_viewport,
        .set_position        = pl_camera_set_position,
        .set_rotation        = pl_camera_set_rotation,
        .set_transform       = pl_camera_set_transform,
        .set_euler  = pl_camera_set_euler,
        .translate           = pl_camera_translate,
        .translate_local     = pl_camera_translate_local,
        .rotate_euler        = pl_camera_rotate_euler,
        .rotate_euler_local        = pl_camera_rotate_euler_local,
        .update              = pl_camera_update,
        .look_at             = pl_camera_look_at,
    };
    pl_set_api(ptApiRegistry, plCameraI, &tApi0);

    const plCameraEcsI tApi1 = {
        .register_ecs_system = pl_camera_ecs_register_ecs_system,
        .run_ecs             = pl_camera_ecs_run_ecs,
        .get_ecs_type_key    = pl_camera_ecs_get_ecs_type_key,
        .create_perspective  = pl_camera_ecs_create_perspective,
        .create_orthographic = pl_camera_ecs_create_orthographic,
    };
    pl_set_api(ptApiRegistry, plCameraEcsI, &tApi1);

    gptProfile = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog     = pl_get_api_latest(ptApiRegistry, plLogI);
    gptECS     = pl_get_api_latest(ptApiRegistry, plEcsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    if(bReload)
    {
        gptCameraCtx = ptDataRegistry->get_data("plCameraContext");
    }
    else // first load
    {
        static plCameraContext tCtx = {0};
        gptCameraCtx = &tCtx;
        ptDataRegistry->set_data("plCameraContext", gptCameraCtx);
    }
}

void
pl_unload_camera_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plCameraI* ptApi0 = pl_get_api_latest(ptApiRegistry, plCameraI);
    ptApiRegistry->remove_api(ptApi0);

    const plCameraEcsI* ptApi1 = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
    ptApiRegistry->remove_api(ptApi1);
}