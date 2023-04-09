/*
   pl_draw.h
*/

// library version
#define PL_DRAW_VERSION    "0.1.0"
#define PL_DRAW_VERSION_NUM 00100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums
// [SECTION] structs
*/

#ifndef PL_DRAW_H
#define PL_DRAW_H

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#ifndef PL_MAX_FRAMES_IN_FLIGHT
    #define PL_MAX_FRAMES_IN_FLIGHT 2
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// drawing
typedef struct _plDrawContext plDrawContext;    // pl_draw context (opaque structure)
typedef struct _plDrawLayer   plDrawLayer;      // layer for out of order drawing(opaque structure)

// vertex types
typedef struct _plDrawVertex       plDrawVertex;       // single vertex (2D pos + uv + color)
typedef struct _plDrawVertex3D     plDrawVertex3D;     // single vertex (3D pos + uv + color)
typedef struct _plDrawVertex3DLine plDrawVertex3DLine; // single vertex (pos + uv + color)

// draw lists
typedef struct _plDrawList   plDrawList;   // collection of draw layers for a specific target (opaque structure)
typedef struct _plDrawList3D plDrawList3D;

// draw commands
typedef struct _plDrawCommand plDrawCommand;    // single draw call (opaque structure)

// fonts
typedef struct _plFontChar       plFontChar;       // internal for now (opaque structure)
typedef struct _plFontGlyph      plFontGlyph;      // internal for now (opaque structure)
typedef struct _plFontCustomRect plFontCustomRect; // internal for now (opaque structure)
typedef struct _plFontPrepData   plFontPrepData;   // internal for now (opaque structure)
typedef struct _plFontRange      plFontRange;      // a range of characters
typedef struct _plFont           plFont;           // a single font with a specific size and config
typedef struct _plFontConfig     plFontConfig;     // configuration for loading a single font
typedef struct _plFontAtlas      plFontAtlas;      // atlas for multiple fonts

// enums
typedef int pl3DDrawFlags;

// plTextureID: used to represent texture for renderer backend
typedef void* plTextureId;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// context
void            pl_set_draw_context(plDrawContext* ptCtx);
plDrawContext*  pl_get_draw_context(void);
void            pl_cleanup_draw_context(plDrawContext* ptCtx);  // implemented by backend

// setup
void            pl_register_drawlist   (plDrawContext* ptCtx, plDrawList* ptDrawlist);
void            pl_register_3d_drawlist(plDrawContext* ptCtx, plDrawList3D* ptDrawlist);
plDrawLayer*    pl_request_draw_layer  (plDrawList* ptDrawlist, const char* pcName);
void            pl_return_draw_layer   (plDrawLayer* ptLayer);

// per frame
void            pl_new_draw_frame   (plDrawContext* ptCtx); // implemented by backend
void            pl_submit_draw_layer(plDrawLayer* ptLayer);

// drawing
void            pl_add_line           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
void            pl_add_lines          (plDrawLayer* ptLayer, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
void            pl_add_text           (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void            pl_add_text_ex        (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
void            pl_add_text_clipped   (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
void            pl_add_text_clipped_ex(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
void            pl_add_triangle       (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
void            pl_add_triangle_filled(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
void            pl_add_rect           (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness);
void            pl_add_rect_filled    (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor);
void            pl_add_quad           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
void            pl_add_quad_filled    (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
void            pl_add_circle         (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
void            pl_add_circle_filled  (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);
void            pl_add_image          (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax);
void            pl_add_image_ex       (plDrawLayer* ptLayer, plTextureId tTexture, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor);
void            pl_add_bezier_quad    (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
void            pl_add_bezier_cubic   (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

// 3D drawing
void            pl_add_3d_triangle_filled(plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
void            pl_add_3d_line           (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
void            pl_add_3d_point          (plDrawList3D* ptDrawlist, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
void            pl_add_3d_transform      (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fLength, float fThickness);
void            pl_add_3d_frustum        (plDrawList3D* ptDrawlist, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
void            pl_add_3d_centered_box   (plDrawList3D* ptDrawlist, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
void            pl_add_3d_bezier_quad    (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
void            pl_add_3d_bezier_cubic   (plDrawList3D* ptDrawlist, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

// fonts
void            pl_build_font_atlas        (plDrawContext* ptCtx, plFontAtlas* ptAtlas); // implemented by backend
void            pl_cleanup_font_atlas      (plFontAtlas* ptAtlas);                     // implemented by backend
void            pl_add_default_font        (plFontAtlas* ptAtlas);
void            pl_add_font_from_file_ttf  (plFontAtlas* ptAtlas, plFontConfig tConfig, const char* pcFile);
void            pl_add_font_from_memory_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, void* pData);
plVec2          pl_calculate_text_size     (plFont* ptFont, float fSize, const char* pcText, float fWrap);
plVec2          pl_calculate_text_size_ex  (plFont* ptFont, float fSize, const char* pcText, const char* pcTextEnd, float fWrap);
plRect          pl_calculate_text_bb       (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, float fWrap);
plRect          pl_calculate_text_bb_ex    (plFont* ptFont, float fSize, plVec2 tP, const char* pcText, const char* pcTextEnd, float fWrap);

// clipping
void            pl_push_clip_rect_pt       (plDrawList* ptDrawlist, const plRect* ptRect);
void            pl_push_clip_rect          (plDrawList* ptDrawlist, plRect tRect, bool bAccumulate);
void            pl_pop_clip_rect           (plDrawList* ptDrawlist);
const plRect*   pl_get_clip_rect           (plDrawList* ptDrawlist);

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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex
{
    float    pos[2];
    float    uv[2];
    uint32_t uColor;
} plDrawVertex;

typedef struct _plDrawVertex3D
{
    float    pos[3];
    uint32_t uColor;
} plDrawVertex3D;

typedef struct _plDrawVertex3DLine
{
    float    pos[3];
    float    fDirection;
    float    fThickness;
    float    fMultiply;
    float    posother[3];
    uint32_t uColor;
} plDrawVertex3DLine;

typedef struct _plFontRange
{
    int         firstCodePoint;
    uint32_t    charCount;
    plFontChar* ptrFontChar; // offset into parent font's char data
} plFontRange;

typedef struct _plFontConfig
{
    float        fontSize;
    plFontRange* sbRanges;
    int*         sbIndividualChars;

    // BITMAP
    uint32_t vOverSampling;
    uint32_t hOverSampling;

    // SDF
    bool          sdf;
    int           sdfPadding;
    unsigned char onEdgeValue;
    float         sdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    plFontConfig config;
    plFontAtlas* parentAtlas;
    float        lineSpacing;
    float        ascent;
    float        descent;
    
    uint32_t*    sbCodePoints; // glyph index lookup based on codepoint
    plFontGlyph* sbGlyphs;     // glyphs
    plFontChar*  sbCharData;
} plFont;

typedef struct _plFontAtlas
{
    plDrawContext*    ctx;
    plFont*           sbFonts;
    plFontCustomRect* sbCustomRects;
    unsigned char*    pixelsAsAlpha8;
    unsigned char*    pixelsAsRGBA32;
    uint32_t          atlasSize[2];
    float             whiteUv[2];
    bool              dirty;
    int               glyphPadding;
    size_t            pixelDataSize;
    plFontCustomRect* whiteRect;
    plTextureId       texture;
    plFontPrepData*   _sbPrepData;
} plFontAtlas;

typedef struct _plDrawList
{
    plDrawContext* ctx;
    plDrawLayer**  sbSubmittedLayers;
    plDrawLayer**  sbLayerCache;
    plDrawLayer**  sbLayersCreated;
    plDrawCommand* sbDrawCommands;
    plDrawVertex*  sbVertexBuffer;
    uint32_t       indexBufferByteSize;
    uint32_t       layersCreated;
    plRect*        sbClipStack;
} plDrawList;

typedef struct _plDrawList3D
{
    plDrawContext*      ctx;
    plDrawVertex3D*     sbVertexBuffer;
    uint32_t*           sbIndexBuffer;
    plDrawVertex3DLine* sbLineVertexBuffer;
    uint32_t*           sbLineIndexBuffer;
} plDrawList3D;

typedef struct _plFontCustomRect
{
    uint32_t       width;
    uint32_t       height;
    uint32_t       x;
    uint32_t       y;
    unsigned char* bytes;
} plFontCustomRect;

typedef struct _plDrawCommand
{
    uint32_t    vertexOffset;
    uint32_t    indexOffset;
    uint32_t    elementCount;
    uint32_t    layer;
    plTextureId textureId;
    plRect      tClip;
    bool        sdf;
} plDrawCommand;

typedef struct _plDrawLayer
{
    char            name[PL_MAX_NAME_LENGTH];
    plDrawList*     drawlist;
    plDrawCommand*  sbCommandBuffer;
    uint32_t*       sbIndexBuffer;
    plVec2*         sbPath;
    uint32_t        vertexCount;
    plDrawCommand*  _lastCommand;
} plDrawLayer;

typedef struct _plFontChar
{
    uint16_t x0;
    uint16_t y0;
    uint16_t x1;
    uint16_t y1;
    float    xOff;
    float    yOff;
    float    xAdv;
    float    xOff2;
    float    yOff2;
} plFontChar;

typedef struct _plFontGlyph
{
    float x0;
    float y0;
    float u0;
    float v0;
    float x1;
    float y1;
    float u1;
    float v1;
    float xAdvance;
    float leftBearing;  
} plFontGlyph;

typedef struct _plDrawContext
{
    plDrawList**   sbDrawlists;
    plDrawList3D** sb3DDrawlists;
    uint64_t       frameCount;
    plFontAtlas*   fontAtlas;
    plVec2         tFrameBufferScale;
    void*          _platformData;
} plDrawContext;

#endif // PL_DRAW_H