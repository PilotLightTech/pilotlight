#ifndef PL_SHADER_INTEROP_H
#define PL_SHADER_INTEROP_H

#ifdef PL_SHADER_CODE
    #define PL_BEGIN_STRUCT(X) struct X {
    #define PL_END_STRUCT(X) };
    #define PL_TYPEDEF(X, Y)
#else
    #include "pl_math.h"
    #define uint uint32_t
    #define vec2 plVec2
    #define vec3 plVec3
    #define vec4 plVec4
    #define mat3 plMat3
    #define mat4 plMat4
    #define PL_BEGIN_STRUCT(X) typedef struct _##X {
    #define PL_END_STRUCT(X) } X;
    #define PL_TYPEDEF(X, Y) typedef X Y;
#endif

#endif // PL_SHADER_INTEROP_H