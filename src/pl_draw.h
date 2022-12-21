/*
   pl_draw.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
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

#ifndef PL_ASSERT
#include <assert.h>
#define PL_ASSERT(x) assert((x))
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
#define PL_DECLARE_STRUCT(name) typedef struct _ ## name  name
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>  // uint*_t
#include <stdbool.h> // bool

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// drawing
PL_DECLARE_STRUCT(plDrawContext);    // pl_draw context (opaque structure)
PL_DECLARE_STRUCT(plDrawList);       // collection of draw layers for a specific target (opaque structure)
PL_DECLARE_STRUCT(plDrawLayer);      // layer for out of order drawing(opaque structure)
PL_DECLARE_STRUCT(plDrawCommand);    // single draw call (opaque structure)
PL_DECLARE_STRUCT(plDrawVertex);     // single vertex (pos + uv + color)

// fonts
PL_DECLARE_STRUCT(plFontChar);       // internal for now (opaque structure)
PL_DECLARE_STRUCT(plFontGlyph);      // internal for now (opaque structure)
PL_DECLARE_STRUCT(plFontCustomRect); // internal for now (opaque structure)
PL_DECLARE_STRUCT(plFontPrepData);   // internal for now (opaque structure)
PL_DECLARE_STRUCT(plFontRange);      // a range of characters
PL_DECLARE_STRUCT(plFont);           // a single font with a specific size and config
PL_DECLARE_STRUCT(plFontConfig);     // configuration for loading a single font
PL_DECLARE_STRUCT(plFontAtlas);      // atlas for multiple fonts

// math
typedef union _plVec2 plVec2;
typedef union _plVec3 plVec3;
typedef union _plVec4 plVec4;

// plTextureID: used to represent texture for renderer backend
typedef void* plTextureId;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

// setup/shutdown
void            pl_register_drawlist   (plDrawContext* ptCtx, plDrawList* ptDrawlist);
plDrawLayer*    pl_request_draw_layer  (plDrawList* ptDrawlist, const char* pcName);
void            pl_return_draw_layer   (plDrawLayer* ptLayer);
void            pl_cleanup_draw_context(plDrawContext* ptCtx);  // implemented by backend

// per frame
void            pl_new_draw_frame   (plDrawContext* ptCtx); // implemented by backend
void            pl_submit_draw_layer(plDrawLayer* ptLayer);

// drawing
void            pl_add_line           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
void            pl_add_lines          (plDrawLayer* ptLayer, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
void            pl_add_text           (plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
void            pl_add_triangle       (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
void            pl_add_triangle_filled(plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
void            pl_add_rect           (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness);
void            pl_add_rect_filled    (plDrawLayer* ptLayer, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor);
void            pl_add_quad           (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
void            pl_add_quad_filled    (plDrawLayer* ptLayer, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
void            pl_add_circle         (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
void            pl_add_circle_filled  (plDrawLayer* ptLayer, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);

// fonts
void            pl_build_font_atlas        (plDrawContext* ptCtx, plFontAtlas* ptAtlas); // implemented by backend
void            pl_cleanup_font_atlas      (plFontAtlas* ptAtlas);                     // implemented by backend
void            pl_add_default_font        (plFontAtlas* ptAtlas);
void            pl_add_font_from_file_ttf  (plFontAtlas* ptAtlas, plFontConfig tConfig, const char* pcFile);
void            pl_add_font_from_memory_ttf(plFontAtlas* ptAtlas, plFontConfig tConfig, void* pData);
plVec2          pl_calculate_text_size     (plFont* ptFont, float fSize, const char* pcText, float fWrap);

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex
{
    float    pos[2];
    float    uv[2];
    uint32_t uColor;
} plDrawVertex;

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
    void*             _platformData;
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
    void*          _platformData;
} plDrawList;

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
    plDrawList** sbDrawlists;
    uint64_t     frameCount;
    plFontAtlas* fontAtlas;
    void*        _platformData;
} plDrawContext;

#endif // PL_DRAW_H