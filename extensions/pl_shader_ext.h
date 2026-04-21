/*
   pl_shader_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plVfsI          (v2.x)
        * plLogI          (v1.x)
        * plScreenLogI    (v2.x)
        * plStringInternI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_EXT_H
#define PL_SHADER_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

// compile-time options
// #define PL_OFFLINE_SHADERS_ONLY
#ifndef PL_MAX_SHADER_INCLUDE_DIRECTORIES
    #define PL_MAX_SHADER_INCLUDE_DIRECTORIES 16
#endif

#ifndef PL_MAX_SHADER_DIRECTORIES
    #define PL_MAX_SHADER_DIRECTORIES 16
#endif

#ifndef PL_MAX_SHADER_MACRO_DEFINITIONS
    #define PL_MAX_SHADER_MACRO_DEFINITIONS 16
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plShaderI_version {1, 4, 0}

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
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_shader_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_shader_ext(plApiRegistryI*, bool reload);

// setup
PL_API bool                   pl_shader_initialize (const plShaderOptions*);
PL_API void                   pl_shader_cleanup    (void);

// settings
PL_API void                   pl_shader_set_options(const plShaderOptions*);
PL_API const plShaderOptions* pl_shader_get_options(void);

// load shader (compile if not already compiled)
PL_API plShaderModule         pl_shader_load_glsl(const char* shader, const char* entryFunc, const char* file, plShaderOptions*);

// compilation (pass null shader options to use default)
PL_API plShaderModule         pl_shader_compile_glsl(const char* shader, const char* entryFunc, plShaderOptions*);

// write/read on disk
PL_API void                   pl_shader_write_to_disk (const char* shader, const plShaderModule*);
PL_API plShaderModule         pl_shader_read_from_disk(const char* shader, const char* entryFunc);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plShaderI
{
    // setup
    bool (*initialize) (const plShaderOptions*);
    void (*cleanup)(void);

    // settings
    void                   (*set_options)(const plShaderOptions*);
    const plShaderOptions* (*get_options)(void);

    // load shader (compile if not already compiled)
    plShaderModule (*load_glsl)(const char* shader, const char* entryFunc, const char* file, plShaderOptions*);

    // compilation (pass null shader options to use default)
    plShaderModule (*compile_glsl) (const char* shader, const char* entryFunc, plShaderOptions*);
    
    // write/read on disk
    void           (*write_to_disk) (const char* shader, const plShaderModule*);
    plShaderModule (*read_from_disk)(const char* shader, const char* entryFunc);
} plShaderI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderMacroDefinition
{
    const char* pcName;
    const char* pcValue;
} plShaderMacroDefinition;

typedef struct _plShaderOptions
{
    plShaderFlags             tFlags;
    plShaderOptimizationLevel tOptimizationLevel;
    plShaderMacroDefinition   atMacroDefinitions[PL_MAX_SHADER_MACRO_DEFINITIONS];
    const char*               apcIncludeDirectories[PL_MAX_SHADER_INCLUDE_DIRECTORIES + 1];
    const char*               apcDirectories[PL_MAX_SHADER_DIRECTORIES + 1];
    const char*               pcCacheOutputDirectory;

    // [INTERNAL]
    uint32_t _uDirectoriesCount;
    uint32_t _uIncludeDirectoriesCount;
    uint32_t _uMacroDefinitionCount;
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
    PL_SHADER_FLAGS_METAL_OUTPUT   = 1 << 3, // output metal shader
    PL_SHADER_FLAGS_SPIRV_OUTPUT   = 1 << 4, // output SPIRV shader
    PL_SHADER_FLAGS_AUTO_OUTPUT    = 1 << 5, // output SPIRV or metal shader code depending on backend
};

enum _plShaderOptimizationLevel
{
    PL_SHADER_OPTIMIZATION_NONE,
    PL_SHADER_OPTIMIZATION_SIZE,
    PL_SHADER_OPTIMIZATION_PERFORMANCE
};

#ifdef __cplusplus
}
#endif

#endif // PL_SHADER_EXT_H