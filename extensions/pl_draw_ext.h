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
// [SECTION] structs for backends
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_DRAW_EXT_H
#define PL_DRAW_EXT_H

// extension version (format XYYZZ)
#define PL_DRAW_EXT_VERSION    "1.0.0"
#define PL_DRAW_EXT_VERSION_NUM 10000

//-----------------------------------------------------------------------------
// [SECTION] defines
//-----------------------------------------------------------------------------

#ifndef PL_MAX_NAME_LENGTH
    #define PL_MAX_NAME_LENGTH 1024
#endif

#define PL_UNICODE_CODEPOINT_INVALID 0xFFFD // invalid Unicode code point (standard value).
#define PL_UNICODE_CODEPOINT_MAX     0xFFFF // maximum Unicode code point supported by this build.

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
typedef struct _plDrawInit    plDrawInit;    // initialization options (reserved for future use)
typedef struct _plFontAtlas   plFontAtlas;   // font atlas data
typedef struct _plDrawList2D  plDrawList2D;  // drawlist data for 2D
typedef struct _plDrawList3D  plDrawList3D;  // drawlist data for 3D
typedef struct _plDrawLayer2D plDrawLayer2D; // opaque type for 2D draw layers

// vertex buffer types
typedef struct _plDrawVertex        plDrawVertex;        // vertex type (LAYOUT & PADDING MATTERS)
typedef struct _plDrawVertex3DSolid plDrawVertex3DSolid; // vertex type (LAYOUT & PADDING MATTERS)
typedef struct _plDrawVertex3DLine  plDrawVertex3DLine;  // vertex type (LAYOUT & PADDING MATTERS)
typedef struct _plDraw3DText        plDraw3DText;

// primitive options
typedef struct _plDrawLineOptions  plDrawLineOptions;  // options for lines
typedef struct _plDrawSolidOptions plDrawSolidOptions; // options for solids
typedef struct _plDrawTextOptions  plDrawTextOptions;  // options for text
typedef struct _plDrawCapsuleDesc  plDrawCapsuleDesc;  // description for drawing capsules
typedef struct _plDrawSphereDesc   plDrawSphereDesc;   // description for drawing spheres
typedef struct _plDrawCylinderDesc plDrawCylinderDesc; // description for drawing cylinders
typedef struct _plDrawConeDesc     plDrawConeDesc;     // description for drawing cones 
typedef struct _plDrawFrustumDesc  plDrawFrustumDesc;  // description for drawing frustums 

// font types
typedef struct _plFontRange      plFontRange;      // a range of characters
typedef struct _plFont           plFont;           // a single font with a specific size and config
typedef struct _plFontConfig     plFontConfig;     // configuration for loading a single font
typedef struct _plFontChar       plFontChar;       // internal type
typedef struct _plFontGlyph      plFontGlyph;      // internal type
typedef struct _plFontCustomRect plFontCustomRect; // internal type

// character types
typedef uint16_t plUiWChar;

// enums
typedef int plDrawFlags;     // -> enum _plDrawFlags     // Flags:
typedef int plDrawRectFlags; // -> enum _plDrawRectFlags // Flags:

// backend texture type
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
    void (*cleanup)   (void); // usually called by backend "cleanup" func.

    // per frame
    void (*new_frame)(void); // usually called by backend "new_frame" func.

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~fonts~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // font atlas
    plFontAtlas* (*create_font_atlas)     (void);
    bool         (*prepare_font_atlas)    (plFontAtlas*); // usually called by backend "build_font_atlas" func.
    void         (*cleanup_font_atlas)    (plFontAtlas*); // usually called by backend "cleanup_font_atlas" func.
    void         (*set_font_atlas)        (plFontAtlas*);
    plFontAtlas* (*get_current_font_atlas)(void);

    plFont* (*add_default_font)        (plFontAtlas*);
    plFont* (*add_font_from_file_ttf)  (plFontAtlas*, plFontConfig, const char* file);
    plFont* (*add_font_from_memory_ttf)(plFontAtlas*, plFontConfig, void* data);
    plVec2  (*calculate_text_size)     (const char* text, plDrawTextOptions);
    plRect  (*calculate_text_bb)       (plVec2 p, const char* text, plDrawTextOptions);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~2D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList2D* (*request_2d_drawlist)(void);
    void          (*return_2d_drawlist) (plDrawList2D*);
    void          (*prepare_2d_drawlist)(plDrawList2D*); // usually called by backend "submit_2d_drawlist" func.

    // layers
    plDrawLayer2D* (*request_2d_layer)(plDrawList2D*);
    void           (*return_2d_layer) (plDrawLayer2D*);
    void           (*submit_2d_layer) (plDrawLayer2D*);

    // drawing (lines)
    void (*add_line)        (plDrawLayer2D*, plVec2 p0, plVec2 p1, plDrawLineOptions);
    void (*add_lines)       (plDrawLayer2D*, plVec2* points, uint32_t count, plDrawLineOptions);
    void (*add_triangle)    (plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, plDrawLineOptions);
    void (*add_rect)        (plDrawLayer2D*, plVec2 pMin, plVec2 pMax, plDrawLineOptions);
    void (*add_rect_rounded)(plDrawLayer2D*, plVec2 pMin, plVec2 pMax, float radius, uint32_t segments, plDrawRectFlags, plDrawLineOptions);
    void (*add_quad)        (plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, plVec2 p3, plDrawLineOptions);
    void (*add_circle)      (plDrawLayer2D*, plVec2 p, float radius, uint32_t segments, plDrawLineOptions);
    void (*add_bezier_quad) (plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, uint32_t segments, plDrawLineOptions);
    void (*add_bezier_cubic)(plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, plVec2 p3, uint32_t segments, plDrawLineOptions);

    // drawing (solids)
    void (*add_triangle_filled)    (plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, plDrawSolidOptions);
    void (*add_triangles_filled)   (plDrawLayer2D*, plVec2* points, uint32_t count, plDrawSolidOptions);
    void (*add_rect_filled)        (plDrawLayer2D*, plVec2 minP, plVec2 maxP, plDrawSolidOptions);
    void (*add_rect_rounded_filled)(plDrawLayer2D*, plVec2 minP, plVec2 maxP, float radius, uint32_t segments, plDrawRectFlags, plDrawSolidOptions);
    void (*add_quad_filled)        (plDrawLayer2D*, plVec2 p0, plVec2 p1, plVec2 p2, plVec2 p3, plDrawSolidOptions);
    void (*add_circle_filled)      (plDrawLayer2D*, plVec2 p, float radius, uint32_t segments, plDrawSolidOptions);
    void (*add_image)              (plDrawLayer2D*, plTextureID, plVec2 minP, plVec2 maxP);
    void (*add_image_ex)           (plDrawLayer2D*, plTextureID, plVec2 minP, plVec2 maxP, plVec2 minUV, plVec2 maxUV, uint32_t color);

    // drawing (text)
    void (*add_text)        (plDrawLayer2D*, plVec2 p, const char* text, plDrawTextOptions);
    void (*add_text_clipped)(plDrawLayer2D*, plVec2 p, const char* text, plVec2 clipMin, plVec2 clipMax, plDrawTextOptions);

    // clipping
    void          (*push_clip_rect_pt)(plDrawList2D*, const plRect*, bool bAccumulate);
    void          (*push_clip_rect)   (plDrawList2D*, plRect, bool bAccumulate);
    void          (*pop_clip_rect)    (plDrawList2D*);
    const plRect* (*get_clip_rect)    (plDrawList2D*);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~3D~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // drawlists
    plDrawList3D* (*request_3d_drawlist)(void);
    void          (*return_3d_drawlist)(plDrawList3D*);

    // text
    void (*add_3d_text)(plDrawList3D*, plVec3 p, const char* text, plDrawTextOptions);

    // solid
    void (*add_3d_triangle_filled)    (plDrawList3D*, plVec3 p0, plVec3 p1, plVec3 p2, plDrawSolidOptions);
    void (*add_3d_circle_xz_filled)   (plDrawList3D*, plVec3 center, float radius, uint32_t segments, plDrawSolidOptions);
    void (*add_3d_band_xz_filled)     (plDrawList3D*, plVec3 center, float innerRadius, float outerRadius, uint32_t segments, plDrawSolidOptions);
    void (*add_3d_band_xy_filled)     (plDrawList3D*, plVec3 center, float innerRadius, float outerRadius, uint32_t segments, plDrawSolidOptions);
    void (*add_3d_band_yz_filled)     (plDrawList3D*, plVec3 center, float innerRadius, float outerRadius, uint32_t segments, plDrawSolidOptions);
    void (*add_3d_centered_box_filled)(plDrawList3D*, plVec3 center, float width, float height, float depth, plDrawSolidOptions);
    void (*add_3d_plane_xz_filled)    (plDrawList3D*, plVec3 center, float width, float height, plDrawSolidOptions);
    void (*add_3d_plane_xy_filled)    (plDrawList3D*, plVec3 center, float width, float height, plDrawSolidOptions);
    void (*add_3d_plane_yz_filled)    (plDrawList3D*, plVec3 center, float width, float height, plDrawSolidOptions);
    void (*add_3d_sphere_filled)      (plDrawList3D*, plDrawSphereDesc, plDrawSolidOptions);
    void (*add_3d_cylinder_filled)    (plDrawList3D*, plDrawCylinderDesc, plDrawSolidOptions);
    void (*add_3d_cone_filled)        (plDrawList3D*, plDrawConeDesc, plDrawSolidOptions);

    // wireframe
    void (*add_3d_line)        (plDrawList3D*, plVec3 p0, plVec3 p1, plDrawLineOptions);
    void (*add_3d_cross)       (plDrawList3D*, plVec3 p, float length, plDrawLineOptions);
    void (*add_3d_transform)   (plDrawList3D*, const plMat4* transform, float length, plDrawLineOptions);
    void (*add_3d_frustum)     (plDrawList3D*, const plMat4* transform, plDrawFrustumDesc, plDrawLineOptions);
    void (*add_3d_centered_box)(plDrawList3D*, plVec3 center, float width, float height, float depth, plDrawLineOptions);
    void (*add_3d_aabb)        (plDrawList3D*, plVec3 minP, plVec3 maxP, plDrawLineOptions);
    void (*add_3d_bezier_quad) (plDrawList3D*, plVec3 p0, plVec3 p1, plVec3 p2, uint32_t segments, plDrawLineOptions);
    void (*add_3d_bezier_cubic)(plDrawList3D*, plVec3 p0, plVec3 p1, plVec3 p2, plVec3 tP3, uint32_t segments, plDrawLineOptions);
    void (*add_3d_circle_xz)   (plDrawList3D*, plVec3 center, float radius, uint32_t segments, plDrawLineOptions);
    void (*add_3d_sphere)      (plDrawList3D*, plDrawSphereDesc, plDrawLineOptions);
    void (*add_3d_capsule)     (plDrawList3D*, plDrawCapsuleDesc, plDrawLineOptions);
    void (*add_3d_cylinder)    (plDrawList3D*, plDrawCylinderDesc, plDrawLineOptions);
    void (*add_3d_cone)        (plDrawList3D*, plDrawConeDesc, plDrawLineOptions);

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
    PL_DRAW_RECT_FLAG_NONE                       = 0, // default: PL_DRAW_RECT_FLAG_ROUND_CORNERS_All
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT     = 1 << 0,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT    = 1 << 1,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT  = 1 << 2,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT = 1 << 4,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_NONE         = 1 << 5,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP          = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM       = PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_LEFT         = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_RIGHT        = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
    PL_DRAW_RECT_FLAG_ROUND_CORNERS_All          = PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_TOP_RIGHT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_LEFT | PL_DRAW_RECT_FLAG_ROUND_CORNERS_BOTTOM_RIGHT,
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
    uint32_t uLatBands;  // default: 16
    uint32_t uLongBands; // default: 16
    float    fEndOffsetRatio;
} plDrawCapsuleDesc;

typedef struct _plDrawSphereDesc
{
    plVec3   tCenter;
    float    fRadius;
    uint32_t uLatBands;  // default: 16
    uint32_t uLongBands; // default: 16
} plDrawSphereDesc;

typedef struct _plDrawCylinderDesc
{
    plVec3   tBasePos;
    plVec3   tTipPos;
    float    fRadius;
    uint32_t uSegments; // default: 12
} plDrawCylinderDesc;

typedef struct _plDrawConeDesc
{
    plVec3   tBasePos;
    plVec3   tTipPos;
    float    fRadius;
    uint32_t uSegments; // default: 12
} plDrawConeDesc;

typedef struct _plDrawFrustumDesc
{
    float fYFov;
    float fAspectRatio;
    float fNearZ;
    float fFarZ;
} plDrawFrustumDesc;

typedef struct _plDrawLineOptions
{
    uint32_t uColor;
    float    fThickness;
} plDrawLineOptions;

typedef struct _plDrawSolidOptions
{
    uint32_t uColor;
} plDrawSolidOptions;

typedef struct _plDrawTextOptions
{
    plFont*     ptFont;
    float       fSize;     // if zero, will use loaded size
    uint32_t    uColor;
    float       fWrap;     // 0.0f, no wrap
    const char* pcTextEnd; // if null terminated, set to NULL
} plDrawTextOptions;

typedef struct _plFontRange
{
    int         iFirstCodePoint;
    uint32_t    uCharCount;

    // [INTERNAL]
    uint32_t _uConfigIndex;
} plFontRange;

typedef struct _plFontConfig
{
    float              fSize;
    const plFontRange* ptRanges;
    uint32_t           uRangeCount;
    const int*         piIndividualChars;
    uint32_t           uIndividualCharCount;
    plFont*            ptMergeFont;

    // BITMAP ONLY
    uint32_t uVOverSampling;
    uint32_t uHOverSampling;

    // SDF ONLY (ttf only)
    bool          bSdf;
    int           iSdfPadding;
    unsigned char ucOnEdgeValue;
    
    // [INTERNAL]
    plFontRange* _sbtRanges;
    plFontChar* _sbtCharData;
    float       _fSdfPixelDistScale;
} plFontConfig;

typedef struct _plFont
{
    float fSize; // loaded size
    
    // [INTERNAL]
    float                   _fLineSpacing;
    plFontRange*            _sbtRanges;
    uint32_t                _uCodePointCount;
    uint32_t*               _auCodePoints; // glyph index lookup based on codepoint
    plFontGlyph*            _sbtGlyphs;     // glyphs
    plFontConfig*           _sbtConfigs;
    struct _plFontPrepData* _sbtPreps;
    plFont*                 _ptNextFont;
    plFontGlyph*            _ptFallbackGlyph;
} plFont;

//-----------------------------------------------------------------------------
// [SECTION] structs for backends
//-----------------------------------------------------------------------------

typedef struct _plDrawVertex
{
    float    afPos[2];
    float    afUv[2];
    uint32_t uColor;
} plDrawVertex;

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
    plFont*  ptFont;
    float    fSize;
    plVec3   tP;
    uint32_t uColor;
    char     acText[PL_MAX_NAME_LENGTH];
    float    fWrap;
} plDraw3DText;

typedef struct _plDrawList3D
{
    // solid
    plDrawVertex3DSolid* sbtSolidVertexBuffer;
    uint32_t*            sbtSolidIndexBuffer;

    // lines
    plDrawVertex3DLine*  sbtLineVertexBuffer;
    uint32_t*            sbtLineIndexBuffer;

    // text
    plDraw3DText*  sbtTextEntries;
    plDrawList2D*  pt2dDrawlist;
    plDrawLayer2D* ptLayer;
} plDrawList3D;

typedef struct _plDrawCommand
{
    uint32_t    uVertexOffset;
    uint32_t    uIndexOffset;
    uint32_t    uElementCount;
    plTextureID tTextureId;
    plRect      tClip;
    bool        bSdf;
} plDrawCommand;

typedef struct _plDrawList2D
{
    plDrawVertex*  sbtVertexBuffer;
    uint32_t*      sbuIndexBuffer;
    uint32_t       uIndexBufferByteSize;
    plDrawCommand* sbtDrawCommands;
    
    // [INTERNAL]
    plDrawLayer2D** _sbtSubmittedLayers;
    plDrawLayer2D** _sbtLayerCache;
    plDrawLayer2D** _sbtLayersCreated;
    plRect*         _sbtClipStack;
} plDrawList2D;

typedef struct _plFontAtlas
{

    plVec2         tAtlasSize;
    plTextureID    tTexture;
    unsigned char* pucPixelsAsRGBA32;
    void*          ptUserData;

    // [INTERNAL]
    plFont*           _ptFontListHead;
    plFontCustomRect* _sbtCustomRects;
    unsigned char*    _pucPixelsAsAlpha8;
    plVec2            _tWhiteUv;
    int               _iGlyphPadding;
    size_t            _szPixelDataSize;
    plFontCustomRect* _ptWhiteRect;
    float             _fTotalArea;
} plFontAtlas;

#endif // PL_DRAW_EXT_H