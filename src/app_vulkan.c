/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_setup
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pilotlight.h"
#include "pl_graphics_vulkan.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#include "pl_draw_vulkan.h"
#include "pl_math.h"
#include "pl_camera.h"
#include "pl_registry.h" // data registry
#include "pl_ext.h"      // extension registry
#include "pl_ui.h"

// extensions
#include "pl_draw_extension.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plGraphics          tGraphics;
    plDrawContext       tCtx;
    plDrawList          drawlist;
    plDrawLayer*        fgDrawLayer;
    plDrawLayer*        bgDrawLayer;
    plFontAtlas         fontAtlas;
    plProfileContext    tProfileCtx;
    plLogContext        tLogCtx;
    plMemoryContext     tMemoryCtx;
    plDataRegistry      tDataRegistryCtx;
    plExtensionRegistry tExtensionRegistryCtx;
    plCamera            tCamera;
    plUiContext         tUiContext;

    // extension apis
    plDrawExtension* ptDrawExtApi;

} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plIOContext* ptIOCtx, plAppData* ptAppData)
{
    if(ptAppData) // reload
    {
        pl_set_log_context(&ptAppData->tLogCtx);
        pl_set_profile_context(&ptAppData->tProfileCtx);
        pl_set_memory_context(&ptAppData->tMemoryCtx);
        pl_set_data_registry(&ptAppData->tDataRegistryCtx);
        pl_set_extension_registry(&ptAppData->tExtensionRegistryCtx);
        pl_set_io_context(ptIOCtx);

        plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
        ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

        return ptAppData;
    }

    plAppData* tPNewData = malloc(sizeof(plAppData));
    memset(tPNewData, 0, sizeof(plAppData));

    pl_set_io_context(ptIOCtx);
    pl_initialize_memory_context(&tPNewData->tMemoryCtx);
    pl_initialize_profile_context(&tPNewData->tProfileCtx);
    pl_initialize_data_registry(&tPNewData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&tPNewData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&tPNewData->tExtensionRegistryCtx);
    pl_register_data("memory", &tPNewData->tMemoryCtx);
    pl_register_data("profile", &tPNewData->tProfileCtx);
    pl_register_data("log", &tPNewData->tLogCtx);
    pl_register_data("io", ptIOCtx);
    pl_register_data("draw", &tPNewData->tCtx);

    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    tPNewData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    return tPNewData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_setup
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_setup(plAppData* ptAppData)
{
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();

    // setup renderer
    pl_setup_graphics(&ptAppData->tGraphics);
    
    // setup drawing api
    pl_initialize_draw_context_vulkan(&ptAppData->tCtx, ptAppData->tGraphics.tDevice.tPhysicalDevice, ptAppData->tGraphics.tSwapchain.uImageCount, ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_register_drawlist(&ptAppData->tCtx, &ptAppData->drawlist);
    pl_setup_drawlist_vulkan(&ptAppData->drawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(&ptAppData->tCtx, &ptAppData->fontAtlas);

    // ui
    pl_ui_setup_context(&ptAppData->tCtx, &ptAppData->tUiContext);
    pl_setup_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, ptAppData->tGraphics.tRenderPass, ptAppData->tGraphics.tSwapchain.tMsaaSamples);
    ptAppData->tUiContext.ptFont = &ptAppData->fontAtlas.sbFonts[0];

    // camera
    ptAppData->tCamera = pl_create_perspective_camera((plVec3){0.0f, 0.0f, 8.5f}, PL_PI_3, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1], 0.01f, 400.0f);

}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    vkDeviceWaitIdle(ptAppData->tGraphics.tDevice.tLogicalDevice);
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_cleanup_draw_context(&ptAppData->tCtx);
    pl_ui_cleanup_context(&ptAppData->tUiContext);
    pl_cleanup_graphics(&ptAppData->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_extension_registry();
    pl_cleanup_log_context();
    pl_cleanup_data_registry();
    pl_cleanup_memory_context();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_resize_graphics(&ptAppData->tGraphics);
    pl_camera_set_aspect(&ptAppData->tCamera, ptIOCtx->afMainViewportSize[0] / ptIOCtx->afMainViewportSize[1]);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~frame setup~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    pl_begin_profile_frame(ptAppData->tCtx.frameCount);
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_handle_extension_reloads();
    pl_new_io_frame();
    pl_new_draw_frame(&ptAppData->tCtx);
    pl_ui_new_frame(&ptAppData->tUiContext);
    pl_process_cleanup_queue(&ptAppData->tGraphics.tResourceManager, 1);

    plFrameContext* ptCurrentFrame = pl_get_frame_resources(&ptAppData->tGraphics);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~begin frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    if(pl_begin_frame(&ptAppData->tGraphics))
    {
        pl_begin_recording(&ptAppData->tGraphics);

        pl_begin_main_pass(&ptAppData->tGraphics);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~input handling~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        static const float fCameraTravelSpeed = 8.0f;
        if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime);
        if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIOCtx->fDeltaTime);
        if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(&ptAppData->tCamera, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(&ptAppData->tCamera,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f,  0.0f);
        if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f, -fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);
        if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(&ptAppData->tCamera,  0.0f,  fCameraTravelSpeed * ptIOCtx->fDeltaTime,  0.0f);

        if(pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, -0.0f))
        {
            const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, -0.0f);
            pl_camera_rotate(&ptAppData->tCamera,  -tMouseDelta.y * 0.1f * ptIOCtx->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIOCtx->fDeltaTime);
            pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
        }

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~drawing api~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "Drawn from extension!");

        // draw profiling info

        pl_begin_profile_sample("Draw Profiling Info");
        char cPProfileValue[64] = {0};
        for(uint32_t i = 0u; i < pl_sb_size(ptAppData->tProfileCtx.ptLastFrame->sbtSamples); i++)
        {
            plProfileSample* tPSample = &ptAppData->tProfileCtx.ptLastFrame->sbtSamples[i];
            pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, tPSample->pcName, 0.0f);
            plVec2 sampleTextSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, tPSample->pcName, 0.0f);
            pl_sprintf(cPProfileValue, ": %0.5f", tPSample->dDuration);
            pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){sampleTextSize.x + 15.0f + (float)tPSample->uDepth * 15.0f, 50.0f + (float)i * 15.0f}, (plVec4){1.0f, 1.0f, 1.0f, 1.0f}, cPProfileValue, 0.0f);
        }
        pl_end_profile_sample();

        // ui

        static bool bOpen = true;


        if(pl_ui_begin_window("Pilot Light", NULL, false))
        {
            pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);
            pl_ui_checkbox("Camera Info", &bOpen);  
        }
        pl_ui_end_window();

        if(bOpen)
        {
            if(pl_ui_begin_window("Camera Info", &bOpen, true))
            {
                pl_ui_text("Pos: %.3f, %.3f, %.3f", ptAppData->tCamera.tPos.x, ptAppData->tCamera.tPos.y, ptAppData->tCamera.tPos.z);
                pl_ui_text("Pitch: %.3f, Yaw: %.3f, Roll:%.3f", ptAppData->tCamera.fPitch, ptAppData->tCamera.fYaw, ptAppData->tCamera.fRoll);
                pl_ui_text("Up: %.3f, %.3f, %.3f", ptAppData->tCamera._tUpVec.x, ptAppData->tCamera._tUpVec.y, ptAppData->tCamera._tUpVec.z);
                pl_ui_text("Forward: %.3f, %.3f, %.3f", ptAppData->tCamera._tForwardVec.x, ptAppData->tCamera._tForwardVec.y, ptAppData->tCamera._tForwardVec.z);
                pl_ui_text("Right: %.3f, %.3f, %.3f", ptAppData->tCamera._tRightVec.x, ptAppData->tCamera._tRightVec.y, ptAppData->tCamera._tRightVec.z);  
            }
            pl_ui_end_window();
        }

        if(pl_ui_begin_window("UI Demo", NULL, false))
        {
            pl_ui_progress_bar(0.75f, (plVec2){-1.0f, 0.0f}, NULL);
            if(pl_ui_button("Hover me!"))
                bOpen = true;

            if(pl_ui_was_last_item_hovered())
            {
                pl_ui_begin_tooltip();
                pl_ui_text("Clicking this button will reshow the camera window!", ptIOCtx->fDeltaTime);
                pl_ui_end_tooltip();
            }
            static int iValue = 0;
            pl_ui_text("Radio Buttons");
            pl_ui_radio_button("Option 1", &iValue, 0);
            pl_ui_same_line(0.0f, -1.0f);
            pl_ui_radio_button("Option 2", &iValue, 1);
            pl_ui_same_line(0.0f, -1.0f);
            pl_ui_radio_button("Option 3", &iValue, 2);
            pl_ui_text("Selectables");
            static bool bSelectable0 = false;
            static bool bSelectable1 = false;
            static bool bSelectable2 = false;
            pl_ui_selectable("Selectable 1", &bSelectable0);
            pl_ui_selectable("Selectable 2", &bSelectable1);
            pl_ui_selectable("Selectable 3", &bSelectable2);
            static bool bOpen0 = false;
            if(pl_ui_tree_node("Root Node", &bOpen0))
            {
                static bool bOpen1 = false;
                if(pl_ui_tree_node("Child 1", &bOpen1))
                {
                    if(pl_ui_button("Press me"))
                        bOpen = true;
                    pl_ui_tree_pop();
                }
                static bool bOpen2 = false;
                if(pl_ui_tree_node("Child 2", &bOpen2))
                {
                    pl_ui_button("Press me");
                    pl_ui_tree_pop();
                }
                pl_ui_tree_pop();
            }
            static bool bOpen3 = false;
            if(pl_ui_collapsing_header("Collapsing Header", &bOpen3))
            {
                pl_ui_checkbox("Camera window2", &bOpen);
            }

            if(pl_ui_begin_tab_bar("Tabs1"))
            {
                if(pl_ui_begin_tab("Tab 0"))
                {
                    pl_ui_selectable("Selectable 1", &bSelectable0);
                    pl_ui_selectable("Selectable 2", &bSelectable1);
                    pl_ui_selectable("Selectable 3", &bSelectable2);   
                }
                pl_ui_end_tab();

                if(pl_ui_begin_tab("Tab 1"))
                {
                    pl_ui_radio_button("Option 1", &iValue, 0);
                    pl_ui_radio_button("Option 2", &iValue, 1);
                    pl_ui_radio_button("Option 3", &iValue, 2);
                }
                pl_ui_end_tab();

                if(pl_ui_begin_tab("Tab 2"))
                {
                    pl_ui_radio_button("Option 1", &iValue, 0);
                    pl_ui_selectable("Selectable 2", &bSelectable1);
                }
                pl_ui_end_tab();
            }
            pl_ui_end_tab_bar();
        }
        pl_ui_end_window();

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~submit draws~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        // submit draw layers
        pl_begin_profile_sample("Submit draw layers");
        pl_submit_draw_layer(ptAppData->bgDrawLayer);
        pl_submit_draw_layer(ptAppData->fgDrawLayer);
        pl_end_profile_sample();

        pl_ui_render();

        // submit draw lists
        pl_submit_drawlist_vulkan(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);
        pl_submit_drawlist_vulkan(ptAppData->tUiContext.ptDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1], ptCurrentFrame->tCmdBuf, (uint32_t)ptAppData->tGraphics.szCurrentFrameIndex);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~end frame~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        pl_end_main_pass(&ptAppData->tGraphics);
        pl_end_recording(&ptAppData->tGraphics);
        pl_end_frame(&ptAppData->tGraphics);
    }
    pl_end_io_frame();
    pl_ui_end_frame();
    pl_end_profile_frame();
}