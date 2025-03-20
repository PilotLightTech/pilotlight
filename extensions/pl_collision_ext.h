/*
   pl_collision_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_COLLISION_EXT_H
#define PL_COLLISION_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plCollisionI_version (plVersion){0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plCollisionPlane  plCollisionPlane;
typedef struct _plCollisionSphere plCollisionSphere;
typedef struct _plCollisionBox    plCollisionBox;
typedef struct _plCollisionInfo   plCollisionInfo;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plCollisionI
{

    // collision only
    bool (*sphere_sphere)    (const plCollisionSphere*, const plCollisionSphere*);
    bool (*box_box)          (const plCollisionBox*, const plCollisionBox*);
    bool (*box_sphere)       (const plCollisionBox*, const plCollisionSphere*);
    bool (*box_half_space)   (const plCollisionBox*, const plCollisionPlane*);
    bool (*sphere_half_space)(const plCollisionSphere*, const plCollisionPlane*);

    // collision & penetration
    bool (*pen_sphere_sphere)(const plCollisionSphere*, const plCollisionSphere*, plCollisionInfo* infoOut);
    bool (*pen_box_box)      (const plCollisionBox*, const plCollisionBox*, plCollisionInfo* infoOut);
    bool (*pen_box_sphere)   (const plCollisionBox*, const plCollisionSphere*, plCollisionInfo* infoOut);

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

typedef struct _plCollisionPlane
{
    float  fOffset;
    plVec3 tDirection;
} plCollisionPlane;

typedef struct _plCollisionSphere
{
    float  fRadius;
    plVec3 tCenter;
} plCollisionSphere;

typedef struct _plCollisionBox
{
    plMat4 tTransform;
    plVec3 tHalfSize;
} plCollisionBox;

#endif // PL_COLLISION_EXT_H