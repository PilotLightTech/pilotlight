/*
   pl_ui_ext.h
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] apis
// [SECTION] forward declarations
// [SECTION] public api struct
// [SECTION] enums & flags
// [SECTION] structs
*/


// library version
#define PL_UI_EXT_VERSION    "0.1.0"
#define PL_UI_EXT_VERSION_NUM 000100

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] forward declarations
// [SECTION] public api
// [SECTION] enums & flags
// [SECTION] structs
*/

//-----------------------------------------------------------------------------
// [SECTION] header mess
//-----------------------------------------------------------------------------

#ifndef PL_UI_EXT_H
#define PL_UI_EXT_H

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdbool.h> // bool
#include <stdint.h>  // uint*_t
#include <stdarg.h>  // va list
#include <stddef.h>  // size_t
#include "pl_math.h"

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

#define PL_API_UI "PL_API_UI"
typedef struct _plUiI plUiI;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiContext  plUiContext;  // (opaque structure)
typedef struct _plUiClipper  plUiClipper;  // data used with "pl_step_clipper(...)" function (see function)

// enums
typedef int plUiConditionFlags;   // -> enum plUiConditionFlags_   // Enum: A conditional for some functions (PL_UI_COND_XXX value)
typedef int plUiLayoutRowType;    // -> enum plUiLayoutRowType_    // Enum: A row type for the layout system (PL_UI_LAYOUT_ROW_TYPE_XXX)
typedef int plUiInputTextFlags;   // -> enum plUiInputTextFlags_   // Enum: Internal flags for input text (PL_UI_INPUT_TEXT_FLAGS_XXX)
typedef int plUiWindowFlags;      // -> enum plUiWindowFlags_      // Enum: An input event source (PL_UI_WINDOW_FLAGS_XXXX)
typedef int plUiColor;            // -> enum plUiColor_            // Enum: An input event source (PL_UI_COLOR_XXXX)

// external
typedef struct _plDrawList2D    plDrawList2D;    // pl_draw_ext.h
typedef struct _plDrawLayer2D   plDrawLayer2D;   // pl_draw_ext.h
typedef union  _plFontHandle    plFontHandle;    // pl_draw_ext.h
typedef struct _plRenderEncoder plRenderEncoder; // pl_graphics_ext.h
typedef union  plTextureHandle  plTextureHandle; // pl_graphics_ext.h

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plUiI
{

    void (*initialize)(void);
    void (*cleanup)(void);

    // render data
    plDrawList2D* (*get_draw_list)      (void);
    plDrawList2D* (*get_debug_draw_list)(void);

    // main
    void (*new_frame)(void); // start a new pilotlight ui frame, this should be the first command before calling any commands below
    void (*end_frame)(void); // ends pilotlight ui frame, automatically called by pl_render()
    void (*render)(plRenderEncoder, float fWidth, float fHeight, uint32_t uMSAASampleCount); // submits draw layers, you can then submit the ptDrawlist & ptDebugDrawlist from context

    // tools
    void (*show_debug_window)       (bool* pbOpen);
    void (*show_style_editor_window)(bool* pbOpen);
    void (*show_demo_window)        (bool* pbOpen);

    // styling
    void (*set_dark_theme)  (void);
    void (*push_theme_color)(plUiColor tColor, const plVec4* ptColor);
    void (*pop_theme_color) (uint32_t uCount);

    // fonts
    void         (*set_default_font)(plFontHandle);
    plFontHandle (*get_default_font)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~windows~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // windows
    // - only call "pl_end_window()" if "pl_begin_window()" returns true (its call automatically if false)
    // - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
    // - "pl_end_window()" will return false if collapsed or clipped
    // - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
    bool (*begin_window)(const char* pcName, bool* pbOpen, plUiWindowFlags);
    void (*end_window)  (void);

    // window utilities
    plDrawLayer2D* (*get_window_fg_drawlayer)(void); // returns current window foreground drawlist (call between pl_begin_window(...) & pl_end_window(...))
    plDrawLayer2D* (*get_window_bg_drawlayer)(void); // returns current window background drawlist (call between pl_begin_window(...) & pl_end_window(...))
    plVec2 (*get_cursor_pos)(void); // returns current cursor position (where the next widget will start drawing)

    // child windows
    // - only call "pl_end_child()" if "pl_begin_child()" returns true (its call automatically if false)
    // - self-contained window with scrolling & clipping
    bool (*begin_child)(const char* pcName);
    void (*end_child)  (void);

    // tooltips
    // - window that follows the mouse (usually used in combination with "pl_was_last_item_hovered()")
    void (*begin_tooltip)(void);
    void (*end_tooltip)  (void);

    // window utilities
    // - refers to current window (between "pl_begin_window()" & "pl_end_window()")
    plVec2 (*get_window_pos)       (void);
    plVec2 (*get_window_size)      (void);
    plVec2 (*get_window_scroll)    (void);
    plVec2 (*get_window_scroll_max)(void);
    void   (*set_window_scroll)    (plVec2 tScroll);

    // window manipulation
    // - call before "pl_begin_window()"
    void (*set_next_window_pos)     (plVec2 tPos, plUiConditionFlags);
    void (*set_next_window_size)    (plVec2 tSize, plUiConditionFlags);
    void (*set_next_window_collapse)(bool bCollapsed, plUiConditionFlags);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widgets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    bool (*button)          (const char* pcText);
    bool (*selectable)      (const char* pcText, bool* bpValue);
    bool (*checkbox)        (const char* pcText, bool* pbValue);
    bool (*radio_button)    (const char* pcText, int* piValue, int iButtonValue);
    void (*image)           (plTextureHandle, plVec2 tSize);
    void (*image_ex)        (plTextureHandle, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);
    bool (*image_button)    (const char* pcId, plTextureHandle, plVec2 tSize);
    bool (*image_button_ex) (const char* pcId, plTextureHandle, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor);
    bool (*invisible_button)(const char* pcText, plVec2 tSize);
    void (*dummy)           (plVec2 tSize);

    // plotting
    void (*progress_bar)(float fFraction, plVec2 tSize, const char* pcOverlay);

    // text
    void (*text)          (const char* pcFmt, ...);
    void (*text_v)        (const char* pcFmt, va_list args);
    void (*color_text)    (plVec4 tColor, const char* pcFmt, ...);
    void (*color_text_v)  (plVec4 tColor, const char* pcFmt, va_list args);
    void (*labeled_text)  (const char* pcLabel, const char* pcFmt, ...);
    void (*labeled_text_v)(const char* pcLabel, const char* pcFmt, va_list args);

    // input
    bool (*input_text)     (const char* pcLabel, char* pcBuffer, size_t szBufferSize);
    bool (*input_text_hint)(const char* pcLabel, const char* pcHint, char* pcBuffer, size_t szBufferSize);
    bool (*input_float)    (const char* pcLabel, float* fValue, const char* pcFormat);
    bool (*input_int)      (const char* pcLabel, int* iValue);

    // sliders
    bool (*slider_float)  (const char* pcLabel, float* pfValue, float fMin, float fMax);
    bool (*slider_float_f)(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat);
    bool (*slider_int)    (const char* pcLabel, int* piValue, int iMin, int iMax);
    bool (*slider_int_f)  (const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat);

    // drag sliders
    bool (*drag_float)  (const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax);
    bool (*drag_float_f)(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat);

    // trees
    // - only call "pl_tree_pop()" if "pl_tree_node()" returns true (its call automatically if false)
    // - only call "pl_end_collapsing_header()" if "pl_collapsing_header()" returns true (its call automatically if false)
    bool (*collapsing_header)    (const char* pcText);
    void (*end_collapsing_header)(void);
    bool (*tree_node)            (const char* pcText);
    bool (*tree_node_f)          (const char* pcFmt, ...);
    bool (*tree_node_v)          (const char* pcFmt, va_list args);
    void (*tree_pop)             (void);

    // tabs & tab bars
    // - only call "pl_end_tab_bar()" if "pl_begin_tab_bar()" returns true (its call automatically if false)
    // - only call "pl_end_tab()" if "pl_begin_tab()" returns true (its call automatically if false)
    bool (*begin_tab_bar)(const char* pcText);
    void (*end_tab_bar)  (void);
    bool (*begin_tab)    (const char* pcText);
    void (*end_tab)      (void);

    // misc.
    void (*separator)       (void);
    void (*vertical_spacing)(void);
    void (*indent)          (float);
    void (*unindent)        (float);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~clipper~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // - clipper based on "Dear ImGui"'s ImGuiListClipper (https://github.com/ocornut/imgui)
    // - Used for large numbers of evenly spaced rows.
    // - Without Clipper:
    //       for(uint32_t i = 0; i < QUANTITY; i++)
    //           ptUi->text("%i", i);
    // - With Clipper:
    //       plUiClipper tClipper = {QUANTITY};
    //       while(ptUi->step_clipper(&tClipper))
    //           for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
    //               ptUi->text("%i", i);
    bool (*step_clipper)(plUiClipper*);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~layout systems~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // - layout systems are based on "Nuklear" (https://github.com/Immediate-Mode-UI/Nuklear)
    // - 6 different layout strategies
    // - default layout strategy is system 2 with uWidgetCount=1 & fWidth=300 & fHeight=0
    // - setting fHeight=0 will cause the row height to be equal to the minimal height of the maximum height widget

    // layout system 1
    // - provides each widget with the same horizontal space and grows dynamically with the parent window
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_dynamic)(float fHeight, uint32_t uWidgetCount);

    // layout system 2
    // - provides each widget with the same horizontal pixel widget and does not grow with the parent window
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_static)(float fHeight, float fWidth, uint32_t uWidgetCount);

    // layout system 3
    // - allows user to change the width per widget
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth is pixel width
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width
    // - does not wrap
    void (*layout_row_begin)(plUiLayoutRowType, float fHeight, uint32_t uWidgetCount);
    void (*layout_row_push) (float fWidth);
    void (*layout_row_end)  (void);

    // layout system 4
    // - same as layout system 3 but the "array" form
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_row)(plUiLayoutRowType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios);

    // layout system 5
    // - similar to a flexbox
    // - wraps (i.e. setting uWidgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_template_begin)        (float fHeight);
    void (*layout_template_push_dynamic) (void);         // can go to minimum widget width if not enough space (10 pixels)
    void (*layout_template_push_variable)(float fWidth); // variable width with min pixel width of fWidth but can grow bigger if enough space
    void (*layout_template_push_static)  (float fWidth); // static pixel width of fWidth
    void (*layout_template_end)          (void);

    // layout system 6
    // - allows user to place widgets freely
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then fWidth/fHeight is pixel width/height
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth/fHeight is a ratio of the available width/height (for pl_layout_space_begin())
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then fWidth is a ratio of the available width & fHeight is a ratio of fHeight given to "pl_layout_space_begin()" (for pl_layout_space_push())
    void (*layout_space_begin)(plUiLayoutRowType, float fHeight, uint32_t uWidgetCount);
    void (*layout_space_push) (float fX, float fY, float fWidth, float fHeight);
    void (*layout_space_end)  (void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~state query~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    bool (*was_last_item_hovered)(void);
    bool (*was_last_item_active) (void);

} plUiI;


//-----------------------------------------------------------------------------
// [SECTION] enums & flags
//-----------------------------------------------------------------------------

enum plUiColor_
{
    PL_UI_COLOR_TITLE_ACTIVE,
    PL_UI_COLOR_TITLE_BG,
    PL_UI_COLOR_TITLE_BG_COLLAPSED,
    PL_UI_COLOR_WINDOW_BG,
    PL_UI_COLOR_WINDOW_BORDER,
    PL_UI_COLOR_CHILD_BG,
    PL_UI_COLOR_BUTTON,
    PL_UI_COLOR_BUTTON_HOVERED,
    PL_UI_COLOR_BUTTON_ACTIVE,
    PL_UI_COLOR_TEXT,
    PL_UI_COLOR_PROGRESS_BAR,
    PL_UI_COLOR_CHECKMARK,
    PL_UI_COLOR_FRAME_BG,
    PL_UI_COLOR_FRAME_BG_HOVERED,
    PL_UI_COLOR_FRAME_BG_ACTIVE,
    PL_UI_COLOR_HEADER,
    PL_UI_COLOR_HEADER_HOVERED,
    PL_UI_COLOR_HEADER_ACTIVE,
    PL_UI_COLOR_SCROLLBAR_BG,
    PL_UI_COLOR_SCROLLBAR_HANDLE,
    PL_UI_COLOR_SCROLLBAR_FRAME,
    PL_UI_COLOR_SCROLLBAR_ACTIVE,
    PL_UI_COLOR_SCROLLBAR_HOVERED
};

enum plUiWindowFlags_
{
    PL_UI_WINDOW_FLAGS_NONE         = 0,
    PL_UI_WINDOW_FLAGS_NO_TITLE_BAR = 1 << 0,
    PL_UI_WINDOW_FLAGS_NO_RESIZE    = 1 << 1,
    PL_UI_WINDOW_FLAGS_NO_MOVE      = 1 << 2,
    PL_UI_WINDOW_FLAGS_NO_COLLAPSE  = 1 << 3,
    PL_UI_WINDOW_FLAGS_AUTO_SIZE    = 1 << 4,

    // internal
    PL_UI_WINDOW_FLAGS_CHILD_WINDOW = 1 << 5,
    PL_UI_WINDOW_FLAGS_TOOLTIP      = 1 << 6,
};

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

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plUiClipper
{
    uint32_t uItemCount;
    uint32_t uDisplayStart;
    uint32_t uDisplayEnd;
    float    _fItemHeight;
    float    _fStartPosY;
} plUiClipper;

#endif // PL_UI_EXT_H