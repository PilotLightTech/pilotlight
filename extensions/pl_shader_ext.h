/*
   pl_shader_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_EXT_H
#define PL_SHADER_EXT_H

// extension version (format XYYZZ)
#define PL_SHADER_EXT_VERSION    "1.0.0"
#define PL_SHADER_EXT_VERSION_NUM 10000

// compile-time options
// #define PL_OFFLINE_SHADERS_ONLY
#ifndef PL_MAX_SHADER_INCLUDE_DIRECTORIES
    #define PL_MAX_SHADER_INCLUDE_DIRECTORIES 16
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define PL_API_SHADER "PL_API_SHADER"
typedef struct _plShaderI plShaderI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderOptions         plShaderOptions;
typedef struct _plShaderMacroDefinition plShaderMacroDefinition;

// enums
typedef int plShaderFlags;             // -> enum _plShaderFlags // Flag:
typedef int plShaderOptimizationLevel; // -> enum _plShaderOptimizationLevel // Enum:

// external
typedef struct _plShaderModule plShaderModule; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plShaderI
{
    // setup
    bool (*initialize)(const plShaderOptions*);

    // load shader (compile if not already compiled)
    plShaderModule (*load_glsl)(const char* pcShader, const char* pcEntryFunc, const char* pcFile, plShaderOptions*);

    // compilation (pass null shader options to use default)
    plShaderModule (*compile_glsl) (const char* pcShader, const char* pcEntryFunc, plShaderOptions*);
    
    // write/read on disk
    void           (*write_to_disk) (const char* pcShader, const plShaderModule*);
    plShaderModule (*read_from_disk)(const char* pcShader, const char* pcEntryFunc);
} plShaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderMacroDefinition
{
    const char* pcName;
    size_t      szNameLength; // do NOT count null termination character
    const char* pcValue;
    size_t      szValueLength; // do NOT count null termination character
} plShaderMacroDefinition;

typedef struct _plShaderOptions
{
    plShaderFlags                  tFlags;
    plShaderOptimizationLevel      tOptimizationLevel;
    const plShaderMacroDefinition* ptMacroDefinitions;
    uint32_t                       uMacroDefinitionCount;
    const char*                    apcIncludeDirectories[PL_MAX_SHADER_INCLUDE_DIRECTORIES];
    uint32_t                       uIncludeDirectoriesCount;
} plShaderOptions;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plShaderFlags
{
    PL_SHADER_FLAGS_NONE           = 0,
    PL_SHADER_FLAGS_INCLUDE_DEBUG  = 1 << 0, // include debug information
    PL_SHADER_FLAGS_ALWAYS_COMPILE = 1 << 1, // ignore cached shader modules
    PL_SHADER_FLAGS_NEVER_CACHE    = 1 << 2, // never cache shaders to disk
};

enum _plShaderOptimizationLevel
{
    PL_SHADER_OPTIMIZATION_NONE,
    PL_SHADER_OPTIMIZATION_SIZE,
    PL_SHADER_OPTIMIZATION_PERFORMANCE
};

#endif // PL_SHADER_EXT_H