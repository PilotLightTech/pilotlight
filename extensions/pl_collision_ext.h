/*
   pl_collision_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
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

#define plCollisionI_version {0, 2, 0}

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

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_collision_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_collision_ext(plApiRegistryI*, bool reload);

// intersection
PL_API bool pl_collision_intersect_ray_plane            (plVec3 point, plVec3 dir, const plPlane*, plVec3* intersectionPointOut);
PL_API bool pl_collision_intersect_line_segment_plane   (plVec3 a, plVec3 b, const plPlane*, plVec3* intersectionPointOut);
PL_API bool pl_collision_intersect_line_segment_cylinder(plVec3 a, plVec3 b, const plCylinder*, float*);

// closest point
PL_API plVec3 pl_collision_point_closest_point_plane       (plVec3, const plPlane*);
PL_API plVec3 pl_collision_point_closest_point_line_segment(plVec3, plVec3, plVec3, float*);
PL_API plVec3 pl_collision_point_closest_point_aabb        (plVec3, plAABB);

// collision only
PL_API bool pl_collision_aabb_aabb        (const plAABB*, const plAABB*);
PL_API bool pl_collision_sphere_sphere    (const plSphere*, const plSphere*);
PL_API bool pl_collision_box_box          (const plBox*, const plBox*);
PL_API bool pl_collision_box_sphere       (const plBox*, const plSphere*);
PL_API bool pl_collision_box_half_space   (const plBox*, const plPlane*);
PL_API bool pl_collision_sphere_half_space(const plSphere*, const plPlane*);

// collision & penetration
PL_API bool pl_collision_pen_sphere_sphere(const plSphere*, const plSphere*, plCollisionInfo* infoOut);
PL_API bool pl_collision_pen_box_box      (const plBox*, const plBox*, plCollisionInfo* infoOut);
PL_API bool pl_collision_pen_box_sphere   (const plBox*, const plSphere*, plCollisionInfo* infoOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plCollisionI
{
    bool   (*intersect_ray_plane)            (plVec3 point, plVec3 dir, const plPlane*, plVec3* intersectionPointOut);
    bool   (*intersect_line_segment_plane)   (plVec3 a, plVec3 b, const plPlane*, plVec3* intersectionPointOut);
    bool   (*intersect_line_segment_cylinder)(plVec3 a, plVec3 b, const plCylinder*, float*);
    plVec3 (*point_closest_point_plane)       (plVec3, const plPlane*);
    plVec3 (*point_closest_point_line_segment)(plVec3, plVec3, plVec3, float*);
    plVec3 (*point_closest_point_aabb)        (plVec3, plAABB);
    bool   (*aabb_aabb)                       (const plAABB*, const plAABB*);
    bool   (*sphere_sphere)                   (const plSphere*, const plSphere*);
    bool   (*box_box)                         (const plBox*, const plBox*);
    bool   (*box_sphere)                      (const plBox*, const plSphere*);
    bool   (*box_half_space)                  (const plBox*, const plPlane*);
    bool   (*sphere_half_space)               (const plSphere*, const plPlane*);
    bool   (*pen_sphere_sphere)               (const plSphere*, const plSphere*, plCollisionInfo* infoOut);
    bool   (*pen_box_box)                     (const plBox*, const plBox*, plCollisionInfo* infoOut);
    bool   (*pen_box_sphere)                  (const plBox*, const plSphere*, plCollisionInfo* infoOut);
} plCollisionI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCollisionInfo
{
    float  fPenetration;
    plVec3 tNormal;
    plVec3 tPoint;
    bool   bFlip;
} plCollisionInfo;

#ifdef __cplusplus
}
#endif

#endif // PL_COLLISION_EXT_H