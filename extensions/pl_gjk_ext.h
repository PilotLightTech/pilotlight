/*
   pl_gjk_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] inline support functions
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GJK_EXT_H
#define PL_GJK_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include <float.h>
#include <math.h>

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plGjkI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plGjkCollisionInfo plGjkCollisionInfo;
typedef struct _plConvexPoly       plConvexPoly;
typedef struct _plFrustum          plFrustum;

// support function signature: given a shape and direction, return the
// farthest point on the shape in that direction.
typedef plVec3 (*plGjkSupportFunc)(const void* pShape, plVec3 tDir);

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading/unloading
PL_API void pl_load_gjk_ext  (plApiRegistryI*, bool bReload);
PL_API void pl_unload_gjk_ext(plApiRegistryI*, bool bReload);

PL_API bool pl_gjk_pen(plGjkSupportFunc tFn1, const void* pShape1, plGjkSupportFunc tFn2, const void* pShape2, plGjkCollisionInfo* ptInfoOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plGjkI
{
    bool (*pen)(plGjkSupportFunc tFn1, const void* pShape1, plGjkSupportFunc tFn2, const void* pShape2, plGjkCollisionInfo* ptInfoOut);
} plGjkI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// generic convex polyhedron for arbitrary vertex data
typedef struct _plConvexPoly
{
    plVec3* pVertices;
    int     iVertexCount;
} plConvexPoly;

typedef struct _plGjkCollisionInfo
{
    float  fPenetration;
    plVec3 tNormal;
    plVec3 tPoint;
} plGjkCollisionInfo;

// frustum defined by 6 planes: 0=left, 1=right, 2=top, 3=bottom, 4=near, 5=far
typedef struct _plFrustum
{
    plPlane atPlanes[6];
} plFrustum;

//-----------------------------------------------------------------------------
// [SECTION] inline support functions
//-----------------------------------------------------------------------------

static inline plVec3
pl_gjk_support_sphere(const void* pShape, plVec3 tDir)
{
    const plSphere* ptSphere = (const plSphere*)pShape;
    plVec3 tNormDir = pl_norm_vec3(tDir);
    return (plVec3){
        ptSphere->tCenter.x + ptSphere->fRadius * tNormDir.x,
        ptSphere->tCenter.y + ptSphere->fRadius * tNormDir.y,
        ptSphere->tCenter.z + ptSphere->fRadius * tNormDir.z
    };
}

static inline plVec3
pl_gjk_support_aabb(const void* pShape, plVec3 tDir)
{
    const plAABB* ptAABB = (const plAABB*)pShape;
    return (plVec3){
        tDir.x >= 0.0f ? ptAABB->tMax.x : ptAABB->tMin.x,
        tDir.y >= 0.0f ? ptAABB->tMax.y : ptAABB->tMin.y,
        tDir.z >= 0.0f ? ptAABB->tMax.z : ptAABB->tMin.z
    };
}

static inline plVec3
pl_gjk_support_box(const void* pShape, plVec3 tDir)
{
    const plBox* ptBox = (const plBox*)pShape;
    plMat4 tInv = pl_mat4_invert(&ptBox->tTransform);
    plVec3 tLocalDir = pl_mul_mat4_vec4(&tInv, (plVec4){tDir.x, tDir.y, tDir.z, 0.0f}).xyz;
    plVec3 tLocalPoint = (plVec3){
        tLocalDir.x >= 0.0f ? ptBox->tHalfSize.x : -ptBox->tHalfSize.x,
        tLocalDir.y >= 0.0f ? ptBox->tHalfSize.y : -ptBox->tHalfSize.y,
        tLocalDir.z >= 0.0f ? ptBox->tHalfSize.z : -ptBox->tHalfSize.z
    };
    return pl_mul_mat4_vec3(&ptBox->tTransform, tLocalPoint);
}

static inline plVec3
pl_gjk_support_capsule(const void* pShape, plVec3 tDir)
{
    const plCapsule* ptCapsule = (const plCapsule*)pShape;
    plVec3 tNormDir = pl_norm_vec3(tDir);
    float fDotBase = pl_dot_vec3(ptCapsule->tBasePos, tDir);
    float fDotTip  = pl_dot_vec3(ptCapsule->tTipPos, tDir);
    plVec3 tCenter = fDotTip >= fDotBase ? ptCapsule->tTipPos : ptCapsule->tBasePos;
    return (plVec3){
        tCenter.x + ptCapsule->fRadius * tNormDir.x,
        tCenter.y + ptCapsule->fRadius * tNormDir.y,
        tCenter.z + ptCapsule->fRadius * tNormDir.z
    };
}

static inline plVec3
pl_gjk_support_cylinder(const void* pShape, plVec3 tDir)
{
    const plCylinder* ptCylinder = (const plCylinder*)pShape;
    plVec3 tAxis = pl_sub_vec3(ptCylinder->tTipPos, ptCylinder->tBasePos);
    float fAxisLen = pl_length_vec3(tAxis);
    plVec3 tAxisNorm = pl_mul_vec3_scalarf(tAxis, 1.0f / fAxisLen);
    float fDotAxis = pl_dot_vec3(tDir, tAxisNorm);
    plVec3 tPerp = pl_sub_vec3(tDir, pl_mul_vec3_scalarf(tAxisNorm, fDotAxis));
    float fPerpLen = pl_length_vec3(tPerp);
    plVec3 tCenter = fDotAxis >= 0.0f ? ptCylinder->tTipPos : ptCylinder->tBasePos;
    if (fPerpLen > 1e-8f) {
        plVec3 tPerpNorm = pl_mul_vec3_scalarf(tPerp, 1.0f / fPerpLen);
        return pl_add_vec3(tCenter, pl_mul_vec3_scalarf(tPerpNorm, ptCylinder->fRadius));
    }
    return tCenter;
}

static inline plVec3
pl_gjk_support_cone(const void* pShape, plVec3 tDir)
{
    const plCone* ptCone = (const plCone*)pShape;
    plVec3 tAxis = pl_sub_vec3(ptCone->tTipPos, ptCone->tBasePos);
    float fAxisLen = pl_length_vec3(tAxis);
    plVec3 tAxisNorm = pl_mul_vec3_scalarf(tAxis, 1.0f / fAxisLen);
    float fDotAxis = pl_dot_vec3(tDir, tAxisNorm);
    float fSinAngle = ptCone->fRadius / sqrtf(fAxisLen * fAxisLen + ptCone->fRadius * ptCone->fRadius);
    if (fDotAxis > pl_length_vec3(tDir) * fSinAngle)
        return ptCone->tTipPos;
    plVec3 tPerp = pl_sub_vec3(tDir, pl_mul_vec3_scalarf(tAxisNorm, fDotAxis));
    float fPerpLen = pl_length_vec3(tPerp);
    if (fPerpLen > 1e-8f) {
        plVec3 tPerpNorm = pl_mul_vec3_scalarf(tPerp, 1.0f / fPerpLen);
        return pl_add_vec3(ptCone->tBasePos, pl_mul_vec3_scalarf(tPerpNorm, ptCone->fRadius));
    }
    return ptCone->tBasePos;
}

static inline plVec3
pl_gjk_support_convex_poly(const void* pShape, plVec3 tDir)
{
    const plConvexPoly* ptPoly = (const plConvexPoly*)pShape;
    float fMaxDot = -FLT_MAX;
    int iBest = 0;
    for (int ii = 0; ii < ptPoly->iVertexCount; ii++) {
        float fDot = pl_dot_vec3(ptPoly->pVertices[ii], tDir);
        if (fDot > fMaxDot) {
            fMaxDot = fDot;
            iBest = ii;
        }
    }
    return ptPoly->pVertices[iBest];
}

static inline plVec3
pl_gjk_support_frustum(const void* pShape, plVec3 tDir)
{
    const plFrustum* ptFrustum = (const plFrustum*)pShape;
    static const int aiCornerPlanes[8][3] = {
        {0, 3, 4}, {1, 3, 4}, {1, 2, 4}, {0, 2, 4},
        {0, 3, 5}, {1, 3, 5}, {1, 2, 5}, {0, 2, 5},
    };
    float fMaxDot = -FLT_MAX;
    plVec3 tBest = {0};
    for (int ii = 0; ii < 8; ii++)
    {
        const plPlane* ptP0 = &ptFrustum->atPlanes[aiCornerPlanes[ii][0]];
        const plPlane* ptP1 = &ptFrustum->atPlanes[aiCornerPlanes[ii][1]];
        const plPlane* ptP2 = &ptFrustum->atPlanes[aiCornerPlanes[ii][2]];
        plVec3 tCross = pl_cross_vec3(ptP1->tDirection, ptP2->tDirection);
        float fDenom = pl_dot_vec3(ptP0->tDirection, tCross);
        if (fabsf(fDenom) < 1e-8f) continue;
        plVec3 tPoint = pl_mul_vec3_scalarf(
            pl_add_vec3(
                pl_add_vec3(
                    pl_mul_vec3_scalarf(tCross, ptP0->fOffset),
                    pl_mul_vec3_scalarf(pl_cross_vec3(ptP2->tDirection, ptP0->tDirection), ptP1->fOffset)),
                pl_mul_vec3_scalarf(pl_cross_vec3(ptP0->tDirection, ptP1->tDirection), ptP2->fOffset)),
            1.0f / fDenom);
        float fDot = pl_dot_vec3(tPoint, tDir);
        if (fDot > fMaxDot)
        {
            fMaxDot = fDot;
            tBest = tPoint;
        }
    }
    return tBest;
}

#endif // PL_GJK_EXT_H