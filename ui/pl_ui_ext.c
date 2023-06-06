/*
   pl_ui_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] stb_text mess
// [SECTION] context
// [SECTION] global data
// [SECTION] public api implementations
// [SECTION] internal api implementations
// [SECTION] extension loading
// [SECTION] unity
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "pilotlight.h"
#include "pl_io.h"
#include "pl_ui_ext.h"
#include "pl_ui_internal.h"
#include "pl_draw_ext.h"
#include <float.h>

//-----------------------------------------------------------------------------
// [SECTION] stb_text mess
//-----------------------------------------------------------------------------

static plVec2
pl__input_text_calc_text_size_w(const plWChar* text_begin, const plWChar* text_end, const plWChar** remaining, plVec2* out_offset, bool stop_on_new_line)
{
    plFont* font = gptCtx->ptFont;
    const float line_height = gptCtx->tStyle.fFontSize;
    const float scale = line_height / font->config.fontSize;

    plVec2 text_size = {0};
    float line_width = 0.0f;

    const plWChar* s = text_begin;
    while (s < text_end)
    {
        unsigned int c = (unsigned int)(*s++);
        if (c == '\n')
        {
            text_size.x = pl_max(text_size.x, line_width);
            text_size.y += line_height;
            line_width = 0.0f;
            if (stop_on_new_line)
                break;
            continue;
        }
        if (c == '\r')
            continue;

        const float char_width = font->sbGlyphs[font->sbCodePoints[(plWChar)c]].xAdvance * scale;
        line_width += char_width;
    }

    if (text_size.x < line_width)
        text_size.x = line_width;

    if (out_offset)
        *out_offset = (plVec2){line_width, text_size.y + line_height};  // offset allow for the possibility of sitting after a trailing \n

    if (line_width > 0 || text_size.y == 0.0f)                        // whereas size.y will ignore the trailing \n
        text_size.y += line_height;

    if (remaining)
        *remaining = s;

    return text_size;
}


static int     STB_TEXTEDIT_STRINGLEN(const plInputTextState* obj)                             { return obj->iCurrentLengthW; }
static plWChar STB_TEXTEDIT_GETCHAR(const plInputTextState* obj, int idx)                      { return obj->sbTextW[idx]; }
static float   STB_TEXTEDIT_GETWIDTH(plInputTextState* obj, int line_start_idx, int char_idx)  { plWChar c = obj->sbTextW[line_start_idx + char_idx]; if (c == '\n') return STB_TEXTEDIT_GETWIDTH_NEWLINE; return gptCtx->ptFont->sbGlyphs[gptCtx->ptFont->sbCodePoints[c]].xAdvance * (gptCtx->tStyle.fFontSize / gptCtx->ptFont->config.fontSize); }
static int     STB_TEXTEDIT_KEYTOTEXT(int key)                                                    { return key >= 0x200000 ? 0 : key; }
static plWChar STB_TEXTEDIT_NEWLINE = '\n';
static void    STB_TEXTEDIT_LAYOUTROW(StbTexteditRow* r, plInputTextState* obj, int line_start_idx)
{
    const plWChar* text = obj->sbTextW;
    const plWChar* text_remaining = NULL;
    const plVec2 size = pl__input_text_calc_text_size_w(text + line_start_idx, text + obj->iCurrentLengthW, &text_remaining, NULL, true);
    r->x0 = 0.0f;
    r->x1 = size.x;
    r->baseline_y_delta = size.y;
    r->ymin = 0.0f;
    r->ymax = size.y;
    r->num_chars = (int)(text_remaining - (text + line_start_idx));
}

static bool pl__is_separator(unsigned int c)
{
    return c==',' || c==';' || c=='(' || c==')' || c=='{' || c=='}' || c=='[' || c==']' || c=='|' || c=='\n' || c=='\r' || c=='.' || c=='!';
}
static inline bool pl__char_is_blank_w(unsigned int c)  { return c == ' ' || c == '\t' || c == 0x3000; }

static int pl__is_word_boundary_from_right(plInputTextState* obj, int idx)
{
    // When PL_UI_INPUT_TEXT_FLAGS_PASSWORD is set, we don't want actions such as CTRL+Arrow to leak the fact that underlying data are blanks or separators.
    // if ((obj->tFlags & PL_UI_INPUT_TEXT_FLAGS_PASSWORD) || idx <= 0)
    //     return 0;

    bool prev_white = pl__char_is_blank_w(obj->sbTextW[idx - 1]);
    bool prev_separ = pl__is_separator(obj->sbTextW[idx - 1]);
    bool curr_white = pl__char_is_blank_w(obj->sbTextW[idx]);
    bool curr_separ = pl__is_separator(obj->sbTextW[idx]);
    return ((prev_white || prev_separ) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
}
static int pl__is_word_boundary_from_left(plInputTextState* obj, int idx)
{
    // if ((obj->Flags & ImGuiInputTextFlags_Password) || idx <= 0)
    //     return 0;

    bool prev_white = pl__char_is_blank_w(obj->sbTextW[idx]);
    bool prev_separ = pl__is_separator(obj->sbTextW[idx]);
    bool curr_white = pl__char_is_blank_w(obj->sbTextW[idx - 1]);
    bool curr_separ = pl__is_separator(obj->sbTextW[idx - 1]);
    return ((prev_white) && !(curr_separ || curr_white)) || (curr_separ && !prev_separ);
}
static int  STB_TEXTEDIT_MOVEWORDLEFT_IMPL(plInputTextState* obj, int idx)   { idx--; while (idx >= 0 && !pl__is_word_boundary_from_right(obj, idx)) idx--; return idx < 0 ? 0 : idx; }
static int  STB_TEXTEDIT_MOVEWORDRIGHT_MAC(plInputTextState* obj, int idx)   { idx++; int len = obj->iCurrentLengthW; while (idx < len && !pl__is_word_boundary_from_left(obj, idx)) idx++; return idx > len ? len : idx; }
static int  STB_TEXTEDIT_MOVEWORDRIGHT_WIN(plInputTextState* obj, int idx)   { idx++; int len = obj->iCurrentLengthW; while (idx < len && !pl__is_word_boundary_from_right(obj, idx)) idx++; return idx > len ? len : idx; }
// static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(plInputTextState* obj, int idx)  { if (g.IO.ConfigMacOSXBehaviors) return STB_TEXTEDIT_MOVEWORDRIGHT_MAC(obj, idx); else return STB_TEXTEDIT_MOVEWORDRIGHT_WIN(obj, idx); }
static int  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL(plInputTextState* obj, int idx)  { return STB_TEXTEDIT_MOVEWORDRIGHT_WIN(obj, idx); }
#define STB_TEXTEDIT_MOVEWORDLEFT   STB_TEXTEDIT_MOVEWORDLEFT_IMPL  // They need to be #define for stb_textedit.h
#define STB_TEXTEDIT_MOVEWORDRIGHT  STB_TEXTEDIT_MOVEWORDRIGHT_IMPL

static inline int pl__text_count_utf8_bytes_from_char(unsigned int c)
{
    if (c < 0x80) return 1;
    if (c < 0x800) return 2;
    if (c < 0x10000) return 3;
    if (c <= 0x10FFFF) return 4;
    return 3;
}

static int pl__text_count_utf8_bytes_from_str(const plWChar* in_text, const plWChar* in_text_end)
{
    int bytes_count = 0;
    while ((!in_text_end || in_text < in_text_end) && *in_text)
    {
        unsigned int c = (unsigned int)(*in_text++);
        if (c < 0x80)
            bytes_count++;
        else
            bytes_count += pl__text_count_utf8_bytes_from_char(c);
    }
    return bytes_count;
}

static void STB_TEXTEDIT_DELETECHARS(plInputTextState* obj, int pos, int n)
{
    plWChar* dst = obj->sbTextW + pos;

    // We maintain our buffer length in both UTF-8 and wchar formats
    obj->bEdited = true;
    obj->iCurrentLengthA -= pl__text_count_utf8_bytes_from_str(dst, dst + n);
    obj->iCurrentLengthW -= n;

    // Offset remaining text (FIXME-OPT: Use memmove)
    const plWChar* src = obj->sbTextW + pos + n;
    plWChar c = *src++;
    while (c)
    {
        *dst++ = c;
        c = *src++;
    }
    *dst = '\0';
}

static bool STB_TEXTEDIT_INSERTCHARS(plInputTextState* obj, int pos, const plWChar* new_text, int new_text_len)
{
    // const bool is_resizable = (obj->Flags & ImGuiInputTextFlags_CallbackResize) != 0;
    const bool is_resizable = false;
    const int text_len = obj->iCurrentLengthW;
    PL_ASSERT(pos <= text_len);

    const int new_text_len_utf8 = pl__text_count_utf8_bytes_from_str(new_text, new_text + new_text_len);
    if (!is_resizable && (new_text_len_utf8 + obj->iCurrentLengthA + 1 > obj->iBufferCapacityA))
        return false;

    // Grow internal buffer if needed
    if (new_text_len + text_len + 1 > (int)pl_sb_size(obj->sbTextW))
    {
        if (!is_resizable)
            return false;
        PL_ASSERT(text_len < (int)pl_sb_size(obj->sbTextW));
        pl_sb_resize(obj->sbTextW, text_len + pl_clampi(32, new_text_len * 4, pl_max(256, new_text_len)) + 1);
    }

    plWChar* text = obj->sbTextW;
    if (pos != text_len)
        memmove(text + pos + new_text_len, text + pos, (size_t)(text_len - pos) * sizeof(plWChar));
    memcpy(text + pos, new_text, (size_t)new_text_len * sizeof(plWChar));

    obj->bEdited = true;
    obj->iCurrentLengthW += new_text_len;
    obj->iCurrentLengthA += new_text_len_utf8;
    obj->sbTextW[obj->iCurrentLengthW] = '\0';

    return true;
}

// We don't use an enum so we can build even with conflicting symbols (if another user of stb_textedit.h leak their STB_TEXTEDIT_K_* symbols)
#define STB_TEXTEDIT_K_LEFT         0x200000 // keyboard input to move cursor left
#define STB_TEXTEDIT_K_RIGHT        0x200001 // keyboard input to move cursor right
#define STB_TEXTEDIT_K_UP           0x200002 // keyboard input to move cursor up
#define STB_TEXTEDIT_K_DOWN         0x200003 // keyboard input to move cursor down
#define STB_TEXTEDIT_K_LINESTART    0x200004 // keyboard input to move cursor to start of line
#define STB_TEXTEDIT_K_LINEEND      0x200005 // keyboard input to move cursor to end of line
#define STB_TEXTEDIT_K_TEXTSTART    0x200006 // keyboard input to move cursor to start of text
#define STB_TEXTEDIT_K_TEXTEND      0x200007 // keyboard input to move cursor to end of text
#define STB_TEXTEDIT_K_DELETE       0x200008 // keyboard input to delete selection or character under cursor
#define STB_TEXTEDIT_K_BACKSPACE    0x200009 // keyboard input to delete selection or character left of cursor
#define STB_TEXTEDIT_K_UNDO         0x20000A // keyboard input to perform undo
#define STB_TEXTEDIT_K_REDO         0x20000B // keyboard input to perform redo
#define STB_TEXTEDIT_K_WORDLEFT     0x20000C // keyboard input to move cursor left one word
#define STB_TEXTEDIT_K_WORDRIGHT    0x20000D // keyboard input to move cursor right one word
#define STB_TEXTEDIT_K_PGUP         0x20000E // keyboard input to move cursor up a page
#define STB_TEXTEDIT_K_PGDOWN       0x20000F // keyboard input to move cursor down a page
#define STB_TEXTEDIT_K_SHIFT        0x400000

#define STB_TEXTEDIT_IMPLEMENTATION
#include "stb_textedit.h"

// stb_textedit internally allows for a single undo record to do addition and deletion, but somehow, calling
// the stb_textedit_paste() function creates two separate records, so we perform it manually. (FIXME: Report to nothings/stb?)
static void stb_textedit_replace(plInputTextState* str, STB_TexteditState* state, const STB_TEXTEDIT_CHARTYPE* text, int text_len)
{
    stb_text_makeundo_replace(str, state, 0, str->iCurrentLengthW, text_len);
    STB_TEXTEDIT_DELETECHARS(str, 0, str->iCurrentLengthW);
    state->cursor = state->select_start = state->select_end = 0;
    if (text_len <= 0)
        return;
    if (STB_TEXTEDIT_INSERTCHARS(str, 0, text, text_len))
    {
        state->cursor = state->select_start = state->select_end = text_len;
        state->has_preferred_x = 0;
        return;
    }
    PL_ASSERT(0); // Failed to insert character, normally shouldn't happen because of how we currently use stb_textedit_replace()
}

static void
pl__text_state_on_key_press(plInputTextState* ptState, int iKey)
{
    stb_textedit_key(ptState, &ptState->tStb, iKey);
    ptState->bCursorFollow = true;
    pl__text_state_cursor_anim_reset(ptState);
}

// Based on stb_to_utf8() from github.com/nothings/stb/
static inline int
pl__text_char_to_utf8_inline(char* buf, int buf_size, unsigned int c)
{
    if (c < 0x80)
    {
        buf[0] = (char)c;
        return 1;
    }
    if (c < 0x800)
    {
        if (buf_size < 2) return 0;
        buf[0] = (char)(0xc0 + (c >> 6));
        buf[1] = (char)(0x80 + (c & 0x3f));
        return 2;
    }
    if (c < 0x10000)
    {
        if (buf_size < 3) return 0;
        buf[0] = (char)(0xe0 + (c >> 12));
        buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[2] = (char)(0x80 + ((c ) & 0x3f));
        return 3;
    }
    if (c <= 0x10FFFF)
    {
        if (buf_size < 4) return 0;
        buf[0] = (char)(0xf0 + (c >> 18));
        buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
        buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
        buf[3] = (char)(0x80 + ((c ) & 0x3f));
        return 4;
    }
    // Invalid code point, the max unicode is 0x10FFFF
    return 0;
}

static inline plInputTextState*
pl__get_input_text_state(uint32_t id)
{ 
    return (id != 0 && gptCtx->tInputTextState.uId == id) ? &gptCtx->tInputTextState : NULL; 
}

static const plWChar*
pl__str_bol_w(const plWChar* buf_mid_line, const plWChar* buf_begin) // find beginning-of-line
{
    while (buf_mid_line > buf_begin && buf_mid_line[-1] != '\n')
        buf_mid_line--;
    return buf_mid_line;
}

static int
pl__text_str_to_utf8(char* out_buf, int out_buf_size, const plWChar* in_text, const plWChar* in_text_end)
{
    char* buf_p = out_buf;
    const char* buf_end = out_buf + out_buf_size;
    while (buf_p < buf_end - 1 && (!in_text_end || in_text < in_text_end) && *in_text)
    {
        unsigned int c = (unsigned int)(*in_text++);
        if (c < 0x80)
            *buf_p++ = (char)c;
        else
            buf_p += pl__text_char_to_utf8_inline(buf_p, (int)(buf_end - buf_p - 1), c);
    }
    *buf_p = 0;
    return (int)(buf_p - out_buf);
}

static int
pl__text_str_from_utf8(plWChar* buf, int buf_size, const char* in_text, const char* in_text_end, const char** in_text_remaining)
{
    plWChar* buf_out = buf;
    plWChar* buf_end = buf + buf_size;
    while (buf_out < buf_end - 1 && (!in_text_end || in_text < in_text_end) && *in_text)
    {
        unsigned int c;
        in_text += pl_text_char_from_utf8(&c, in_text, in_text_end);
        *buf_out++ = (plWChar)c;
    }
    *buf_out = 0;
    if (in_text_remaining)
        *in_text_remaining = in_text;
    return (int)(buf_out - buf);
}

static int
pl__text_count_chars_from_utf8(const char* in_text, const char* in_text_end)
{
    int char_count = 0;
    while ((!in_text_end || in_text < in_text_end) && *in_text)
    {
        unsigned int c;
        in_text += pl_text_char_from_utf8(&c, in_text, in_text_end);
        char_count++;
    }
    return char_count;
}

static int
pl__input_text_calc_text_len_and_line_count(const char* text_begin, const char** out_text_end)
{
    int line_count = 0;
    const char* s = text_begin;
    char c = *s++;
    while (c) // We are only matching for \n so we can ignore UTF-8 decoding
    {
        if (c == '\n')
            line_count++;
        c = *s++;
    }
    s--;
    if (s[0] != '\n' && s[0] != '\r')
        line_count++;
    *out_text_end = s;
    return line_count;
}

//-----------------------------------------------------------------------------
// [SECTION] context
//-----------------------------------------------------------------------------

plUiContext* gptCtx = NULL;

//-----------------------------------------------------------------------------
// [SECTION] global data
//-----------------------------------------------------------------------------

static plDrawApiI* gptDraw = NULL;

//-----------------------------------------------------------------------------
// [SECTION] public api implementations
//-----------------------------------------------------------------------------

plUiApiI*
pl_load_ui_api(void)
{
    static plUiApiI tApi0 = {
        .create_context                = pl_ui_create_context,
        .destroy_context               = pl_ui_destroy_context,
        .set_context                   = pl_ui_set_context,
        .get_context                   = pl_ui_get_context,
        .get_draw_list                 = pl_ui_get_draw_list,
        .get_debug_draw_list           = pl_ui_get_debug_draw_list,
        .new_frame                     = pl_ui_new_frame,
        .end_frame                     = pl_ui_end_frame,
        .render                        = pl_ui_render,
        .debug                         = pl_ui_debug,
        .style                         = pl_ui_style,
        .demo                          = pl_ui_demo,
        .set_dark_theme                = pl_ui_set_dark_theme,
        .set_default_font              = pl_ui_set_default_font,
        .get_default_font              = pl_ui_get_default_font,
        .begin_window                  = pl_ui_begin_window,
        .end_window                    = pl_ui_end_window,
        .get_window_fg_drawlayer       = pl_ui_get_window_fg_drawlayer,
        .get_window_bg_drawlayer       = pl_ui_get_window_bg_drawlayer,
        .get_cursor_pos                = pl_ui_get_cursor_pos,
        .begin_child                   = pl_ui_begin_child,
        .end_child                     = pl_ui_end_child,
        .begin_tooltip                 = pl_ui_begin_tooltip,
        .end_tooltip                   = pl_ui_end_tooltip,
        .get_window_pos                = pl_ui_get_window_pos,
        .get_window_size               = pl_ui_get_window_size,
        .get_window_scroll             = pl_ui_get_window_scroll,
        .get_window_scroll_max         = pl_ui_get_window_scroll_max,
        .set_window_scroll             = pl_ui_set_window_scroll,
        .set_next_window_pos           = pl_ui_set_next_window_pos,
        .set_next_window_size          = pl_ui_set_next_window_size,
        .set_next_window_collapse      = pl_ui_set_next_window_collapse,
        .button                        = pl_ui_button,
        .selectable                    = pl_ui_selectable,
        .checkbox                      = pl_ui_checkbox,
        .radio_button                  = pl_ui_radio_button,
        .image                         = pl_ui_image,
        .image_ex                      = pl_ui_image_ex,
        .invisible_button              = pl_ui_invisible_button,
        .dummy                         = pl_ui_dummy,
        .progress_bar                  = pl_ui_progress_bar,
        .text                          = pl_ui_text,
        .text_v                        = pl_ui_text_v,
        .color_text                    = pl_ui_color_text,
        .color_text_v                  = pl_ui_color_text_v,
        .labeled_text                  = pl_ui_labeled_text,
        .labeled_text_v                = pl_ui_labeled_text_v,
        .input_text                    = pl_ui_input_text,
        .input_text_hint               = pl_ui_input_text_hint,
        .input_float                   = pl_ui_input_float,
        .input_int                     = pl_ui_input_int,
        .slider_float                  = pl_ui_slider_float,
        .slider_float_f                = pl_ui_slider_float_f,
        .slider_int                    = pl_ui_slider_int,
        .slider_int_f                  = pl_ui_slider_int_f,
        .drag_float                    = pl_ui_drag_float,
        .drag_float_f                  = pl_ui_drag_float_f,
        .collapsing_header             = pl_ui_collapsing_header,
        .end_collapsing_header         = pl_ui_end_collapsing_header,
        .tree_node                     = pl_ui_tree_node,
        .tree_node_f                   = pl_ui_tree_node_f,
        .tree_node_v                   = pl_ui_tree_node_v,
        .tree_pop                      = pl_ui_tree_pop,
        .begin_tab_bar                 = pl_ui_begin_tab_bar,
        .end_tab_bar                   = pl_ui_end_tab_bar,
        .begin_tab                     = pl_ui_begin_tab,
        .end_tab                       = pl_ui_end_tab,
        .separator                     = pl_ui_separator,
        .vertical_spacing              = pl_ui_vertical_spacing,
        .indent                        = pl_ui_indent,
        .unindent                      = pl_ui_unindent,
        .step_clipper                  = pl_ui_step_clipper,
        .layout_dynamic                = pl_ui_layout_dynamic,
        .layout_static                 = pl_ui_layout_static,
        .layout_row_begin              = pl_ui_layout_row_begin,
        .layout_row_push               = pl_ui_layout_row_push,
        .layout_row_end                = pl_ui_layout_row_end,
        .layout_row                    = pl_ui_layout_row,
        .layout_template_begin         = pl_ui_layout_template_begin,
        .layout_template_push_dynamic  = pl_ui_layout_template_push_dynamic,
        .layout_template_push_variable = pl_ui_layout_template_push_variable,
        .layout_template_push_static   = pl_ui_layout_template_push_static,
        .layout_template_end           = pl_ui_layout_template_end,
        .layout_space_begin            = pl_ui_layout_space_begin,
        .layout_space_push             = pl_ui_layout_space_push,
        .layout_space_end              = pl_ui_layout_space_end,
        .was_last_item_hovered         = pl_ui_was_last_item_hovered,
        .was_last_item_active          = pl_ui_was_last_item_active
    };

    return &tApi0;
}

//-----------------------------------------------------------------------------
// [SECTION] internal api implementations
//-----------------------------------------------------------------------------

plUiContext*
pl_ui_create_context(void)
{
    gptCtx = PL_ALLOC(sizeof(plUiContext));
    memset(gptCtx, 0, sizeof(plUiContext));
    gptCtx->ptDrawlist = PL_ALLOC(sizeof(plDrawList));
    gptCtx->ptDebugDrawlist = PL_ALLOC(sizeof(plDrawList));
    memset(gptCtx->ptDrawlist, 0, sizeof(plDrawList));
    memset(gptCtx->ptDebugDrawlist, 0, sizeof(plDrawList));
    gptDraw->register_drawlist(gptDraw->get_context(), gptCtx->ptDrawlist);
    gptDraw->register_drawlist(gptDraw->get_context(), gptCtx->ptDebugDrawlist);
    gptCtx->ptBgLayer = gptDraw->request_layer(gptCtx->ptDrawlist, "plui Background");
    gptCtx->ptFgLayer = gptDraw->request_layer(gptCtx->ptDrawlist, "plui Foreground");
    gptCtx->ptDebugLayer = gptDraw->request_layer(gptCtx->ptDebugDrawlist, "ui debug");
    gptCtx->tTooltipWindow.ptBgLayer = gptDraw->request_layer(gptCtx->ptDrawlist, "plui Tooltip Background");
    gptCtx->tTooltipWindow.ptFgLayer = gptDraw->request_layer(gptCtx->ptDrawlist, "plui Tooltip Foreground");
    pl_ui_set_dark_theme();
    return gptCtx;
}

void
pl_ui_destroy_context(plUiContext* ptContext)
{
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        pl_sb_free(gptCtx->sbptWindows[i]->tStorage.sbtData);
        pl_sb_free(gptCtx->sbptWindows[i]->sbuTempLayoutIndexSort);
        pl_sb_free(gptCtx->sbptWindows[i]->sbtTempLayoutSort);
        pl_sb_free(gptCtx->sbptWindows[i]->sbtRowStack);
        pl_sb_free(gptCtx->sbptWindows[i]->sbtChildWindows);
        pl_sb_free(gptCtx->sbptWindows[i]->sbtRowTemplateEntries);
        PL_FREE(gptCtx->sbptWindows[i]);
    }
    PL_FREE(gptCtx->ptDrawlist);
    PL_FREE(gptCtx->ptDebugDrawlist);
    pl_sb_free(gptCtx->tWindows.sbtData);
    pl_sb_free(gptCtx->sbptWindows);
    pl_sb_free(gptCtx->sbtTabBars);
    pl_sb_free(gptCtx->sbptFocusedWindows);
    pl_sb_free(gptCtx->sbuIdStack);
    PL_FREE(gptCtx);
    gptCtx = NULL;
}

void
pl_ui_set_context(plUiContext* ptCtx)
{
    gptCtx = ptCtx;
}

plUiContext*
pl_ui_get_context(void)
{
    return gptCtx;
}

plDrawList*
pl_ui_get_draw_list(plUiContext* ptContext)
{
    if(ptContext == NULL)
        ptContext = gptCtx;
    return ptContext->ptDrawlist;
}

plDrawList*
pl_ui_get_debug_draw_list(plUiContext* ptContext)
{
    if(ptContext == NULL)
        ptContext = gptCtx;
    return ptContext->ptDebugDrawlist;
}

void
pl_ui_new_frame(void)
{
    plIOContext* ptIo = pl_get_io_context();

    // track click ownership
    for(uint32_t i = 0; i < 5; i++)
    {
        if(ptIo->_abMouseClicked[i])
            ptIo->_abMouseOwned[i] = (gptCtx->ptHoveredWindow != NULL);
    }

    pl_new_io_frame();
    gptDraw->new_frame(gptDraw->get_context());

    if(pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT))
        gptCtx->uNextActiveId = gptCtx->uActiveId;
}

void
pl_ui_end_frame(void)
{

    const plVec2 tMousePos = pl_get_mouse_pos();

    // update state id's from previous frame
    gptCtx->uHoveredId = gptCtx->uNextHoveredId;
    gptCtx->uActiveId = gptCtx->uNextActiveId;
    pl_get_io_context()->bWantCaptureKeyboard = gptCtx->bWantCaptureKeyboardNextFrame;
    pl_get_io_context()->bWantCaptureMouse = gptCtx->bWantCaptureMouseNextFrame || gptCtx->uActiveId != 0 || gptCtx->ptMovingWindow != NULL;

    // null starting state
    gptCtx->bActiveIdJustActivated = false;
    gptCtx->bWantCaptureMouseNextFrame = false;
    gptCtx->ptHoveredWindow = NULL;
    gptCtx->ptActiveWindow = NULL;
    gptCtx->ptWheelingWindow = NULL;
    gptCtx->uNextHoveredId = 0u;
    gptCtx->uNextActiveId = 0u;
    gptCtx->tPrevItemData.bHovered = false;
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;
    gptCtx->tNextWindowData.tCollapseCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tPosCondition = PL_UI_COND_NONE;
    gptCtx->tNextWindowData.tSizeCondition = PL_UI_COND_NONE;

    if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
    {
        gptCtx->bWantCaptureMouseNextFrame = false;
        gptCtx->ptMovingWindow = NULL;
        gptCtx->ptSizingWindow = NULL;
        gptCtx->ptScrollingWindow = NULL;
        gptCtx->ptActiveWindow = NULL;
    }

    // reset active window
    if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        gptCtx->uActiveWindowId = 0;

    // submit windows in display order
    pl_sb_reset(gptCtx->sbptWindows);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptFocusedWindows); i++)
    {
        plUiWindow* ptRootWindow = gptCtx->sbptFocusedWindows[i];

        if(ptRootWindow->bActive)
            pl_ui_submit_window(ptRootWindow);

        // adjust window size if outside viewport
        if(ptRootWindow->tPos.x > pl_get_io_context()->afMainViewportSize[0])
            ptRootWindow->tPos.x = pl_get_io_context()->afMainViewportSize[0] - ptRootWindow->tSize.x / 2.0f;

        if(ptRootWindow->tPos.y > pl_get_io_context()->afMainViewportSize[1])
        {
            ptRootWindow->tPos.y = pl_get_io_context()->afMainViewportSize[1] - ptRootWindow->tSize.y / 2.0f;
            ptRootWindow->tPos.y = pl_maxf(ptRootWindow->tPos.y, 0.0f);
        }
    }

    // move newly activated window to front of focus order
    if(gptCtx->bActiveIdJustActivated)
    {
        pl_sb_top(gptCtx->sbptFocusedWindows)->uFocusOrder = gptCtx->ptActiveWindow->uFocusOrder;
        gptCtx->ptActiveWindow->uFocusOrder = pl_sb_size(gptCtx->sbptFocusedWindows) - 1;
        pl_sb_del_swap(gptCtx->sbptFocusedWindows, pl_sb_top(gptCtx->sbptFocusedWindows)->uFocusOrder);
        pl_sb_push(gptCtx->sbptFocusedWindows, gptCtx->ptActiveWindow);
    }

    // scrolling window
    if(gptCtx->ptWheelingWindow)
    {
        gptCtx->ptWheelingWindow->tScroll.y -= pl_get_mouse_wheel() * 10.0f;
        gptCtx->ptWheelingWindow->tScroll.y = pl_clampf(0.0f, gptCtx->ptWheelingWindow->tScroll.y, gptCtx->ptWheelingWindow->tScrollMax.y);
    }

    // moving window
    if(gptCtx->ptMovingWindow && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {

        if(tMousePos.x > 0.0f && tMousePos.x < pl_get_io_context()->afMainViewportSize[0])
            gptCtx->ptMovingWindow->tPos.x = gptCtx->ptMovingWindow->tPos.x + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).x;

        if(tMousePos.y > 0.0f && tMousePos.y < pl_get_io_context()->afMainViewportSize[1])
            gptCtx->ptMovingWindow->tPos.y = gptCtx->ptMovingWindow->tPos.y + pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 2.0f).y;  

        // clamp x
        gptCtx->ptMovingWindow->tPos.x = pl_maxf(gptCtx->ptMovingWindow->tPos.x, -gptCtx->ptMovingWindow->tSize.x / 2.0f);   
        gptCtx->ptMovingWindow->tPos.x = pl_minf(gptCtx->ptMovingWindow->tPos.x, pl_get_io_context()->afMainViewportSize[0] - gptCtx->ptMovingWindow->tSize.x / 2.0f);

        // clamp y
        gptCtx->ptMovingWindow->tPos.y = pl_maxf(gptCtx->ptMovingWindow->tPos.y, 0.0f);   
        gptCtx->ptMovingWindow->tPos.y = pl_minf(gptCtx->ptMovingWindow->tPos.y, pl_get_io_context()->afMainViewportSize[1] - 50.0f);

        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    pl_end_io_frame(); 
}

void
pl_ui_render(void)
{
    gptDraw->submit_layer(gptCtx->ptBgLayer);
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbptWindows); i++)
    {
        if(gptCtx->sbptWindows[i]->uHideFrames == 0)
        {
            gptDraw->submit_layer(gptCtx->sbptWindows[i]->ptBgLayer);
            gptDraw->submit_layer(gptCtx->sbptWindows[i]->ptFgLayer);
        }
        else
        {
            gptCtx->sbptWindows[i]->uHideFrames--;
        }
    }
    gptDraw->submit_layer(gptCtx->tTooltipWindow.ptBgLayer);
    gptDraw->submit_layer(gptCtx->tTooltipWindow.ptFgLayer);
    gptDraw->submit_layer(gptCtx->ptFgLayer);
    gptDraw->submit_layer(gptCtx->ptDebugLayer);

    pl_ui_end_frame();
}

void
pl_ui_set_dark_theme(void)
{
    // styles
    gptCtx->tStyle.fTitlePadding            = 10.0f;
    gptCtx->tStyle.fFontSize                = 13.0f;
    gptCtx->tStyle.fWindowHorizontalPadding = 5.0f;
    gptCtx->tStyle.fWindowVerticalPadding   = 5.0f;
    gptCtx->tStyle.fIndentSize              = 15.0f;
    gptCtx->tStyle.fScrollbarSize           = 10.0f;
    gptCtx->tStyle.fSliderSize              = 12.0f;
    gptCtx->tStyle.tItemSpacing  = (plVec2){8.0f, 4.0f};
    gptCtx->tStyle.tInnerSpacing = (plVec2){4.0f, 4.0f};
    gptCtx->tStyle.tFramePadding = (plVec2){4.0f, 4.0f};

    // colors
    gptCtx->tColorScheme.tTitleActiveCol      = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCol          = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tTitleBgCollapsedCol = (plVec4){0.04f, 0.04f, 0.04f, 1.00f};
    gptCtx->tColorScheme.tWindowBgColor       = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tWindowBorderColor   = (plVec4){0.33f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tChildBgColor        = (plVec4){0.10f, 0.10f, 0.10f, 0.78f};
    gptCtx->tColorScheme.tButtonCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonHoveredCol    = (plVec4){0.61f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tButtonActiveCol     = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tTextCol             = (plVec4){1.00f, 1.00f, 1.00f, 1.00f};
    gptCtx->tColorScheme.tProgressBarCol      = (plVec4){0.90f, 0.70f, 0.00f, 1.00f};
    gptCtx->tColorScheme.tCheckmarkCol        = (plVec4){0.87f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgCol          = (plVec4){0.23f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tFrameBgHoveredCol   = (plVec4){0.26f, 0.59f, 0.98f, 0.40f};
    gptCtx->tColorScheme.tFrameBgActiveCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.67f};
    gptCtx->tColorScheme.tHeaderCol           = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tHeaderHoveredCol    = (plVec4){0.26f, 0.59f, 0.98f, 0.80f};
    gptCtx->tColorScheme.tHeaderActiveCol     = (plVec4){0.26f, 0.59f, 0.98f, 1.00f};
    gptCtx->tColorScheme.tScrollbarBgCol      = (plVec4){0.05f, 0.05f, 0.05f, 0.85f};
    gptCtx->tColorScheme.tScrollbarHandleCol  = (plVec4){0.51f, 0.02f, 0.10f, 1.00f};
    gptCtx->tColorScheme.tScrollbarFrameCol   = (plVec4){0.00f, 0.00f, 0.00f, 0.00f};
    gptCtx->tColorScheme.tScrollbarActiveCol  = gptCtx->tColorScheme.tButtonActiveCol;
    gptCtx->tColorScheme.tScrollbarHoveredCol = gptCtx->tColorScheme.tButtonHoveredCol;
}

void
pl_ui_set_default_font(plFont* ptFont)
{
    gptCtx->ptFont = ptFont;
}

plFont*
pl_ui_get_default_font(void)
{
    return gptCtx->ptFont;
}

bool
pl_ui_begin_window(const char* pcName, bool* pbOpen, bool bAutoSize)
{
    bool bResult = pl_ui_begin_window_ex(pcName, pbOpen, bAutoSize ? PL_UI_WINDOW_FLAGS_AUTO_SIZE : PL_UI_WINDOW_FLAGS_NONE);

    static const float pfRatios[] = {300.0f};
    if(bResult)
    {
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
        pl_ui_end_window();
    return bResult;
}

void
pl_ui_end_window(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        ptWindow->tContentSize = pl_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX)
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;

    if(ptWindow->bScrollbarY)
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;

    const bool bScrollBarsPresent = ptWindow->bScrollbarX || ptWindow->bScrollbarY;

    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR)
        fTitleBarHeight = 0.0f;

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // autosized non collapsed
    if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE && !ptWindow->bCollapsed)
    {

        const plRect tBgRect = pl_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        // ensure window doesn't get too small
        ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding * 2.0f;
        ptWindow->tSize.y = fTitleBarHeight + ptWindow->tContentSize.y + gptCtx->tStyle.fWindowVerticalPadding;
        
        // clamp window size to min/max
        ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

        ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
        ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
        
        // remove scissor rect
        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

        // draw background
        gptDraw->add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

        ptWindow->tFullSize = ptWindow->tSize;
    }

    // regular window non collapsed
    else if(!ptWindow->bCollapsed)
    {
        plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

        plRect tBgRect = pl_calculate_rect(
            (plVec2){ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight},
            (plVec2){ptWindow->tSize.x, ptWindow->tSize.y - fTitleBarHeight});

        plRect tParentBgRect = ptParentWindow->tOuterRect;

        if(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
        {
            tBgRect = pl_rect_clip(&tBgRect, &tParentBgRect);
        }

        gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

        const uint32_t uResizeHash = ptWindow->uId + 1;
        const uint32_t uWestResizeHash = uResizeHash + 1;
        const uint32_t uEastResizeHash = uResizeHash + 2;
        const uint32_t uNorthResizeHash = uResizeHash + 3;
        const uint32_t uSouthResizeHash = uResizeHash + 4;
        const uint32_t uVerticalScrollHash = uResizeHash + 5;
        const uint32_t uHorizonatalScrollHash = uResizeHash + 6;
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fHoverPadding = 4.0f;

        // draw background
        gptDraw->add_rect_filled(ptWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

        // vertical scroll bar
        if(ptWindow->bScrollbarY)
            pl_ui_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

        // horizontal scroll bar
        if(ptWindow->bScrollbarX)
            pl_ui_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

        const plVec2 tTopLeft = pl_rect_top_left(&ptWindow->tOuterRect);
        const plVec2 tBottomLeft = pl_rect_bottom_left(&ptWindow->tOuterRect);
        const plVec2 tTopRight = pl_rect_top_right(&ptWindow->tOuterRect);
        const plVec2 tBottomRight = pl_rect_bottom_right(&ptWindow->tOuterRect);

        // resizing grip
        {
            const plVec2 tCornerTopLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, -15.0f});
            const plVec2 tCornerTopPos = pl_add_vec2(tBottomRight, (plVec2){0.0f, -15.0f});
            const plVec2 tCornerLeftPos = pl_add_vec2(tBottomRight, (plVec2){-15.0f, 0.0f});

            const plRect tBoundingBox = pl_calculate_rect(tCornerTopLeftPos, (plVec2){15.0f, 15.0f});
            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uResizeHash)
            {
                gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.99f, 0.02f, 0.10f, 1.0f});
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
            }
            else if(gptCtx->uHoveredId == uResizeHash)
            {
                gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.66f, 0.02f, 0.10f, 1.0f});
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NWSE);
            }
            else
            {
                gptDraw->add_triangle_filled(ptWindow->ptFgLayer, tBottomRight, tCornerTopPos, tCornerLeftPos, (plVec4){0.33f, 0.02f, 0.10f, 1.0f});   
            }
        }

        // east border
        {

            plRect tBoundingBox = pl_calculate_rect(tTopRight, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uEastResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uEastResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
            else if(gptCtx->uHoveredId == uEastResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopRight, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
        }

        // west border
        {
            plRect tBoundingBox = pl_calculate_rect(tTopLeft, (plVec2){0.0f, ptWindow->tSize.y - 15.0f});
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){fHoverPadding / 2.0f, 0.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uWestResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uWestResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
            else if(gptCtx->uHoveredId == uWestResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tBottomLeft, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_EW);
            }
        }

        // north border
        {
            plRect tBoundingBox = {tTopLeft, (plVec2){tTopRight.x - 15.0f, tTopRight.y}};
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uNorthResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uNorthResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
            else if(gptCtx->uHoveredId == uNorthResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tTopLeft, tTopRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
        }

        // south border
        {
            plRect tBoundingBox = {tBottomLeft, (plVec2){tBottomRight.x - 15.0f, tBottomRight.y}};
            tBoundingBox = pl_rect_expand_vec2(&tBoundingBox, (plVec2){0.0f, fHoverPadding / 2.0f});

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uSouthResizeHash, &bHovered, &bHeld);

            if(gptCtx->uActiveId == uSouthResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.99f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
            else if(gptCtx->uHoveredId == uSouthResizeHash)
            {
                gptDraw->add_line(ptWindow->ptFgLayer, tBottomLeft, tBottomRight, (plVec4){0.66f, 0.02f, 0.10f, 1.0f}, 2.0f);
                pl_set_mouse_cursor(PL_MOUSE_CURSOR_RESIZE_NS);
            }
        }

        // draw border
        gptDraw->add_rect(ptWindow->ptFgLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, gptCtx->tColorScheme.tWindowBorderColor, 1.0f);

        // handle corner resizing
        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
        {
            const plVec2 tMousePos = pl_get_mouse_pos();

            if(gptCtx->uActiveId == uResizeHash)
            {  
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize = pl_sub_vec2(tMousePos, ptWindow->tPos);
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle east resizing
            else if(gptCtx->uActiveId == uEastResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tMousePos.x - ptWindow->tPos.x;
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle west resizing
            else if(gptCtx->uActiveId == uWestResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.x = tTopRight.x - tMousePos.x;  
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.x = tTopRight.x - ptWindow->tSize.x;
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle north resizing
            else if(gptCtx->uActiveId == uNorthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tBottomRight.y - tMousePos.y;  
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tPos.y = tBottomRight.y - ptWindow->tSize.y;
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle south resizing
            else if(gptCtx->uActiveId == uSouthResizeHash)
            {
                gptCtx->ptSizingWindow = ptWindow;
                ptWindow->tSize.y = tMousePos.y - ptWindow->tPos.y;
                ptWindow->tSize = pl_max_vec2(ptWindow->tSize, ptWindow->tMinSize);
                ptWindow->tScroll = pl_clamp_vec2((plVec2){0}, ptWindow->tScroll, ptWindow->tScrollMax);
            }

            // handle vertical scrolling with scroll bar
            else if(gptCtx->uActiveId == uVerticalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.y > ptWindow->tPos.y && tMousePos.y < ptWindow->tPos.y + ptWindow->tSize.y)
                {
                    const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
                    ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
                    ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }

            // handle horizontal scrolling with scroll bar
            else if(gptCtx->uActiveId == uHorizonatalScrollHash)
            {
                gptCtx->ptScrollingWindow = ptWindow;

                if(tMousePos.x > ptWindow->tPos.x && tMousePos.x < ptWindow->tPos.x + ptWindow->tSize.x)
                {
                    const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
                    ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
                    ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
                    pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
                }
            }
        }
        gptCtx->ptCurrentWindow->tFullSize = ptWindow->tSize;
    }

    gptCtx->ptCurrentWindow = NULL;
    pl_sb_pop(gptCtx->sbuIdStack);
}

plDrawLayer*
pl_ui_get_window_fg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptFgLayer;
}

plDrawLayer*
pl_ui_get_window_bg_drawlayer(void)
{
    PL_ASSERT(gptCtx->ptCurrentWindow && "no current window");
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->ptBgLayer; 
}

plVec2
pl_ui_get_cursor_pos(void)
{
    return pl__ui_get_cursor_pos();
}

bool
pl_ui_begin_child(const char* pcName)
{
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(200.0f);

    const plUiWindowFlags tFlags = 
        PL_UI_WINDOW_FLAGS_CHILD_WINDOW |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    pl_ui_set_next_window_size(tWidgetSize, PL_UI_COND_ALWAYS);
    bool bValue =  pl_ui_begin_window_ex(pcName, NULL, tFlags);

    static const float pfRatios[] = {300.0f};
    if(bValue)
    {
        plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
        ptWindow->tMinSize = pl_min_vec2(ptWindow->tMinSize, tWidgetSize);
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
    }
    else
    {
        pl_ui_end_child();
    }
    return bValue;
}

void
pl_ui_end_child(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiWindow* ptParentWindow = ptWindow->ptParentWindow;

    // set content sized based on last frames maximum cursor position
    if(ptWindow->bVisible)
    {
        ptWindow->tContentSize = pl_add_vec2(
            (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding}, 
            pl_sub_vec2(ptWindow->tTempData.tCursorMaxPos, ptWindow->tTempData.tCursorStartPos)
        
        );
    }
    ptWindow->tScrollMax = pl_sub_vec2(ptWindow->tContentSize, ptWindow->tSize);
    
    // clamp scrolling max
    ptWindow->tScrollMax = pl_max_vec2(ptWindow->tScrollMax, (plVec2){0});
    ptWindow->bScrollbarX = ptWindow->tScrollMax.x > 0.0f;
    ptWindow->bScrollbarY = ptWindow->tScrollMax.y > 0.0f;

    if(ptWindow->bScrollbarX && ptWindow->bScrollbarY)
    {
        ptWindow->tScrollMax.y += gptCtx->tStyle.fScrollbarSize + 2.0f;
        ptWindow->tScrollMax.x += gptCtx->tStyle.fScrollbarSize + 2.0f;
    }

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    plRect tParentBgRect = ptParentWindow->tOuterRect;
    const plRect tBgRect = pl_rect_clip(&ptWindow->tOuterRect, &tParentBgRect);

    gptDraw->pop_clip_rect(gptCtx->ptDrawlist);

    const uint32_t uVerticalScrollHash = pl_str_hash("##scrollright", 0, pl_sb_top(gptCtx->sbuIdStack));
    const uint32_t uHorizonatalScrollHash = pl_str_hash("##scrollbottom", 0, pl_sb_top(gptCtx->sbuIdStack));

    // draw background
    gptDraw->add_rect_filled(ptParentWindow->ptBgLayer, tBgRect.tMin, tBgRect.tMax, gptCtx->tColorScheme.tWindowBgColor);

    // vertical scroll bar
    if(ptWindow->bScrollbarY)
        pl_ui_render_scrollbar(ptWindow, uVerticalScrollHash, PL_UI_AXIS_Y);

    // horizontal scroll bar
    if(ptWindow->bScrollbarX)
        pl_ui_render_scrollbar(ptWindow, uHorizonatalScrollHash, PL_UI_AXIS_X);

    // handle vertical scrolling with scroll bar
    if(gptCtx->uActiveId == uVerticalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.y / ptWindow->tSize.y);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uVerticalScrollHash;
        ptWindow->tScroll.y += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).y * fScrollConversion;
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    // handle horizontal scrolling with scroll bar
    else if(gptCtx->uActiveId == uHorizonatalScrollHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 2.0f))
    {
        const float fScrollConversion = roundf(ptWindow->tContentSize.x / ptWindow->tSize.x);
        gptCtx->ptScrollingWindow = ptWindow;
        gptCtx->uNextHoveredId = uHorizonatalScrollHash;
        ptWindow->tScroll.x += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fScrollConversion;
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }

    ptWindow->tFullSize = ptWindow->tSize;
    pl_sb_pop(gptCtx->sbuIdStack);
    gptCtx->ptCurrentWindow = ptParentWindow;

    pl_ui_advance_cursor(ptWindow->tSize.x, ptWindow->tSize.y);
}

void
pl_ui_begin_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tContentSize = pl_add_vec2(
        (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, gptCtx->tStyle.fWindowVerticalPadding},
        pl_sub_vec2(
            ptWindow->tTempData.tCursorMaxPos, 
            ptWindow->tTempData.tCursorStartPos)
    );

    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));

    ptWindow->tFlags |= 
        PL_UI_WINDOW_FLAGS_TOOLTIP |
        PL_UI_WINDOW_FLAGS_NO_TITLE_BAR | 
        PL_UI_WINDOW_FLAGS_NO_RESIZE | 
        PL_UI_WINDOW_FLAGS_NO_COLLAPSE | 
        PL_UI_WINDOW_FLAGS_AUTO_SIZE | 
        PL_UI_WINDOW_FLAGS_NO_MOVE;

    // place window at mouse position
    const plVec2 tMousePos = pl_get_mouse_pos();
    ptWindow->tTempData.tCursorStartPos = pl_add_vec2(tMousePos, (plVec2){gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});
    ptWindow->tPos = tMousePos;
    ptWindow->tTempData.tRowPos.x = floorf(gptCtx->tStyle.fWindowHorizontalPadding + tMousePos.x);
    ptWindow->tTempData.tRowPos.y = floorf(gptCtx->tStyle.fWindowVerticalPadding + tMousePos.y);

    const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y };
    const plVec2 tEndClip = { ptWindow->tSize.x, ptWindow->tSize.y };
    gptDraw->push_clip_rect(gptCtx->ptDrawlist, pl_calculate_rect(tStartClip, tEndClip), false);

    ptWindow->ptParentWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentWindow = ptWindow;

    static const float pfRatios[] = {300.0f};
    pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios);
}

void
pl_ui_end_tooltip(void)
{
    plUiWindow* ptWindow = &gptCtx->tTooltipWindow;

    ptWindow->tSize.x = ptWindow->tContentSize.x + gptCtx->tStyle.fWindowHorizontalPadding;
    ptWindow->tSize.y = ptWindow->tContentSize.y;

    gptDraw->add_rect_filled(ptWindow->ptBgLayer,
        ptWindow->tPos, 
        pl_add_vec2(ptWindow->tPos, ptWindow->tSize), gptCtx->tColorScheme.tWindowBgColor);

    gptDraw->pop_clip_rect(gptCtx->ptDrawlist);
    gptCtx->ptCurrentWindow = ptWindow->ptParentWindow;
}

plVec2
pl_ui_get_window_pos(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tPos;
}

plVec2
pl_ui_get_window_size(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tSize;
}

plVec2
pl_ui_get_window_scroll(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScroll;
}

plVec2
pl_ui_get_window_scroll_max(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    return ptWindow->tScrollMax;
}

void
pl_ui_set_window_scroll(plVec2 tScroll)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    if(ptWindow->tScrollMax.x >= tScroll.x)
        ptWindow->tScroll.x = tScroll.x;

    if(ptWindow->tScrollMax.y >= tScroll.y)
        ptWindow->tScroll.y = tScroll.y;
}

void
pl_ui_set_next_window_pos(plVec2 tPos, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tPos = tPos;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS;
    gptCtx->tNextWindowData.tPosCondition = tCondition;
}

void
pl_ui_set_next_window_size(plVec2 tSize, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.tSize = tSize;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE;
    gptCtx->tNextWindowData.tSizeCondition = tCondition;
}

void
pl_ui_set_next_window_collapse(bool bCollapsed, plUiConditionFlags tCondition)
{
    gptCtx->tNextWindowData.bCollapsed = bCollapsed;
    gptCtx->tNextWindowData.tFlags |= PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED;
    gptCtx->tNextWindowData.tCollapseCondition = tCondition;    
}

bool
pl_ui_button(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    bool bPressed = false;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
        const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);

        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonCol);

        const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        plVec2 tTextStartPos = {
            .x = tStartPos.x,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        if((pl_rect_width(&tBoundingBox) < tTextSize.x)) // clipping, so start at beginning of widget
            tTextStartPos.x += gptCtx->tStyle.tFramePadding.x;
        else // not clipping, so center on widget
            tTextStartPos.x += tStartPos.x + tWidgetSize.x / 2.0f - tTextActualCenter.x;

        pl_ui_add_clipped_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, tBoundingBox.tMin, tBoundingBox.tMax, gptCtx->tColorScheme.tTextCol, pcText, 0.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_selectable(const char* pcText, bool* bpValue)
{

    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    bool bPressed = false;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));

        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        const plVec2 tTextStartPos = {
            .x = tStartPos.x + gptCtx->tStyle.tFramePadding.x,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        const plVec2 tEndPos = pl_add_vec2(tStartPos, tWidgetSize);

        const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(bPressed)
            *bpValue = !*bpValue;

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tColorScheme.tHeaderActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tColorScheme.tHeaderHoveredCol);

        if(*bpValue)
            gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tEndPos, gptCtx->tColorScheme.tHeaderCol);

        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed; 
}

bool
pl_ui_checkbox(const char* pcText, bool* bpValue)
{
    // temporary hack
    static bool bDummyState = true;
    if(bpValue == NULL) bpValue = &bDummyState;

    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    bool bPressed = false;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const bool bOriginalValue = *bpValue;
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);


        const plVec2 tTextStartPos = {
            .x = tStartPos.x + tWidgetSize.y + gptCtx->tStyle.tInnerSpacing.x,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        const plRect tBoundingBox = pl_calculate_rect(tStartPos, (plVec2){tWidgetSize.y, tWidgetSize.y});
        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(bPressed)
            *bpValue = !bOriginalValue;

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgCol);

        if(*bpValue)
            gptDraw->add_line(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tCheckmarkCol, 2.0f);

        // add label
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f); 
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_radio_button(const char* pcText, int* piValue, int iButtonValue)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    bool bPressed = false;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
        const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        const plVec2 tSize = {tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + tWidgetSize.y, tWidgetSize.y};
        // tSize = pl_floor_vec2(tSize);

        const plVec2 tTextStartPos = {
            .x = tStartPos.x + tWidgetSize.y + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        plRect tBoundingBox = pl_rect_expand_vec2(&tTextBounding, (plVec2){0.5f * (gptCtx->tStyle.tFramePadding.x + gptCtx->tStyle.tInnerSpacing.x + tWidgetSize.y), 0.0f});
        tBoundingBox = pl_rect_move_start_x(&tBoundingBox, tStartPos.x + gptCtx->tStyle.tFramePadding.x);
        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(bPressed)
            *piValue = iButtonValue;

        if(gptCtx->uActiveId == uHash)       gptDraw->add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tColorScheme.tFrameBgActiveCol, 12);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tColorScheme.tFrameBgHoveredCol, 12);
        else                                 gptDraw->add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 1.5f, gptCtx->tColorScheme.tFrameBgCol, 12);

        if(*piValue == iButtonValue)
            gptDraw->add_circle_filled(ptWindow->ptFgLayer, (plVec2){tStartPos.x + tWidgetSize.y / 2.0f, tStartPos.y + tWidgetSize.y / 2.0f}, gptCtx->tStyle.fFontSize / 2.5f, gptCtx->tColorScheme.tCheckmarkCol, 12);

        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return bPressed;
}

bool
pl_ui_collapsing_header(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        const plVec2 tTextStartPos = {
            .x = tStartPos.x + tWidgetSize.y * 1.5f,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
        bool bHovered = false;
        bool bHeld = false;
        const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(bPressed)
            *pbOpenState = !*pbOpenState;

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tHeaderActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tHeaderHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tHeaderCol);

        if(*pbOpenState)
        {
            const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
            const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
            const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
            const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
            gptDraw->add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
        }
        else
        {
            const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
            const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
            const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
            const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
            gptDraw->add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
        }

        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f);
    }
    if(*pbOpenState)
        pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return *pbOpenState; 
}

void
pl_ui_end_collapsing_header(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
}

bool
pl_ui_tree_node(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    
    bool* pbOpenState = pl_ui_get_bool_ptr(&ptWindow->tStorage, uHash, false);
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {

        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        const plRect tBoundingBox = pl_calculate_rect(tStartPos, tWidgetSize);
        bool bHovered = false;
        bool bHeld = false;
        const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(bPressed)
            *pbOpenState = !*pbOpenState;

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tHeaderActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tHeaderHoveredCol);

        if(*pbOpenState)
        {
            const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
            const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){ 0.0f,  4.0f});
            const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ 4.0f, -4.0f});
            const plVec2 leftPos  = pl_add_vec2(centerPoint,  (plVec2){-4.0f, -4.0f});
            gptDraw->add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}); 
        }
        else
        {
            const plVec2 centerPoint = {tStartPos.x + 8.0f * 1.5f, tStartPos.y + tWidgetSize.y / 2.0f};
            const plVec2 pointPos = pl_add_vec2(centerPoint, (plVec2){  4.0f,  0.0f});
            const plVec2 rightPos = pl_add_vec2(centerPoint, (plVec2){ -4.0f, -4.0f});
            const plVec2 leftPos  = pl_add_vec2(centerPoint, (plVec2){ -4.0f,  4.0f});
            gptDraw->add_triangle_filled(ptWindow->ptFgLayer, pointPos, rightPos, leftPos, (plVec4){1.0f, 1.0f, 1.0f, 1.0f});
        }

        const plVec2 tTextStartPos = {
            .x = tStartPos.x + tWidgetSize.y * 1.5f,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);

    if(*pbOpenState)
    {
        ptWindow->tTempData.uTreeDepth++;
        pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
        pl_sb_push(gptCtx->sbuIdStack, uHash);
    }

    return *pbOpenState; 
}

bool
pl_ui_tree_node_f(const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    bool bOpen = pl_ui_tree_node_v(pcFmt, args);
    va_end(args);
    return bOpen;
}

bool
pl_ui_tree_node_v(const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    pl_vsprintf(acTempBuffer, pcFmt, args);
    return pl_ui_tree_node(acTempBuffer);
}

void
pl_ui_tree_pop(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.uTreeDepth--;
    pl_sb_pop(gptCtx->sbuIdStack);
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
}

bool
pl_ui_begin_tab_bar(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const float fFrameHeight = pl_ui_get_frame_height();

    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());

    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    ptWindow->tTempData.tCurrentLayoutRow.fRowStartX = tStartPos.x - ptWindow->tTempData.tRowPos.x;
    ptWindow->tTempData.fAccumRowX += ptWindow->tTempData.tCurrentLayoutRow.fRowStartX;
    
    pl_ui_layout_dynamic(0.0f, 1);
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    pl_sb_push(gptCtx->sbuIdStack, uHash);

    // check if tab bar existed
    gptCtx->ptCurrentTabBar = NULL;
    for(uint32_t i = 0; i < pl_sb_size(gptCtx->sbtTabBars); i++)
    {
        if(gptCtx->sbtTabBars[i].uId == uHash)
        {
            gptCtx->ptCurrentTabBar = &gptCtx->sbtTabBars[i];
            break;
        }
    }

    // new tab bar needs to be created
    if(gptCtx->ptCurrentTabBar == NULL)
    {
        plUiTabBar tTabBar = {
            .uId = uHash
        };;

        pl_sb_push(gptCtx->sbtTabBars, tTabBar);
        gptCtx->ptCurrentTabBar = &pl_sb_top(gptCtx->sbtTabBars);
    }

    gptCtx->ptCurrentTabBar->tStartPos = tStartPos;
    gptCtx->ptCurrentTabBar->tCursorPos = tStartPos;
    gptCtx->ptCurrentTabBar->uCurrentIndex = 0u;

    gptDraw->add_line(ptWindow->ptFgLayer, 
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x, gptCtx->ptCurrentTabBar->tStartPos.y + fFrameHeight},
        (plVec2){gptCtx->ptCurrentTabBar->tStartPos.x + tWidgetSize.x, gptCtx->ptCurrentTabBar->tStartPos.y + fFrameHeight},
        gptCtx->tColorScheme.tButtonActiveCol, 1.0f);

    pl_ui_advance_cursor(tWidgetSize.x, fFrameHeight);
    return true;
}

void
pl_ui_end_tab_bar(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->ptCurrentTabBar->uValue = gptCtx->ptCurrentTabBar->uNextValue;
    pl_sb_pop(gptCtx->sbuIdStack);
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
    ptWindow->tTempData.fAccumRowX -= ptWindow->tTempData.tCurrentLayoutRow.fRowStartX;
}

bool
pl_ui_begin_tab(const char* pcText)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_push(ptWindow->sbtRowStack, ptWindow->tTempData.tCurrentLayoutRow);
    pl_ui_layout_dynamic(0.0f, 1);
    plUiTabBar* ptTabBar = gptCtx->ptCurrentTabBar;
    const float fFrameHeight = pl_ui_get_frame_height();
    const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
    pl_sb_push(gptCtx->sbuIdStack, uHash);

    if(ptTabBar->uValue == 0u) ptTabBar->uValue = uHash;

    const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcText, -1.0f);
    const plVec2 tStartPos = ptTabBar->tCursorPos;

    plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcText, pl_ui_find_renderered_text_end(pcText, NULL), -1.0f);
    const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

    const plVec2 tFinalSize = {tTextSize.x + 2.0f * gptCtx->tStyle.tFramePadding.x, fFrameHeight};

    const plVec2 tTextStartPos = {
        .x = tStartPos.x + tStartPos.x + tFinalSize.x / 2.0f - tTextActualCenter.x,
        .y = tStartPos.y + tStartPos.y + fFrameHeight / 2.0f - tTextActualCenter.y
    };

    const plRect tBoundingBox = pl_calculate_rect(tStartPos, tFinalSize);
    bool bHovered = false;
    bool bHeld = false;
    const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

    if(uHash == gptCtx->uActiveId)
    {
        ptTabBar->uNextValue = uHash;
    }

    if(gptCtx->uActiveId== uHash)        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonActiveCol);
    else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonHoveredCol);
    else if(ptTabBar->uValue == uHash)   gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonActiveCol);
    else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tButtonCol);
    
    pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcText, -1.0f);

    ptTabBar->tCursorPos.x += gptCtx->tStyle.tInnerSpacing.x + tFinalSize.x;
    ptTabBar->uCurrentIndex++;

    if(ptTabBar->uValue != uHash)
        pl_ui_end_tab();

    return ptTabBar->uValue == uHash;
}

void
pl_ui_end_tab(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    pl_sb_pop(gptCtx->sbuIdStack);
    ptWindow->tTempData.tCurrentLayoutRow = pl_sb_pop(ptWindow->sbtRowStack);
}

void
pl_ui_separator(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(gptCtx->tStyle.tItemSpacing.y * 2.0f);
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
        gptDraw->add_line(ptWindow->ptFgLayer, tStartPos, (plVec2){tStartPos.x + tWidgetSize.x, tStartPos.y}, gptCtx->tColorScheme.tCheckmarkCol, 1.0f);

    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_vertical_spacing(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    ptWindow->tTempData.tRowPos.y += gptCtx->tStyle.tItemSpacing.y * 2.0f;
}

void
pl_ui_indent(float fIndent)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fHorizontalOffset += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    ptWindow->tTempData.fExtraIndent += fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
}

void
pl_ui_unindent(float fIndent)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fHorizontalOffset -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
    ptWindow->tTempData.fExtraIndent -= fIndent == 0.0f ? gptCtx->tStyle.fIndentSize : fIndent;
}

bool
pl_ui_step_clipper(plUiClipper* ptClipper)
{
    if(ptClipper->uItemCount == 0)
        return false;
        
    if(ptClipper->uDisplayStart == 0 && ptClipper->uDisplayEnd == 0)
    {
        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 1;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = pl__ui_get_cursor_pos().y;
        return true;
    }
    else if (ptClipper->_fItemHeight == 0.0f)
    {
        ptClipper->_fItemHeight = pl__ui_get_cursor_pos().y - ptClipper->_fStartPosY;
        if(ptClipper->_fStartPosY < pl_ui_get_window_pos().y)
            ptClipper->uDisplayStart = (uint32_t)((pl_ui_get_window_pos().y - ptClipper->_fStartPosY) / ptClipper->_fItemHeight);
        ptClipper->uDisplayEnd = ptClipper->uDisplayStart + (uint32_t)(pl_ui_get_window_size().y / ptClipper->_fItemHeight) + 1;
        ptClipper->uDisplayEnd = pl_minu(ptClipper->uDisplayEnd, ptClipper->uItemCount) + 1;
        if(ptClipper->uDisplayStart > 0)
            ptClipper->uDisplayStart--;

        if(ptClipper->uDisplayEnd > ptClipper->uItemCount)
            ptClipper->uDisplayEnd = ptClipper->uItemCount;
        
        if(ptClipper->uDisplayStart > 0)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_ui_advance_cursor(0.0f, (float)ptClipper->uDisplayStart * ptClipper->_fItemHeight);
        }
        ptClipper->uDisplayStart++;
        return true;
    }
    else
    {
        if(ptClipper->uDisplayEnd < ptClipper->uItemCount)
        {
            for(uint32_t i = 0; i < gptCtx->ptCurrentWindow->tTempData.tCurrentLayoutRow.uColumns; i++)
                pl_ui_advance_cursor(0.0f, (float)(ptClipper->uItemCount - ptClipper->uDisplayEnd) * ptClipper->_fItemHeight);
        }

        ptClipper->uDisplayStart = 0;
        ptClipper->uDisplayEnd = 0;
        ptClipper->_fItemHeight = 0.0f;
        ptClipper->_fStartPosY = 0.0f;
        ptClipper->uItemCount = 0;
        return false;
    }
}

void
pl_ui_text(const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_text_v(pcFmt, args);
    va_end(args);
}

void
pl_ui_text_v(const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        pl_vsprintf(acTempBuffer, pcFmt, args);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl_ui_find_renderered_text_end(acTempBuffer, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y}, gptCtx->tColorScheme.tTextCol, acTempBuffer, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_color_text(plVec4 tColor, const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_color_text_v(tColor, pcFmt, args);
    va_end(args);
}

void
pl_ui_color_text_v(plVec4 tColor, const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        pl_vsprintf(acTempBuffer, pcFmt, args);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl_ui_find_renderered_text_end(acTempBuffer, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y}, tColor, acTempBuffer, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_labeled_text(const char* pcLabel, const char* pcFmt, ...)
{
    va_list args;
    va_start(args, pcFmt);
    pl_ui_labeled_text_v(pcLabel, pcFmt, args);
    va_end(args);
}

void
pl_ui_labeled_text_v(const char* pcLabel, const char* pcFmt, va_list args)
{
    static char acTempBuffer[1024];
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        pl_vsprintf(acTempBuffer, pcFmt, args);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, acTempBuffer, pl_ui_find_renderered_text_end(acTempBuffer, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        const plVec2 tStartLocation = {tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y};
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, pl_add_vec2(tStartLocation, (plVec2){(tWidgetSize.x / 3.0f), 0.0f}), gptCtx->tColorScheme.tTextCol, acTempBuffer, -1.0f);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartLocation, gptCtx->tColorScheme.tTextCol, pcLabel, -1.0f);
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

bool
pl_ui_input_text(const char* pcLabel, char* pcBuffer, size_t szBufferSize)
{
    return pl_ui_input_text_ex(pcLabel, NULL, pcBuffer, szBufferSize, 0);
}

bool
pl_ui_input_text_hint(const char* pcLabel, const char* pcHint, char* pcBuffer, size_t szBufferSize)
{
    return pl_ui_input_text_ex(pcLabel, pcHint, pcBuffer, szBufferSize, 0);
}

bool
pl_ui_input_float(const char* pcLabel, float* pfValue, const char* pcFormat)
{
    char acBuffer[64] = {0};
    pl_sprintf(acBuffer, pcFormat, *pfValue);
    const bool bChanged = pl_ui_input_text_ex(pcLabel, NULL, acBuffer, 64, PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC);

    if(bChanged)
        *pfValue = (float)atof(acBuffer);

    return bChanged;
}

bool
pl_ui_input_int(const char* pcLabel, int* piValue)
{
    char acBuffer[64] = {0};
    pl_sprintf(acBuffer, "%d", *piValue);
    const bool bChanged = pl_ui_input_text_ex(pcLabel, NULL, acBuffer, 64, PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL);

    if(bChanged)
        *piValue = atoi(acBuffer);

    return bChanged;
}

bool
pl_ui_input_text_ex(const char* pcLabel, const char* pcHint, char* pcBuffer, size_t szBufferSize, plUiInputTextFlags tFlags)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tMousePos = pl_get_mouse_pos();
    plIOContext* ptIo = pl_get_io_context();

    const bool RENDER_SELECTION_WHEN_INACTIVE = false;
    const bool bIsMultiLine = (tFlags & PL_UI_INPUT_TEXT_FLAGS_MULTILINE) != 0;
    const bool bIsReadOnly  = (tFlags & PL_UI_INPUT_TEXT_FLAGS_READ_ONLY) != 0;
    const bool bIsPassword  = (tFlags & PL_UI_INPUT_TEXT_FLAGS_PASSWORD) != 0;
    const bool bIsUndoable  = (tFlags & PL_UI_INPUT_TEXT_FLAGS_NO_UNDO_REDO) != 0;
    const bool bIsResizable = (tFlags & PL_UI_INPUT_TEXT_FLAGS_CALLBACK_RESIZE) != 0;

    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();


    const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };
    const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));

    const plRect tLabelTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl_ui_find_renderered_text_end(pcLabel, NULL), -1.0f);
    const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

    const plVec2 tFrameSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
    const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tFrameSize);

    plUiWindow* ptDrawWindow = ptWindow;
    plVec2 tInnerSize = tFrameSize;

    const bool bHovered = pl_ui_is_item_hoverable(&tBoundingBox, uHash) && (gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0);

    if(bHovered)
    {
        pl_set_mouse_cursor(PL_MOUSE_CURSOR_TEXT_INPUT);
        gptCtx->uNextHoveredId = uHash;
    }

    plInputTextState* ptState = pl__get_input_text_state(uHash);

    // TODO: scroll stuff
    const bool bUserClicked = bHovered && pl_is_mouse_clicked(0, false);
    const bool bUserScrollFinish = false; // bIsMultiLine && ptState != NULL && gptCtx->uNextActiveId == 0 && gptCtx->uActiveId == GetWindowScrollbarID(ptDrawWindow, ImGuiAxis_Y);
    const bool bUserScrollActive = false; // bIsMultiLine && ptState != NULL && gptCtx->uNextActiveId == GetWindowScrollbarID(ptDrawWindow, ImGuiAxis_Y);

    bool bClearActiveId = false;
    bool bSelectAll = false;

    float fScrollY = bIsMultiLine ? ptDrawWindow->tScroll.y : FLT_MAX;

    const bool bInitChangedSpecs = (ptState != NULL && ptState->tStb.single_line != !bIsMultiLine); // state != NULL means its our state.
    const bool bInitMakeActive = (bUserClicked || bUserScrollFinish);
    const bool bInitState = (bInitMakeActive || bUserScrollActive);
    if((bUserClicked && gptCtx->uActiveId != uHash) || bInitChangedSpecs)
    {
        // Access state even if we don't own it yet.
        ptState = &gptCtx->tInputTextState;
        pl__text_state_cursor_anim_reset(ptState);

        // Take a copy of the initial buffer value (both in original UTF-8 format and converted to wchar)
        // From the moment we focused we are ignoring the content of 'buf' (unless we are in read-only mode)
        const int iBufferLength = (int)strlen(pcBuffer);
        pl_sb_resize(ptState->sbInitialTextA, iBufferLength + 1); // UTF-8. we use +1 to make sure that it is always pointing to at least an empty string.
        memcpy(ptState->sbInitialTextA, pcBuffer, iBufferLength + 1);

        // Preserve cursor position and undo/redo stack if we come back to same widget
        // FIXME: Since we reworked this on 2022/06, may want to differenciate recycle_cursor vs recycle_undostate?
        bool bRecycleState = (ptState->uId == uHash && !bInitChangedSpecs);
        if (bRecycleState && (ptState->iCurrentLengthA != iBufferLength || (ptState->bTextAIsValid && strncmp(ptState->sbTextA, pcBuffer, iBufferLength) != 0)))
            bRecycleState = false;

        // start edition
        const char* pcBufferEnd = NULL;
        ptState->uId = uHash;
        pl_sb_resize(ptState->sbTextW, (uint32_t)szBufferSize + 1);          // wchar count <= UTF-8 count. we use +1 to make sure that .Data is always pointing to at least an empty string.
        pl_sb_reset(ptState->sbTextA);
        ptState->bTextAIsValid = false;                // TextA is not valid yet (we will display buf until then)
        ptState->iCurrentLengthW = pl__text_str_from_utf8(ptState->sbTextW, (int)szBufferSize, pcBuffer, NULL, &pcBufferEnd);
        ptState->iCurrentLengthA = (int)(pcBufferEnd - pcBuffer);      // We can't get the result from ImStrncpy() above because it is not UTF-8 aware. Here we'll cut off malformed UTF-8.

        if (bRecycleState)
        {
            // Recycle existing cursor/selection/undo stack but clamp position
            // Note a single mouse click will override the cursor/position immediately by calling stb_textedit_click handler.
            pl__text_state_cursor_clamp(ptState);
        }
        else
        {
            ptState->fScrollX = 0.0f;
            stb_textedit_initialize_state(&ptState->tStb, !bIsMultiLine);
        }

        if (!bIsMultiLine)
        {
            if (tFlags & PL_UI_INPUT_TEXT_FLAGS_AUTO_SELECT_ALL)
                bSelectAll = true;
            // if (input_requested_by_nav && (!bRecycleState || !(g.NavActivateFlags & ImGuiActivateFlags_TryToPreserveState)))
            //     bSelectAll = true;
            // if (input_requested_by_tabbing || (bUserClicked &&ptIo->bKeyCtrl))
            //     bSelectAll = true;
        }

        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_ALWAYS_OVERWRITE)
            ptState->tStb.insert_mode = 1; // stb field name is indeed incorrect (see #2863)

    }

    const bool bIsOsX = ptIo->bConfigMacOSXBehaviors;
    if (gptCtx->uActiveId != uHash && bInitMakeActive)
    {
        PL_ASSERT(ptState && ptState->uId == uHash);
        gptCtx->tPrevItemData.bActive = true;
        gptCtx->uNextActiveId = uHash;
        // SetActiveID(id, window);
        // SetFocusID(id, window);
        // FocusWindow(window);
    }
    if (gptCtx->uActiveId == uHash)
    {
        gptCtx->tPrevItemData.bActive = true;
        gptCtx->uNextActiveId = uHash;
        // Declare some inputs, the other are registered and polled via Shortcut() routing system.
        // if (bUserClicked)
        //     SetKeyOwner(PL_KEY_MouseLeft, id);
        // g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Left) | (1 << ImGuiDir_Right);
        // if (bIsMultiLine || (flags & ImGuiInputTextFlags_CallbackHistory))
        //     g.ActiveIdUsingNavDirMask |= (1 << ImGuiDir_Up) | (1 << ImGuiDir_Down);
        // SetKeyOwner(PL_KEY_Home, id);
        // SetKeyOwner(PL_KEY_End, id);
        // if (bIsMultiLine)
        // {
        //     SetKeyOwner(PL_KEY_PageUp, id);
        //     SetKeyOwner(PL_KEY_PageDown, id);
        // }
        // if (bIsOsX)
        //     SetKeyOwner(ImGuiMod_Alt, id);
        // if (flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_AllowTabInput)) // Disable keyboard tabbing out as we will use the \t character.
        //     SetShortcutRouting(PL_KEY_Tab, id);
    }

    // We have an edge case if ActiveId was set through another widget (e.g. widget being swapped), clear id immediately (don't wait until the end of the function)
    if (gptCtx->uActiveId == uHash && ptState == NULL)
        gptCtx->uNextActiveId = 0;

    // Release focus when we click outside
    if (gptCtx->uActiveId == uHash && pl_is_mouse_clicked(0, false) && !bInitState && !bInitMakeActive) //-V560
        bClearActiveId = true;

    // Lock the decision of whether we are going to take the path displaying the cursor or selection
    bool bRenderCursor = (gptCtx->uActiveId == uHash) || (ptState && bUserScrollActive);
    bool bRenderSelection = ptState && (pl__text_state_has_selection(ptState) || bSelectAll) && (RENDER_SELECTION_WHEN_INACTIVE || bRenderCursor);
    bool bValueChanged = false;
    bool bValidated = false;

    // When read-only we always use the live data passed to the function
    // FIXME-OPT: Because our selection/cursor code currently needs the wide text we need to convert it when active, which is not ideal :(
    if (bIsReadOnly && ptState != NULL && (bRenderCursor || bRenderSelection))
    {
        const char* pcBufferEnd = NULL;
        pl_sb_resize(ptState->sbTextW, (uint32_t)szBufferSize + 1);
        ptState->iCurrentLengthW = pl__text_str_from_utf8(ptState->sbTextW, pl_sb_size(ptState->sbTextW), pcBuffer, NULL, &pcBufferEnd);
        ptState->iCurrentLengthA = (int)(pcBufferEnd - pcBuffer);
        pl__text_state_cursor_clamp(ptState);
        bRenderSelection &= pl__text_state_has_selection(ptState);
    }

    // Select the buffer to render.
    const bool bBufferDisplayFromState = (bRenderCursor || bRenderSelection || gptCtx->uActiveId == uHash) && !bIsReadOnly && ptState && ptState->bTextAIsValid;
    const bool bIsDisplayingHint = (pcHint != NULL && (bBufferDisplayFromState ? ptState->sbTextA : pcBuffer)[0] == 0);

    // Password pushes a temporary font with only a fallback glyph
    if (bIsPassword && !bIsDisplayingHint)
    {
        // const ImFontGlyph* glyph = g.Font->FindGlyph('*');
        // ImFont* password_font = &g.InputTextPasswordFont;
        // password_font->FontSize = g.Font->FontSize;
        // password_font->Scale = g.Font->Scale;
        // password_font->Ascent = g.Font->Ascent;
        // password_font->Descent = g.Font->Descent;
        // password_font->ContainerAtlas = g.Font->ContainerAtlas;
        // password_font->FallbackGlyph = glyph;
        // password_font->FallbackAdvanceX = glyph->AdvanceX;
        // PL_ASSERT(password_font->Glyphs.empty() && password_font->IndexAdvanceX.empty() && password_font->IndexLookup.empty());
        // PushFont(password_font);
    }

    // process mouse inputs and character inputs
    int iBackupCurrentTextLength = 0;
    if(gptCtx->uActiveId == uHash)
    {
        PL_ASSERT(ptState != NULL);
        iBackupCurrentTextLength = ptState->iCurrentLengthA;
        ptState->bEdited = false;
        ptState->iBufferCapacityA = (int)szBufferSize;
        ptState->tFlags = tFlags;

        // Although we are active we don't prevent mouse from hovering other elements unless we are interacting right now with the widget.
        // Down the line we should have a cleaner library-wide concept of Selected vs Active.
        // g.ActiveIdAllowOverlap = !io.MouseDown[0];

        // Edit in progress
        const float fMouseX = (tMousePos.x - tBoundingBox.tMin.x - gptCtx->tStyle.tFramePadding.x) + ptState->fScrollX;
        const float fMouseY = (bIsMultiLine ? (tMousePos.y - tBoundingBox.tMin.y) : (gptCtx->tStyle.fFontSize * 0.5f));

        if (bSelectAll)
        {
            pl__text_state_select_all(ptState);
            ptState->bSelectedAllMouseLock = true;
        }
        else if (bHovered && ptIo->_auMouseClickedCount[0] >= 2 && !ptIo->bKeyShift)
        {
            stb_textedit_click(ptState, &ptState->tStb, fMouseX, fMouseY);
            const int iMultiClipCount = (ptIo->_auMouseClickedCount[0] - 2);
            if ((iMultiClipCount % 2) == 0)
            {
                // Double-click: Select word
                // We always use the "Mac" word advance for double-click select vs CTRL+Right which use the platform dependent variant:
                // FIXME: There are likely many ways to improve this behavior, but there's no "right" behavior (depends on use-case, software, OS)
                const bool bIsBol = (ptState->tStb.cursor == 0) || STB_TEXTEDIT_GETCHAR(ptState, ptState->tStb.cursor - 1) == '\n';
                if (STB_TEXT_HAS_SELECTION(&ptState->tStb) || !bIsBol)
                    pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_WORDLEFT);
                if (!STB_TEXT_HAS_SELECTION(&ptState->tStb))
                    stb_textedit_prep_selection_at_cursor(&ptState->tStb);
                ptState->tStb.cursor = STB_TEXTEDIT_MOVEWORDRIGHT_MAC(ptState, ptState->tStb.cursor);
                ptState->tStb.select_end = ptState->tStb.cursor;
                stb_textedit_clamp(ptState, &ptState->tStb);
            }
            else
            {
                // Triple-click: Select line
                const bool is_eol = STB_TEXTEDIT_GETCHAR(ptState, ptState->tStb.cursor) == '\n';
                pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_LINESTART);
                pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_LINEEND | STB_TEXTEDIT_K_SHIFT);
                pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_RIGHT | STB_TEXTEDIT_K_SHIFT);
                if (!is_eol && bIsMultiLine)
                {
                    int iOne = ptState->tStb.select_start;
                    int iTwo = ptState->tStb.select_end;
                    ptState->tStb.select_start = iTwo;
                    ptState->tStb.select_end = iOne;
                    ptState->tStb.cursor = ptState->tStb.select_end;
                }
                ptState->bCursorFollow = false;
            }
            pl__text_state_cursor_anim_reset(ptState);
        }
        else if (ptIo->_abMouseClicked[0] && !ptState->bSelectedAllMouseLock)
        {
            if (bHovered)
            {
                if (ptIo->bKeyShift)
                    stb_textedit_drag(ptState, &ptState->tStb, fMouseX, fMouseY);
                else
                    stb_textedit_click(ptState, &ptState->tStb, fMouseX, fMouseY);
                pl__text_state_cursor_anim_reset(ptState);
            }
        }
        else if (ptIo->_abMouseDown[0] && !ptState->bSelectedAllMouseLock && (ptIo->_tMouseDelta.x != 0.0f || ptIo->_tMouseDelta.y != 0.0f))
        {
            stb_textedit_drag(ptState, &ptState->tStb, fMouseX, fMouseY);
            pl__text_state_cursor_anim_reset(ptState);
            ptState->bCursorFollow = true;
        }
        if (ptState->bSelectedAllMouseLock && !ptIo->_abMouseDown[0])
            ptState->bSelectedAllMouseLock = false;

        // We expect backends to emit a Tab key but some also emit a Tab character which we ignore (#2467, #1336)
        // (For Tab and Enter: Win32/SFML/Allegro are sending both keys and chars, GLFW and SDL are only sending keys. For Space they all send all threes)
        if ((tFlags & PL_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT) && pl_is_key_down(PL_KEY_TAB) && !bIsReadOnly)
        {
            unsigned int c = '\t'; // Insert TAB
            if (pl__input_text_filter_character(&c, tFlags))
                pl__text_state_on_key_press(ptState, (int)c);
        }

        // Process regular text input (before we check for Return because using some IME will effectively send a Return?)
        // We ignore CTRL inputs, but need to allow ALT+CTRL as some keyboards (e.g. German) use AltGR (which _is_ Alt+Ctrl) to input certain characters.
        const bool bIgnoreCharInputs = (ptIo->bKeyCtrl && !ptIo->bKeyAlt) || (bIsOsX && ptIo->bKeySuper);

        if (pl_sb_size(ptIo->_sbInputQueueCharacters) > 0)
        {
            if (!bIgnoreCharInputs && !bIsReadOnly) // && input_requested_by_nav
                for (uint32_t n = 0; n < pl_sb_size(ptIo->_sbInputQueueCharacters); n++)
                {
                    // Insert character if they pass filtering
                    unsigned int c = (unsigned int)ptIo->_sbInputQueueCharacters[n];
                    if (c == '\t') // Skip Tab, see above.
                        continue;
                    if (pl__input_text_filter_character(&c, tFlags))
                        pl__text_state_on_key_press(ptState, c);
                }

            // consume characters
            pl_sb_reset(pl_get_io_context()->_sbInputQueueCharacters)
        }
    }

    // Process other shortcuts/key-presses
    bool bRevertEdit = false;
    if (gptCtx->uActiveId == uHash && !gptCtx->bActiveIdJustActivated && !bClearActiveId)
    {
        PL_ASSERT(ptState != NULL);

        const int iRowCountPerPage = pl_max((int)((tInnerSize.y - gptCtx->tStyle.tFramePadding.y) / gptCtx->tStyle.fFontSize), 1);
        ptState->tStb.row_count_per_page = iRowCountPerPage;

        const int iKMask = (ptIo->bKeyShift ? STB_TEXTEDIT_K_SHIFT : 0);
        const bool bIsWordmoveKeyDown = bIsOsX ? ptIo->bKeyAlt : ptIo->bKeyCtrl;                     // OS X style: Text editing cursor movement using Alt instead of Ctrl
        const bool bIsStartendKeyDown = bIsOsX && ptIo->bKeySuper && !ptIo->bKeyCtrl && !ptIo->bKeyAlt;  // OS X style: Line/Text Start and End using Cmd+Arrows instead of Home/End

        // Using Shortcut() with ImGuiInputFlags_RouteFocused (default policy) to allow routing operations for other code (e.g. calling window trying to use CTRL+A and CTRL+B: formet would be handled by InputText)
        // Otherwise we could simply assume that we own the keys as we are active.
        // const ImGuiInputFlags bRepeat = ImGuiInputFlags_Repeat;
        const bool bRepeat = false;
        const bool bIsCut   = (ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_X, bRepeat)) || (ptIo->bKeyShift && pl_is_key_pressed(PL_KEY_DELETE, bRepeat)) && !bIsReadOnly && !bIsPassword && (!bIsMultiLine || pl__text_state_has_selection(ptState));
        const bool bIsCopy  = (ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_C, bRepeat)) || (ptIo->bKeyCtrl  && pl_is_key_pressed(PL_KEY_INSERT, bRepeat)) && !bIsPassword && (!bIsMultiLine || pl__text_state_has_selection(ptState));
        const bool bIsPaste = (ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_V, bRepeat)) || ((ptIo->bKeyShift && pl_is_key_pressed(PL_KEY_INSERT, bRepeat)) && !bIsReadOnly);
        const bool bIsUndo  = (ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_Z, bRepeat)) && !bIsReadOnly && bIsUndoable;
        const bool bIsRedo =  (ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_Y, bRepeat)) || (bIsOsX && ptIo->bKeyShift && ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_Z, bRepeat)) && !bIsReadOnly && bIsUndoable;
        const bool bIsSelectAll = ptIo->bKeyCtrl && pl_is_key_pressed(PL_KEY_A, bRepeat);

        // We allow validate/cancel with Nav source (gamepad) to makes it easier to undo an accidental NavInput press with no keyboard wired, but otherwise it isn't very useful.
        const bool bNavGamepadActive = false; // (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) != 0 && (io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0;
        const bool bIsEnterPressed = pl_is_key_pressed(PL_KEY_ENTER, true) || pl_is_key_pressed(PL_KEY_KEYPAD_ENTER, true);
        const bool bIsGamepadValidate = false; // nav_gamepad_active && (pl_is_key_pressed(PL_KEY_NavGamepadActivate, false) || pl_is_key_pressed(PL_KEY_NavGamepadInput, false));
        const bool bIsCancel = pl_is_key_pressed(PL_KEY_ESCAPE, bRepeat); // Shortcut(PL_KEY_Escape, id, bRepeat) || (nav_gamepad_active && Shortcut(PL_KEY_NavGamepadCancel, uHash, bRepeat));

        // FIXME: Should use more Shortcut() and reduce pl_is_key_pressed()+SetKeyOwner(), but requires modifiers combination to be taken account of.
        if (pl_is_key_pressed(PL_KEY_LEFT_ARROW, true))                        { pl__text_state_on_key_press(ptState, (bIsStartendKeyDown ? STB_TEXTEDIT_K_LINESTART : bIsWordmoveKeyDown ? STB_TEXTEDIT_K_WORDLEFT : STB_TEXTEDIT_K_LEFT) | iKMask); }
        else if (pl_is_key_pressed(PL_KEY_RIGHT_ARROW, true))                  { pl__text_state_on_key_press(ptState, (bIsStartendKeyDown ? STB_TEXTEDIT_K_LINEEND : bIsWordmoveKeyDown ? STB_TEXTEDIT_K_WORDRIGHT : STB_TEXTEDIT_K_RIGHT) | iKMask); }
        // else if (pl_is_key_pressed(PL_KEY_UP_ARROW, true) && bIsMultiLine)     { if (ptIo->bKeyCtrl) SetScrollY(ptDrawWindow, pl_max(ptDrawWindow->tScroll.y - gptCtx->tStyle.fFontSize, 0.0f)); else pl__text_state_on_key_press(ptState, (bIsStartendKeyDown ? STB_TEXTEDIT_K_TEXTSTART : STB_TEXTEDIT_K_UP) | iKMask); }
        // else if (pl_is_key_pressed(PL_KEY_DOWN_ARROW, true) && bIsMultiLine)   { if (ptIo->bKeyCtrl) SetScrollY(ptDrawWindow, pl_min(ptDrawWindow->tScroll.y + gptCtx->tStyle.fFontSize, GetScrollMaxY())); else pl__text_state_on_key_press(ptState, (bIsStartendKeyDown ? STB_TEXTEDIT_K_TEXTEND : STB_TEXTEDIT_K_DOWN) | iKMask); }
        // else if (pl_is_key_pressed(PL_KEY_PAGE_UP, true) && bIsMultiLine)      { pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_PGUP | iKMask); fScrollY -= iRowCountPerPage * gptCtx->tStyle.fFontSize; }
        // else if (pl_is_key_pressed(PL_KEY_PAGE_DOWN, true) && bIsMultiLine)    { pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_PGDOWN | iKMask); fScrollY += iRowCountPerPage * gptCtx->tStyle.fFontSize; }
        else if (pl_is_key_pressed(PL_KEY_HOME, true))                        { pl__text_state_on_key_press(ptState,ptIo->bKeyCtrl ? STB_TEXTEDIT_K_TEXTSTART | iKMask : STB_TEXTEDIT_K_LINESTART | iKMask); }
        else if (pl_is_key_pressed(PL_KEY_END, true))                         { pl__text_state_on_key_press(ptState,ptIo->bKeyCtrl ? STB_TEXTEDIT_K_TEXTEND | iKMask : STB_TEXTEDIT_K_LINEEND | iKMask); }
        else if (pl_is_key_pressed(PL_KEY_DELETE, true) && !bIsReadOnly && !bIsCut)
        {
            if (!pl__text_state_has_selection(ptState))
            {
                // OSX doesn't seem to have Super+Delete to delete until end-of-line, so we don't emulate that (as opposed to Super+Backspace)
                if (bIsWordmoveKeyDown)
                    pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_WORDRIGHT | STB_TEXTEDIT_K_SHIFT);
            }
            pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_DELETE | iKMask);
        }
        else if (pl_is_key_pressed(PL_KEY_BACKSPACE, true) && !bIsReadOnly)
        {
            if (!pl__text_state_has_selection(ptState))
            {
                if (bIsWordmoveKeyDown)
                    pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_WORDLEFT | STB_TEXTEDIT_K_SHIFT);
                else if (bIsOsX && ptIo->bKeySuper && !ptIo->bKeyAlt && !ptIo->bKeyCtrl)
                    pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_LINESTART | STB_TEXTEDIT_K_SHIFT);
            }
            pl__text_state_on_key_press(ptState, STB_TEXTEDIT_K_BACKSPACE | iKMask);
        }
        else if (bIsEnterPressed || bIsGamepadValidate)
        {
            // Determine if we turn Enter into a \n character
            bool bCtrlEnterForNewLine = (tFlags & PL_UI_INPUT_TEXT_FLAGS_CTRL_ENTER_FOR_NEW_LINE) != 0;
            if (!bIsMultiLine || bIsGamepadValidate || (bCtrlEnterForNewLine && !ptIo->bKeyCtrl) || (!bCtrlEnterForNewLine && ptIo->bKeyCtrl))
            {
                bValidated = true;
                // if (io.ConfigInputTextEnterKeepActive && !bIsMultiLine)
                //     pl__text_state_select_all(ptState); // No need to scroll
                // else
                    bClearActiveId = true;
            }
            else if (!bIsReadOnly)
            {
                unsigned int c = '\n'; // Insert new line
                if (pl__input_text_filter_character(&c, tFlags))
                    pl__text_state_on_key_press(ptState, (int)c);
            }
        }
        else if (bIsCancel)
        {
            if (tFlags & PL_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL)
            {
                if (ptState->iCurrentLengthA > 0)
                {
                    bRevertEdit = true;
                }
                else
                {
                    bRenderCursor = bRenderSelection = false;
                    bClearActiveId = true;
                }
            }
            else
            {
                bClearActiveId = bRevertEdit = true;
                bRenderCursor = bRenderSelection = false;
            }
        }
        else if (bIsUndo || bIsRedo)
        {
            pl__text_state_on_key_press(ptState, bIsUndo ? STB_TEXTEDIT_K_UNDO : STB_TEXTEDIT_K_REDO);
            pl__text_state_clear_selection(ptState);
        }
        else if (bIsSelectAll)
        {
            pl__text_state_select_all(ptState);
            ptState->bCursorFollow = true;
        }
        else if (bIsCut || bIsCopy)
        {
            // Cut, Copy
            if (ptIo->set_clipboard_text_fn)
            {
                const int ib = pl__text_state_has_selection(ptState) ? pl_min(ptState->tStb.select_start, ptState->tStb.select_end) : 0;
                const int ie = pl__text_state_has_selection(ptState) ? pl_max(ptState->tStb.select_start, ptState->tStb.select_end) : ptState->iCurrentLengthW;
                const int clipboard_data_len = pl__text_count_utf8_bytes_from_str(ptState->sbTextW + ib, ptState->sbTextW + ie) + 1;
                char* clipboard_data = (char*)PL_ALLOC(clipboard_data_len * sizeof(char));
                pl__text_str_to_utf8(clipboard_data, clipboard_data_len, ptState->sbTextW + ib, ptState->sbTextW + ie);
                ptIo->set_clipboard_text_fn(ptIo->pClipboardUserData, clipboard_data);
                PL_FREE(clipboard_data);
            }
            if (bIsCut)
            {
                if (!pl__text_state_has_selection(ptState))
                    pl__text_state_select_all(ptState);
                ptState->bCursorFollow = true;
                stb_textedit_cut(ptState, &ptState->tStb);
            }
        }
        else if (bIsPaste)
        {
            const char* clipboard = ptIo->get_clipboard_text_fn(ptIo->pClipboardUserData);
            if (clipboard)
            {
                // Filter pasted buffer
                const int clipboard_len = (int)strlen(clipboard);
                plWChar* clipboard_filtered = (plWChar*)PL_ALLOC((clipboard_len + 1) * sizeof(plWChar));
                int clipboard_filtered_len = 0;
                for (const char* s = clipboard; *s != 0; )
                {
                    unsigned int c;
                    s += pl_text_char_from_utf8(&c, s, NULL);
                    if (!pl__input_text_filter_character(&c, tFlags))
                        continue;
                    clipboard_filtered[clipboard_filtered_len++] = (plWChar)c;
                }
                clipboard_filtered[clipboard_filtered_len] = 0;
                if (clipboard_filtered_len > 0) // If everything was filtered, ignore the pasting operation
                {
                    stb_textedit_paste(ptState, &ptState->tStb, clipboard_filtered, clipboard_filtered_len);
                    ptState->bCursorFollow = true;
                }
                PL_FREE(clipboard_filtered);
            }
        }

        // Update render selection flag after events have been handled, so selection highlight can be displayed during the same frame.
        bRenderSelection |= pl__text_state_has_selection(ptState) && (RENDER_SELECTION_WHEN_INACTIVE || bRenderCursor);
    }

    // Process callbacks and apply result back to user's buffer.
    const char* pcApplyNewText = NULL;
    int iApplyNewTextLength = 0;
    if (gptCtx->uActiveId == uHash)
    {
        PL_ASSERT(ptState != NULL);
        if (bRevertEdit && !bIsReadOnly)
        {
            if (tFlags & PL_UI_INPUT_TEXT_FLAGS_ESCAPE_CLEARS_ALL)
            {
                // Clear input
                pcApplyNewText = "";
                iApplyNewTextLength = 0;
                STB_TEXTEDIT_CHARTYPE empty_string = 0;
                stb_textedit_replace(ptState, &ptState->tStb, &empty_string, 0);
            }
            else if (strcmp(pcBuffer, ptState->sbInitialTextA) != 0)
            {
                // Restore initial value. Only return true if restoring to the initial value changes the current buffer contents.
                // Push records into the undo stack so we can CTRL+Z the revert operation itself
                pcApplyNewText = ptState->sbInitialTextA;
                iApplyNewTextLength = pl_sb_size(ptState->sbInitialTextA) - 1;
                bValueChanged = true;
                plWChar* sbWText = NULL;
                if (iApplyNewTextLength > 0)
                {
                    pl_sb_resize(sbWText, pl__text_count_chars_from_utf8(pcApplyNewText, pcApplyNewText + iApplyNewTextLength) + 1);
                    pl__text_str_from_utf8(sbWText, pl_sb_size(sbWText), pcApplyNewText, pcApplyNewText + iApplyNewTextLength, NULL);
                }
                stb_textedit_replace(ptState, &ptState->tStb, sbWText, (iApplyNewTextLength > 0) ? (pl_sb_size(sbWText) - 1) : 0);
            }
        }

        // Apply ASCII value
        if (!bIsReadOnly)
        {
            ptState->bTextAIsValid = true;
            pl_sb_resize(ptState->sbTextA, pl_sb_size(ptState->sbTextW) * 4 + 1);
            pl__text_str_to_utf8(ptState->sbTextA, pl_sb_size(ptState->sbTextA), ptState->sbTextW, NULL);
        }

        // When using 'ImGuiInputTextFlags_EnterReturnsTrue' as a special case we reapply the live buffer back to the input buffer before clearing ActiveId, even though strictly speaking it wasn't modified on this frame.
        // If we didn't do that, code like InputInt() with ImGuiInputTextFlags_EnterReturnsTrue would fail.
        // This also allows the user to use InputText() with ImGuiInputTextFlags_EnterReturnsTrue without maintaining any user-side storage (please note that if you use this property along ImGuiInputTextFlags_CallbackResize you can end up with your temporary string object unnecessarily allocating once a frame, either store your string data, either if you don't then don't use ImGuiInputTextFlags_CallbackResize).
        const bool bApplyEditBackToUserBuffer = !bRevertEdit || (bValidated && (tFlags & PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE) != 0);
        if (bApplyEditBackToUserBuffer)
        {
            // Apply new value immediately - copy modified buffer back
            // Note that as soon as the input box is active, the in-widget value gets priority over any underlying modification of the input buffer
            // FIXME: We actually always render 'buf' when calling DrawList->AddText, making the comment above incorrect.
            // FIXME-OPT: CPU waste to do this every time the widget is active, should mark dirty state from the stb_textedit callbacks.

            // User callback
            // if ((flags & (ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_CallbackEdit | ImGuiInputTextFlags_CallbackAlways)) != 0)
            // {
            //     PL_ASSERT(callback != NULL);

            //     // The reason we specify the usage semantic (Completion/History) is that Completion needs to disable keyboard TABBING at the moment.
            //     ImGuiInputTextFlags event_flag = 0;
            //     ImGuiKey event_key = ImGuiKey_None;
            //     if ((flags & ImGuiInputTextFlags_CallbackCompletion) != 0 && Shortcut(ImGuiKey_Tab, id))
            //     {
            //         event_flag = ImGuiInputTextFlags_CallbackCompletion;
            //         event_key = ImGuiKey_Tab;
            //     }
            //     else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(ImGuiKey_UpArrow))
            //     {
            //         event_flag = ImGuiInputTextFlags_CallbackHistory;
            //         event_key = ImGuiKey_UpArrow;
            //     }
            //     else if ((flags & ImGuiInputTextFlags_CallbackHistory) != 0 && IsKeyPressed(ImGuiKey_DownArrow))
            //     {
            //         event_flag = ImGuiInputTextFlags_CallbackHistory;
            //         event_key = ImGuiKey_DownArrow;
            //     }
            //     else if ((flags & ImGuiInputTextFlags_CallbackEdit) && state->Edited)
            //     {
            //         event_flag = ImGuiInputTextFlags_CallbackEdit;
            //     }
            //     else if (flags & ImGuiInputTextFlags_CallbackAlways)
            //     {
            //         event_flag = ImGuiInputTextFlags_CallbackAlways;
            //     }

            //     if (event_flag)
            //     {
            //         ImGuiInputTextCallbackData callback_data;
            //         callback_data.Ctx = &g;
            //         callback_data.EventFlag = event_flag;
            //         callback_data.Flags = flags;
            //         callback_data.UserData = callback_user_data;

            //         char* callback_buf = bIsReadOnly ? buf : state->TextA.Data;
            //         callback_data.EventKey = event_key;
            //         callback_data.Buf = callback_buf;
            //         callback_data.BufTextLen = ptState->iCurrentLengthA;
            //         callback_data.BufSize = state->BufCapacityA;
            //         callback_data.BufDirty = false;

            //         // We have to convert from wchar-positions to UTF-8-positions, which can be pretty slow (an incentive to ditch the plWChar buffer, see https://github.com/nothings/stb/issues/188)
            //         plWChar* text = ptState->sbTextW;
            //         const int utf8_cursor_pos = callback_data.CursorPos = pl__text_count_utf8_bytes_from_str(text, text + ptState->tStb.cursor);
            //         const int utf8_selection_start = callback_data.SelectionStart = pl__text_count_utf8_bytes_from_str(text, text + ptState->tStb.select_start);
            //         const int utf8_selection_end = callback_data.SelectionEnd = pl__text_count_utf8_bytes_from_str(text, text + ptState->tStb.select_end);

            //         // Call user code
            //         callback(&callback_data);

            //         // Read back what user may have modified
            //         callback_buf = bIsReadOnly ? buf : state->TextA.Data; // Pointer may have been invalidated by a resize callback
            //         PL_ASSERT(callback_data.Buf == callback_buf);         // Invalid to modify those fields
            //         PL_ASSERT(callback_data.BufSize == state->BufCapacityA);
            //         PL_ASSERT(callback_data.Flags == flags);
            //         const bool buf_dirty = callback_data.BufDirty;
            //         if (callback_data.CursorPos != utf8_cursor_pos || buf_dirty)            { ptState->tStb.cursor = pl__text_count_chars_from_utf8(callback_data.Buf, callback_data.Buf + callback_data.CursorPos); ptState->bCursorFollow = true; }
            //         if (callback_data.SelectionStart != utf8_selection_start || buf_dirty)  { ptState->tStb.select_start = (callback_data.SelectionStart == callback_data.CursorPos) ? ptState->tStb.cursor : pl__text_count_chars_from_utf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionStart); }
            //         if (callback_data.SelectionEnd != utf8_selection_end || buf_dirty)      { ptState->tStb.select_end = (callback_data.SelectionEnd == callback_data.SelectionStart) ? ptState->tStb.select_start : pl__text_count_chars_from_utf8(callback_data.Buf, callback_data.Buf + callback_data.SelectionEnd); }
            //         if (buf_dirty)
            //         {
            //             PL_ASSERT((flags & ImGuiInputTextFlags_ReadOnly) == 0);
            //             PL_ASSERT(callback_data.BufTextLen == (int)strlen(callback_data.Buf)); // You need to maintain BufTextLen if you change the text!
            //             InputTextReconcileUndoStateAfterUserCallback(state, callback_data.Buf, callback_data.BufTextLen); // FIXME: Move the rest of this block inside function and rename to InputTextReconcileStateAfterUserCallback() ?
            //             if (callback_data.BufTextLen > backup_current_text_length && is_resizable)
            //                 state->TextW.resize(state->TextW.Size + (callback_data.BufTextLen - backup_current_text_length)); // Worse case scenario resize
            //             state->CurLenW = pl__text_str_from_utf8(ptState->sbTextW, state->TextW.Size, callback_data.Buf, NULL);
            //             ptState->iCurrentLengthA = callback_data.BufTextLen;  // Assume correct length and valid UTF-8 from user, saves us an extra strlen()
            //             state->CursorAnimReset();
            //         }
            //     }
            // }

            // Will copy result string if modified
            if (!bIsReadOnly && strcmp(ptState->sbTextA, pcBuffer) != 0)
            {
                pcApplyNewText = ptState->sbTextA;
                iApplyNewTextLength = ptState->iCurrentLengthA;
                bValueChanged = true;
            }
        }
    }

    // Handle reapplying final data on deactivation (see InputTextDeactivateHook() for details)
    // if (g.InputTextDeactivatedState.ID == id)
    // {
    //     if (g.ActiveId != id && IsItemDeactivatedAfterEdit() && !is_readonly)
    //     {
    //         apply_new_text = g.InputTextDeactivatedState.TextA.Data;
    //         apply_new_text_length = g.InputTextDeactivatedState.TextA.Size - 1;
    //         value_changed |= (strcmp(g.InputTextDeactivatedState.TextA.Data, buf) != 0);
    //         //IMGUI_DEBUG_LOG("InputText(): apply Deactivated data for 0x%08X: \"%.*s\".\n", id, apply_new_text_length, apply_new_text);
    //     }
    //     g.InputTextDeactivatedState.ID = 0;
    // }

    // Copy result to user buffer. This can currently only happen when (g.ActiveId == id)
    if (pcApplyNewText != NULL)
    {
        // We cannot test for 'backup_current_text_length != apply_new_text_length' here because we have no guarantee that the size
        // of our owned buffer matches the size of the string object held by the user, and by design we allow InputText() to be used
        // without any storage on user's side.
        PL_ASSERT(iApplyNewTextLength >= 0);
        // if (bIsResizable)
        // {
        //     ImGuiInputTextCallbackData callback_data;
        //     callback_data.Ctx = &g;
        //     callback_data.EventFlag = ImGuiInputTextFlags_CallbackResize;
        //     callback_data.Flags = flags;
        //     callback_data.Buf = buf;
        //     callback_data.BufTextLen = apply_new_text_length;
        //     callback_data.BufSize = pl_max(buf_size, apply_new_text_length + 1);
        //     callback_data.UserData = callback_user_data;
        //     callback(&callback_data);
        //     buf = callback_data.Buf;
        //     buf_size = callback_data.BufSize;
        //     apply_new_text_length = pl_min(callback_data.BufTextLen, buf_size - 1);
        //     PL_ASSERT(apply_new_text_length <= buf_size);
        // }
        //IMGUI_DEBUG_PRINT("InputText(\"%s\"): apply_new_text length %d\n", label, apply_new_text_length);

        // If the underlying buffer resize was denied or not carried to the next frame, apply_new_text_length+1 may be >= buf_size.
        strncpy(pcBuffer, pcApplyNewText, pl_min(iApplyNewTextLength + 1, szBufferSize));
    }

    // Release active ID at the end of the function (so e.g. pressing Return still does a final application of the value)
    // Otherwise request text input ahead for next frame.
    if (gptCtx->uActiveId == uHash && bClearActiveId)
        gptCtx->uNextActiveId = 0;
    else if (gptCtx->uActiveId == uHash)
        ptIo->bWantTextInput = true;

    // Render frame
    if (!bIsMultiLine)
    {
        // RenderNavHighlight(frame_bb, id);
        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgCol);
    }


    // const ImVec4 clip_rect(frame_bb.Min.x, frame_bb.Min.y, frame_bb.Min.x + tInnerSize.x, frame_bb.Min.y + tInnerSize.y); // Not using frame_bb.Max because we have adjusted size
    const plRect clip_rect = {
        .tMin = tBoundingBox.tMin,
        .tMax = {
            .x = tBoundingBox.tMin.x + tInnerSize.x,
            .y = tBoundingBox.tMin.y + tInnerSize.y
        }
    };
    plVec2 draw_pos = bIsMultiLine ? tStartPos : pl_add_vec2(tFrameStartPos, gptCtx->tStyle.tFramePadding);
    plVec2 text_size = {0};

    // Set upper limit of single-line InputTextEx() at 2 million characters strings. The current pathological worst case is a long line
    // without any carriage return, which would makes ImFont::RenderText() reserve too many vertices and probably crash. Avoid it altogether.
    // Note that we only use this limit on single-line InputText(), so a pathologically large line on a InputTextMultiline() would still crash.
    const int iBufferDisplayMaxLength = 2 * 1024 * 1024;
    const char* pcBufferDisplay = bBufferDisplayFromState ? ptState->sbTextA : pcBuffer; //-V595
    const char* pcBufferDisplayEnd = NULL; // We have specialized paths below for setting the length
    if (bIsDisplayingHint)
    {
        pcBufferDisplay = pcHint;
        pcBufferDisplayEnd = pcHint + strlen(pcHint);
    }

    // Render text. We currently only render selection when the widget is active or while scrolling.
    // FIXME: We could remove the '&& bRenderCursor' to keep rendering selection when inactive.
    if (bRenderCursor || bRenderSelection)
    {
        PL_ASSERT(ptState != NULL);
        if (!bIsDisplayingHint)
            pcBufferDisplayEnd = pcBufferDisplay + ptState->iCurrentLengthA;

        // Render text (with cursor and selection)
        // This is going to be messy. We need to:
        // - Display the text (this alone can be more easily clipped)
        // - Handle scrolling, highlight selection, display cursor (those all requires some form of 1d->2d cursor position calculation)
        // - Measure text height (for scrollbar)
        // We are attempting to do most of that in **one main pass** to minimize the computation cost (non-negligible for large amount of text) + 2nd pass for selection rendering (we could merge them by an extra refactoring effort)
        // FIXME: This should occur on pcBufferDisplay but we'd need to maintain cursor/select_start/select_end for UTF-8.
        const plWChar* text_begin = ptState->sbTextW;
        plVec2 cursor_offset = {0};
        plVec2 select_start_offset = {0};

        {
            // Find lines numbers straddling 'cursor' (slot 0) and 'select_start' (slot 1) positions.
            const plWChar* searches_input_ptr[2] = { NULL, NULL };
            int searches_result_line_no[2] = { -1000, -1000 };
            int searches_remaining = 0;
            if (bRenderCursor)
            {
                searches_input_ptr[0] = text_begin + ptState->tStb.cursor;
                searches_result_line_no[0] = -1;
                searches_remaining++;
            }
            if (bRenderSelection)
            {
                searches_input_ptr[1] = text_begin + pl_min(ptState->tStb.select_start, ptState->tStb.select_end);
                searches_result_line_no[1] = -1;
                searches_remaining++;
            }

            // Iterate all lines to find our line numbers
            // In multi-line mode, we never exit the loop until all lines are counted, so add one extra to the searches_remaining counter.
            searches_remaining += bIsMultiLine ? 1 : 0;
            int line_count = 0;
            //for (const plWChar* s = text_begin; (s = (const plWChar*)wcschr((const wchar_t*)s, (wchar_t)'\n')) != NULL; s++)  // FIXME-OPT: Could use this when wchar_t are 16-bit
            for (const plWChar* s = text_begin; *s != 0; s++)
                if (*s == '\n')
                {
                    line_count++;
                    if (searches_result_line_no[0] == -1 && s >= searches_input_ptr[0]) { searches_result_line_no[0] = line_count; if (--searches_remaining <= 0) break; }
                    if (searches_result_line_no[1] == -1 && s >= searches_input_ptr[1]) { searches_result_line_no[1] = line_count; if (--searches_remaining <= 0) break; }
                }
            line_count++;
            if (searches_result_line_no[0] == -1)
                searches_result_line_no[0] = line_count;
            if (searches_result_line_no[1] == -1)
                searches_result_line_no[1] = line_count;

            // Calculate 2d position by finding the beginning of the line and measuring distance
            cursor_offset.x = pl__input_text_calc_text_size_w(pl__str_bol_w(searches_input_ptr[0], text_begin), searches_input_ptr[0], NULL, NULL, false).x;
            cursor_offset.y = searches_result_line_no[0] * gptCtx->tStyle.fFontSize;
            if (searches_result_line_no[1] >= 0)
            {
                select_start_offset.x = pl__input_text_calc_text_size_w(pl__str_bol_w(searches_input_ptr[1], text_begin), searches_input_ptr[1], NULL, NULL, false).x;
                select_start_offset.y = searches_result_line_no[1] * gptCtx->tStyle.fFontSize;
            }

            // Store text height (note that we haven't calculated text width at all, see GitHub issues #383, #1224)
            if (bIsMultiLine)
                text_size = (plVec2){tInnerSize.x, line_count * gptCtx->tStyle.fFontSize};
        }

        // Scroll
        if (bRenderCursor && ptState->bCursorFollow)
        {
            // Horizontal scroll in chunks of quarter width
            if (!(tFlags & PL_UI_INPUT_TEXT_FLAGS_NO_HORIZONTAL_SCROLL))
            {
                const float scroll_increment_x = tInnerSize.x * 0.25f;
                const float visible_width = tInnerSize.x - gptCtx->tStyle.tFramePadding.x;
                if (cursor_offset.x < ptState->fScrollX)
                    ptState->fScrollX = floorf(pl_max(0.0f, cursor_offset.x - scroll_increment_x));
                else if (cursor_offset.x - visible_width >= ptState->fScrollX)
                    ptState->fScrollX = floorf(cursor_offset.x - visible_width + scroll_increment_x);
            }
            else
            {
                ptState->fScrollX = 0.0f;
            }

            // Vertical scroll
            if (bIsMultiLine)
            {
                // Test if cursor is vertically visible
                if (cursor_offset.y - gptCtx->tStyle.fFontSize < fScrollY)
                    fScrollY = pl_max(0.0f, cursor_offset.y - gptCtx->tStyle.fFontSize);
                else if (cursor_offset.y - (tInnerSize.y - gptCtx->tStyle.tFramePadding.y * 2.0f) >= fScrollY)
                    fScrollY = cursor_offset.y - tInnerSize.y + gptCtx->tStyle.tFramePadding.y * 2.0f;
                const float scroll_max_y = pl_max((text_size.y + gptCtx->tStyle.tFramePadding.y * 2.0f) - tInnerSize.y, 0.0f);
                fScrollY = pl_clampf(0.0f, fScrollY, scroll_max_y);
                draw_pos.y += (ptDrawWindow->tScroll.y - fScrollY);   // Manipulate cursor pos immediately avoid a frame of lag
                ptDrawWindow->tScroll.y = fScrollY;
            }

            ptState->bCursorFollow = false;
        }

        // Draw selection
        const plVec2 draw_scroll = (plVec2){ptState->fScrollX, 0.0f};
        if (bRenderSelection)
        {
            const plWChar* text_selected_begin = text_begin + pl_min(ptState->tStb.select_start, ptState->tStb.select_end);
            const plWChar* text_selected_end = text_begin + pl_max(ptState->tStb.select_start, ptState->tStb.select_end);

            // ImU32 bg_color = GetColorU32(ImGuiCol_TextSelectedBg, bRenderCursor ? 1.0f : 0.6f); // FIXME: current code flow mandate that bRenderCursor is always true here, we are leaving the transparent one for tests.
            float bg_offy_up = bIsMultiLine ? 0.0f : -1.0f;    // FIXME: those offsets should be part of the style? they don't play so well with multi-line selection.
            float bg_offy_dn = bIsMultiLine ? 0.0f : 2.0f;
            plVec2 rect_pos = pl_sub_vec2(pl_add_vec2(draw_pos, select_start_offset), draw_scroll);
            for (const plWChar* p = text_selected_begin; p < text_selected_end; )
            {
                if (rect_pos.y > clip_rect.tMax.y + gptCtx->tStyle.fFontSize)
                    break;
                if (rect_pos.y < clip_rect.tMin.y)
                {
                    //p = (const plWChar*)wmemchr((const wchar_t*)p, '\n', text_selected_end - p);  // FIXME-OPT: Could use this when wchar_t are 16-bit
                    //p = p ? p + 1 : text_selected_end;
                    while (p < text_selected_end)
                        if (*p++ == '\n')
                            break;
                }
                else
                {
                    plVec2 rect_size = pl__input_text_calc_text_size_w(p, text_selected_end, &p, NULL, true);
                    if (rect_size.x <= 0.0f) rect_size.x = floorf(gptCtx->ptFont->sbGlyphs[gptCtx->ptFont->sbCodePoints[(plWChar)' ']].xAdvance * 0.50f); // So we can see selected empty lines
                    plRect rect = {
                        pl_add_vec2(rect_pos, (plVec2){0.0f, bg_offy_up - gptCtx->tStyle.fFontSize}), 
                        pl_add_vec2(rect_pos, (plVec2){rect_size.x, bg_offy_dn})
                    };
                    rect = pl_rect_clip(&rect, &clip_rect);
                    if (pl_rect_overlaps_rect(&rect, &clip_rect))
                        gptDraw->add_rect_filled(ptWindow->ptFgLayer, rect.tMin, rect.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
                }
                rect_pos.x = draw_pos.x - draw_scroll.x;
                rect_pos.y += gptCtx->tStyle.fFontSize;
            }
        }

        // We test for 'buf_display_max_length' as a way to avoid some pathological cases (e.g. single-line 1 MB string) which would make ImDrawList crash.
        if (bIsMultiLine || (pcBufferDisplayEnd - pcBufferDisplay) < iBufferDisplayMaxLength)
        {
            // ImU32 col = GetColorU32(bIsDisplayingHint ? ImGuiCol_TextDisabled : ImGuiCol_Text);
            gptDraw->add_text_ex(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, pl_sub_vec2(draw_pos, draw_scroll), gptCtx->tColorScheme.tTextCol, 
                pcBufferDisplay, pcBufferDisplayEnd, 0.0f);
            // draw_window->DrawList->AddText(g.Font, gptCtx->tStyle.fFontSize, draw_pos - draw_scroll, col, pcBufferDisplay, pcBufferDisplayEnd, 0.0f, bIsMultiLine ? NULL : &clip_rect);
        }

        // Draw blinking cursor
        if (bRenderCursor)
        {
            ptState->fCursorAnim += ptIo->fDeltaTime;
            // bool cursor_is_visible = (!g.IO.ConfigInputTextCursorBlink) || (ptState->fCursorAnim <= 0.0f) || fmodf(ptState->fCursorAnim, 1.20f) <= 0.80f;
            bool bCursorIsVisible = (ptState->fCursorAnim <= 0.0f) || fmodf(ptState->fCursorAnim, 1.20f) <= 0.80f;
            plVec2 cursor_screen_pos = pl_floor_vec2(pl_sub_vec2(pl_add_vec2(draw_pos, cursor_offset), draw_scroll));
            plRect cursor_screen_rect = {
                {cursor_screen_pos.x, cursor_screen_pos.y - gptCtx->tStyle.fFontSize + 0.5f},
                {cursor_screen_pos.x + 1.0f, cursor_screen_pos.y - 1.5f}
            };
            if (bCursorIsVisible && pl_rect_overlaps_rect(&cursor_screen_rect, &clip_rect))
                gptDraw->add_line(ptWindow->ptFgLayer, cursor_screen_rect.tMin, pl_rect_bottom_left(&cursor_screen_rect), gptCtx->tColorScheme.tTextCol, 1.0f);

            // Notify OS of text input position for advanced IME (-1 x offset so that Windows IME can cover our cursor. Bit of an extra nicety.)
            // if (!bIsReadOnly)
            // {
            //     g.PlatformImeData.WantVisible = true;
            //     g.PlatformImeData.InputPos = plVec2(cursor_screen_pos.x - 1.0f, cursor_screen_pos.y - gptCtx->tStyle.fFontSize);
            //     g.PlatformImeData.InputLineHeight = gptCtx->tStyle.fFontSize;
            // }
        }
    }
    else
    {
        // Render text only (no selection, no cursor)
        if (bIsMultiLine)
            text_size = (plVec2){tInnerSize.x, pl__input_text_calc_text_len_and_line_count(pcBufferDisplay, &pcBufferDisplayEnd) * gptCtx->tStyle.fFontSize}; // We don't need width
        else if (!bIsDisplayingHint && gptCtx->uActiveId == uHash)
            pcBufferDisplayEnd = pcBufferDisplay + ptState->iCurrentLengthA;
        else if (!bIsDisplayingHint)
            pcBufferDisplayEnd = pcBufferDisplay + strlen(pcBufferDisplay);

        if (bIsMultiLine || (pcBufferDisplayEnd - pcBufferDisplay) < iBufferDisplayMaxLength)
        {
            gptDraw->add_text_ex(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, draw_pos, gptCtx->tColorScheme.tTextCol, 
                pcBufferDisplay, pcBufferDisplayEnd, 0.0f);
        }
    }

    // if (bIsPassword && !bIsDisplayingHint)
    //     PopFont();

    if (bIsMultiLine)
    {
        // // For focus requests to work on our multiline we need to ensure our child ItemAdd() call specifies the ImGuiItemFlags_Inputable (ref issue #4761)...
        // Dummy(ImVec2(text_size.x, text_size.y + style.FramePadding.y));
        // ImGuiItemFlags backup_item_flags = g.CurrentItemFlags;
        // g.CurrentItemFlags |= ImGuiItemFlags_Inputable | ImGuiItemFlags_NoTabStop;
        // EndChild();
        // item_data_backup.StatusFlags |= (g.LastItemData.StatusFlags & ImGuiItemStatusFlags_HoveredWindow);
        // g.CurrentItemFlags = backup_item_flags;

        // // ...and then we need to undo the group overriding last item data, which gets a bit messy as EndGroup() tries to forward scrollbar being active...
        // // FIXME: This quite messy/tricky, should attempt to get rid of the child window.
        // EndGroup();
        // if (g.LastItemData.ID == 0)
        // {
        //     g.LastItemData.ID = id;
        //     g.LastItemData.InFlags = item_data_backup.InFlags;
        //     g.LastItemData.StatusFlags = item_data_backup.StatusFlags;
        // }
    }

    // if (pcLabel.x > 0)
    {
        // RenderText(ImVec2(frame_bb.Max.x + style.ItemInnerSpacing.x, frame_bb.Min.y + style.FramePadding.y), label);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tColorScheme.tTextCol, pcLabel, -1.0f);
    }

    // if (value_changed && !(flags & ImGuiInputTextFlags_NoMarkEdited))
    //     MarkItemEdited(id);

    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);

    if ((tFlags & PL_UI_INPUT_TEXT_FLAGS_ENTER_RETURNS_TRUE) != 0)
        return bValidated;
    else
        return bValueChanged;
}

bool
pl_ui_slider_float(const char* pcLabel, float* pfValue, float fMin, float fMax)
{
    return pl_ui_slider_float_f(pcLabel, pfValue, fMin, fMax, "%0.3f");
}

bool
pl_ui_slider_float_f(const char* pcLabel, float* pfValue, float fMin, float fMax, const char* pcFormat)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    const float fOriginalValue = *pfValue;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };
        *pfValue = pl_clampf(fMin, *pfValue, fMax);
        const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));

        char acTextBuffer[64] = {0};
        pl_sprintf(acTextBuffer, pcFormat, *pfValue);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl_ui_find_renderered_text_end(acTextBuffer, NULL), -1.0f);
        const plRect tLabelTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl_ui_find_renderered_text_end(pcLabel, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
        const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

        const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
        const plVec2 tTextStartPos = { 
            tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
            tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        const float fRange = fMax - fMin;
        const float fConv = fRange / (tSize.x - gptCtx->tStyle.fSliderSize);
        
        const plVec2 tGrabStartPos = {
            .x = tFrameStartPos.x + ((*pfValue) - fMin) / fConv,
            .y = tFrameStartPos.y
        };

        const plVec2 tGrabSize = { gptCtx->tStyle.fSliderSize, tWidgetSize.y};
        const plRect tGrabBox = pl_calculate_rect(tGrabStartPos, tGrabSize);
        bool bHovered = false;
        bool bHeld = false;
        const bool bPressed = pl_ui_button_behavior(&tGrabBox, uHash, &bHovered, &bHeld);

        const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);
        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgCol);

        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tGrabStartPos, tGrabBox.tMax, gptCtx->tColorScheme.tButtonCol);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tColorScheme.tTextCol, pcLabel, -1.0f);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, acTextBuffer, -1.0f);

        bool bDragged = false;
        if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            *pfValue += pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fConv;
            *pfValue = pl_clampf(fMin, *pfValue, fMax);
            if(pl_get_mouse_pos().x < tBoundingBox.tMin.x) *pfValue = fMin;
            if(pl_get_mouse_pos().x > tBoundingBox.tMax.x) *pfValue = fMax;
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return fOriginalValue != *pfValue;
}

bool
pl_ui_slider_int(const char* pcLabel, int* piValue, int iMin, int iMax)
{
    return pl_ui_slider_int_f(pcLabel, piValue, iMin, iMax, "%d");
}

bool
pl_ui_slider_int_f(const char* pcLabel, int* piValue, int iMin, int iMax, const char* pcFormat)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    const int iOriginalValue = *piValue;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };

        *piValue = pl_clampi(iMin, *piValue, iMax);
        const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));
        const int iBlocks = iMax - iMin + 1;
        const int iBlock = *piValue - iMin;

        char acTextBuffer[64] = {0};
        pl_sprintf(acTextBuffer, pcFormat, *piValue);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl_ui_find_renderered_text_end(acTextBuffer, NULL), -1.0f);
        const plRect tLabelTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl_ui_find_renderered_text_end(pcLabel, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
        const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

        const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
        const plVec2 tTextStartPos = { 
            tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
            tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };
        const float fBlockLength = tSize.x / (float)iBlocks;
        
        const plVec2 tGrabStartPos = {
            .x = tFrameStartPos.x + (float)iBlock * fBlockLength,
            .y = tFrameStartPos.y
        };

        const plVec2 tGrabSize = { fBlockLength, tWidgetSize.y};
        const plRect tGrabBox = pl_calculate_rect(tGrabStartPos, tGrabSize);
        bool bHovered = false;
        bool bHeld = false;
        const bool bPressed = pl_ui_button_behavior(&tGrabBox, uHash, &bHovered, &bHeld);

        const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);
        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgCol);

        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tGrabStartPos, tGrabBox.tMax, gptCtx->tColorScheme.tButtonCol);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tColorScheme.tTextCol, pcLabel, -1.0f);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, acTextBuffer, -1.0f);

        bool bDragged = false;
        if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            const plVec2 tMousePos = pl_get_mouse_pos();

            if(tMousePos.x > tGrabBox.tMax.x)
                (*piValue)++;
            if(tMousePos.x < tGrabStartPos.x)
                (*piValue)--;

            *piValue = pl_clampi(iMin, *piValue, iMax);
        }
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return iOriginalValue != *piValue;
}

bool
pl_ui_drag_float(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax)
{
    return pl_ui_drag_float_f(pcLabel, pfValue, fSpeed, fMin, fMax, "%.3f");
}

bool
pl_ui_drag_float_f(const char* pcLabel, float* pfValue, float fSpeed, float fMin, float fMax, const char* pcFormat)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();
    const float fOriginalValue = *pfValue;
    if(pl__ui_should_render(&tStartPos, &tWidgetSize))
    {
        const plVec2 tFrameStartPos = {floorf(tStartPos.x + (tWidgetSize.x / 3.0f)), tStartPos.y };

        *pfValue = pl_clampf(fMin, *pfValue, fMax);
        const uint32_t uHash = pl_str_hash(pcLabel, 0, pl_sb_top(gptCtx->sbuIdStack));

        char acTextBuffer[64] = {0};
        pl_sprintf(acTextBuffer, pcFormat, *pfValue);
        const plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, acTextBuffer, pl_ui_find_renderered_text_end(acTextBuffer, NULL), -1.0f);
        const plRect tLabelTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tFrameStartPos, pcLabel, pl_ui_find_renderered_text_end(pcLabel, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);
        const plVec2 tLabelTextActualCenter = pl_rect_center(&tLabelTextBounding);

        const plVec2 tSize = { 2.0f * (tWidgetSize.x / 3.0f), tWidgetSize.y};
        const plVec2 tTextStartPos = { 
            tFrameStartPos.x + tFrameStartPos.x + (2.0f * (tWidgetSize.x / 3.0f)) / 2.0f - tTextActualCenter.x, 
            tFrameStartPos.y + tFrameStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };
        const plRect tBoundingBox = pl_calculate_rect(tFrameStartPos, tSize);

        bool bHovered = false;
        bool bHeld = false;
        const bool bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);

        if(gptCtx->uActiveId == uHash)       gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgActiveCol);
        else if(gptCtx->uHoveredId == uHash) gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgHoveredCol);
        else                                 gptDraw->add_rect_filled(ptWindow->ptFgLayer, tFrameStartPos, tBoundingBox.tMax, gptCtx->tColorScheme.tFrameBgCol);

        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, (plVec2){tStartPos.x, tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tLabelTextActualCenter.y}, gptCtx->tColorScheme.tTextCol, pcLabel, -1.0f);
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, acTextBuffer, -1.0f);

        bool bDragged = false;
        if(gptCtx->uActiveId == uHash && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
        {
            *pfValue = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f).x * fSpeed;
            *pfValue = pl_clampf(fMin, *pfValue, fMax);
        }
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
    return fOriginalValue != *pfValue;    
}

void
pl_ui_image(plTextureId tTexture, plVec2 tSize)
{
    pl_ui_image_ex(tTexture, tSize, (plVec2){0}, (plVec2){1.0f, 1.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, (plVec4){0});
}

void
pl_ui_image_ex(plTextureId tTexture, plVec2 tSize, plVec2 tUv0, plVec2 tUv1, plVec4 tTintColor, plVec4 tBorderColor)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    const plVec2 tStartPos = pl__ui_get_cursor_pos();

    const plVec2 tFinalPos = pl_add_vec2(tStartPos, tSize);

    if(!(tFinalPos.y < ptWindow->tPos.y || tStartPos.y > ptWindow->tPos.y + ptWindow->tFullSize.y))
    {

        gptDraw->add_image_ex(ptWindow->ptFgLayer, tTexture, tStartPos, tFinalPos, tUv0, tUv1, tTintColor);

        if(tBorderColor.a > 0.0f)
            gptDraw->add_rect(ptWindow->ptFgLayer, tStartPos, tFinalPos, tBorderColor, 1.0f);

    }
    pl_ui_advance_cursor(tSize.x, tSize.y);
}

bool
pl_ui_invisible_button(const char* pcText, plVec2 tSize)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    bool bPressed = false;
    if(!(tStartPos.y + tSize.y < ptWindow->tPos.y || tStartPos.y > ptWindow->tPos.y + ptWindow->tFullSize.y))
    {
        const uint32_t uHash = pl_str_hash(pcText, 0, pl_sb_top(gptCtx->sbuIdStack));
        const plRect tBoundingBox = pl_calculate_rect(tStartPos, tSize);

        bool bHovered = false;
        bool bHeld = false;
        bPressed = pl_ui_button_behavior(&tBoundingBox, uHash, &bHovered, &bHeld);
    }
    pl_ui_advance_cursor(tSize.x, tSize.y);
    return bPressed;  
}

void
pl_ui_dummy(plVec2 tSize)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    pl_ui_advance_cursor(tSize.x, tSize.y);
}

void
pl_ui_progress_bar(float fFraction, plVec2 tSize, const char* pcOverlay)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    const plVec2 tWidgetSize = pl_ui_calculate_item_size(pl_ui_get_frame_height());
    const plVec2 tStartPos   = pl__ui_get_cursor_pos();

    if(tSize.y == 0.0f) tSize.y = tWidgetSize.y;
    if(tSize.x < 0.0f) tSize.x = tWidgetSize.x;

    if(!(tStartPos.y + tSize.y < ptWindow->tPos.y || tStartPos.y > ptWindow->tPos.y + ptWindow->tFullSize.y))
    {

        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, tSize), gptCtx->tColorScheme.tFrameBgCol);
        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){tSize.x * fFraction, tSize.y}), gptCtx->tColorScheme.tProgressBarCol);

        const char* pcTextPtr = pcOverlay;
        
        if(pcOverlay == NULL)
        {
            static char acBuffer[32] = {0};
            pl_sprintf(acBuffer, "%.1f%%", 100.0f * fFraction);
            pcTextPtr = acBuffer;
        }

        const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcTextPtr, -1.0f);
        plRect tTextBounding = gptDraw->calculate_text_bb_ex(gptCtx->ptFont, gptCtx->tStyle.fFontSize, tStartPos, pcTextPtr, pl_ui_find_renderered_text_end(pcTextPtr, NULL), -1.0f);
        const plVec2 tTextActualCenter = pl_rect_center(&tTextBounding);

        plVec2 tTextStartPos = {
            .x = tStartPos.x + gptCtx->tStyle.tInnerSpacing.x + gptCtx->tStyle.tFramePadding.x + tSize.x * fFraction,
            .y = tStartPos.y + tStartPos.y + tWidgetSize.y / 2.0f - tTextActualCenter.y
        };

        if(tTextStartPos.x + tTextSize.x > tStartPos.x + tSize.x)
            tTextStartPos.x = tStartPos.x + tSize.x - tTextSize.x - gptCtx->tStyle.tInnerSpacing.x;

        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, tTextStartPos, gptCtx->tColorScheme.tTextCol, pcTextPtr, -1.0f);

        const bool bHovered = pl_is_mouse_hovering_rect(tStartPos, pl_add_vec2(tStartPos, tWidgetSize)) && ptWindow == gptCtx->ptHoveredWindow;
        gptCtx->tPrevItemData.bHovered = bHovered;
    }
    pl_ui_advance_cursor(tWidgetSize.x, tWidgetSize.y);
}

void
pl_ui_layout_dynamic(float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_DYNAMIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_DYNAMIC,
        .uColumns         = uWidgetCount,
        .fWidth           = 1.0f / (float)uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_static(float fHeight, float fWidth, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_STATIC,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_STATIC,
        .uColumns         = uWidgetCount,
        .fWidth           = fWidth
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_row_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_row_push(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptCurrentRow->fWidth = fWidth;
}

void
pl_ui_layout_row_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_row(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount, const float* pfSizesOrRatios)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_ARRAY,
        .uColumns         = uWidgetCount,
        .pfSizesOrRatios  = pfSizesOrRatios
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_template_begin(float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = fHeight,
        .tType            = PL_UI_LAYOUT_ROW_TYPE_NONE,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE,
        .uColumns         = 0,
        .uEntryStartIndex = pl_sb_size(ptWindow->sbtRowTemplateEntries)
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_template_push_dynamic(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uDynamicEntryCount++;
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = 0.0f;
    ptCurrentRow->uColumns++;
}

void
pl_ui_layout_template_push_variable(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->uVariableEntryCount++;
    ptCurrentRow->fWidth += fWidth;
    pl_sb_push(ptWindow->sbuTempLayoutIndexSort, ptCurrentRow->uColumns);
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_VARIABLE;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_ui_layout_template_push_static(float fWidth)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    ptCurrentRow->fWidth += fWidth;
    ptCurrentRow->uStaticEntryCount++;
    pl_sb_add(ptWindow->sbtRowTemplateEntries);
    pl_sb_back(ptWindow->sbtRowTemplateEntries).tType = PL_UI_LAYOUT_ROW_ENTRY_TYPE_STATIC;
    pl_sb_back(ptWindow->sbtRowTemplateEntries).fWidth = fWidth;
    ptCurrentRow->uColumns++;
    ptWindow->tTempData.fTempStaticWidth += fWidth;
    ptWindow->tTempData.fTempMinWidth += fWidth;
}

void
pl_ui_layout_template_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // total available width minus padding/spacing
    float fWidthAvailable = 0.0f;
    if(ptWindow->bScrollbarY)
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
    else
        fWidthAvailable = (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);

    // simplest cast, not enough room, so nothing left to distribute to dynamic widths
    if(ptWindow->tTempData.fTempMinWidth >= fWidthAvailable)
    {
        for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
        {
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
            if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                ptEntry->fWidth = 0.0f;
        }
    }
    else if((ptCurrentRow->uDynamicEntryCount + ptCurrentRow->uVariableEntryCount) != 0)
    {

        // sort large to small (slow bubble sort, should replace later)
        bool bSwapOccured = true;
        while(bSwapOccured)
        {
            if(ptCurrentRow->uVariableEntryCount == 0)
                break;
            bSwapOccured = false;
            for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount - 1; i++)
            {
                const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
                const uint32_t jj = ptWindow->sbuTempLayoutIndexSort[i + 1];
                
                plUiLayoutRowEntry* ptEntry0 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];
                plUiLayoutRowEntry* ptEntry1 = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + jj];

                if(ptEntry0->fWidth < ptEntry1->fWidth)
                {
                    ptWindow->sbuTempLayoutIndexSort[i] = jj;
                    ptWindow->sbuTempLayoutIndexSort[i + 1] = ii;
                    bSwapOccured = true;
                }
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {

            // dynamic entries appended to the end so they will be "sorted" from the get go
            for(uint32_t i = 0; i < ptCurrentRow->uColumns; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + i];
                if(ptEntry->tType == PL_UI_LAYOUT_ROW_ENTRY_TYPE_DYNAMIC)
                    pl_sb_push(ptWindow->sbuTempLayoutIndexSort, i);
            }
        }

        // organize into levels
        float fCurrentWidth = -10000.0f;
        for(uint32_t i = 0; i < ptCurrentRow->uVariableEntryCount; i++)
        {
            const uint32_t ii = ptWindow->sbuTempLayoutIndexSort[i];
            plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ii];

            if(ptEntry->fWidth == fCurrentWidth)
            {
                pl_sb_back(ptWindow->sbtTempLayoutSort).uCount++;
            }
            else
            {
                const plUiLayoutSortLevel tNewSortLevel = {
                    .fWidth      = ptEntry->fWidth,
                    .uCount      = 1,
                    .uStartIndex = i
                };
                pl_sb_push(ptWindow->sbtTempLayoutSort, tNewSortLevel);
                fCurrentWidth = ptEntry->fWidth;
            }
        }

        // add dynamic to the end
        if(ptCurrentRow->uDynamicEntryCount > 0)
        {
            const plUiLayoutSortLevel tInitialSortLevel = {
                .fWidth      = 0.0f,
                .uCount      = ptCurrentRow->uDynamicEntryCount,
                .uStartIndex = ptCurrentRow->uVariableEntryCount
            };
            pl_sb_push(ptWindow->sbtTempLayoutSort, tInitialSortLevel);
        }

        // calculate left over width
        float fExtraWidth = fWidthAvailable - ptWindow->tTempData.fTempMinWidth;

        // distribute to levels
        const uint32_t uLevelCount = pl_sb_size(ptWindow->sbtTempLayoutSort);
        if(uLevelCount == 1)
        {
            plUiLayoutSortLevel tCurrentSortLevel = pl_sb_pop(ptWindow->sbtTempLayoutSort);
            const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
            for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uCount; i++)
            {
                plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                ptEntry->fWidth += fDistributableWidth;
            }
        }
        else
        {
            while(fExtraWidth > 0.0f)
            {
                plUiLayoutSortLevel tCurrentSortLevel = pl_sb_pop(ptWindow->sbtTempLayoutSort);

                if(pl_sb_size(ptWindow->sbtTempLayoutSort) == 0) // final
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    break;
                }
                    
                const float fDelta = pl_sb_back(ptWindow->sbtTempLayoutSort).fWidth - tCurrentSortLevel.fWidth;
                const float fTotalOwed = fDelta * (float)tCurrentSortLevel.uCount;
                
                if(fTotalOwed < fExtraWidth) // perform operations
                {
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDelta;
                    }
                    pl_sb_back(ptWindow->sbtTempLayoutSort).uCount += tCurrentSortLevel.uCount;
                    fExtraWidth -= fTotalOwed;
                }
                else // do the best we can
                {
                    const float fDistributableWidth = fExtraWidth / (float)tCurrentSortLevel.uCount;
                    for(uint32_t i = tCurrentSortLevel.uStartIndex; i < tCurrentSortLevel.uStartIndex + tCurrentSortLevel.uCount; i++)
                    {
                        plUiLayoutRowEntry* ptEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptWindow->sbuTempLayoutIndexSort[i]];
                        ptEntry->fWidth += fDistributableWidth;
                    }
                    fExtraWidth = 0.0f;
                }
            }
        }

    }

    pl_sb_reset(ptWindow->sbuTempLayoutIndexSort);
    pl_sb_reset(ptWindow->sbtTempLayoutSort);
    ptWindow->tTempData.fTempMinWidth = 0.0f;
    ptWindow->tTempData.fTempStaticWidth = 0.0f;
}

void
pl_ui_layout_space_begin(plUiLayoutRowType tType, float fHeight, uint32_t uWidgetCount)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow tNewRow = {
        .fHeight          = fHeight,
        .fSpecifiedHeight = tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fHeight : 1.0f,
        .tType            = tType,
        .tSystemType      = PL_UI_LAYOUT_SYSTEM_TYPE_SPACE,
        .uColumns         = uWidgetCount
    };
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

void
pl_ui_layout_space_push(float fX, float fY, float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptCurrentRow->fHorizontalOffset = ptCurrentRow->tType == PL_UI_LAYOUT_ROW_TYPE_DYNAMIC ? fX * ptWindow->tSize.x : fX;
    ptCurrentRow->fVerticalOffset = fY * ptCurrentRow->fSpecifiedHeight;
    ptCurrentRow->fWidth = fWidth;
    ptCurrentRow->fHeight = fHeight * ptCurrentRow->fSpecifiedHeight;
}

void
pl_ui_layout_space_end(void)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;
    PL_ASSERT(ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_SPACE);
    ptWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, ptWindow->tTempData.tCursorMaxPos.x);
    ptWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight, ptWindow->tTempData.tCursorMaxPos.y);
    ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

    // temp
    plUiLayoutRow tNewRow = {0};
    ptWindow->tTempData.tCurrentLayoutRow = tNewRow;
}

bool
pl_ui_was_last_item_hovered(void)
{
    return gptCtx->tPrevItemData.bHovered;
}

bool
pl_ui_was_last_item_active(void)
{
    return gptCtx->tPrevItemData.bActive;
}

int
pl_ui_get_int(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return iDefaultValue;
    return ptIterator->iValue;
}

float
pl_ui_get_float(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return fDefaultValue;
    return ptIterator->fValue;
}

bool
pl_ui_get_bool(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return pl_ui_get_int(ptStorage, uKey, bDefaultValue ? 1 : 0) != 0;
}

void*
pl_ui_get_ptr(plUiStorage* ptStorage, uint32_t uKey)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if((ptIterator == pl_sb_end(ptStorage->sbtData)) || (ptIterator->uKey != uKey))
        return NULL;
    return ptIterator->pValue;    
}

int*
pl_ui_get_int_ptr(plUiStorage* ptStorage, uint32_t uKey, int iDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->iValue;
}

float*
pl_ui_get_float_ptr(plUiStorage* ptStorage, uint32_t uKey, float fDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->fValue;
}

bool*
pl_ui_get_bool_ptr(plUiStorage* ptStorage, uint32_t uKey, bool bDefaultValue)
{
    return (bool*)pl_ui_get_int_ptr(ptStorage, uKey, bDefaultValue ? 1 : 0);
}

void**
pl_ui_get_ptr_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pDefaultValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pDefaultValue}));
        ptIterator = &ptStorage->sbtData[uIndex];
    }    
    return &ptIterator->pValue;
}

void
pl_ui_set_int(plUiStorage* ptStorage, uint32_t uKey, int iValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .iValue = iValue}));
        return;
    }
    ptIterator->iValue = iValue;
}

void
pl_ui_set_float(plUiStorage* ptStorage, uint32_t uKey, float fValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .fValue = fValue}));
        return;
    }
    ptIterator->fValue = fValue;
}

void
pl_ui_set_bool(plUiStorage* ptStorage, uint32_t uKey, bool bValue)
{
    pl_ui_set_int(ptStorage, uKey, bValue ? 1 : 0);
}

void
pl_ui_set_ptr(plUiStorage* ptStorage, uint32_t uKey, void* pValue)
{
    plUiStorageEntry* ptIterator = pl_ui_lower_bound(ptStorage->sbtData, uKey);
    if(ptIterator == pl_sb_end(ptStorage->sbtData) || (ptIterator->uKey != uKey))
    {
        uint32_t uIndex = (uint32_t)((uintptr_t)ptIterator - (uintptr_t)ptStorage->sbtData) / (uint32_t)sizeof(plUiStorageEntry);
        pl_sb_insert(ptStorage->sbtData, uIndex, ((plUiStorageEntry){.uKey = uKey, .pValue = pValue}));
        return;
    }
    ptIterator->pValue = pValue;    
}

void
pl_ui_debug(bool* pbOpen)
{
    static bool bShowWindowOuterRect = false;
    static bool bShowWindowOuterClippedRect = false;
    static bool bShowWindowInnerRect = false;
    static bool bShowWindowInnerClipRect = false;

    if(pl_ui_begin_window("Pilot Light UI Metrics/Debugger", pbOpen, false))
    {

        plIOContext* ptIOCtx = pl_get_io_context();

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);

        pl_ui_separator();

        pl_ui_checkbox("Show Window Inner Rect", &bShowWindowInnerRect);
        pl_ui_checkbox("Show Window Inner Clip Rect", &bShowWindowInnerClipRect);
        pl_ui_checkbox("Show Window Outer Rect", &bShowWindowOuterRect);
        pl_ui_checkbox("Show Window Outer Rect Clipped", &bShowWindowOuterClippedRect);

        for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
        {
            const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

            if(ptWindow->bActive)
            {
                if(bShowWindowInnerRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerRect.tMin, ptWindow->tInnerRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowInnerClipRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tInnerClipRect.tMin, ptWindow->tInnerClipRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRect.tMin, ptWindow->tOuterRect.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);

                if(bShowWindowOuterClippedRect)
                    gptDraw->add_rect(gptCtx->ptDebugLayer, ptWindow->tOuterRectClipped.tMin, ptWindow->tOuterRectClipped.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
            }
        }
        
        pl_ui_separator();

        if(pl_ui_tree_node("Windows"))
        {
            for(uint32_t uWindowIndex = 0; uWindowIndex < pl_sb_size(gptCtx->sbptWindows); uWindowIndex++)
            {
                const plUiWindow* ptWindow = gptCtx->sbptWindows[uWindowIndex];

                if(pl_ui_tree_node(ptWindow->pcName))
                {
                    
                    if(pl_ui_tree_node("Draw Layers"))
                    {
                        if(pl_ui_tree_node_f("Foreground %d vtx, %d indices, %d cmds", ptWindow->ptFgLayer->vertexCount, pl_sb_size(ptWindow->ptFgLayer->sbIndexBuffer), pl_sb_size(ptWindow->ptFgLayer->sbCommandBuffer)))
                        {
                            for(uint32_t i = 0; i < pl_sb_size(ptWindow->ptFgLayer->sbCommandBuffer); i++)
                            {
                                const plDrawCommand* ptDrawCmd = &ptWindow->ptFgLayer->sbCommandBuffer[i];
                                if(pl_ui_tree_node_f("Cmd: %d tris, ClipRect(%0.1f, %0.1f)-(%0.1f, %0.1f)", ptDrawCmd->elementCount / 3, ptDrawCmd->tClip.tMin.x, ptDrawCmd->tClip.tMin.y, ptDrawCmd->tClip.tMax.x, ptDrawCmd->tClip.tMax.y))
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        gptDraw->add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                    pl_ui_tree_pop();
                                }
                                else
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        gptDraw->add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                }
                            }
                            
                            pl_ui_tree_pop(); 
                        }
                        if(pl_ui_tree_node_f("Background %d vtx, %d indices, %d cmds", ptWindow->ptBgLayer->vertexCount, pl_sb_size(ptWindow->ptBgLayer->sbIndexBuffer), pl_sb_size(ptWindow->ptBgLayer->sbCommandBuffer)))
                        {
                            for(uint32_t i = 0; i < pl_sb_size(ptWindow->ptBgLayer->sbCommandBuffer); i++)
                            {
                                const plDrawCommand* ptDrawCmd = &ptWindow->ptBgLayer->sbCommandBuffer[i];
                                if(pl_ui_tree_node_f("Cmd: %d tris, ClipRect(%0.1f, %0.1f)-(%0.1f, %0.1f)", ptDrawCmd->elementCount / 3, ptDrawCmd->tClip.tMin.x, ptDrawCmd->tClip.tMin.y, ptDrawCmd->tClip.tMax.x, ptDrawCmd->tClip.tMax.y))
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        gptDraw->add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                    pl_ui_tree_pop();
                                }
                                else
                                {
                                    if(pl_ui_was_last_item_hovered())
                                        gptDraw->add_rect(gptCtx->ptFgLayer, ptDrawCmd->tClip.tMin, ptDrawCmd->tClip.tMax, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 1.0f);
                                }
                            }
                            
                            pl_ui_tree_pop(); 
                        }
                        pl_ui_tree_pop(); 
                    }

                    pl_ui_text(" - Pos:          (%0.1f, %0.1f)", ptWindow->tPos.x, ptWindow->tPos.y);
                    pl_ui_text(" - Size:         (%0.1f, %0.1f)", ptWindow->tSize.x, ptWindow->tSize.y);
                    pl_ui_text(" - Content Size: (%0.1f, %0.1f)", ptWindow->tContentSize.x, ptWindow->tContentSize.y);
                    pl_ui_text(" - Min Size:     (%0.1f, %0.1f)", ptWindow->tMinSize.x, ptWindow->tMinSize.y);
                    pl_ui_text(" - Scroll:       (%0.1f/%0.1f, %0.1f/%0.1f)", ptWindow->tScroll.x, ptWindow->tScrollMax.x, ptWindow->tScroll.y, ptWindow->tScrollMax.y);
                    pl_ui_text(" - Active:       %s", ptWindow->uId == gptCtx->uActiveWindowId ? "1" : "0");
                    pl_ui_text(" - Hovered:      %s", ptWindow == gptCtx->ptHoveredWindow ? "1" : "0");
                    pl_ui_text(" - Dragging:     %s", ptWindow == gptCtx->ptMovingWindow ? "1" : "0");
                    pl_ui_text(" - Scrolling:    %s", ptWindow == gptCtx->ptWheelingWindow ? "1" : "0");
                    pl_ui_text(" - Resizing:     %s", ptWindow == gptCtx->ptSizingWindow ? "1" : "0");
                    pl_ui_text(" - Collapsed:    %s", ptWindow->bCollapsed ? "1" : "0");
                    pl_ui_text(" - Auto Sized:   %s", ptWindow->tFlags &  PL_UI_WINDOW_FLAGS_AUTO_SIZE ? "1" : "0");

                    pl_ui_tree_pop();
                }  
            }
            pl_ui_tree_pop();
        }
        if(pl_ui_tree_node("Internal State"))
        {
            pl_ui_text("Windows");
            pl_ui_indent(0.0f);
            pl_ui_text("Hovered Window: %s", gptCtx->ptHoveredWindow ? gptCtx->ptHoveredWindow->pcName : "NULL");
            pl_ui_text("Moving Window:  %s", gptCtx->ptMovingWindow ? gptCtx->ptMovingWindow->pcName : "NULL");
            pl_ui_text("Sizing Window:  %s", gptCtx->ptSizingWindow ? gptCtx->ptSizingWindow->pcName : "NULL");
            pl_ui_text("Scrolling Window:  %s", gptCtx->ptScrollingWindow ? gptCtx->ptScrollingWindow->pcName : "NULL");
            pl_ui_text("Wheeling Window:  %s", gptCtx->ptWheelingWindow ? gptCtx->ptWheelingWindow->pcName : "NULL");
            pl_ui_unindent(0.0f);
            pl_ui_text("Items");
            pl_ui_indent(0.0f);
            pl_ui_text("Active ID:      %u", gptCtx->uActiveId);
            pl_ui_text("Hovered ID:     %u", gptCtx->uHoveredId);
            pl_ui_unindent(0.0f);
            pl_ui_tree_pop();
        }
        pl_ui_end_window();
    } 
}

void
pl_ui_style(bool* pbOpen)
{

    if(pl_ui_begin_window("Pilot Light UI Style", pbOpen, false))
    {

        const float pfRatios[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);

        plUiStyle* ptStyle = &gptCtx->tStyle;

        if(pl_ui_begin_tab_bar("Tabs"))
        {
            if(pl_ui_begin_tab("Colors"))
            { 
                pl_ui_end_tab();
            }
            
            if(pl_ui_begin_tab("Sizes"))
            {
                pl_ui_vertical_spacing();
                pl_ui_text("Title");
                pl_ui_slider_float("Title Padding", &ptStyle->fTitlePadding, 0.0f, 32.0f);

                pl_ui_vertical_spacing();
                pl_ui_text("Window");
                pl_ui_slider_float("Horizontal Padding## window", &ptStyle->fWindowHorizontalPadding, 0.0f, 32.0f);
                pl_ui_slider_float("Vertical Padding## window", &ptStyle->fWindowVerticalPadding, 0.0f, 32.0f);

                pl_ui_vertical_spacing();
                pl_ui_text("Scrollbar");
                pl_ui_slider_float("Size##scrollbar", &ptStyle->fScrollbarSize, 0.0f, 32.0f);
                
                pl_ui_vertical_spacing();
                pl_ui_text("Misc");
                pl_ui_slider_float("Indent", &ptStyle->fIndentSize, 0.0f, 32.0f); 
                pl_ui_slider_float("Slider Size", &ptStyle->fSliderSize, 3.0f, 32.0f); 
                pl_ui_slider_float("Font Size", &ptStyle->fFontSize, 13.0f, 48.0f); 
                pl_ui_end_tab();
            }
            pl_ui_end_tab_bar();
        }     
        pl_ui_end_window();
    }  
}

void
pl_ui_demo(bool* pbOpen)
{
    if(pl_ui_begin_window("UI Demo", pbOpen, false))
    {

        static const float pfRatios0[] = {1.0f};
        pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios0);

        pl_ui_text("Pilot Light UI v%s", PL_UI_VERSION);

        if(pl_ui_collapsing_header("Help"))
        {
            pl_ui_text("Under construction");
            pl_ui_end_collapsing_header();
        }
    
        if(pl_ui_collapsing_header("Window Options"))
        {
            pl_ui_text("Under construction");
            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Widgets"))
        {
            if(pl_ui_tree_node("Basic"))
            {

                pl_ui_layout_static(0.0f, 100, 2);
                pl_ui_button("Button");
                pl_ui_checkbox("Checkbox", NULL);

                pl_ui_layout_dynamic(0.0f, 2);
                pl_ui_button("Button");
                pl_ui_checkbox("Checkbox", NULL);

                pl_ui_layout_dynamic(0.0f, 1);
                static char buff[64] = {'c', 'a', 'a'};
                pl_ui_input_text("label 0", buff, 64);
                static char buff2[64] = {'c', 'c', 'c'};
                pl_ui_input_text_hint("label 1", "hint", buff2, 64);

                static float fValue = 3.14f;
                static int iValue117 = 117;

                pl_ui_input_float("label 2", &fValue, "%0.3f");
                pl_ui_input_int("label 3", &iValue117);

                static int iValue = 0;
                pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

                pl_ui_layout_row_push(0.33f);
                pl_ui_radio_button("Option 1", &iValue, 0);

                pl_ui_layout_row_push(0.33f);
                pl_ui_radio_button("Option 2", &iValue, 1);

                pl_ui_layout_row_push(0.34f);
                pl_ui_radio_button("Option 3", &iValue, 2);

                pl_ui_layout_row_end();

                const float pfRatios[] = {1.0f};
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
                pl_ui_separator();
                pl_ui_labeled_text("Label", "Value");
                static int iValue1 = 0;
                static float fValue1 = 23.0f;
                static float fValue2 = 100.0f;
                static int iValue2 = 3;
                pl_ui_slider_float("float slider 1", &fValue1, 0.0f, 100.0f);
                pl_ui_slider_float("float slider 2", &fValue2, -50.0f, 100.0f);
                pl_ui_slider_int("int slider 1", &iValue1, 0, 10);
                pl_ui_slider_int("int slider 2", &iValue2, -5, 10);
                pl_ui_drag_float("float drag", &fValue2, 1.0f, -100.0f, 100.0f);

                const float pfRatios22[] = {100.0f, 120.0f};
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios22);
                pl_ui_button("Hover me!");
                if(pl_ui_was_last_item_hovered())
                {
                    pl_ui_begin_tooltip();
                    pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios22);
                    pl_ui_text("I'm a tooltip!");
                    pl_ui_end_tooltip();
                }
                pl_ui_button("Just a button");

                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Selectables"))
            {
                static bool bSelectable0 = false;
                static bool bSelectable1 = false;
                static bool bSelectable2 = false;
                pl_ui_selectable("Selectable 1", &bSelectable0);
                pl_ui_selectable("Selectable 2", &bSelectable1);
                pl_ui_selectable("Selectable 3", &bSelectable2);
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Plotting"))
            {
                pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Trees"))
            {
                
                if(pl_ui_tree_node("Root Node"))
                {
                    if(pl_ui_tree_node("Child 1"))
                    {
                        pl_ui_button("Press me");
                        pl_ui_tree_pop();
                    }
                    if(pl_ui_tree_node("Child 2"))
                    {
                        pl_ui_button("Press me");
                        pl_ui_tree_pop();
                    }
                    pl_ui_tree_pop();
                }
                pl_ui_tree_pop();
            }

            if(pl_ui_tree_node("Tabs"))
            {
                if(pl_ui_begin_tab_bar("Tabs1"))
                {
                    if(pl_ui_begin_tab("Tab 0"))
                    {
                        static bool bSelectable0 = false;
                        static bool bSelectable1 = false;
                        static bool bSelectable2 = false;
                        pl_ui_selectable("Selectable 1", &bSelectable0);
                        pl_ui_selectable("Selectable 2", &bSelectable1);
                        pl_ui_selectable("Selectable 3", &bSelectable2);
                        pl_ui_end_tab();
                    }

                    if(pl_ui_begin_tab("Tab 1"))
                    {
                        static int iValue = 0;
                        pl_ui_radio_button("Option 1", &iValue, 0);
                        pl_ui_radio_button("Option 2", &iValue, 1);
                        pl_ui_radio_button("Option 3", &iValue, 2);
                        pl_ui_end_tab();
                    }

                    if(pl_ui_begin_tab("Tab 2"))
                    {
                        if(pl_ui_begin_child("CHILD2"))
                        {
                            const float pfRatios3[] = {600.0f};
                            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                            for(uint32_t i = 0; i < 25; i++)
                                pl_ui_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");
                            pl_ui_end_child();
                        }
                        
                        pl_ui_end_tab();
                    }
                    pl_ui_end_tab_bar();
                }
                pl_ui_tree_pop();
            }
            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Layout & Scrolling"))
        {
            const float pfRatios2[] = {0.5f, 0.50f};
            const float pfRatios3[] = {600.0f};

            pl_ui_layout_static(0.0f, 100, 1);
            static bool bUseClipper = true;
            pl_ui_checkbox("Use Clipper", &bUseClipper);
            
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 300.0f, 2, pfRatios2);
            if(pl_ui_begin_child("CHILD"))
            {

                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 2, pfRatios2);


                if(bUseClipper)
                {
                    plUiClipper tClipper = {1000000};
                    while(pl_ui_step_clipper(&tClipper))
                    {
                        for(uint32_t i = tClipper.uDisplayStart; i < tClipper.uDisplayEnd; i++)
                        {
                            pl_ui_text("%u Label", i);
                            pl_ui_text("%u Value", i);
                        } 
                    }
                }
                else
                {
                    for(uint32_t i = 0; i < 1000000; i++)
                    {
                            pl_ui_text("%u Label", i);
                            pl_ui_text("%u Value", i);
                    }
                }


                pl_ui_end_child();
            }
            

            if(pl_ui_begin_child("CHILD2"))
            {
                pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 1, pfRatios3);

                for(uint32_t i = 0; i < 25; i++)
                    pl_ui_text("Long text is happening11111111111111111111111111111111111111111111111111111111123456789");

                pl_ui_end_child();
            }
            

            pl_ui_end_collapsing_header();
        }


        if(pl_ui_collapsing_header("Testing 0"))
        {
            // first row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 60!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 40");

            pl_ui_layout_row_end();

            // space
            pl_ui_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 500.0f, UINT32_MAX);

            pl_ui_layout_space_push(0.0f, 0.0f, 100.0f, 100.0f);
            pl_ui_button("Hover me A!");

            pl_ui_layout_space_push(105.0f, 105.0f, 100.0f, 100.0f);
            pl_ui_button("Hover me B!");

            pl_ui_layout_space_end();

            // space
            pl_ui_layout_space_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 500.0f, 2);

            pl_ui_layout_space_push(0.0f, 0.0f, 0.5f, 0.1f);
            pl_ui_button("Hover me AA!");

            pl_ui_layout_space_push(0.25f, 0.50f, 0.5f, 0.1f);
            pl_ui_button("Hover me BB!");

            pl_ui_layout_space_end();

            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Testing 1"))
        {
            // first row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 60.0f, 2);

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 60!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 40");

            pl_ui_layout_row_end();

            // second row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_STATIC, 100.0f, 3);

            pl_ui_layout_row_push(100.0f);
            pl_ui_button("Hover me 100!");

            pl_ui_layout_row_push(200.0f);
            pl_ui_button("Hover me 200");

            pl_ui_layout_row_push(300.0f);
            pl_ui_button("Hover me 300");

            pl_ui_layout_row_end();


            // third row
            pl_ui_layout_row_begin(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3);

            pl_ui_layout_row_push(0.33f);
            pl_ui_button("Hover me 100!");

            pl_ui_layout_row_push(0.33f);
            pl_ui_button("Hover me 200");

            pl_ui_layout_row_push(0.34f);
            pl_ui_button("Hover me 300");

            pl_ui_layout_row_end();

            // fourth & fifth row
            const float pfRatios33[] = {0.25f, 0.25f, 0.50f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 3, pfRatios33);

            // row 4
            pl_ui_button("Hover me 100a!");
            pl_ui_button("Hover me 200a");
            pl_ui_button("Hover me 300a");

            // row 5
            pl_ui_button("Hover me 100b!");
            pl_ui_button("Hover me 200b");
            pl_ui_button("Hover me 300b");

            // sixth
            const float pfRatios2[] = {100.0f, 100.0f};
            pl_ui_layout_row(PL_UI_LAYOUT_ROW_TYPE_STATIC, 0.0f, 2, pfRatios2);

            // row 6
            pl_ui_button("Hover me 100c!");
            pl_ui_button("Hover me 200c");

            pl_ui_end_collapsing_header();
        }

        if(pl_ui_collapsing_header("Testing 2"))
        {
            pl_ui_layout_template_begin(60.0f);
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_end();

            pl_ui_button("static");
            pl_ui_button("variable");
            pl_ui_button("dynamic");

            pl_ui_button("static");
            pl_ui_button("variable");
            pl_ui_button("dynamic");

            pl_ui_layout_template_begin(30.0f);
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_end();

            pl_ui_button("static");
            pl_ui_button("dynamic");
            pl_ui_button("variable");
            
            pl_ui_button("static##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("variable##11");

            pl_ui_layout_template_begin(0.0f);
            pl_ui_layout_template_push_variable(500.0f);
            pl_ui_layout_template_push_dynamic();
            pl_ui_layout_template_push_static(100.0f);
            pl_ui_layout_template_end();

            
            pl_ui_button("variable##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("static##11");

            pl_ui_button("variable##11");
            pl_ui_button("dynamic##11");
            pl_ui_button("static##11");

            pl_ui_end_collapsing_header();
        }
        pl_ui_end_window();
    }
}

const char*
pl_ui_find_renderered_text_end(const char* pcText, const char* pcTextEnd)
{
    const char* pcTextDisplayEnd = pcText;
    if (!pcTextEnd)
        pcTextEnd = (const char*)-1;

    while (pcTextDisplayEnd < pcTextEnd && *pcTextDisplayEnd != '\0' && (pcTextDisplayEnd[0] != '#' || pcTextDisplayEnd[1] != '#'))
        pcTextDisplayEnd++;
    return pcTextDisplayEnd;
}

bool
pl_ui_button_behavior(const plRect* ptBox, uint32_t uHash, bool* pbOutHovered, bool* pbOutHeld)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    gptCtx->tPrevItemData.bActive = false;

    bool bPressed = false;
    bool bHovered = pl_ui_is_item_hoverable(ptBox, uHash) && (gptCtx->uActiveId == uHash || gptCtx->uActiveId == 0);

    if(bHovered)
        gptCtx->uNextHoveredId = uHash;

    bool bHeld = bHovered && pl_is_mouse_down(PL_MOUSE_BUTTON_LEFT);

    if(uHash == gptCtx->uActiveId)
    {  
        gptCtx->tPrevItemData.bActive = true;

        if(bHeld)
            gptCtx->uNextActiveId = uHash;
    }

    if(bHovered)
    {
        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->uNextActiveId = uHash;
            gptCtx->tPrevItemData.bActive = true;
        }
        else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
        {
            gptCtx->uNextActiveId = 0;
            bPressed = uHash == gptCtx->uActiveId;
        }
    }

    *pbOutHeld = bHeld;
    *pbOutHovered = bHovered;
    gptCtx->tPrevItemData.bHovered = bHovered;
    return bPressed;
}

bool
pl_ui_is_item_hoverable(const plRect* ptBox, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    if(gptCtx->uActiveId != 0 && gptCtx->uActiveId != uHash)
        return false;

    if(!pl_is_mouse_hovering_rect(ptBox->tMin, ptBox->tMax))
        return false;

    return true;
}

bool
pl_ui_is_item_hoverable_circle(plVec2 tP, float fRadius, uint32_t uHash)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;

    if(gptCtx->uHoveredId != 0 && gptCtx->uHoveredId != uHash)
        return false;

    if(gptCtx->ptHoveredWindow != gptCtx->ptCurrentWindow)
        return false;

    if(gptCtx->uActiveId != 0 && gptCtx->uActiveId != uHash)
        return false;

    if(!pl_ui_does_circle_contain_point(tP, fRadius, pl_get_mouse_pos()))
        return false;

    return true;
}

void
pl_ui_add_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x), roundf(tP.y)}, tColor, pcText, pl_ui_find_renderered_text_end(pcText, pcTextEnd), fWrap);
}

void
pl_ui_add_clipped_text(plDrawLayer* ptLayer, plFont* ptFont, float fSize, plVec2 tP, plVec2 tMin, plVec2 tMax, plVec4 tColor, const char* pcText, float fWrap)
{
    const char* pcTextEnd = pcText + strlen(pcText);
    gptDraw->add_text_clipped_ex(ptLayer, ptFont, fSize, (plVec2){roundf(tP.x + 0.5f), roundf(tP.y + 0.5f)}, tMin, tMax, tColor, pcText, pl_ui_find_renderered_text_end(pcText, pcTextEnd), fWrap);   
}

plVec2
pl_ui_calculate_text_size(plFont* font, float size, const char* text, float wrap)
{
    const char* pcTextEnd = text + strlen(text);
    return gptDraw->calculate_text_size_ex(font, size, text, pl_ui_find_renderered_text_end(text, pcTextEnd), wrap);  
}

bool
pl_ui_does_triangle_contain_point(plVec2 p0, plVec2 p1, plVec2 p2, plVec2 point)
{
    bool b1 = ((point.x - p1.x) * (p0.y - p1.y) - (point.y - p1.y) * (p0.x - p1.x)) < 0.0f;
    bool b2 = ((point.x - p2.x) * (p1.y - p2.y) - (point.y - p2.y) * (p1.x - p2.x)) < 0.0f;
    bool b3 = ((point.x - p0.x) * (p2.y - p0.y) - (point.y - p0.y) * (p2.x - p0.x)) < 0.0f;
    return ((b1 == b2) && (b2 == b3));
}

plUiStorageEntry*
pl_ui_lower_bound(plUiStorageEntry* sbtData, uint32_t uKey)
{
    plUiStorageEntry* ptFirstEntry = sbtData;
    uint32_t uCount = pl_sb_size(sbtData);
    while (uCount > 0)
    {
        uint32_t uCount2 = uCount >> 1;
        plUiStorageEntry* ptMiddleEntry = ptFirstEntry + uCount2;
        if(ptMiddleEntry->uKey < uKey)
        {
            ptFirstEntry = ++ptMiddleEntry;
            uCount -= uCount2 + 1;
        }
        else
            uCount = uCount2;
    }

    return ptFirstEntry;
}

bool
pl_ui_begin_window_ex(const char* pcName, bool* pbOpen, plUiWindowFlags tFlags)
{
    plUiWindow* ptWindow = NULL;                          // window we are working on
    plUiWindow* ptParentWindow = gptCtx->ptCurrentWindow; // parent window if there any

    // generate hashed ID
    const uint32_t uWindowID = pl_str_hash(pcName, 0, ptParentWindow ? pl_sb_top(gptCtx->sbuIdStack) : 0);
    pl_sb_push(gptCtx->sbuIdStack, uWindowID);

    // title text & title bar sizes
    const plVec2 tTextSize = pl_ui_calculate_text_size(gptCtx->ptFont, gptCtx->tStyle.fFontSize, pcName, 0.0f);
    const float fTitleBarHeight = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    // see if window already exist in storage
    ptWindow = pl_ui_get_ptr(&gptCtx->tWindows, uWindowID);

    // new window needs to be created
    if(ptWindow == NULL)
    {
        // allocate new window
        ptWindow = PL_ALLOC(sizeof(plUiWindow));
        memset(ptWindow, 0, sizeof(plUiWindow));
        ptWindow->uId                     = uWindowID;
        ptWindow->pcName                  = pcName;
        ptWindow->tPos                    = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMinSize                = (plVec2){ 200.0f, 200.0f};
        ptWindow->tMaxSize                = (plVec2){ 10000.0f, 10000.0f};
        ptWindow->tSize                   = (plVec2){ 500.0f, 500.0f};
        ptWindow->ptBgLayer               = gptDraw->request_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->ptFgLayer               = gptDraw->request_layer(gptCtx->ptDrawlist, pcName);
        ptWindow->tPosAllowableFlags      = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tSizeAllowableFlags     = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->tCollapseAllowableFlags = PL_UI_COND_ALWAYS | PL_UI_COND_ONCE;
        ptWindow->ptParentWindow          = (tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? ptParentWindow : ptWindow;
        ptWindow->uFocusOrder             = pl_sb_size(gptCtx->sbptFocusedWindows);
        ptWindow->tFlags                  = PL_UI_WINDOW_FLAGS_NONE;

        // add to focused windows
        if(!(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW))
        {
            pl_sb_push(gptCtx->sbptFocusedWindows, ptWindow);
            ptWindow->ptRootWindow = ptWindow;
        }
        else
            ptWindow->ptRootWindow = ptParentWindow->ptRootWindow;

        // add window to storage
        pl_ui_set_ptr(&gptCtx->tWindows, uWindowID, ptWindow);
    }

    // seen this frame (obviously)
    ptWindow->bActive = true;
    ptWindow->tFlags = tFlags;

    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {

        plUiLayoutRow* ptCurrentRow = &ptParentWindow->tTempData.tCurrentLayoutRow;
        const plVec2 tStartPos   = pl__ui_get_cursor_pos();

        // set window position to parent window current cursor
        ptWindow->tPos = tStartPos;

        pl_sb_push(ptParentWindow->sbtChildWindows, ptWindow);
    }

    // reset per frame window temporary data
    memset(&ptWindow->tTempData, 0, sizeof(plUiTempWindowData));
    pl_sb_reset(ptWindow->sbtChildWindows);
    pl_sb_reset(ptWindow->sbtRowStack);
    pl_sb_reset(ptWindow->sbtRowTemplateEntries);

    // clamp window size to min/max
    ptWindow->tSize = pl_clamp_vec2(ptWindow->tMinSize, ptWindow->tSize, ptWindow->tMaxSize);

    // should window collapse
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_COLLAPSED)
    {
        if(ptWindow->tCollapseAllowableFlags & gptCtx->tNextWindowData.tCollapseCondition)
        {
            ptWindow->bCollapsed = true;
            ptWindow->tCollapseAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    // position & size
    const plVec2 tMousePos = pl_get_mouse_pos();
    plVec2 tStartPos = ptWindow->tPos;

    // next window calls
    bool bWindowSizeSet = false;
    bool bWindowPosSet = false;
    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_POS)
    {
        bWindowPosSet = ptWindow->tPosAllowableFlags & gptCtx->tNextWindowData.tPosCondition;
        if(bWindowPosSet)
        {
            tStartPos = gptCtx->tNextWindowData.tPos;
            ptWindow->tPos = tStartPos;
            ptWindow->tPosAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(gptCtx->tNextWindowData.tFlags & PL_NEXT_WINDOW_DATA_FLAGS_HAS_SIZE)
    {
        bWindowSizeSet = ptWindow->tSizeAllowableFlags & gptCtx->tNextWindowData.tSizeCondition;
        if(bWindowSizeSet)
        {
            ptWindow->tSize = gptCtx->tNextWindowData.tSize;
            if(ptWindow->tSize.x < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.x = -(ptWindow->tPos.x - ptParentWindow->tPos.x) + ptParentWindow->tSize.x - ptWindow->tSize.x - (ptParentWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            if(ptWindow->tSize.y < 0.0f && ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) 
                ptWindow->tSize.y = -(ptWindow->tPos.y - ptParentWindow->tPos.y) + ptParentWindow->tSize.y - ptWindow->tSize.y - (ptParentWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f) - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f);
            
            ptWindow->tSizeAllowableFlags &= ~PL_UI_COND_ONCE;
        }   
    }

    if(ptWindow->bCollapsed)
        ptWindow->tSize = (plVec2){ptWindow->tSize.x, fTitleBarHeight};

    // updating outer rect here but autosized windows do so again in pl_ui_end_window(..)
    ptWindow->tOuterRect = pl_calculate_rect(ptWindow->tPos, ptWindow->tSize);
    ptWindow->tOuterRectClipped = ptWindow->tOuterRect;
    ptWindow->tInnerRect = ptWindow->tOuterRect;

    // remove scrollbars from inner rect
    if(ptWindow->bScrollbarX)
        ptWindow->tInnerRect.tMax.y -= gptCtx->tStyle.fScrollbarSize + 2.0f;
    if(ptWindow->bScrollbarY)
        ptWindow->tInnerRect.tMax.x -= gptCtx->tStyle.fScrollbarSize + 2.0f;

    // decorations
    if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR))
    {

        ptWindow->tInnerRect.tMin.y += fTitleBarHeight;

        // draw title bar
        plVec4 tTitleColor;
        if(ptWindow->uId == gptCtx->uActiveWindowId)
            tTitleColor = gptCtx->tColorScheme.tTitleActiveCol;
        else if(ptWindow->bCollapsed)
            tTitleColor = gptCtx->tColorScheme.tTitleBgCollapsedCol;
        else
            tTitleColor = gptCtx->tColorScheme.tTitleBgCol;
        gptDraw->add_rect_filled(ptWindow->ptFgLayer, tStartPos, pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x, fTitleBarHeight}), tTitleColor);

        // draw title text
        const plVec2 titlePos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x / 2.0f - tTextSize.x / 2.0f, gptCtx->tStyle.fTitlePadding});
        pl_ui_add_text(ptWindow->ptFgLayer, gptCtx->ptFont, gptCtx->tStyle.fFontSize, titlePos, gptCtx->tColorScheme.tTextCol, pcName, 0.0f);

        // draw close button
        const float fTitleBarButtonRadius = 8.0f;
        float fTitleButtonStartPos = fTitleBarButtonRadius * 2.0f;
        if(pbOpen)
        {
            plVec2 tCloseCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;
            if(pl_ui_does_circle_contain_point(tCloseCenterPos, fTitleBarButtonRadius, tMousePos) && gptCtx->ptHoveredWindow == ptWindow)
            {
                gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 12);
                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false)) gptCtx->uActiveId = 1;
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT)) *pbOpen = false;       
            }
            else
                gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCloseCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.0f, 0.0f, 1.0f}, 12);
        }

        if(!(tFlags & PL_UI_WINDOW_FLAGS_NO_COLLAPSE))
        {
            // draw collapse button
            plVec2 tCollapsingCenterPos = pl_add_vec2(tStartPos, (plVec2){ptWindow->tSize.x - fTitleButtonStartPos, fTitleBarHeight / 2.0f});
            fTitleButtonStartPos += fTitleBarButtonRadius * 2.0f + gptCtx->tStyle.tItemSpacing.x;

            if(pl_ui_does_circle_contain_point(tCollapsingCenterPos, fTitleBarButtonRadius, tMousePos) &&  gptCtx->ptHoveredWindow == ptWindow)
            {
                gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 12);

                if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
                {
                    gptCtx->uActiveId = 2;
                }
                else if(pl_is_mouse_released(PL_MOUSE_BUTTON_LEFT))
                {
                    ptWindow->bCollapsed = !ptWindow->bCollapsed;
                    if(!ptWindow->bCollapsed)
                    {
                        ptWindow->tSize = ptWindow->tFullSize;
                        if(tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE)
                            ptWindow->uHideFrames = 2;
                    }
                }
            }
            else
                gptDraw->add_circle_filled(ptWindow->ptFgLayer, tCollapsingCenterPos, fTitleBarButtonRadius, (plVec4){0.5f, 0.5f, 0.0f, 1.0f}, 12);
        }

    }

    // remove padding for inner clip rect
    ptWindow->tInnerClipRect = pl_rect_expand_vec2(&ptWindow->tInnerRect, (plVec2){-gptCtx->tStyle.fWindowHorizontalPadding, 0.0f});

    if(!ptWindow->bCollapsed)
    {
        const plVec2 tStartClip = { ptWindow->tPos.x, ptWindow->tPos.y + fTitleBarHeight };

        const plVec2 tInnerClip = { 
            ptWindow->tSize.x - (ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f),
            ptWindow->tSize.y - fTitleBarHeight - (ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f)
        };

        if(pl_sb_size(gptCtx->ptDrawlist->sbClipStack) > 0)
        {
            ptWindow->tInnerClipRect = pl_rect_clip_full(&ptWindow->tInnerClipRect, &pl_sb_back(gptCtx->ptDrawlist->sbClipStack));
            ptWindow->tOuterRectClipped = pl_rect_clip_full(&ptWindow->tOuterRectClipped, &pl_sb_back(gptCtx->ptDrawlist->sbClipStack));
        }
        gptDraw->push_clip_rect(gptCtx->ptDrawlist, ptWindow->tInnerClipRect, false);

    }

    // update cursors
    ptWindow->tTempData.tCursorStartPos.x = gptCtx->tStyle.fWindowHorizontalPadding + tStartPos.x - ptWindow->tScroll.x;
    ptWindow->tTempData.tCursorStartPos.y = gptCtx->tStyle.fWindowVerticalPadding + tStartPos.y + fTitleBarHeight - ptWindow->tScroll.y;
    ptWindow->tTempData.tRowPos = ptWindow->tTempData.tCursorStartPos;
    ptWindow->tTempData.tCursorStartPos = pl_floor_vec2(ptWindow->tTempData.tCursorStartPos);

    // reset next window flags
    gptCtx->tNextWindowData.tFlags = PL_NEXT_WINDOW_DATA_FLAGS_NONE;

    gptCtx->ptCurrentWindow = ptWindow;
    if(tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW)
    {
        ptWindow->bVisible = pl_rect_overlaps_rect(&ptWindow->tInnerClipRect, &ptParentWindow->tInnerClipRect);
        return ptWindow->bVisible && !pl_rect_is_inverted(&ptWindow->tInnerClipRect);
    }

    ptWindow->bVisible = true;
    return !ptWindow->bCollapsed;
}

void
pl_ui_render_scrollbar(plUiWindow* ptWindow, uint32_t uHash, plUiAxis tAxis)
{
    const plRect tParentBgRect = ptWindow->ptParentWindow->tOuterRect;
    if(tAxis == PL_UI_AXIS_X)
    {
        const float fRightSidePadding = ptWindow->bScrollbarY ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.x = pl_clampf(0.0f, ptWindow->tScroll.x, ptWindow->tScrollMax.x);

        const float fScrollbarHandleSize  = pl_maxf(5.0f, floorf((ptWindow->tSize.x - fRightSidePadding) * ((ptWindow->tSize.x - fRightSidePadding) / (ptWindow->tContentSize.x))));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.x - fRightSidePadding - fScrollbarHandleSize) * (ptWindow->tScroll.x/(ptWindow->tScrollMax.x)));
        
        const plVec2 tStartPos = pl_add_vec2(ptWindow->tPos, (plVec2){fScrollbarHandleStart, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f});
        
        plRect tScrollBackground = {
            pl_add_vec2(ptWindow->tPos, (plVec2){0.0f, ptWindow->tSize.y - gptCtx->tStyle.fScrollbarSize - 2.0f}),
            pl_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - fRightSidePadding, ptWindow->tSize.y - 2.0f})
        };

        if(pl_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {
            const plVec2 tFinalSize = {fScrollbarHandleSize, gptCtx->tStyle.fScrollbarSize};
            plRect tHandleBox = pl_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = pl_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = pl_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            gptDraw->add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);   
            if(gptCtx->uActiveId == uHash)
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash)
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tStartPos, pl_add_vec2(tStartPos, tFinalSize), gptCtx->tColorScheme.tScrollbarHandleCol);
        }
    }
    else if(tAxis == PL_UI_AXIS_Y)
    {
          
        const float fBottomPadding = ptWindow->bScrollbarX ? gptCtx->tStyle.fScrollbarSize + 2.0f : 0.0f;
        const float fTopPadding = (ptWindow->tFlags & PL_UI_WINDOW_FLAGS_CHILD_WINDOW) ? 0.0f : gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

        // this is needed if autosizing and parent changes sizes
        ptWindow->tScroll.y = pl_clampf(0.0f, ptWindow->tScroll.y, ptWindow->tScrollMax.y);

        const float fScrollbarHandleSize  = pl_maxf(5.0f, floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding) * ((ptWindow->tSize.y - fTopPadding - fBottomPadding) / (ptWindow->tContentSize.y))));
        const float fScrollbarHandleStart = floorf((ptWindow->tSize.y - fTopPadding - fBottomPadding - fScrollbarHandleSize) * (ptWindow->tScroll.y / (ptWindow->tScrollMax.y)));

        const plVec2 tStartPos = pl_add_vec2(ptWindow->tPos, (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding + fScrollbarHandleStart});
        
        plRect tScrollBackground = pl_calculate_rect(pl_add_vec2(ptWindow->tPos, 
            (plVec2){ptWindow->tSize.x - gptCtx->tStyle.fScrollbarSize - 2.0f, fTopPadding}),
            (plVec2){gptCtx->tStyle.fScrollbarSize, ptWindow->tSize.y - fBottomPadding});

        if(pl_rect_overlaps_rect(&tParentBgRect, &tScrollBackground))
        {    

            const plVec2 tFinalSize = {gptCtx->tStyle.fScrollbarSize, fScrollbarHandleSize};
            plRect tHandleBox = pl_calculate_rect(tStartPos, tFinalSize);
            tScrollBackground = pl_rect_clip(&tScrollBackground, &ptWindow->tOuterRectClipped);
            tHandleBox = pl_rect_clip(&tHandleBox, &ptWindow->tOuterRectClipped);

            // scrollbar background
            gptDraw->add_rect_filled(ptWindow->ptBgLayer, tScrollBackground.tMin, tScrollBackground.tMax, gptCtx->tColorScheme.tScrollbarBgCol);

            bool bHovered = false;
            bool bHeld = false;
            const bool bPressed = pl_ui_button_behavior(&tHandleBox, uHash, &bHovered, &bHeld);

            // scrollbar handle
            if(gptCtx->uActiveId == uHash) 
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarActiveCol);
            else if(gptCtx->uHoveredId == uHash) 
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHoveredCol);
            else
                gptDraw->add_rect_filled(ptWindow->ptBgLayer, tHandleBox.tMin, tHandleBox.tMax, gptCtx->tColorScheme.tScrollbarHandleCol);
        }
    }
}

plVec2
pl_ui_calculate_item_size(float fDefaultHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    float fHeight = ptCurrentRow->fHeight;

    if(fHeight == 0.0f)
        fHeight = fDefaultHeight;

    if(ptCurrentRow->tSystemType ==  PL_UI_LAYOUT_SYSTEM_TYPE_TEMPLATE)
    {
        const plUiLayoutRowEntry* ptCurrentEntry = &ptWindow->sbtRowTemplateEntries[ptCurrentRow->uEntryStartIndex + ptCurrentRow->uCurrentColumn];
        const plVec2 tWidgetSize = { ptCurrentEntry->fWidth, fHeight};
        return tWidgetSize;
    }
    else
    {
        // when passed array of sizes/ratios, override
        if(ptCurrentRow->pfSizesOrRatios)
            ptCurrentRow->fWidth = ptCurrentRow->pfSizesOrRatios[ptCurrentRow->uCurrentColumn];

        float fWidth = ptCurrentRow->fWidth;

        if(ptCurrentRow->tType ==  PL_UI_LAYOUT_ROW_TYPE_DYNAMIC) // width was a ratio
        {
            if(ptWindow->bScrollbarY)
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - 2.0f - gptCtx->tStyle.fScrollbarSize - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
            else
                fWidth *= (ptWindow->tSize.x - gptCtx->tStyle.fWindowHorizontalPadding * 2.0f  - gptCtx->tStyle.tItemSpacing.x * (float)(ptCurrentRow->uColumns - 1) - (float)gptCtx->ptCurrentWindow->tTempData.uTreeDepth * gptCtx->tStyle.fIndentSize);
        }

        const plVec2 tWidgetSize = { fWidth, fHeight};
        return tWidgetSize;
    }
}

void
pl_ui_advance_cursor(float fWidth, float fHeight)
{
    plUiWindow* ptWindow = gptCtx->ptCurrentWindow;
    plUiLayoutRow* ptCurrentRow = &ptWindow->tTempData.tCurrentLayoutRow;

    ptCurrentRow->uCurrentColumn++;
    
    ptCurrentRow->fMaxWidth = pl_maxf(ptCurrentRow->fHorizontalOffset + fWidth, ptCurrentRow->fMaxWidth);
    ptCurrentRow->fMaxHeight = pl_maxf(ptCurrentRow->fMaxHeight, ptCurrentRow->fVerticalOffset + fHeight);

    // not yet at end of row
    if(ptCurrentRow->uCurrentColumn < ptCurrentRow->uColumns)
        ptCurrentRow->fHorizontalOffset += fWidth + gptCtx->tStyle.tItemSpacing.x;

    // automatic wrap
    if(ptCurrentRow->uCurrentColumn == ptCurrentRow->uColumns && ptCurrentRow->tSystemType != PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        ptWindow->tTempData.tRowPos.y = ptWindow->tTempData.tRowPos.y + ptCurrentRow->fMaxHeight + gptCtx->tStyle.tItemSpacing.y;

        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x = pl_maxf(ptWindow->tTempData.tRowPos.x + ptCurrentRow->fMaxWidth, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.x);
        gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y = pl_maxf(ptWindow->tTempData.tRowPos.y, gptCtx->ptCurrentWindow->tTempData.tCursorMaxPos.y);   

        // reset
        ptCurrentRow->uCurrentColumn = 0;
        ptCurrentRow->fMaxWidth = 0.0f;
        ptCurrentRow->fMaxHeight = 0.0f;
        ptCurrentRow->fHorizontalOffset = ptCurrentRow->fRowStartX + ptWindow->tTempData.fExtraIndent;
        ptCurrentRow->fVerticalOffset = 0.0f;
    }

    // passed end of row
    if(ptCurrentRow->uCurrentColumn > ptCurrentRow->uColumns && ptCurrentRow->tSystemType == PL_UI_LAYOUT_SYSTEM_TYPE_ROW_XXX)
    {
        PL_ASSERT(false);
    }
}

void
pl_ui_submit_window(plUiWindow* ptWindow)
{
    ptWindow->bActive = false; // no longer active (for next frame)

    const size_t ulCurrentFrame = pl_get_io_context()->ulFrameCount;
    // const plVec2 tMousePos = pl_get_mouse_pos();
    const float fTitleBarHeight = gptCtx->tStyle.fFontSize + 2.0f * gptCtx->tStyle.fTitlePadding;

    const plRect tTitleBarHitRegion = {
        .tMin = {ptWindow->tPos.x + 2.0f, ptWindow->tPos.y + 2.0f},
        .tMax = {ptWindow->tPos.x + ptWindow->tSize.x - 2.0f, ptWindow->tPos.y + fTitleBarHeight}
    };

    plRect tBoundBox = ptWindow->tOuterRectClipped;

    // add padding for resizing from borders
    if(!(ptWindow->tFlags & (PL_UI_WINDOW_FLAGS_NO_RESIZE | PL_UI_WINDOW_FLAGS_AUTO_SIZE)))
        tBoundBox = pl_rect_expand(&tBoundBox, 2.0f);

    // check if window is hovered
    if(pl_is_mouse_hovering_rect(tBoundBox.tMin, tBoundBox.tMax))
    {
        gptCtx->ptHoveredWindow = ptWindow;
        gptCtx->bWantCaptureMouseNextFrame = true;

        // check if window is activated
        if(pl_is_mouse_clicked(PL_MOUSE_BUTTON_LEFT, false))
        {
            gptCtx->ptMovingWindow = NULL;
            gptCtx->uActiveWindowId = ptWindow->ptParentWindow->uId;
            gptCtx->bActiveIdJustActivated = true;
            gptCtx->ptActiveWindow = ptWindow->ptParentWindow;

            pl_get_io_context()->_abMouseOwned[PL_MOUSE_BUTTON_LEFT] = true;

            // check if window titlebar is clicked
            if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_NO_TITLE_BAR) && pl_is_mouse_hovering_rect(tTitleBarHitRegion.tMin, tTitleBarHitRegion.tMax))
                gptCtx->ptMovingWindow = ptWindow;
        }

        // scrolling
        if(!(ptWindow->tFlags & PL_UI_WINDOW_FLAGS_AUTO_SIZE) && pl_get_mouse_wheel() != 0.0f)
            gptCtx->ptWheelingWindow = ptWindow;
    }

    pl_sb_push(gptCtx->sbptWindows, ptWindow);
    for(uint32_t j = 0; j < pl_sb_size(ptWindow->sbtChildWindows); j++)
        pl_ui_submit_window(ptWindow->sbtChildWindows[j]);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ui_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
    plDataRegistryApiI* ptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(ptDataRegistry->get_data("memory"));
    pl_set_io_context(ptDataRegistry->get_data(PL_CONTEXT_IO_NAME));
    gptDraw = ptApiRegistry->first(PL_API_DRAW);
    plUiApiI* ptUiApi = pl_load_ui_api();
    if(bReload)
    {
        ptUiApi->set_context(ptDataRegistry->get_data("ui"));
        ptApiRegistry->replace(ptApiRegistry->first(PL_API_UI), ptUiApi);
    }
    else
    {
        ptApiRegistry->add(PL_API_UI, ptUiApi);
    }
}

PL_EXPORT void
pl_unload_ui_ext(plApiRegistryApiI* ptApiRegistry)
{
    
}

//-----------------------------------------------------------------------------
// [SECTION] internal
//-----------------------------------------------------------------------------

static bool
pl__input_text_filter_character(unsigned int* puChar, plUiInputTextFlags tFlags)
{
    //PL_ASSERT(input_source == ImGuiInputSource_Keyboard || input_source == ImGuiInputSource_Clipboard);
    unsigned int c = *puChar;

    // Filter non-printable (NB: isprint is unreliable! see #2467)
    bool apply_named_filters = true;
    if (c < 0x20)
    {
        bool pass = false;
        pass |= (c == '\n' && (tFlags & PL_UI_INPUT_TEXT_FLAGS_MULTILINE)); // Note that an Enter KEY will emit \r and be ignored (we poll for KEY in InputText() code)
        pass |= (c == '\t' && (tFlags & PL_UI_INPUT_TEXT_FLAGS_ALLOW_TAB_INPUT));
        if (!pass)
            return false;
        apply_named_filters = false; // Override named filters below so newline and tabs can still be inserted.
    }

    // if (input_source != ImGuiInputSource_Clipboard)
    {
        // We ignore Ascii representation of delete (emitted from Backspace on OSX, see #2578, #2817)
        if (c == 127)
            return false;

        // Filter private Unicode range. GLFW on OSX seems to send private characters for special keys like arrow keys (FIXME)
        if (c >= 0xE000 && c <= 0xF8FF)
            return false;
    }

    // Filter Unicode ranges we are not handling in this build
    if (c > PL_UNICODE_CODEPOINT_MAX)
        return false;

    // Generic named filters
    if (apply_named_filters && (tFlags & (PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL | PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL | PL_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE | PL_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK | PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC)))
    {
        // The libc allows overriding locale, with e.g. 'setlocale(LC_NUMERIC, "de_DE.UTF-8");' which affect the output/input of printf/scanf to use e.g. ',' instead of '.'.
        // The standard mandate that programs starts in the "C" locale where the decimal point is '.'.
        // We don't really intend to provide widespread support for it, but out of empathy for people stuck with using odd API, we support the bare minimum aka overriding the decimal point.
        // Change the default decimal_point with:
        //   ImGui::GetCurrentContext()->PlatformLocaleDecimalPoint = *localeconv()->decimal_point;
        // Users of non-default decimal point (in particular ',') may be affected by word-selection logic (pl__is_word_boundary_from_right/pl__is_word_boundary_from_left) functions.
        const unsigned c_decimal_point = '.';

        // Full-width -> half-width conversion for numeric fields (https://en.wikipedia.org/wiki/Halfwidth_and_Fullwidth_Forms_(Unicode_block)
        // While this is mostly convenient, this has the side-effect for uninformed users accidentally inputting full-width characters that they may
        // scratch their head as to why it works in numerical fields vs in generic text fields it would require support in the font.
        if (tFlags & (PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL | PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC | PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL))
            if (c >= 0xFF01 && c <= 0xFF5E)
                c = c - 0xFF01 + 0x21;

        // Allow 0-9 . - + * /
        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CHARS_DECIMAL)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/'))
                return false;

        // Allow 0-9 . - + * / e E
        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CHARS_SCIENTIFIC)
            if (!(c >= '0' && c <= '9') && (c != c_decimal_point) && (c != '-') && (c != '+') && (c != '*') && (c != '/') && (c != 'e') && (c != 'E'))
                return false;

        // Allow 0-9 a-F A-F
        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CHARS_HEXADECIMAL)
            if (!(c >= '0' && c <= '9') && !(c >= 'a' && c <= 'f') && !(c >= 'A' && c <= 'F'))
                return false;

        // Turn a-z into A-Z
        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CHARS_UPPERCASE)
            if (c >= 'a' && c <= 'z')
                c += (unsigned int)('A' - 'a');

        if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CHARS_NO_BLANK)
            if (pl__char_is_blank_w(c))
                return false;

        *puChar = c;
    }

    // Custom callback filter
    if (tFlags & PL_UI_INPUT_TEXT_FLAGS_CALLBACK_CHAR_FILTER)
    {
        // ImGuiContext& g = *GImGui;
        // ImGuiInputTextCallbackData callback_data;
        // callback_data.Ctx = &g;
        // callback_data.EventFlag = ImGuiInputTextFlags_CallbackCharFilter;
        // callback_data.EventChar = (ImWchar)c;
        // callback_data.Flags = tFlags;
        // callback_data.UserData = user_data;
        // if (callback(&callback_data) != 0)
        //     return false;
        // *p_char = callback_data.EventChar;
        // if (!callback_data.EventChar)
        //     return false;
    }

    return true;
}