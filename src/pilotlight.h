/*
   pilotlight.h
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] settings
// [SECTION] helper macros
// [SECTION] misc
*/

#ifndef PILOTLIGHT_H
#define PILOTLIGHT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl_config.h"
#include <stdint.h>  // uint32_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] helper macros
//-----------------------------------------------------------------------------

#ifndef PL_ASSERT
    #include <assert.h>
    #define PL_ASSERT(x) assert((x))
#endif

#if defined(_MSC_VER) //  Microsoft 
    #define PL_EXPORT __declspec(dllexport)
#elif defined(__GNUC__) //  GCC
    #define PL_EXPORT __attribute__((visibility("default")))
#else //  do nothing and hope for the best?
    #define PL_EXPORT
    #pragma warning Unknown dynamic link import/export semantics.
#endif

//-----------------------------------------------------------------------------
// [SECTION] misc
//-----------------------------------------------------------------------------

#ifdef PL_USE_STB_SPRINTF
    #include "stb_sprintf.h"
#endif

#ifndef pl_sprintf
#ifdef PL_USE_STB_SPRINTF
    #define pl_sprintf stbsp_sprintf
    #define pl_vsprintf stbsp_vsprintf
#else
    #define pl_sprintf sprintf
    #define pl_vsprintf vsprintf
#endif
#endif

#endif // PILOTLIGHT_H