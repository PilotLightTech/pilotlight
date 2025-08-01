/*
   pl_shader_variant_ext.h
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    Limitations:
        * only a single manifest is supported at the moment
        * bind group layouts are not cleanup up when unloading manifest
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_VARIANT_EXT_H
#define PL_SHADER_VARIANT_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plShaderVariantI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderVariantInit plShaderVariantInit;

// external
typedef union plShaderHandle           plShaderHandle;           // pl_graphics_ext.h
typedef union plComputeShaderHandle    plComputeShaderHandle;    // pl_graphics_ext.h
typedef union plBindGroupLayoutHandle  plBindGroupLayoutHandle;  // pl_graphics_ext.h
typedef union plRenderPassLayoutHandle plRenderPassLayoutHandle; // pl_graphics_ext.h
typedef struct _plGraphicsState        plGraphicsState;          // pl_graphics_ext.h
typedef struct _plDevice               plDevice;                 // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plShaderVariantI
{
    void (*initialize)(plShaderVariantInit);
    void (*cleanup)   (void);

    bool (*load_manifest)  (const char* path);
    bool (*unload_manifest)(const char* path);

    plShaderHandle          (*get_shader)                    (const char* name, const plGraphicsState*, const void* tempConstantData, const plRenderPassLayoutHandle*);
    plComputeShaderHandle   (*get_compute_shader)            (const char* name, const void* tempConstantData);
    plBindGroupLayoutHandle (*get_compute_bind_group_layout) (const char* name, uint32_t index);
    plBindGroupLayoutHandle (*get_graphics_bind_group_layout)(const char* name, uint32_t index);
    plBindGroupLayoutHandle (*get_bind_group_layout)         (const char* name);

    void (*update_stats)(void);

} plShaderVariantI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderVariantInit
{
    plDevice* ptDevice;
} plShaderVariantInit;

#endif // PL_SHADER_VARIANT_EXT_H