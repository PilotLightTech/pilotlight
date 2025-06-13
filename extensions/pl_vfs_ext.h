/*
   pl_vfs_ext.h
     - simple virtual file system
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
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

        * plFileI (v1.x)
        * plPakI  (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VFS_EXT_H
#define PL_VFS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plVfsI_version {0, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef union _plFileHandle plFileHandle;

// enums/flags
typedef int plOpenFileFlags; // -> enum _plOpenFileFlags // Flag:

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plVfsI
{
    // setup/shutdown
    void (*initialize)(void);
    void (*cleanup)   (void);

    // mounting
    bool (*mount_directory)(const char* directory, const char* physicalDirectory);
    bool (*mount_pak)      (const char* directory, const char* pakFilePath);

    // files
    plFileHandle (*get_file)(const char* file);
    bool         (*open)    (plFileHandle, plOpenFileFlags);
    void         (*close)   (plFileHandle);
    size_t       (*write)   (plFileHandle, const void*, size_t);
    bool         (*read)    (plFileHandle, void*, size_t*);

} plVfsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union _plFileHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plFileHandle;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plOpenFileFlags
{
    PL_OPEN_FILE_FLAGS_NONE  = 0,
    PL_OPEN_FILE_FLAGS_READ  = 1 << 0,
    PL_OPEN_FILE_FLAGS_WRITE = 1 << 1, // doesn't work with pak files currently
};

#endif // PL_VFS_EXT_H