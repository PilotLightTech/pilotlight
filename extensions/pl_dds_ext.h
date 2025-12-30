/*
   pl_dds_ext.h
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] header mess
// [SECTION] APIs
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
	Writing:

    texture data need to be in the following layout in the DDS file, tightly packed:
		- Array slice 0 / cubemap face +X
			- mipmap 0
				- depth slice 0
				- depth slice 1
				- ...
			- mipmap 1
				- depth slice 0
				- depth slice 1
				- ...
			- ...
		- Array slice 1 / cubemap face -X
			- mipmap 0
				- depth slice 0
				- depth slice 1
				- ...
			- mipmap 1
				- depth slice 0
				- depth slice 1
				- ...
			- ...
		- ...
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DDS_EXT_H
#define PL_DDS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] APIs
//-----------------------------------------------------------------------------

#define plDdsI_version {1, 0, 0}

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDdsReadInfo  plDdsReadInfo;
typedef struct _plDdsWriteInfo plDdsWriteInfo;
typedef struct _plDdsMipInfo   plDdsMipInfo;

// external
typedef int plFormat;      // pl_graphics_ext.h
typedef int plTextureType; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDdsI
{
    uint32_t (*get_header_size)(void);
    bool     (*read_info)      (uint8_t*, plDdsReadInfo*);
    void     (*write_info)     (uint8_t*, const plDdsWriteInfo*);
} plDdsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDdsMipInfo
{
    uint32_t uSize;     // size in bytes of this mip level
    uint32_t uOffset;   // relative to slice offset
    uint32_t uRowPitch; // mip data should be copied using this
    uint32_t uWidth;    // width of this mip level
    uint32_t uHeight;   // height of this mip level
} plDdsMipInfo;

typedef struct _plDdsReadInfo
{
    uint32_t      uOffset;       // offset into file (where data begins)
    uint32_t      uSize;         // size in bytes of entire image including slices & mips
    uint32_t      uWidth;        // width of mip 0
    uint32_t      uHeight;       // height of mip 0
    uint32_t      uDepth;        // depth of mip 0
    uint32_t      uMips;         // mip map count
    uint32_t      uLayers;       // array slice count
    plFormat      tFormat;       // texture format
    plTextureType tType;         // texture type
    uint32_t      uSliceSize;    // slice size in bytes (all mips of individual layer)
    plDdsMipInfo  atMipInfo[16]; // information for each mip level
} plDdsReadInfo;

typedef struct _plDdsWriteInfo
{
    uint32_t      uWidth;  // width of mip 0
    uint32_t      uHeight; // height of mip 0
    uint32_t      uDepth;  // depth of mip 0
    uint32_t      uMips;   // mip map count
    uint32_t      uLayers; // layer or slice count
    plFormat      tFormat; // texture format
    plTextureType tType;   // texture type
} plDdsWriteInfo;

#endif // PL_DDS_EXT_H