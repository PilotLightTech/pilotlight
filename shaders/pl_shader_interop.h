#ifndef PL_SHADER_INTEROP_H
#define PL_SHADER_INTEROP_H

#ifndef PL_SHADER_CODE
    #include "pl_math.h"

    // type conversions
    #define uint uint32_t
    #define vec2 plVec2
    #define vec3 plVec3
    #define vec4 plVec4
    #define mat3 plMat3
    #define mat4 plMat4

    // structs
    #define PL_BEGIN_STRUCT(X) typedef struct _##X {
    #define PL_END_STRUCT(X) } X;

    // enums
    #define PL_BEGIN_ENUM(X) typedef int X; enum _##X {
    #define PL_ENUM_ITEM(X, Y) X = Y,
    #define PL_END_ENUM };

#else
    #define PL_BEGIN_STRUCT(X) struct X {
    #define PL_END_STRUCT(X) };
    #define PL_BEGIN_ENUM(X)
    #define PL_ENUM_ITEM(X, Y) const int X = Y;
    #define PL_END_ENUM
#endif

#endif // PL_SHADER_INTEROP_H