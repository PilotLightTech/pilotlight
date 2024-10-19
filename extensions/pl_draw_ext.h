/*
   pl_draw_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations & basic types
// [SECTION] public api struct
// [SECTION] enums
// [SECTION] structs
// [SECTION] structs for backends
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_EXT_H
#define PL_DRAW_EXT_H

// extension version (format XYYZZ)
// #define PL_DRAW_EXT_VERSION    "1.0.0"
// #define PL_DRAW_EXT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_DRAW "PL_API_DRAW"
typedef struct _plDrawI plDrawI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations & basic types
//-----------------------------------------------------------------------------

// basic types
typedef struct _plDrawInit    plDrawInit;
typedef struct _plFontAtlas   plFontAtlas;
typedef struct _plDrawList2D  plDrawList2D;
typedef struct _plDrawList3D  plDrawList3D;
typedef struct _plDrawLayer2D plDrawLayer2D;

// primitive options
typedef struct _plDrawCapsuleDesc  plDrawCapsuleDesc;
typedef struct _plDrawSphereDesc   plDrawSphereDesc;
typedef struct _plDrawCylinderDesc plDrawCylinderDesc;
typedef struct _plDrawConeDesc     plDrawConeDesc;

// font types
typedef struct _plFontChar       plFontChar;
typedef struct _plFontGlyph      plFontGlyph;
typedef struct _plFontCustomRect plFontCustomRect;
typedef struct _plFontRange      plFontRange;      // a range of characters
typedef struct _plFont           plFont;           // a single font with a specific size and config
typedef struct _plFontConfig     plFontConfig;     // configuration for loading a single font

// enums
typedef int plDrawFlags;     // -> enum _plDrawFlags     // Flags:
typedef int plDrawRectFlags; // -> enum _plDrawRectFlags // Flags:

#ifndef plTextureID
    typedef uint64_t plTextureID;
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plDrawI
{
    // init/cleanup
    void (*initialize)(const plDrawInit*);
    void (*cleanup)   (void); // usually called by backend

    // per frame
    void (*new_frame)(void); // usually called by backend

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~fonts~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // font atlas
    plFontAtlas* (*create_font_atlas)     (void);
    bool         (*prepare_font_atlas)    (plFontAtlas*); // usually called by backend
    void         (*cleanup_font_atlas)    (plFontAtlas*); // usually called by backend
    void         (*set_font_atlas)        (plFontAtlas*);
    plFontAtlas* (*get_current_font_atlas)(void);

    plFont* (*add_default_font)        (plFontAtlas*);
    plFont* (*add_font_from_file_ttf)  (plFontAtlas*, plFontConfig, const char* pcFile);
    plFont* (*add_font_from_memory_ttf)(plFontAtlas*, plFontConfig, void* pData);
    plVec2  (*calculate_text_size)     (plFont*, float fSize, const char* pcText, float fWrap);
    plVec2  (*calculate_text_size_ex)  (plFont*, float fSize, const char* pcText, const char* pcTextEnd, float fWrap);
    plRect  (*calculate_text_bb)       (plFont*, float fSize, plVec2 tP, const char* pcText, float fWrap);
    plRect  (*calculate_text_bb_ex)    (plFont*, float fSize, plVec2 tP, const char* pcText, const char* pcTextEnd, float fWrap);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~2D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList2D* (*request_2d_drawlist)(void);
    void          (*return_2d_drawlist) (plDrawList2D*);

    // layers
    plDrawLayer2D* (*request_2d_layer)(plDrawList2D*);
    void           (*return_2d_layer) (plDrawLayer2D*);
    void           (*submit_2d_layer) (plDrawLayer2D*);

    // drawing
    void (*add_line)           (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec4 tColor, float fThickness);
    void (*add_lines)          (plDrawLayer2D*, plVec2* atPoints, uint32_t uCount, plVec4 tColor, float fThickness);
    void (*add_text)           (plDrawLayer2D*, plFont*, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap);
    void (*add_text_ex)        (plDrawLayer2D*, plFont*, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
    void (*add_text_clipped)   (plDrawLayer2D*, plFont*, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap);
    void (*add_text_clipped_ex)(plDrawLayer2D*, plFont*, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, const char* pcTextEnd, float fWrap);
    void (*add_triangle)       (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness);
    void (*add_triangle_filled)(plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor);
    void (*add_rect)           (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments);
    void (*add_rect_filled)    (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments);
    void (*add_rect_ex)        (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fThickness, float fRadius, uint32_t uSegments, plDrawRectFlags);
    void (*add_rect_filled_ex) (plDrawLayer2D*, plVec2 tMinP, plVec2 tMaxP, plVec4 tColor, float fRadius, uint32_t uSegments, plDrawRectFlags);
    void (*add_quad)           (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness);
    void (*add_quad_filled)    (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor);
    void (*add_circle)         (plDrawLayer2D*, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments, float fThickness);
    void (*add_circle_filled)  (plDrawLayer2D*, plVec2 tP, float fRadius, plVec4 tColor, uint32_t uSegments);
    void (*add_image)          (plDrawLayer2D*, plTextureID, plVec2 tPMin, plVec2 tPMax);
    void (*add_image_ex)       (plDrawLayer2D*, plTextureID, plVec2 tPMin, plVec2 tPMax, plVec2 tUvMin, plVec2 tUvMax, plVec4 tColor);
    void (*add_bezier_quad)    (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec4 tColor, float fThickness, uint32_t uSegments);
    void (*add_bezier_cubic)   (plDrawLayer2D*, plVec2 tP0, plVec2 tP1, plVec2 tP2, plVec2 tP3, plVec4 tColor, float fThickness, uint32_t uSegments);

    // clipping
    void          (*push_clip_rect_pt)(plDrawList2D*, const plRect*);
    void          (*push_clip_rect)   (plDrawList2D*, plRect, bool bAccumulate);
    void          (*pop_clip_rect)    (plDrawList2D*);
    const plRect* (*get_clip_rect)    (plDrawList2D*);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList3D* (*request_3d_drawlist)(void);
    void          (*return_3d_drawlist)(plDrawList3D*);

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
    void (*add_3d_text)(plDrawList3D*, plFont*, float fSize, plVec3 tP, plVec4 tColor, const char* pcText, float fWrap);

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

typedef struct _plDrawInit
{
    int _iUnused;
} plDrawInit;

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

typedef struct _plFontRange
{
    int         iFirstCodePoint;
    uint32_t    uCharCount;

    // internal
    uint32_t _uConfigIndex;
} plFontRange;

typedef struct _plFontConfig
{
    float              fFontSize;
    const plFontRange* ptRanges;
    uint32_t           uRangeCount;
    const int*         piIndividualChars;
    uint32_t           uIndividualCharCount;
    bool               bMergeFont;
    plFont*            tMergeFont;

    // BITMAP
    uint32_t uVOverSampling;
    uint32_t uHOverSampling;

    // SDF (ttf only)
    bool          bSdf;
    int           iSdfPadding;
    unsigned char ucOnEdgeValue;
    
    // internal
    plFontRange* _sbtRanges;
    plFontChar* _sbtCharData;
    float       _fSdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    float fSize;
    float fLineSpacing;

    // internal
    plFontRange*  sbtRanges;
    uint32_t      _uCodePointCount;
    uint32_t*     _auCodePoints; // glyph index lookup based on codepoint
    plFontGlyph*  sbtGlyphs;     // glyphs
    plFontConfig* _sbtConfigs;
    struct _plFontPrepData* _sbtPreps;

    plFont* _ptNextFont;
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

//-----------------------------------------------------------------------------
// [SECTION] structs for backends
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex3DSolid
{
    float    afPos[3];
    uint32_t uColor;
} plDrawVertex3DSolid;

typedef struct _plDrawVertex3DLine
{
    float    afPos[3];
    float    fDirection;
    float    fThickness;
    float    fMultiply;
    float    afPosOther[3];
    uint32_t uColor;
} plDrawVertex3DLine;

typedef struct _plDraw3DText
{
    plFont*      tFontHandle;
    float        fSize;
    plVec3       tP;
    plVec4       tColor;
    char         acText[PL_MAX_NAME_LENGTH];
    float        fWrap;
} plDraw3DText;

typedef struct _plDrawList3D
{
    plDrawVertex3DSolid* sbtSolidVertexBuffer;
    uint32_t*            sbtSolidIndexBuffer;
    plDrawVertex3DLine*  sbtLineVertexBuffer;
    uint32_t*            sbtLineIndexBuffer;

    // text
    plDraw3DText*  sbtTextEntries;
    plDrawList2D*  pt2dDrawlist;
    plDrawLayer2D* ptLayer;
} plDrawList3D;

typedef struct _plDrawVertex
{
    float    afPos[2];
    float    afUv[2];
    uint32_t uColor;
} plDrawVertex;

typedef struct _plDrawCommand
{
    uint32_t    uVertexOffset;
    uint32_t    uIndexOffset;
    uint32_t    uElementCount;
    plTextureID tTextureId;
    plRect      tClip;
    bool        bSdf;
} plDrawCommand;

typedef struct _plDrawLayer2D
{
    plDrawList2D*  ptDrawlist;
    plDrawCommand* sbtCommandBuffer;
    uint32_t*      sbuIndexBuffer;
    plVec2*        sbtPath;
    uint32_t       uVertexCount;
    plDrawCommand* _ptLastCommand;
} plDrawLayer2D;

typedef struct _plDrawList2D
{
    plDrawLayer2D** sbtSubmittedLayers;
    plDrawLayer2D** sbtLayerCache;
    plDrawLayer2D** sbtLayersCreated;
    plDrawCommand*  sbtDrawCommands;
    plDrawVertex*   sbtVertexBuffer;
    uint32_t        uIndexBufferByteSize;
    uint32_t        uLayersCreated;
    plRect*         sbtClipStack;
    int             _padding;
} plDrawList2D;

typedef struct _plFontAtlas
{
    // fonts
    plFont*   _ptFontListHead;

    plFontCustomRect* sbtCustomRects;
    unsigned char*    pucPixelsAsAlpha8;
    unsigned char*    pucPixelsAsRGBA32;
    uint32_t          auAtlasSize[2];
    float             afWhiteUv[2];
    bool              bDirty;
    int               iGlyphPadding;
    size_t            szPixelDataSize;
    plFontCustomRect* ptWhiteRect;
    plTextureID       tTexture;
    float             fTotalArea;
    
} plFontAtlas;

#endif // PL_DRAW_EXT_H