/*
   pl.inc
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] forward declarations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_INC
#define PL_INC

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifdef __cplusplus
    #define PL_ZERO_INIT {}
    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT extern "C" __declspec(dllexport)
        #define PL_CALL_CONVENTION (__cdecl *)
    #elif defined(__GNUC__) //  GCC or clang
        #define PL_EXPORT extern "C" __attribute__((visibility("default")))
        #define PL_CALL_CONVENTION (__attribute__(()) *)
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #define PL_CALL_CONVENTION
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#else
    #define PL_ZERO_INIT {0}
    #if defined(_MSC_VER) //  Microsoft 
        #define PL_EXPORT __declspec(dllexport)
        #define PL_CALL_CONVENTION (__cdecl *)
    #elif defined(__GNUC__) //  GCC or clang
        #define PL_EXPORT __attribute__((visibility("default")))
        #define PL_CALL_CONVENTION (__attribute__(()) *)
    #else //  do nothing and hope for the best?
        #define PL_EXPORT
        #define PL_CALL_CONVENTION
        #pragma warning Unknown dynamic link import/export semantics.
    #endif
#endif

#define PL_API PL_EXPORT 

#define PL_FOURCC(a, b, c, d) \
     ((uint32_t)(a)       | \
     ((uint32_t)(b) << 8) | \
     ((uint32_t)(c) << 16) | \
     ((uint32_t)(d) << 24))

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

typedef struct _plApiRegistryI plApiRegistryI;

#endif // PL_INC
