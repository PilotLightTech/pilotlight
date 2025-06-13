/*
   pl_compress_ext.h
     - DEFLATE-style sliding-window dictionary compression
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] public api struct
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_COMPRESS_EXT_H
#define PL_COMPRESS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plCompressI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h> // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plCompressI
{
    // compression
    //   Compresses data. It is recommended to make the dataOut buffer twice
    //   the size of the input buffer just to be safe. You can run the
    //   function with "dataOut = NULL" and it will return the minimum size of
    //   the dataOut, however this is expense since it must do the work of the
    //   full compression algorithm. You can also just guess but if you do,
    //   make sure to check if the return value is <= the size of the out buffer
    //   if not, try again with a larger buffer. The function is safe in that it
    //   will stop writing to the buffer after "sizeOut" is reached.
    uint32_t (*compress)(uint8_t* dataIn, uint32_t sizeIn, uint8_t* dataOut, uint32_t sizeOut);

    // decompression
    //   Decompresses data compressed using the "compress" function above.
    //   To find the required size of the "dataOut" buffer, run the function
    //   with "dataOut = NULL" first, and the function will return the decompressed
    //   size in bytes.
    uint32_t (*decompress)(uint8_t* dataIn, uint32_t sizeIn, uint8_t* dataOut, uint32_t sizeOut);
} plCompressI;

#endif // PL_COMPRESS_EXT_H