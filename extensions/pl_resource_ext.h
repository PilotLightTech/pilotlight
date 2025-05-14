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
// [SECTION] public api structs
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
        * plFileI          (v1.x)
        * plImageI         (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_RESOURCE_EXT_H
#define PL_RESOURCE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define plResourceI_version {0, 2, 1}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

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

// external
typedef struct _plDevice       plDevice;        // pl_graphics_ext.h
typedef union  plTextureHandle plTextureHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plResourceI
{
    // setup/shutdown
    void (*initialize)(plResourceManagerInit);
    void (*cleanup)   (void);
    void (*clear)     (void);
    void (*new_frame) (void);

    // typical usage
    //   - file:  file name
    //   - flags: specify flags that modify behavior (optional)
    plResourceHandle (*load)(const char* file, plResourceLoadFlags);

    // explicit usage
    //   - file:              file name
    //   - flags:             specify flags that modify behavior (optional)
    //   - fileData:          pointer to file data if loaded manually (optional)
    //   - fileByteSize:      size of file data, if loaded manually (optional)
    //   - containerFileName: if name is not the name of source file, set the source file name here
    //   - fileBytesOffset:   if using container_filename, you can give the offset for the resource within the file here
    plResourceHandle (*load_ex)(const char* file, plResourceLoadFlags, uint8_t* fileData, size_t fileByteSize, const char* containerFileName, size_t fileBytesOffset);
    
    // resource query
    bool (*is_valid) (plResourceHandle);
    bool (*is_loaded)(const char* file);

    // resource retrieval
    plTextureHandle (*get_texture)(plResourceHandle);

    // misc
    const uint8_t* (*get_file_data)(plResourceHandle, size_t* fileByteSizeOut);

} plResourceI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plResourceLoadFlags
{
    PL_RESOURCE_LOAD_FLAG_NONE,
    PL_RESOURCE_LOAD_FLAG_RETAIN_FILE_DATA
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plResourceManagerInit
{
    plDevice* ptDevice;
    uint32_t  uMaxTextureResolution; // default: 1024
} plResourceManagerInit;

#endif // PL_RESOURCE_EXT_H