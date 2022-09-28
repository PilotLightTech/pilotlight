/*
   pl.h
     * settings & common functions
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] settings
// [SECTION] helper macros
// [SECTION] misc
*/

#ifndef PL_H
#define PL_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint32_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] settings
//-----------------------------------------------------------------------------

#define PL_MAX_FRAMES_IN_FLIGHT 2
#define PL_MAX_NAME_LENGTH 1024

//-----------------------------------------------------------------------------
// [SECTION] helper macros
//-----------------------------------------------------------------------------

#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert(x)
#endif

//-----------------------------------------------------------------------------
// [SECTION] misc
//-----------------------------------------------------------------------------

#ifdef _WIN32
#define PL_VULKAN_BACKEND
#elif defined(__APPLE__)
#define PL_METAL_BACKEND
#else // linux
#define PL_VULKAN_BACKEND
#endif

#endif // PL_H