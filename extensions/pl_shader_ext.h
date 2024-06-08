/*
   pl_shader_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_EXT_H
#define PL_SHADER_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_SHADER "PL_API_SHADER"
typedef struct _plShaderI plShaderI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderExtInit plShaderExtInit;

// external
typedef struct _plShaderModule plShaderModule; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plShaderI
{
    void (*initialize)(const plShaderExtInit*);

    plShaderModule (*compile_glsl)(const char* pcShader, const char* pcEntryFunc);
} plShaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderExtInit
{
    const char* pcIncludeDirectory;
} plShaderExtInit;

#endif // PL_SHADER_EXT_H