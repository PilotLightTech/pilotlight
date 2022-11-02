/*
   pl_math.h, v0.1 (WIP)
*/

/*
Index of this file:
// [SECTION] forward declarations & basic types
// [SECTION] structs
*/

#ifndef PL_MATH_H
#define PL_MATH_H

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef union _plVec2 plVec2;
typedef union _plVec3 plVec3;
typedef union _plVec4 plVec4;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union _plVec2
{
    struct { float x, y; };
    struct { float u, v; };
} plVec2;

typedef union _plVec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
} plVec3;

typedef union _plVec4
{
    struct{ float x, y, z, w;};
    struct{ float r, g, b, a;};
} plVec4;

#endif // PL_MATH_H