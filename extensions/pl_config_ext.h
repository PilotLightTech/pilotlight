/*
   pl_config_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_CONFIG_EXT_H
#define PL_CONFIG_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plConfigI_version {0, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConfigSettings plConfigSettings;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plConfigI
{
    // setup/shutdown
    void (*initialize)(plConfigSettings);
    void (*cleanup)   (void);

    // saving/loading
    void (*load_from_disk)(const char* fileName);
    void (*save_to_disk)  (const char* fileName);
    
    // loading
    bool     (*load_bool)(const char* name, bool defaultValue);
    int      (*load_int)(const char* name, int defaultValue);
    uint32_t (*load_uint)(const char* name, uint32_t defaultValue);
    double   (*load_double)(const char* name, double defaultValue);

    // setting
    void (*set_bool)(const char* name, bool value);
    void (*set_int)(const char* name, int value);
    void (*set_uint)(const char* name, uint32_t value);
    void (*set_double)(const char* name, double value);
} plConfigI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

// basic types
typedef struct _plConfigSettings
{
    int iUnused;
} plConfigSettings;

#endif // PL_CONFIG_EXT_H