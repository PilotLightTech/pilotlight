/*
   pl_shader_tools_ext.h
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
        * plShaderI   (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_TOOLS_EXT_H
#define PL_SHADER_TOOLS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plShaderToolsI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] include
//-----------------------------------------------------------------------------

#include <stdint.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderToolsInit plShaderToolsInit;

// external
typedef union plShaderHandle        plShaderHandle;        // pl_graphics_ext.h
typedef union plComputeShaderHandle plComputeShaderHandle; // pl_graphics_ext.h
typedef struct _plGraphicsState     plGraphicsState;       // pl_graphics_ext.h
typedef struct _plDevice            plDevice;              // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plShaderToolsI
{
    //----------------------------setup/shutdown-----------------------------------

    void (*initialize)(plShaderToolsInit);
    void (*cleanup)   (void);

    //-----------------------graphics shader variants------------------------------

    // creates or retrieves variant of shader
    plShaderHandle (*get_variant)   (plShaderHandle, plGraphicsState, const void* tempConstantData);
    void           (*reset_variants)(plShaderHandle); // queues variants for deletion
    void           (*clear_variants)(plShaderHandle); // queues variants for deletion & unregisters parent shader

    //-----------------------compute shader variants-------------------------------

    // creates or retrieves variant of shader
    plComputeShaderHandle (*get_compute_variant)   (plComputeShaderHandle, const void* tempConstantData);
    void                  (*reset_compute_variants)(plComputeShaderHandle); // queues variants for deletion
    void                  (*clear_compute_variants)(plComputeShaderHandle); // queues variants for deletion & unregisters parent shader

    //----------------------------miscellaneous------------------------------------

    void (*update_stats)(void);

} plShaderToolsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderToolsInit
{
    plDevice* ptDevice;
} plShaderToolsInit;

#endif // PL_SHADER_TOOLS_EXT_H