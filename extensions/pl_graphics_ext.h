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
#include "pl_math.h"

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

// 3D drawing api
typedef struct _plDrawList3D        plDrawList3D;
typedef struct _plDrawVertex3DSolid plDrawVertex3DSolid; // single vertex (3D pos + uv + color)
typedef struct _plDrawVertex3DLine  plDrawVertex3DLine; // single vertex (pos + uv + color)

// enums
typedef int pl3DDrawFlags;

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

    // 2D drawing api
    void (*draw_lists)(plGraphics* ptGraphics, uint32_t uListCount, plDrawList* atLists);
    void (*create_font_atlas)(plFontAtlas* ptAtlas);
    void (*destroy_font_atlas)(plFontAtlas* ptAtlas);

    // 3D drawing api
    void (*submit_3d_drawlist)    (plDrawList3D* ptDrawlist, float fWidth, float fHeight, const plMat4* ptMVP, pl3DDrawFlags tFlags);
    void (*register_3d_drawlist)  (plGraphics* ptGraphics, plDrawList3D* ptDrawlist);
    void (*add_3d_triangle_filled)(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_3d_line)           (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_3d_point)          (plDrawList3D* ptDrawlist, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_3d_transform)      (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_3d_frustum)        (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_3d_centered_box)   (plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_3d_bezier_quad)    (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_bezier_cubic)   (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);
} plGraphicsI;

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex3DSolid
{
    float    pos[3];
    uint32_t uColor;
} plDrawVertex3DSolid;

typedef struct _plDrawVertex3DLine
{
    float    pos[3];
    float    fDirection;
    float    fThickness;
    float    fMultiply;
    float    posother[3];
    uint32_t uColor;
} plDrawVertex3DLine;

typedef struct _plDrawList3D
{
    plGraphics*          ptGraphics;
    plDrawVertex3DSolid* sbtSolidVertexBuffer;
    uint32_t*            sbtSolidIndexBuffer;
    plDrawVertex3DLine*  sbtLineVertexBuffer;
    uint32_t*            sbtLineIndexBuffer;
} plDrawList3D;

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
    plDrawList3D** sbt3DDrawlists;
    void* _pInternalData;
} plGraphics;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _pl3DDrawFlags
{
    PL_PIPELINE_FLAG_NONE          = 0,
    PL_PIPELINE_FLAG_DEPTH_TEST    = 1 << 0,
    PL_PIPELINE_FLAG_DEPTH_WRITE   = 1 << 1,
    PL_PIPELINE_FLAG_CULL_FRONT    = 1 << 2,
    PL_PIPELINE_FLAG_CULL_BACK     = 1 << 3,
    PL_PIPELINE_FLAG_FRONT_FACE_CW = 1 << 4,
};

#endif // PL_GRAPHICS_EXT_H