/*
   pl_math.h, v0.2 (WIP)
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] includes
// [SECTION] general math
// [SECTION] vector ops
// [SECTION] matrix ops
// [SECTION] implementations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MATH_H
#define PL_MATH_H

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
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <math.h>
#include "pl_math.inc"

//-----------------------------------------------------------------------------
// [SECTION] general math
//-----------------------------------------------------------------------------

static inline float pl_radiansf(float fDegrees)                       { return fDegrees * 0.0174532925f; }
static inline float pl_degreesf(float fRadians)                       { return fRadians * 57.29577951f; }
static inline float pl_maxf    (float fValue1, float fValue2)         { return fValue1 > fValue2 ? fValue1 : fValue2; }
static inline float pl_minf    (float fValue1, float fValue2)         { return fValue1 > fValue2 ? fValue2 : fValue1; }
static inline int   pl_maxi    (int iValue1, int iValue2)             { return iValue1 > iValue2 ? iValue1 : iValue2; }
static inline int   pl_mini    (int iValue1, int iValue2)             { return iValue1 > iValue2 ? iValue2 : iValue1; }
static inline float pl_squaref (float fValue)                         { return fValue * fValue;}
static inline float pl_cubef   (float fValue)                         { return fValue * fValue * fValue;}
static inline float pl_clampf  (float fMin, float fValue, float fMax) { if (fValue < fMin) return fMin; else if (fValue > fMax) return fMax; return fValue; }
static inline float pl_clamp01f(float fValue)                         { return pl_clampf(0.0f, fValue, 1.0f); }

//-----------------------------------------------------------------------------
// [SECTION] vector ops
//-----------------------------------------------------------------------------

// unary ops
static inline float pl_length_sqr_vec2  (plVec2 tVec)                { return sqrtf(pl_squaref(tVec.x) + pl_squaref(tVec.y)); }
static inline float pl_length_sqr_vec3  (plVec3 tVec)                { return sqrtf(pl_squaref(tVec.x) + pl_squaref(tVec.y) + pl_squaref(tVec.z)); }
static inline float pl_length_sqr_vec4  (plVec4 tVec)                { return sqrtf(pl_squaref(tVec.x) + pl_squaref(tVec.y) + pl_squaref(tVec.z) + pl_squaref(tVec.w)); }
static inline float pl_length_vec2      (plVec2 tVec)                { return sqrtf(pl_length_sqr_vec2(tVec)); }
static inline float pl_length_vec3      (plVec3 tVec)                { return sqrtf(pl_length_sqr_vec3(tVec)); }
static inline float pl_length_vec4      (plVec4 tVec)                { return sqrtf(pl_length_sqr_vec4(tVec)); }

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

    return (plMat4){
        .col[0].x = (1.0f - (yy + zz)) * tS.x,
        .col[0].y = (xy - wz) * tS.y,
        .col[0].z = (xz + wy) * tS.z,
        .col[0].w = tV.x,

        .col[1].x = (xy + wz) * tS.x,
        .col[1].y = (1 - (xx + zz)) * tS.y,
        .col[1].z = (yz - wx) * tS.z,
        .col[1].w = tV.y,

        .col[2].x = (xz - wy) * tS.x,
        .col[2].y = (yz + wx) * tS.y,
        .col[2].z = (1.0f - (xx + yy)) * tS.z,
        .col[2].w = tV.z,

        .col[3].x = 0.0f,
        .col[3].y = 0.0f,
        .col[3].z = 0.0f,
        .col[3].w = 1.0f
    };
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

#endif // PL_MATH_H