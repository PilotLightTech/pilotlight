/*
   pl_math.h, v0.6 (WIP)

   Do this:
        #define PL_MATH_INCLUDE_FUNCTIONS
   before you include this file in *one* C or C++ file to create include math functions.
   // i.e. it should look like this:
   #include ...
   #include ...
   #include ...
   #define PL_MATH_INCLUDE_FUNCTIONS
   #include "pl_math.h"
*/

// library version
#define PL_MATH_VERSION    "0.6.4"
#define PL_MATH_VERSION_NUM 00604

/*
Index of this file:
// [SECTION] header mess
// [SECTION] forward declarations & basic types
// [SECTION] defines
// [SECTION] structs
// [SECTION] header file section
// [SECTION] includes
// [SECTION] general math
// [SECTION] vector ops
// [SECTION] matrix ops
// [SECTION] quaternion ops
// [SECTION] rect ops
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MATH_INC
#define PL_MATH_INC
#define PL_MATH_DEFINED

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// forward declarations
typedef union  _plVec2 plVec2;
typedef union  _plVec3 plVec3;
typedef union  _plVec4 plVec4;
typedef union  _plMat4 plMat4;
typedef struct _plRect plRect;
typedef struct _plAABB plAABB;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#define PL_E        2.71828182f // e
#define PL_LOG2E    1.44269504f // log2(e)
#define PL_LOG10E   0.43429448f // log10(e)
#define PL_LN2      0.69314718f // ln(2)
#define PL_LN10     2.30258509f // ln(10)
#define PL_PI       3.14159265f // pi
#define PL_2PI      6.28318530f // pi
#define PL_PI_2     1.57079632f // pi/2
#define PL_PI_3     1.04719755f // pi/3
#define PL_PI_4     0.78539816f // pi/4
#define PL_1_PI     0.31830988f // 1/pi
#define PL_2_PI     0.63661977f // 2/pi
#define PL_2_SQRTPI 1.12837916f // 2/sqrt(pi)
#define PL_SQRT2    1.41421356f // sqrt(2)
#define PL_SQRT1_2  0.70710678f // 1/sqrt(2)
#define PL_PI_D     3.1415926535897932 // pi

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

#ifndef PL_MATH_VEC2_DEFINED
#define PL_MATH_VEC2_DEFINED
typedef union _plVec2
{
    struct { float x, y; };
    struct { float r, g; };
    struct { float u, v; };
    float d[2];
} plVec2;
#endif // PL_MATH_VEC2_DEFINED

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

typedef struct _plAABB
{
    plVec3 tMin;
    plVec3 tMax;
} plAABB;

#endif // PL_MATH_INC

#if defined(PL_MATH_INCLUDE_FUNCTIONS) && !defined(PL_MATH_INCLUDE_FUNCTIONS_H)
#define PL_MATH_INCLUDE_FUNCTIONS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include <assert.h>

//-----------------------------------------------------------------------------
// [SECTION] helpers
//-----------------------------------------------------------------------------

#ifdef __cplusplus
    #define pl_create_vec2(XARG, YARG)                  {(XARG), (YARG)}
    #define pl_create_vec3(XARG, YARG, ZARG)            {(XARG), (YARG), (ZARG)}
    #define pl_create_vec4(XARG, YARG, ZARG, WARG)      {(XARG), (YARG), (ZARG), (WARG)}
    #define pl_create_mat4_diag(XARG, YARG, ZARG, WARG) {(XARG), 0.0, 0.0f, 0.0f, 0.0f, (YARG), 0.0f, 0.0f, 0.0f, 0.0f, (ZARG), 0.0f, 0.0f, 0.0f, 0.0f, (WARG)}
    #define pl_create_mat4_cols(XARG, YARG, ZARG, WARG) {(XARG).x, (XARG).y, (XARG).z, (XARG).w, (YARG).x, (YARG).y, (YARG).z, (YARG).w, (ZARG).x, (ZARG).y, (ZARG).z, (ZARG).w, (WARG).x, (WARG).y, (WARG).z, (WARG).w}
    #define pl_create_rect_vec2(XARG, YARG)             {(XARG), (YARG)}
    #define pl_create_rect(XARG, YARG, ZARG, WARG)      {{(XARG), (YARG)}, {(ZARG), (WARG)}}
#else
    #define pl_create_vec2(XARG, YARG)                  (plVec2){(XARG), (YARG)}
    #define pl_create_vec3(XARG, YARG, ZARG)            (plVec3){(XARG), (YARG), (ZARG)}
    #define pl_create_vec4(XARG, YARG, ZARG, WARG)      (plVec4){(XARG), (YARG), (ZARG), (WARG)}
    #define pl_create_mat4_diag(XARG, YARG, ZARG, WARG) (plMat4){.x11 = (XARG), .x22 = (YARG), .x33 = (ZARG), .x44 = (WARG)}
    #define pl_create_mat4_cols(XARG, YARG, ZARG, WARG) (plMat4){.col[0] = (XARG), .col[1] = (YARG), .col[2] = (ZARG), .col[3] = (WARG)}
    #define pl_create_rect_vec2(XARG, YARG)             (plRect){.tMin = (XARG), .tMax = (YARG)}
    #define pl_create_rect(XARG, YARG, ZARG, WARG)      (plRect){.tMin = {.x = (XARG), .y = (YARG)}, .tMax = {.x = (ZARG), .y = (WARG)}}
#endif

//-----------------------------------------------------------------------------
// [SECTION] general math
//-----------------------------------------------------------------------------

#define pl_max(Value1, Value2) ((Value1) > (Value2) ? (Value1) : (Value2))
#define pl_min(Value1, Value2) ((Value1) > (Value2) ? (Value2) : (Value1))
#define pl_square(Value)       ((Value) * (Value))
#define pl_cube(Value)         ((Value) * (Value) * (Value))

static inline float    pl_radiansf(float fDegrees)                          { return fDegrees * 0.0174532925f; }
static inline float    pl_degreesf(float fRadians)                          { return fRadians * 57.29577951f; }
static inline float    pl_maxf    (float fValue1, float fValue2)            { return fValue1 > fValue2 ? fValue1 : fValue2; }
static inline float    pl_minf    (float fValue1, float fValue2)            { return fValue1 > fValue2 ? fValue2 : fValue1; }
static inline int      pl_maxi    (int iValue1, int iValue2)                { return iValue1 > iValue2 ? iValue1 : iValue2; }
static inline int      pl_mini    (int iValue1, int iValue2)                { return iValue1 > iValue2 ? iValue2 : iValue1; }
static inline uint32_t pl_maxu    (uint32_t uValue1, uint32_t uValue2)      { return uValue1 > uValue2 ? uValue1 : uValue2; }
static inline uint32_t pl_minu    (uint32_t uValue1, uint32_t uValue2)      { return uValue1 > uValue2 ? uValue2 : uValue1; }
static inline double   pl_maxd    (double dValue1, double dValue2)          { return dValue1 > dValue2 ? dValue1 : dValue2; }
static inline double   pl_mind    (double dValue1, double dValue2)          { return dValue1 > dValue2 ? dValue2 : dValue1; }
static inline float    pl_squaref (float fValue)                            { return fValue * fValue;}
static inline float    pl_cubef   (float fValue)                            { return fValue * fValue * fValue;}
static inline int      pl_clampi  (int iMin, int iValue, int iMax)          { if (iValue < iMin) return iMin; else if (iValue > iMax) return iMax; return iValue; }
static inline float    pl_clampf  (float fMin, float fValue, float fMax)    { if (fValue < fMin) return fMin; else if (fValue > fMax) return fMax; return fValue; }
static inline double   pl_clampd  (double dMin, double dValue, double dMax) { if (dValue < dMin) return dMin; else if (dValue > dMax) return dMax; return dValue; }
static inline float    pl_clamp01f(float fValue)                            { return pl_clampf(0.0f, fValue, 1.0f); }
static inline double   pl_clamp01d(double dValue)                           { return pl_clampd(0.0, dValue, 1.0); }
static inline size_t   pl_align_up(size_t szValue, size_t szAlign)          { return ((szValue + (szAlign - 1)) & ~(szAlign - 1)); }

#define PL__ALIGN_UP(num, align) (((num) + ((align)-1)) & ~((align)-1))

//-----------------------------------------------------------------------------
// [SECTION] vector ops
//-----------------------------------------------------------------------------

// unary ops
static inline float  pl_length_sqr_vec2  (plVec2 tVec)                             { return pl_squaref(tVec.x) + pl_squaref(tVec.y); }
static inline float  pl_length_sqr_vec3  (plVec3 tVec)                             { return pl_squaref(tVec.x) + pl_squaref(tVec.y) + pl_squaref(tVec.z); }
static inline float  pl_length_sqr_vec4  (plVec4 tVec)                             { return pl_squaref(tVec.x) + pl_squaref(tVec.y) + pl_squaref(tVec.z) + pl_squaref(tVec.w); }
static inline float  pl_length_vec2      (plVec2 tVec)                             { return sqrtf(pl_length_sqr_vec2(tVec)); }
static inline float  pl_length_vec3      (plVec3 tVec)                             { return sqrtf(pl_length_sqr_vec3(tVec)); }
static inline float  pl_length_vec4      (plVec4 tVec)                             { return sqrtf(pl_length_sqr_vec4(tVec)); }
static inline plVec2 pl_floor_vec2       (plVec2 tVec)                             { return pl_create_vec2(floorf(tVec.x), floorf(tVec.y));}
static inline plVec3 pl_floor_vec3       (plVec3 tVec)                             { return pl_create_vec3(floorf(tVec.x), floorf(tVec.y), floorf(tVec.z));}
static inline plVec4 pl_floor_vec4       (plVec4 tVec)                             { return pl_create_vec4(floorf(tVec.x), floorf(tVec.y), floorf(tVec.z), floorf(tVec.w));}
static inline plVec2 pl_clamp_vec2       (plVec2 tMin, plVec2 tValue, plVec2 tMax) { return pl_create_vec2(pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y));}
static inline plVec3 pl_clamp_vec3       (plVec3 tMin, plVec3 tValue, plVec3 tMax) { return pl_create_vec3(pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y), pl_clampf(tMax.z, tValue.z, tMax.z));}
static inline plVec4 pl_clamp_vec4       (plVec4 tMin, plVec4 tValue, plVec4 tMax) { return pl_create_vec4(pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y), pl_clampf(tMax.z, tValue.z, tMax.z), pl_clampf(tMax.w, tValue.w, tMax.w));}
static inline plVec2 pl_min_vec2        (plVec2 tValue0, plVec2 tValue1)           { return pl_create_vec2(pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y));}
static inline plVec3 pl_min_vec3        (plVec3 tValue0, plVec3 tValue1)           { return pl_create_vec3(pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y), pl_minf(tValue0.z, tValue1.z));}
static inline plVec4 pl_min_vec4        (plVec4 tValue0, plVec4 tValue1)           { return pl_create_vec4(pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y), pl_minf(tValue0.z, tValue1.z), pl_minf(tValue0.w, tValue1.w));}
static inline plVec2 pl_max_vec2        (plVec2 tValue0, plVec2 tValue1)           { return pl_create_vec2(pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y));}
static inline plVec3 pl_max_vec3        (plVec3 tValue0, plVec3 tValue1)           { return pl_create_vec3(pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y), pl_maxf(tValue0.z, tValue1.z));}
static inline plVec4 pl_max_vec4        (plVec4 tValue0, plVec4 tValue1)           { return pl_create_vec4(pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y), pl_maxf(tValue0.z, tValue1.z), pl_maxf(tValue0.w, tValue1.w));}

// binary ops
static inline float  pl_dot_vec2        (plVec2 tVec1, plVec2 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y; }
static inline float  pl_dot_vec3        (plVec3 tVec1, plVec3 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y + tVec1.z * tVec2.z; }
static inline float  pl_dot_vec4        (plVec4 tVec1, plVec4 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y + tVec1.z * tVec2.z + tVec1.w * tVec2.w; }
static inline plVec3 pl_cross_vec3      (plVec3 tVec1, plVec3 tVec2) { return pl_create_vec3(tVec1.y * tVec2.z - tVec2.y * tVec1.z, tVec1.z * tVec2.x - tVec2.z * tVec1.x, tVec1.x * tVec2.y - tVec2.x * tVec1.y); }

static inline plVec2 pl_add_vec2        (plVec2 tVec1, plVec2 tVec2) { return pl_create_vec2(tVec1.x + tVec2.x, tVec1.y + tVec2.y); }
static inline plVec3 pl_add_vec3        (plVec3 tVec1, plVec3 tVec2) { return pl_create_vec3(tVec1.x + tVec2.x, tVec1.y + tVec2.y, tVec1.z + tVec2.z); }
static inline plVec4 pl_add_vec4        (plVec4 tVec1, plVec4 tVec2) { return pl_create_vec4(tVec1.x + tVec2.x, tVec1.y + tVec2.y, tVec1.z + tVec2.z, tVec1.w + tVec2.w); }

static inline plVec2 pl_sub_vec2        (plVec2 tVec1, plVec2 tVec2) { return pl_create_vec2(tVec1.x - tVec2.x, tVec1.y - tVec2.y); }
static inline plVec3 pl_sub_vec3        (plVec3 tVec1, plVec3 tVec2) { return pl_create_vec3(tVec1.x - tVec2.x, tVec1.y - tVec2.y, tVec1.z - tVec2.z); }
static inline plVec4 pl_sub_vec4        (plVec4 tVec1, plVec4 tVec2) { return pl_create_vec4(tVec1.x - tVec2.x, tVec1.y - tVec2.y, tVec1.z - tVec2.z, tVec1.w - tVec2.w) ;}

static inline plVec2 pl_mul_vec2        (plVec2 tVec1, plVec2 tVec2) { return pl_create_vec2(tVec1.x * tVec2.x, tVec1.y * tVec2.y); }
static inline plVec3 pl_mul_vec3        (plVec3 tVec1, plVec3 tVec2) { return pl_create_vec3(tVec1.x * tVec2.x, tVec1.y * tVec2.y, tVec1.z * tVec2.z); }
static inline plVec4 pl_mul_vec4        (plVec4 tVec1, plVec4 tVec2) { return pl_create_vec4(tVec1.x * tVec2.x, tVec1.y * tVec2.y, tVec1.z * tVec2.z, tVec1.w * tVec2.w); }

static inline plVec2 pl_div_vec2        (plVec2 tVec1, plVec2 tVec2) { return pl_create_vec2(tVec1.x / tVec2.x, tVec1.y / tVec2.y); }
static inline plVec3 pl_div_vec3        (plVec3 tVec1, plVec3 tVec2) { return pl_create_vec3(tVec1.x / tVec2.x, tVec1.y / tVec2.y, tVec1.z / tVec2.z); }
static inline plVec4 pl_div_vec4        (plVec4 tVec1, plVec4 tVec2) { return pl_create_vec4(tVec1.x / tVec2.x, tVec1.y / tVec2.y, tVec1.z / tVec2.z, tVec1.w / tVec2.w); }

static inline plVec2 pl_mul_vec2_scalarf(plVec2 tVec, float fValue)  { return pl_create_vec2(fValue * tVec.x, fValue * tVec.y); }
static inline plVec3 pl_mul_vec3_scalarf(plVec3 tVec, float fValue)  { return pl_create_vec3(fValue * tVec.x, fValue * tVec.y, fValue * tVec.z); }
static inline plVec4 pl_mul_vec4_scalarf(plVec4 tVec, float fValue)  { return pl_create_vec4(fValue * tVec.x, fValue * tVec.y, fValue * tVec.z, fValue * tVec.w); }

static inline plVec2 pl_div_vec2_scalarf(plVec2 tVec, float fValue)  { return pl_create_vec2(tVec.x / fValue, tVec.y / fValue); }
static inline plVec3 pl_div_vec3_scalarf(plVec3 tVec, float fValue)  { return pl_create_vec3(tVec.x / fValue, tVec.y / fValue, tVec.z / fValue); }
static inline plVec4 pl_div_vec4_scalarf(plVec4 tVec, float fValue)  { return pl_create_vec4(tVec.x / fValue, tVec.y / fValue, tVec.z / fValue, tVec.w / fValue); }

static inline plVec2 pl_div_scalarf_vec2(float fValue, plVec2 tVec)  { return pl_create_vec2(fValue / tVec.x, fValue / tVec.y); }
static inline plVec3 pl_div_scalarf_vec3(float fValue, plVec3 tVec)  { return pl_create_vec3(fValue / tVec.x, fValue / tVec.y, fValue / tVec.z); }
static inline plVec4 pl_div_scalarf_vec4(float fValue, plVec4 tVec)  { return pl_create_vec4(fValue / tVec.x, fValue / tVec.y, fValue / tVec.z, fValue / tVec.w); }

static inline plVec2 pl_norm_vec2       (plVec2 tVec)                { float fLength = pl_length_vec2(tVec); if(fLength > 0) fLength = 1.0f / fLength; return pl_mul_vec2_scalarf(tVec, fLength); }
static inline plVec3 pl_norm_vec3       (plVec3 tVec)                { float fLength = pl_length_vec3(tVec); if(fLength > 0) fLength = 1.0f / fLength; return pl_mul_vec3_scalarf(tVec, fLength); }
static inline plVec4 pl_norm_vec4       (plVec4 tVec)                { float fLength = pl_length_vec4(tVec); if(fLength > 0) fLength = 1.0f / fLength; return pl_mul_vec4_scalarf(tVec, fLength); }

//-----------------------------------------------------------------------------
// [SECTION] matrix ops
//-----------------------------------------------------------------------------

// general ops
static inline float  pl_mat4_get                  (const plMat4* ptMat, int iRow, int iCol)         { return ptMat->col[iCol].d[iRow];}
static inline void   pl_mat4_set                  (plMat4* ptMat, int iRow, int iCol, float fValue) { ptMat->col[iCol].d[iRow] = fValue;}
static inline plMat4 pl_identity_mat4             (void)                                            { return pl_create_mat4_diag(1.0f, 1.0f, 1.0f, 1.0f);}
static inline plMat4 pl_mat4_transpose            (const plMat4* ptMat)                             { plMat4 tResult = {0}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) pl_mat4_set(&tResult, i, j, pl_mat4_get(ptMat, j, i)); return tResult;}
static inline plMat4 pl_mat4_invert               (const plMat4* ptMat);
static inline plMat4 pl_mul_scalarf_mat4          (float fLeft, const plMat4* ptRight)              { plMat4 tResult = {0}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) pl_mat4_set(&tResult, i, j, fLeft * pl_mat4_get(ptRight, j, i)); return tResult;}
static inline plVec3 pl_mul_mat4_vec3             (const plMat4* ptLeft, plVec3 tRight);
static inline plVec4 pl_mul_mat4_vec4             (const plMat4* ptLeft, plVec4 tRight);
static inline plMat4 pl_mul_mat4                  (const plMat4* ptLeft, const plMat4* ptRight);

// translation, rotation, scaling
static inline plMat4 pl_mat4_translate_xyz        (float fX, float fY, float fZ)               { plMat4 tResult = pl_create_mat4_diag(1.0f, 1.0f, 1.0f, 1.0f); tResult.x14 = fX; tResult.x24 = fY; tResult.x34 = fZ; return tResult;}
static inline plMat4 pl_mat4_translate_vec3       (plVec3 tVec)                                { return pl_mat4_translate_xyz(tVec.x, tVec.y, tVec.z);}
static inline plMat4 pl_mat4_rotate_vec3          (float fAngle, plVec3 tVec);
static inline plMat4 pl_mat4_rotate_xyz           (float fAngle, float fX, float fY, float fZ) { return pl_mat4_rotate_vec3(fAngle, pl_create_vec3(fX, fY, fZ));}
static inline plMat4 pl_mat4_scale_xyz            (float fX, float fY, float fZ)               { return pl_create_mat4_diag(fX, fY, fZ, 1.0f);}
static inline plMat4 pl_mat4_scale_vec3           (plVec3 tVec)                                { return pl_mat4_scale_xyz(tVec.x, tVec.y, tVec.z);}
static inline plMat4 pl_mat4_rotate_quat          (plVec4 tQ);
static inline plMat4 pl_rotation_translation_scale(plVec4 tQ, plVec3 tV, plVec3 tS);

// transforms (optimized for orthogonal matrices)
static inline plMat4 pl_mat4t_invert              (const plMat4* ptMat);
static inline plMat4 pl_mul_mat4t                 (const plMat4* ptLeft, const plMat4* ptRight);

//-----------------------------------------------------------------------------
// [SECTION] quaternion ops
//-----------------------------------------------------------------------------

static inline plVec4 pl_mul_quat                 (plVec4 tQ1, plVec4 tQ2)                      { return pl_create_vec4(tQ1.w * tQ2.x + tQ1.x * tQ2.w + tQ1.y * tQ2.z - tQ1.z * tQ2.y, tQ1.w * tQ2.y - tQ1.x * tQ2.z + tQ1.y * tQ2.w + tQ1.z * tQ2.x, tQ1.w * tQ2.z + tQ1.x * tQ2.y - tQ1.y * tQ2.x + tQ1.z * tQ2.w, tQ1.w * tQ2.w - tQ1.x * tQ2.x - tQ1.y * tQ2.y - tQ1.z * tQ2.z);}
static inline plVec4 pl_quat_rotation_normal     (float fAngle, float fX, float fY, float fZ)  { const float fSin2 = sinf(0.5f * fAngle); return pl_create_vec4(fSin2 * fX, fSin2 * fY, fSin2 * fZ, cosf(0.5f * fAngle));}
static inline plVec4 pl_quat_rotation_normal_vec3(float fAngle, plVec3 tNormalAxis)            { return pl_quat_rotation_normal(fAngle, tNormalAxis.x, tNormalAxis.y, tNormalAxis.z);}
static inline plVec4 pl_norm_quat                (plVec4 tQ)                                   { const plVec3 tNorm = pl_norm_vec3(tQ.xyz); return pl_create_vec4(tNorm.x, tNorm.y, tNorm.z, tQ.w);}
static inline plVec4 pl_quat_slerp               (plVec4 tQ1, plVec4 tQ2, float fT);
static inline void   pl_decompose_matrix         (const plMat4* ptM, plVec3* ptS, plVec4* ptQ, plVec3* ptT);

//-----------------------------------------------------------------------------
// [SECTION] rect ops
//-----------------------------------------------------------------------------

static inline plRect pl_calculate_rect     (plVec2 tStart, plVec2 tSize)                      { return pl_create_rect_vec2(tStart, pl_add_vec2(tStart, tSize));}
static inline float  pl_rect_width         (const plRect* ptRect)                             { return ptRect->tMax.x - ptRect->tMin.x;}
static inline float  pl_rect_height        (const plRect* ptRect)                             { return ptRect->tMax.y - ptRect->tMin.y;}
static inline plVec2 pl_rect_size          (const plRect* ptRect)                             { return pl_sub_vec2(ptRect->tMax, ptRect->tMin);}
static inline plVec2 pl_rect_center        (const plRect* ptRect)                             { return pl_create_vec2((ptRect->tMax.x + ptRect->tMin.x) * 0.5f, (ptRect->tMax.y + ptRect->tMin.y) * 0.5f);}
static inline plVec2 pl_rect_top_left      (const plRect* ptRect)                             { return ptRect->tMin;}
static inline plVec2 pl_rect_top_right     (const plRect* ptRect)                             { return pl_create_vec2(ptRect->tMax.x, ptRect->tMin.y);}
static inline plVec2 pl_rect_bottom_left   (const plRect* ptRect)                             { return pl_create_vec2(ptRect->tMin.x, ptRect->tMax.y);}
static inline plVec2 pl_rect_bottom_right  (const plRect* ptRect)                             { return ptRect->tMax;}
static inline bool   pl_rect_contains_point(const plRect* ptRect, plVec2 tP)                  { return tP.x >= ptRect->tMin.x && tP.y >= ptRect->tMin.y && tP.x < ptRect->tMax.x && tP.y < ptRect->tMax.y; }
static inline bool   pl_rect_contains_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.x >= ptRect0->tMin.x && ptRect1->tMin.y >= ptRect0->tMin.y && ptRect1->tMax.x <= ptRect0->tMax.x && ptRect1->tMax.y <= ptRect0->tMax.y; }
static inline bool   pl_rect_overlaps_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.y <  ptRect0->tMax.y && ptRect1->tMax.y >  ptRect0->tMin.y && ptRect1->tMin.x <  ptRect0->tMax.x && ptRect1->tMax.x > ptRect0->tMin.x; }
static inline bool   pl_rect_is_inverted   (const plRect* ptRect)                             { return ptRect->tMin.x > ptRect->tMax.x || ptRect->tMin.y > ptRect->tMax.y; }
static inline plRect pl_rect_expand        (const plRect* ptRect, float fPadding)             { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x - fPadding, ptRect->tMin.y - fPadding); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x + fPadding, ptRect->tMax.y + fPadding); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_expand_vec2   (const plRect* ptRect, plVec2 tPadding)            { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x - tPadding.x, ptRect->tMin.y - tPadding.y); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x + tPadding.x, ptRect->tMax.y + tPadding.y); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_clip          (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = pl_create_vec2(pl_maxf(ptRect0->tMin.x, ptRect1->tMin.x), pl_maxf(ptRect0->tMin.y, ptRect1->tMin.y)); const plVec2 tMax = pl_create_vec2(pl_minf(ptRect0->tMax.x, ptRect1->tMax.x), pl_minf(ptRect0->tMax.y, ptRect1->tMax.y)); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_clip_full     (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = pl_clamp_vec2(ptRect1->tMin, ptRect0->tMin, ptRect1->tMax); const plVec2 tMax = pl_clamp_vec2(ptRect1->tMin, ptRect0->tMax, ptRect1->tMax); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_floor         (const plRect* ptRect)                             { const plVec2 tMin = pl_create_vec2( floorf(ptRect->tMin.x), floorf(ptRect->tMin.y)); const plVec2 tMax = pl_create_vec2(floorf(ptRect->tMax.x), floorf(ptRect->tMax.y)); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_translate_vec2(const plRect* ptRect, plVec2 tDelta)              { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x + tDelta.x, ptRect->tMin.y + tDelta.y); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x + tDelta.x, ptRect->tMax.y + tDelta.y); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_translate_x   (const plRect* ptRect, float fDx)                  { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x + fDx, ptRect->tMin.y); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x + fDx, ptRect->tMax.y); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_translate_y   (const plRect* ptRect, float fDy)                  { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x, ptRect->tMin.y + fDy); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x, ptRect->tMax.y + fDy); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_add_point     (const plRect* ptRect, plVec2 tP)                  { const plVec2 tMin = pl_create_vec2(ptRect->tMin.x > tP.x ? tP.x : ptRect->tMin.x, ptRect->tMin.y > tP.y ? tP.y : ptRect->tMin.y); const plVec2 tMax = pl_create_vec2(ptRect->tMax.x < tP.x ? tP.x : ptRect->tMax.x, ptRect->tMax.y < tP.y ? tP.y : ptRect->tMax.y); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_add_rect      (const plRect* ptRect0, const plRect* ptRect1)     { const plVec2 tMin = pl_create_vec2(ptRect0->tMin.x > ptRect1->tMin.x ? ptRect1->tMin.x : ptRect0->tMin.x, ptRect0->tMin.y > ptRect1->tMin.y ? ptRect1->tMin.y : ptRect0->tMin.y); const plVec2 tMax = pl_create_vec2(ptRect0->tMax.x < ptRect1->tMax.x ? ptRect1->tMax.x : ptRect0->tMax.x, ptRect0->tMax.y < ptRect1->tMax.y ? ptRect1->tMax.y : ptRect0->tMax.y); return pl_create_rect_vec2(tMin, tMax);}
static inline plRect pl_rect_move_center   (const plRect* ptRect, float fX, float fY)         { const plVec2 tCurrentCenter = pl_rect_center(ptRect); const float fDx = fX - tCurrentCenter.x; const float fDy = fY - tCurrentCenter.y; const plRect tResult = {{ ptRect->tMin.x + fDx, ptRect->tMin.y + fDy},{ ptRect->tMax.x + fDx, ptRect->tMax.y + fDy}}; return tResult;}
static inline plRect pl_rect_move_center_y (const plRect* ptRect, float fY)                   { const plVec2 tCurrentCenter = pl_rect_center(ptRect); const float fDy = fY - tCurrentCenter.y; const plRect tResult = {{ ptRect->tMin.x, ptRect->tMin.y + fDy},{ ptRect->tMax.x, ptRect->tMax.y + fDy}}; return tResult;}
static inline plRect pl_rect_move_center_x (const plRect* ptRect, float fX)                   { const plVec2 tCurrentCenter = pl_rect_center(ptRect); const float fDx = fX - tCurrentCenter.x; const plRect tResult = { { ptRect->tMin.x + fDx, ptRect->tMin.y}, { ptRect->tMax.x + fDx, ptRect->tMax.y} }; return tResult;}
static inline plRect pl_rect_move_start    (const plRect* ptRect, float fX, float fY)         { const plRect tResult = {{ fX, fY}, { fX + ptRect->tMax.x - ptRect->tMin.x, fY + ptRect->tMax.y - ptRect->tMin.y} }; return tResult;}
static inline plRect pl_rect_move_start_x  (const plRect* ptRect, float fX)                   { const plRect tResult = { { fX, ptRect->tMin.y}, { fX + ptRect->tMax.x - ptRect->tMin.x, ptRect->tMax.y} }; return tResult;}
static inline plRect pl_rect_move_start_y  (const plRect* ptRect, float fY)                   { const plRect tResult = {{ ptRect->tMin.x, fY}, { ptRect->tMax.x, fY + ptRect->tMax.y - ptRect->tMin.y}}; return tResult;}

//-----------------------------------------------------------------------------
// [SECTION] implementations
//-----------------------------------------------------------------------------

static inline plVec3
pl_mul_mat4_vec3(const plMat4* ptLeft, plVec3 tRight) 
{
    const plVec4 Mov0 = { tRight.x, tRight.x, tRight.x, tRight.x };
    const plVec4 Mov1 = { tRight.y, tRight.y, tRight.y, tRight.y };
    const plVec4 Mul0 = pl_mul_vec4(ptLeft->col[0], Mov0);
    const plVec4 Mul1 = pl_mul_vec4(ptLeft->col[1], Mov1);
    const plVec4 Add0 = pl_add_vec4(Mul0, Mul1);
    const plVec4 Mov2 = { tRight.z, tRight.z, tRight.z, tRight.z };
    const plVec4 Mov3 = { 1.0f, 1.0f, 1.0f, 1.0f };
    const plVec4 Mul2 = pl_mul_vec4(ptLeft->col[2], Mov2);
    const plVec4 Mul3 = pl_mul_vec4(ptLeft->col[3], Mov3);
    const plVec4 Add1 = pl_add_vec4(Mul2, Mul3);
    const plVec4 Add2 = pl_add_vec4(Add0, Add1);
    return pl_create_vec3(Add2.x, Add2.y, Add2.z );    
}

static inline plVec4
pl_mul_mat4_vec4(const plMat4* ptLeft, plVec4 tRight) 
{
    const plVec4 Mov0 = { tRight.x, tRight.x, tRight.x, tRight.x };
    const plVec4 Mov1 = { tRight.y, tRight.y, tRight.y, tRight.y };
    const plVec4 Mul0 = pl_mul_vec4(ptLeft->col[0], Mov0);
    const plVec4 Mul1 = pl_mul_vec4(ptLeft->col[1], Mov1);
    const plVec4 Add0 = pl_add_vec4(Mul0, Mul1);
    const plVec4 Mov2 = { tRight.z, tRight.z, tRight.z, tRight.z };
    const plVec4 Mov3 = { tRight.w, tRight.w, tRight.w, tRight.w };
    const plVec4 Mul2 = pl_mul_vec4(ptLeft->col[2], Mov2);
    const plVec4 Mul3 = pl_mul_vec4(ptLeft->col[3], Mov3);
    const plVec4 Add1 = pl_add_vec4(Mul2, Mul3);
    return pl_add_vec4(Add0, Add1);
}

static inline plMat4
pl_mul_mat4(const plMat4* ptLeft, const plMat4* ptRight)
{
    plMat4 tResult;

    // row 0
    tResult.x11 = ptLeft->col[0].d[0] * ptRight->col[0].d[0] + ptLeft->col[1].d[0] * ptRight->col[0].d[1] + ptLeft->col[2].d[0] * ptRight->col[0].d[2] + ptLeft->col[3].d[0] * ptRight->col[0].d[3];
    tResult.x12 = ptLeft->col[0].d[0] * ptRight->col[1].d[0] + ptLeft->col[1].d[0] * ptRight->col[1].d[1] + ptLeft->col[2].d[0] * ptRight->col[1].d[2] + ptLeft->col[3].d[0] * ptRight->col[1].d[3];
    tResult.x13 = ptLeft->col[0].d[0] * ptRight->col[2].d[0] + ptLeft->col[1].d[0] * ptRight->col[2].d[1] + ptLeft->col[2].d[0] * ptRight->col[2].d[2] + ptLeft->col[3].d[0] * ptRight->col[2].d[3];
    tResult.x14 = ptLeft->col[0].d[0] * ptRight->col[3].d[0] + ptLeft->col[1].d[0] * ptRight->col[3].d[1] + ptLeft->col[2].d[0] * ptRight->col[3].d[2] + ptLeft->col[3].d[0] * ptRight->col[3].d[3];

    // row 1
    tResult.x21 = ptLeft->col[0].d[1] * ptRight->col[0].d[0] + ptLeft->col[1].d[1] * ptRight->col[0].d[1] + ptLeft->col[2].d[1] * ptRight->col[0].d[2] + ptLeft->col[3].d[1] * ptRight->col[0].d[3];
    tResult.x22 = ptLeft->col[0].d[1] * ptRight->col[1].d[0] + ptLeft->col[1].d[1] * ptRight->col[1].d[1] + ptLeft->col[2].d[1] * ptRight->col[1].d[2] + ptLeft->col[3].d[1] * ptRight->col[1].d[3];
    tResult.x23 = ptLeft->col[0].d[1] * ptRight->col[2].d[0] + ptLeft->col[1].d[1] * ptRight->col[2].d[1] + ptLeft->col[2].d[1] * ptRight->col[2].d[2] + ptLeft->col[3].d[1] * ptRight->col[2].d[3];
    tResult.x24 = ptLeft->col[0].d[1] * ptRight->col[3].d[0] + ptLeft->col[1].d[1] * ptRight->col[3].d[1] + ptLeft->col[2].d[1] * ptRight->col[3].d[2] + ptLeft->col[3].d[1] * ptRight->col[3].d[3];

    // row 2
    tResult.x31 = ptLeft->col[0].d[2] * ptRight->col[0].d[0] + ptLeft->col[1].d[2] * ptRight->col[0].d[1] + ptLeft->col[2].d[2] * ptRight->col[0].d[2] + ptLeft->col[3].d[2] * ptRight->col[0].d[3];
    tResult.x32 = ptLeft->col[0].d[2] * ptRight->col[1].d[0] + ptLeft->col[1].d[2] * ptRight->col[1].d[1] + ptLeft->col[2].d[2] * ptRight->col[1].d[2] + ptLeft->col[3].d[2] * ptRight->col[1].d[3];
    tResult.x33 = ptLeft->col[0].d[2] * ptRight->col[2].d[0] + ptLeft->col[1].d[2] * ptRight->col[2].d[1] + ptLeft->col[2].d[2] * ptRight->col[2].d[2] + ptLeft->col[3].d[2] * ptRight->col[2].d[3];
    tResult.x34 = ptLeft->col[0].d[2] * ptRight->col[3].d[0] + ptLeft->col[1].d[2] * ptRight->col[3].d[1] + ptLeft->col[2].d[2] * ptRight->col[3].d[2] + ptLeft->col[3].d[2] * ptRight->col[3].d[3];

    // row 3
    tResult.x41 = ptLeft->col[0].d[3] * ptRight->col[0].d[0] + ptLeft->col[1].d[3] * ptRight->col[0].d[1] + ptLeft->col[2].d[3] * ptRight->col[0].d[2] + ptLeft->col[3].d[3] * ptRight->col[0].d[3];
    tResult.x42 = ptLeft->col[0].d[3] * ptRight->col[1].d[0] + ptLeft->col[1].d[3] * ptRight->col[1].d[1] + ptLeft->col[2].d[3] * ptRight->col[1].d[2] + ptLeft->col[3].d[3] * ptRight->col[1].d[3];
    tResult.x43 = ptLeft->col[0].d[3] * ptRight->col[2].d[0] + ptLeft->col[1].d[3] * ptRight->col[2].d[1] + ptLeft->col[2].d[3] * ptRight->col[2].d[2] + ptLeft->col[3].d[3] * ptRight->col[2].d[3];
    tResult.x44 = ptLeft->col[0].d[3] * ptRight->col[3].d[0] + ptLeft->col[1].d[3] * ptRight->col[3].d[1] + ptLeft->col[2].d[3] * ptRight->col[3].d[2] + ptLeft->col[3].d[3] * ptRight->col[3].d[3];

    return tResult;
}

static inline plMat4
pl_mat4_rotate_vec3(float fAngle, plVec3 tVec)
{
    const float fCos = cosf(fAngle);
    const float fSin = sinf(fAngle);

    const plVec3 tAxis = pl_norm_vec3(tVec);
    const plVec3 tTemp = pl_mul_vec3_scalarf(tAxis, 1.0f - fCos);

    const plMat4 tM = pl_identity_mat4();
    const plMat4 tRotate = {
        fCos + tTemp.x * tAxis.x,
        tTemp.x * tAxis.y + fSin * tAxis.z,
        tTemp.x * tAxis.z - fSin * tAxis.y,
        0.0f,
        tTemp.y * tAxis.x - fSin * tAxis.z,
        fCos + tTemp.y * tAxis.y,
        tTemp.y * tAxis.z + fSin * tAxis.x,
        0.0f,
        tTemp.z * tAxis.x + fSin * tAxis.y,
        tTemp.z * tAxis.y - fSin * tAxis.x,
        fCos + tTemp.z * tAxis.z,
        0.0f
    };

    return pl_create_mat4_cols(
        pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[0].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[0].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[0].d[2]))),
        pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[1].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[1].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[1].d[2]))),
        pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[2].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[2].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[2].d[2]))),
        tM.col[3]); 
}

static inline plMat4
pl_mat4_invert(const plMat4* ptMat)
{
    const plVec3 tA = ptMat->col[0].xyz;
    const plVec3 tB = ptMat->col[1].xyz;
    const plVec3 tC = ptMat->col[2].xyz;
    const plVec3 tD = ptMat->col[3].xyz;

    const float fX = pl_mat4_get(ptMat, 3, 0);
    const float fY = pl_mat4_get(ptMat, 3, 1);
    const float fZ = pl_mat4_get(ptMat, 3, 2);
    const float fW = pl_mat4_get(ptMat, 3, 3);

    plVec3 tS = pl_cross_vec3(tA, tB);
    plVec3 tT = pl_cross_vec3(tC, tD);
    plVec3 tU = pl_sub_vec3(pl_mul_vec3_scalarf(tA, fY), pl_mul_vec3_scalarf(tB, fX));
    plVec3 tV = pl_sub_vec3(pl_mul_vec3_scalarf(tC, fW), pl_mul_vec3_scalarf(tD, fZ));

    const float fInvDet = 1.0f / (pl_dot_vec3(tS, tV) + pl_dot_vec3(tT, tU));
    tS = pl_mul_vec3_scalarf(tS, fInvDet);
    tT = pl_mul_vec3_scalarf(tT, fInvDet);
    tU = pl_mul_vec3_scalarf(tU, fInvDet);
    tV = pl_mul_vec3_scalarf(tV, fInvDet);

    const plVec3 tR0 = pl_add_vec3(pl_cross_vec3(tB, tV), pl_mul_vec3_scalarf(tT, fY));
    const plVec3 tR1 = pl_sub_vec3(pl_cross_vec3(tV, tA), pl_mul_vec3_scalarf(tT, fX));
    const plVec3 tR2 = pl_add_vec3(pl_cross_vec3(tD, tU), pl_mul_vec3_scalarf(tS, fW));
    const plVec3 tR3 = pl_sub_vec3(pl_cross_vec3(tU, tC), pl_mul_vec3_scalarf(tS, fZ));
    
    plMat4 tResult;
    tResult.x11 = tR0.x;
    tResult.x21 = tR1.x;
    tResult.x31 = tR2.x;
    tResult.x41 = tR3.x;
    tResult.x12 = tR0.y;
    tResult.x22 = tR1.y;
    tResult.x32 = tR2.y;
    tResult.x42 = tR3.y;
    tResult.x13 = tR0.z;
    tResult.x23 = tR1.z;
    tResult.x33 = tR2.z;
    tResult.x43 = tR3.z;
    tResult.x14 = -pl_dot_vec3(tB, tT);
    tResult.x24 =  pl_dot_vec3(tA, tT);
    tResult.x34 = -pl_dot_vec3(tD, tS);
    tResult.x44 =  pl_dot_vec3(tC, tS);
    return tResult;
}

static inline plMat4
pl_mat4_rotate_quat(plVec4 tQ)
{
    const float x2 = tQ.x * tQ.x;
    const float y2 = tQ.y * tQ.y;
    const float z2 = tQ.z * tQ.z;
    const float xy = tQ.x * tQ.y;
    const float xz = tQ.x * tQ.z;
    const float yz = tQ.y * tQ.z;
    const float wx = tQ.w * tQ.x;
    const float wy = tQ.w * tQ.y;
    const float wz = tQ.w * tQ.z;

    plMat4 tResult = {0};
    tResult.col[0].x = 1.0f - 2.0f * (y2 + z2);
    tResult.col[0].y = 2.0f * (xy + wz);
    tResult.col[0].z = 2.0f * (xz - wy);

    tResult.col[1].x = 2.0f * (xy - wz);
    tResult.col[1].y = 1.0f - 2.0f * (x2 + z2);
    tResult.col[1].z = 2.0f * (yz + wx);

    tResult.col[2].x = 2.0f * (xz + wy);
    tResult.col[2].y = 2.0f * (yz - wx);
    tResult.col[2].z = 1.0f - 2.0f * (x2 + y2);

    tResult.col[3].w = 1.0f;

    return tResult;
}

static inline plMat4
pl_rotation_translation_scale(plVec4 tQ, plVec3 tV, plVec3 tS)
{

    const plMat4 tScale = pl_mat4_scale_vec3(tS);
    const plMat4 tTranslation = pl_mat4_translate_vec3(tV);
    const plMat4 tRotation = pl_mat4_rotate_quat(tQ);

    plMat4 tResult0 = pl_mul_mat4(&tRotation, &tScale);
    tResult0 = pl_mul_mat4(&tTranslation, &tResult0);
    return tResult0;
}

static inline plMat4
pl_mul_mat4t(const plMat4* ptLeft, const plMat4* ptRight)
{
    plMat4 tResult = pl_create_mat4_diag(0.0f, 0.0f, 0.0f, 1.0f);

    // row 0
    tResult.x11 = ptLeft->x11 * ptRight->x11 + ptLeft->x12 * ptRight->x21 + ptLeft->x13 * ptRight->x31;
    tResult.x12 = ptLeft->x11 * ptRight->x12 + ptLeft->x12 * ptRight->x22 + ptLeft->x13 * ptRight->x32;
    tResult.x13 = ptLeft->x11 * ptRight->x13 + ptLeft->x12 * ptRight->x23 + ptLeft->x13 * ptRight->x33;
    tResult.x14 = ptLeft->x11 * ptRight->x14 + ptLeft->x12 * ptRight->x24 + ptLeft->x13 * ptRight->x34 + ptLeft->x14;

    // row 1
    tResult.x21 = ptLeft->x21 * ptRight->x11 + ptLeft->x22 * ptRight->x21 + ptLeft->x23 * ptRight->x31;
    tResult.x22 = ptLeft->x21 * ptRight->x12 + ptLeft->x22 * ptRight->x22 + ptLeft->x23 * ptRight->x32;
    tResult.x23 = ptLeft->x21 * ptRight->x13 + ptLeft->x22 * ptRight->x23 + ptLeft->x23 * ptRight->x33;
    tResult.x24 = ptLeft->x21 * ptRight->x14 + ptLeft->x22 * ptRight->x24 + ptLeft->x23 * ptRight->x34 + ptLeft->x24;

    // row 2
    tResult.x31 = ptLeft->x31 * ptRight->x11 + ptLeft->x32 * ptRight->x21 + ptLeft->x33 * ptRight->x31;
    tResult.x32 = ptLeft->x31 * ptRight->x12 + ptLeft->x32 * ptRight->x22 + ptLeft->x33 * ptRight->x32;
    tResult.x33 = ptLeft->x31 * ptRight->x13 + ptLeft->x32 * ptRight->x23 + ptLeft->x33 * ptRight->x33;
    tResult.x34 = ptLeft->x31 * ptRight->x14 + ptLeft->x32 * ptRight->x24 + ptLeft->x33 * ptRight->x34 + ptLeft->x34;

    return tResult;
}

static inline plMat4
pl_mat4t_invert(const plMat4* ptMat)
{
    const plVec3 tA = ptMat->col[0].xyz;
    const plVec3 tB = ptMat->col[1].xyz;
    const plVec3 tC = ptMat->col[2].xyz;
    const plVec3 tD = ptMat->col[3].xyz;

    plVec3 tS = pl_cross_vec3(tA, tB);
    plVec3 tT = pl_cross_vec3(tC, tD);

    const float fInvDet = 1.0f / pl_dot_vec3(tS, tC);
    tS = pl_mul_vec3_scalarf(tS, fInvDet);
    tT = pl_mul_vec3_scalarf(tT, fInvDet);

    const plVec3 tV = pl_mul_vec3_scalarf(tC, fInvDet);
    const plVec3 tR0 = pl_cross_vec3(tB, tV);
    const plVec3 tR1 = pl_cross_vec3(tV, tA);
    
    plMat4 tResult;
    tResult.x11 = tR0.x;
    tResult.x21 = tR1.x;
    tResult.x31 = tS.x;
    tResult.x41 = 0.0f;
    tResult.x12 = tR0.y;
    tResult.x22 = tR1.y;
    tResult.x32 = tS.y;
    tResult.x42 = 0.0f;
    tResult.x13 = tR0.z;
    tResult.x23 = tR1.z;
    tResult.x33 = tS.z;
    tResult.x43 = 0.0f;
    tResult.x14 = -pl_dot_vec3(tB, tT);
    tResult.x24 = pl_dot_vec3(tA, tT);
    tResult.x34 = -pl_dot_vec3(tD, tS);
    tResult.x44 = 1.0f;
    return tResult;
}

static inline void
pl_decompose_matrix(const plMat4* ptM, plVec3* ptS, plVec4* ptQ, plVec3* ptT)
{
    // method borrowed from blender source

    // caller must ensure matrices aren't negative for valid results

    *ptT = ptM->col[3].xyz;

    ptS->x = pl_length_vec3(ptM->col[0].xyz);
    ptS->y = pl_length_vec3(ptM->col[1].xyz);
    ptS->z = pl_length_vec3(ptM->col[2].xyz);

    // method outlined by Mike Day, ref: https://math.stackexchange.com/a/3183435/220949
    // with an additional `sqrtf(..)` for higher precision result.

    plMat4 tMat = {0};
    tMat.col[0] = pl_norm_vec4(ptM->col[0]);
    tMat.col[1] = pl_norm_vec4(ptM->col[1]);
    tMat.col[2] = pl_norm_vec4(ptM->col[2]);

    if (tMat.col[2].d[2] < 0.0f)
    {
        if (tMat.col[0].d[0] > tMat.col[1].d[1])
        {
            const float fTrace = 1.0f + tMat.col[0].d[0] - tMat.col[1].d[1] - tMat.col[2].d[2];
            float fS = 2.0f * sqrtf(fTrace);
            if (tMat.col[1].d[2] < tMat.col[2].d[1]) // ensure W is non-negative for a canonical result
                fS = -fS;
            ptQ->d[0] = 0.25f * fS;
            fS = 1.0f / fS;
            ptQ->d[3] = (tMat.col[1].d[2] - tMat.col[2].d[1]) * fS;
            ptQ->d[1] = (tMat.col[0].d[1] + tMat.col[1].d[0]) * fS;
            ptQ->d[2] = (tMat.col[2].d[0] + tMat.col[0].d[2]) * fS;
            if ((fTrace == 1.0f) && (ptQ->d[3] == 0.0f && ptQ->d[1] == 0.0f && ptQ->d[2] == 0.0f)) // avoids the need to normalize the degenerate case
                ptQ->d[0] = 1.0f;
        }
        else
        {
            const float fTrace = 1.0f - tMat.col[0].d[0] + tMat.col[1].d[1] - tMat.col[2].d[2];
            float fS = 2.0f * sqrtf(fTrace);
            if (tMat.col[2].d[0] < tMat.col[0].d[2]) // ensure W is non-negative for a canonical result
                fS = -fS;
            ptQ->d[1] = 0.25f * fS;
            fS = 1.0f / fS;
            ptQ->d[3] = (tMat.col[2].d[0] - tMat.col[0].d[2]) * fS;
            ptQ->d[0] = (tMat.col[0].d[1] + tMat.col[1].d[0]) * fS;
            ptQ->d[2] = (tMat.col[1].d[2] + tMat.col[2].d[1]) * fS;
            if ((fTrace == 1.0f) && (ptQ->d[3] == 0.0f && ptQ->d[0] == 0.0f && ptQ->d[2] == 0.0f)) // avoids the need to normalize the degenerate case
                ptQ->d[1] = 1.0f;
        }
    }
    else
    {
        if (tMat.col[0].d[0] < -tMat.col[1].d[1])
        {
            const float fTrace = 1.0f - tMat.col[0].d[0] - tMat.col[1].d[1] + tMat.col[2].d[2];
            float fS = 2.0f * sqrtf(fTrace);
            if (tMat.col[0].d[1] < tMat.col[1].d[0]) // ensure W is non-negative for a canonical result
                fS = -fS;
            ptQ->d[2] = 0.25f * fS;
            fS = 1.0f / fS;
            ptQ->d[3] = (tMat.col[0].d[1] - tMat.col[1].d[0]) * fS;
            ptQ->d[0] = (tMat.col[2].d[0] + tMat.col[0].d[2]) * fS;
            ptQ->d[1] = (tMat.col[1].d[2] + tMat.col[2].d[1]) * fS;
            if ((fTrace == 1.0f) && (ptQ->d[3] == 0.0f && ptQ->d[0] == 0.0f && ptQ->d[1] == 0.0f)) // avoids the need to normalize the degenerate case
                ptQ->d[2] = 1.0f;
        }
        else
        {
            // note: a zero matrix will fall through to this block,
            // needed so a zero scaled matrices to return a quaternion without rotation
            const float fTrace = 1.0f + tMat.col[0].d[0] + tMat.col[1].d[1] + tMat.col[2].d[2];
            float fS = 2.0f * sqrtf(fTrace);
            ptQ->d[3] = 0.25f * fS;
            fS = 1.0f / fS;
            ptQ->d[0] = (tMat.col[1].d[2] - tMat.col[2].d[1]) * fS;
            ptQ->d[1] = (tMat.col[2].d[0] - tMat.col[0].d[2]) * fS;
            ptQ->d[2] = (tMat.col[0].d[1] - tMat.col[1].d[0]) * fS;
            if ((fTrace == 1.0f) && (ptQ->d[0] == 0.0f && ptQ->d[1] == 0.0f && ptQ->d[2] == 0.0f)) // avoids the need to normalize the degenerate case
                ptQ->d[3] = 1.0f;
        }
    }

    assert(!(ptQ->d[3] < 0.0f));
}

static inline plVec4
pl_quat_slerp(plVec4 tQ1, plVec4 tQ2, float fT)
{

	// from https://glmatrix.net/docs/quat.js.html
	plVec4 tQn1 = pl_norm_vec4(tQ1);
	plVec4 tQn2 = pl_norm_vec4(tQ2);

	plVec4 tResult = {0};

	float fAx = tQn1.x;
	float fAy = tQn1.y;
	float fAz = tQn1.z;
	float fAw = tQn1.w;

	float fBx = tQn2.x;
	float fBy = tQn2.y;
	float fBz = tQn2.z;
	float fBw = tQn2.w;

	float fOmega = 0.0f;
	float fCosom = fAx * fBx + fAy * fBy + fAz * fBz + fAw * fBw;
	float fSinom = 0.0f;
	float fScale0 = 0.0f;
	float fScale1 = 0.0f;

	// adjust signs (if necessary)
	if (fCosom < 0.0f) 
	{
		fCosom = -fCosom;
		fBx = -fBx;
		fBy = -fBy;
		fBz = -fBz;
		fBw = -fBw;
	}

	// calculate coefficients
	if (1.0f - fCosom > 0.000001f)
	{
		// standard case (slerp)
		fOmega = acosf(fCosom);
		fSinom = sinf(fOmega);
		fScale0 = sinf((1.0f - fT) * fOmega) / fSinom;
		fScale1 = sinf(fT * fOmega) / fSinom;
	}
	else 
	{
		// "from" and "to" quaternions are very close
		//  ... so we can do a linear interpolation
		fScale0 = 1.0f - fT;
		fScale1 = fT;
	}

	// calculate final values
	tResult.d[0] = fScale0 * fAx + fScale1 * fBx;
	tResult.d[1] = fScale0 * fAy + fScale1 * fBy;
	tResult.d[2] = fScale0 * fAz + fScale1 * fBz;
	tResult.d[3] = fScale0 * fAw + fScale1 * fBw;

	tResult = pl_norm_vec4(tResult);

	return tResult;
}

#endif // PL_MATH_INCLUDE_FUNCTIONS
