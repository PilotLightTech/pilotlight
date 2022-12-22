/*
   pl_math.inc, v0.2 (WIP)
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations & basic types
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MATH_INC
#define PL_MATH_INC

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef union  _plVec2 plVec2;
typedef union  _plVec3 plVec3;
typedef union  _plVec4 plVec4;
typedef union  _plMat4 plMat4;
typedef struct _plRect plRect;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union _plVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} plVec2;

typedef union _plVec3
{
    struct { float x, y, z; };
    struct { float r, g, b; };
    struct { float u, v, __; };
    struct { plVec2 xy; float ignore0_; };
    struct { plVec2 rg; float ignore1_; };
    struct { plVec2 uv; float ignore2_; };
    struct { float ignore3_; plVec2 yz; };
    struct { float ignore4_; plVec2 gb; };
    struct { float ignore5_; plVec2 v__; };
    float d[3];
} plVec3;

typedef union _plVec4
{
    struct
    {
        union
        {
            plVec3 xyz;
            struct{ float x, y, z;};
        };

        float w;
    };
    struct
    {
        union
        {
            plVec3 rgb;
            struct{ float r, g, b;};
        };
        float a;
    };
    struct
    {
        plVec2 xy;
        float ignored0_, ignored1_;
    };
    struct
    {
        float ignored2_;
        plVec2 yz;
        float ignored3_;
    };
    struct
    {
        float ignored4_, ignored5_;
        plVec2 zw;
    };
    float d[4];
} plVec4;

typedef union _plMat4
{
    plVec4 col[4];
    struct {
        float x11;
        float x21;
        float x31;
        float x41;
        float x12;
        float x22;
        float x32;
        float x42;
        float x13;
        float x23;
        float x33;
        float x43;
        float x14;
        float x24;
        float x34;
        float x44;
    };
    float d[16];
} plMat4;

typedef struct _plRect
{
    plVec2 tMin;
    plVec2 tMax;
} plRect;

#endif // PL_MATH_INC