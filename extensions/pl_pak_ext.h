/*
   pl_pak_ext.h
     - simple archive reader/writer
*/

/*
Index of this file:
// [SECTION] implementation notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] implementation notes
//-----------------------------------------------------------------------------

/*

    Implementation:
        The provided implementation of this extension depends on the following
        APIs being available:

        * plVfsI      (v2.x)
        * plCompressI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PAK_EXT_H
#define PL_PAK_EXT_H

#ifdef __cplusplus
extern "C" {
#endif

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPakI_version {1, 2, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pl.inc"
#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plPakFile      plPakFile;
typedef struct _plPakInfo      plPakInfo;
typedef struct _plPakEntryInfo plPakEntryInfo;
typedef struct _plPakChildFile plPakChildFile;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// extension loading
PL_API void pl_load_pak_ext  (plApiRegistryI*, bool reload);
PL_API void pl_unload_pak_ext(plApiRegistryI*, bool reload);

// packing
PL_API bool            pl_pak_begin_packing  (const char* file, uint32_t contentVersion, plPakFile**);
PL_API bool            pl_pak_add_from_disk  (plPakFile*, const char* pakPath, const char* filePath, bool compress);
PL_API void            pl_pak_add_from_memory(plPakFile*, const char* pakPath, uint8_t* fileData, size_t fileByteSize, bool compress);
PL_API void            pl_pak_end_packing    (plPakFile**);

// unpacking
PL_API bool            pl_pak_load     (const char* file, plPakInfo*, plPakFile**);
PL_API void            pl_pak_unload   (plPakFile**);
PL_API bool            pl_pak_read_file(plPakFile*, const char* file, uint8_t* bufferOut, size_t* fileByteSizeOut);

// streaming usage
PL_API plPakChildFile* pl_pak_open_file                     (plPakFile*, const char* file);
PL_API void            pl_pak_close_file                    (plPakChildFile*);
PL_API size_t          pl_pak_read_file_stream              (plPakChildFile*, size_t elementSize, size_t elementCount, void* bufferOut);
PL_API size_t          pl_pak_get_file_stream_position      (plPakChildFile*);
PL_API void            pl_pak_reset_file_stream_position    (plPakChildFile*);
PL_API void            pl_pak_set_file_stream_position      (plPakChildFile*, size_t);
PL_API void            pl_pak_increment_file_stream_position(plPakChildFile*, size_t);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plPakI
{
    // packing
    bool (*begin_packing)  (const char* file, uint32_t contentVersion, plPakFile**);
    bool (*add_from_disk)  (plPakFile*, const char* pakPath, const char* filePath, bool compress);
    void (*add_from_memory)(plPakFile*, const char* pakPath, uint8_t* fileData, size_t fileByteSize, bool compress);
    void (*end_packing)    (plPakFile**);

    // unpacking
    bool   (*load)                          (const char* file, plPakInfo*, plPakFile**);
    void   (*unload)                        (plPakFile**);
    bool   (*read_file)                     (plPakFile*, const char* file, uint8_t* bufferOut, size_t* fileByteSizeOut);

    // streaming usage
    plPakChildFile* (*open_file)                     (plPakFile*, const char* file);
    void            (*close_file)                    (plPakChildFile*);
    size_t          (*read_file_stream)              (plPakChildFile*, size_t elementSize, size_t elementCount, void* bufferOut);
    size_t          (*get_file_stream_position)      (plPakChildFile*);
    void            (*reset_file_stream_position)    (plPakChildFile*);
    void            (*set_file_stream_position)      (plPakChildFile*, size_t);
    void            (*increment_file_stream_position)(plPakChildFile*, size_t);

    //-----------------------------DEPRECATED--------------------------------------

    #ifndef PL_DISABLE_OBSOLETE

    bool (*get_file)(plPakFile*, const char* file, uint8_t* bufferOut, size_t* fileByteSizeOut);
    
    #endif // PL_DISABLE_OBSOLETE
} plPakI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plPakEntryInfo
{
    const char* pcFilePath; // path inside pak file
    size_t      szSize;     // uncompressed size in bytes
} plPakEntryInfo;

typedef struct _plPakInfo
{
    uint32_t        uPakVersion;
    uint32_t        uContentVersion;
    uint32_t        uEntryCount;
    plPakEntryInfo* atEntries;
} plPakInfo;

#ifdef __cplusplus
}
#endif

#endif // PL_PAK_EXT_H