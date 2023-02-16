/*
   vulkan_app.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_dx11.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_io.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_registry.h" // data registry
#include "pl_ext.h"      // extension registry
#include "pl_ui.h"

// extensions
#include "pl_draw_extension.h"

#ifndef PL_DX11
#include <assert.h>
#define PL_DX11(x) assert(SUCCEEDED((x)))
#endif

#ifndef PL_COM
#define PL_COM(x) (x)->lpVtbl
#endif

#ifndef PL_COM_RELEASE
#define PL_COM_RELEASE(x) (x)->lpVtbl->Release((x))
#endif

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxguid.lib")

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    ID3D11Device*           ptDevice;
    ID3D11DeviceContext*    ptContext;
    IDXGISwapChain*         ptSwapChain;
    ID3D11Texture2D*        ptFrameBuffer;
    ID3D11RenderTargetView* ptFrameBufferView;
    plDrawList              drawlist;
    plDrawLayer*            fgDrawLayer;
    plDrawLayer*            bgDrawLayer;
    plFontAtlas             fontAtlas;
    plProfileContext        tProfileCtx;
    plLogContext            tLogCtx;
    plMemoryContext         tMemoryCtx;
    plDataRegistry          tDataRegistryCtx;
    plExtensionRegistry     tExtensionRegistryCtx;
    plUiContext             tUiContext;

    // extension apis
    plDrawExtension*        ptDrawExtApi;
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
        pl_ui_set_context(&ptAppData->tUiContext);

        plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
        ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

        return ptAppData;
    }

    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    pl_set_io_context(ptIOCtx);

    // setup memory context
    pl_initialize_memory_context(&ptAppData->tMemoryCtx);

    // setup profiling context
    pl_initialize_profile_context(&ptAppData->tProfileCtx);

    // setup data registry
    pl_initialize_data_registry(&ptAppData->tDataRegistryCtx);

    // setup logging
    pl_initialize_log_context(&ptAppData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // setup extension registry
    pl_initialize_extension_registry(&ptAppData->tExtensionRegistryCtx);
    pl_register_data("memory", &ptAppData->tMemoryCtx);
    pl_register_data("profile", &ptAppData->tProfileCtx);
    pl_register_data("log", &ptAppData->tLogCtx);
    pl_register_data("io", ptIOCtx);

    plExtension tExtension = {0};
    pl_get_draw_extension_info(&tExtension);
    pl_load_extension(&tExtension);

    plExtension* ptExtension = pl_get_extension(PL_EXT_DRAW);
    ptAppData->ptDrawExtApi = pl_get_api(ptExtension, PL_EXT_API_DRAW);

    // create swapchain & device
    DXGI_SWAP_CHAIN_DESC tSwapChainDescription = 
    {
        .BufferDesc.Width  = (UINT)ptIOCtx->afMainViewportSize[0],
        .BufferDesc.Height = (UINT)ptIOCtx->afMainViewportSize[1],
        .BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc.Count  = 1,
        .BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount       = 2,
        .Windowed          = true,
        .SwapEffect        = DXGI_SWAP_EFFECT_SEQUENTIAL,
        .OutputWindow    = *(HWND*)ptIOCtx->pBackendPlatformData
    };

    PL_DX11(D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_DISABLE_GPU_TIMEOUT,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &tSwapChainDescription,
        &ptAppData->ptSwapChain,
        &ptAppData->ptDevice,
        NULL,
        &ptAppData->ptContext
    ));

    // create render targets
    PL_DX11(PL_COM(ptAppData->ptSwapChain)->GetBuffer(ptAppData->ptSwapChain, 0, &IID_ID3D11Texture2D, (void**)&ptAppData->ptFrameBuffer));

    // create render target views
    PL_DX11(PL_COM(ptAppData->ptDevice)->CreateRenderTargetView(ptAppData->ptDevice, (ID3D11Resource*)ptAppData->ptFrameBuffer, 0, &ptAppData->ptFrameBufferView));

    // setup memory context
    pl_initialize_memory_context(&ptAppData->tMemoryCtx);

    // setup profiling context
    pl_initialize_profile_context(&ptAppData->tProfileCtx);

    // setup logging
    pl_initialize_log_context(&ptAppData->tLogCtx);
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info(0, "Setup logging");

    // ui
    pl_ui_setup_context(&ptAppData->tUiContext);

    // setup drawing api
    pl_initialize_draw_context_dx11(ptAppData->tUiContext.ptDrawCtx, ptAppData->ptDevice, ptAppData->ptContext);
    pl_register_drawlist(ptAppData->tUiContext.ptDrawCtx, &ptAppData->drawlist);
    pl_setup_drawlist_dx11(&ptAppData->drawlist);
    ptAppData->bgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Background Layer");
    ptAppData->fgDrawLayer = pl_request_draw_layer(&ptAppData->drawlist, "Foreground Layer");

    // create font atlas
    pl_add_default_font(&ptAppData->fontAtlas);
    pl_build_font_atlas(ptAppData->tUiContext.ptDrawCtx, &ptAppData->fontAtlas);

    pl_setup_drawlist_dx11(ptAppData->tUiContext.ptDrawlist);
    pl_setup_drawlist_dx11(ptAppData->tUiContext.ptDebugDrawlist);

    ptAppData->tUiContext.ptFont = &ptAppData->fontAtlas.sbFonts[0];
    pl_register_data("draw", ptAppData->tUiContext.ptDrawCtx);

    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{

    PL_COM_RELEASE(ptAppData->ptFrameBufferView);
    PL_COM_RELEASE(ptAppData->ptFrameBuffer);
    PL_COM_RELEASE(ptAppData->ptSwapChain);
    PL_COM_RELEASE(ptAppData->ptContext);
    PL_COM_RELEASE(ptAppData->ptDevice);

    // clean up contexts
    pl_cleanup_font_atlas(&ptAppData->fontAtlas);
    pl_ui_cleanup_context();
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
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();

    if(ptAppData->ptDevice)
    {
        PL_COM_RELEASE(ptAppData->ptFrameBufferView);
        PL_COM(ptAppData->ptContext)->OMSetRenderTargets(ptAppData->ptContext, 0, 0, 0);
        PL_COM_RELEASE(ptAppData->ptFrameBuffer);
        PL_COM(ptAppData->ptSwapChain)->ResizeBuffers(ptAppData->ptSwapChain, 0, (UINT)ptIOCtx->afMainViewportSize[0], (UINT)ptIOCtx->afMainViewportSize[1], DXGI_FORMAT_UNKNOWN, 0);
        PL_COM(ptAppData->ptSwapChain)->GetBuffer(ptAppData->ptSwapChain, 0, &IID_ID3D11Texture2D, (void**)&ptAppData->ptFrameBuffer);

        // create render target
        D3D11_TEXTURE2D_DESC tTextureDescription;
        PL_COM(ptAppData->ptFrameBuffer)->GetDesc(ptAppData->ptFrameBuffer, &tTextureDescription);

        // create the target view on the texture
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {
            .Format = tTextureDescription.Format,
            .ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D
        };
        PL_DX11(PL_COM(ptAppData->ptDevice)->CreateRenderTargetView(ptAppData->ptDevice, (ID3D11Resource*)ptAppData->ptFrameBuffer, &rtvDesc, &ptAppData->ptFrameBufferView));
    }  
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    
    // get io context
    plIOContext* ptIOCtx = pl_get_io_context();
    pl_begin_profile_frame(ptIOCtx->ulFrameCount);

    pl_handle_extension_reloads();
    pl_ui_new_frame();

    // set viewport
    D3D11_VIEWPORT tViewport = {
        .TopLeftX = 0.0f,
        .TopLeftY = 0.0f,
        .Width = ptIOCtx->afMainViewportSize[0],
        .Height = ptIOCtx->afMainViewportSize[1],
        .MaxDepth = 1.0f
    };
    PL_COM(ptAppData->ptContext)->RSSetViewports(ptAppData->ptContext, 1, &tViewport);

    // clear & set render targets
    static float afClearColor[4] = { 0.1f, 0.0f, 0.0f, 1.0f };
    PL_COM(ptAppData->ptContext)->ClearRenderTargetView(ptAppData->ptContext, ptAppData->ptFrameBufferView, afClearColor);
    PL_COM(ptAppData->ptContext)->OMSetRenderTargets(ptAppData->ptContext, 1, &ptAppData->ptFrameBufferView, NULL);

    ptAppData->ptDrawExtApi->pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){100.0f, 100.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, "extension baby");

    // draw profiling info
    pl_begin_profile_sample("Draw Profiling Info");
    static char pcDeltaTime[64] = {0};
    pl_sprintf(pcDeltaTime, "%.6f ms", ptIOCtx->fDeltaTime);
    pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){10.0f, 10.0f}, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, pcDeltaTime, 0.0f);
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

    // draw commands
    pl_begin_profile_sample("Add draw commands");
    pl_add_text(ptAppData->fgDrawLayer, &ptAppData->fontAtlas.sbFonts[0], 13.0f, (plVec2){300.0f, 10.0f}, (plVec4){0.1f, 0.5f, 0.0f, 1.0f}, "Pilot Light\nGraphics", 0.0f);
    pl_add_triangle_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 50.0f}, (plVec2){300.0f, 150.0f}, (plVec2){350.0f, 50.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f});
    pl_begin_profile_sample("Calculate text size");
    plVec2 textSize = pl_calculate_text_size(&ptAppData->fontAtlas.sbFonts[0], 13.0f, "Pilot Light\nGraphics", 0.0f);
    pl_end_profile_sample();
    pl_add_rect_filled(ptAppData->bgDrawLayer, (plVec2){300.0f, 10.0f}, (plVec2){300.0f + textSize.x, 10.0f + textSize.y}, (plVec4){0.0f, 0.0f, 0.8f, 0.5f});
    pl_add_line(ptAppData->bgDrawLayer, (plVec2){500.0f, 10.0f}, (plVec2){10.0f, 500.0f}, (plVec4){1.0f, 1.0f, 1.0f, 0.5f}, 2.0f);
    pl_end_profile_sample();

    static bool bOpen = true;

    if(pl_ui_begin_window("Pilot Light", NULL, false))
    {
        pl_ui_text("%.6f ms", ptIOCtx->fDeltaTime);

        pl_ui_checkbox("Camera Info", &bOpen);
        pl_ui_end_window();
    }
    

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_draw_layer(ptAppData->bgDrawLayer);
    pl_submit_draw_layer(ptAppData->fgDrawLayer);
    pl_end_profile_sample();

    pl_ui_render();

    // submit draw lists
    pl_submit_drawlist_dx11(&ptAppData->drawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1]);
    pl_submit_drawlist_dx11(ptAppData->tUiContext.ptDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1]);
    pl_submit_drawlist_dx11(ptAppData->tUiContext.ptDebugDrawlist, (float)ptIOCtx->afMainViewportSize[0], (float)ptIOCtx->afMainViewportSize[1]);

    // present
    PL_COM(ptAppData->ptSwapChain)->Present(ptAppData->ptSwapChain, 1, 0);

    pl_end_profile_frame();
}