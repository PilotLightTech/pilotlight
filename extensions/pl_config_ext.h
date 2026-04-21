/*
   pl_config_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plVfsI (v2.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONFIG_EXT_H
#define PL_CONFIG_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plConfigI_version {1, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConfigSettings plConfigSettings;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_config_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_config_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void     pl_config_initialize(plConfigSettings);
PL_API void     pl_config_cleanup   (void);

// saving/loading
PL_API void     pl_config_load_from_disk(const char* fileName);
PL_API void     pl_config_save_to_disk  (const char* fileName);

// loading
PL_API bool     pl_config_load_bool  (const char* name, bool     defaultValue);
PL_API int      pl_config_load_int   (const char* name, int      defaultValue);
PL_API uint32_t pl_config_load_uint  (const char* name, uint32_t defaultValue);
PL_API double   pl_config_load_double(const char* name, double   defaultValue);
PL_API plVec2   pl_config_load_vec2  (const char* name, plVec2   defaultValue);

// setting
PL_API void     pl_config_set_bool  (const char* name, bool);
PL_API void     pl_config_set_int   (const char* name, int);
PL_API void     pl_config_set_uint  (const char* name, uint32_t);
PL_API void     pl_config_set_double(const char* name, double);
PL_API void     pl_config_set_vec2  (const char* name, plVec2);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plConfigI
{
    // setup/shutdown
    void (*initialize)(plConfigSettings);
    void (*cleanup)   (void);

    // saving/loading
    void (*load_from_disk)(const char* fileName); // if NULL, default: "pl_config.json"
    void (*save_to_disk)  (const char* fileName); // if NULL, default: "pl_config.json"
    
    // loading
    bool     (*load_bool)  (const char* name, bool     defaultValue);
    int      (*load_int)   (const char* name, int      defaultValue);
    uint32_t (*load_uint)  (const char* name, uint32_t defaultValue);
    double   (*load_double)(const char* name, double   defaultValue);
    plVec2   (*load_vec2)  (const char* name, plVec2   defaultValue);

    // setting
    void (*set_bool)  (const char* name, bool);
    void (*set_int)   (const char* name, int);
    void (*set_uint)  (const char* name, uint32_t);
    void (*set_double)(const char* name, double);
    void (*set_vec2)  (const char* name, plVec2);
} plConfigI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plConfigSettings
{
    int _iUnused;
} plConfigSettings;

#ifdef __cplusplus
}
#endif

#endif // PL_CONFIG_EXT_H