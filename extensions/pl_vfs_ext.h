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
    
    Limitations:
        Currently, pak files can't be written to. This is a limitation for the
        current plPakI version. This limitation will be removed in the near 
        future.
        
    Future Capabilities:
        Several new capabilities are planned for the near future including:

        * file time information (i.e. creation, modifications)
        * directory operations & querying
        * file searching
        * missing file ops (i.e. move, rename, copy)
        * additional error codes
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_VFS_EXT_H
#define PL_VFS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plVfsI_version {2, 0, 0}

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
typedef union _plVfsFileHandle plVfsFileHandle;

// enums/flags
typedef int plVfsFileMode;   // -> enum _plVfsFileMode   // Enum:
typedef int plVfsMountFlags; // -> enum _plVfsMountFlags // Flags:
typedef int plVfsResult;     // -> enum _plVfsResult     // Enum

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plVfsI
{
    // mounting
    plVfsResult (*mount_directory)(const char* directory, const char* physicalDirectory, plVfsMountFlags);
    plVfsResult (*mount_pak)      (const char* directory, const char* pakFilePath, plVfsMountFlags);
    plVfsResult (*mount_memory)   (const char* directory, plVfsMountFlags);

    // basic file usage
    bool            (*does_file_exist)  (const char* file);
    size_t          (*get_file_size_str)(const char* file);
    plVfsFileHandle (*register_file)    (const char* file, bool mustExist);
    plVfsFileHandle (*open_file)        (const char* file, plVfsFileMode);
    void            (*close_file)       (plVfsFileHandle);
    plVfsResult     (*delete_file)      (plVfsFileHandle);
    bool            (*is_file_open)     (plVfsFileHandle);
    const char*     (*get_real_path)    (plVfsFileHandle);
    bool            (*is_file_valid)    (plVfsFileHandle);
    plVfsResult     (*read_file)        (plVfsFileHandle, void*, size_t*);
    size_t          (*write_file)       (plVfsFileHandle, const void*, size_t);

    // stream usage
    size_t (*get_file_stream_position)      (plVfsFileHandle);
    void   (*reset_file_stream_position)    (plVfsFileHandle);
    void   (*set_file_stream_position)      (plVfsFileHandle, size_t);
    void   (*increment_file_stream_position)(plVfsFileHandle, size_t);
    size_t (*read_file_stream)              (plVfsFileHandle, size_t elementSize, size_t elementCount, void*);
    size_t (*write_file_stream)             (plVfsFileHandle, size_t elementSize, size_t elementCount, void*);
} plVfsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union _plVfsFileHandle
{
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    };
    uint64_t uData;
} plVfsFileHandle;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plVfsFileMode
{
    PL_VFS_FILE_MODE_READ = 0,
    PL_VFS_FILE_MODE_WRITE,       // doesn't work with pak files currently
    PL_VFS_FILE_MODE_APPEND,      // doesn't work with pak files currently
    PL_VFS_FILE_MODE_READ_WRITE,  // doesn't work with pak files currently
    PL_VFS_FILE_MODE_READ_APPEND, // doesn't work with pak files currently
};

enum _plVfsMountFlags
{
    PL_VFS_MOUNT_FLAGS_NONE = 0
};

enum _plVfsResult
{
    PL_VFS_RESULT_FAIL    = 0,
    PL_VFS_RESULT_SUCCESS = 1
};

#endif // PL_VFS_EXT_H