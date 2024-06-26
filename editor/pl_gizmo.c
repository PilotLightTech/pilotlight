/*
   pl_gizmo.c
       - TODO: lots of optimizing and cleanup
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] internal structs & enums
// [SECTION] internal api declarations
// [SECTION] public api implementation
// [SECTION] internal api implementation
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_editor.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs & enums
//-----------------------------------------------------------------------------

typedef enum _plSelectionMode
{
    PL_SELECTION_MODE_NONE,
    PL_SELECTION_MODE_TRANSLATION,
    PL_SELECTION_MODE_ROTATION,
    PL_SELECTION_MODE_SCALE,

    PL_SELECTION_MODE_COUNT,
} plSelectionMode;

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
} plGizmoState;

typedef struct _plGizmoData
{
    plSelectionMode tSelectionMode;
    plGizmoState    tState;
    float           fCaptureScale;
    plVec3          tBeginPos;
    plVec3          tOriginalPos;
    float           fOriginalDist;
    float           fOriginalScale;
    plVec4          tOriginalRot;
} plGizmoData;

//-----------------------------------------------------------------------------
// [SECTION] internal api declarations
//-----------------------------------------------------------------------------

static bool pl__does_line_intersect_cylinder(plVec3 tP0, plVec3 tV0, plVec3 tP1, plVec3 tV1, float fRadius, float fHeight, float* pfDistance);
static bool pl__does_line_intersect_plane   (plVec3 tP0, plVec3 tV0, plVec4 tPlane, plVec3* ptQ);
static void pl__gizmo_translation(plGizmoData*, plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);
static void pl__gizmo_rotation(plGizmoData*, plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);
static void pl__gizmo_scale(plGizmoData*, plDrawList3D*, plCameraComponent*, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform);


//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

plGizmoData*
pl_initialize_gizmo_data(void)
{
    static plGizmoData tData = {
        .tSelectionMode = PL_SELECTION_MODE_TRANSLATION
    };
    return &tData;
}

void
pl_change_gizmo_mode(plGizmoData* ptGizmoData)
{
    ptGizmoData->tSelectionMode = (ptGizmoData->tSelectionMode + 1) % PL_SELECTION_MODE_COUNT;
}

void
pl_gizmo(plGizmoData* ptGizmoData, plDrawList3D* ptGizmoDrawlist, plComponentLibrary* ptMainComponentLibrary, plCameraComponent* ptCamera, plEntity tSelectedEntity)
{

    plObjectComponent* ptSelectedObject = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tSelectedEntity);
    plTransformComponent* ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tSelectedEntity);
    plTransformComponent* ptParentTransform = NULL;
    plHierarchyComponent* ptHierarchyComp = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_HIERARCHY, tSelectedEntity);
    if(ptHierarchyComp)
    {
        ptParentTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptHierarchyComp->tParent);
    }
    if(ptSelectedObject)
        ptSelectedTransform = gptEcs->get_component(ptMainComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptSelectedObject->tTransform);
    if(ptSelectedTransform)
    {
        plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;
        if(ptGizmoData->tState == PL_GIZMO_STATE_DEFAULT)
        {
            ptGizmoData->fCaptureScale = pl_length_vec3(pl_sub_vec3(*ptCenter, ptCamera->tPos));
        }

        switch(ptGizmoData->tSelectionMode)
        {
            case PL_SELECTION_MODE_TRANSLATION:
                pl__gizmo_translation(ptGizmoData, ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                break;
            case PL_SELECTION_MODE_ROTATION:
                pl__gizmo_rotation(ptGizmoData, ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                break;
            case PL_SELECTION_MODE_SCALE:
                pl__gizmo_scale(ptGizmoData, ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform);
                break;

        }
    }
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementation
//-----------------------------------------------------------------------------

// static float
// pl__distance_line_line(plVec3 tP0, plVec3 tV0, plVec3 tP1, plVec3 tV1)
// {
//     plVec3 tDp = pl_sub_vec3(tP1, tP0);

//     float fV12 = pl_dot_vec3(tV0, tV0);
//     float fV22 = pl_dot_vec3(tV1, tV1);
//     float fV1V2 = pl_dot_vec3(tV0, tV1);

//     float fDet = fV1V2 * fV1V2 - fV12 * fV22;

//     if(fabsf(fDet) > FLT_MIN)
//     {
//         fDet = 1.0f / fDet;
//         float fDpv1 = pl_dot_vec3(tDp, tV0);
//         float fDpv2 = pl_dot_vec3(tDp, tV1);

//         float fT1 = (fV1V2 * fDpv2 - fV22 * fDpv1) * fDet;
//         float fT2 = (fV12 * fDpv2 - fV1V2 * fDpv1) * fDet;

//         return pl_length_vec3(pl_add_vec3(tDp, pl_sub_vec3(pl_mul_vec3_scalarf(tV1, fT2), pl_mul_vec3_scalarf(tV0, fT1))));
//     }

//     // lines are nearly parallel
//     plVec3 tA = pl_cross_vec3(tDp, tV1);
//     return sqrtf(pl_dot_vec3(tA, tA) / fV12);
// }

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
pl__gizmo_translation(plGizmoData* ptGizmoData, plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIO->get_mouse_pos();

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIO->get_io()->afMainViewportSize[0], -1.0f + 2.0f * tMousePos.y / gptIO->get_io()->afMainViewportSize[1], 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

    const float fAxisRadius  = 0.0035f * ptGizmoData->fCaptureScale;
    const float fArrowRadius = 0.0075f * ptGizmoData->fCaptureScale;
    const float fArrowLength = 0.03f * ptGizmoData->fCaptureScale;
    const float fLength = 0.15f * ptGizmoData->fCaptureScale;

    if(ptGizmoData->tState != PL_GIZMO_STATE_DEFAULT)
    {
        gptDraw->add_3d_sphere_filled(ptGizmoDrawlist, ptGizmoData->tBeginPos, fAxisRadius * 2, (plVec4){0.5f, 0.5f, 0.5f, 0.5f});
        gptDraw->add_3d_line(ptGizmoDrawlist, ptGizmoData->tBeginPos, *ptCenter, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, fAxisRadius);
        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "offset: %0.3f, %0.3f, %0.3f", ptCenter->x - ptGizmoData->tBeginPos.x, ptCenter->y - ptGizmoData->tBeginPos.y, ptCenter->z - ptGizmoData->tBeginPos.z);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plFontHandle){0}, 13.0f, (plVec3){ptCenter->x, ptCenter->y + fLength * 1.1f, ptCenter->z}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, acTextBuffer, 0.0f);
    }

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

    if(ptGizmoData->tState == PL_GIZMO_STATE_DEFAULT)
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
    else if(ptGizmoData->tState == PL_GIZMO_STATE_X_TRANSLATION)
    {
        bXSelected = true;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){fXDistanceAlong - ptGizmoData->fOriginalDist, 0.0f, 0.0f});

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
    else if(ptGizmoData->tState == PL_GIZMO_STATE_Y_TRANSLATION)
    {
        bYSelected = true;
        bXSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, fYDistanceAlong - ptGizmoData->fOriginalDist, 0.0f});
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
    else if(ptGizmoData->tState == PL_GIZMO_STATE_Z_TRANSLATION)
    {
        bZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, 0.0f, fZDistanceAlong - ptGizmoData->fOriginalDist});
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

    else if(ptGizmoData->tState == PL_GIZMO_STATE_YZ_TRANSLATION)
    {
        bYZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bXYSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){0.0f, tYZIntersectionPoint.y - ptGizmoData->tOriginalPos.y - ptCenter->y, tYZIntersectionPoint.z - ptGizmoData->tOriginalPos.z - ptCenter->z});
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

    else if(ptGizmoData->tState == PL_GIZMO_STATE_XZ_TRANSLATION)
    {
        bXZSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXYSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){tXZIntersectionPoint.x - ptGizmoData->tOriginalPos.x - ptCenter->x, 0.0, tXZIntersectionPoint.z - ptGizmoData->tOriginalPos.z - ptCenter->z});
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

    else if(ptGizmoData->tState == PL_GIZMO_STATE_XY_TRANSLATION)
    {
        bXYSelected = true;
        bXSelected = false;
        bYSelected = false;
        bZSelected = false;
        bXZSelected = false;
        bYZSelected = false;
        *ptCenter = pl_add_vec3(*ptCenter, (plVec3){tXYIntersectionPoint.x - ptGizmoData->tOriginalPos.x - ptCenter->x, tXYIntersectionPoint.y - ptGizmoData->tOriginalPos.y - ptCenter->y, 0.0f});
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

    if(bXSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->fOriginalDist = fXDistanceAlong;
        ptGizmoData->tOriginalPos = *ptCenter;
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tState = PL_GIZMO_STATE_X_TRANSLATION;
    }
    else if(bYSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->fOriginalDist = fYDistanceAlong;
        ptGizmoData->tOriginalPos = *ptCenter;
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tState = PL_GIZMO_STATE_Y_TRANSLATION;
    }
    else if(bZSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->fOriginalDist = fZDistanceAlong;
        ptGizmoData->tOriginalPos = *ptCenter;
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tState = PL_GIZMO_STATE_Z_TRANSLATION;
    }
    else if(bYZSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tOriginalPos = tYZIntersectionPoint;
        ptGizmoData->tOriginalPos.y -= ptCenter->y;
        ptGizmoData->tOriginalPos.z -= ptCenter->z;
        ptGizmoData->tState = PL_GIZMO_STATE_YZ_TRANSLATION;
    }
    else if(bXZSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tOriginalPos = tXZIntersectionPoint;
        ptGizmoData->tOriginalPos.x -= ptCenter->x;
        ptGizmoData->tOriginalPos.z -= ptCenter->z;
        ptGizmoData->tState = PL_GIZMO_STATE_XZ_TRANSLATION;
    }
    else if(bXYSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tOriginalPos = tXYIntersectionPoint;
        ptGizmoData->tOriginalPos.x -= ptCenter->x;
        ptGizmoData->tOriginalPos.y -= ptCenter->y;
        ptGizmoData->tState = PL_GIZMO_STATE_XY_TRANSLATION;
    }

    if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        ptGizmoData->tBeginPos = *ptCenter;
        ptGizmoData->tOriginalPos = *ptCenter;
        ptGizmoData->tState = PL_GIZMO_STATE_DEFAULT;
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
    gptDraw->fill_cone_desc_default(&tDrawDesc0);
    tDrawDesc0.tColor = tXColor;
    tDrawDesc0.tBasePos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
    tDrawDesc0.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptGizmoDrawlist, &tDrawDesc0);

    // y arrow head
    plDrawConeDesc tDrawDesc1 = {0};
    gptDraw->fill_cone_desc_default(&tDrawDesc1);
    tDrawDesc1.tColor = tYColor;
    tDrawDesc1.tBasePos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc1.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
    tDrawDesc1.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptGizmoDrawlist, &tDrawDesc1);

    // z arrow head
    plDrawConeDesc tDrawDesc2 = {0};
    gptDraw->fill_cone_desc_default(&tDrawDesc2);
    tDrawDesc2.tColor = tZColor;
    tDrawDesc2.tBasePos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc2.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
    tDrawDesc2.fRadius = fArrowRadius;
    gptDraw->add_3d_cone_filled_ex(ptGizmoDrawlist, &tDrawDesc2);

    // x axis
    plDrawCylinderDesc tDrawDesc3 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc3);
    tDrawDesc3.tColor = tXColor;
    tDrawDesc3.tBasePos = *ptCenter;
    tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength - fArrowLength, ptCenter->y, ptCenter->z};
    tDrawDesc3.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc3);

    // y axis
    plDrawCylinderDesc tDrawDesc4 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc4);
    tDrawDesc4.tColor = tYColor;
    tDrawDesc4.tBasePos = *ptCenter;
    tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength - fArrowLength, ptCenter->z};
    tDrawDesc4.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc4);

    // z axis
    plDrawCylinderDesc tDrawDesc5 = {0};
    gptDraw->fill_cylinder_desc_default(&tDrawDesc5);
    tDrawDesc5.tColor = tZColor;
    tDrawDesc5.tBasePos = *ptCenter;
    tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength - fArrowLength};
    tDrawDesc5.fRadius = fAxisRadius;
    gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc5);

    // origin
    gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
        *ptCenter,
        fAxisRadius * 4,
        fAxisRadius * 4,
        fAxisRadius * 4,
        (plVec4){0.5f, 0.5f, 0.5f, 1.0f});

    // PLANES
    gptDraw->add_3d_plane_xy_filled(ptGizmoDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y + fLength * 0.25f, ptCenter->z}, fLength * 0.25f, fLength * 0.25f, tXYColor);
    gptDraw->add_3d_plane_yz_filled(ptGizmoDrawlist, (plVec3){ptCenter->x, ptCenter->y + fLength * 0.25f, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, tYZColor);
    gptDraw->add_3d_plane_xz_filled(ptGizmoDrawlist, (plVec3){ptCenter->x + fLength * 0.25f, ptCenter->y, ptCenter->z + fLength * 0.25f}, fLength * 0.25f, fLength * 0.25f, tXZColor); 
}

static void
pl__gizmo_rotation(plGizmoData* ptGizmoData, plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIO->get_mouse_pos();

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIO->get_io()->afMainViewportSize[0], -1.0f + 2.0f * tMousePos.y / gptIO->get_io()->afMainViewportSize[1], 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

    const float fOuterRadius = 0.15f * ptGizmoData->fCaptureScale;
    const float fInnerRadius = fOuterRadius - 0.03f * ptGizmoData->fCaptureScale;

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
    if(ptGizmoData->tState == PL_GIZMO_STATE_DEFAULT)
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

    if(ptGizmoData->tState == PL_GIZMO_STATE_X_ROTATION)
    {
        bXSelected = true;
        bYSelected = false;
        bZSelected = false;

        plVec3 tUpDir = {0.0f, 1.0f, 0.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(ptGizmoData->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tXIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(ptGizmoData->tOriginalPos.z < ptCenter->z && tXIntersectionPoint.z < ptCenter->z)
            fAngleBetweenVecs *= -1.0f;
        else if(ptGizmoData->tOriginalPos.z < ptCenter->z && tXIntersectionPoint.z > ptCenter->z)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(ptGizmoData->tOriginalPos.z > ptCenter->z && tXIntersectionPoint.z < ptCenter->z)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "x-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plFontHandle){0}, 13.0f, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, acTextBuffer, 0.0f);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plVec4){0.7f, 0.7f, 0.7f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 1.0f, 0.0f, 0.0f), ptGizmoData->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 1.0f, 0.0f, 0.0f), ptGizmoData->tOriginalRot);
            }
        }
    }
    else if(ptGizmoData->tState == PL_GIZMO_STATE_Y_ROTATION)
    {
        bYSelected = true;
        bXSelected = false;
        bZSelected = false;

        plVec3 tUpDir = {0.0f, 0.0f, 1.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(ptGizmoData->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tYIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(ptGizmoData->tOriginalPos.x < ptCenter->x && tYIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs *= -1.0f;
        else if(ptGizmoData->tOriginalPos.x < ptCenter->x && tYIntersectionPoint.x > ptCenter->x)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(ptGizmoData->tOriginalPos.x > ptCenter->x && tYIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "y-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plFontHandle){0}, 13.0f, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, acTextBuffer, 0.0f);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plVec4){0.7f, 0.7f, 0.7f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 0.0f, 1.0f, 0.0f), ptGizmoData->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 0.0f, 1.0f, 0.0f), ptGizmoData->tOriginalRot);
            }
        }
    }
    else if(ptGizmoData->tState == PL_GIZMO_STATE_Z_ROTATION)
    {
        bXSelected = false;
        bYSelected = false;
        bZSelected = true;

        plVec3 tUpDir = {0.0f, 1.0f, 0.0f};
        plVec3 tDir0 = pl_norm_vec3(pl_sub_vec3(ptGizmoData->tOriginalPos, *ptCenter));
        plVec3 tDir1 = pl_norm_vec3(pl_sub_vec3(tZIntersectionPoint, *ptCenter));
        const float fAngleBetweenVec0 = acosf(pl_dot_vec3(tDir0, tUpDir));
        const float fAngleBetweenVec1 = acosf(pl_dot_vec3(tDir1, tUpDir));
        float fAngleBetweenVecs = fAngleBetweenVec1 - fAngleBetweenVec0;

        if(ptGizmoData->tOriginalPos.x < ptCenter->x && tZIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs *= -1.0f;
        else if(ptGizmoData->tOriginalPos.x < ptCenter->x && tZIntersectionPoint.x > ptCenter->x)
            fAngleBetweenVecs = fAngleBetweenVec1 + fAngleBetweenVec0;
        else if(ptGizmoData->tOriginalPos.x > ptCenter->x && tZIntersectionPoint.x < ptCenter->x)
            fAngleBetweenVecs = -(fAngleBetweenVec1 + fAngleBetweenVec0);

        char acTextBuffer[256] = {0};
        pl_sprintf(acTextBuffer, "z-axis rotation: %0.0f degrees", fAngleBetweenVecs * 180.0f / PL_PI);
        gptDraw->add_3d_text(ptGizmoDrawlist, (plFontHandle){0}, 13.0f, (plVec3){ptCenter->x, ptCenter->y + fOuterRadius * 1.1f, ptCenter->z}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, acTextBuffer, 0.0f);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir0, fInnerRadius)), (plVec4){0.7f, 0.7f, 0.7f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);
        gptDraw->add_3d_line(ptGizmoDrawlist, *ptCenter, pl_add_vec3(*ptCenter, pl_mul_vec3_scalarf(tDir1, fInnerRadius)), (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.0035f * ptGizmoData->fCaptureScale);

        if(fAngleBetweenVecs != 0.0f)
        {
            if(ptParentTransform)
            {
                tCurrentRot = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 0.0f, 0.0f, -1.0f), ptGizmoData->tOriginalRot);

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tRotation = pl_mul_quat(pl_quat_rotation_normal(fAngleBetweenVecs, 0.0f, 0.0f, -1.0f), ptGizmoData->tOriginalRot);
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

    if(bXSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tOriginalPos = tXIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &ptGizmoData->tOriginalRot, &tCurrentTrans);
        }
        else
            ptGizmoData->tOriginalRot = ptSelectedTransform->tRotation;
        ptGizmoData->tState = PL_GIZMO_STATE_X_ROTATION;
    }
    else if(bYSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tOriginalPos = tYIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &ptGizmoData->tOriginalRot, &tCurrentTrans);
        }
        else
            ptGizmoData->tOriginalRot = ptSelectedTransform->tRotation;
        ptGizmoData->tState = PL_GIZMO_STATE_Y_ROTATION;
    }
    else if(bZSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
    {
        ptGizmoData->tOriginalPos = tZIntersectionPoint;
        if(ptParentTransform)
        {
            pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &ptGizmoData->tOriginalRot, &tCurrentTrans);
        }
        else
            ptGizmoData->tOriginalRot = ptSelectedTransform->tRotation;
        ptGizmoData->tState = PL_GIZMO_STATE_Z_ROTATION;
    }

    if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        ptGizmoData->tOriginalPos = *ptCenter;
        ptGizmoData->tState = PL_GIZMO_STATE_DEFAULT;

    }

    gptDraw->add_3d_band_yz_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, tXColor, 36);
    gptDraw->add_3d_band_xz_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, tYColor, 36);
    gptDraw->add_3d_band_xy_filled(ptGizmoDrawlist, *ptCenter, fInnerRadius, fOuterRadius, tZColor, 36);
}

static void
pl__gizmo_scale(plGizmoData* ptGizmoData, plDrawList3D* ptGizmoDrawlist, plCameraComponent* ptCamera, plTransformComponent* ptSelectedTransform, plTransformComponent* ptParentTransform)
{

    plVec3* ptCenter = &ptSelectedTransform->tWorld.col[3].xyz;

    plVec2 tMousePos = gptIO->get_mouse_pos();

    plMat4 tTransform = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    tTransform = pl_mat4_invert(&tTransform);

    plVec4 tNDC = {-1.0f + 2.0f * tMousePos.x / gptIO->get_io()->afMainViewportSize[0], -1.0f + 2.0f * tMousePos.y / gptIO->get_io()->afMainViewportSize[1], 1.0f, 1.0f};
    tNDC = pl_mul_mat4_vec4(&tTransform, tNDC);
    tNDC = pl_div_vec4_scalarf(tNDC, tNDC.w);

        const float fAxisRadius  = 0.0035f * ptGizmoData->fCaptureScale;
        const float fArrowRadius = 0.0075f * ptGizmoData->fCaptureScale;
        const float fLength = 0.15f * ptGizmoData->fCaptureScale;

        plVec4 tCurrentRot = {0};
        plVec3 tCurrentTrans = {0};
        plVec3 tCurrentScale = {0};
        pl_decompose_matrix(&ptSelectedTransform->tWorld, &tCurrentScale, &tCurrentRot, &tCurrentTrans);

        if(ptGizmoData->tState != PL_GIZMO_STATE_DEFAULT)
        {
            char acTextBuffer[256] = {0};
            pl_sprintf(acTextBuffer, "scaling: %0.3f, %0.3f, %0.3f", tCurrentScale.x, tCurrentScale.y, tCurrentScale.z);
            gptDraw->add_3d_text(ptGizmoDrawlist, (plFontHandle){0}, 13.0f, (plVec3){ptCenter->x, ptCenter->y + fLength * 1.2f, ptCenter->z}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, acTextBuffer, 0.0f);
        }

        float fXDistanceAlong = 0.0f;
        float fYDistanceAlong = 0.0f;
        float fZDistanceAlong = 0.0f;

        bool bXSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){1.0f, 0.0f, 0.0f}, fArrowRadius, fLength, &fXDistanceAlong);
        bool bYSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 1.0f, 0.0f}, fArrowRadius, fLength, &fYDistanceAlong);
        bool bZSelected = pl__does_line_intersect_cylinder(ptCamera->tPos, pl_norm_vec3(pl_sub_vec3(tNDC.xyz, ptCamera->tPos)), *ptCenter, (plVec3){0.0f, 0.0f, 1.0f}, fArrowRadius, fLength, &fZDistanceAlong);
        
        if(ptGizmoData->tState == PL_GIZMO_STATE_DEFAULT)
        {
            const float apf[3] = {
                bXSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x + fXDistanceAlong, ptCenter->y, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
                bYSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y + fYDistanceAlong, ptCenter->z}, ptCamera->tPos)) : FLT_MAX,
                bZSelected ? pl_length_vec3(pl_sub_vec3((plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fZDistanceAlong}, ptCamera->tPos)) : FLT_MAX
            };

            bool bSomethingSelected = bXSelected || bYSelected || bZSelected;

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

        if(ptGizmoData->tState == PL_GIZMO_STATE_X_SCALE)
        {
            bXSelected = true;
            bYSelected = false;
            bZSelected = false;

            if(ptParentTransform)
            {
                tCurrentScale.x = (1.0f + fXDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tScale.x = (1.0f + fXDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;
            }
        }


        else if(ptGizmoData->tState == PL_GIZMO_STATE_Y_SCALE)
        {
            bXSelected = false;
            bYSelected = true;
            bZSelected = false;

            if(ptParentTransform)
            {
                tCurrentScale.y = (1.0f + fYDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tScale.y = (1.0f + fYDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;
            }
        }

        else if(ptGizmoData->tState == PL_GIZMO_STATE_Z_SCALE)
        {
            bXSelected = false;
            bYSelected = false;
            bZSelected = true;

            if(ptParentTransform)
            {
                tCurrentScale.z = (1.0f + fZDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;

                plMat4 tDesired = pl_rotation_translation_scale(tCurrentRot, tCurrentTrans, tCurrentScale);

                plMat4 tInvParent = pl_mat4_invert(&ptParentTransform->tWorld);
                plMat4 tChildWorld = pl_mul_mat4(&tInvParent, &tDesired);
                pl_decompose_matrix(&tChildWorld, &ptSelectedTransform->tScale, &ptSelectedTransform->tRotation, &ptSelectedTransform->tTranslation);
            }
            else
            {
                ptSelectedTransform->tScale.z = (1.0f + fZDistanceAlong - ptGizmoData->fOriginalDist) * ptGizmoData->fOriginalScale;
            }
        }

        if(bXSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            ptGizmoData->fOriginalScale = tCurrentScale.x;
            ptGizmoData->fOriginalDist = fXDistanceAlong;
            ptGizmoData->tOriginalPos = *ptCenter;
            ptGizmoData->tState = PL_GIZMO_STATE_X_SCALE;
        }
        else if(bYSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            ptGizmoData->fOriginalScale = tCurrentScale.y;
            ptGizmoData->fOriginalDist = fYDistanceAlong;
            ptGizmoData->tOriginalPos = *ptCenter;
            ptGizmoData->tState = PL_GIZMO_STATE_Y_SCALE;
        }
        else if(bZSelected && gptIO->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            ptGizmoData->fOriginalScale = tCurrentScale.z;
            ptGizmoData->fOriginalDist = fZDistanceAlong;
            ptGizmoData->tOriginalPos = *ptCenter;
            ptGizmoData->tState = PL_GIZMO_STATE_Z_SCALE;
        }

        if(gptIO->is_mouse_released(PL_MOUSE_BUTTON_LEFT))
        {
            ptGizmoData->tOriginalPos = *ptCenter;
            ptGizmoData->tState = PL_GIZMO_STATE_DEFAULT;
        }

        plVec4 tXColor = (plVec4){1.0f, 0.0f, 0.0f, 1.0f};
        plVec4 tYColor = (plVec4){0.0f, 1.0f, 0.0f, 1.0f};
        plVec4 tZColor = (plVec4){0.0f, 0.0f, 1.0f, 1.0f};

        if(bXSelected)       tXColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
        else if(bYSelected)  tYColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};
        else if(bZSelected)  tZColor = (plVec4){1.0f, 1.0f, 0.0f, 1.0f};

        // x axis
        plDrawCylinderDesc tDrawDesc3 = {0};
        gptDraw->fill_cylinder_desc_default(&tDrawDesc3);
        tDrawDesc3.tColor = tXColor;
        tDrawDesc3.tBasePos = *ptCenter;
        tDrawDesc3.tTipPos = (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z};
        tDrawDesc3.fRadius = fAxisRadius;
        gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc3);

        // y axis
        plDrawCylinderDesc tDrawDesc4 = {0};
        gptDraw->fill_cylinder_desc_default(&tDrawDesc4);
        tDrawDesc4.tColor = tYColor;
        tDrawDesc4.tBasePos = *ptCenter;
        tDrawDesc4.tTipPos = (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z};
        tDrawDesc4.fRadius = fAxisRadius;
        gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc4);

        // z axis
        plDrawCylinderDesc tDrawDesc5 = {0};
        gptDraw->fill_cylinder_desc_default(&tDrawDesc5);
        tDrawDesc5.tColor = tZColor;
        tDrawDesc5.tBasePos = *ptCenter;
        tDrawDesc5.tTipPos = (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength};
        tDrawDesc5.fRadius = fAxisRadius;
        gptDraw->add_3d_cylinder_filled_ex(ptGizmoDrawlist, &tDrawDesc5);

        // x end
        gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
            (plVec3){ptCenter->x + fLength, ptCenter->y, ptCenter->z},
            fAxisRadius * 4,
            fAxisRadius * 4,
            fAxisRadius * 4,
            tXColor);

        // y end
        gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
            (plVec3){ptCenter->x, ptCenter->y + fLength, ptCenter->z},
            fAxisRadius * 4,
            fAxisRadius * 4,
            fAxisRadius * 4,
            tYColor);

        // z end
        gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
            (plVec3){ptCenter->x, ptCenter->y, ptCenter->z + fLength},
            fAxisRadius * 4,
            fAxisRadius * 4,
            fAxisRadius * 4,
            tZColor);

        // origin
        gptDraw->add_3d_centered_box_filled(ptGizmoDrawlist,
            *ptCenter,
            fAxisRadius * 4,
            fAxisRadius * 4,
            fAxisRadius * 4,
            (plVec4){0.5f, 0.5f, 0.5f, 1.0f});
}