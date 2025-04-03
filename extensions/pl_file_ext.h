/*
   pl_file_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] APIs
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_FILE_EXT_H
#define PL_FILE_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint8_t
#include <stdbool.h> // bool
#include <stddef.h>  // size_t

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plFileI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// enums
typedef int plFileResult; // -> enum _plFileResult // Enum:

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

typedef struct _plFileI
{

    // simple file ops
    bool         (*exists)(const char* path);
    plFileResult (*remove)(const char* path);
    plFileResult (*copy)  (const char* source, const char* destination);

    // binary files
    plFileResult (*binary_read) (const char* file, size_t* sizeOut, uint8_t* buffer); // pass NULL for buffer to get size
    plFileResult (*binary_write)(const char* file, size_t, uint8_t* buffer);

} plFileI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plFileResult
{
    PL_FILE_RESULT_FAIL    = 0,
    PL_FILE_RESULT_SUCCESS = 1
};

#endif // PL_FILE_EXT_H