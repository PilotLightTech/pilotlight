/*
   pl_graphics_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] public api structs
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_GRAPHICS_EXT_H
#define PL_GRAPHICS_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_GRAPHICS "PL_API_GRAPHICS"
typedef struct _plGraphicsI plGraphicsI;

#define PL_API_DEVICE "PL_API_DEVICE"
typedef struct _plDeviceI plDeviceI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDevice        plDevice;
typedef struct _plBuffer        plBuffer;
typedef struct _plCommandBuffer plCommandBuffer;

typedef struct _plGraphics      plGraphics;
typedef struct _plDraw          plDraw;
typedef struct _plDrawArea      plDrawArea;
typedef struct _plMesh          plMesh;

// external
typedef struct _plDrawList plDrawList;
typedef struct _plFontAtlas plFontAtlas;

//-----------------------------------------------------------------------------
// [SECTION] public api structs
//-----------------------------------------------------------------------------

typedef struct _plDeviceI
{
    // commited resources
    uint32_t (*create_index_buffer) (plDevice* ptDevice, size_t szSize, const void* pData, const char* pcName);
    uint32_t (*create_vertex_buffer)(plDevice* ptDevice, size_t szSize, size_t szStride, const void* pData, const char* pcName);
} plDeviceI;

typedef struct _plGraphicsI
{
    void (*initialize)(plGraphics* ptGraphics);
    void (*resize)    (plGraphics* ptGraphics);
    void (*cleanup)   (plGraphics* ptGraphics);

    // per frame
    bool (*begin_frame)    (plGraphics* ptGraphics);
    void (*end_frame)      (plGraphics* ptGraphics);
    void (*begin_recording)(plGraphics* ptGraphics);
    void (*end_recording)  (plGraphics* ptGraphics);

    // drawing
    void (*draw_areas)(plGraphics* ptGraphics, uint32_t uAreaCount, plDrawArea* atAreas, plDraw* atDraws);

    // drawing api
    void (*draw_lists)(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists);
    void (*create_font_atlas)(plFontAtlas* ptAtlas);
    void (*destroy_font_atlas)(plFontAtlas* ptAtlas);

} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plMesh
{
    uint32_t uVertexBuffer;
    uint32_t uIndexBuffer;
    uint32_t uVertexOffset;
    uint32_t uVertexCount;
    uint32_t uIndexOffset;
    uint32_t uIndexCount;
    uint64_t ulVertexStreamMask; // PL_MESH_FORMAT_FLAG_*
} plMesh;

typedef struct _plDrawArea
{
    // VkViewport   tViewport;
    // VkRect2D     tScissor;
    // plBindGroup* ptBindGroup0;
    // uint32_t     uDynamicBufferOffset0;
    uint32_t     uDrawOffset;
    uint32_t     uDrawCount;
} plDrawArea;

typedef struct _plDraw
{
    plMesh*      ptMesh;
    // uint32_t     uShaderVariant;
    // plBindGroup* aptBindGroups[2];
    // uint32_t     auDynamicBufferOffset[2];
} plDraw;

typedef struct _plBuffer
{
    void* pBuffer;
} plBuffer;

typedef struct _plCommandBuffer
{
    void* _pInternalData;
} plCommandBuffer;

typedef struct _plDevice
{

    plBuffer* sbtBuffers;

    void* _pInternalData;
} plDevice;

typedef struct _plGraphics
{
    plDevice tDevice;
    void* _pInternalData;
} plGraphics;

#endif // PL_GRAPHICS_EXT_H