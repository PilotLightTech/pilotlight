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
        The provided implementation retrieves the following APIs. Core services
        are included; this lists direct dependencies, not their dependencies.

        * plCompressI (v1.1.1+)
        * plFileI (v2.1+)
        * plMemoryI (v1.x)
        * plVfsI (v3.x)
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

#define plPakI_version {2, 0, 0}

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

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plPakI
{
    // Packing writes a temporary sibling; end_packing atomically replaces the destination.
    // Packing failures mark the writer failed. end_packing reports failure,
    // closes/frees the writer and clears the caller pointer even on failure.
    bool (*begin_packing)  (const char* file, uint32_t contentVersion, plPakFile**);
    bool (*add_from_disk)  (plPakFile*, const char* pakPath, const char* filePath, bool compress);
    bool (*add_from_memory)(plPakFile*, const char* pakPath, uint8_t* fileData, size_t fileByteSize, bool compress);
    bool (*end_packing)    (plPakFile**);

    // unpacking
    bool   (*load)      (const char* file, plPakInfo*, plPakFile**);
    void   (*unload)    (plPakFile**);
    bool   (*read_file) (plPakFile*, const char* file, uint8_t* bufferOut, size_t* fileByteSizeOut);

    // streaming usage: independent handles; close before unload (unload invalidates all).
    // read_file_stream returns complete elements, matching fread. Seeks stay inside the entry.
    plPakChildFile* (*open_file)                     (plPakFile*, const char* file);
    void            (*close_file)                    (plPakChildFile*);
    size_t          (*read_file_stream)              (plPakChildFile*, size_t elementSize, size_t elementCount, void* bufferOut);
    size_t          (*get_file_stream_position)      (plPakChildFile*);
    void            (*reset_file_stream_position)    (plPakChildFile*);
    bool            (*set_file_stream_position)      (plPakChildFile*, size_t);
    bool            (*increment_file_stream_position)(plPakChildFile*, size_t);

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
