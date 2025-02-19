/*
   pl_ui_ext.h
     - immediate mode UI extension
*/

/*
Index of this file:
// [SECTION] header mess
// [SECTION] includes
// [SECTION] api
// [SECTION] forward declarations
// [SECTION] public api struct
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
// [SECTION] api
//-----------------------------------------------------------------------------

#define plUiI_version (plVersion){1, 1, 0}

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// basic types
typedef struct _plUiContext    plUiContext;    // (opaque structure)
typedef struct _plUiClipper    plUiClipper;    // data used with "step_clipper(...)" function (see function)
typedef struct _plUiTextFilter plUiTextFilter; // data used with "step_clipper(...)" function (see function)

// enums
typedef int plUiConditionFlags;  // -> enum plUiConditionFlags_   // Enum:  A conditional for some functions (PL_UI_COND_XXX value)
typedef int plUiLayoutRowType;   // -> enum plUiLayoutRowType_    // Enum:  A row type for the layout system (PL_UI_LAYOUT_ROW_TYPE_XXX)
typedef int plUiInputTextFlags;  // -> enum plUiInputTextFlags_   // Flags: flags for input text (PL_UI_INPUT_TEXT_FLAGS_XXX)
typedef int plUiWindowFlags;     // -> enum plUiWindowFlags_      // Flags: An input event source (PL_UI_WINDOW_FLAGS_XXXX)
typedef int plUiComboFlags;      // -> enum plUiComboFlags_       // Enum:  An input event source (PL_UI_WINDOW_FLAGS_XXXX)
typedef int plUiColor;           // -> enum plUiColor_            // Enum:  An input event source (PL_UI_COLOR_XXXX)
typedef int plUiChildFlags;      // -> enum plUiChildFlags_       // Flags: reserved for future use
typedef int plUiSelectableFlags; // -> enum plUiSelectableFlags_  // Flags: reserved for future use
typedef int plUiSliderFlags;     // -> enum plUiSliderFlags_      // Flags: reserved for future use
typedef int plUiTreeNodeFlags;   // -> enum plUiTreeNodeFlags_    // Flags: reserved for future use
typedef int plUiTabBarFlags;     // -> enum plUiTabBarFlags_      // Flags: reserved for future use
typedef int plUiTabFlags;        // -> enum plUiTabFlags_         // Flags: reserved for future use
typedef int plUiPopupFlags;      // -> enum plUiPopupFlags_       // Flags: reserved for future use

// external
typedef struct _plDrawList2D  plDrawList2D;  // pl_draw_ext.h
typedef struct _plDrawLayer2D plDrawLayer2D; // pl_draw_ext.h
typedef struct _plFont        plFont;        // pl_draw_ext.h

#ifndef plTextureID
    typedef uint32_t plTextureID;
#endif

//-----------------------------------------------------------------------------
// [SECTION] public api struct
//-----------------------------------------------------------------------------

typedef struct _plUiI
{
    // setup/shutdown
    void (*initialize)(void);
    void (*cleanup)   (void);

    // render data
    plDrawList2D* (*get_draw_list)      (void);
    plDrawList2D* (*get_debug_draw_list)(void);

    // main
    void (*new_frame)(void); // start a new ui frame, this should be the first command before calling any commands below
    void (*end_frame)(void); // ends ui frame

    // mouse/keyboard ownership
    bool (*wants_mouse_capture)   (void);
    bool (*wants_keyboard_capture)(void);
    bool (*wants_text_input)      (void);

    // tools
    void (*show_debug_window)       (bool* open);
    void (*show_style_editor_window)(bool* open);

    // styling
    void (*set_dark_theme)  (void);
    void (*push_theme_color)(plUiColor colorIndex, plVec4 color);
    void (*pop_theme_color) (uint32_t count);

    // fonts
    void    (*set_default_font)(plFont*);
    plFont* (*get_default_font)(void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~windows~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // windows
    // - only call "end_window()" if "begin_window()" returns true (its call automatically if false)
    // - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
    // - "begin_window()" will return false if collapsed or clipped
    // - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
    bool (*begin_window)(const char* name, bool* open, plUiWindowFlags);
    void (*end_window)  (void);

    // popups
    // - only call "end_popup()" if "begin_popup()" returns true (its call automatically if false)
    // - passing a valid pointer to pbOpen will show a red circle that will turn pbOpen false when clicked
    // - "begin_popup()" will return false if collapsed or clipped
    // - if you use autosize, make sure at least 1 row has a static component (or the window will grow unbounded)
    void (*open_popup)         (const char* name, plUiPopupFlags);
    void (*close_current_popup)(void);
    bool (*is_popup_open)      (const char* name);
    bool (*begin_popup)        (const char* name, plUiWindowFlags);
    void (*end_popup)          (void);

    // window utilities
    plDrawLayer2D* (*get_window_fg_drawlayer)(void); // returns current window foreground drawlist (call between begin_window(...) & end_window(...))
    plDrawLayer2D* (*get_window_bg_drawlayer)(void); // returns current window background drawlist (call between begin_window(...) & end_window(...))
    plVec2         (*get_cursor_pos)         (void); // returns current cursor position (where the next widget will start drawing)

    // child windows
    // - only call "end_child()" if "begin_child()" returns true (its call automatically if false)
    // - self-contained window with scrolling & clipping
    bool (*begin_child)(const char* name, plUiChildFlags, plUiWindowFlags);
    void (*end_child)  (void);

    // tooltips
    // - window that follows the mouse (usually used in combination with "was_last_item_hovered()")
    void (*begin_tooltip)(void);
    void (*end_tooltip)  (void);

    // window utilities
    // - refers to current window (between "begin_window()" & "end_window()")
    plVec2 (*get_window_pos)       (void);
    plVec2 (*get_window_size)      (void);
    plVec2 (*get_window_scroll)    (void);
    plVec2 (*get_window_scroll_max)(void);
    void   (*set_window_scroll)    (plVec2 scroll);

    // window manipulation
    // - call before "begin_window()"
    void (*set_next_window_pos)     (plVec2 pos, plUiConditionFlags);
    void (*set_next_window_size)    (plVec2 size, plUiConditionFlags);
    void (*set_next_window_collapse)(bool collapsed, plUiConditionFlags);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~widgets~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // main
    bool (*button)          (const char* text);
    bool (*selectable)      (const char* text, bool* value, plUiSelectableFlags);
    bool (*checkbox)        (const char* text, bool* value);
    bool (*radio_button)    (const char* text, int* valuePtr, int value);
    void (*image)           (plTextureID, plVec2 size);
    void (*image_ex)        (plTextureID, plVec2 size, plVec2 uv0, plVec2 uv1, plVec4 tintColor, plVec4 borderColor);
    bool (*image_button)    (const char* id, plTextureID, plVec2 size);
    bool (*image_button_ex) (const char* id, plTextureID, plVec2 size, plVec2 uv0, plVec2 uv1, plVec4 tintColor, plVec4 borderColor);
    bool (*invisible_button)(const char* text, plVec2 size);
    void (*dummy)           (plVec2 size);

    // plotting
    void (*progress_bar)(float fraction, plVec2 size, const char* overlay);

    // text
    void (*text)          (const char* fmt, ...);
    void (*text_v)        (const char* fmt, va_list args);
    void (*color_text)    (plVec4 color, const char* fmt, ...);
    void (*color_text_v)  (plVec4 color, const char* fmt, va_list args);
    void (*labeled_text)  (const char* label, const char* fmt, ...);
    void (*labeled_text_v)(const char* label, const char* fmt, va_list args);

    // input
    bool (*input_text)     (const char* label, char* buffer, size_t bufferSize, plUiInputTextFlags);
    bool (*input_text_hint)(const char* label, const char* hint, char* buffer, size_t bufferSize, plUiInputTextFlags);
    bool (*input_float)    (const char* label, float* value, const char* fmt, plUiInputTextFlags);
    bool (*input_float2)   (const char* label, float value[2], const char* fmt, plUiInputTextFlags);
    bool (*input_float3)   (const char* label, float value[3], const char* fmt, plUiInputTextFlags);
    bool (*input_float4)   (const char* label, float value[4], const char* fmt, plUiInputTextFlags);
    bool (*input_int)      (const char* label, int* value, plUiInputTextFlags);
    bool (*input_int2)     (const char* label, int value[2], plUiInputTextFlags);
    bool (*input_int3)     (const char* label, int value[3], plUiInputTextFlags);
    bool (*input_int4)     (const char* label, int value[4], plUiInputTextFlags);
    bool (*input_uint)     (const char* label, uint32_t* value, plUiInputTextFlags);
    bool (*input_uint2)    (const char* label, uint32_t value[2], plUiInputTextFlags);
    bool (*input_uint3)    (const char* label, uint32_t value[3], plUiInputTextFlags);
    bool (*input_uint4)    (const char* label, uint32_t value[4], plUiInputTextFlags);

    // sliders
    bool (*slider_float)  (const char* label, float* value, float minValue, float maxValue, plUiSliderFlags);
    bool (*slider_float_f)(const char* label, float* value, float minValue, float maxValue, const char* fmt, plUiSliderFlags);
    bool (*slider_int)    (const char* label, int* value, int minValue, int maxValue, plUiSliderFlags);
    bool (*slider_int_f)  (const char* label, int* value, int minValue, int maxValue, const char* fmt, plUiSliderFlags);
    bool (*slider_uint)   (const char* label, uint32_t* value, uint32_t minValue, uint32_t maxValue, plUiSliderFlags);
    bool (*slider_uint_f) (const char* label, uint32_t* value, uint32_t minValue, uint32_t maxValue, const char* fmt, plUiSliderFlags);

    // drag sliders
    bool (*drag_float)  (const char* label, float* value, float speed, float minValue, float maxValue, plUiSliderFlags);
    bool (*drag_float_f)(const char* label, float* value, float speed, float minValue, float maxValue, const char* fmt, plUiSliderFlags);

    // combo
    bool (*begin_combo)(const char* label, const char* preview, plUiComboFlags);
    void (*end_combo)  (void);

    // trees
    // - only call "tree_pop()" if "tree_node()" returns true (its call automatically if false)
    // - only call "end_collapsing_header()" if "begin_collapsing_header()" returns true (its call automatically if false)
    bool (*begin_collapsing_header)(const char* text, plUiTreeNodeFlags);
    void (*end_collapsing_header)  (void);
    bool (*tree_node)              (const char* text, plUiTreeNodeFlags);
    bool (*tree_node_f)            (const char* fmt, plUiTreeNodeFlags, ...);
    bool (*tree_node_v)            (const char* fmt, plUiTreeNodeFlags, va_list args);
    void (*tree_pop)               (void);

    // tabs & tab bars
    // - only call "end_tab_bar()" if "begin_tab_bar()" returns true (its call automatically if false)
    // - only call "end_tab()" if "begin_tab()" returns true (its call automatically if false)
    bool (*begin_tab_bar)(const char* text, plUiTabBarFlags);
    void (*end_tab_bar)  (void);
    bool (*begin_tab)    (const char* text, plUiTabFlags);
    void (*end_tab)      (void);

    // menus (not ready, do not use yet)
    bool (*begin_menu)      (const char* label, bool enabled);
    void (*end_menu)        (void);
    bool (*menu_item)       (const char* label, const char* shortcut, bool selected, bool enabled);
    bool (*menu_item_toggle)(const char* label, const char* shortcut, bool* selected, bool enabled);

    // misc.
    void (*separator_text)  (const char* text);
    void (*separator)       (void);
    void (*vertical_spacing)(void);
    void (*indent)          (float);
    void (*unindent)        (float);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~text filter~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    void (*text_filter_cleanup)(plUiTextFilter*);
    void (*text_filter_build)  (plUiTextFilter*);
    bool (*text_filter_pass)   (plUiTextFilter*, const char* text, const char* text_end);
    bool (*text_filter_active) (plUiTextFilter*);

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
    // - default layout strategy is system 2 with widgetCount=1 & width=300 & height=0
    // - setting fHeight=0 will cause the row height to be equal to the minimum height of the maximum height widget
    // - currently, there is a bug where the layout system isn't restored (see ISSUE #54),
    //   this will be fixed soon

    // layout system 1
    // - provides each widget with the same horizontal space and grows dynamically with the parent window
    // - wraps (i.e. setting widgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_dynamic)(float height, uint32_t widgetCount);

    // layout system 2
    // - provides each widget with the same horizontal pixel widget and does not grow with the parent window
    // - wraps (i.e. setting widgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_static)(float height, float width, uint32_t widgetCount);

    // layout system 3
    // - allows user to change the width per widget
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then width is pixel width
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then width is a ratio of the available width
    // - does not wrap
    void (*layout_row_begin)(plUiLayoutRowType, float height, uint32_t widgetCount);
    void (*layout_row_push) (float width);
    void (*layout_row_end)  (void);

    // layout system 4
    // - same as layout system 3 but the "array" form
    // - wraps (i.e. setting widgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_row)(plUiLayoutRowType, float height, uint32_t widgetCount, const float* sizesOrRatios);

    // layout system 5
    // - similar to a flexbox
    // - wraps (i.e. setting widgetCount to 2 and adding 4 widgets will create 2 rows)
    void (*layout_template_begin)        (float height);
    void (*layout_template_push_dynamic) (void);        // can go to minimum widget width if not enough space (10 pixels)
    void (*layout_template_push_variable)(float width); // variable width with min pixel width of width but can grow bigger if enough space
    void (*layout_template_push_static)  (float width); // static pixel width of width
    void (*layout_template_end)          (void);

    // layout system 6
    // - allows user to place widgets freely
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_STATIC, then width/height is pixel width/height
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then width/height is a ratio of the available width/height (for layout_space_begin())
    // - if tType=PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, then width is a ratio of the available width & height is a ratio of height given to "layout_space_begin()" (for layout_space_push())
    void (*layout_space_begin)(plUiLayoutRowType, float height, uint32_t widgetCount);
    void (*layout_space_push) (float fX, float fY, float width, float height);
    void (*layout_space_end)  (void);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ID stack~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    void (*push_id_string) (const char*);
    void (*push_id_pointer)(const void*);
    void (*push_id_uint)   (uint32_t);
    void (*pop_id)         (void);

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
    PL_UI_COLOR_TEXT_DISABLED,
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
    PL_UI_COLOR_SCROLLBAR_HOVERED,
    PL_UI_COLOR_SEPARATOR,
    PL_UI_COLOR_TAB,
    PL_UI_COLOR_TAB_HOVERED,
    PL_UI_COLOR_TAB_SELECTED,
    PL_UI_COLOR_RESIZE_GRIP,
    PL_UI_COLOR_RESIZE_GRIP_HOVERED,
    PL_UI_COLOR_RESIZE_GRIP_ACTIVE,
    PL_UI_COLOR_SLIDER,
    PL_UI_COLOR_SLIDER_HOVERED,
    PL_UI_COLOR_SLIDER_ACTIVE,
    PL_UI_COLOR_POPUP_BG,

    PL_UI_COLOR_COUNT
};

enum plUiWindowFlags_
{
    PL_UI_WINDOW_FLAGS_NONE                 = 0,
    PL_UI_WINDOW_FLAGS_NO_TITLE_BAR         = 1 << 0,
    PL_UI_WINDOW_FLAGS_NO_RESIZE            = 1 << 1,
    PL_UI_WINDOW_FLAGS_NO_MOVE              = 1 << 2,
    PL_UI_WINDOW_FLAGS_NO_COLLAPSE          = 1 << 3,
    PL_UI_WINDOW_FLAGS_AUTO_SIZE            = 1 << 4,
    PL_UI_WINDOW_FLAGS_NO_BACKGROUND        = 1 << 5,
    PL_UI_WINDOW_FLAGS_NO_SCROLLBAR         = 1 << 6,
    PL_UI_WINDOW_FLAGS_HORIZONTAL_SCROLLBAR = 1 << 7,

    // [INTERNAL]
    PL_UI_WINDOW_FLAGS_CHILD_WINDOW = 1 << 8,
    PL_UI_WINDOW_FLAGS_POPUP_WINDOW = 1 << 9,
    PL_UI_WINDOW_FLAGS_MENU         = 1 << 10,
};

enum plUiComboFlags_
{
    PL_UI_COMBO_FLAGS_NONE            = 0,
    PL_UI_COMBO_FLAGS_HEIGHT_SMALL    = 1 << 0, // max ~4 items visible
    PL_UI_COMBO_FLAGS_HEIGHT_REGULAR  = 1 << 1, // max ~8 items visible (default)
    PL_UI_COMBO_FLAGS_HEIGHT_LARGE    = 1 << 2, // max ~20 items visible
    PL_UI_COMBO_FLAGS_NO_ARROW_BUTTON = 1 << 3, // hide arrow button
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

enum plUiInputTextFlags_
{
    PL_UI_INPUT_TEXT_FLAGS_NONE                    = 0,
    PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL           = 1 << 0,   // allow 0123456789.+-*/
    PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL       = 1 << 1,   // allow 0123456789ABCDEFabcdef
    PL_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE         = 1 << 2,   // turn a..z into A..Z
    PL_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK          = 1 << 3,   // filter out spaces, tabs
    PL_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL         = 1 << 4,   // select entire text when first taking mouse focus
    PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE      = 1 << 5,   // return 'true' when Enter is pressed (as opposed to every time the value was modified). Consider looking at the IsItemDeactivatedAfterEdit() function.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_COMPLETION     = 1 << 6,   // callback on pressing TAB (for completion handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_HISTORY        = 1 << 7,   // callback on pressing Up/Down arrows (for history handling)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_ALWAYS         = 1 << 8,   // callback on each iteration. User code may query cursor position, modify text buffer.
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_CHAR_FILTER    = 1 << 9,   // callback on character inputs to replace or discard them. Modify 'EventChar' to replace or discard, or return 1 in callback to discard.
    PL_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT         = 1 << 10,  // pressing TAB input a '\t' character into the text field
    PL_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE = 1 << 11,  // in multi-line mode, unfocus with Enter, add new line with Ctrl+Enter (default is opposite: unfocus with Ctrl+Enter, add line with Enter).
    PL_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL    = 1 << 12,  // disable following the cursor horizontally
    PL_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE        = 1 << 13,  // overwrite mode
    PL_UI_INPUT_TEXT_FLAGS_READ_ONLY               = 1 << 14,  // read-only mode
    PL_UI_INPUT_TEXT_FLAGS_PASSWORD                = 1 << 15,  // password mode, display all characters as '*'
    PL_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO            = 1 << 16,  // disable undo/redo. Note that input text owns the text data while active, if you want to provide your own undo/redo stack you need e.g. to call ClearActiveID().
    PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC        = 1 << 17,  // allow 0123456789.+-*/eE (Scientific notation input)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_RESIZE         = 1 << 18,  // callback on buffer capacity changes request (beyond 'buf_size' parameter value), allowing the string to grow. Notify when the string wants to be resized (for string types which hold a cache of their Size). You will be provided a new BufSize in the callback and NEED to honor it. (see misc/cpp/imgui_stdlib.h for an example of using this)
    PL_UI_INPUT_TEXT_FLAGS_CALLBACK_EDIT           = 1 << 19,  // callback on any edit (note that InputText() already returns true on edit, the callback is useful mainly to manipulate the underlying buffer while focus is active)
    PL_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL       = 1 << 20,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    
    // [INTERNAL]
    PL_UI_INPUT_TEXT_FLAGS_MULTILINE      = 1 << 21,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
    PL_UI_INPUT_TEXT_FLAGS_NO_MARK_EDITED = 1 << 22,  // escape key clears content if not empty, and deactivate otherwise (contrast to default behavior of Escape to revert)
};

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plUiClipper
{
    uint32_t uItemCount;
    uint32_t uDisplayStart;
    uint32_t uDisplayEnd;

    // [INTERNAL]
    float _fItemHeight;
    float _fStartPosY;
} plUiClipper;

typedef struct _plUiTextFilter
{
    char                         acInputBuffer[256];
    uint32_t                     uCountGrep;
    struct _plUiTextFilterRange* sbtFilters;
} plUiTextFilter;

#endif // PL_UI_EXT_H