/*
   pl_gizmo_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs & enums
// [SECTION] context
// [SECTION] internal api declarations
// [SECTION] public api implementation
// [SECTION] internal api implementation
// [SECTION] extension loading
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.h"
#include <float.h>
#include <stdbool.h>
#include "pl_gizmo_ext.h"
#include "pl_draw_ext.h"
#include "pl_ecs_ext.h"
#include "pl_ui_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
static const plUiI*   gptUI   = NULL;
static const plDrawI* gptDraw = NULL;
static const plIOI*   gptIOI  = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs & enums
//-----------------------------------------------------------------------------

typedef enum _plGizmoState
{
    PL_GIZMO_STATE_DEFAULT,
    PL_GIZMO_STATE_X_TRANSLATION,
    PL_GIZMO_STATE_Y_TRANSLATION,
    PL_GIZMO_STATE_Z_TRANSLATION,
    PL_GIZMO_STATE_YZ_TRANSLATION,
    PL_GIZMO_STATE_XY_TRANSLATION,
    PL_GIZMO_STATE_XZ_TRANSLATION,
    PL_GIZMO_STATE_X_ROTATION,
    PL_GIZMO_STATE_Y_ROTATION,
    PL_GIZMO_STATE_Z_ROTATION,
    PL_GIZMO_STATE_X_SCALE,
    PL_GIZMO_STATE_Y_SCALE,
    PL_GIZMO_STATE_Z_SCALE,
    PL_GIZMO_STATE_SCALE,
} plGizmoState;

typedef struct _plGizmoContext
{
    plGizmoMode  tSelectionMode;
    plGizmoState tState;
    float        fCaptureScale;
    plVec3       tBeginPos;
    plVec3       tOriginalPos;
    float        fOriginalDist;
    float        fOriginalDistX;
    float        fOriginalDistY;
    float        fOriginalDistZ;
    float        fOriginalScaleX;
    float        fOriginalScaleY;
    float        fOriginalScaleZ;
    plVec4       tOriginalRot;
    bool         bActive;
} plGizmoContext;

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

static plGizmoContext* gptGizmoCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] internal api declarations
//-----------------------------------------------------------------------------

static bool pl__does_line_intersect_cylinder(plVec3 tP0, plVec3 tV0, plVec3 tP1, plVec3 tV1, float fRadius, float fHeight, float* pfDistance);
static bool pl__does_line_intersect_plane   (plVec3 tP0, plVec3 tV0, plVec4 tPlane, plVec3* ptQ);
static void pl__gizmo_translation           (plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);
static void pl__gizmo_rotation              (plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);
static void pl__gizmo_scale                 (plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

static void
pl_gizmo_set_mode(plGizmoMode tMode)
{
    gptGizmoCtx->tSelectionMode = tMode;
}

static bool
pl_gizmo_active(void)
{
    return gptGizmoCtx->bActive;
}

static void
pl_gizmo_next_mode(void)
{
    gptGizmoCtx->tSelectionMode = (gptGizmoCtx->tSelectionMode + 1) % PL_GIZMO_MODE_COUNT;
}

static void
pl_gizmo(plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    if(ptSelectedTransform)
    {
        plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;
        if(gptGizmoCtx->tState == PL_GIZMO_STATE_DEFAULT)
        {
            gptGizmoCtx->fCaptureScale = pl_length_vec3(pl_sub_vec3(*ptCenter, ptCamera->tPos)) * 0.75f;
        }

        switch(gptGizmoCtx->tSelectionMode)
        {
            case PL_GIZMO_MODE_TRANSLATION:
                pl__gizmo_translation(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                ptSelectedTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
                break;
            case PL_GIZMO_MODE_ROTATION:
                pl__gizmo_rotation(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                ptSelectedTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
                break;
            case PL_GIZMO_MODE_SCALE:
                pl__gizmo_scale(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                ptSelectedTransform->tFlags |= PL_TRANSFORM_FLAGS_DIRTY;
                break;
            case PL_GIZMO_MODE_NONE:
            default:
                break;

        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

static bool
pl__does_line_intersect_cylinder(plVec3 tP0, plVec3 tV0, plVec3 tP1, plVec3 tV1, float fRadius, float fHeight, float* pfDistance)
{
    plVec3 tDp = pl_sub_vec3(tP1, tP0);

    float fV12 = pl_dot_vec3(tV0, tV0);
    float fV22 = pl_dot_vec3(tV1, tV1);
    float fV1V2 = pl_dot_vec3(tV0, tV1);

    float fDet = fV1V2 * fV1V2 - fV12 * fV22;

    float fDistance = FLT_MAX;

    if(fabsf(fDet) > FLT_MIN)
    {
        fDet = 1.0f / fDet;
        float fDpv1 = pl_dot_vec3(tDp, tV0);
        float fDpv2 = pl_dot_vec3(tDp, tV1);

        float fT1 = (fV1V2 * fDpv2 - fV22 * fDpv1) * fDet;
        float fT2 = (fV12 * fDpv2 - fV1V2 * fDpv1) * fDet;

        fDistance = pl_length_vec3(pl_add_vec3(tDp, pl_sub_vec3(pl_mul_vec3_scalarf(tV1, fT2), pl_mul_vec3_scalarf(tV0, fT1))));

        *pfDistance = fT2;

        if(fDistance < fRadius && fT2 > 0.0f && fT2 < fHeight)
            return true;
    }
    return false;
}

static bool
pl__does_line_intersect_plane(plVec3 tP0, plVec3 tV0, plVec4 tPlane, plVec3* ptQ)
{
    const float fFv = pl_dot_vec3(tPlane.xyz, tV0);
    if(fabsf(fFv) > FLT_MIN)
    {
        *ptQ = pl_sub_vec3(tP0, pl_mul_vec3_scalarf(tV0, (pl_dot_vec3(tPlane.xyz, tP0) + tPlane.w) / fFv));
        return true;
    }
    return false;
}

static void
pl__gizmo_translation(plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIOI->get_mouse_pos();

    if(gptUI->wants_mouse_capture())
    {
        tMousePos.x = -1.0f;
        tMousePos.y = -1.0f;
    }

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIOI->get_io()->tMainViewportSize.x, -1.0f + 2.0f * tMousePos.y / gptIOI->get_io()->tMainViewportSize.y, 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

    const float fAxisRadius  = 0.0035f * gptGizmoCtx->fCaptureScale;
    const float fArrowRadius = 0.0075f * gptGizmoCtx->fCaptureScale;
    const float fArrowLength = 0.03f * gptGizmoCtx->fCaptureScale;
    const float fLength = 0.15f * gptGizmoCtx->fCaptureScale;

    if(gptGizmoCtx->tState != PL_GIZMO_STATE_DEFAULT)
    {
        gptDraw->add_3d_sphere_filled(ptGizmoDrawlist, (plDrawSphereDesc){.tCenter = gptGizmoCtx->tBeginPos, .fRadius = fAxisRadius * 2}, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.5f, 0.5f, 0.5f)});
        gptDraw->add_3d_line(ptGizmoDrawlist, gptGizmoCtx->tBeginPos, *ptCenter, (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = fAxisRadius});
        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "offset: %0.3f, %0.3f, %0.3f", ptCenter->x - gptGizmoCtx->tBeginPos.x, ptCenter->y - gptGizmoCtx->tBeginPos.y, ptCenter->z - gptGizmoCtx->tBeginPos.z);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fLength * 1.1f, ptCenter->z}, acTextBuffer,
            (plDrawTextOptions){
                .ptFont = gptUI->get_default_font(),
                .uColor = PL_COLOR_32_YELLOW
            });
    };

    float fXDistanceAlong = 0.0f;
    float fYDistanceAlong = 0.0f;
    float fZDistanceAlong = 0.0f;

    bool bXSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){1.0f, 0.0f, 0.0f}, fArrowRadius, fLength, &fXDistanceAlong);
    bool bYSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 1.0f, 0.0f}, fArrowRadius, fLength, &fYDistanceAlong);
    bool bZSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 0.0f, 1.0f}, fArrowRadius, fLength, &fZDistanceAlong);
    
    plVec3 tYZIntersectionPoint = {0};
    plVec3 tXZIntersectionPoint = {0};
    plVec3 tXYIntersectionPoint = {0};

    bool bYZSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){1.0f, 0.0f, 0.0f, pl_dot_vec3((plVec3){-1.0f, 0.0f, 0.0f}, *ptCenter)}, &tYZIntersectionPoint);
    bool bXZSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){0.0f, 1.0f, 0.0f, pl_dot_vec3((plVec3){0.0f, -1.0f, 0.0f}, *ptCenter)}, &tXZIntersectionPoint);
    bool bXYSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){0.0f, 0.0f, 1.0f, pl_dot_vec3((plVec3){0.0f, 0.0f, -1.0f}, *ptCenter)}, &tXYIntersectionPoint);
    
    bYZSelected = bYZSelected && 
        (tYZIntersectionPoint.y < ptCenter->y + fLength * 0.375f) &&
        (tYZIntersectionPoint.y > ptCenter->y + fLength * 0.125f) &&
        (tYZIntersectionPoint.z > ptCenter->z + fLength * 0.125f) &&
        (tYZIntersectionPoint.z < ptCenter->z + fLength * 0.375f);

    bXZSelected = bXZSelected && 
        (tXZIntersectionPoint.x < ptCenter->x + fLength * 0.375f) &&
        (tXZIntersectionPoint.x > ptCenter->x + fLength * 0.125f) &&
        (tXZIntersectionPoint.z > ptCenter->z + fLength * 0.125f) &&
        (tXZIntersectionPoint.z < ptCenter->z + fLength * 0.375f);

    bXYSelected = bXYSelected && 
        (tXYIntersectionPoint.x < ptCenter->x + fLength * 0.375f) &&
        (tXYIntersectionPoint.x > ptCenter->x + fLength * 0.125f) &&
        (tXYIntersectionPoint.y > ptCenter->y + fLength * 0.125f) &&
        (tXYIntersectionPoint.y < ptCenter->y + fLength * 0.375f);

    if(gptGizmoCtx->tState == PL_GIZMO_STATE_DEFAULT)
    {
        const float apf[6] = {
            bXSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x + fXDistanceAlong, ptCenter->y, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
            bYSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y + fYDistanceAlong, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
            bZSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fZDistanceAlong}, ptCamera->tPos)) : FLT_MAX,
            bYZSelected ? pl_length_vec3(pl_sub_vec3(tYZIntersectionPoint, ptCamera->tPos)) : FLT_MAX,
            bXZSelected ? pl_length_vec3(pl_sub_vec3(tXZIntersectionPoint, ptCamera->tPos)) : FLT_MAX,
            bXYSelected ? pl_length_vec3(pl_sub_vec3(tXYIntersectionPoint, ptCamera->tPos)) : FLT_MAX
        };

        bool bSomethingSelected = bXSelected || bYSelected || bZSelected || bYZSelected || bXZSelected || bXYSelected;

        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bYZSelected = false;
        bXZSelected = false;
        bXYSelected = false;


        bool* apb[6] = {
            &bXSelected,
            &bYSelected,
            &bZSelected,
            &bYZSelected,
            &bXZSelected,
            &bXYSelected
        };

        uint32_t uMinIndex = 0;
        if(bSomethingSelected)
        {
            for(uint32_t i = 0; i < 6; i++)
            {
                if(apf[i] <= apf[uMinIndex])
                {
                    uMinIndex = i;
                    for(uint32_t j = 0; j < 6; j++)
                    {
                        *apb[j] = false;
                    }
                    *apb[uMinIndex] = true;
                }
            }
        }
    }
    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_X_TRANSLATION)
    {
        bXSelected = true;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){fXDistanceAlong - gptGizmoCtx->fOriginalDistX, 0.0f, 0.0f});

        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }
    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Y_TRANSLATION)
    {
        bYSelected = true;
        bXSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, fYDistanceAlong - gptGizmoCtx->fOriginalDistY, 0.0f});
        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }
    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Z_TRANSLATION)
    {
        bZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, 0.0f, fZDistanceAlong - gptGizmoCtx->fOriginalDistZ});
        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }

    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_YZ_TRANSLATION)
    {
        bYZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, tYZIntersectionPoint.y - gptGizmoCtx->tOriginalPos.y - ptCenter->y, tYZIntersectionPoint.z - gptGizmoCtx->tOriginalPos.z - ptCenter->z});
        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }

    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_XZ_TRANSLATION)
    {
        bXZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){tXZIntersectionPoint.x - gptGizmoCtx->tOriginalPos.x - ptCenter->x, 0.0, tXZIntersectionPoint.z - gptGizmoCtx->tOriginalPos.z - ptCenter->z});
        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }

    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_XY_TRANSLATION)
    {
        bXYSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){tXYIntersectionPoint.x - gptGizmoCtx->tOriginalPos.x - ptCenter->x, tXYIntersectionPoint.y - gptGizmoCtx->tOriginalPos.y - ptCenter->y, 0.0f});
        if(ptParentTransform)
        {
            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, *ptCenter, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tTranslation = *ptCenter;
        }
    }

    if(bXSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalDistX = fXDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_X_TRANSLATION;
    }
    else if(bYSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalDistY = fYDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Y_TRANSLATION;
    }
    else if(bZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalDistZ = fZDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Z_TRANSLATION;
    }
    else if(bYZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tOriginalPos = tYZIntersectionPoint;
        gptGizmoCtx->tOriginalPos.y -= ptCenter->y;
        gptGizmoCtx->tOriginalPos.z -= ptCenter->z;
        gptGizmoCtx->tState = PL_GIZMO_STATE_YZ_TRANSLATION;
    }
    else if(bXZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tOriginalPos = tXZIntersectionPoint;
        gptGizmoCtx->tOriginalPos.x -= ptCenter->x;
        gptGizmoCtx->tOriginalPos.z -= ptCenter->z;
        gptGizmoCtx->tState = PL_GIZMO_STATE_XZ_TRANSLATION;
    }
    else if(bXYSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tOriginalPos = tXYIntersectionPoint;
        gptGizmoCtx->tOriginalPos.x -= ptCenter->x;
        gptGizmoCtx->tOriginalPos.y -= ptCenter->y;
        gptGizmoCtx->tState = PL_GIZMO_STATE_XY_TRANSLATION;
    }

    if(gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptGizmoCtx->bActive = false;
        gptGizmoCtx->tBeginPos = *ptCenter;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_DEFAULT;
    }

    plVec4 tXColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    plVec4 tYColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    plVec4 tZColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    plVec4 tYZColor = (plVec4){1.0f, 0.0f, 0.0f, 0.5f};
    plVec4 tXYColor = (plVec4){0.0f, 0.0f, 1.0f, 0.5f};
    plVec4 tXZColor = (plVec4){0.0f, 1.0f, 0.0f, 0.5f};

    if(bXSelected)       tXColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bYSelected)  tYColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bZSelected)  tZColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bYZSelected) tYZColor = (plVec4){1.0f, 1.0f, 0.0f, 0.5f};
    else if(bXZSelected) tXZColor = (plVec4){1.0f, 1.0f, 0.0f, 0.5f};
    else if(bXYSelected) tXYColor = (plVec4){1.0f, 1.0f, 0.0f, 0.5f};

    // x arrow head
    plDrawConeDesc tDrawDesc0 = {0};
    tDrawDesc0.tBasePos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled(ptGizmoDrawlist, tDrawDesc0, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXColor)});

    // y arrow head
    plDrawConeDesc tDrawDesc1 = {0};
    tDrawDesc1.tBasePos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc1.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
    tDrawDesc1.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled(ptGizmoDrawlist, tDrawDesc1, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYColor)});

    // z arrow head
    plDrawConeDesc tDrawDesc2 = {0};
    tDrawDesc2.tBasePos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc2.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
    tDrawDesc2.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled(ptGizmoDrawlist, tDrawDesc2, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tZColor)});

    // x axis
    plDrawCylinderDesc tDrawDesc3 = {0};
    tDrawDesc3.tBasePos = *ptCenter;
    tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc3.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc3, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXColor)});

    // y axis
    plDrawCylinderDesc tDrawDesc4 = {0};
    tDrawDesc4.tBasePos = *ptCenter;
    tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc4.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc4, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYColor)});

    // z axis
    plDrawCylinderDesc tDrawDesc5 = {0};
    tDrawDesc5.tBasePos = *ptCenter;
    tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc5.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc5, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tZColor)});

    // origin
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        *ptCenter,
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_RGB(0.5f, 0.5f, 0.5f)});

    // PLANES
    gptDraw->add_3d_plane_xy_filled(ptGizmoDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y + fLength * 0.25f, ptCenter->z}, fLength * 0.25f, fLength * 0.25f, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXYColor)});
    gptDraw->add_3d_plane_yz_filled(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fLength * 0.25f, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYZColor)});
    gptDraw->add_3d_plane_xz_filled(ptGizmoDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXZColor)}); 
}

static void
pl__gizmo_rotation(plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIOI->get_mouse_pos();

    if(gptUI->wants_mouse_capture())
    {
        tMousePos.x = -1.0f;
        tMousePos.y = -1.0f;
    }

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIOI->get_io()->tMainViewportSize.x, -1.0f + 2.0f * tMousePos.y / gptIOI->get_io()->tMainViewportSize.y, 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

    const float fOuterRadius = 0.15f * gptGizmoCtx->fCaptureScale;
    const float fInnerRadius = fOuterRadius - 0.03f * gptGizmoCtx->fCaptureScale;

    plVec4 tCurrentRot = {0};
    plVec3 tCurrentTrans = {0};
    plVec3 tCurrentScale = {0};
    pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

    plVec3 tXIntersectionPoint = {0};
    plVec3 tYIntersectionPoint = {0};
    plVec3 tZIntersectionPoint = {0};

    bool bXSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){1.0f, 0.0f, 0.0f, pl_dot_vec3((plVec3){-1.0f, 0.0f, 0.0f}, *ptCenter)}, &tXIntersectionPoint);
    bool bYSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){0.0f, 1.0f, 0.0f, pl_dot_vec3((plVec3){0.0f, -1.0f, 0.0f}, *ptCenter)}, &tYIntersectionPoint);
    bool bZSelected = pl__does_line_intersect_plane(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), (plVec4){0.0f, 0.0f, 1.0f, pl_dot_vec3((plVec3){0.0f, 0.0f, -1.0f}, *ptCenter)}, &tZIntersectionPoint);

    plVec4 tXColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    plVec4 tYColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    plVec4 tZColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    if(gptGizmoCtx->tState == PL_GIZMO_STATE_DEFAULT)
    {

        const float fXDistance = pl_length_vec3(pl_sub_vec3(tXIntersectionPoint, *ptCenter));
        const float fYDistance = pl_length_vec3(pl_sub_vec3(tYIntersectionPoint, *ptCenter));
        const float fZDistance = pl_length_vec3(pl_sub_vec3(tZIntersectionPoint, *ptCenter));

        bXSelected = bXSelected && fXDistance < fOuterRadius && fXDistance > fInnerRadius;
        bYSelected = bYSelected && fYDistance < fOuterRadius && fYDistance > fInnerRadius;
        bZSelected = bZSelected && fZDistance < fOuterRadius && fZDistance > fInnerRadius;

        bool bSomethingSelected = bXSelected || bYSelected || bZSelected;
        const float apf[3] = {
            bXSelected ? pl_length_vec3(pl_sub_vec3(tXIntersectionPoint, ptCamera->tPos)) : FLT_MAX,
            bYSelected ? pl_length_vec3(pl_sub_vec3(tYIntersectionPoint, ptCamera->tPos)) : FLT_MAX,
            bZSelected ? pl_length_vec3(pl_sub_vec3(tZIntersectionPoint, ptCamera->tPos)) : FLT_MAX
        };

        bXSelected = false;
        bYSelected = false;
        bZSelected = false;

        bool* apb[3] = {
            &bXSelected,
            &bYSelected,
            &bZSelected
        };

        uint32_t uMinIndex = 0;
        if(bSomethingSelected)
        {
            for(uint32_t i = 0; i < 3; i++)
            {
                if(apf[i] <= apf[uMinIndex])
                {
                    uMinIndex = i;
                    for(uint32_t j = 0; j < 3; j++)
                    {
                        *apb[j] = false;
                    }
                    *apb[uMinIndex] = true;
                }
            }
        }
    }

    if(gptGizmoCtx->tState == PL_GIZMO_STATE_X_ROTATION)
    {
        bXSelected = true;
        bYSelected = false;
        bZSelected = false;

        plVec3 tUpDir = {0.0f, 1.0f, 0.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(gptGizmoCtx->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tXIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(gptGizmoCtx->tOriginalPos.z < ptCenter->z && tXIntersectionPoint.z < ptCenter->z)
            fAngleBetweenVecs *= -1.0f;
        else if(gptGizmoCtx->tOriginalPos.z < ptCenter->z && tXIntersectionPoint.z > ptCenter->z)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(gptGizmoCtx->tOriginalPos.z > ptCenter->z && tXIntersectionPoint.z < ptCenter->z)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "x-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, acTextBuffer,
            (plDrawTextOptions){
                .ptFont = gptUI->get_default_font(),
                .uColor = PL_COLOR_32_YELLOW
            });
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(0.7f, 0.7f, 0.7f, 1.0f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGBA(1.0f, 1.0f, 0.0f, 1.0f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 1.0f, 0.0f, 0.0f), gptGizmoCtx->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 1.0f, 0.0f, 0.0f), gptGizmoCtx->tOriginalRot);
            }
        }
    }
    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Y_ROTATION)
    {
        bYSelected = true;
        bXSelected = false;
        bZSelected = false;

        plVec3 tUpDir = {0.0f, 0.0f, 1.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(gptGizmoCtx->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tYIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(gptGizmoCtx->tOriginalPos.x < ptCenter->x && tYIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs *= -1.0f;
        else if(gptGizmoCtx->tOriginalPos.x < ptCenter->x && tYIntersectionPoint.x > ptCenter->x)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(gptGizmoCtx->tOriginalPos.x > ptCenter->x && tYIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "y-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, acTextBuffer,
            (plDrawTextOptions){
                .ptFont = gptUI->get_default_font(),
                .uColor = PL_COLOR_32_YELLOW
            });
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.7f, 0.7f, 0.7f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 0.0f, 1.0f, 0.0f), gptGizmoCtx->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 0.0f, 1.0f, 0.0f), gptGizmoCtx->tOriginalRot);
            }
        }
    }
    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Z_ROTATION)
    {
        bXSelected = false;
        bYSelected = false;
        bZSelected = true;

        plVec3 tUpDir = {0.0f, 1.0f, 0.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(gptGizmoCtx->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tZIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(gptGizmoCtx->tOriginalPos.x < ptCenter->x && tZIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs *= -1.0f;
        else if(gptGizmoCtx->tOriginalPos.x < ptCenter->x && tZIntersectionPoint.x > ptCenter->x)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(gptGizmoCtx->tOriginalPos.x > ptCenter->x && tZIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "z-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, acTextBuffer,
            (plDrawTextOptions){
                .ptFont = gptUI->get_default_font(),
                .uColor = PL_COLOR_32_YELLOW
            });
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.7f, 0.7f, 0.7f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(1.0f, 1.0f, 0.0f), .fThickness = 0.0035f * gptGizmoCtx->fCaptureScale});

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 0.0f, 0.0f, -1.0f), gptGizmoCtx->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation(fAngleBetweenVecs, 0.0f, 0.0f, -1.0f), gptGizmoCtx->tOriginalRot);
            }
        }
    }

    if(bXSelected)
    {
        tXColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    }
    else if(bYSelected)
    {
        tYColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    }
    else if(bZSelected)
    {
        tZColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    }

    if(bXSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tOriginalPos = tXIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &gptGizmoCtx->tOriginalRot, &tCurrentTrans);
        }
        else
            gptGizmoCtx->tOriginalRot = ptSelectedTransform->tRotation;
        gptGizmoCtx->tState = PL_GIZMO_STATE_X_ROTATION;
    }
    else if(bYSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tOriginalPos = tYIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &gptGizmoCtx->tOriginalRot, &tCurrentTrans);
        }
        else
            gptGizmoCtx->tOriginalRot = ptSelectedTransform->tRotation;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Y_ROTATION;
    }
    else if(bZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->tOriginalPos = tZIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &gptGizmoCtx->tOriginalRot, &tCurrentTrans);
        }
        else
            gptGizmoCtx->tOriginalRot = ptSelectedTransform->tRotation;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Z_ROTATION;
    }

    if(gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptGizmoCtx->bActive = false;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_DEFAULT;
    }

    gptDraw->add_3d_band_yz_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, 36, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXColor)});
    gptDraw->add_3d_band_xz_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, 36, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYColor)});
    gptDraw->add_3d_band_xy_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, 36, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tZColor)});
}

static void
pl__gizmo_scale(plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIOI->get_mouse_pos();

    if(gptUI->wants_mouse_capture())
    {
        tMousePos.x = -1.0f;
        tMousePos.y = -1.0f;
    }

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIOI->get_io()->tMainViewportSize.x, -1.0f + 2.0f * tMousePos.y / gptIOI->get_io()->tMainViewportSize.y, 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

    const float fAxisRadius  = 0.0035f * gptGizmoCtx->fCaptureScale;
    const float fArrowRadius = 0.0075f * gptGizmoCtx->fCaptureScale;
    const float fLength = 0.15f * gptGizmoCtx->fCaptureScale;

    float fXDistanceAlong = 0.0f;
    float fYDistanceAlong = 0.0f;
    float fZDistanceAlong = 0.0f;
    plVec3 tXYZIntersectionPoint = {0};

    bool bXSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){1.0f, 0.0f, 0.0f}, fArrowRadius, fLength, &fXDistanceAlong);
    bool bYSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 1.0f, 0.0f}, fArrowRadius, fLength, &fYDistanceAlong);
    bool bZSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 0.0f, 1.0f}, fArrowRadius, fLength, &fZDistanceAlong);
    
    plVec3 tCameraDir = pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos));
    bool bXYZSelected = pl__does_line_intersect_plane(ptCamera->tPos, tCameraDir, (plVec4){-tCameraDir.x, -tCameraDir.y, -tCameraDir.z, pl_dot_vec3(tCameraDir, *ptCenter)}, &tXYZIntersectionPoint);


    if(gptGizmoCtx->tState != PL_GIZMO_STATE_DEFAULT)
    {
        float fScaleX = gptGizmoCtx->tState == PL_GIZMO_STATE_X_SCALE ? gptGizmoCtx->fOriginalScaleX + (fXDistanceAlong - gptGizmoCtx->fOriginalDistX) * gptGizmoCtx->fOriginalScaleX : gptGizmoCtx->fOriginalScaleX;
        float fScaleY = gptGizmoCtx->tState == PL_GIZMO_STATE_Y_SCALE ? gptGizmoCtx->fOriginalScaleY + (fYDistanceAlong - gptGizmoCtx->fOriginalDistY) * gptGizmoCtx->fOriginalScaleY : gptGizmoCtx->fOriginalScaleY;
        float fScaleZ = gptGizmoCtx->tState == PL_GIZMO_STATE_Z_SCALE ? gptGizmoCtx->fOriginalScaleZ + (fZDistanceAlong - gptGizmoCtx->fOriginalDistZ) * gptGizmoCtx->fOriginalScaleZ : gptGizmoCtx->fOriginalScaleZ;
        if(gptGizmoCtx->tState == PL_GIZMO_STATE_SCALE)
        {
            float fScale = gptGizmoCtx->fOriginalScaleX + gptGizmoCtx->fOriginalScaleX * pl_length_vec3((plVec3){fXDistanceAlong - gptGizmoCtx->fOriginalDistX, fYDistanceAlong - gptGizmoCtx->fOriginalDistY, fZDistanceAlong - gptGizmoCtx->fOriginalDistZ});
            fScaleX = fScale;
            fScaleY = fScale;
            fScaleZ = fScale;
        }
        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "scaling: %0.3f, %0.3f, %0.3f", fScaleX, fScaleY, fScaleZ);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fLength * 1.2f, ptCenter->z}, acTextBuffer,
            (plDrawTextOptions){
                .ptFont = gptUI->get_default_font(),
                .uColor = PL_COLOR_32_YELLOW
            });
    }

    if(gptGizmoCtx->tState == PL_GIZMO_STATE_DEFAULT)
    {

        const float fXYZDistance = pl_length_vec3(pl_sub_vec3(tXYZIntersectionPoint, *ptCenter));
        bXYZSelected = bXYZSelected && fXYZDistance < fAxisRadius * 4;

        const float apf[4] = {
            bXSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x + fXDistanceAlong, ptCenter->y, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
            bYSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y + fYDistanceAlong, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
            bZSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fZDistanceAlong}, ptCamera->tPos)) : FLT_MAX,
            bXYZSelected ? pl_length_vec3(pl_sub_vec3(*ptCenter, ptCamera->tPos)) - fAxisRadius * 4: FLT_MAX
        };

        bool bSomethingSelected = bXSelected || bYSelected || bZSelected | bXYZSelected;

        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXYZSelected = false;


        bool* apb[4] = {
            &bXSelected,
            &bYSelected,
            &bZSelected,
            &bXYZSelected
        };

        uint32_t uMinIndex = 0;
        if(bSomethingSelected)
        {
            for(uint32_t i = 0; i < 4; i++)
            {
                if(apf[i] <= apf[uMinIndex])
                {
                    uMinIndex = i;
                    for(uint32_t j = 0; j < 4; j++)
                    {
                        *apb[j] = false;
                    }
                    *apb[uMinIndex] = true;
                }
            }
        }
    }

    if(gptGizmoCtx->tState == PL_GIZMO_STATE_X_SCALE)
    {
        bXSelected = true;
        bYSelected = false;
        bZSelected = false;
        bXYZSelected = false;

        if(ptParentTransform)
        {

            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            tCurrentScale.x = gptGizmoCtx->fOriginalScaleX + (fXDistanceAlong - gptGizmoCtx->fOriginalDistX) * gptGizmoCtx->fOriginalScaleX;

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tScale.x = gptGizmoCtx->fOriginalScaleX + (fXDistanceAlong - gptGizmoCtx->fOriginalDistX) * gptGizmoCtx->fOriginalScaleX;
        }
    }


    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Y_SCALE)
    {
        bXSelected = false;
        bYSelected = true;
        bZSelected = false;
        bXYZSelected = false;

        if(ptParentTransform)
        {

            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);


            tCurrentScale.y = gptGizmoCtx->fOriginalScaleY + (fYDistanceAlong - gptGizmoCtx->fOriginalDistY) * gptGizmoCtx->fOriginalScaleY;

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tScale.y = gptGizmoCtx->fOriginalScaleY + (fYDistanceAlong - gptGizmoCtx->fOriginalDistY) * gptGizmoCtx->fOriginalScaleY;
        }
    }

    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_Z_SCALE)
    {
        bXSelected = false;
        bYSelected = false;
        bZSelected = true;
        bXYZSelected = false;

        if(ptParentTransform)
        {

            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);


            tCurrentScale.z = gptGizmoCtx->fOriginalScaleZ + (fZDistanceAlong - gptGizmoCtx->fOriginalDistZ) * gptGizmoCtx->fOriginalScaleZ;

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            ptSelectedTransform->tScale.z = gptGizmoCtx->fOriginalScaleZ + (fZDistanceAlong - gptGizmoCtx->fOriginalDistZ) * gptGizmoCtx->fOriginalScaleZ;
        }
    }

    else if(gptGizmoCtx->tState == PL_GIZMO_STATE_SCALE)
    {
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXYZSelected = true;

        if(ptParentTransform)
        {

            plVec4 tCurrentRot = {0};
            plVec3 tCurrentTrans = {0};
            plVec3 tCurrentScale = {0};
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

            if(fYDistanceAlong - gptGizmoCtx->fOriginalDistY > 0)
                tCurrentScale.x = gptGizmoCtx->fOriginalScaleX + pl_length_vec3(pl_sub_vec3(tXYZIntersectionPoint, gptGizmoCtx->tOriginalPos)) * gptGizmoCtx->fOriginalScaleX;
            else
                tCurrentScale.x = gptGizmoCtx->fOriginalScaleX - pl_length_vec3(pl_sub_vec3(tXYZIntersectionPoint, gptGizmoCtx->tOriginalPos)) * gptGizmoCtx->fOriginalScaleX;
            tCurrentScale.y = tCurrentScale.x;
            tCurrentScale.z = tCurrentScale.x;

            plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

            plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
            plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
            pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
        }
        else
        {
            if(fYDistanceAlong - gptGizmoCtx->fOriginalDistY > 0)
                ptSelectedTransform->tScale.x = gptGizmoCtx->fOriginalScaleX + pl_length_vec3(pl_sub_vec3(tXYZIntersectionPoint, gptGizmoCtx->tOriginalPos)) * gptGizmoCtx->fOriginalScaleX;
            else
                ptSelectedTransform->tScale.x = gptGizmoCtx->fOriginalScaleX - pl_length_vec3(pl_sub_vec3(tXYZIntersectionPoint, gptGizmoCtx->tOriginalPos)) * gptGizmoCtx->fOriginalScaleX;
            ptSelectedTransform->tScale.y = ptSelectedTransform->tScale.x;
            ptSelectedTransform->tScale.z = ptSelectedTransform->tScale.x;


        }
    }

    plVec4 tCurrentRot = {0};
    plVec3 tCurrentTrans = {0};
    plVec3 tCurrentScale = ptSelectedTransform->tScale;
    // pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);


    if(bXSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalScaleX = tCurrentScale.x;
        gptGizmoCtx->fOriginalScaleY = tCurrentScale.y;
        gptGizmoCtx->fOriginalScaleZ = tCurrentScale.z;
        gptGizmoCtx->fOriginalDistX = fXDistanceAlong;
        gptGizmoCtx->fOriginalDistY = fYDistanceAlong;
        gptGizmoCtx->fOriginalDistZ = fZDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_X_SCALE;
    }
    else if(bYSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalScaleX = tCurrentScale.x;
        gptGizmoCtx->fOriginalScaleY = tCurrentScale.y;
        gptGizmoCtx->fOriginalScaleZ = tCurrentScale.z;
        gptGizmoCtx->fOriginalDistX = fXDistanceAlong;
        gptGizmoCtx->fOriginalDistY = fYDistanceAlong;
        gptGizmoCtx->fOriginalDistZ = fZDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Y_SCALE;
    }
    else if(bZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalScaleX = tCurrentScale.x;
        gptGizmoCtx->fOriginalScaleY = tCurrentScale.y;
        gptGizmoCtx->fOriginalScaleZ = tCurrentScale.z;
        gptGizmoCtx->fOriginalDistX = fXDistanceAlong;
        gptGizmoCtx->fOriginalDistY = fYDistanceAlong;
        gptGizmoCtx->fOriginalDistZ = fZDistanceAlong;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_Z_SCALE;
    }
    else if(bXYZSelected && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        gptGizmoCtx->bActive = true;
        gptGizmoCtx->fOriginalScaleX = tCurrentScale.x;
        gptGizmoCtx->fOriginalScaleY = tCurrentScale.y;
        gptGizmoCtx->fOriginalScaleZ = tCurrentScale.z;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_SCALE;
    }

    if(gptIOI->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptGizmoCtx->bActive = false;
        gptGizmoCtx->tOriginalPos = *ptCenter;
        gptGizmoCtx->tState = PL_GIZMO_STATE_DEFAULT;
    }

    plVec4 tXColor   = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
    plVec4 tYColor   = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
    plVec4 tZColor   = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};
    plVec4 tXYZColor = (plVec4){0.5f, 0.5f, 0.5f, 1.0f};

    if(bXSelected)        tXColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bYSelected)   tYColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bZSelected)   tZColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
    else if(bXYZSelected) tXYZColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};

    // x axis
    plDrawCylinderDesc tDrawDesc3 = {0};
    tDrawDesc3.tBasePos = *ptCenter;
    tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
    tDrawDesc3.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc3, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXColor)});

    // y axis
    plDrawCylinderDesc tDrawDesc4 = {0};
    tDrawDesc4.tBasePos = *ptCenter;
    tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
    tDrawDesc4.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc4, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYColor)});

    // z axis
    plDrawCylinderDesc tDrawDesc5 = {0};
    tDrawDesc5.tBasePos = *ptCenter;
    tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
    tDrawDesc5.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled(ptGizmoDrawlist, tDrawDesc5, (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tZColor)});

    // x end
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXColor)});

    // y end
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tYColor)});

    // z end
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength},
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tZColor)});

    // origin
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        *ptCenter,
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plDrawSolidOptions){.uColor = PL_COLOR_32_VEC4(tXYZColor)});
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_gizmo_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGizmoI tApi = {
        .set_mode  = pl_gizmo_set_mode,
        .next_mode = pl_gizmo_next_mode,
        .active    = pl_gizmo_active,
        .gizmo     = pl_gizmo
    };
    pl_set_api(ptApiRegistry, plGizmoI, &tApi);

    gptIOI    = pl_get_api_latest(ptApiRegistry, plIOI);
    gptUI     = pl_get_api_latest(ptApiRegistry, plUiI);
    gptDraw   = pl_get_api_latest(ptApiRegistry, plDrawI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    if(bReload)
    {
        gptGizmoCtx = ptDataRegistry->get_data("plGizmoContext");
    }
    else
    {
        static plGizmoContext gtGizmoCtx = {
            .tSelectionMode = PL_GIZMO_MODE_TRANSLATION
        };
        gptGizmoCtx = &gtGizmoCtx;
        ptDataRegistry->set_data("plGizmoContext", gptGizmoCtx);
    }
}

PL_EXPORT void
pl_unload_gizmo_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    if(bReload)
        return;
        
    const plGizmoI* ptApi = pl_get_api_latest(ptApiRegistry, plGizmoI);
    ptApiRegistry->remove_api(ptApi);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#ifndef PL_UNITY_BUILD
    #ifdef PL_USE_STB_SPRINTF
        #define STB_SPRINTF_IMPLEMENTATION
        #include "stb_sprintf.h"
        #undef STB_SPRINTF_IMPLEMENTATION
    #endif
#endif