/*
   pl_resource_ext.h
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plGraphicsI      (v1.x)
        * plGPUAllocatorsI (v1.x)
        * plVfsI           (v2.x)
        * plImageI         (v1.x)
        * plDxtI           (v1.x)
        * plDdsI           (v1.x)
        * plPakI           (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_EXT_H
#define PL_RESOURCE_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plResourceI_version {1, 5, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stddef.h>            // size_t
#include <stdbool.h>           // bool
#include "pl_resource_ext.inl" // plResourceHandle

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plResourceManagerInit plResourceManagerInit;

// enums/falgs
typedef int plResourceLoadFlags; // -> enum _plResourceLoadFlags // Flag: resource load flags (PL_RESOURCE_LOAD_FLAG_XXXX)
typedef int plResourceEvictFlags; // -> enum _plResourceEvictFlags // Flag: resource load flags (PL_RESOURCE_LOAD_FLAG_XXXX)

// external
typedef struct _plDevice       plDevice;        // pl_graphics_ext.h
typedef union  plTextureHandle plTextureHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_resource_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_resource_ext(plApiRegistryI*, bool reload);

// setup/shutdown
PL_API void pl_resource_initialize(plResourceManagerInit);
PL_API void pl_resource_cleanup   (void);
PL_API void pl_resource_clear     (void);

// per frame
PL_API void pl_resource_new_frame (void);

// typical usage
//   - file:  file name
//   - flags: specify flags that modify behavior (optional)
PL_API plResourceHandle pl_resource_load(const char* file, plResourceLoadFlags);

// explicit usage
//   - file:              file name
//   - flags:             specify flags that modify behavior (optional)
//   - fileData:          pointer to file data if loaded manually (optional)
//   - fileByteSize:      size of file data, if loaded manually (optional)
//   - containerFileName: if name is not the name of source file, set the source file name here
//   - fileBytesOffset:   if using container_filename, you can give the offset for the resource within the file here
PL_API plResourceHandle pl_resource_load_ex(const char* file, plResourceLoadFlags, uint8_t* fileData, size_t fileByteSize, const char* containerFileName, size_t fileBytesOffset);

// resource query
PL_API void pl_resource_unload   (plResourceHandle);
PL_API bool pl_resource_is_valid (plResourceHandle);
PL_API bool pl_resource_is_loaded(const char* file);

// residency control
PL_API void pl_resource_make_resident(plResourceHandle);
PL_API void pl_resource_evict        (plResourceHandle);
PL_API void pl_resource_evict_ex     (plResourceHandle, plResourceEvictFlags);
PL_API bool pl_resource_is_resident  (plResourceHandle, plResourceEvictFlags);

// resource retrieval
PL_API plTextureHandle pl_resource_get_texture(plResourceHandle);

// misc
PL_API const uint8_t* pl_resource_get_file_data(plResourceHandle, size_t* fileByteSizeOut);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plResourceI
{
    void             (*initialize)   (plResourceManagerInit);
    void             (*cleanup)      (void);
    void             (*clear)        (void);
    void             (*new_frame)    (void);
    plResourceHandle (*load)         (const char* file, plResourceLoadFlags);
    plResourceHandle (*load_ex)      (const char* file, plResourceLoadFlags, uint8_t* fileData, size_t fileByteSize, const char* containerFileName, size_t fileBytesOffset);
    void             (*unload)       (plResourceHandle);
    bool             (*is_valid)     (plResourceHandle);
    bool             (*is_loaded)    (const char* file);
    void             (*make_resident)(plResourceHandle);
    void             (*evict)        (plResourceHandle);
    void             (*evict_ex)     (plResourceHandle, plResourceEvictFlags);
    bool             (*is_resident)  (plResourceHandle, plResourceEvictFlags);
    plTextureHandle  (*get_texture)  (plResourceHandle);
    const uint8_t*   (*get_file_data)(plResourceHandle, size_t* fileByteSizeOut);
} plResourceI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plResourceLoadFlags
{
    PL_RESOURCE_LOAD_FLAG_NONE             = 0,
    PL_RESOURCE_LOAD_FLAG_RETAIN_FILE_DATA = 1 << 0,
    PL_RESOURCE_LOAD_FLAG_BLOCK_COMPRESSED = 1 << 1, // if possible
    PL_RESOURCE_LOAD_FLAG_NO_CACHING       = 1 << 2
};

enum _plResourceEvictFlags
{
    PL_RESOURCE_EVICT_FLAG_NONE           = 0,
    PL_RESOURCE_EVICT_FLAG_DROP_GPU       = 1 << 0, // default
    PL_RESOURCE_EVICT_FLAG_DROP_CACHE     = 1 << 1,
    PL_RESOURCE_EVICT_FLAG_DROP_FILE_DATA = 1 << 2,
    PL_RESOURCE_EVICT_FLAG_DROP_ALL       = PL_RESOURCE_EVICT_FLAG_DROP_GPU | PL_RESOURCE_EVICT_FLAG_DROP_CACHE | PL_RESOURCE_EVICT_FLAG_DROP_FILE_DATA

};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plResourceManagerInit
{
    plDevice*   ptDevice;
    uint32_t    uMaxTextureResolution; // default: 1024
    const char* pcCacheDirectory;      // default: ../cache
} plResourceManagerInit;

#ifdef __cplusplus
}
#endif

#endif // PL_RESOURCE_EXT_H