/*
   pl_collision_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] public api implementation
// [SECTION] extension loading
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <float.h>
#include "pl.h"
#include "pl_collision_ext.h"
#include "pl_gjk_ext.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
    static const plGjkI*     gptGjk = NULL;
    #define PL_ALLOC(x)      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
    #define PL_REALLOC(x, y) gptMemory->tracked_realloc((x), (y), __FILE__, __LINE__)
    #define PL_FREE(x)       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)

    #ifndef PL_DS_ALLOC
        #define PL_DS_ALLOC(x)                      gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
        #define PL_DS_ALLOC_INDIRECT(x, FILE, LINE) gptMemory->tracked_realloc(NULL, (x), FILE, LINE)
        #define PL_DS_FREE(x)                       gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
    #endif
#endif

#include "pl_ds.h"

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

bool
pl_collision_intersect_ray_plane(plVec3 tPoint, plVec3 tDir, const plPlane* ptPlane, plVec3* ptIntersectionPointOut)
{
    float fDenom = pl_dot_vec3(ptPlane->tDirection, tDir);
    if(fDenom == 0.0f)
        return false;

    float fT = (ptPlane->fOffset - pl_dot_vec3(ptPlane->tDirection, tPoint)) / fDenom;

    bool bResult = false;
    if(fT >= 0.0f)
    {
        bResult = true;
        if(ptIntersectionPointOut)
        {
            *ptIntersectionPointOut = pl_add_vec3(tPoint, pl_mul_vec3_scalarf(tDir, fT));
        }
    }
    return bResult;
}

bool
pl_collision_intersect_line_segment_plane(plVec3 tA, plVec3 tB, const plPlane* ptPlane, plVec3* ptIntersectionPointOut)
{
    plVec3 tDir = pl_sub_vec3(tB, tA);

    float fDenom = pl_dot_vec3(ptPlane->tDirection, tDir);
    if(fDenom == 0.0f)
        return false;

    float fT = (ptPlane->fOffset - pl_dot_vec3(ptPlane->tDirection, tA)) / fDenom;

    bool bResult = false;
    if(fT >= 0.0f && fT <= 1.0f)
    {
        bResult = true;
        if(ptIntersectionPointOut)
        {
            *ptIntersectionPointOut = pl_add_vec3(tA, pl_mul_vec3_scalarf(tDir, fT));
        }
    }
    return bResult;
}

bool
pl_collision_intersect_line_segment_cylinder(plVec3 tA, plVec3 tB, const plCylinder* ptCylinder, float* pfT)
{
    plVec3 tD = pl_sub_vec3(ptCylinder->tTipPos, ptCylinder->tBasePos);
    plVec3 tM = pl_sub_vec3(tA, ptCylinder->tBasePos);
    plVec3 tN = pl_sub_vec3(tB, tA);

    float fMd = pl_dot_vec3(tM, tD);
    float fNd = pl_dot_vec3(tN, tD);
    float fDd = pl_dot_vec3(tD, tD);

    // test if segment fully outside endcap of cylinder
    if(fMd < 0.0f && fMd + fNd < 0.0f)
        return false;
    if(fMd > fDd && fMd + fNd > fDd)
        return false;

    float fNn = pl_dot_vec3(tN, tN);
    float fMn = pl_dot_vec3(tM, tN);
    float fA = fDd * fNn - fNd * fNd;
    float fK = pl_dot_vec3(tM, tM) - ptCylinder->fRadius * ptCylinder->fRadius;
    float fC = fDd * fK - fMd * fMd;
    float fT = 0.0f;

    if(fabsf(fA) < 0.0001f)
    {
        // segment runs parallel to cylinder axis
        if(fC > 0.0f)
            return false;
        
        // now known segment intersects cylinder, figure out how
        if(fMd < 0.0f) // intersect endcap at base
            fT = -fMn / fNn;
        else if (fMd > fDd) // intersect endcap at tip
            fT = (fNd - fMn) / fNn;
        else // lies inside cylinder
            fT = 0.0f;
        if(pfT)
            *pfT = fT;
        return true;
    }

    float fB = fDd * fMn - fNd * fMd;
    float fDiscr = fB * fB - fA * fC;
    if(fDiscr < 0.0f) // no real roots, no intersection
        return false;
    
    fT = (-fB - sqrtf(fDiscr)) / fA;
    if(fT < 0.0f || fT > 1.0f) // intersection lies outside segment
        return false;

    if(fMd + fT * fNd < 0.0f)
    {
        // intersection outside cylinder on base side
        if(fNd <= 0.0f) // segment pointing away from endcap
            return false;
        fT = -fMd / fNd;
        if(pfT)
            *pfT = fT;
        return fK + 2.0f * fT * (fMn + fT * fNn) <= 0.0f;
    }
    else if(fMd + fT * fNd > fDd)
    {
        // intersection outside cylinder on tip side
        if(fNd >= 0.0f) // segment pointing away from endcap
            return false;
        fT = (fDd - fMd) / fNd;
        if(pfT)
            *pfT = fT;
        return fK + fDd - 2.0f * fMd + fT * (2.0f * (fMn - fNd) + fT * fNn) <= 0.0f;
    }
    if(pfT)
        *pfT = fT;
    return true;
}

plVec3
pl_collision_point_closest_point_plane(plVec3 tPoint, const plPlane* ptPlane)
{
    float fT = pl_dot_vec3(ptPlane->tDirection, tPoint) - ptPlane->fOffset;
    return pl_sub_vec3(tPoint, pl_mul_vec3_scalarf(ptPlane->tDirection, fT));
}

plVec3
pl_collision_point_closest_point_line_segment(plVec3 tPoint, plVec3 tA, plVec3 tB, float* pfT)
{
    plVec3 tResult = {0};
    plVec3 tAB = pl_sub_vec3(tB, tA);
    
    float fT = pl_dot_vec3(pl_sub_vec3(tPoint, tA), tAB);

    if(fT <= 0.0f)
    {
        fT = 0.0f;
        tResult = tA;
    }
    else
    {
        float fDenom = pl_dot_vec3(tAB, tAB);
        if(fT >= fDenom)
        {
            fT = 1.0f;
            tResult = tB;
        }
        else
        {
            fT = fT / fDenom;
            tResult = pl_add_vec3(tA, pl_mul_vec3_scalarf(tAB, fT));
        }
    }

    if(pfT)
        *pfT = fT;

    return tResult;
}

plVec3
pl_collision_point_closest_point_line_aabb(plVec3 tPoint, plAABB tAABB)
{
    plVec3 tResult = {0};
    for(uint32_t i = 0; i < 3; i++)
    {
        float fV = tPoint.d[i];
        if(fV < tAABB.tMin.d[i]) fV = tAABB.tMin.d[i];
        if(fV > tAABB.tMax.d[i]) fV = tAABB.tMax.d[i];
        tResult.d[i] = fV;
    }
    return tResult;
}

bool
pl_collision_aabb_aabb(const plAABB* ptAABB0, const plAABB* ptAABB1)
{
    if(ptAABB0->tMax.x < ptAABB1->tMin.x || ptAABB0->tMin.x > ptAABB1->tMax.x) return false;
    if(ptAABB0->tMax.y < ptAABB1->tMin.y || ptAABB0->tMin.y > ptAABB1->tMax.y) return false;
    if(ptAABB0->tMax.z < ptAABB1->tMin.z || ptAABB0->tMin.z > ptAABB1->tMax.z) return false;
    return true;
}

bool
pl_collision_sphere_sphere(const plSphere* ptSphere0, const plSphere* ptSphere1)
{
    // find the vector between the objects
    const plVec3 tMidLine = pl_sub_vec3(ptSphere0->tCenter, ptSphere1->tCenter);

    // see if it is large enough
    return pl_length_sqr_vec3(tMidLine) < (ptSphere0->fRadius + ptSphere1->fRadius) * (ptSphere0->fRadius + ptSphere1->fRadius);
}

static float
pl__collision_transform_to_axis(const plBox* ptBox, const plVec3* ptAxis)
{
    return ptBox->tHalfSize.x * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[0].xyz)) + 
        ptBox->tHalfSize.y * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[1].xyz)) + 
        ptBox->tHalfSize.z * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[2].xyz));
}

static bool
pl__collision_overlap_on_axis(const plBox* ptBox0, const plBox* ptBox1, plVec3 tAxis, plVec3 tToCenter)
{
    // project the half-size of one onto axis
    float fOneProject = pl__collision_transform_to_axis(ptBox0, &tAxis);
    float fTwoProject = pl__collision_transform_to_axis(ptBox1, &tAxis);

    // project this onto the axis
    float fDistance = fabsf(pl_dot_vec3(tToCenter, tAxis));

    // check for overlap
    float fCheckDistance = fOneProject + fTwoProject;
    if(fCheckDistance == 0.0f && fDistance == 0.0f)
        return true;
    return (fDistance < fOneProject + fTwoProject);
}

bool
pl_collision_box_box(const plBox* ptBox0, const plBox* ptBox1)
{
    plVec3 tToCenter = pl_sub_vec3(ptBox1->tTransform.col[3].xyz, ptBox0->tTransform.col[3].xyz);
    return (
        // check on box 0's axes first
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[0].xyz, tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[1].xyz, tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox0->tTransform.col[2].xyz, tToCenter) && 

        // box 1
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[0].xyz, tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[1].xyz, tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, ptBox1->tTransform.col[2].xyz, tToCenter) && 

        // cross products
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[2].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[2].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[0].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[1].xyz), tToCenter) && 
        pl__collision_overlap_on_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[2].xyz), tToCenter)
    );
}

bool
pl_collision_box_half_space(const plBox* ptBox, const plPlane* ptPlane)
{
    float fProjectedRadius = pl__collision_transform_to_axis(ptBox, &ptPlane->tDirection);
    float fBoxDistance = pl_dot_vec3(ptPlane->tDirection, ptBox->tTransform.col[3].xyz) - fProjectedRadius;
    return fBoxDistance <= ptPlane->fOffset;
}

bool
pl_collision_sphere_half_space(const plSphere* ptSphere, const plPlane* ptPlane)
{
    float fRadius = ptSphere->fRadius;
    float fBallDistance = pl_dot_vec3(ptPlane->tDirection, ptSphere->tCenter) - fRadius;
    return fBallDistance <= ptPlane->fOffset;
}

bool
pl_collision_box_sphere(const plBox* ptBox, const plSphere* ptSphere)
{
    // transform sphere center into box coordinates
    plMat4 tInverseTransform = pl_mat4_invert(&ptBox->tTransform);
    plVec3 tRelCenter = pl_mul_mat4_vec3(&tInverseTransform, ptSphere->tCenter);

    plVec3 tHalfSize = ptBox->tHalfSize;

    // early out check
    if(fabsf(tRelCenter.x) - ptSphere->fRadius > tHalfSize.x ||
       fabsf(tRelCenter.y) - ptSphere->fRadius > tHalfSize.y ||
       fabsf(tRelCenter.z) - ptSphere->fRadius > tHalfSize.z)
    {
        return false;
    }

    plVec3 tClosestPoint = {0};
    float fDist = 0.0f;

    // clamp each coordinate to box
    fDist = tRelCenter.x;
    if(fDist > tHalfSize.x)  fDist = tHalfSize.x;
    if(fDist < -tHalfSize.x) fDist = -tHalfSize.x;
    tClosestPoint.x = fDist;

    fDist = tRelCenter.y;
    if(fDist > tHalfSize.y)  fDist = tHalfSize.y;
    if(fDist < -tHalfSize.y) fDist = -tHalfSize.y;
    tClosestPoint.y = fDist;

    fDist = tRelCenter.z;
    if(fDist > tHalfSize.z)  fDist = tHalfSize.z;
    if(fDist < -tHalfSize.z) fDist = -tHalfSize.z;
    tClosestPoint.z = fDist;

    // check we're in contact
    fDist = pl_length_sqr_vec3(pl_sub_vec3(tClosestPoint, tRelCenter));
    if(fDist > ptSphere->fRadius * ptSphere->fRadius)
        return false;

    return true;
}

bool
pl_collision_pen_sphere_sphere(const plSphere* ptSphere0, const plSphere* ptSphere1, plCollisionInfo* ptInfoOut)
{
    // find the vector between the objects
    const plVec3 tMidLine = pl_sub_vec3(ptSphere0->tCenter, ptSphere1->tCenter);

    float fCombinedRadius = ptSphere0->fRadius + ptSphere1->fRadius;
    const float fMidLineLength = pl_length_vec3(tMidLine);

    // see if it is large enough
    if(fMidLineLength <= 0.0f || fMidLineLength >= fCombinedRadius)
        return false;

    if(ptInfoOut)
    {
        ptInfoOut->fPenetration = fCombinedRadius - fMidLineLength;
        ptInfoOut->tNormal = pl_mul_vec3_scalarf(tMidLine, 1.0f / fMidLineLength);
        // ptInfoOut->tPoint = pl_add_vec3(ptSphere0->tCenter, pl_mul_vec3_scalarf(tMidLine, 0.5f));
        ptInfoOut->tPoint = pl_sub_vec3(ptSphere0->tCenter, pl_mul_vec3_scalarf(ptInfoOut->tNormal, ptSphere0->fRadius));
    }

    return true;
}

static inline float
pl__collision_penetration_on_axis(const plBox* ptBox0, const plBox* ptBox1, const plVec3* ptAxis, const plVec3* ptToCenter)
{
    // project half-size of one onto axis
    float fOneProject = pl__collision_transform_to_axis(ptBox0, ptAxis);
    float fTwoProject = pl__collision_transform_to_axis(ptBox1, ptAxis);

    // project this onto axis
    float fDistance = fabsf(pl_dot_vec3(*ptToCenter, *ptAxis));

    // return overlap
    return fOneProject + fTwoProject - fDistance;
}

static inline bool
pl__collision_try_axis(const plBox* ptBox0, const plBox* ptBox1, plVec3 tAxis,
    const plVec3* ptToCenter, uint32_t uIndex, float* pfSmallestPenetration, uint32_t* puSmallestCase)
{
    // make sure we have normalized axis and don't check almost parallel axes
    if(pl_length_sqr_vec3(tAxis) < 0.0001f)
        return true;
    
    tAxis = pl_norm_vec3(tAxis);

    float fPenetration = pl__collision_penetration_on_axis(ptBox0, ptBox1, &tAxis, ptToCenter);

    if(fPenetration < 0.0f)
        return false;
    if(fPenetration < *pfSmallestPenetration)
    {
        *pfSmallestPenetration = fPenetration;
        *puSmallestCase = uIndex;
    }
    return true;
}

static inline void
pl__collision_fill_point_face_box_box(const plBox* ptBox0, const plBox* ptBox1,
    const plVec3* ptToCenter, uint32_t uBest, plCollisionInfo* ptInfoOut)
{
    // we know which axis collision is on (i.e best), but
    // we need to work out which of the two faces
    plVec3 tNormal = ptBox0->tTransform.col[uBest].xyz;
    if(pl_dot_vec3(tNormal, *ptToCenter) > 0.0f)
    {
        tNormal = pl_mul_vec3_scalarf(tNormal, -1.0f);
    }

    // work out which vertex of box 1 we're colliding with.
    plVec3 tVertex = ptBox1->tHalfSize;
    if(pl_dot_vec3(ptBox1->tTransform.col[0].xyz, tNormal) < 0.0f) tVertex.x = -tVertex.x;
    if(pl_dot_vec3(ptBox1->tTransform.col[1].xyz, tNormal) < 0.0f) tVertex.y = -tVertex.y;
    if(pl_dot_vec3(ptBox1->tTransform.col[2].xyz, tNormal) < 0.0f) tVertex.z = -tVertex.z;

    ptInfoOut->tNormal = tNormal;
    ptInfoOut->tPoint = pl_mul_mat4_vec3(&ptBox1->tTransform, tVertex);
}

static inline plVec3
pl__collision_contact_point(const plVec3* ptPOne, const plVec3* ptDOne, float fOneSize, const plVec3* ptPTwo, const plVec3* ptDTwo, float fTwoSize, bool bUseOne)
{
    
    float fSmOne = pl_length_sqr_vec3(*ptDOne);
    float fSmTwo = pl_length_sqr_vec3(*ptDTwo);
    float fDpOneTwo = pl_dot_vec3(*ptDTwo, *ptDOne);

    plVec3 tToSt = pl_sub_vec3(*ptPOne, *ptPTwo);
    float fDpStAOne = pl_dot_vec3(*ptDOne, tToSt);
    float fDpStATwo = pl_dot_vec3(*ptDTwo, tToSt);

    float fDenom = fSmOne * fSmTwo - fDpOneTwo * fDpOneTwo;

    // zero denominator indicates parallel lines
    if(fabsf(fDenom) < 0.0001f)
        return bUseOne ? *ptPOne : *ptPTwo;

    float fMua = (fDpOneTwo * fDpStATwo - fSmTwo * fDpStAOne) / fDenom;
    float fMub = (fSmOne * fDpStATwo - fDpOneTwo * fDpStAOne) / fDenom;

    // If either of the edges has the nearest point out
    // of bounds, then the edges aren't crossed, we have
    // an edge-face contact. Our point is on the edge, which
    // we know from the useOne parameter.
    if (fMua > fOneSize ||
        fMua < -fOneSize ||
        fMub > fTwoSize ||
        fMub < -fTwoSize)
    {
        return bUseOne ? *ptPOne : *ptPTwo;
    }
    else
    {
        plVec3 tCOne = pl_add_vec3( *ptPOne, pl_mul_vec3_scalarf(*ptDOne, fMua));
        plVec3 tCTwo = pl_add_vec3( *ptPTwo, pl_mul_vec3_scalarf(*ptDTwo, fMub));
        return pl_add_vec3(pl_mul_vec3_scalarf(tCOne, 0.5f), pl_mul_vec3_scalarf(tCTwo, 0.5f));
    }
}

bool
pl_collision_pen_box_box(const plBox* ptBox0, const plBox* ptBox1, plCollisionInfo* ptInfoOut)
{
    // find vector between centers
    plVec3 tToCenter = pl_sub_vec3(ptBox1->tTransform.col[3].xyz, ptBox0->tTransform.col[3].xyz);

    // start by assuming no contact
    float fPen = FLT_MAX;
    uint32_t uBest = UINT32_MAX;

    // new check each axes, returning if it gives us a
    // separating axis, and keeping track of axis with smallest
    // penetration otherwise
    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[0].xyz, &tToCenter, 0, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[1].xyz, &tToCenter, 1, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox0->tTransform.col[2].xyz, &tToCenter, 2, &fPen, &uBest)) return false;

    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[0].xyz, &tToCenter, 3, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[1].xyz, &tToCenter, 4, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, ptBox1->tTransform.col[2].xyz, &tToCenter, 5, &fPen, &uBest)) return false;

    // store best axis-major in case we run into almost
    // parallel edge collisions later
    uint32_t uBestSingleAxis = uBest;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter,  6, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter,  7, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[0].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter,  8, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter,  9, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter, 10, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[1].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter, 11, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[0].xyz), &tToCenter, 12, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[1].xyz), &tToCenter, 13, &fPen, &uBest)) return false;
    if(!pl__collision_try_axis(ptBox0, ptBox1, pl_cross_vec3(ptBox0->tTransform.col[2].xyz, ptBox1->tTransform.col[2].xyz), &tToCenter, 14, &fPen, &uBest)) return false;

    // make sure we have a result
    PL_ASSERT(uBest != UINT32_MAX);

    if(ptInfoOut == NULL)
        return true;

    ptInfoOut->fPenetration = fPen;

    // we not know there's a collision, and we know which
    // axes gave smallest penetration. We now can deal with it
    // depending on the case
    if(uBest < 3)
    {
        // We've got a vertex of box two on a face of box one.
        pl__collision_fill_point_face_box_box(ptBox0, ptBox1, &tToCenter, uBest, ptInfoOut);
    }
    else if(uBest < 6)
    {
        // We've got a vertex of box one on a face of box two.
        // We use the same algorithm as above, but swap around
        // one and two (and therefore also the vector between their
        // centres).
        tToCenter = pl_mul_vec3_scalarf(tToCenter, -1.0f);
        ptInfoOut->bFlip = true;
        pl__collision_fill_point_face_box_box(ptBox1, ptBox0, &tToCenter, uBest - 3, ptInfoOut);
    }
    else
    {
        // We've got an edge-edge contact. Find out which axes
        uBest -= 6;
        uint32_t uOneAxisIndex = uBest / 3;
        uint32_t uTwoAxisIndex = uBest % 3;

        plVec3 tOneAxis = ptBox0->tTransform.col[uOneAxisIndex].xyz;
        plVec3 tTwoAxis = ptBox1->tTransform.col[uTwoAxisIndex].xyz;
        plVec3 tAxis = pl_norm_vec3(pl_cross_vec3(tOneAxis, tTwoAxis));

        // The axis should point from box one to box two.
        if (pl_dot_vec3(tAxis, tToCenter) > 0.0f)
            tAxis = pl_mul_vec3_scalarf(tAxis, -1.0f);

        // We have the axes, but not the edges: each axis has 4 edges parallel
        // to it, we need to find which of the 4 for each object. We do
        // that by finding the point in the centre of the edge. We know
        // its component in the direction of the box's collision axis is zero
        // (its a mid-point) and we determine which of the extremes in each
        // of the other axes is closest.

        const plVec3 tHalfExtent0 = ptBox0->tHalfSize;
        const plVec3 tHalfExtent1 = ptBox1->tHalfSize;

        plVec3 tPointOnOneEdge = tHalfExtent0;
        plVec3 tPointOnTwoEdge = tHalfExtent1;
        for(uint32_t i = 0; i < 3; i++)
        {
            if(i == uOneAxisIndex)
                tPointOnOneEdge.d[i] = 0;
            else if(pl_dot_vec3(ptBox0->tTransform.col[i].xyz, tAxis) > 0.0f)
                tPointOnOneEdge.d[i] = -tPointOnOneEdge.d[i];

            if(i == uTwoAxisIndex)
                tPointOnTwoEdge.d[i] = 0;
            else if(pl_dot_vec3(ptBox1->tTransform.col[i].xyz, tAxis) < 0.0f)
                tPointOnTwoEdge.d[i] = -tPointOnTwoEdge.d[i];
        }

        // Move them into world coordinates (they are already oriented
        // correctly, since they have been derived from the axes).
        tPointOnOneEdge = pl_mul_mat4_vec3(&ptBox0->tTransform, tPointOnOneEdge);
        tPointOnTwoEdge = pl_mul_mat4_vec3(&ptBox1->tTransform, tPointOnTwoEdge);

        // So we have a point and a direction for the colliding edges.
        // We need to find out point of closest approach of the two
        // line-segments.
        plVec3 tVertex = pl__collision_contact_point(
            &tPointOnOneEdge, &tOneAxis, tHalfExtent0.d[uOneAxisIndex],
            &tPointOnTwoEdge, &tTwoAxis, tHalfExtent1.d[uTwoAxisIndex],
            uBestSingleAxis > 2);

        ptInfoOut->tNormal = tAxis;
        ptInfoOut->tPoint = tVertex;
    }
    return true;
}

bool
pl_collision_pen_box_sphere(const plBox* ptBox, const plSphere* ptSphere, plCollisionInfo* ptInfoOut)
{
    // transform sphere center into box coordinates
    plMat4 tInverseTransform = pl_mat4_invert(&ptBox->tTransform);
    plVec3 tRelCenter = pl_mul_mat4_vec3(&tInverseTransform, ptSphere->tCenter);

    plVec3 tHalfSize = ptBox->tHalfSize;

    // early out check
    if(fabsf(tRelCenter.x) - ptSphere->fRadius > tHalfSize.x ||
       fabsf(tRelCenter.y) - ptSphere->fRadius > tHalfSize.y ||
       fabsf(tRelCenter.z) - ptSphere->fRadius > tHalfSize.z)
    {
        return false;
    }

    plVec3 tClosestPoint = {0};
    float fDist = 0.0f;

    // clamp each coordinate to box
    fDist = tRelCenter.x;
    if(fDist > tHalfSize.x)  fDist = tHalfSize.x;
    if(fDist < -tHalfSize.x) fDist = -tHalfSize.x;
    tClosestPoint.x = fDist;

    fDist = tRelCenter.y;
    if(fDist > tHalfSize.y)  fDist = tHalfSize.y;
    if(fDist < -tHalfSize.y) fDist = -tHalfSize.y;
    tClosestPoint.y = fDist;

    fDist = tRelCenter.z;
    if(fDist > tHalfSize.z)  fDist = tHalfSize.z;
    if(fDist < -tHalfSize.z) fDist = -tHalfSize.z;
    tClosestPoint.z = fDist;

    // check we're in contact
    fDist = pl_length_sqr_vec3(pl_sub_vec3(tClosestPoint, tRelCenter));
    if(fDist > ptSphere->fRadius * ptSphere->fRadius)
        return false;

    if(ptInfoOut)
    {
        if(fDist < 1e-12f)
        {
            int axis = 0;
            for(int i = 1; i < 3; i++)
            {
                if(tHalfSize.d[i] - fabsf(tRelCenter.d[i]) < tHalfSize.d[axis] - fabsf(tRelCenter.d[axis]))
                    axis=i;
            }
            float sign = tRelCenter.d[axis] < 0 ? -1.0f : 1.0f;
            ptInfoOut->fPenetration = ptSphere->fRadius + tHalfSize.d[axis] - fabsf(tRelCenter.d[axis]);
            tClosestPoint = tRelCenter;
            tClosestPoint.d[axis] = sign * tHalfSize.d[axis];
            ptInfoOut->tNormal = pl_mul_vec3_scalarf(ptBox->tTransform.col[axis].xyz, -sign);
            ptInfoOut->tPoint = pl_mul_mat4_vec3(&ptBox->tTransform, tClosestPoint);
            return true;
        }
        plVec3 tClosestPointWorld = pl_mul_mat4_vec3(&ptBox->tTransform, tClosestPoint);
        ptInfoOut->fPenetration = ptSphere->fRadius - sqrtf(fDist);
        ptInfoOut->tNormal = pl_norm_vec3(pl_sub_vec3(tClosestPointWorld, ptSphere->tCenter)),
        ptInfoOut->tPoint = tClosestPointWorld;
    }

    return true;
}

// Convex descriptors borrow their vertex data; queries allocate no shape storage.
plVec3
pl_collision_support_convex(const plConvexShape* ptShape, plVec3 tDirection)
{
    plVec3 tLocal = {
        pl_dot_vec3(ptShape->tTransform.col[0].xyz, tDirection),
        pl_dot_vec3(ptShape->tTransform.col[1].xyz, tDirection),
        pl_dot_vec3(ptShape->tTransform.col[2].xyz, tDirection)
    };
    plVec3 tPoint = {0};
    switch(ptShape->tType)
    {
        case PL_CONVEX_SHAPE_BOX:
            for(uint32_t i = 0; i < 3; i++)
                tPoint.d[i] = tLocal.d[i] < 0 ? -ptShape->tHalfSize.d[i] : ptShape->tHalfSize.d[i];
            break;
        case PL_CONVEX_SHAPE_SPHERE:
        case PL_CONVEX_SHAPE_CAPSULE:
            tPoint = pl_mul_vec3_scalarf(pl_norm_vec3(tLocal), ptShape->fRadius);
            if(ptShape->tType == PL_CONVEX_SHAPE_CAPSULE)
                tPoint.y += tLocal.y < 0 ? -0.5f * ptShape->fHeight : 0.5f * ptShape->fHeight;
            break;
        case PL_CONVEX_SHAPE_CYLINDER:
        case PL_CONVEX_SHAPE_CONE:
        {
            float fLength = sqrtf(tLocal.x * tLocal.x + tLocal.z * tLocal.z);
            if(fLength > 0)
            {
                tPoint.x = ptShape->fRadius * tLocal.x / fLength;
                tPoint.z = ptShape->fRadius * tLocal.z / fLength;
            }
            if(ptShape->tType == PL_CONVEX_SHAPE_CYLINDER)
                tPoint.y = tLocal.y < 0 ? -0.5f * ptShape->fHeight : 0.5f * ptShape->fHeight;
            else
            {
                tPoint.y = -0.25f * ptShape->fHeight;
                plVec3 tTip = {0, 0.75f * ptShape->fHeight, 0};
                if(pl_dot_vec3(tTip, tLocal) > pl_dot_vec3(tPoint, tLocal))
                    tPoint = tTip;
            }
            break;
        }
        case PL_CONVEX_SHAPE_HULL:
        {
            float fBest = -FLT_MAX;
            for(uint32_t i = 0; i < ptShape->uVertexCount; i++)
            {
                float fDot = pl_dot_vec3(ptShape->atVertices[i], tLocal);
                if(fDot > fBest)
                {
                    fBest = fDot;
                    tPoint = ptShape->atVertices[i];
                }
            }
            break;
        }
    }
    return pl_mul_mat4_vec3(&ptShape->tTransform, tPoint);
}

plAABB
pl_collision_convex_bounds(const plConvexShape* ptShape)
{
    plAABB tBounds = {0};
    for(uint32_t i = 0; i < 3; i++)
    {
        plVec3 tAxis = {0};
        tAxis.d[i] = 1;
        plVec3 tMax = pl_collision_support_convex(ptShape, tAxis);
        plVec3 tMin = pl_collision_support_convex(ptShape, pl_mul_vec3_scalarf(tAxis, -1));
        tBounds.tMax.d[i] = tMax.d[i];
        tBounds.tMin.d[i] = tMin.d[i];
    }
    return tBounds;
}

static plVec3
pl__collision_support_convex(const void* pShape, plVec3 tDirection)
{
    return pl_collision_support_convex(pShape, tDirection);
}

bool
pl_collision_pen_convex_convex(const plConvexShape* a, const plConvexShape* b, plCollisionInfo* ptInfo)
{
    if((a->tType == PL_CONVEX_SHAPE_HULL && (!a->atVertices || !a->uVertexCount)) || (b->tType == PL_CONVEX_SHAPE_HULL && (!b->atVertices || !b->uVertexCount)))
        return false;
    if((a->tType == PL_CONVEX_SHAPE_SPHERE || a->tType == PL_CONVEX_SHAPE_CAPSULE) && (b->tType == PL_CONVEX_SHAPE_SPHERE || b->tType == PL_CONVEX_SHAPE_CAPSULE))
    {
        plVec3 da = pl_mul_vec3_scalarf(a->tTransform.col[1].xyz, a->tType == PL_CONVEX_SHAPE_CAPSULE ? a->fHeight : 0);
        plVec3 db = pl_mul_vec3_scalarf(b->tTransform.col[1].xyz, b->tType == PL_CONVEX_SHAPE_CAPSULE ? b->fHeight : 0);
        plVec3 p = pl_sub_vec3(a->tTransform.col[3].xyz, pl_mul_vec3_scalarf(da, 0.5f));
        plVec3 q = pl_sub_vec3(b->tTransform.col[3].xyz, pl_mul_vec3_scalarf(db, 0.5f));
        plVec3 r = pl_sub_vec3(p, q);
        float aa = pl_dot_vec3(da, da), bb = pl_dot_vec3(db, db), ab = pl_dot_vec3(da, db);
        float ar = pl_dot_vec3(da, r), br = pl_dot_vec3(db, r);
        float s = 0, t = 0;
        if(aa > 0)
        {
            float fDenom = aa * bb - ab * ab;
            s = fDenom > 1e-12f ? pl_clampf(0, (ab * br - ar * bb) / fDenom, 1) : 0;
        }
        if(bb > 0)
        {
            t = pl_clampf(0, (ab * s + br) / bb, 1);
            if(aa > 0)
                s = pl_clampf(0, (ab * t - ar) / aa, 1);
        }
        else if(aa > 0)
            s = pl_clampf(0, -ar / aa, 1);
        p = pl_add_vec3(p, pl_mul_vec3_scalarf(da, s));
        q = pl_add_vec3(q, pl_mul_vec3_scalarf(db, t));
        plVec3 tDelta = pl_sub_vec3(p, q);
        float fDistance = pl_length_vec3(tDelta);
        if(fDistance > a->fRadius + b->fRadius)
            return false;
        if(ptInfo)
        {
            plVec3 tNormal = fDistance > 1e-8f ? pl_mul_vec3_scalarf(tDelta, 1 / fDistance) : pl_norm_vec3(pl_cross_vec3(da, db));
            if(pl_length_sqr_vec3(tNormal) < 0.5f)
            {
                plVec3 tAxis = aa > 0 ? pl_norm_vec3(da) : bb > 0 ? pl_norm_vec3(db) : (plVec3){0, 1, 0};
                tNormal = pl_norm_vec3(pl_cross_vec3(tAxis, fabsf(tAxis.y) < 0.9f ? (plVec3){0, 1, 0} : (plVec3){0, 0, 1}));
            }
            *ptInfo = (plCollisionInfo){
                .tNormal      = tNormal,
                .fPenetration = a->fRadius + b->fRadius - fDistance,
                .tPoint       = pl_mul_vec3_scalarf(pl_add_vec3(pl_sub_vec3(p, pl_mul_vec3_scalarf(tNormal, a->fRadius)), pl_add_vec3(q, pl_mul_vec3_scalarf(tNormal, b->fRadius))), 0.5f)};
        }
        return true;
    }
    plAABB tA = pl_collision_convex_bounds(a);
    plAABB tB = pl_collision_convex_bounds(b);
    if(!pl_collision_aabb_aabb(&tA, &tB))
        return false;
    plGjkCollisionInfo tInfo = {0};
    if(!gptGjk->pen(pl__collision_support_convex, a, pl__collision_support_convex, b, ptInfo ? &tInfo : NULL))
        return false;
    if(ptInfo)
    {
        // EPA's outward Minkowski normal is the direction to move B, not A.
        *ptInfo = (plCollisionInfo){
            .fPenetration = tInfo.fPenetration,
            .tNormal      = pl_mul_vec3_scalarf(tInfo.tNormal, -1),
            .tPoint       = tInfo.tPoint
        };
        if(pl_length_sqr_vec3(ptInfo->tNormal) < 0.5f)
            return false;
    }
    return true;
}

plVec3
pl_collision_point_closest_point_triangle(plVec3 p, plVec3 a, plVec3 b, plVec3 c)
{
    plVec3 ab = pl_sub_vec3(b, a), ac = pl_sub_vec3(c, a), ap = pl_sub_vec3(p, a);
    if(pl_length_sqr_vec3(pl_cross_vec3(ab, ac)) < 1e-16f)
    {
        plVec3 tBest = a;
        plVec3 atPoints[3] = {a, b, c};
        for(int i = 0; i < 3; i++)
        {
            float fT;
            plVec3 tPoint = pl_collision_point_closest_point_line_segment(p, atPoints[i], atPoints[(i + 1) % 3], &fT);
            if(pl_length_sqr_vec3(pl_sub_vec3(p, tPoint)) < pl_length_sqr_vec3(pl_sub_vec3(p, tBest)))
                tBest = tPoint;
        }
        return tBest;
    }
    float d1 = pl_dot_vec3(ab, ap), d2 = pl_dot_vec3(ac, ap);
    if(d1 <= 0 && d2 <= 0)
        return a;
    plVec3 bp = pl_sub_vec3(p, b);
    float d3 = pl_dot_vec3(ab, bp), d4 = pl_dot_vec3(ac, bp);
    if(d3 >= 0 && d4 <= d3)
        return b;
    float vc = d1 * d4 - d3 * d2;
    if(vc <= 0 && d1 >= 0 && d3 <= 0)
        return pl_add_vec3(a, pl_mul_vec3_scalarf(ab, d1 / (d1 - d3)));
    plVec3 cp = pl_sub_vec3(p, c);
    float d5 = pl_dot_vec3(ab, cp), d6 = pl_dot_vec3(ac, cp);
    if(d6 >= 0 && d5 <= d6)
        return c;
    float vb = d5 * d2 - d1 * d6;
    if(vb <= 0 && d2 >= 0 && d6 <= 0)
        return pl_add_vec3(a, pl_mul_vec3_scalarf(ac, d2 / (d2 - d6)));
    float va = d3 * d6 - d5 * d4;
    if(va <= 0 && d4 >= d3 && d5 >= d6)
        return pl_add_vec3(b, pl_mul_vec3_scalarf(pl_sub_vec3(c, b), (d4 - d3) / ((d4 - d3) + (d5 - d6))));
    float fInv = 1.0f / (va + vb + vc);
    return pl_add_vec3(a, pl_add_vec3(pl_mul_vec3_scalarf(ab, vb * fInv), pl_mul_vec3_scalarf(ac, vc * fInv)));
}

bool
pl_collision_pen_convex_triangle(const plConvexShape* ptShape, plVec3 a, plVec3 b, plVec3 c, plCollisionInfo* ptInfo)
{
    plVec3 tNormal = pl_cross_vec3(pl_sub_vec3(b, a), pl_sub_vec3(c, a));
    if(pl_length_sqr_vec3(tNormal) < 1e-16f)
        return false;
    if(ptShape->tType == PL_CONVEX_SHAPE_SPHERE)
    {
        plVec3 tCenter = ptShape->tTransform.col[3].xyz;
        plVec3 tPoint = pl_collision_point_closest_point_triangle(tCenter, a, b, c);
        plVec3 tDelta = pl_sub_vec3(tCenter, tPoint);
        float fDistance = pl_length_vec3(tDelta);
        if(fDistance > ptShape->fRadius)
            return false;
        if(ptInfo)
            *ptInfo = (plCollisionInfo){
                .tPoint       = tPoint,
                .tNormal      = fDistance > 1e-8f ? pl_mul_vec3_scalarf(tDelta, 1.0f / fDistance) : pl_norm_vec3(tNormal),
                .fPenetration = ptShape->fRadius - fDistance};
        return true;
    }
    plVec3 atVertices[] = {a, b, c};
    plConvexShape tTriangle = {
        .tType        = PL_CONVEX_SHAPE_HULL,
        .tTransform   = pl_identity_mat4(),
        .atVertices   = atVertices,
        .uVertexCount = 3
    };
    return pl_collision_pen_convex_convex(ptShape, &tTriangle, ptInfo);
}

bool
pl_collision_pen_convex_half_space(const plConvexShape* ptShape, const plPlane* ptPlane, plCollisionInfo* ptInfo)
{
    float fLength = pl_length_vec3(ptPlane->tDirection);
    if(fLength <= 0)
        return false;
    plVec3 tNormal = pl_mul_vec3_scalarf(ptPlane->tDirection, 1.0f / fLength);
    plVec3 tPoint = pl_collision_support_convex(ptShape, pl_mul_vec3_scalarf(tNormal, -1));
    float fDepth = ptPlane->fOffset / fLength - pl_dot_vec3(tPoint, tNormal);
    if(fDepth < 0)
        return false;
    if(ptInfo)
        *ptInfo = (plCollisionInfo){.tPoint = pl_add_vec3(tPoint, pl_mul_vec3_scalarf(tNormal, fDepth)), .tNormal = tNormal, .fPenetration = fDepth};
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_collision_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCollisionI tApi = {
        .intersect_ray_plane              = pl_collision_intersect_ray_plane,
        .intersect_line_segment_plane     = pl_collision_intersect_line_segment_plane,
        .intersect_line_segment_cylinder  = pl_collision_intersect_line_segment_cylinder,
        .point_closest_point_plane        = pl_collision_point_closest_point_plane,
        .point_closest_point_line_segment = pl_collision_point_closest_point_line_segment,
        .point_closest_point_aabb         = pl_collision_point_closest_point_line_aabb,
        .aabb_aabb                        = pl_collision_aabb_aabb,
        .sphere_sphere                    = pl_collision_sphere_sphere,
        .box_box                          = pl_collision_box_box,
        .box_half_space                   = pl_collision_box_half_space,
        .sphere_half_space                = pl_collision_sphere_half_space,
        .box_sphere                       = pl_collision_box_sphere,
        .pen_sphere_sphere                = pl_collision_pen_sphere_sphere,
        .pen_box_box                      = pl_collision_pen_box_box,
        .pen_box_sphere                   = pl_collision_pen_box_sphere,
        .support_convex                   = pl_collision_support_convex,
        .convex_bounds                    = pl_collision_convex_bounds,
        .pen_convex_convex                = pl_collision_pen_convex_convex,
        .pen_convex_triangle              = pl_collision_pen_convex_triangle,
        .pen_convex_half_space            = pl_collision_pen_convex_half_space,
        .point_closest_point_triangle     = pl_collision_point_closest_point_triangle,
    };
    pl_set_api(ptApiRegistry, plCollisionI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
    #ifndef PL_UNITY_BUILD
    gptGjk = pl_get_api_latest(ptApiRegistry, plGjkI);
    #endif
}

void
pl_unload_collision_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plCollisionI* ptApi = pl_get_api_latest(ptApiRegistry, plCollisionI);
    ptApiRegistry->remove_api(ptApi);
}
