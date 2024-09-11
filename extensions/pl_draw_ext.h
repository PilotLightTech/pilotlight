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

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDrawList2D  plDrawList2D;
typedef struct _plDrawList3D  plDrawList3D;
typedef struct _plDrawLayer2D plDrawLayer2D;

// primitive options
typedef struct _plDrawCapsuleDesc  plDrawCapsuleDesc;
typedef struct _plDrawSphereDesc   plDrawSphereDesc;
typedef struct _plDrawCylinderDesc plDrawCylinderDesc;
typedef struct _plDrawConeDesc     plDrawConeDesc;

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
typedef int plDrawRectFlags;

// external
typedef struct _plDevice             plDevice;              // pl_graphics_ext.h
typedef union  plRenderEncoderHandle plRenderEncoderHandle; // pl_graphics_ext.h
typedef union  plTextureHandle       plTextureHandle;       // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDrawI
{
    // init/cleanup
    void (*initialize)(plDevice*);
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
    void           (*submit_2d_drawlist) (plDrawList2D*, plRenderEncoderHandle, float fWidth, float fHeight, uint32_t uMSAASampleCount);

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
    void (*add_rect)               (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments);
    void (*add_rect_filled)        (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments);
    void (*add_rect_ex)            (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments, plDrawRectFlags);
    void (*add_rect_filled_ex)     (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments, plDrawRectFlags);
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

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList3D* (*request_3d_drawlist)(void);
    void          (*return_3d_drawlist)(plDrawList3D*);
    void          (*submit_3d_drawlist)(plDrawList3D*, plRenderEncoderHandle, float fWidth, float fHeight, const plMat4* ptMVP, plDrawFlags, uint32_t uMSAASampleCount);

    // solid
    void (*add_3d_triangle_filled)    (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor);
    void (*add_3d_sphere_filled)      (plDrawList3D*, plVec3 tCenter, float fRadius, plVec4 tColor);
    void (*add_3d_sphere_filled_ex)   (plDrawList3D*, const plDrawSphereDesc*);
    void (*add_3d_circle_xz_filled)   (plDrawList3D*, plVec3 tCenter, float fRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_3d_band_xz_filled)     (plDrawList3D*, plVec3 tCenter, float fInnerRadius, float fOuterRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_3d_band_xy_filled)     (plDrawList3D*, plVec3 tCenter, float fInnerRadius, float fOuterRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_3d_band_yz_filled)     (plDrawList3D*, plVec3 tCenter, float fInnerRadius, float fOuterRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_3d_cylinder_filled_ex) (plDrawList3D*, const plDrawCylinderDesc*);
    void (*add_3d_cone_filled_ex)     (plDrawList3D*, const plDrawConeDesc*);
    void (*add_3d_centered_box_filled)(plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor);
    void (*add_3d_plane_xz_filled)    (plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, plVec4 tColor);
    void (*add_3d_plane_xy_filled)    (plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, plVec4 tColor);
    void (*add_3d_plane_yz_filled)    (plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, plVec4 tColor);

    // wireframe
    void (*add_3d_line)        (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec4 tColor, float fThickness);
    void (*add_3d_cross)       (plDrawList3D*, plVec3 tP0, plVec4 tColor, float fLength, float fThickness);
    void (*add_3d_transform)   (plDrawList3D*, const plMat4* ptTransform, float fLength, float fThickness);
    void (*add_3d_frustum)     (plDrawList3D*, const plMat4* ptTransform, float fYFov, float fAspect, float fNearZ, float fFarZ, plVec4 tColor, float fThickness);
    void (*add_3d_centered_box)(plDrawList3D*, plVec3 tCenter, float fWidth, float fHeight, float fDepth, plVec4 tColor, float fThickness);
    void (*add_3d_aabb)        (plDrawList3D*, plVec3 tMin, plVec3 tMax, plVec4 tColor, float fThickness);
    void (*add_3d_bezier_quad) (plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_bezier_cubic)(plDrawList3D*, plVec3 tP0, plVec3 tP1, plVec3 tP2, plVec3 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_3d_circle_xz)   (plDrawList3D*, plVec3 tCenter, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
    void (*add_3d_capsule)     (plDrawList3D*, plVec3 tBasePos, plVec3 tTipPos, float fRadius, plVec4 tColor, float fThickness);
    void (*add_3d_sphere)      (plDrawList3D*, plVec3 tCenter, float fRadius, plVec4 tColor, float fThickness);
    void (*add_3d_sphere_ex)   (plDrawList3D*, const plDrawSphereDesc*);
    void (*add_3d_capsule_ex)  (plDrawList3D*, const plDrawCapsuleDesc*);
    void (*add_3d_cylinder_ex) (plDrawList3D*, const plDrawCylinderDesc*);
    void (*add_3d_cone_ex)     (plDrawList3D*, const plDrawConeDesc*);

    // text
    void (*add_3d_text)(plDrawList3D*, plFontHandle, float fSize, plVec3 tP, plVec4 tColor, const char* pcText, float fWrap);

    // primitive option helpers (fills defaults)
    void (*fill_capsule_desc_default) (plDrawCapsuleDesc*);
    void (*fill_sphere_desc_default)  (plDrawSphereDesc*);
    void (*fill_cylinder_desc_default)(plDrawCylinderDesc*);
    void (*fill_cone_desc_default)    (plDrawConeDesc*);

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

enum _plDrawRectFlags
{
    PL_DRAW_RECT_FLAG_NONE                        = 0,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT      = 1 << 0,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT     = 1 << 1,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT   = 1 << 2,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT  = 1 << 4,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE          = 1 << 5,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP           = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM        = PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_LEFT          = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_RIGHT         = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_All           = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plDrawCapsuleDesc
{
    plVec3   tBasePos;
    float    fBaseRadius;
    plVec3   tTipPos;
    float    fTipRadius;
    plVec4   tColor;
    float    fThickness;
    uint32_t uLatBands;
    uint32_t uLongBands;
    float    fEndOffsetRatio;
} plDrawCapsuleDesc;

typedef struct _plDrawSphereDesc
{
    plVec3   tCenter;
    float    fRadius;
    plVec4   tColor;
    uint32_t uLatBands;
    uint32_t uLongBands;
    float    fThickness;
} plDrawSphereDesc;

typedef struct _plDrawCylinderDesc
{
    plVec3   tBasePos;
    plVec3   tTipPos;
    float    fRadius;
    plVec4   tColor;
    float    fThickness;
    uint32_t uSegments;
} plDrawCylinderDesc;

typedef struct _plDrawConeDesc
{
    plVec3   tBasePos;
    plVec3   tTipPos;
    float    fRadius;
    plVec4   tColor;
    float    fThickness;
    uint32_t uSegments;
} plDrawConeDesc;

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

    // internal
    uint32_t _uConfigIndex;
} plFontRange;

typedef struct _plFontConfig
{
    float        fFontSize;
    plFontRange* sbtRanges;
    int*         sbiIndividualChars;
    bool         bMergeFont;
    plFontHandle tMergeFont;

    // BITMAP
    uint32_t uVOverSampling;
    uint32_t uHOverSampling;

    // SDF (ttf only)
    bool          bSdf;
    int           iSdfPadding;
    unsigned char ucOnEdgeValue;
    
    // internal
    plFontChar* _sbtCharData;
    float       _fSdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    float        fSize;
    float        fLineSpacing;

    plFontRange* sbtRanges;
    
    uint32_t     _uCodePointCount;
    uint32_t*    _auCodePoints; // glyph index lookup based on codepoint
    plFontGlyph* sbtGlyphs;     // glyphs
    
    // internal
    plFontConfig* _sbtConfigs;
    struct _plFontPrepData* _sbtPreps;
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