/*
   example_basic_5.cpp
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates minimal use of graphics extension
     - demonstrates ui extension
*/

/*
Index of this file:
// [SECTION] quick notes
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] full demo
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] quick notes
//-----------------------------------------------------------------------------

/*
    This example is isn't really here to demonstrate anything (yet). It
    utilizes an unstable extension and experimental backend but it can still
    be used to experiment with Dear ImGui or used as a template to create tool
    with this version of Pilot Light. For reference, the experimental backend
    is currently "experimental" because it has some Dear ImGui setup code in
    it. Once more features are exposed by the windows extension & backend, 
    the Dear ImGui code can be completely handled by users or extension. Until
    then, it will be experimental.

    IMPORTANT:
        To use this example, you must build Pilot Light and the examples with
        "debug_experimental" or "release_experimental".

        ~~~> build.bat -c debug_release
        or
        ~~~> build.sh -c debug_release

    Dear ImGui is an immediate mode UI library similar to our UI extension (which
    was actually inspired by both Dear ImGui and Nuklear). There are differences
    so make sure you check the header files (documentation) for both before 
    assumming the APIs work the exact same way. In particular when it comes to
    paired calls for things like begin/end widgets/windows/containers.
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <stdlib.h> // malloc, free
#include <stdio.h>  // printf
#include <string.h> // memset
#include "pl.h"

// extensions
#include "pl_window_ext.h"
#include "pl_ui_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"
#include "pl_graphics_ext.h"

// dear imgui
#include "pl_dear_imgui_ext.h"
#include "imgui.h"
#include "implot.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plAppData
{
    plWindow* ptWindow;
} plAppData;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*          gptIO          = NULL;
const plWindowI*      gptWindows     = NULL;
const plUiI*          gptUi          = NULL;
const plDrawBackendI* gptDrawBackend = NULL;
const plStarterI*     gptStarter     = NULL;
const plDearImGuiI*   gptDearImGui   = NULL;
const plGraphicsI*    gptGfx         = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "pAppData" will be NULL but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~dear imgui context stuff~~~~~~~~~~~~~~~~~~~~~~~~~~

    // Dear ImGui uses globals for it's content & memory allocators. If you are
    // crossing a dll/so boundary, you must set these. The experimental backend
    // provides these to the data registry with the names below, so they must
    // be retrieved and set.

    // retrieve/set imgui context
    ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    // retrieve/set imgui allocator functions
    ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)ptDataRegistry->get_data("imgui allocate");
    ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)ptDataRegistry->get_data("imgui free");
    ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptDearImGui   = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    // load extensions
    //   * first argument is the shared library name WITHOUT the extension
    //   * second & third argument is the load/unload functions names (use NULL for the default of "pl_load_ext" &
    //     "pl_unload_ext")
    //   * fourth argument indicates if the extension is reloadable (should we check for changes and reload if changed)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false); // provides the file API used by the drawing ext
    ptExtensionRegistry->load("pl_dear_imgui_ext", NULL, NULL, false); // provides the imgui backend stuff
    
    // load required apis
    gptIO          = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows     = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptDearImGui   = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);

    // use window API to create a window

    plWindowDesc tWindowDesc = {
        "Example Basic 5",
        500,
        500,
        200,
        200
    };
    gptWindows->create_window(tWindowDesc, &ptAppData->ptWindow);

    plStarterInit tStarterInit = {};
    tStarterInit.tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    gptStarter->initialize(tStarterInit);

    // wraps up (i.e. builds font atlas)
    gptStarter->finalize();

    // initializes out Dear ImGui backend (similar to the draw backend extension)
    gptDearImGui->initialize(gptStarter->get_device(), gptStarter->get_swapchain(), gptStarter->get_render_pass());

    // same process for implot as imgui
    ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    plDevice* ptDevice = gptStarter->get_device();
    gptGfx->flush_device(ptDevice);

    // cleanup backend resources
    gptDearImGui->cleanup();

    gptStarter->cleanup();
    gptWindows->destroy_window(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    gptStarter->resize();
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{

    if(!gptStarter->begin_frame())
        return;

    gptDearImGui->new_frame(gptStarter->get_device(), gptStarter->get_render_pass());

    ImGui::DockSpaceOverViewport();

    // create a window
    if(ImGui::Begin("Pilot Light"))
    {
        if(ImGui::Button("Print To Console"))
        {
            printf("Button Clicked");
        }
    }
    ImGui::End();

    // just some provided demo windows
    ImPlot::ShowDemoWindow(nullptr);
    ImGui::ShowDemoWindow(nullptr);


    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    // submit Dear ImGui stuff
    gptDearImGui->render(ptEncoder, gptGfx->get_encoder_command_buffer(ptEncoder));

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}
