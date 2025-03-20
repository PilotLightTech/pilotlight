/*
   pl_string_intern_ext.h
     - string interning extension
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_STRING_INTERN_EXT_H
#define PL_STRING_INTERN_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plStringInternI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plStringRepository plStringRepository;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plStringInternI
{
    plStringRepository* (*create_string_repository) (void);
    void                (*destroy_string_repository)(plStringRepository*);
    
    const char* (*intern)(plStringRepository*, const char* pcString);
    void        (*remove)(plStringRepository*, const char* pcString);

} plStringInternI;

#endif // PL_STRING_INTERN_EXT_H