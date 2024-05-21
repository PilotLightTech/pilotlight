/*
   pl_draw_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] defines
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_EXT_H
#define PL_DRAW_EXT_H

#define PL_DRAW_EXT_VERSION    "0.1.0"
#define PL_DRAW_EXT_VERSION_NUM 000100

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_DRAW "PL_API_DRAW"
typedef struct _plDrawI plDrawI;

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_DRAWLISTS
    #define PL_MAX_DRAWLISTS 64
#endif

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_math.h"

#include "pl_graphics_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDrawList2D  plDrawList2D;
typedef struct _plDrawList3D  plDrawList3D;
typedef struct _plDrawLayer2D plDrawLayer2D;

// font types
typedef struct _plFontChar       plFontChar;       // internal for now (opaque structure)
typedef struct _plFontGlyph      plFontGlyph;      // internal for now (opaque structure)
typedef struct _plFontCustomRect plFontCustomRect; // internal for now (opaque structure)
typedef struct _plFontRange      plFontRange;      // a range of characters
typedef struct _plFont           plFont;           // a single font with a specific size and config
typedef struct _plFontConfig     plFontConfig;     // configuration for loading a single font
typedef union  _plFontHandle     plFontHandle;

// enums
typedef int plDrawFlags;

// external
typedef struct _plGraphics      plGraphics;      // pl_graphics_ext.h
typedef struct _plRenderEncoder plRenderEncoder; // pl_graphics_ext.h
typedef union  plTextureHandle  plTextureHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDrawI
{
    // init/cleanup
    void (*initialize)(plGraphics*);
    void (*cleanup)   (void);

    // per frame
    void (*new_frame)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~fonts~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    void         (*build_font_atlas)        (void);
    void         (*cleanup_font_atlas)      (void);
    plFontHandle (*add_default_font)        (void);
    plFontHandle (*add_font_from_file_ttf)  (plFontConfig, const char* pcFile);
    plFontHandle (*add_font_from_memory_ttf)(plFontConfig, void* pData);
    plVec2       (*calculate_text_size)     (plFontHandle, float fSize, const char* pcText, float fWrap);
    plVec2       (*calculate_text_size_ex)  (plFontHandle, float fSize, const char* pcText, const char* pcTextEnd, float fWrap);
    plRect       (*calculate_text_bb)       (plFontHandle, float fSize, plVec2 tP, const char* pcText, float fWrap);
    plRect       (*calculate_text_bb_ex)    (plFontHandle, float fSize, plVec2 tP, const char* pcText, const char* pcTextEnd, float fWrap);
    plFont*      (*get_font)                (plFontHandle); // do not store

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~2D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList2D*  (*request_2d_drawlist)(void);
    void           (*return_2d_drawlist) (plDrawList2D*);
    void           (*submit_2d_drawlist) (plDrawList2D*, plRenderEncoder, float fWidth, float fHeight, uint32_t uMSAASampleCount);

    // layers
    plDrawLayer2D* (*request_2d_layer)(plDrawList2D*, const char* pcName);
    void           (*return_2d_layer) (plDrawLayer2D*);
    void           (*submit_2d_layer) (plDrawLayer2D*);

    // drawing
    void (*add_line)               (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
    void (*add_lines)              (plDrawLayer2D*, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
    void (*add_text)               (plDrawLayer2D*, plFontHandle, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
    void (*add_text_ex)            (plDrawLayer2D*, plFontHandle, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
    void (*add_text_clipped)       (plDrawLayer2D*, plFontHandle, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
    void (*add_text_clipped_ex)    (plDrawLayer2D*, plFontHandle, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
    void (*add_triangle)           (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
    void (*add_triangle_filled)    (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
    void (*add_rect)               (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness);
    void (*add_rect_filled)        (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor);
    void (*add_rect_rounded)       (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments);
    void (*add_rect_rounded_filled)(plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments);
    void (*add_quad)               (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
    void (*add_quad_filled)        (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
    void (*add_circle)             (plDrawLayer2D*, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
    void (*add_circle_filled)      (plDrawLayer2D*, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_image)              (plDrawLayer2D*, plTextureHandle, plVec2 tPMin, plVec2 tPMax);
    void (*add_image_ex)           (plDrawLayer2D*, plTextureHandle, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor);
    void (*add_bezier_quad)        (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_bezier_cubic)       (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

    // clipping
    void          (*push_clip_rect_pt)(plDrawList2D*, const plRect*);
    void          (*push_clip_rect)   (plDrawList2D*, plRect, bool bAccumulate);
    void          (*pop_clip_rect)    (plDrawList2D*);
    const plRect* (*get_clip_rect)    (plDrawList2D*);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~2D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList3D* (*request_3d_drawlist)(void);
    void          (*return_3d_drawlist)(plDrawList3D*);
    void          (*submit_3d_drawlist)(plDrawList3D*, plRenderEncoder, float fWidth, float fHeight, const plMat4* ptMVP, plDrawFlags, uint32_t uMSAASampleCount);

    // drawing
    void (*add_3d_triangle_filled)(plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_3d_line)           (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_3d_point)          (plDrawList3D*, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_3d_transform)      (plDrawList3D*, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_3d_frustum)        (plDrawList3D*, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_3d_centered_box)   (plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_3d_aabb)           (plDrawList3D*, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness);
    void (*add_3d_bezier_quad)    (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_bezier_cubic)   (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

} plDrawI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum _plDrawFlags
{
    PL_DRAW_FLAG_NONE          = 0,
    PL_DRAW_FLAG_DEPTH_TEST    = 1 << 0,
    PL_DRAW_FLAG_DEPTH_WRITE   = 1 << 1,
    PL_DRAW_FLAG_CULL_FRONT    = 1 << 2,
    PL_DRAW_FLAG_CULL_BACK     = 1 << 3,
    PL_DRAW_FLAG_FRONT_FACE_CW = 1 << 4,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef union _plFontHandle
{ 
    struct
    {
        uint32_t uIndex;
        uint32_t uGeneration;
    }; 
    uint64_t ulData;
} plFontHandle;

typedef struct _plFontRange
{
    int         iFirstCodePoint;
    uint32_t    uCharCount;
    plFontChar* ptFontChar; // offset into parent font's char data
} plFontRange;

typedef struct _plFontConfig
{
    float        fFontSize;
    plFontRange* sbtRanges;
    int*         sbiIndividualChars;

    // BITMAP
    uint32_t uVOverSampling;
    uint32_t uHOverSampling;

    // SDF
    bool          bSdf;
    int           iSdfPadding;
    unsigned char ucOnEdgeValue;
    float         fSdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    plFontConfig tConfig;
    float        fLineSpacing;
    float        fAscent;
    float        fDescent;
    
    uint32_t*    sbuCodePoints; // glyph index lookup based on codepoint
    plFontGlyph* sbtGlyphs;     // glyphs
    plFontChar*  sbtCharData;
} plFont;

typedef struct _plFontCustomRect
{
    uint32_t       uWidth;
    uint32_t       uHeight;
    uint32_t       uX;
    uint32_t       uY;
    unsigned char* pucBytes;
} plFontCustomRect;

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

#endif // PL_DRAW_EXT_H