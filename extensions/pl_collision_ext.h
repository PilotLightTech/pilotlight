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