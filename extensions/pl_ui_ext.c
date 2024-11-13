#include "pl_ui_ext.h"
#include "pl_ui_internal.h"

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

#include "pl_ui_core.c"
#include "pl_ui_widgets.c"
#include "pl_ui_demo.c"


static void
pl_load_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    const plUiI tApi = {
        .initialize                    = pl_ui_initialize,
        .cleanup                       = pl_ui_cleanup,
        .get_draw_list                 = pl_get_draw_list,
        .get_debug_draw_list           = pl_get_debug_draw_list,
        .new_frame                     = pl_new_frame,
        .end_frame                     = pl_end_frame,
        .wants_keyboard_capture        = pl_wants_keyboard_capture,
        .wants_text_input              = pl_wants_text_input,
        .wants_mouse_capture           = pl_wants_mouse_capture,
        .show_debug_window             = pl_show_debug_window,
        .show_style_editor_window      = pl_show_style_editor_window,
        .show_demo_window              = pl_show_demo_window,
        .set_dark_theme                = pl_set_dark_theme,
        .push_theme_color              = pl_push_theme_color,
        .pop_theme_color               = pl_pop_theme_color,
        .set_default_font              = pl_set_default_font,
        .get_default_font              = pl_get_default_font,
        .begin_window                  = pl_begin_window,
        .end_window                    = pl_end_window,
        .get_window_fg_drawlayer       = pl_get_window_fg_drawlayer,
        .get_window_bg_drawlayer       = pl_get_window_bg_drawlayer,
        .get_cursor_pos                = pl_get_cursor_pos,
        .begin_child                   = pl_begin_child,
        .end_child                     = pl_end_child,
        .is_popup_open                 = pl_is_popup_open,
        .close_current_popup           = pl_close_current_popup,
        .open_popup                    = pl_open_popup,
        .begin_popup                   = pl_begin_popup,
        .end_popup                     = pl_end_popup,
        .begin_tooltip                 = pl_begin_tooltip,
        .end_tooltip                   = pl_end_tooltip,
        .get_window_pos                = pl_get_window_pos,
        .get_window_size               = pl_get_window_size,
        .get_window_scroll             = pl_get_window_scroll,
        .get_window_scroll_max         = pl_get_window_scroll_max,
        .set_window_scroll             = pl_set_window_scroll,
        .set_next_window_pos           = pl_set_next_window_pos,
        .set_next_window_size          = pl_set_next_window_size,
        .set_next_window_collapse      = pl_set_next_window_collapse,
        .button                        = pl_button,
        .selectable                    = pl_selectable,
        .checkbox                      = pl_checkbox,
        .radio_button                  = pl_radio_button,
        .image                         = pl_image,
        .image_ex                      = pl_image_ex,
        .image_button                  = pl_image_button,
        .image_button_ex               = pl_image_button_ex,
        .invisible_button              = pl_invisible_button,
        .dummy                         = pl_dummy,
        .progress_bar                  = pl_progress_bar,
        .text                          = pl_text,
        .text_v                        = pl_text_v,
        .color_text                    = pl_color_text,
        .color_text_v                  = pl_color_text_v,
        .labeled_text                  = pl_labeled_text,
        .labeled_text_v                = pl_labeled_text_v,
        .begin_combo                   = pl_begin_combo,
        .end_combo                     = pl_end_combo,
        .input_text                    = pl_input_text,
        .input_text_hint               = pl_input_text_hint,
        .input_float                   = pl_input_float,
        .input_float2                  = pl_input_float2,
        .input_float3                  = pl_input_float3,
        .input_float4                  = pl_input_float4,
        .input_int                     = pl_input_int,
        .input_int2                    = pl_input_int2,
        .input_int3                    = pl_input_int3,
        .input_int4                    = pl_input_int4,
        .slider_float                  = pl_slider_float,
        .slider_float_f                = pl_slider_float_f,
        .slider_int                    = pl_slider_int,
        .slider_int_f                  = pl_slider_int_f,
        .drag_float                    = pl_drag_float,
        .drag_float_f                  = pl_drag_float_f,
        .begin_collapsing_header       = pl_begin_collapsing_header,
        .end_collapsing_header         = pl_end_collapsing_header,
        .tree_node                     = pl_tree_node,
        .tree_node_f                   = pl_tree_node_f,
        .tree_node_v                   = pl_tree_node_v,
        .tree_pop                      = pl_tree_pop,
        .begin_menu                    = pl_begin_menu,
        .end_menu                      = pl_end_menu,
        .menu_item                     = pl_menu_item,
        .menu_item_toggle              = pl_menu_item_toggle,
        .begin_tab_bar                 = pl_begin_tab_bar,
        .end_tab_bar                   = pl_end_tab_bar,
        .begin_tab                     = pl_begin_tab,
        .end_tab                       = pl_end_tab,
        .separator                     = pl_separator,
        .separator_text                = pl_separator_text,
        .vertical_spacing              = pl_vertical_spacing,
        .indent                        = pl_indent,
        .unindent                      = pl_unindent,
        .step_clipper                  = pl_step_clipper,
        .layout_dynamic                = pl_layout_dynamic,
        .layout_static                 = pl_layout_static,
        .layout_row_begin              = pl_layout_row_begin,
        .layout_row_push               = pl_layout_row_push,
        .layout_row_end                = pl_layout_row_end,
        .layout_row                    = pl_layout_row,
        .layout_template_begin         = pl_layout_template_begin,
        .layout_template_push_dynamic  = pl_layout_template_push_dynamic,
        .layout_template_push_variable = pl_layout_template_push_variable,
        .layout_template_push_static   = pl_layout_template_push_static,
        .layout_template_end           = pl_layout_template_end,
        .layout_space_begin            = pl_layout_space_begin,
        .layout_space_push             = pl_layout_space_push,
        .layout_space_end              = pl_layout_space_end,
        .was_last_item_hovered         = pl_was_last_item_hovered,
        .was_last_item_active          = pl_was_last_item_active,
        .push_id_string                = pl_push_id_string,
        .push_id_pointer               = pl_push_id_pointer,
        .push_id_uint                  = pl_push_id_uint,
        .pop_id                        = pl_pop_id,
    };
    pl_set_api(ptApiRegistry, plUiI, &tApi);

    if(bReload)
    {
        gptCtx = gptDataRegistry->get_data("plUiContext");
    }
    else // first load
    {
        static plUiContext tContext = {0};
        gptCtx = &tContext;
        gptDataRegistry->set_data("plUiContext", gptCtx);
    }
}

static void
pl_unload_ui_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{

    if(bReload)
        return;

    const plUiI* ptApi = pl_get_api(ptApiRegistry, plUiI);
    ptApiRegistry->remove(ptApi);

    gptCtx = NULL;
}