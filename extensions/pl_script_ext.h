/*
    pl_script_ext.h
      - this is the interface script components must implement
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SCRIPT_EXT_H
#define PL_SCRIPT_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plScriptI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// external 
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h
typedef union  _plEntity           plEntity;           // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plScriptI
{
    // ran when creating a new script component (optional)
    void (*setup)(plComponentLibrary*, plEntity);

    // ran every frame
    void (*run)(plComponentLibrary*, plEntity);
} plScriptI;

#endif // PL_SCRIPT_EXT_H