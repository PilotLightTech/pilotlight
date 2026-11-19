/*
   pl_collision_ext.h
     - intersection, closest point, & penetration tests for simple shapes
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*
    Implementation:
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plGjkI (v0.1.x)
        * plMemoryI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_COLLISION_EXT_H
#define PL_COLLISION_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plCollisionI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plCollisionInfo plCollisionInfo;
typedef struct _plConvexShape   plConvexShape;

// enums
typedef int plConvexShapeType;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_collision_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_collision_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plCollisionI
{
    // intersections
    bool (*intersect_ray_plane)            (plVec3 point, plVec3 dir, const plPlane*, plVec3* intersectionPointOut);
    bool (*intersect_line_segment_plane)   (plVec3 a, plVec3 b, const plPlane*, plVec3* intersectionPointOut);
    bool (*intersect_line_segment_cylinder)(plVec3 a, plVec3 b, const plCylinder*, float*); // float out: segment parameter (0 at a, 1 at b)
    
    // closest point
    plVec3 (*point_closest_point_plane)       (plVec3, const plPlane*);
    plVec3 (*point_closest_point_line_segment)(plVec3, plVec3, plVec3, float*);
    plVec3 (*point_closest_point_aabb)        (plVec3, plAABB);
    plVec3 (*point_closest_point_triangle)    (plVec3 point, plVec3 a, plVec3 b, plVec3 c);

    // simple collision
    bool (*aabb_aabb)        (const plAABB*, const plAABB*);
    bool (*sphere_sphere)    (const plSphere*, const plSphere*);
    bool (*box_box)          (const plBox*, const plBox*);
    bool (*box_sphere)       (const plBox*, const plSphere*);
    bool (*box_half_space)   (const plBox*, const plPlane*);
    bool (*sphere_half_space)(const plSphere*, const plPlane*);

    // penetration tests ("pen_"): also fill out contact information
    bool (*pen_sphere_sphere)    (const plSphere*, const plSphere*, plCollisionInfo* infoOut);
    bool (*pen_box_box)          (const plBox*, const plBox*, plCollisionInfo* infoOut);
    bool (*pen_box_sphere)       (const plBox*, const plSphere*, plCollisionInfo* infoOut);
    bool (*pen_convex_convex)    (const plConvexShape*, const plConvexShape*, plCollisionInfo*);
    bool (*pen_convex_triangle)  (const plConvexShape*, plVec3 a, plVec3 b, plVec3 c, plCollisionInfo*);
    bool (*pen_convex_half_space)(const plConvexShape*, const plPlane*, plCollisionInfo*);

    // Convex queries. Transform must be rigid (rotation/translation only).
    // Contact normal points from shape B toward A; moving A along it separates.
    plVec3 (*support_convex) (const plConvexShape*, plVec3 direction);
    plAABB (*convex_bounds)  (const plConvexShape*);
    
} plCollisionI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCollisionInfo
{
    float  fPenetration;
    plVec3 tNormal;
    plVec3 tPoint;
    bool   bFlip; // true if the shapes were swapped internally (box/box: vertex of 2nd on face of 1st)
} plCollisionInfo;

enum _plConvexShapeType
{
    PL_CONVEX_SHAPE_BOX = 0,
    PL_CONVEX_SHAPE_SPHERE,
    PL_CONVEX_SHAPE_CAPSULE,
    PL_CONVEX_SHAPE_CYLINDER,
    PL_CONVEX_SHAPE_CONE,
    PL_CONVEX_SHAPE_HULL
};

typedef struct _plConvexShape
{
    plConvexShapeType tType;
    plMat4            tTransform;
    plVec3            tHalfSize;
    float             fRadius;
    float             fHeight; // +Y axis; capsule: distance between cap centers, others: full height

    // Cone is centered at its mass center: base y=-height/4, tip y=3*height/4.
    const plVec3* atVertices; // hull of these local-space points; borrowed for the call
    uint32_t      uVertexCount;
} plConvexShape;

#ifdef __cplusplus
}
#endif

#endif // PL_COLLISION_EXT_H