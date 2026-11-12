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
#include "pl_camera_ext.h"
#include "pl_animation_ext.h"
#include "pl_collision_ext.h"
#include "pl_transform_ext.h"
#include "pl_ecs_ext.h"
#include "pl_ui_ext.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
static const plUiI*        gptUI         = NULL;
static const plDrawI*      gptDraw       = NULL;
static const plIOI*        gptIOI        = NULL;
static const plCollisionI* gptCollision  = NULL;
static const plTransformI* gptTransform  = NULL;
static const plEcsI*       gptEcs        = NULL;
#endif

//-----------------------------------------------------------------------------
// [SECTION] internal structs & enums
//-----------------------------------------------------------------------------

typedef struct _plGizmoContext
{
    plGizmoMode          tSelectionMode;
    plGizmoSpace         tSpace;
    bool                 bActive;
    int                  iHandle; // 0..2 axes, 3..5 planes (normal axis), 6 uniform scale
    plComponentLibrary*  ptTargetLibrary;
    plEntity             tTargetEntity;
    plVec3               atAxes[3];
    plVec3               tCenter;
    plVec3               tPlaneNormal;
    plVec3               tStartHit;
    plVec2               tStartMouse;
    float                fLength;
    float                fTranslationSnap, fRotationSnap, fScaleSnap;
    float                fDragAngle;
    plTransformComponent tStartTransform;
    plMat4               tParentInverse;
} plGizmoContext;

static plGizmoContext* gptGizmoCtx;

static inline void
pl__gizmo_cancel_drag(void)
{
    gptGizmoCtx->bActive = false;
}

static bool
pl__gizmo_mouse_ray(const plCamera* ptCamera, plVec2 tMouse, plVec2 tOffset, plVec2 tSize, plVec3* ptOrigin, plVec3* ptDirection)
{
    if(tSize.x <= 0.0f || tSize.y <= 0.0f)
        return false;
        
    float x = 2.0f * (tMouse.x - tOffset.x) / tSize.x - 1.0f;
    float y = 2.0f * (tMouse.y - tOffset.y) / tSize.y - 1.0f;
    float z = ptCamera->eDepthMode == PL_CAMERA_DEPTH_MODE_REVERSE_Z ? 1.0f : 0.0f;
    plVec4 tNear = pl_mul_mat4_vec4(&ptCamera->tInvViewProjMat, (plVec4){ x, y, z, 1.0f});
    plVec4 tMiddle = pl_mul_mat4_vec4(&ptCamera->tInvViewProjMat, (plVec4){ x, y, 0.5f, 1.0f});

    if(fabsf(tNear.w) < 1e-8f || fabsf(tMiddle.w) < 1e-8f)
        return false;

    *ptOrigin = pl_div_vec4_scalarf(tNear, tNear.w).xyz;
    plVec3 tDelta = pl_sub_vec3(pl_div_vec4_scalarf(tMiddle, tMiddle.w).xyz, *ptOrigin);
    float fLength = pl_length_vec3(tDelta);
    if(!isfinite(fLength) || fLength < 1e-8f)
        return false;
    *ptDirection = pl_div_vec3_scalarf(tDelta, fLength);

    return isfinite(ptOrigin->x) && isfinite(ptOrigin->y) && isfinite(ptOrigin->z);
}

static inline float
pl__gizmo_signed_angle(plVec3 tFrom, plVec3 tTo, plVec3 tAxis)
{
    return atan2f(pl_dot_vec3(tAxis, pl_cross_vec3(tFrom, tTo)), pl_dot_vec3(tFrom, tTo));
}

static inline float
pl__gizmo_scale_factor(float fDistance, float fLength)
{
    return fmaxf(0.001f, 1.0f + fDistance / fmaxf(fLength, 1e-6f));
}

static bool
pl__does_line_intersect_cylinder(plVec3 tP0, plVec3 tV0, plVec3 tP1, plVec3 tV1, float fRadius, float fHeight, float* pfDistance)
{
    *pfDistance = NAN;
    plVec3 tDp = pl_sub_vec3(tP1, tP0);

    float fV12 = pl_dot_vec3(tV0, tV0);
    float fV22 = pl_dot_vec3(tV1, tV1);
    float fV1V2 = pl_dot_vec3(tV0, tV1);

    float fDet = fV1V2 * fV1V2 - fV12 * fV22;

    float fDistance = FLT_MAX;

    if(fabsf(fDet) > 1e-6f)
    {
        fDet = 1.0f / fDet;
        float fDpv1 = pl_dot_vec3(tDp, tV0);
        float fDpv2 = pl_dot_vec3(tDp, tV1);

        float fT1 = (fV1V2 * fDpv2 - fV22 * fDpv1) * fDet;
        float fT2 = (fV12 * fDpv2 - fV1V2 * fDpv1) * fDet;

        fDistance = pl_length_vec3(pl_add_vec3(tDp, pl_sub_vec3(pl_mul_vec3_scalarf(tV1, fT2), pl_mul_vec3_scalarf(tV0, fT1))));

        *pfDistance = fT2;

        if(fT1 >= 0.0f && fDistance < fRadius && fT2 > 0.0f && fT2 < fHeight)
            return true;
    }
    return false;
}


static inline bool
pl__gizmo_plane_hit(plVec3 tOrigin, plVec3 tDirection, plVec3 tCenter, plVec3 tNormal, plVec3* ptHit)
{
    float fDenominator = pl_dot_vec3(tDirection, tNormal);
    if(fabsf(fDenominator) < 1e-5f)
        return false;
    float fDistance = pl_dot_vec3(pl_sub_vec3(tCenter, tOrigin), tNormal) / fDenominator;
    if(!isfinite(fDistance) || fDistance < 0)
        return false;
    *ptHit = pl_add_vec3(tOrigin, pl_mul_vec3_scalarf(tDirection, fDistance));
    return true;
}

// A fixed plane containing the dragged axis, facing the initial mouse ray.
static inline bool
pl__gizmo_axis_plane(plVec3 tAxis, plVec3 tRay, plVec3* ptNormal)
{
    *ptNormal = pl_sub_vec3(tRay, pl_mul_vec3_scalarf(tAxis, pl_dot_vec3(tRay, tAxis)));
    float fLength = pl_length_vec3(*ptNormal);
    if(fLength < 0.05f)
        return false; // End-on axis: use another view or the center handle.
    *ptNormal = pl_div_vec3_scalarf(*ptNormal, fLength);
    return true;
}

static inline void
pl__gizmo_axes(const plMat4* ptWorld, plGizmoSpace tSpace, plVec3 atAxes[3])
{
    for(int i = 0; i < 3; i++)
    {
        atAxes[i] = (plVec3){0};
        atAxes[i].d[i] = 1;
        if(tSpace == PL_GIZMO_SPACE_LOCAL)
        {
            float fLength = pl_length_vec3(ptWorld->col[i].xyz);
            if(fLength > 1e-6f)
                atAxes[i] = pl_div_vec3_scalarf(ptWorld->col[i].xyz, fLength);
        }
    }
}

// Coordinates in a possibly skewed local plane (nonuniformly scaled parent).
static inline plVec2
pl__gizmo_plane_coordinates(plVec3 tDelta, plVec3 u, plVec3 v)
{
    float uu = pl_dot_vec3(u,u);
    float uv = pl_dot_vec3(u,v);
    float vv = pl_dot_vec3(v,v);
    float du = pl_dot_vec3(tDelta,u);
    float dv = pl_dot_vec3(tDelta,v);

    float determinant = uu * vv - uv * uv;
    if(fabsf(determinant) < 1e-8f)
        return (plVec2){0};
    return (plVec2){ (du * vv - dv * uv) / determinant, ( dv * uu - du * uv) / determinant};
}

static plVec3
pl__gizmo_plane_normal(const plVec3 axes[3], int axis)
{
    plVec3 normal = pl_cross_vec3(axes[(axis + 1) % 3], axes[(axis + 2) % 3]);
    float length = pl_length_vec3(normal);
    return length > 1e-6f ? pl_div_vec3_scalarf(normal, length) : (plVec3){0};
}

void
pl_gizmo_set_mode(plGizmoMode tMode)
{
    if(tMode < PL_GIZMO_MODE_NONE || tMode >= PL_GIZMO_MODE_COUNT)
        tMode = PL_GIZMO_MODE_NONE;
    if(gptGizmoCtx->tSelectionMode != tMode)
        pl__gizmo_cancel_drag();
    gptGizmoCtx->tSelectionMode = tMode;
}

void
pl_gizmo_set_snap(float fTranslation, float fRotationDegrees, float fScale)
{
    gptGizmoCtx->fTranslationSnap = isfinite(fTranslation) && fTranslation > 0 ? fTranslation : 0;
    gptGizmoCtx->fRotationSnap = isfinite(fRotationDegrees) && fRotationDegrees > 0 ? fRotationDegrees * (PL_PI / 180.0f) : 0;
    gptGizmoCtx->fScaleSnap = isfinite(fScale) && fScale > 0 ? fScale : 0;
}

static inline float
pl__gizmo_snap(float value, float step)
{
    return gptIOI->get_io()->bKeyCtrl && step > 0 ? roundf(value / step) * step : value;
}

void
pl_gizmo_next_mode(void)
{
    pl_gizmo_set_mode((gptGizmoCtx->tSelectionMode + 1) % PL_GIZMO_MODE_COUNT);
}

void
pl_gizmo_set_space(plGizmoSpace tSpace)
{
    if(tSpace != PL_GIZMO_SPACE_LOCAL)
        tSpace = PL_GIZMO_SPACE_WORLD;
    if(gptGizmoCtx->tSpace != tSpace)
        pl__gizmo_cancel_drag();
    gptGizmoCtx->tSpace = tSpace;
}

plGizmoSpace
pl_gizmo_get_space(void)
{
    return gptGizmoCtx->tSpace;
}

bool
pl_gizmo_active(void)
{
    if(gptGizmoCtx->bActive && !gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        pl__gizmo_cancel_drag();
    return gptGizmoCtx->bActive;
}

// Every update is relative to the captured transform, never the previous frame.
static void
pl__gizmo_apply_drag(plComponentLibrary* ptLibrary, plEntity tEntity, plVec3 tHit, plVec2 tMouse)
{
    plGizmoContext* c = gptGizmoCtx;
    plTransformComponent* t = gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity);
    plVec3 tDelta = pl_sub_vec3(tHit, c->tStartHit);
    int axis = c->iHandle;
    if(c->tSelectionMode == PL_GIZMO_MODE_TRANSLATION)
    {
        if(axis < 3)
            tDelta = pl_mul_vec3_scalarf(c->atAxes[axis], pl__gizmo_snap(pl_dot_vec3(tDelta, c->atAxes[axis]), c->fTranslationSnap));
        else
        {
            int normal = axis - 3;
            plVec3 u = c->atAxes[(normal + 1) % 3], v = c->atAxes[(normal + 2) % 3];
            plVec2 coordinates = pl__gizmo_plane_coordinates(tDelta,u,v);
            tDelta = pl_add_vec3(pl_mul_vec3_scalarf(u,pl__gizmo_snap(coordinates.x,c->fTranslationSnap)), pl_mul_vec3_scalarf(v,pl__gizmo_snap(coordinates.y,c->fTranslationSnap)));
        }
        plVec3 tLocalDelta = pl_mul_mat4_vec4(&c->tParentInverse, (plVec4){.xyz = tDelta, .w = 0.0f}).xyz;
        t->tTranslation = pl_add_vec3(c->tStartTransform.tTranslation, tLocalDelta);
    }
    else if(c->tSelectionMode == PL_GIZMO_MODE_SCALE)
    {
        float fFactor = axis == 6 ? pl__gizmo_scale_factor(c->tStartMouse.y - tMouse.y, 200) : pl__gizmo_scale_factor(pl_dot_vec3(tDelta, c->atAxes[axis]), c->fLength);
        fFactor = fmaxf(0.001f, 1.0f + pl__gizmo_snap(fFactor - 1.0f, c->fScaleSnap));
        if(fFactor == 1.0f)
        {
            t->tTranslation = c->tStartTransform.tTranslation;
            t->tRotation = c->tStartTransform.tRotation;
            t->tScale = c->tStartTransform.tScale;
        }
        else if(axis == 6 || c->tSpace == PL_GIZMO_SPACE_LOCAL)
        {
            t->tScale = c->tStartTransform.tScale;
            if(axis == 6)
                t->tScale = pl_mul_vec3_scalarf(t->tScale, fFactor);
            else
                t->tScale.d[axis] *= fFactor;
        }
        else
        {
            plMat4 tDesired = c->tStartTransform.tWorld;
            // Stretch the original world basis; keep its position fixed.
            for(int col = 0; col < 3; col++)
                tDesired.col[col].d[axis] *= fFactor;
            gptTransform->set_world_transform(ptLibrary, tEntity, &tDesired);
        }
    }

    else if(c->tSelectionMode == PL_GIZMO_MODE_ROTATION)
    {
        plVec3 tFrom = pl_sub_vec3(c->tStartHit, c->tCenter);
        plVec3 tTo = pl_sub_vec3(tHit, c->tCenter);
        if(pl_length_sqr_vec3(tTo) < 1e-12f)
            return;
        plVec2 from = pl__gizmo_plane_coordinates(tFrom,c->atAxes[(axis + 1) % 3], c->atAxes[(axis + 2) % 3]);
        plVec2 to = pl__gizmo_plane_coordinates(tTo,c->atAxes[(axis + 1) % 3], c->atAxes[(axis + 2) % 3]);
        float fAngle = pl__gizmo_signed_angle((plVec3){from.x, from.y, 0},(plVec3){to.x, to.y, 0}, (plVec3){0, 0, 1});
        fAngle = pl__gizmo_snap(fAngle,c->fRotationSnap);
        c->fDragAngle = fAngle;
        if(c->tSpace == PL_GIZMO_SPACE_LOCAL)
        {
            plVec3 tAxis = {0};
            tAxis.d[axis] = 1;
            t->tRotation = pl_norm_quat(pl_mul_quat(c->tStartTransform.tRotation, pl_quat_rotation(fAngle, tAxis.x, tAxis.y, tAxis.z)));
        }
        else
        {
            plMat4 tRotation = pl_rotation_translation_scale(pl_quat_rotation(fAngle, c->atAxes[axis].x, c->atAxes[axis].y, c->atAxes[axis].z), (plVec3){0}, (plVec3){1.0f , 1.0f , 1.0f});
            plMat4 tDesired = pl_mul_mat4(&tRotation, &c->tStartTransform.tWorld);
            tDesired.col[3] = c->tStartTransform.tWorld.col[3];
            gptTransform->set_world_transform(ptLibrary, tEntity, &tDesired);
        }
    }
    t->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
}

static void
pl__gizmo_draw(plDrawList3D* ptList, plVec3 tCenter, const plVec3 atAxes[3], float fLength, int iHover)
{
    const uint32_t auColors[3] = {PL_COLOR_32_RED, PL_COLOR_32_GREEN, PL_COLOR_32_BLUE};
    const float fRadius = fLength * (0.0035f / 0.15f);
    for(int i = 0; i < 3; i++)
    {
        uint32_t color = iHover == i ? PL_COLOR_32_YELLOW : auColors[i];
        if(gptGizmoCtx->tSelectionMode == PL_GIZMO_MODE_ROTATION)
        {
            plVec3 u = atAxes[(i + 1) % 3], v = atAxes[(i + 2) % 3];
            // Original filled rotation bands, expressed in the selected basis.
            for(int j = 0; j < 36; j++)
            {
                float a = 2 * PL_PI * j / 36, b = 2 * PL_PI * (j + 1) / 36;
                plVec3 p = pl_mul_vec3_scalarf(pl_add_vec3(pl_mul_vec3_scalarf(u, cosf(a)), pl_mul_vec3_scalarf(v, sinf(a))), fLength);
                plVec3 q = pl_mul_vec3_scalarf(pl_add_vec3(pl_mul_vec3_scalarf(u, cosf(b)), pl_mul_vec3_scalarf(v, sinf(b))), fLength);
                plVec3 innerP = pl_add_vec3(tCenter, pl_mul_vec3_scalarf(p, 0.8f));
                plVec3 innerQ = pl_add_vec3(tCenter, pl_mul_vec3_scalarf(q, 0.8f));
                p = pl_add_vec3(tCenter,p);
                q = pl_add_vec3(tCenter,q);
                gptDraw->add_3d_triangle_filled(ptList, innerP, p, q, (plDrawSolidOptions){.uColor=color});
                gptDraw->add_3d_triangle_filled(ptList, innerP, q, innerQ, (plDrawSolidOptions){.uColor=color});
            }
        }
        else
        {
            plVec3 tip = pl_add_vec3(tCenter,pl_mul_vec3_scalarf(atAxes[i],fLength));
            plVec3 shaftTip = gptGizmoCtx->tSelectionMode == PL_GIZMO_MODE_TRANSLATION ? pl_sub_vec3(tip,pl_mul_vec3_scalarf(atAxes[i], fLength * 0.2f)) : tip;
            gptDraw->add_3d_cylinder_filled(ptList,(plCylinder){.tBasePos = tCenter, .tTipPos = shaftTip,.fRadius = fRadius}, 0, (plDrawSolidOptions){.uColor = color});
            if(gptGizmoCtx->tSelectionMode == PL_GIZMO_MODE_TRANSLATION)
                gptDraw->add_3d_cone_filled(ptList, (plCone){.tBasePos = shaftTip, .tTipPos = tip, .fRadius = fLength * 0.05f}, 0, (plDrawSolidOptions){.uColor = color});
            else
                gptDraw->add_3d_centered_box_filled(ptList, tip, fRadius * 4, fRadius * 4, fRadius * 4, (plDrawSolidOptions){.uColor = color});
        }
        if(gptGizmoCtx->tSelectionMode == PL_GIZMO_MODE_TRANSLATION)
        {
            plVec3 u = atAxes[(i + 1) % 3];
            plVec3 v = atAxes[(i + 2) % 3];
            plVec3 p[4] = {0};

            const float x[4] = { 0.125f, 0.375f, 0.375f, 0.125f};
            const float y[4] = { 0.125f, 0.125f, 0.375f, 0.375f};
            for(int j = 0; j < 4; j++)
                p[j]=pl_add_vec3(tCenter, pl_mul_vec3_scalarf(pl_add_vec3(pl_mul_vec3_scalarf(u, x[j]), pl_mul_vec3_scalarf(v, y[j])), fLength));
            color = (iHover==i + 3 ? PL_COLOR_32_YELLOW : auColors[i]) & 0x00ffffffu;
            color |= PL_COLOR_32_RGBA(0, 0, 0, 0.5f);
            gptDraw->add_3d_triangle_filled(ptList, p[0], p[1], p[2], (plDrawSolidOptions){.uColor = color});
            gptDraw->add_3d_triangle_filled(ptList, p[0], p[2], p[3], (plDrawSolidOptions){.uColor = color});
        }
    }
    if(gptGizmoCtx->tSelectionMode != PL_GIZMO_MODE_ROTATION)
        gptDraw->add_3d_centered_box_filled(ptList, tCenter, fRadius * 4, fRadius * 4, fRadius * 4, (plDrawSolidOptions){.uColor = iHover==6 ? PL_COLOR_32_YELLOW : PL_COLOR_32_RGB(0.5f, 0.5f, 0.5f)});
}

static void
pl__gizmo_draw_feedback(plDrawList3D* ptList, const plTransformComponent* t, plVec3 tHit)
{
    const plGizmoContext* c = gptGizmoCtx;
    const float radius = c->fLength * (0.0035f / 0.15f);
    plVec3 center = t->tWorld.col[3].xyz;
    plVec3 label = pl_add_vec3(center,(plVec3){0,c->fLength*1.1f,0});
    plDrawTextOptions options = {.ptFont = gptUI->get_default_font(), .uColor = PL_COLOR_32_YELLOW};
    if(c->tSelectionMode == PL_GIZMO_MODE_TRANSLATION)
    {
        plVec3 offset = pl_sub_vec3(center,c->tCenter);
        gptDraw->add_3d_sphere_filled(ptList, (plSphere){.tCenter = c->tCenter, .fRadius = radius * 2}, 0, 0, (plDrawSolidOptions){.uColor = PL_COLOR_32_RGBA(0.5f, 0.5f, 0.5f, 0.5f)});
        gptDraw->add_3d_line(ptList, c->tCenter, center,(plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = radius});
        gptDraw->add_3d_text(ptList, label, options,"offset: %0.3f, %0.3f, %0.3f", offset.x, offset.y, offset.z);
    }
    else if(c->tSelectionMode == PL_GIZMO_MODE_SCALE)
    {
        label.y = center.y + c->fLength * 1.2f;
        gptDraw->add_3d_text(ptList, label, options,"scaling: %0.3f, %0.3f, %0.3f", t->tScale.x, t->tScale.y, t->tScale.z);
    }
    else
    {
        plVec3 from = pl_norm_vec3(pl_sub_vec3(c->tStartHit,c->tCenter));
        plVec3 u = c->atAxes[(c->iHandle + 1) % 3], v = c->atAxes[(c->iHandle + 2) % 3];
        plVec2 start = pl__gizmo_plane_coordinates(pl_sub_vec3(c->tStartHit, c->tCenter), u, v);
        float angle = c->fDragAngle;
        float end = atan2f(start.y, start.x) + angle;
        plVec3 to = pl_norm_vec3(pl_add_vec3(pl_mul_vec3_scalarf(u, cosf(end)), pl_mul_vec3_scalarf(v, sinf(end))));
        gptDraw->add_3d_text(ptList, label, options, "%c-axis rotation: %0.0f degrees",'x' + c->iHandle, angle * 180.0f / PL_PI);
        gptDraw->add_3d_line(ptList, center, pl_add_vec3(center, pl_mul_vec3_scalarf(from, c->fLength * 0.8f)), (plDrawLineOptions){.uColor = PL_COLOR_32_RGB(0.7f, 0.7f, 0.7f), .fThickness = radius});
        gptDraw->add_3d_line(ptList, center, pl_add_vec3(center, pl_mul_vec3_scalarf(to, c->fLength * 0.8f)), (plDrawLineOptions){.uColor = PL_COLOR_32_YELLOW, .fThickness = radius});
    }
}

void
pl_gizmo_gizmo(plComponentLibrary* ptLibrary, plEntity tEntity, plDrawList3D* ptList, plCamera* ptCamera, plVec2 tOffset, plVec2 tScale)
{
    plGizmoContext* c=gptGizmoCtx;
    if(c->ptTargetLibrary != ptLibrary || c->tTargetEntity.uData != tEntity.uData || (c->bActive && !gptIOI->is_mouse_down(PL_MOUSE_BUTTON_LEFT)))
        pl__gizmo_cancel_drag();
    c->ptTargetLibrary = ptLibrary;
    c->tTargetEntity = tEntity;
    if(!ptLibrary || !ptCamera || !ptList || !gptEcs->has_component(ptLibrary,gptTransform->get_ecs_type_key_transform(), tEntity))
    {
        pl__gizmo_cancel_drag();
        return;
    }

    if(c->tSelectionMode == PL_GIZMO_MODE_NONE)
        return;

    plTransformComponent* t = gptEcs->get_component(ptLibrary, gptTransform->get_ecs_type_key_transform(), tEntity);
    if(c->bActive && gptIOI->is_key_pressed(PL_KEY_ESCAPE,false))
    {
        t->tTranslation = c->tStartTransform.tTranslation;
        t->tRotation = c->tStartTransform.tRotation;
        t->tScale = c->tStartTransform.tScale;
        t->tWorld = c->tStartTransform.tWorld;
        t->eFlags |= PL_TRANSFORM_FLAGS_DIRTY;
        pl__gizmo_cancel_drag();
        pl__gizmo_draw(ptList, t->tWorld.col[3].xyz, c->atAxes, c->fLength, -1);
        return;
    }
    plVec3 center=t->tWorld.col[3].xyz;
    plVec3 axes[3];
    pl__gizmo_axes(&t->tWorld,c->tSpace,axes);
    float length = fmaxf(0.001f, 0.1125f * (ptCamera->eProjectionType==PL_CAMERA_PROJECTION_TYPE_ORTHOGRAPHIC ? ptCamera->fHeight : pl_length_vec3(pl_sub_vec3(center,ptCamera->tPositionF))));
    plVec2 mouse=gptIOI->get_mouse_pos();
    plVec2 size = pl_mul_vec2(gptIOI->get_io()->tMainViewportSize, tScale);
    plVec3 origin = {0};
    plVec3 dir = {0};
    plVec3 hit = c->tStartHit;

    if(!pl__gizmo_mouse_ray(ptCamera, mouse, tOffset, size, &origin, &dir))
    {
        pl__gizmo_cancel_drag();
        return;
    }
    if(c->bActive)
    {
        if(c->iHandle==6 || pl__gizmo_plane_hit(origin, dir, c->tCenter, c->tPlaneNormal,&hit))
            pl__gizmo_apply_drag(ptLibrary, tEntity, c->iHandle == 6 ? c->tStartHit : hit, mouse);
        center=t->tWorld.col[3].xyz;
        pl__gizmo_draw_feedback(ptList, t, hit);
        pl__gizmo_draw(ptList, center, c->atAxes, c->fLength, c->iHandle);
        return;
    }
    int hover =- 1;
    float nearest = FLT_MAX;
    bool canPick = !gptUI->wants_mouse_capture() && mouse.x >= tOffset.x && mouse.y >= tOffset.y && mouse.x < tOffset.x + size.x && mouse.y < tOffset.y + size.y;
    if(canPick)
    {
        for(int i = 0; i < 3; i++)
        {
            float along = 0;
            hit = center;
            bool selected = false;
            if(c->tSelectionMode == PL_GIZMO_MODE_ROTATION)
            {
                selected = pl__gizmo_plane_hit(origin, dir, center, pl__gizmo_plane_normal(axes, i), &hit);
                plVec2 coordinates = pl__gizmo_plane_coordinates(pl_sub_vec3(hit, center), axes[(i + 1) % 3], axes[(i + 2) % 3]);
                float radius = pl_length_vec2(coordinates);
                selected = selected && radius > length * 0.8f && radius < length;
            }
            else
            {
                selected = pl__does_line_intersect_cylinder(origin, dir, center, axes[i], length * 0.075f, length * 1.075f, &along);
                hit = pl_add_vec3(center, pl_mul_vec3_scalarf(axes[i], along));
            }
            float distance = pl_length_vec3(pl_sub_vec3(hit, origin));
            if(selected && distance<nearest)
            {
                nearest=distance;
                hover=i;
            }
            
            if(c->tSelectionMode == PL_GIZMO_MODE_TRANSLATION && pl__gizmo_plane_hit(origin, dir, center, pl__gizmo_plane_normal(axes, i), &hit))
            {
                plVec3 delta = pl_sub_vec3(hit, center);
                plVec2 coordinates = pl__gizmo_plane_coordinates(delta, axes[(i + 1) % 3], axes[(i + 2) % 3]);
                float u = coordinates.x / length;
                float v = coordinates.y / length;
                distance = pl_length_vec3(pl_sub_vec3(hit, origin));
                if(u > 0.125f && u < 0.375f && v > 0.125f && v < 0.375f && distance < nearest)
                {
                    nearest = distance;
                    hover = i + 3;
                }
            }
        }
        if(c->tSelectionMode == PL_GIZMO_MODE_SCALE && pl__gizmo_plane_hit(origin, dir, center, dir, &hit) && pl_length_vec3(pl_sub_vec3(hit, center)) < length * 0.10f)
            hover = 6;
    }
    if(hover>=0 && gptIOI->is_mouse_clicked(PL_MOUSE_BUTTON_LEFT,false))
    {
        plVec3 normal = dir;
        bool valid = true;
        if(hover<3)
        {
            if(c->tSelectionMode == PL_GIZMO_MODE_ROTATION)
                normal = pl__gizmo_plane_normal(axes, hover);
            else
                valid = pl__gizmo_axis_plane(axes[hover], dir, &normal);
        }
        else if(hover < 6)
            normal = pl__gizmo_plane_normal(axes, hover - 3);
        if(valid && pl__gizmo_plane_hit(origin, dir, center, normal, &hit))
        {
            c->fDragAngle = 0;
            c->iHandle = hover;
            c->tStartHit = hit;
            c->tPlaneNormal = normal;
            c->tCenter = center;
            c->tStartTransform =* t;
            c->fLength = length;
            c->tStartMouse = mouse;
            for(int i = 0; i < 3; i++)
                c->atAxes[i] = axes[i];
            plMat4 parent = gptTransform->compute_parent_transform(ptLibrary, tEntity);
            float determinant = pl_dot_vec3(parent.col[0].xyz, pl_cross_vec3(parent.col[1].xyz, parent.col[2].xyz));
            if(isfinite(determinant) && fabsf(determinant) > 1e-8f)
            {
                c->tParentInverse = pl_mat4_invert(&parent);
                c->bActive = true;
            }
        }
    }
    pl__gizmo_draw(ptList, center, axes, length, hover);
}

void
pl_load_gizmo_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plGizmoI tApi = {
        .set_mode  = pl_gizmo_set_mode,
        .next_mode = pl_gizmo_next_mode,
        .active    = pl_gizmo_active,
        .gizmo     = pl_gizmo_gizmo,
        .set_space = pl_gizmo_set_space,
        .get_space = pl_gizmo_get_space,
        .set_snap  = pl_gizmo_set_snap
    };
    pl_set_api(ptApiRegistry, plGizmoI, &tApi);

    gptIOI       = pl_get_api_latest(ptApiRegistry, plIOI);
    gptUI        = pl_get_api_latest(ptApiRegistry, plUiI);
    gptDraw      = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptCollision = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptTransform = pl_get_api_latest(ptApiRegistry, plTransformI);
    gptEcs       = pl_get_api_latest(ptApiRegistry, plEcsI);

    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    if(bReload)
    {
        gptGizmoCtx = ptDataRegistry->get_data("plGizmoContext");
    }
    else
    {
        static plGizmoContext gtGizmoCtx = {
            .tSelectionMode   = PL_GIZMO_MODE_TRANSLATION,
            .fTranslationSnap = 1.0f,
            .fRotationSnap    = PL_PI / 12.0f,
            .fScaleSnap       = 0.1f
        };
        gptGizmoCtx = &gtGizmoCtx;
        ptDataRegistry->set_data("plGizmoContext", gptGizmoCtx);
    }
}

void
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