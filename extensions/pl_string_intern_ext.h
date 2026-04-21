/*
   pl_string_intern_ext.h
     - string interning extension
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STRING_INTERN_EXT_H
#define PL_STRING_INTERN_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plStringInternI_version {2, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStringRepository plStringRepository;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_string_intern_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_string_intern_ext(plApiRegistryI*, bool reload);

PL_API plStringRepository* pl_string_intern_create_repository (void);
PL_API void                pl_string_intern_destroy_repository(plStringRepository*);

PL_API const char*         pl_string_intern_intern(plStringRepository*, const char* pcString);
PL_API void                pl_string_intern_remove(plStringRepository*, const char* pcString);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plStringInternI
{
    plStringRepository* (*create_repository) (void);
    void                (*destroy_repository)(plStringRepository*);
    
    const char* (*intern)(plStringRepository*, const char* pcString);
    void        (*remove)(plStringRepository*, const char* pcString);

} plStringInternI;

#ifdef __cplusplus
}
#endif

#endif // PL_STRING_INTERN_EXT_H