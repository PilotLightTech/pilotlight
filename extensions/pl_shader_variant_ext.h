/*
   pl_shader_variant_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:
        
        * plGraphicsI (v1.x)
        * plStatsI    (v1.x)
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
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderVariantInit plShaderVariantInit;

// external
typedef union plShaderHandle        plShaderHandle;        // pl_graphics_ext.h
typedef union plComputeShaderHandle plComputeShaderHandle; // pl_graphics_ext.h
typedef struct _plGraphicsState     plGraphicsState;       // pl_graphics_ext.h
typedef struct _plDevice            plDevice;              // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plShaderVariantI
{
    //----------------------------setup/shutdown-----------------------------------

    void (*initialize)(plShaderVariantInit);
    void (*cleanup)   (void);

    //---------------------------graphics shaders----------------------------------

    // creates or retrieves variant of shader
    plShaderHandle (*get_variant)   (plShaderHandle, plGraphicsState, const void* tempConstantData);
    void           (*reset_variants)(plShaderHandle); // queues variants for deletion
    void           (*clear_variants)(plShaderHandle); // queues variants for deletion & unregisters parent shader

    //---------------------------compute shaders-----------------------------------

    // creates or retrieves variant of shader
    plComputeShaderHandle (*get_compute_variant)   (plComputeShaderHandle, const void* tempConstantData);
    void                  (*reset_compute_variants)(plComputeShaderHandle); // queues variants for deletion
    void                  (*clear_compute_variants)(plComputeShaderHandle); // queues variants for deletion & unregisters parent shader

    //----------------------------miscellaneous------------------------------------

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