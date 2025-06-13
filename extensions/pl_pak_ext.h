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

        * plFileI     (v1.x)
        * plCompressI (v1.x)
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_PAK_EXT_H
#define PL_PAK_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plPakI_version {1, 0, 0}

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
typedef struct _plPakFile      plPakFile;
typedef struct _plPakInfo      plPakInfo;
typedef struct _plPakEntryInfo plPakEntryInfo;

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
    bool (*load)    (const char* file, plPakInfo*, plPakFile**);
    void (*unload)  (plPakFile**);
    bool (*get_file)(plPakFile*, const char* file, uint8_t* bufferOut, size_t* fileByteSizeOut);

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


#endif // PL_PAK_EXT_H