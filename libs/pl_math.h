/*
   pl_math.h, v0.4 (WIP)

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
#define PL_MATH_VERSION    "0.4.0"
#define PL_MATH_VERSION_NUM 00400

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
// [SECTION] rect ops
// [SECTION] implementations
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

#if defined(PL_MATH_INCLUDE_FUNCTIONS) && !defined(PL_MATH_INCLUDE_FUNCTIONS_H)
#define PL_MATH_INCLUDE_FUNCTIONS_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] general math
//-----------------------------------------------------------------------------

static inline float    pl_radiansf(float fDegrees)                       { return fDegrees * 0.0174532925f; }
static inline float    pl_degreesf(float fRadians)                       { return fRadians * 57.29577951f; }
static inline float    pl_maxf    (float fValue1, float fValue2)         { return fValue1 > fValue2 ? fValue1 : fValue2; }
static inline float    pl_minf    (float fValue1, float fValue2)         { return fValue1 > fValue2 ? fValue2 : fValue1; }
static inline int      pl_maxi    (int iValue1, int iValue2)             { return iValue1 > iValue2 ? iValue1 : iValue2; }
static inline int      pl_mini    (int iValue1, int iValue2)             { return iValue1 > iValue2 ? iValue2 : iValue1; }
static inline uint32_t pl_maxu    (uint32_t uValue1, uint32_t uValue2)   { return uValue1 > uValue2 ? uValue1 : uValue2; }
static inline uint32_t pl_minu    (uint32_t uValue1, uint32_t uValue2)   { return uValue1 > uValue2 ? uValue2 : uValue1; }
static inline float    pl_squaref (float fValue)                         { return fValue * fValue;}
static inline float    pl_cubef   (float fValue)                         { return fValue * fValue * fValue;}
static inline int      pl_clampi  (int iMin, int iValue, int iMax)       { if (iValue < iMin) return iMin; else if (iValue > iMax) return iMax; return iValue; }
static inline float    pl_clampf  (float fMin, float fValue, float fMax) { if (fValue < fMin) return fMin; else if (fValue > fMax) return fMax; return fValue; }
static inline float    pl_clamp01f(float fValue)                         { return pl_clampf(0.0f, fValue, 1.0f); }
static inline size_t   pl_align_up(size_t szValue, size_t szAlign)       { return ((szValue + (szAlign - 1)) & ~(szAlign - 1)); }

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
static inline plVec2 pl_floor_vec2       (plVec2 tVec)                             { return (plVec2){floorf(tVec.x), floorf(tVec.y)};}
static inline plVec3 pl_floor_vec3       (plVec3 tVec)                             { return (plVec3){floorf(tVec.x), floorf(tVec.y), floorf(tVec.z)};}
static inline plVec4 pl_floor_vec4       (plVec4 tVec)                             { return (plVec4){floorf(tVec.x), floorf(tVec.y), floorf(tVec.z), floorf(tVec.w)};}
static inline plVec2 pl_clamp_vec2       (plVec2 tMin, plVec2 tValue, plVec2 tMax) { return (plVec2){pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y)};}
static inline plVec3 pl_clamp_vec3       (plVec3 tMin, plVec3 tValue, plVec3 tMax) { return (plVec3){pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y), pl_clampf(tMax.z, tValue.z, tMax.z)};}
static inline plVec4 pl_clamp_vec4       (plVec4 tMin, plVec4 tValue, plVec4 tMax) { return (plVec4){pl_clampf(tMin.x, tValue.x, tMax.x), pl_clampf(tMin.y, tValue.y, tMax.y), pl_clampf(tMax.z, tValue.z, tMax.z), pl_clampf(tMax.w, tValue.w, tMax.w)};}
static inline plVec2 pl_min_vec2        (plVec2 tValue0, plVec2 tValue1)           { return (plVec2){pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y)};}
static inline plVec3 pl_min_vec3        (plVec3 tValue0, plVec3 tValue1)           { return (plVec3){pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y), pl_minf(tValue0.z, tValue1.z)};}
static inline plVec4 pl_min_vec4        (plVec4 tValue0, plVec4 tValue1)           { return (plVec4){pl_minf(tValue0.x, tValue1.x), pl_minf(tValue0.y, tValue1.y), pl_minf(tValue0.z, tValue1.z), pl_minf(tValue0.w, tValue1.w)};}
static inline plVec2 pl_max_vec2        (plVec2 tValue0, plVec2 tValue1)           { return (plVec2){pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y)};}
static inline plVec3 pl_max_vec3        (plVec3 tValue0, plVec3 tValue1)           { return (plVec3){pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y), pl_maxf(tValue0.z, tValue1.z)};}
static inline plVec4 pl_max_vec4        (plVec4 tValue0, plVec4 tValue1)           { return (plVec4){pl_maxf(tValue0.x, tValue1.x), pl_maxf(tValue0.y, tValue1.y), pl_maxf(tValue0.z, tValue1.z), pl_maxf(tValue0.w, tValue1.w)};}

// binary ops
static inline float  pl_dot_vec2        (plVec2 tVec1, plVec2 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y; }
static inline float  pl_dot_vec3        (plVec3 tVec1, plVec3 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y + tVec1.z * tVec2.z; }
static inline float  pl_dot_vec4        (plVec4 tVec1, plVec4 tVec2) { return tVec1.x * tVec2.x + tVec1.y * tVec2.y + tVec1.z * tVec2.z + tVec1.w * tVec2.w; }
static inline plVec3 pl_cross_vec3      (plVec3 tVec1, plVec3 tVec2) { return (plVec3){tVec1.y * tVec2.z - tVec2.y * tVec1.z, tVec1.z * tVec2.x - tVec2.z * tVec1.x, tVec1.x * tVec2.y - tVec2.x * tVec1.y}; }

static inline plVec2 pl_add_vec2        (plVec2 tVec1, plVec2 tVec2) { return (plVec2){tVec1.x + tVec2.x, tVec1.y + tVec2.y}; }
static inline plVec3 pl_add_vec3        (plVec3 tVec1, plVec3 tVec2) { return (plVec3){tVec1.x + tVec2.x, tVec1.y + tVec2.y, tVec1.z + tVec2.z}; }
static inline plVec4 pl_add_vec4        (plVec4 tVec1, plVec4 tVec2) { return (plVec4){tVec1.x + tVec2.x, tVec1.y + tVec2.y, tVec1.z + tVec2.z, tVec1.w + tVec2.w}; }

static inline plVec2 pl_sub_vec2        (plVec2 tVec1, plVec2 tVec2) { return (plVec2){tVec1.x - tVec2.x, tVec1.y - tVec2.y}; }
static inline plVec3 pl_sub_vec3        (plVec3 tVec1, plVec3 tVec2) { return (plVec3){tVec1.x - tVec2.x, tVec1.y - tVec2.y, tVec1.z - tVec2.z}; }
static inline plVec4 pl_sub_vec4        (plVec4 tVec1, plVec4 tVec2) { return (plVec4){tVec1.x - tVec2.x, tVec1.y - tVec2.y, tVec1.z - tVec2.z, tVec1.w - tVec2.w} ;}

static inline plVec2 pl_mul_vec2        (plVec2 tVec1, plVec2 tVec2) { return (plVec2){tVec1.x * tVec2.x, tVec1.y * tVec2.y}; }
static inline plVec3 pl_mul_vec3        (plVec3 tVec1, plVec3 tVec2) { return (plVec3){tVec1.x * tVec2.x, tVec1.y * tVec2.y, tVec1.z * tVec2.z}; }
static inline plVec4 pl_mul_vec4        (plVec4 tVec1, plVec4 tVec2) { return (plVec4){tVec1.x * tVec2.x, tVec1.y * tVec2.y, tVec1.z * tVec2.z, tVec1.w * tVec2.w}; }

static inline plVec2 pl_div_vec2        (plVec2 tVec1, plVec2 tVec2) { return (plVec2){tVec1.x / tVec2.x, tVec1.y / tVec2.y}; }
static inline plVec3 pl_div_vec3        (plVec3 tVec1, plVec3 tVec2) { return (plVec3){tVec1.x / tVec2.x, tVec1.y / tVec2.y, tVec1.z / tVec2.z}; }
static inline plVec4 pl_div_vec4        (plVec4 tVec1, plVec4 tVec2) { return (plVec4){tVec1.x / tVec2.x, tVec1.y / tVec2.y, tVec1.z / tVec2.z, tVec1.w / tVec2.w}; }

static inline plVec2 pl_mul_vec2_scalarf(plVec2 tVec, float fValue)  { return (plVec2){fValue * tVec.x, fValue * tVec.y}; }
static inline plVec3 pl_mul_vec3_scalarf(plVec3 tVec, float fValue)  { return (plVec3){fValue * tVec.x, fValue * tVec.y, fValue * tVec.z}; }
static inline plVec4 pl_mul_vec4_scalarf(plVec4 tVec, float fValue)  { return (plVec4){fValue * tVec.x, fValue * tVec.y, fValue * tVec.z, fValue * tVec.w}; }

static inline plVec2 pl_div_vec2_scalarf(plVec2 tVec, float fValue)  { return (plVec2){tVec.x / fValue, tVec.y / fValue}; }
static inline plVec3 pl_div_vec3_scalarf(plVec3 tVec, float fValue)  { return (plVec3){tVec.x / fValue, tVec.y / fValue, tVec.z / fValue}; }
static inline plVec4 pl_div_vec4_scalarf(plVec4 tVec, float fValue)  { return (plVec4){tVec.x / fValue, tVec.y / fValue, tVec.z / fValue, tVec.w / fValue}; }

static inline plVec2 pl_div_scalarf_vec2(float fValue, plVec2 tVec)  { return (plVec2){fValue / tVec.x, fValue / tVec.y}; }
static inline plVec3 pl_div_scalarf_vec3(float fValue, plVec3 tVec)  { return (plVec3){fValue / tVec.x, fValue / tVec.y, fValue / tVec.z}; }
static inline plVec4 pl_div_scalarf_vec4(float fValue, plVec4 tVec)  { return (plVec4){fValue / tVec.x, fValue / tVec.y, fValue / tVec.z, fValue / tVec.w}; }

static inline plVec2 pl_norm_vec2       (plVec2 tVec)                { return pl_div_vec2_scalarf(tVec, pl_length_vec2(tVec)); }
static inline plVec3 pl_norm_vec3       (plVec3 tVec)                { return pl_div_vec3_scalarf(tVec, pl_length_vec3(tVec)); }
static inline plVec4 pl_norm_vec4       (plVec4 tVec)                { return pl_div_vec4_scalarf(tVec, pl_length_vec4(tVec)); }

//-----------------------------------------------------------------------------
// [SECTION] matrix ops
//-----------------------------------------------------------------------------

// general ops
static inline float  pl_mat4_get                  (const plMat4* ptMat, int iRow, int iCol)         { return ptMat->col[iCol].d[iRow];}
static inline void   pl_mat4_set                  (plMat4* ptMat, int iRow, int iCol, float fValue) { ptMat->col[iCol].d[iRow] = fValue;}
static inline plMat4 pl_identity_mat4             (void)                                            { return (plMat4){.x11 = 1.0f, .x22 = 1.0f, .x33 = 1.0f, .x44 = 1.0f};}
static inline plMat4 pl_mat4_transpose            (const plMat4* ptMat)                             { plMat4 tResult = {0}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) pl_mat4_set(&tResult, i, j, pl_mat4_get(ptMat, j, i)); return tResult;}
static inline plMat4 pl_mat4_invert               (const plMat4* ptMat);
static inline plMat4 pl_mul_scalarf_mat4          (float fLeft, const plMat4* ptRight)              { plMat4 tResult = {0}; for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) pl_mat4_set(&tResult, i, j, fLeft * pl_mat4_get(ptRight, j, i)); return tResult;}
static inline plVec3 pl_mul_mat4_vec3             (const plMat4* ptLeft, plVec3 tRight);
static inline plVec4 pl_mul_mat4_vec4             (const plMat4* ptLeft, plVec4 tRight);
static inline plMat4 pl_mul_mat4                  (const plMat4* ptLeft, const plMat4* ptRight);

// translation, rotation, scaling
static inline plMat4 pl_mat4_translate_xyz        (float fX, float fY, float fZ)               { return (plMat4){.x11 = 1.0f, .x22 = 1.0f, .x33 = 1.0f, .x44 = 1.0f, .x14 = fX, .x24 = fY, .x34 = fZ };}
static inline plMat4 pl_mat4_translate_vec3       (plVec3 tVec)                                { return pl_mat4_translate_xyz(tVec.x, tVec.y, tVec.z);}
static inline plMat4 pl_mat4_rotate_vec3          (float fAngle, plVec3 tVec);
static inline plMat4 pl_mat4_rotate_xyz           (float fAngle, float fX, float fY, float fZ) { return pl_mat4_rotate_vec3(fAngle, (plVec3){fX, fY, fZ});}
static inline plMat4 pl_mat4_scale_xyz            (float fX, float fY, float fZ)               { return (plMat4){.x11 = fX, .x22 = fY, .x33 = fZ, .x44 = 1.0f};}
static inline plMat4 pl_mat4_scale_vec3           (plVec3 tVec)                                { return pl_mat4_scale_xyz(tVec.x, tVec.y, tVec.z);}
static inline plMat4 pl_rotation_translation_scale(plVec4 tQ, plVec3 tV, plVec3 tS);

// transforms (optimized for orthogonal matrices)
static inline plMat4 pl_mat4t_invert              (const plMat4* ptMat);
static inline plMat4 pl_mul_mat4t                 (const plMat4* ptLeft, const plMat4* ptRight);

//-----------------------------------------------------------------------------
// [SECTION] rect ops
//-----------------------------------------------------------------------------

static inline plRect pl_create_rect        (float fX1, float fY1, float fX2, float fY2)       { return (plRect){.tMin.x = fX1, .tMin.y = fY1, .tMax.x = fX2, .tMax.y = fY2};}
static inline plRect pl_calculate_rect     (plVec2 tStart, plVec2 tSize)                      { return (plRect){.tMin = tStart, .tMax = pl_add_vec2(tStart, tSize)};}
static inline float  pl_rect_width         (const plRect* ptRect)                             { return ptRect->tMax.x - ptRect->tMin.x;}
static inline float  pl_rect_height        (const plRect* ptRect)                             { return ptRect->tMax.y - ptRect->tMin.y;}
static inline plVec2 pl_rect_size          (const plRect* ptRect)                             { return pl_sub_vec2(ptRect->tMax, ptRect->tMin);}
static inline plVec2 pl_rect_center        (const plRect* ptRect)                             { return (plVec2){(ptRect->tMax.x + ptRect->tMin.x) * 0.5f, (ptRect->tMax.y + ptRect->tMin.y) * 0.5f};}
static inline plVec2 pl_rect_top_left      (const plRect* ptRect)                             { return ptRect->tMin;}
static inline plVec2 pl_rect_top_right     (const plRect* ptRect)                             { return (plVec2){ptRect->tMax.x, ptRect->tMin.y};}
static inline plVec2 pl_rect_bottom_left   (const plRect* ptRect)                             { return (plVec2){ptRect->tMin.x, ptRect->tMax.y};}
static inline plVec2 pl_rect_bottom_right  (const plRect* ptRect)                             { return ptRect->tMax;}
static inline bool   pl_rect_contains_point(const plRect* ptRect, plVec2 tP)                  { return tP.x >= ptRect->tMin.x && tP.y >= ptRect->tMin.y && tP.x < ptRect->tMax.x && tP.y < ptRect->tMax.y; }
static inline bool   pl_rect_contains_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.x >= ptRect0->tMin.x && ptRect1->tMin.y >= ptRect0->tMin.y && ptRect1->tMax.x <= ptRect0->tMax.x && ptRect1->tMax.y <= ptRect0->tMax.y; }
static inline bool   pl_rect_overlaps_rect (const plRect* ptRect0, const plRect* ptRect1)     { return ptRect1->tMin.y <  ptRect0->tMax.y && ptRect1->tMax.y >  ptRect0->tMin.y && ptRect1->tMin.x <  ptRect0->tMax.x && ptRect1->tMax.x > ptRect0->tMin.x; }
static inline bool   pl_rect_is_inverted   (const plRect* ptRect)                             { return ptRect->tMin.x > ptRect->tMax.x || ptRect->tMin.y > ptRect->tMax.y; }
static inline plRect pl_rect_expand        (const plRect* ptRect, float fPadding)             { return (plRect){.tMin = {ptRect->tMin.x - fPadding, ptRect->tMin.y - fPadding}, .tMax = {ptRect->tMax.x + fPadding, ptRect->tMax.y + fPadding}};}
static inline plRect pl_rect_expand_vec2   (const plRect* ptRect, plVec2 tPadding)            { return (plRect){.tMin = {ptRect->tMin.x - tPadding.x, ptRect->tMin.y - tPadding.y}, .tMax = {ptRect->tMax.x + tPadding.x, ptRect->tMax.y + tPadding.y}};}
static inline plRect pl_rect_clip          (const plRect* ptRect0, const plRect* ptRect1)     { return (plRect){.tMin = { pl_maxf(ptRect0->tMin.x, ptRect1->tMin.x), pl_maxf(ptRect0->tMin.y, ptRect1->tMin.y) }, .tMax = {pl_minf(ptRect0->tMax.x, ptRect1->tMax.x), pl_minf(ptRect0->tMax.y, ptRect1->tMax.y)} };}
static inline plRect pl_rect_clip_full     (const plRect* ptRect0, const plRect* ptRect1)     { return (plRect){.tMin = pl_clamp_vec2(ptRect1->tMin, ptRect0->tMin, ptRect1->tMax), .tMax = pl_clamp_vec2(ptRect1->tMin, ptRect0->tMax, ptRect1->tMax)}; }
static inline plRect pl_rect_floor         (const plRect* ptRect)                             { return (plRect){.tMin = { floorf(ptRect->tMin.x), floorf(ptRect->tMin.y)}, .tMax = {floorf(ptRect->tMax.x), floorf(ptRect->tMax.y)}};}
static inline plRect pl_rect_translate_vec2(const plRect* ptRect, plVec2 tDelta)              { return (plRect){.tMin = { ptRect->tMin.x + tDelta.x, ptRect->tMin.y + tDelta.y}, .tMax = {ptRect->tMax.x + tDelta.x, ptRect->tMax.y + tDelta.y}};}
static inline plRect pl_rect_translate_x   (const plRect* ptRect, float fDx)                  { return (plRect){.tMin = { ptRect->tMin.x + fDx, ptRect->tMin.y}, .tMax = {ptRect->tMax.x + fDx, ptRect->tMax.y}};}
static inline plRect pl_rect_translate_y   (const plRect* ptRect, float fDy)                  { return (plRect){.tMin = { ptRect->tMin.x, ptRect->tMin.y + fDy}, .tMax = {ptRect->tMax.x, ptRect->tMax.y + fDy}};}
static inline plRect pl_rect_add_point     (const plRect* ptRect, plVec2 tP)                  { return (plRect){.tMin = { ptRect->tMin.x > tP.x ? tP.x : ptRect->tMin.x, ptRect->tMin.y > tP.y ? tP.y : ptRect->tMin.y}, .tMax = {ptRect->tMax.x < tP.x ? tP.x : ptRect->tMax.x, ptRect->tMax.y < tP.y ? tP.y : ptRect->tMax.y}};}
static inline plRect pl_rect_add_rect      (const plRect* ptRect0, const plRect* ptRect1)     { return (plRect){.tMin = { ptRect0->tMin.x > ptRect1->tMin.x ? ptRect1->tMin.x : ptRect0->tMin.x, ptRect0->tMin.y > ptRect1->tMin.y ? ptRect1->tMin.y : ptRect0->tMin.y}, .tMax = {ptRect0->tMax.x < ptRect1->tMax.x ? ptRect1->tMax.x : ptRect0->tMax.x, ptRect0->tMax.y < ptRect1->tMax.y ? ptRect1->tMax.y : ptRect0->tMax.y}};}
static inline plRect pl_rect_move_center   (const plRect* ptRect, float fX, float fY)         { const plVec2 tCurrentCenter = pl_rect_center(ptRect); const float fDx = fX - tCurrentCenter.x; const float fDy = fY - tCurrentCenter.y; const plRect tResult = {{ ptRect->tMin.x + fDx, ptRect->tMin.y + fDy},{ ptRect->tMax.x + fDx, ptRect->tMax.y + fDy},}; return tResult;}
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
    return (plVec3){ Add2.x, Add2.y, Add2.z };    
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
    return (plMat4){
        .col[0] = pl_add_vec4(pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[0], ptRight->col[0].x), pl_mul_vec4_scalarf(ptLeft->col[1], ptRight->col[0].y)), pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[2], ptRight->col[0].z), pl_mul_vec4_scalarf(ptLeft->col[3], ptRight->col[0].w))),
        .col[1] = pl_add_vec4(pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[0], ptRight->col[1].x), pl_mul_vec4_scalarf(ptLeft->col[1], ptRight->col[1].y)), pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[2], ptRight->col[1].z), pl_mul_vec4_scalarf(ptLeft->col[3], ptRight->col[1].w))),
        .col[2] = pl_add_vec4(pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[0], ptRight->col[2].x), pl_mul_vec4_scalarf(ptLeft->col[1], ptRight->col[2].y)), pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[2], ptRight->col[2].z), pl_mul_vec4_scalarf(ptLeft->col[3], ptRight->col[2].w))),
        .col[3] = pl_add_vec4(pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[0], ptRight->col[3].x), pl_mul_vec4_scalarf(ptLeft->col[1], ptRight->col[3].y)), pl_add_vec4(pl_mul_vec4_scalarf(ptLeft->col[2], ptRight->col[3].z), pl_mul_vec4_scalarf(ptLeft->col[3], ptRight->col[3].w))),
    }; 
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
        .x11 = fCos + tTemp.x * tAxis.x,
        .x21 = tTemp.x * tAxis.y + fSin * tAxis.z,
        .x31 = tTemp.x * tAxis.z - fSin * tAxis.y,
        .x12 = tTemp.y * tAxis.x - fSin * tAxis.z,
        .x22 = fCos + tTemp.y * tAxis.y,
        .x32 = tTemp.y * tAxis.z + fSin * tAxis.x,
        .x13 = tTemp.z * tAxis.x + fSin * tAxis.y,
        .x23 = tTemp.z * tAxis.y - fSin * tAxis.x,
        .x33 = fCos + tTemp.z * tAxis.z
    };

    return (plMat4){
        .col[0] = pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[0].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[0].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[0].d[2]))),
        .col[1] = pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[1].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[1].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[1].d[2]))),
        .col[2] = pl_add_vec4(pl_mul_vec4_scalarf(tM.col[0], tRotate.col[2].d[0]), pl_add_vec4(pl_mul_vec4_scalarf(tM.col[1], tRotate.col[2].d[1]), pl_mul_vec4_scalarf(tM.col[2], tRotate.col[2].d[2]))),
        .col[3] = tM.col[3]
    }; 
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
    
    return (plMat4){
        .col = {
            {
                .x = tR0.x,
                .y = tR1.x,
                .z = tR2.x,
                .w = tR3.x
            },
            {
                .x = tR0.y,
                .y = tR1.y,
                .z = tR2.y,
                .w = tR3.y
            },
            {
                .x = tR0.z,
                .y = tR1.z,
                .z = tR2.z,
                .w = tR3.z
            },
            {
                .x = -pl_dot_vec3(tB, tT),
                .y =  pl_dot_vec3(tA, tT),
                .z = -pl_dot_vec3(tD, tS),
                .w =  pl_dot_vec3(tC, tS)
            }
        }
    };
}

static inline plMat4
pl_rotation_translation_scale(plVec4 tQ, plVec3 tV, plVec3 tS)
{

    const float x2 = tQ.x + tQ.x;
    const float y2 = tQ.y + tQ.y;
    const float z2 = tQ.z + tQ.z;
    const float xx = tQ.x * x2;
    const float xy = tQ.x * y2;
    const float xz = tQ.x * z2;
    const float yy = tQ.y * y2;
    const float yz = tQ.y * z2;
    const float zz = tQ.z * z2;
    const float wx = tQ.w * x2;
    const float wy = tQ.w * y2;
    const float wz = tQ.w * z2;

    const plMat4 tResult = {
        .col = {
            {
                .x = (1.0f - (yy + zz)) * tS.x,
                .y = (xy + wz) * tS.x,
                .z = (xz - wy) * tS.x,
                .w = 0.0f
            },
            {
                .x = (xy - wz) * tS.y,
                .y = (1 - (xx + zz)) * tS.y,
                .z = (yz + wx) * tS.y,
                .w = 0.0f
            },
            {
                .x = (xz + wy) * tS.z,
                .y = (yz - wx) * tS.z,
                .z = (1.0f - (xx + yy)) * tS.z,
                .w = 0.0f,
            },
            {
                .x = tV.x,
                .y = tV.y,
                .z = tV.z,
                .w = 1.0f
            }
        }
    };

    return tResult;
}

static inline plMat4
pl_mul_mat4t(const plMat4* ptLeft, const plMat4* ptRight)
{
    return (plMat4){
        
        // row 0
        .x11 = ptLeft->x11 * ptRight->x11 + ptLeft->x12 * ptRight->x21 + ptLeft->x13 * ptRight->x31,
        .x12 = ptLeft->x11 * ptRight->x12 + ptLeft->x12 * ptRight->x22 + ptLeft->x13 * ptRight->x32,
        .x13 = ptLeft->x11 * ptRight->x13 + ptLeft->x12 * ptRight->x23 + ptLeft->x13 * ptRight->x33,
        .x14 = ptLeft->x11 * ptRight->x14 + ptLeft->x12 * ptRight->x24 + ptLeft->x13 * ptRight->x34 + ptLeft->x14,

        // row 1
        .x21 = ptLeft->x21 * ptRight->x11 + ptLeft->x22 * ptRight->x21 + ptLeft->x23 * ptRight->x31,
        .x22 = ptLeft->x21 * ptRight->x12 + ptLeft->x22 * ptRight->x22 + ptLeft->x23 * ptRight->x32,
        .x23 = ptLeft->x21 * ptRight->x13 + ptLeft->x22 * ptRight->x23 + ptLeft->x23 * ptRight->x33,
        .x24 = ptLeft->x21 * ptRight->x14 + ptLeft->x22 * ptRight->x24 + ptLeft->x23 * ptRight->x34 + ptLeft->x24,

        // row 2
        .x31 = ptLeft->x31 * ptRight->x11 + ptLeft->x32 * ptRight->x21 + ptLeft->x33 * ptRight->x31,
        .x32 = ptLeft->x31 * ptRight->x12 + ptLeft->x32 * ptRight->x22 + ptLeft->x33 * ptRight->x32,
        .x33 = ptLeft->x31 * ptRight->x13 + ptLeft->x32 * ptRight->x23 + ptLeft->x33 * ptRight->x33,
        .x34 = ptLeft->x31 * ptRight->x14 + ptLeft->x32 * ptRight->x24 + ptLeft->x33 * ptRight->x34 + ptLeft->x34,

        .x44 = 1.0f
    }; 
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
    
    return (plMat4){
        .col = {
            {
                .x = tR0.x,
                .y = tR1.x,
                .z = tS.x
            },
            {
                .x = tR0.y,
                .y = tR1.y,
                .z = tS.y
            },
            {
                .x = tR0.z,
                .y = tR1.z,
                .z = tS.z
            },
            {
                .x = -pl_dot_vec3(tB, tT),
                .y =  pl_dot_vec3(tA, tT),
                .z = -pl_dot_vec3(tD, tS),
                .w =  1.0f
            }
        }
    };
}

#endif // PL_MATH_INCLUDE_FUNCTIONS