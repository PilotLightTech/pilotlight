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

void
pl_camera_update(plCamera* ptCamera)
{
    ptCamera->tPos.x = (float)ptCamera->tPosDouble.x;
    ptCamera->tPos.y = (float)ptCamera->tPosDouble.y;
    ptCamera->tPos.z = (float)ptCamera->tPosDouble.z;

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat         = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat         = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat         = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate       = pl_mat4_translate_vec3(ptCamera->tPos);
    const plMat4 tTranslateDouble = pl_identity_mat4();

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat       = pl_mul_mat4t(&tTranslate, &tRotations);
    ptCamera->tTransformMatDouble = pl_mul_mat4t(&tTranslateDouble, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat       = pl_mat4t_invert(&ptCamera->tTransformMat);
    ptCamera->tViewMatDouble = pl_mat4t_invert(&ptCamera->tTransformMatDouble);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat       = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);
    ptCamera->tViewMatDouble = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMatDouble);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    switch(ptCamera->tType)
    {
        case PL_CAMERA_TYPE_PERSPECTIVE:
        {
            const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
            ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
            ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
            ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
            ptCamera->tProjMat.col[2].w = 1.0f;
            ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
            ptCamera->tProjMat.col[3].w = 0.0f;  
            break;
        }

        case PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z:
        {
            const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
            ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
            ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
            ptCamera->tProjMat.col[2].z = ptCamera->fNearZ / (ptCamera->fNearZ - ptCamera->fFarZ);
            ptCamera->tProjMat.col[2].w = 1.0f;
            ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fNearZ - ptCamera->fFarZ);
            ptCamera->tProjMat.col[3].w = 0.0f;  
            break;
        }

        case PL_CAMERA_TYPE_ORTHOGRAPHIC:
        {
            ptCamera->tProjMat.col[0].x = 2.0f / ptCamera->fWidth;
            ptCamera->tProjMat.col[1].y = 2.0f / ptCamera->fHeight;
            ptCamera->tProjMat.col[2].z = 1 / (ptCamera->fFarZ - ptCamera->fNearZ);
            ptCamera->tProjMat.col[3].w = 1.0f;
            break;
        }

        default:
        {
            PL_ASSERT(false && "Unknown camera component type");
            break;
        }
    }
}

plEntity
pl_camera_create_perspective_camera(plComponentLibrary* ptLibrary, const char* pcName, plDVec3 tPos, float fYFov, float fAspect, float fNearZ, float fFarZ, bool bReverseZ, plCamera** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created camera: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    const plCamera tCamera = {
        .tType        = bReverseZ ? PL_CAMERA_TYPE_PERSPECTIVE_REVERSE_Z : PL_CAMERA_TYPE_PERSPECTIVE,
        .tPos         = {(float)tPos.x, (float)tPos.y, (float)tPos.z},
        .tPosDouble   = tPos,
        .fNearZ       = fNearZ,
        .fFarZ        = fFarZ,
        .fFieldOfView = fYFov,
        .fAspectRatio = fAspect
    };

    plCamera* ptCamera = gptECS->add_component(ptLibrary, gptCameraCtx->uManagerIndex, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity; 
}

plEntity
pl_camera_create_orthographic_camera(plComponentLibrary* ptLibrary, const char* pcName, plDVec3 tPos, float fWidth, float fHeight, float fNearZ, float fFarZ, plCamera** pptCompOut)
{
    pcName = pcName ? pcName : "unnamed camera";
    pl_log_debug_f(gptLog, gptECS->get_log_channel(), "created camera: '%s'", pcName);
    plEntity tNewEntity = gptECS->create_entity(ptLibrary, pcName);

    const plCamera tCamera = {
        .tType      = PL_CAMERA_TYPE_ORTHOGRAPHIC,
        .tPos       = {(float)tPos.x, (float)tPos.y, (float)tPos.z},
        .tPosDouble = tPos,
        .fNearZ     = fNearZ,
        .fFarZ      = fFarZ,
        .fWidth     = fWidth,
        .fHeight    = fHeight
    };

    plCamera* ptCamera = gptECS->add_component(ptLibrary, gptCameraCtx->uManagerIndex, tNewEntity);
    *ptCamera = tCamera;
    pl_camera_update(ptCamera);

    if(pptCompOut)
        *pptCompOut = ptCamera;

    return tNewEntity;    
}

void
pl_run_camera_update_system(plComponentLibrary* ptLibrary)
{
    pl_begin_cpu_sample(gptProfile, 0, __FUNCTION__);

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
            ptCamera->tPosDouble.x = (double)ptTransform->tWorld.col[3].x;
            ptCamera->tPosDouble.y = (double)ptTransform->tWorld.col[3].y;
            ptCamera->tPosDouble.z = (double)ptTransform->tWorld.col[3].z;

            pl_camera_update(ptCamera);
        }
    }
    pl_end_cpu_sample(gptProfile, 0);
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
pl_camera_set_pos(plCamera* ptCamera, double dX, double dY, double dZ)
{
    ptCamera->tPosDouble.x = dX;
    ptCamera->tPosDouble.y = dY;
    ptCamera->tPosDouble.z = dZ;
    ptCamera->tPos.x = (float)ptCamera->tPosDouble.x;
    ptCamera->tPos.y = (float)ptCamera->tPosDouble.y;
    ptCamera->tPos.z = (float)ptCamera->tPosDouble.z;
}

void
pl_camera_set_pitch_yaw(plCamera* ptCamera, float fPitch, float fYaw)
{
    ptCamera->fPitch = fPitch;
    ptCamera->fYaw = fYaw;
}

void
pl_camera_translate(plCamera* ptCamera, double dDx, double dDy, double dDz)
{
    plVec3 tRightChange = pl_mul_vec3_scalarf(ptCamera->_tRightVec, (float)dDx);
    plVec3 tForwardChange = pl_mul_vec3_scalarf(ptCamera->_tForwardVec, (float)dDz);
    ptCamera->tPosDouble = pl_add_vec3_d(ptCamera->tPosDouble, (plDVec3){.x = (double)tRightChange.x,   .y = (double)tRightChange.y,   .z = (double)tRightChange.z});
    ptCamera->tPosDouble = pl_add_vec3_d(ptCamera->tPosDouble, (plDVec3){.x = (double)tForwardChange.x, .y = (double)tForwardChange.y, .z = (double)tForwardChange.z});
    ptCamera->tPosDouble.y += dDy;

    ptCamera->tPos.x = (float)ptCamera->tPosDouble.x;
    ptCamera->tPos.y = (float)ptCamera->tPosDouble.y;
    ptCamera->tPos.z = (float)ptCamera->tPosDouble.z;
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
pl_camera_look_at(plCamera* ptCamera, plDVec3 tEye, plDVec3 tTarget)
{
    const plDVec3 tDirection = pl_norm_vec3_d(pl_sub_vec3_d(tTarget, tEye));
    ptCamera->fYaw = (float)atan2(tDirection.x, tDirection.z);
    ptCamera->fPitch = (float)asin(tDirection.y);
    ptCamera->tPosDouble = tEye;

    ptCamera->tPos.x = (float)ptCamera->tPosDouble.x;
    ptCamera->tPos.y = (float)ptCamera->tPosDouble.y;
    ptCamera->tPos.z = (float)ptCamera->tPosDouble.z;
}

void
pl_camera_register_system(void)
{
    static const plComponentDesc tDesc = {
        .pcName = "Camera",
        .szSize = sizeof(plCamera)
    };
    gptCameraCtx->uManagerIndex = gptECS->register_type(tDesc, NULL);
}

plEcsTypeKey
pl_camera_get_ecs_type_key(void)
{
    return gptCameraCtx->uManagerIndex;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

static void
pl_load_camera_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCameraI tApi = {
        .register_ecs_system = pl_camera_register_system,
        .run_ecs             = pl_run_camera_update_system,
        .get_ecs_type_key    = pl_camera_get_ecs_type_key,
        .create_perspective  = pl_camera_create_perspective_camera,
        .create_orthographic = pl_camera_create_orthographic_camera,
        .set_fov             = pl_camera_set_fov,
        .set_clip_planes     = pl_camera_set_clip_planes,
        .set_aspect          = pl_camera_set_aspect,
        .set_pos             = pl_camera_set_pos,
        .set_pitch_yaw       = pl_camera_set_pitch_yaw,
        .translate           = pl_camera_translate,
        .rotate              = pl_camera_rotate,
        .update              = pl_camera_update,
        .look_at             = pl_camera_look_at,
    };
    pl_set_api(ptApiRegistry, plCameraI, &tApi);

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

static void
pl_unload_camera_ecs_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;

    const plCameraI* ptApi1 = pl_get_api_latest(ptApiRegistry, plCameraI);
    ptApiRegistry->remove_api(ptApi1);
}