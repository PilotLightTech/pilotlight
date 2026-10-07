/*
   pl_shader_library_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_SHADER_LIBRARY_EXT_H
#define PL_SHADER_LIBRARY_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plShaderLibraryI_version {0, 3, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plShaderLibraryInit        plShaderLibraryInit;
typedef struct _plShaderLibrary            plShaderLibrary;
typedef struct _plShaderVariantDesc        plShaderVariantDesc;
typedef struct _plComputeShaderVariantDesc plComputeShaderVariantDesc;

// external
typedef union plShaderHandle           plShaderHandle;          // pl_graphics_ext.h
typedef union plComputeShaderHandle    plComputeShaderHandle;   // pl_graphics_ext.h
typedef union plBindGroupLayoutHandle  plBindGroupLayoutHandle; // pl_graphics_ext.h
typedef struct _plGraphicsState        plGraphicsState;         // pl_graphics_ext.h
typedef struct _plDevice               plDevice;                // pl_graphics_ext.h
typedef struct _plRenderAttachmentInfo plRenderAttachmentInfo;  // pl_graphics_ext.h
typedef struct _plShaderDesc           plShaderDesc;            // pl_graphics_ext.h
typedef struct _plComputeShaderDesc    plComputeShaderDesc;     // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_shader_library_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_shader_library_ext(plApiRegistryI*, bool reload);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plShaderLibraryI
{
    void (*initialize)(plShaderLibraryInit);

    // library creation/destruction
    plShaderLibrary* (*create_library) (void);
    void             (*cleanup_library)(plShaderLibrary*);

    // registering/unregistering
    void (*register_shader)           (plShaderLibrary*, const char* name, const plShaderDesc*);
    void (*register_compute_shader)   (plShaderLibrary*, const char* name, const plComputeShaderDesc*);
    void (*register_bind_group_layout)(plShaderLibrary*, const char* name, plBindGroupLayoutHandle);
    void (*unregister_shader)         (plShaderLibrary*, const char* name);
    void (*unregister_compute_shader) (plShaderLibrary*, const char* name);
    void (*unregister_shaders)        (plShaderLibrary*);
    void (*unregister_compute_shaders)(plShaderLibrary*);

    // retrieval/variants
    plShaderHandle          (*get_shader)           (plShaderLibrary*, const char* name, const plShaderVariantDesc*);
    plComputeShaderHandle   (*get_compute_shader)   (plShaderLibrary*, const char* name, const plComputeShaderVariantDesc*);
    plBindGroupLayoutHandle (*get_bind_group_layout)(plShaderLibrary*, const char* name);

} plShaderLibraryI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plShaderLibraryInit
{
    plDevice* ptDevice;
} plShaderLibraryInit;

typedef struct _plShaderVariantDesc
{
    const plGraphicsState*        ptGraphicsState;
    const plRenderAttachmentInfo* ptAttachmentInfo;
    const void*                   pVertexConstants;
    const void*                   pFragmentConstants;
} plShaderVariantDesc;

typedef struct _plComputeShaderVariantDesc
{
    const void* pConstants;
} plComputeShaderVariantDesc;

#ifdef __cplusplus
}
#endif

#endif // PL_SHADER_LIBRARY_EXT_H