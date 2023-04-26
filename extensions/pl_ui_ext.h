/*
   pl_ui_ext.h
*/

// library version
#define PL_UI_VERSION    "0.8.0"
#define PL_UI_VERSION_NUM 00800

/*
Index of this file:
// [SECTION] header mess
// [SECTION] apis
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] public api struct
// [SECTION] enums
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_EXT_H
#define PL_UI_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_UI "PL_API_UI"
typedef struct _plUiApiI plUiApiI;

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiContext plUiContext; // internal

// enums
typedef int plUiConditionFlags;
typedef int plUiLayoutRowType;

// external apis
typedef struct _plIOApiI plIOApiI; // pilotlight.h

// external (pl_draw_ext.h)
typedef struct _plDrawApiI    plDrawApiI;
typedef struct _plDrawContext plDrawContext;
typedef struct _plDrawList    plDrawList;
typedef struct _plDrawLayer   plDrawLayer;
typedef struct _plFont        plFont;
typedef void*                 plTextureId;

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

plUiApiI* pl_load_ui_api(void);

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plUiApiI
{
    // context creation & access
    plUiContext*   (*create_context) (plIOApiI* ptIoI, plDrawApiI* ptDraw);
    void           (*destroy_context)(plUiContext* ptContext);
    void           (*set_context)    (plUiContext* ptCtx); // must be set when crossing DLL boundary
    plUiContext*   (*get_context)    (void);
    void           (*set_draw_api)   (plDrawApiI* ptDraw);
    void           (*set_io_api)     (plIOApiI* ptIoI);

    // render data
    plDrawContext* (*get_draw_context)   (plUiContext* ptContext);
    plDrawList*    (*get_draw_list)      (plUiContext* ptContext);
    plDrawList*    (*get_debug_draw_list)(plUiContext* ptContext);

    // main
    void           (*new_frame)(void); // start a new pilotlight ui frame, this should be the first command before calling any commands below
    void           (*end_frame)(void); // ends pilotlight ui frame, automatically called by pl_ui_render()
    void           (*render)   (void); // submits draw layers, you can then submit the ptDrawlist & ptDebugDrawlist from context

    // tools
    void           (*debug)(bool* pbOpen);
    void           (*style)(bool* pbOpen);
    void           (*demo) (bool* pbOpen);

    // styling
    void           (*set_dark_theme)(void);

    // fonts
    void           (*set_default_font)(plFont* ptFont);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~windows~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // windows
    // - only call "pl_ui_end_window()" if "pl_ui_begin_window()" returns true (its call automatically if false)
    // - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
    // - "pl_ui_end_window()" will return false if collapsed or clipped
    // - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
    bool           (*begin_window)(const char* pcName, bool* pbOpen, bool bAutoSize);
    void           (*end_window)  (void);

    // window utilities
    plDrawLayer*   (*get_window_fg_drawlayer)(void); // returns current window foreground drawlist (call between pl_ui_begin_window(...) & pl_ui_end_window(...))
    plDrawLayer*   (*get_window_bg_drawlayer)(void); // returns current window background drawlist (call between pl_ui_begin_window(...) & pl_ui_end_window(...))
    plVec2         (*get_cursor_pos)(void);          // returns current cursor position (where the next widget will start drawing)

    // child windows
    // - only call "pl_ui_end_child()" if "pl_ui_begin_child()" returns true (its call automatically if false)
    // - self-contained window with scrolling & clipping
    bool           (*begin_child)(const char* pcName);
    void           (*end_child)  (void);

    // tooltips
    // - window that follows the mouse (usually used in combination with "pl_ui_was_last_item_hovered()")
    void           (*begin_tooltip)(void);
    void           (*end_tooltip)  (void);

    // window utilities
    // - refers to current window (between "pl_ui_begin_window()" & "pl_ui_end_window()")
    plVec2         (*get_window_pos) (void);
    plVec2         (*get_window_size)(void);

    // window manipulation
    // - call before "pl_ui_begin_window()"
    void           (*set_next_window_pos)     (plVec2 tPos, plUiConditionFlags tCondition);
    void           (*set_next_window_size)    (plVec2 tSize, plUiConditionFlags tCondition);
    void           (*set_next_window_collapse)(bool bCollapsed, plUiConditionFlags tCondition);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widgets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    bool           (*button)          (const char* pcText);
    bool           (*selectable)      (const char* pcText, bool* bpValue);
    bool           (*checkbox)        (const char* pcText, bool* pbValue);
    bool           (*radio_button)    (const char* pcText, int* piValue, int iButtonValue);
    void           (*image)           (plTextureId tTexture, plVec2 tSize);
    void           (*image_ex)        (plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);
    bool           (*invisible_button)(const char* pcText, plVec2 tSize);
    void           (*dummy)           (plVec2 tSize);

    // plotting
    void           (*progress_bar)(float fFraction, plVec2 tSize, const char* pcOverlay);

    // text
    void           (*text)          (const char* pcFmt, ...);
    void           (*text_v)        (const char* pcFmt, va_list args);
    void           (*color_text)    (plVec4 tColor, const char* pcFmt, ...);
    void           (*color_text_v)  (plVec4 tColor, const char* pcFmt, va_list args);
    void           (*labeled_text)  (const char* pcLabel, const char* pcFmt, ...);
    void           (*labeled_text_v)(const char* pcLabel, const char* pcFmt, va_list args);

    // sliders
    bool           (*slider_float)  (const char* pcLabel, float* pfValue, float fMin, float fMax);
    bool           (*slider_float_f)(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
    bool           (*slider_int)    (const char* pcLabel, int* piValue, int iMin, int iMax);
    bool           (*slider_int_f)  (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);

    // drag sliders
    bool           (*drag_float)  (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
    bool           (*drag_float_f)(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);

    // trees
    // - only call "pl_ui_tree_pop()" if "pl_ui_tree_node()" returns true (its call automatically if false)
    // - only call "pl_ui_end_collapsing_header()" if "pl_ui_collapsing_header()" returns true (its call automatically if false)
    bool           (*collapsing_header)    (const char* pcText);
    void           (*end_collapsing_header)(void);
    bool           (*tree_node)            (const char* pcText);
    bool           (*tree_node_f)          (const char* pcFmt, ...);
    bool           (*tree_node_v)          (const char* pcFmt, va_list args);
    void           (*tree_pop)             (void);

    // tabs & tab bars
    // - only call "pl_ui_end_tab_bar()" if "pl_ui_begin_tab_bar()" returns true (its call automatically if false)
    // - only call "pl_ui_end_tab()" if "pl_ui_begin_tab()" returns true (its call automatically if false)
    bool           (*begin_tab_bar)(const char* pcText);
    void           (*end_tab_bar)  (void);
    bool           (*begin_tab)    (const char* pcText);
    void           (*end_tab)      (void);

    // misc.
    void           (*separator)       (void);
    void           (*vertical_spacing)(void);
    void           (*indent)          (float fIndent);
    void           (*unindent)        (float fIndent);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~layout systems~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // - layout systems are based on "Nuklear" (https://github.com/Immediate-Mode-UI/Nuklear)
    // - 6 different layout strategies
    // - default layout strategy is system 2 with uWidgetCount=1 & fWidth=300 & fHeight=0
    // - setting fHeight=0 will cause the row height to be equal to the minimal height of the maximum height widget

    // layout system 1
    // - provides each widget with the same horizontal space and grows dynamically with the parent window
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void           (*layout_dynamic)(float fHeight, uint32_t uWidgetCount);

    // layout system 2
    // - provides each widget with the same horizontal pixel widget and does not grow with the parent window
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void           (*layout_static)(float fHeight, float fWidth, uint32_t uWidgetCount);

    // layout system 3
    // - allows user to change the width per widget
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth is pixel width
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width
    // - does not wrap
    void           (*layout_row_begin)(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
    void           (*layout_row_push) (float fWidth);
    void           (*layout_row_end)  (void);

    // layout system 4
    // - same as layout system 3 but the "array" form
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void           (*layout_row)(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios);

    // layout system 5
    // - similar to a flexbox
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void           (*layout_template_begin)        (float fHeight);
    void           (*layout_template_push_dynamic) (void);         // can go to minimum widget width if not enough space (10 pixels)
    void           (*layout_template_push_variable)(float fWidth); // variable width with min pixel width of fWidth but can grow bigger if enough space
    void           (*layout_template_push_static)  (float fWidth); // static pixel width of fWidth
    void           (*layout_template_end)          (void);

    // layout system 6
    // - allows user to place widgets freely
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth/fHeight is pixel width/height
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth/fHeight is a ratio of the available width/height (for pl_ui_layout_space_begin())
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width & fHeight is a ratio of fHeight given to "pl_ui_layout_space_begin()" (for pl_ui_layout_space_push())
    void          (*layout_space_begin)(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount);
    void          (*layout_space_push)(float fX, float fY, float fWidth, float fHeight);
    void          (*layout_space_end)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~state query~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    bool          (*was_last_item_hovered)(void);
    bool          (*was_last_item_active) (void);
    bool          (*is_mouse_owned)       (void);
} plUiApiI;

//-----------------------------------------------------------------------------
// [SECTION] enums
//-----------------------------------------------------------------------------

enum plUiConditionFlags_
{
    PL_UI_COND_NONE   = 0,
    PL_UI_COND_ALWAYS = 1 << 0,
    PL_UI_COND_ONCE   = 1 << 1
};

enum plUiLayoutRowType_
{
    PL_UI_LAYOUT_ROW_TYPE_NONE, // don't use (internal only)
    PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
    PL_UI_LAYOUT_ROW_TYPE_STATIC
};

#endif // PL_UI_EXT_H
