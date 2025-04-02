/*
   pl_config_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONFIG_EXT_H
#define PL_CONFIG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plConfigI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConfigSettings plConfigSettings;

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

#endif // PL_CONFIG_EXT_H