/*
    pl_script_ext.h
      - this is the interface script components must implement
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SCRIPT_EXT_H
#define PL_SCRIPT_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include "pl_ecs_ext.inl" // plEntity

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plScriptI_version         {0, 1, 0}
#define plScriptInterface_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// ecs components
typedef struct _plScriptComponent plScriptComponent;

// flags
typedef int plScriptFlags;

// external
typedef struct _plComponentLibrary plComponentLibrary; // pl_ecs_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_script_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_script_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plScriptI
{
    // scripts
    plEntity (*create)(plComponentLibrary*, const char* file, plScriptFlags, plScriptComponent**);
    void     (*attach)(plComponentLibrary*, const char* file, plScriptFlags, plEntity, plScriptComponent**);
    void     (*load)  (plComponentLibrary*, plEntity);

    // system setup/shutdown/etc
    void (*register_ecs_components)(void);
    void (*run_update_system)      (plComponentLibrary*);

    // ecs types
    plEcsTypeKey (*get_ecs_type_key)(void);

} plScriptI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plScriptInterface
{
    // called once after the script is successfully loaded (optional)
    void (*setup)(plComponentLibrary*, plEntity);

    // called each update while the script is playing
    void (*run)(plComponentLibrary*, plEntity);
} plScriptInterface;

//-----------------------------------------------------------------------------
// [SECTION] components
//-----------------------------------------------------------------------------

typedef struct _plScriptComponent
{
    plScriptFlags tFlags;
    const char*   pcPath;

    // [INTERNAL]
    const struct _plScriptInterface* _ptApi;
} plScriptComponent;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plScriptFlags
{
    PL_SCRIPT_FLAG_NONE       = 0,
    PL_SCRIPT_FLAG_PLAYING    = 1 << 0,
    PL_SCRIPT_FLAG_PLAY_ONCE  = 1 << 1,
    PL_SCRIPT_FLAG_RELOADABLE = 1 << 2
};

#ifdef __cplusplus
}
#endif

#endif // PL_SCRIPT_EXT_H