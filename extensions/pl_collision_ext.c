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

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#ifdef PL_UNITY_BUILD
    #include "pl_unity_ext.inc"
#else
    static const plMemoryI*  gptMemory = NULL;
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

plVec3
pl_collision_point_closest_point_plane(plVec3 tPoint, const plCollisionPlane* ptPlane)
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
pl_collision_sphere_sphere(const plCollisionSphere* ptSphere0, const plCollisionSphere* ptSphere1)
{
    // find the vector between the objects
    const plVec3 tMidLine = pl_sub_vec3(ptSphere0->tCenter, ptSphere1->tCenter);

    // see if it is large enough
    return pl_length_sqr_vec3(tMidLine) < (ptSphere0->fRadius + ptSphere1->fRadius) * (ptSphere0->fRadius + ptSphere1->fRadius);
}

static float
pl__collision_transform_to_axis(const plCollisionBox* ptBox, const plVec3* ptAxis)
{
    return ptBox->tHalfSize.x * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[0].xyz)) + 
        ptBox->tHalfSize.y * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[1].xyz)) + 
        ptBox->tHalfSize.z * fabsf(pl_dot_vec3(*ptAxis, ptBox->tTransform.col[2].xyz));
}

static bool
pl__collision_overlap_on_axis(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1, plVec3 tAxis, plVec3 tToCenter)
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
pl_collision_box_box(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1)
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
pl_collision_box_half_space(const plCollisionBox* ptBox, const plCollisionPlane* ptPlane)
{
    float fProjectedRadius = pl__collision_transform_to_axis(ptBox, &ptPlane->tDirection);
    float fBoxDistance = pl_dot_vec3(ptPlane->tDirection, ptBox->tTransform.col[3].xyz) - fProjectedRadius;
    return fBoxDistance <= ptPlane->fOffset;
}

bool
pl_collision_sphere_half_space(const plCollisionSphere* ptSphere, const plCollisionPlane* ptPlane)
{
    float fRadius = ptSphere->fRadius;
    float fBallDistance = pl_dot_vec3(ptPlane->tDirection, ptSphere->tCenter) - fRadius;
    return fBallDistance <= ptPlane->fOffset;
}

bool
pl_collision_box_sphere(const plCollisionBox* ptBox, const plCollisionSphere* ptSphere)
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
pl_collision_pen_sphere_sphere(const plCollisionSphere* ptSphere0, const plCollisionSphere* ptSphere1, plCollisionInfo* ptInfoOut)
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
pl__collision_penetration_on_axis(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1, const plVec3* ptAxis, const plVec3* ptToCenter)
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
pl__collision_try_axis(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1, plVec3 tAxis,
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
pl__collision_fill_point_face_box_box(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1,
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
pl_collision_pen_box_box(const plCollisionBox* ptBox0, const plCollisionBox* ptBox1, plCollisionInfo* ptInfoOut)
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
pl_collision_pen_box_sphere(const plCollisionBox* ptBox, const plCollisionSphere* ptSphere, plCollisionInfo* ptInfoOut)
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
        plVec3 tClosestPointWorld = pl_mul_mat4_vec3(&ptBox->tTransform, tClosestPoint);
        ptInfoOut->fPenetration = ptSphere->fRadius - sqrtf(fDist);
        ptInfoOut->tNormal = pl_norm_vec3(pl_sub_vec3(tClosestPointWorld, ptSphere->tCenter)),
        ptInfoOut->tPoint = tClosestPointWorld;
    }

    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_collision_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plCollisionI tApi = {
        .point_closest_point_plane        = pl_collision_point_closest_point_plane,
        .point_closest_point_line_segment = pl_collision_point_closest_point_line_segment,
        .point_closest_point_aabb         = pl_collision_point_closest_point_line_aabb,
        .sphere_sphere                    = pl_collision_sphere_sphere,
        .box_box                          = pl_collision_box_box,
        .box_half_space                   = pl_collision_box_half_space,
        .sphere_half_space                = pl_collision_sphere_half_space,
        .box_sphere                       = pl_collision_box_sphere,
        .pen_sphere_sphere                = pl_collision_pen_sphere_sphere,
        .pen_box_box                      = pl_collision_pen_box_box,
        .pen_box_sphere                   = pl_collision_pen_box_sphere,
    };
    pl_set_api(ptApiRegistry, plCollisionI, &tApi);

    gptMemory = pl_get_api_latest(ptApiRegistry, plMemoryI);
}

PL_EXPORT void
pl_unload_collision_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plCollisionI* ptApi = pl_get_api_latest(ptApiRegistry, plCollisionI);
    ptApiRegistry->remove_api(ptApi);
}
