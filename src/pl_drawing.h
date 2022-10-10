/*
   pl_drawing.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] structs
*/

#ifndef PL_DRAWING_H
#define PL_DRAWING_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
#define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_MAX_FRAMES_IN_FLIGHT
#define PL_MAX_FRAMES_IN_FLIGHT 2
#endif

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert(x)
#endif

#ifndef PL_ALLOC
#include <stdlib.h>
#define PL_ALLOC(x) malloc(x)
#endif

#ifndef PL_FREE
#define PL_FREE(x) free(x)
#endif

#ifdef PL_USE_STB_SPRINTF
#include "stb_sprintf.h"
#endif

#ifndef pl_sprintf
#ifdef PL_USE_STB_SPRINTF
    #define pl_sprintf stbsp_sprintf
    #define pl_vsprintf stbsp_vsprintf
#else
    #define pl_sprintf sprintf
    #define pl_vsprintf vsprintf
#endif
#endif

#ifndef PL_DECLARE_STRUCT
#define PL_DECLARE_STRUCT(name) typedef struct name ##_t name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h> // uint*_t
#include <stdbool.h>

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

PL_DECLARE_STRUCT(plDrawContext);
PL_DECLARE_STRUCT(plDrawList);
PL_DECLARE_STRUCT(plDrawLayer);
PL_DECLARE_STRUCT(plDrawCommand);
PL_DECLARE_STRUCT(plDrawVertex);

typedef union plVec2_t plVec2;
typedef union plVec3_t plVec3;
typedef union plVec4_t plVec4;

typedef void* plTextureId;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup/shutdown
void         pl_create_drawlist     (plDrawContext* ctx, plDrawList* drawlistOut);
plDrawLayer* pl_request_draw_layer  (plDrawList* drawlist, const char* name);
void         pl_return_draw_layer   (plDrawLayer* layer);
void         pl_cleanup_draw_context(plDrawContext* ctx);  // implementated by backend

// per frame
void pl_new_draw_frame   (plDrawContext* ctx); // implementated by backend
void pl_submit_draw_layer(plDrawLayer* layer);

// drawing
void pl_add_triangle_filled(plDrawLayer* layer, plVec2 p0, plVec2 p1, plVec2 p2, plVec4 color);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union plVec2_t
{
    struct { float x, y; };
    struct { float u, v; };
} plVec2;

typedef union plVec3_t
{
    struct { float x, y, z; };
    struct { float r, g, b; };
} plVec3;

typedef union plVec4_t
{
    struct{ float x, y, z, w;};
    struct{ float r, g, b, a;};
} plVec4;

typedef struct plDrawCommand_t
{
    uint32_t    vertexOffset;
    uint32_t    indexOffset;
    uint32_t    elementCount;
    uint32_t    layer;
    plTextureId textureId;
} plDrawCommand;

typedef struct plDrawContext_t
{
    plDrawList** sbDrawlists;
    uint64_t     frameCount;
    plTextureId  fontAtlas;
    void*        _platformData;
} plDrawContext;

typedef struct plDrawVertex_t
{
    float pos[2];
    float uv[2];
    float color[4];
} plDrawVertex;

typedef struct plDrawList_t
{
    plDrawContext* ctx;
    plDrawLayer**  sbSubmittedLayers;
    plDrawLayer**  sbLayerCache;
    plDrawLayer**  sbLayersCreated;
    plDrawCommand* sbDrawCommands;
    plDrawVertex*  sbVertexBuffer;
    uint32_t       indexBufferByteSize;
    uint32_t       layersCreated;
    void*          _platformData;
} plDrawList;

typedef struct plDrawLayer_t
{
    char            name[PL_MAX_NAME_LENGTH];
    plDrawList*     drawlist;
    plDrawCommand*  sbCommandBuffer;
    uint32_t*       sbIndexBuffer;
    plVec2*         sbPath;
    uint32_t        vertexCount;
    plDrawCommand*  _lastCommand;
} plDrawLayer;

#endif // PL_DRAWING_H