/*
   demo.cpp

   Notes:
     * absolute mess
     * mostly a sandbox for now & testing experimental stuff
     * probably better to look at the examples
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper implementations
// [SECTION] unity build
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include "demo.h"

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryI* ptApiRegistry, plAppData* ptAppData)
{
    // NOTE: on first load, "ptAppData" will be nullptr but on reloads
    //       it will be the value returned from this function

    // retrieve the data registry API, this is the API used for sharing data
    // between extensions & the runtime
    const plDataRegistryI* ptDataRegistry = pl_get_api_latest(ptApiRegistry, plDataRegistryI);

    ImGuiContext* ptImguiContext = (ImGuiContext*)ptDataRegistry->get_data("imgui");
    ImGui::SetCurrentContext(ptImguiContext);

    ImGuiMemAllocFunc p_alloc_func = (ImGuiMemAllocFunc)ptDataRegistry->get_data("imgui allocate");
    ImGuiMemFreeFunc p_free_func = (ImGuiMemFreeFunc)ptDataRegistry->get_data("imgui free");
    ImGui::SetAllocatorFunctions(p_alloc_func, p_free_func, nullptr);

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData) // reload
    {

        // re-retrieve the apis since we are now in
        // a different dll/so
        gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
        gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
        gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
        gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
        gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
        gptCameraEcs     = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
        gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
        gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
        gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
        gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
        gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
        gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
        gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
        gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
        gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
        gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
        gptEcsTools      = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
        gptGizmo         = pl_get_api_latest(ptApiRegistry, plGizmoI);
        gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
        gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
        gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
        gptCollision     = pl_get_api_latest(ptApiRegistry, plCollisionI);
        gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
        gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
        gptDearImGui     = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
        gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
        gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
        gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
        gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
        gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
        gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
        gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
        gptMaterial      = pl_get_api_latest(ptApiRegistry, plMaterialI);
        gptScript        = pl_get_api_latest(ptApiRegistry, plScriptI);
        gptRendererDebug = pl_get_api_latest(ptApiRegistry, plRendererDebugI);
        gptRendererEcs    = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
        gptRendererEditor = pl_get_api_latest(ptApiRegistry, plRendererEditorI);

        ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));

        gptScreenLog->add_message_ex(0, 15.0, PL_COLOR_32_MAGENTA, 1.5f, "%s", "App Hot Reloaded");

        return ptAppData;
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~apis & extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions
    ptExtensionRegistry->load("pl_unity_ext", nullptr, nullptr, true);
    ptExtensionRegistry->load("pl_platform_ext", "pl_load_platform_ext", "pl_unload_platform_ext", false);
    ptExtensionRegistry->load("pl_dear_imgui_ext", "pl_load_dear_imgui_ext", "pl_unload_dear_imgui_ext", false);

    // load apis
    gptWindows       = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptStats         = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptGfx           = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptTools         = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptEcs           = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera        = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptCameraEcs     = pl_get_api_latest(ptApiRegistry, plCameraEcsI);
    gptRenderer      = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptJobs          = pl_get_api_latest(ptApiRegistry, plJobI);
    gptModelLoader   = pl_get_api_latest(ptApiRegistry, plModelLoaderI);
    gptDraw          = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI            = pl_get_api_latest(ptApiRegistry, plUiI);
    gptIO            = pl_get_api_latest(ptApiRegistry, plIOI);
    gptShader        = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptMemory        = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptNetwork       = pl_get_api_latest(ptApiRegistry, plNetworkI);
    gptString        = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptProfile       = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptFile          = pl_get_api_latest(ptApiRegistry, plFileI);
    gptEcsTools      = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptGizmo         = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptConsole       = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptScreenLog     = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics       = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision     = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh           = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig        = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptDearImGui     = pl_get_api_latest(ptApiRegistry, plDearImGuiI);
    gptResource      = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptStarter       = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptAnimation     = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh          = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptShaderVariant = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs           = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak           = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime      = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress      = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptMaterial      = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript        = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptRendererDebug = pl_get_api_latest(ptApiRegistry, plRendererDebugI);
    gptRendererEcs    = pl_get_api_latest(ptApiRegistry, plRendererEcsI);
    gptRendererEditor = pl_get_api_latest(ptApiRegistry, plRendererEditorI);

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = (plAppData*)PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    gptVfs->mount_directory("/gltf-samples", "../assets/gltf-samples/Models", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/environments", "../assets/development/environments", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shaders", "../shaders", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/shader-temp", "../shader-temp", PL_VFS_MOUNT_FLAGS_NONE);
    gptVfs->mount_directory("/assets", "../assets", PL_VFS_MOUNT_FLAGS_NONE);
    gptFile->create_directory("../shader-temp");

    // defaults
    ptAppData->tSelectedEntity.uData = UINT64_MAX;
    ptAppData->bShowPilotLightTool = true;
    ptAppData->bFrustumCulling = true;
    ptAppData->bVSync = true;

    gptConfig->load_from_disk(nullptr);
    ptAppData->bShowEntityWindow = gptConfig->load_bool("bShowEntityWindow", false);
    ptAppData->bPhysicsDebugDraw = gptConfig->load_bool("bPhysicsDebugDraw", false);

    // add console variables
    gptConsole->initialize({PL_CONSOLE_FLAGS_POPUP});
    gptConsole->add_toggle_variable("a.PilotLight", &ptAppData->bShowPilotLightTool, "shows main pilot light window", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.Entities", &ptAppData->bShowEntityWindow, "shows ecs tool", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);
    gptConsole->add_toggle_variable("a.FreezeCullCamera", &ptAppData->bFreezeCullCamera, "freezes culling camera", PL_CONSOLE_VARIABLE_FLAGS_CLOSE_CONSOLE);

    // initialize APIs that require it
    gptEcsTools->initialize();
    gptPhysics->initialize({});

    // create window (only 1 allowed currently)
    plWindowDesc tWindowDesc = {
        PL_WINDOW_FLAG_NONE,
        "Pilot Light Editor",
        1500,
        900,
        50,
        50
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plFullScreenDesc tFullScreen = {};
    tFullScreen.iMonitor = 0;
    // tFullScreen.tMode = PL_FULLSCREEN_MODE_EXCLUSIVE;
    gptWindows->set_fullscreen(ptAppData->ptWindow, &tFullScreen);

    plStarterInit tStarterInit = {};
    tStarterInit.tFlags   = PL_STARTER_FLAGS_NONE;
    tStarterInit.ptWindow = ptAppData->ptWindow;

    // extensions handled by starter
    tStarterInit.tFlags |= PL_STARTER_FLAGS_GRAPHICS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_PROFILE_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_STATS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_CONSOLE_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_TOOLS_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DRAW_EXT;
    tStarterInit.tFlags |= PL_STARTER_FLAGS_UI_EXT;
    // tStarterInit.tFlags |= PL_STARTER_FLAGS_SCREEN_LOG_EXT;

    // initial flags
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);
    
    // initialize shader compiler
    static plShaderOptions tDefaultShaderOptions = PL_ZERO_INIT;
    tDefaultShaderOptions.apcIncludeDirectories[0] = "/shaders/";
    tDefaultShaderOptions.apcDirectories[0] = "/shaders/";
    tDefaultShaderOptions.apcDirectories[1] = "/shader-temp/";
    tDefaultShaderOptions.pcCacheOutputDirectory = "/shader-temp/";
    tDefaultShaderOptions.tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_INCLUDE_DEBUG | PL_SHADER_FLAGS_ALWAYS_COMPILE;
    gptShader->initialize(&tDefaultShaderOptions);

    ptAppData->ptDevice = gptStarter->get_device();

    // initialize job system
    gptJobs->initialize({});

    const plShaderVariantInit tShaderVariantInit = {
        ptAppData->ptDevice
    };
    gptShaderVariant->initialize(tShaderVariantInit);

    // setup reference renderer
    plRendererSettings tRenderSettings = PL_ZERO_INIT;
    tRenderSettings.ptDevice              = ptAppData->ptDevice;
    tRenderSettings.uMaxTextureResolution = 1024;
    tRenderSettings.ptSwapchain           = gptStarter->get_swapchain();
    gptRenderer->initialize(&tRenderSettings);

    // initialize ecs component library
    gptEcs->initialize({});
    gptRendererEcs->register_system();
    gptScript->register_ecs_system();
    gptAnimation->register_ecs_system();
    gptCameraEcs->register_ecs_system();
    gptMesh->register_ecs_system();
    gptPhysics->register_ecs_system();
    gptMaterial->register_ecs_system();
    gptEcs->finalize();
    ptAppData->ptCompLibrary = gptEcs->get_default_library();

    gptTools->initialize({ptAppData->ptDevice});

    // retrieve some console variables
    ptAppData->pbShowLogging              = (bool*)gptConsole->get_variable("t.LogTool", nullptr, nullptr);
    ptAppData->pbShowStats                = (bool*)gptConsole->get_variable("t.StatTool", nullptr, nullptr);
    ptAppData->pbShowProfiling            = (bool*)gptConsole->get_variable("t.ProfileTool", nullptr, nullptr);
    ptAppData->pbShowMemoryAllocations    = (bool*)gptConsole->get_variable("t.MemoryAllocationTool", nullptr, nullptr);
    ptAppData->pbShowDeviceMemoryAnalyzer = (bool*)gptConsole->get_variable("t.DeviceMemoryAnalyzerTool", nullptr, nullptr);

    *ptAppData->pbShowLogging = gptConfig->load_bool("pbShowLogging", *ptAppData->pbShowLogging);
    *ptAppData->pbShowStats = gptConfig->load_bool("pbShowStats", *ptAppData->pbShowStats);
    *ptAppData->pbShowProfiling = gptConfig->load_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    *ptAppData->pbShowMemoryAllocations = gptConfig->load_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    *ptAppData->pbShowDeviceMemoryAnalyzer = gptConfig->load_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~setup draw extensions~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // create fonts
    plFontRange tFontRange = PL_ZERO_INIT;
    tFontRange.iFirstCodePoint = 0x0020;
    tFontRange.uCharCount = 0x00FF - 0x0020;

    plFontConfig tFontConfig0 = PL_ZERO_INIT;
    tFontConfig0.bSdf = false;
    tFontConfig0.fSize = 16.0f;
    tFontConfig0.uHOverSampling = 1;
    tFontConfig0.uVOverSampling = 1;
    tFontConfig0.ptRanges = &tFontRange;
    tFontConfig0.uRangeCount = 1;
    ptAppData->tDefaultFont = gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig0, "/assets/core/fonts/Cousine-Regular.ttf");

    plFontRange tIconRange = PL_ZERO_INIT;
    tIconRange.iFirstCodePoint = ICON_MIN_FA;
    tIconRange.uCharCount = ICON_MAX_16_FA - ICON_MIN_FA;

    plFontConfig tFontConfig1 = PL_ZERO_INIT;
    tFontConfig1.bSdf           = false;
    tFontConfig1.fSize          = 16.0f;
    tFontConfig1.uHOverSampling = 1;
    tFontConfig1.uVOverSampling = 1;
    tFontConfig1.ptMergeFont    = ptAppData->tDefaultFont;
    tFontConfig1.ptRanges       = &tIconRange;
    tFontConfig1.uRangeCount    = 1;
    gptDraw->add_font_from_file_ttf(gptDraw->get_current_font_atlas(), tFontConfig1, "/assets/core/fonts/fa-solid-900.otf");
    gptStarter->set_default_font(ptAppData->tDefaultFont);
    gptUI->set_default_font(ptAppData->tDefaultFont);

    gptStarter->finalize();

    gptScreenLog->initialize({ptAppData->tDefaultFont});


    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~app stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // temporary draw layer for submitting fullscreen quad of offscreen render
    ptAppData->ptDrawLayer = gptDraw->request_2d_layer(gptUI->get_draw_list());

    pl__find_models(ptAppData);

    gptDearImGui->initialize(ptAppData->ptDevice, gptStarter->get_swapchain(), gptStarter->get_render_pass());
    // ImGui::GetIO().ConfigFlags &= ~ImGuiBackendFlags_PlatformHasViewports;
    ImPlot::SetCurrentContext((ImPlotContext*)ptDataRegistry->get_data("implot"));
    ImGuiIO& tImGuiIO = ImGui::GetIO();
    tImGuiIO.IniFilename = nullptr;
    ImGui::LoadIniSettingsFromDisk("../internal/demo/pl_imgui.ini");
    tImGuiIO.Fonts->AddFontFromFileTTF("../assets/core/fonts/Cousine-Regular.ttf", 16.0f);
    auto tImGuiFontConfig = ImFontConfig();
    tImGuiFontConfig.MergeMode = true;
    static ImWchar atFontRanges[] = {ICON_MIN_FA, ICON_MAX_16_FA};
    tImGuiIO.FontDefault = tImGuiIO.Fonts->AddFontFromFileTTF("../assets/core/fonts/fa-solid-900.otf", 16.0f, &tImGuiFontConfig, atFontRanges);
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptJobs->cleanup();
    pl_sb_free(ptAppData->sbcTempBuffer);

    // ensure GPU is finished before cleanup
    gptGfx->flush_device(ptAppData->ptDevice);

    gptDearImGui->cleanup();

    gptConfig->set_bool("bShowEntityWindow", ptAppData->bShowEntityWindow);
    gptConfig->set_bool("bPhysicsDebugDraw", ptAppData->bPhysicsDebugDraw);
    gptConfig->set_bool("pbShowLogging", *ptAppData->pbShowLogging);
    gptConfig->set_bool("pbShowStats", *ptAppData->pbShowStats);
    gptConfig->set_bool("pbShowProfiling", *ptAppData->pbShowProfiling);
    gptConfig->set_bool("pbShowMemoryAllocations", *ptAppData->pbShowMemoryAllocations);
    gptConfig->set_bool("pbShowDeviceMemoryAnalyzer", *ptAppData->pbShowDeviceMemoryAnalyzer);

    gptConfig->save_to_disk(nullptr);
    gptConfig->cleanup();
    gptEcsTools->cleanup();
    gptPhysics->cleanup();
    gptShader->cleanup();
    gptScreenLog->cleanup();
    if(ptAppData->ptView)
        gptRenderer->destroy_view(ptAppData->ptView);
    if(ptAppData->ptSecondaryView)
        gptRenderer->destroy_view(ptAppData->ptSecondaryView);
    if(ptAppData->ptScene)
        gptRenderer->destroy_scene(ptAppData->ptScene);
    gptEcs->cleanup();
    gptRenderer->cleanup();
    gptShaderVariant->cleanup();
    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    pl_sb_free(ptAppData->sbtTestModels);
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow*, plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    ptAppData->bResize = true;
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
        
    PL_PROFILE_BEGIN_SAMPLE_API(gptProfile, 0, __FUNCTION__);

    gptResource->new_frame();
    
    // for convience
    plIO* ptIO = gptIO->get_io();

    gptRenderer->begin_frame();

    gptDearImGui->new_frame(ptAppData->ptDevice, gptStarter->get_render_pass());

    if(ptAppData->bResize)
    {
        // gptOS->sleep(32);
        if(ptAppData->ptScene)
            gptRenderer->resize_view(ptAppData->ptView, ptIO->tMainViewportSize);
        ptAppData->bResize = false;
    }

    // update statistics
    gptShaderVariant->update_stats();

    if(ptAppData->ptScene)
    {
        gptCamera->set_aspect((plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera), (ptIO->tMainViewportSize.x * ptAppData->tView0Scale.x) / (ptIO->tMainViewportSize.y * ptAppData->tView0Scale.y));

        plCamera*  ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);
        plCamera*  ptCullCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tCullCamera);
        plCamera*  ptSecondaryCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tSecondaryCamera);
        gptCamera->update(ptCullCamera);
        gptCamera->update(ptSecondaryCamera);

        //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~selection stuff~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

        plVec2 tMousePos = gptIO->get_mouse_pos();

        if(ptAppData->bMainViewHovered && !gptUI->wants_mouse_capture() && !gptGizmo->active())
        {
            static plVec2 tClickPos = {0};
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                tClickPos = tMousePos;
            }
            else if(ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            {
                plVec2 tReleasePos = tMousePos;

                if(tReleasePos.x == tClickPos.x && tReleasePos.y == tClickPos.y)
                    gptRendererEditor->update_hovered_entity(ptAppData->ptView, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
        }

        // run ecs system
        gptScript->run_update_system(ptAppData->ptCompLibrary);
        gptAnimation->run_animation_update_system(ptAppData->ptCompLibrary, ptIO->fDeltaTime);
        gptPhysics->update(ptIO->fDeltaTime, ptAppData->ptCompLibrary);
        gptEcs->run_transform_update_system(ptAppData->ptCompLibrary);
        gptEcs->run_hierarchy_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_light_update_system(ptAppData->ptCompLibrary);
        gptCameraEcs->run_ecs(ptAppData->ptCompLibrary);
        gptAnimation->run_inverse_kinematics_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_skin_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_object_update_system(ptAppData->ptCompLibrary);
        gptRendererEcs->run_environment_probe_update_system(ptAppData->ptCompLibrary); // run after object update

        plEntity tNextEntity = {0};
        if(gptRendererEditor->get_hovered_entity(ptAppData->ptView, &tNextEntity))
        {
            
            if(tNextEntity.uData == 0)
            {
                ptAppData->tSelectedEntity.uData = UINT64_MAX;
                gptRendererEditor->outline_entities(ptAppData->ptScene, 0, nullptr);
            }
            else if(ptAppData->tSelectedEntity.uData != tNextEntity.uData)
            {
                gptScreenLog->add_message_ex(565168477883, 5.0, PL_COLOR_32_RED, 1.0f, "Selected Entity {%u, %u}", tNextEntity.uIndex, tNextEntity.uGeneration);
                gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &tNextEntity);
                ptAppData->tSelectedEntity = tNextEntity;
                gptPhysics->set_angular_velocity(ptAppData->ptCompLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
                gptPhysics->set_linear_velocity(ptAppData->ptCompLibrary, tNextEntity, pl_create_vec3(0, 0, 0));
            }

        }

        if(gptIO->is_key_pressed(PL_KEY_M, true))
            gptGizmo->next_mode();

        if(ptAppData->bShowEntityWindow)
        {
            if(gptEcsTools->show_window(ptAppData->ptCompLibrary, &ptAppData->tSelectedEntity, ptAppData->ptScene, &ptAppData->bShowEntityWindow))
            {
                if(ptAppData->tSelectedEntity.uData == UINT64_MAX)
                {
                    gptRendererEditor->outline_entities(ptAppData->ptScene, 0, nullptr);
                }
                else
                {
                    gptRendererEditor->outline_entities(ptAppData->ptScene, 1, &ptAppData->tSelectedEntity);
                }
            }
        }

        if(ptAppData->tSelectedEntity.uIndex != UINT32_MAX)
        {
            plDrawList3D* ptGizmoDrawlist =  gptRendererEditor->get_gizmo_drawlist(ptAppData->ptView);
            plObjectComponent* ptSelectedObject = (plObjectComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptRendererEcs->get_ecs_type_key_object(), ptAppData->tSelectedEntity);
            plTransformComponent* ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptAppData->tSelectedEntity);
            plTransformComponent* ptParentTransform = nullptr;
            plHierarchyComponent* ptHierarchyComp = (plHierarchyComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_hierarchy(), ptAppData->tSelectedEntity);
            if(ptHierarchyComp)
            {
                ptParentTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptHierarchyComp->tParent);
            }
            if(ptSelectedTransform)
            {
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
            else if(ptSelectedObject)
            {
                ptSelectedTransform = (plTransformComponent*)gptEcs->get_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptSelectedObject->tTransform);
                gptGizmo->gizmo(ptGizmoDrawlist, ptCamera, ptSelectedTransform, ptParentTransform, ptAppData->tView0Offset, ptAppData->tView0Scale);
            }
    }

        if(ptAppData->bPhysicsDebugDraw)
        {
            plDrawList3D* ptDrawlist = gptRendererDebug->get_drawlist(ptAppData->ptView);
            gptPhysics->draw(ptAppData->ptCompLibrary, ptDrawlist);
        }

        // debug rendering
        if(ptAppData->bShowDebugLights)
        {
            plLightComponent* ptLights = nullptr;
            const uint32_t uLightCount = gptEcs->get_components(ptAppData->ptCompLibrary, gptRendererEcs->get_ecs_type_key_light(), (void**)&ptLights, nullptr);
            gptRendererDebug->draw_lights(ptAppData->ptView, ptLights, uLightCount);
            gptRendererDebug->draw_lights(ptAppData->ptSecondaryView, ptLights, uLightCount);
        }

        if(ptAppData->bDrawAllBoundingBoxes)
        {
            gptRendererDebug->draw_all_bound_boxes(ptAppData->ptView);
            gptRendererDebug->draw_all_bound_boxes(ptAppData->ptSecondaryView);
        }

        if(ptAppData->bShowBVH)
        {
            gptRendererDebug->draw_bvh(ptAppData->ptView);
            gptRendererDebug->draw_bvh(ptAppData->ptSecondaryView);
        }

        if(ptAppData->bSecondaryViewActive)
        {
            plDrawFrustumDesc tFrustumDesc = {};
            tFrustumDesc.fAspectRatio = ptSecondaryCamera->fAspectRatio;
            tFrustumDesc.fFarZ        = ptSecondaryCamera->fFarZ;
            tFrustumDesc.fNearZ       = ptSecondaryCamera->fNearZ;
            tFrustumDesc.fYFov        = ptSecondaryCamera->fFieldOfView;
            plDrawLineOptions tDrawCamOptions = {};
            tDrawCamOptions.uColor = PL_COLOR_32_YELLOW;
            tDrawCamOptions.fThickness = 0.02f;
            gptDraw->add_3d_frustum(gptRendererDebug->get_drawlist(ptAppData->ptView), &ptSecondaryCamera->tTransformMat, tFrustumDesc, tDrawCamOptions);
        }
    
        // render scene
        gptRenderer->prepare_scene(ptAppData->ptScene);
        gptRenderer->prepare_view(ptAppData->ptView, ptCamera);
        if(ptAppData->bSecondaryViewActive)
            gptRenderer->prepare_view(ptAppData->ptSecondaryView, ptSecondaryCamera);

        plCamera* ptActiveCullCamera = ptCamera;
        if(ptAppData->bFreezeCullCamera)
            ptActiveCullCamera = ptCullCamera;

        plRenderViewDesc tViewDesc0 = {};
        tViewDesc0.ptCamera = ptCamera;
        tViewDesc0.ptCullCamera = ptAppData->bFrustumCulling ? ptActiveCullCamera : nullptr;
        gptRenderer->render_view(ptAppData->ptView, &tViewDesc0);

        if(ptAppData->bSecondaryViewActive)
        {
            plRenderViewDesc tViewDesc1 = {};
            tViewDesc1.ptCamera = ptSecondaryCamera;
            tViewDesc1.ptCullCamera = ptSecondaryCamera;
            gptRenderer->render_view(ptAppData->ptSecondaryView, &tViewDesc1);
        }
    }

    ImGui::DockSpaceOverViewport(0, 0, ImGuiDockNodeFlags_PassthruCentralNode);

    if(ImGui::BeginMainMenuBar())
    {
        if(ImGui::BeginMenu("File", true))
        {
            if(ImGui::MenuItem("Save Layout"))
                ImGui::SaveIniSettingsToDisk("../internal/demo/pl_imgui.ini");
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Edit", false))
        {
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Tools", true))
        {
            ImGui::SeparatorText("Pilot Light");
            ImGui::MenuItem("Log Tool", nullptr, ptAppData->pbShowLogging);
            ImGui::MenuItem("Stat Tool", nullptr, ptAppData->pbShowStats);
            ImGui::MenuItem("Profile Tool", nullptr, ptAppData->pbShowProfiling);
            ImGui::MenuItem("Allocation Tool", nullptr, ptAppData->pbShowMemoryAllocations);
            ImGui::MenuItem("Device Memory Tool", nullptr, ptAppData->pbShowDeviceMemoryAnalyzer);
            ImGui::SeparatorText("Dear ImGui");
            ImGui::MenuItem("Dear ImGui Demo", nullptr, &ptAppData->bShowImGuiDemo);
            ImGui::MenuItem("Dear ImPlot Demo", nullptr, &ptAppData->bShowPlotDemo);
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Help", false))
        {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // if(ImGui::Begin("Window testing", nullptr, ImGuiWindowFlags_None))
    // {
    //     plWindowAttributeValue tValue = {};
    //     if(ImGui::Button("Maximize"))    gptWindows->set_attribute(ptAppData->ptWindow, PL_WINDOW_ATTRIBUTE_MAXIMIZED, &tValue);
    //     if(ImGui::Button("Minimize"))
    //     {
    //         tValue.bValue = true;
    //         gptWindows->set_attribute(ptAppData->ptWindow, PL_WINDOW_ATTRIBUTE_MINIMIZED, &tValue);
    //     }
    //     if(ImGui::Button("Restore"))
    //     {
    //         tValue.bValue = false;
    //         gptWindows->set_attribute(ptAppData->ptWindow, PL_WINDOW_ATTRIBUTE_MINIMIZED, &tValue);
    //     }
    //     if(ImGui::Button("Hide"))
    //     {
    //         tValue.bValue = true;
    //         gptWindows->set_attribute(ptAppData->ptWindow, PL_WINDOW_ATTRIBUTE_VISIBLE, &tValue);
    //     }
    //     if(ImGui::Button("Set Size"))
    //     {
    //         tValue.tuVec2.x = 500;
    //         tValue.tuVec2.y = 500;
    //         gptWindows->set_attribute(ptAppData->ptWindow, PL_WINDOW_ATTRIBUTE_SIZE, &tValue);
    //     }
    //     if(ImGui::Button("Full Screen"))
    //     {
    //         plFullScreenDesc tFullScreen = {};
    //         tFullScreen.iMonitor = 1;
    //         tFullScreen.tMode = PL_FULLSCREEN_MODE_EXCLUSIVE;
    //         gptWindows->set_fullscreen(ptAppData->ptWindow, &tFullScreen);
    //     }
    // }
    // ImGui::End();

    // main "editor" debug window
    if(ptAppData->bShowPilotLightTool)
        pl__show_editor_window(ptAppData);

    if(ptAppData->bShowUiDemo)
        pl__show_ui_demo_window(ptAppData);

    if(ptAppData->bShowUiStyle)
        gptUI->show_style_editor_window(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        gptUI->show_debug_window(&ptAppData->bShowUiDebug);

    gptDraw->submit_2d_layer(ptAppData->ptDrawLayer);

    pl__show_entity_components(ptAppData, ptAppData->ptScene, ptAppData->tSelectedEntity);

    ImGui::SetNextWindowViewport(ImGui::GetMainViewport()->ID);
    ptAppData->bMainViewHovered = false;
    ImVec2 tLogOffset = {};
    if(ImGui::Begin("Offscreen", nullptr, ImGuiWindowFlags_NoTitleBar))
    {
        if(ImGui::IsWindowHovered())
            ptAppData->bMainViewHovered = true;

        ImVec2 tContextSize = ImGui::GetContentRegionAvail();
        ImVec2 tCursorStart = ImGui::GetCursorScreenPos();
        ImVec2 tHoverMousePos = ImGui::GetMousePos();
        
        ptAppData->tView0Offset = {
            tCursorStart.x - ImGui::GetWindowViewport()->Pos.x,
            tCursorStart.y - ImGui::GetWindowViewport()->Pos.y
        };

        tLogOffset.x = ptAppData->tView0Offset.x;
        tLogOffset.y = ptAppData->tView0Offset.y;

        if(ptAppData->ptScene)
        {

            plCamera* ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);
            if(ptAppData->bMainViewHovered)
                pl__camera_update_imgui(ptCamera);

            ptAppData->tView0Scale = {
                tContextSize.x / ImGui::GetWindowViewport()->Size.x,
                tContextSize.y / ImGui::GetWindowViewport()->Size.y,
            };

            plVec2 tUV = {};
            plBindGroupHandle tTextureHandle = gptRenderer->get_view_color_bind_group(ptAppData->ptView, &tUV);
            ImTextureRef tTexture = ImTextureRef(gptDearImGui->get_texture_id_from_bindgroup(ptAppData->ptDevice, tTextureHandle));
            ImGui::Image(tTexture, tContextSize, ImVec2(0, 0), ImVec2(tUV.x, tUV.y));

        }
    }
    ImGui::End();

    if(ptAppData->bSecondaryViewActive)
    {
        plVec2 tUV = {};
        plBindGroupHandle tTextureHandle = gptRenderer->get_view_color_bind_group(ptAppData->ptSecondaryView, &tUV);
        ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 200.0f), ImVec2(10000.0f, 10000.0f));
        if(ImGui::Begin("Secondary View", &ptAppData->bSecondaryViewActive, ImGuiWindowFlags_NoDocking))
        {
            ImVec2 tContextSize = ImGui::GetContentRegionAvail();
            gptCamera->set_aspect((plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tSecondaryCamera), tContextSize.x / tContextSize.y);

            ImTextureRef tTexture = ImTextureRef(gptDearImGui->get_texture_id_from_bindgroup(ptAppData->ptDevice, tTextureHandle));
            ImGui::Image(tTexture, tContextSize, ImVec2(0, 0), ImVec2(tUV.x, tUV.y));
        }
        ImGui::End();
    }

    if(ptAppData->bShowPlotDemo)
        ImPlot::ShowDemoWindow(&ptAppData->bShowPlotDemo);

    if(ptAppData->bShowImGuiDemo)
        ImGui::ShowDemoWindow(&ptAppData->bShowImGuiDemo);

    plRenderEncoder* ptRenderEncoder = gptStarter->begin_main_pass();
    gptDearImGui->render(ptRenderEncoder, gptGfx->get_encoder_command_buffer(ptRenderEncoder));

    float fWidth = ptIO->tMainViewportSize.x;
    float fHeight = ptIO->tMainViewportSize.y;
    plDrawList2D* ptMessageDrawlist = gptScreenLog->get_drawlist(tLogOffset.x, tLogOffset.y, fWidth * 0.2f, fHeight);
    gptDraw->submit_2d_drawlist(ptMessageDrawlist, ptRenderEncoder, fWidth, fHeight, gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);
    gptStarter->end_main_pass();
    PL_PROFILE_END_SAMPLE_API(gptProfile, 0);
    gptStarter->end_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] helper implementations
//-----------------------------------------------------------------------------

void
pl__find_models(plAppData* ptAppData)
{
    {
        plTestModel tModel = PL_ZERO_INIT;
        tModel.uVariantCount = 1;
        tModel.bSelected = true;
        strcpy(tModel.acName, "Fembot");
        strcpy(tModel.acVariants[0].acType, "glTF");
        strcpy(tModel.acVariants[0].acFilePath, "/assets/core/models/gltf/humanoid.gltf");
        pl_sb_push(ptAppData->sbtTestModels, tModel);
    }

    {
        plTestModel tModel = PL_ZERO_INIT;
        tModel.uVariantCount = 1;
        tModel.bSelected = true;
        strcpy(tModel.acName, "Floor");
        strcpy(tModel.acVariants[0].acType, "glTF");
        strcpy(tModel.acVariants[0].acFilePath, "/assets/core/models/gltf/floor.gltf");
        pl_sb_push(ptAppData->sbtTestModels, tModel);
    }

    if(gptVfs->does_file_exist("/gltf-samples/model-index.json"))
    {
        size_t szJsonFileSize = gptVfs->get_file_size_str("/gltf-samples/model-index.json");
        uint8_t* puFileBuffer = (uint8_t*)PL_ALLOC(szJsonFileSize + 1);
        memset(puFileBuffer, 0, szJsonFileSize + 1);

        plVfsFileHandle tHandle = gptVfs->open_file("/gltf-samples/model-index.json", PL_VFS_FILE_MODE_READ);
        gptVfs->read_file(tHandle, puFileBuffer, &szJsonFileSize);
        gptVfs->close_file(tHandle);

        plJsonObject* ptRootJsonObject = nullptr;
        pl_load_json((const char*)puFileBuffer, &ptRootJsonObject);

        plJsonObject* ptModelList = pl_json_member_by_index(ptRootJsonObject, 0);

        uint32_t uTestModelCount = 0;
        pl_json_member_list(ptModelList, nullptr, &uTestModelCount, nullptr);

        pl_sb_reserve(ptAppData->sbtTestModels, pl_sb_size(ptAppData->sbtTestModels) + uTestModelCount);

        for(uint32_t i = 0; i < uTestModelCount; i++)
        {
            plJsonObject* ptModelObject = pl_json_member_by_index(ptModelList, i);

            plTestModel tTestModel = {0};

            pl_json_string_member(ptModelObject, "label", tTestModel.acLabel, 256);
            pl_json_string_member(ptModelObject, "name", tTestModel.acName, 128);

            plJsonObject* ptVariantsObject = pl_json_member(ptModelObject, "variants");

            char* acVariantNames[8] = {0};
            for(uint32_t j = 0; j < 8; j++)
            { 
                acVariantNames[j] = tTestModel.acVariants[j].acType;
            }
            pl_json_member_list(ptVariantsObject, acVariantNames, &tTestModel.uVariantCount, nullptr);

            for(uint32_t j = 0; j < tTestModel.uVariantCount; j++)
            {
                pl_json_string_member(ptVariantsObject, tTestModel.acVariants[j].acType, tTestModel.acVariants[j].acName, 128);
                pl_sprintf(tTestModel.acVariants[j].acFilePath, "/gltf-samples/%s/%s/%s", tTestModel.acName, tTestModel.acVariants[j].acType, tTestModel.acVariants[j].acName);
            }

            char acTag0[64] = {0};
            char acTag1[64] = {0};
            char acTag2[64] = {0};
            char acTag3[64] = {0};
            char acTag4[64] = {0};

            char* acTags[] = {
                acTag0,
                acTag1,
                acTag2,
                acTag3,
                acTag4,
            };
            uint32_t uTagCount = 0;
            uint32_t uTagLength = 64;
            pl_json_string_array_member(ptVariantsObject, "tags", acTags, &uTagCount, &uTagLength);

            for(uint32_t j = 0; j < uTagCount; j++)
            {
                if(pl_str_equal(acTags[j], "core"))
                    tTestModel.bCore = true;
                if(pl_str_equal(acTags[j], "extensions"))
                    tTestModel.bExtension = true;
                if(pl_str_equal(acTags[j], "testing"))
                    tTestModel.bTesting = true;
            }

            pl_sb_push(ptAppData->sbtTestModels, tTestModel);
        }

        pl_unload_json(&ptRootJsonObject);

        PL_FREE(puFileBuffer);
    }
}

void
pl__show_editor_window(plAppData* ptAppData)
{

    plRendererEditorSceneOptions tEditorSceneOptions = PL_ZERO_INIT;
    plRendererEditorViewOptions tEditorViewOptions = PL_ZERO_INIT;
    plRendererDebugSceneOptions tDebugOptions = PL_ZERO_INIT;
    plRendererTonemapOptions tTonemapOptions = PL_ZERO_INIT;
    plRendererLightingOptions tLightingOptions = PL_ZERO_INIT;
    plRendererShadowOptions tShadowOptions = PL_ZERO_INIT;
    plRendererBloomOptions tBloomOptions = PL_ZERO_INIT;
    plRendererFogOptions tFogOptions = PL_ZERO_INIT;
    
    gptRenderer->get_fog_options(ptAppData->ptScene, &tFogOptions);
    gptRenderer->get_bloom_options(ptAppData->ptView, &tBloomOptions);
    gptRenderer->get_shadow_options(ptAppData->ptScene, &tShadowOptions);
    gptRenderer->get_lighting_options(ptAppData->ptScene, &tLightingOptions);
    gptRenderer->get_tonemap_options(ptAppData->ptView, &tTonemapOptions);
    gptRendererEditor->get_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
    gptRendererEditor->get_view_options(ptAppData->ptView, &tEditorViewOptions);
    gptRendererDebug->get_scene_options(ptAppData->ptScene, &tDebugOptions);

    bool bSceneExists = ptAppData->ptScene != nullptr;

    if(ImGui::Begin("Pilot Light", nullptr, ImGuiWindowFlags_None))
    {
        ImGui::Dummy({25.0f, 15.0f});
        if(ImGui::CollapsingHeader(ICON_FA_CIRCLE_INFO " Information"))
        {
            ImGui::Text("Pilot Light %s", PILOT_LIGHT_VERSION_STRING);
            ImGui::Text("Graphics Backend: %s", gptGfx->get_backend_string());
            if(ImGui::Button("Show Camera Controls"))
            {
                const char* acMouseInfo = "Camera Controls\n"
                "_______________\n"
                "LMB + Drag: Moves camera forward & backward and rotates left & right.\n\n"
                "RMB + Drag: Rotates camera.\n\n"
                "LMB + RMB + Drag: Pans Camera\n\n"
                "Game style (when holding RMB)\n"
                "_____________________________\n"
                "W    Moves the camera forward.\n"
                "S    Moves the camera backward.\n"
                "A    Moves the camera left.\n"
                "D    Moves the camera right.\n"
                "E    Moves the camera up.\n"
                "Q    Moves the camera down.\n"
                "Z    Zooms the camera out (raises FOV).\n"
                "C    Zooms the camera in (lowers FOV).\n";
                gptScreenLog->add_message_ex(651984984, 45.0, PL_COLOR_32_GREEN, 1.5f, acMouseInfo);
            }
        }

        if(ImGui::CollapsingHeader(ICON_FA_SLIDERS " App Options"))
        {

            ImGui::Checkbox("Show Debug Lights", &ptAppData->bShowDebugLights);
            ImGui::Checkbox("Show Bounding Boxes", &ptAppData->bDrawAllBoundingBoxes);
            ImGui::Checkbox("Secondary View", &ptAppData->bSecondaryViewActive);

            if(ptAppData->ptScene)
            {
                if(ImGui::Checkbox("Freeze Culling Camera", &ptAppData->bFreezeCullCamera))
                {
                    plCamera*  ptCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tMainCamera);
                    plCamera*  ptCullCamera = (plCamera*)gptEcs->get_component(ptAppData->ptCompLibrary, gptCameraEcs->get_ecs_type_key(), ptAppData->tCullCamera);
                    *ptCullCamera = *ptCamera;
                }
            }

            bool bLoadScene = false;

            if(ptAppData->ptScene)
            {
                if(ImGui::Button("Unload Scene"))
                {
                    gptPhysics->reset();
                    gptEcs->reset_library(ptAppData->ptCompLibrary);
                    gptRenderer->destroy_view(ptAppData->ptView);
                    gptRenderer->destroy_view(ptAppData->ptSecondaryView);
                    gptRenderer->destroy_scene(ptAppData->ptScene);
                    ptAppData->ptView = nullptr;
                    ptAppData->ptSecondaryView = nullptr;
                    ptAppData->ptScene = nullptr;
                    ptAppData->bSecondaryViewActive = false;
                }
            }
            else
            {
                if(ImGui::Button("Load Scene"))
                {
                    bLoadScene = true;
                }
            }

            ImGui::SameLine();

            if(ImGui::Button("Reset Selection"))
            {
                uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                for(uint32_t i = 0; i < uTestModelCount; i++)
                {
                    ptAppData->sbtTestModels[i].bSelected = false;
                }
            }

            if(ptAppData->ptScene == nullptr)
            {

                static uint32_t uComboSelect = 1;
                static const char* apcEnvMaps[] = {
                    "none",
                    "helipad",
                    "chromatic",
                    "directional",
                    "doge2",
                    "ennis",
                    "field",
                    "footprint_court",
                    "neutral",
                    "papermill",
                    "pisa",
                    "asteroid_field",
                    "brown_dwarf",
                    "galaxy",
                    "nebulae",
                    "planet",
                    "ringed_planet",
                    "hay_bales",
                    "sunset",
                    "sunset2",
                    "sunset3",
                    "sunrise",
                    "sky",
                    "country_road",
                };
                bool abCombo[24] = {0};
                abCombo[uComboSelect] = true;
                if(ImGui::BeginCombo("Environment", apcEnvMaps[uComboSelect]))
                {
                    for(uint32_t i = 0; i < PL_ARRAYSIZE(apcEnvMaps); i++)
                    {
                        if(ImGui::Selectable(apcEnvMaps[i], &abCombo[i], 0))
                        {
                            uComboSelect = i;
                        }
                    }
                    ImGui::EndCombo();
                }

                // if(ImGui::InputTextWithHint(ICON_FA_MAGNIFYING_GLASS, "Filter (inc,-exc)", ptAppData->tFilter.acInputBuffer, 256, 0))
                // {
                //     gptUI->text_filter_build(&ptAppData->tFilter);
                // }
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    ptAppData->filter.Clear();
                }
                ptAppData->filter.Draw(ICON_FA_MAGNIFYING_GLASS);

                if(ImGui::BeginListBox("GLTF Models"))
                {
                    uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                    // for(uint32_t i = 0; i < uTestModelCount; i++)
                    //     ImGui::Selectable(ptAppData->sbtTestModels[i].acName, &ptAppData->sbtTestModels[i].bSelected);


        
                    for (uint32_t n = 0; n < uTestModelCount; n++)
                    {
                        if (ptAppData->filter.PassFilter(ptAppData->sbtTestModels[n].acName))
                            ImGui::Selectable(ptAppData->sbtTestModels[n].acName, &ptAppData->sbtTestModels[n].bSelected);
                    }

                   ImGui::EndListBox();
                }

                if(bLoadScene)
                {

                    pl__create_scene(ptAppData);
                    
                    if(uComboSelect > 0)
                    {
                        char* sbcData = nullptr;
                        pl_sb_sprintf(sbcData, "/environments/%s.hdr", apcEnvMaps[uComboSelect]);
                        gptRendererEcs->load_skybox_from_panorama(ptAppData->ptScene, sbcData, 1024);
                        pl_sb_free(sbcData);
                    }
                    plIO* ptIO = gptIO->get_io();

                    plViewDesc tViewDesc0 = PL_ZERO_INIT;
                    tViewDesc0.uWidth = (uint32_t)ptIO->tMainViewportSize.x;
                    tViewDesc0.uHeight = (uint32_t)ptIO->tMainViewportSize.y;

                    plViewDesc tViewDesc1 = PL_ZERO_INIT;
                    tViewDesc1.uWidth = 500;
                    tViewDesc1.uHeight = 500;
                    ptAppData->ptView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc0);
                    ptAppData->ptSecondaryView = gptRenderer->create_view(ptAppData->ptScene, &tViewDesc1);

                    

                    uint32_t uTestModelCount = pl_sb_size(ptAppData->sbtTestModels);
                    for(uint32_t i = 0; i < uTestModelCount; i++)
                    {
                        if(ptAppData->sbtTestModels[i].bSelected)
                        {
                            for(uint32_t j = 0; j < ptAppData->sbtTestModels[i].uVariantCount; j++)
                            {
                                if(ptAppData->sbtTestModels[i].acVariants[j].acType[4] != '-')
                                {
                                    plModelInstanceHandle tHandle = gptModelLoader->load_gltf(ptAppData->ptCompLibrary, ptAppData->sbtTestModels[i].acVariants[j].acFilePath, nullptr);
                                    const plModelLoaderData* ptLoaderData = gptModelLoader->get_objects(tHandle);
                                    gptRendererEcs->add_drawable_objects_to_scene(ptAppData->ptScene, ptLoaderData->uObjectCount, ptLoaderData->atObjects);
                                    gptModelLoader->free_data(tHandle);
                                    break;
                                }
                            }
                        }
                    }


                }

            }
        }

        bool bReloadShaders = false;

        if(ptAppData->ptScene && ImGui::CollapsingHeader(ICON_FA_DICE_D6 " Graphics"))
        {
            if(ImGui::Checkbox("VSync", &ptAppData->bVSync))
            {
                if(ptAppData->bVSync)
                    gptStarter->activate_vsync();
                else
                    gptStarter->deactivate_vsync();
            }

            static const char* apcShaderDebugModeText[] = {
                "None",
                "Base Color",
                "Metallic",
                "Roughness",
                "Alpha",
                "Emissive",
                "Occlusion",
                "Shading Normal",
                "Texture Normal",
                "Geometry Normal",
                "Geometry Tangent",
                "Geometry Bitangent",
                "UV 0",
                "Clearcoat",
                "Clearcoat Roughness",
                "Clearcoat Normal",
                "Sheen Color",
                "Sheen Roughness",
                "Iridescence Factor",
                "Iridescence Thickness",
                "Anisotropy Strength",
                "Anisotropy Direction",
                "Transmission Strength",
                "Volume Thickness",
                "Diffuse Transmission Strength",
                "Diffuse Transmission Color",
            };
            
            if(ImGui::Combo("Shader Debug Mode", &tDebugOptions.tShaderDebugMode, apcShaderDebugModeText, PL_ARRAYSIZE(apcShaderDebugModeText))) bReloadShaders = true;
            ImGui::Checkbox("Show Origin", &tDebugOptions.bShowOrigin);
            ImGui::Checkbox("Show BVH", &ptAppData->bShowBVH);
            ImGui::Checkbox("Show Skybox", &tEditorViewOptions.bShowSkybox);
            ImGui::Checkbox("Show Grid", &tEditorViewOptions.bShowGrid);
            if(ImGui::Checkbox("Wireframe", &tDebugOptions.bWireframe)) bReloadShaders = true;

            bool bImageBasedLighting = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
            bool bPunctualLighting = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
            bool bNormalMapping = tLightingOptions.tFlags & PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
            
            if(ImGui::Checkbox("Image Based Lighting", &bImageBasedLighting))
            {
                if(bImageBasedLighting) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
                else                    tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_IMAGE_BASED;
                bReloadShaders = true;
            }

            if(ImGui::Checkbox("Punctual Lighting", &bPunctualLighting))
            {
                if(bPunctualLighting) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
                else                    tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_PUNCTUAL_LIGHTS;
                bReloadShaders = true;
            }

            if(ImGui::Checkbox("Normal Mapping", &bNormalMapping))
            {
                if(bNormalMapping) tLightingOptions.tFlags |= PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
                else               tLightingOptions.tFlags &= ~PL_RENDERER_LIGHTING_FLAGS_NORMAL_MAPPING;
                bReloadShaders = true;
            }

            bool bMultiViewportShadows = tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
            bool bPcfShadows = tShadowOptions.tFlags & PL_RENDERER_SHADOW_FLAGS_PCF;

            if(ImGui::Checkbox("MultiViewport Shadows", &bMultiViewportShadows))
            {
                if(bMultiViewportShadows) tShadowOptions.tFlags |= PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
                else                      tShadowOptions.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_MULTI_VIEWPORT;
                bReloadShaders = true;
            }

            if(ImGui::Checkbox("PCF Shadows", &bPcfShadows))
            {
                if(bPcfShadows) tShadowOptions.tFlags |= PL_RENDERER_SHADOW_FLAGS_PCF;
                else            tShadowOptions.tFlags &= ~PL_RENDERER_SHADOW_FLAGS_PCF;
                bReloadShaders = true;
            }

            ImGui::Checkbox("Show Probes", &tDebugOptions.bShowProbes);

            ImGui::Checkbox("Frustum Culling", &ptAppData->bFrustumCulling);
            ImGui::Checkbox("Selected Bounding Box", &tEditorViewOptions.bShowSelectedBoundingBox);
            
            ImGui::InputFloat("Depth Bias", &tShadowOptions.fConstantDepthBias);
            ImGui::InputFloat("Slope Depth Bias", &tShadowOptions.fSlopeDepthBias, 0.0f, 0);

            uint32_t uMinOutline = 2;
            uint32_t uMaxOutline = 50;
            ImGui::SliderScalar("Outline Width", ImGuiDataType_U32, &tEditorViewOptions.uOutlineWidth, &uMinOutline, &uMaxOutline, 0);
            

            if(ptAppData->ptScene)
            {
                if(ImGui::TreeNode("Scene"))
                {
                    ImGui::Checkbox("Dynamic BVH", &ptAppData->bContinuousBVH);
                    if(ImGui::Button("Build BVH") || ptAppData->bContinuousBVH)
                        gptRendererEditor->rebuild_scene_bvh(ptAppData->ptScene);
                    ImGui::TreePop();
                }
            }

        }

        if(ptAppData->ptScene && ImGui::CollapsingHeader(ICON_FA_FILE_IMAGE " Post Process"))
        {

            static const char* apcTonemapText[] = {
                "None",
                "Simple",
                "ACES Filmic (Narkowicz)",
                "ACES Filmic (Hill)",
                "ACES Filmic (Hill Exposure Boost)",
                "Reinhard",
                "Khronos PBR Neutral",
            };
            ImGui::Combo("Tonemapping", &tTonemapOptions.tMode, apcTonemapText, PL_ARRAYSIZE(apcTonemapText));

            ImGui::SliderFloat("Exposure", &tTonemapOptions.fExposure, 0.0f, 3.0f);
            ImGui::SliderFloat("Brightness", &tTonemapOptions.fBrightness, -1.0f, 1.0f);
            ImGui::SliderFloat("Contrast", &tTonemapOptions.fContrast, 0.0f, 2.0f);
            ImGui::SliderFloat("Saturation", &tTonemapOptions.fSaturation, 0.0f, 2.0f);

            ImGui::SeparatorText("Bloom");
            bool bBloomActive = tBloomOptions.tFlags & PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            ImGui::Checkbox("Bloom", &bBloomActive);

            if(bBloomActive)
            {
                ImGui::SliderFloat("Bloom Radius", &tBloomOptions.fRadius, 0.0f, 10.0f, 0);
                ImGui::SliderFloat("Bloom Strength", &tBloomOptions.fStrength, 0.0f, 1.0f, 0);
                int iBloomChainLength = (int)tBloomOptions.uChainLength;
                if(ImGui::SliderInt("Bloom Chain", &iBloomChainLength, 2, 10, 0))
                    tBloomOptions.uChainLength = (uint32_t)iBloomChainLength;
                tBloomOptions.tFlags |= PL_RENDERER_BLOOM_FLAGS_ACTIVE;
            }
            else
                tBloomOptions.tFlags &= ~PL_RENDERER_BLOOM_FLAGS_ACTIVE;

            ImGui::SeparatorText("Fog");

            bool bFog = tFogOptions.tFlags & PL_RENDERER_FOG_FLAGS_ACTIVE;
            ImGui::Checkbox("Fog", &bFog);
            if(bFog)
            {
                tFogOptions.tFlags |= PL_RENDERER_FOG_FLAGS_ACTIVE;
                ImGui::RadioButton("Linear Fog", &tFogOptions.tMode, 0);
                ImGui::RadioButton("Exponential Fog", &tFogOptions.tMode, 1);
                ImGui::SliderFloat("Fog Start", &tFogOptions.fStart, 0.0f, 100.0f);
                ImGui::SliderFloat("Fog End", &tFogOptions.fCutOffDistance, 0.0f, 10000.0f);
                ImGui::ColorEdit3("Fog Color", tFogOptions.tColor.d);
                if(tFogOptions.tMode == PL_RENDERER_FOG_MODE_EXPONENTIAL)
                {
                    ImGui::SliderFloat("Fog Max Opacity", &tFogOptions.fMaxOpacity, 0.0f, 1.0f);
                    ImGui::SliderFloat("Fog Density", &tFogOptions.fDensity, 0.0f, 1.0f);
                    ImGui::SliderFloat("Fog Height", &tFogOptions.fHeight, -100.0f, 100.0f);
                    ImGui::SliderFloat("Fog Height Falloff", &tFogOptions.fHeightFalloff, 0.0f, 1.0f);
                }  
            }
            else
                tFogOptions.tFlags &= ~PL_RENDERER_FOG_FLAGS_ACTIVE;
        }

        if(bReloadShaders)
        {
            gptRendererEditor->reload_scene_shaders(ptAppData->ptScene);
        }

        if(ImGui::CollapsingHeader(ICON_FA_BOXES_STACKED " Physics", 0))
        {
            plPhysicsEngineSettings tPhysicsSettings = {};
            gptPhysics->get_settings(&tPhysicsSettings);

            ImGui::Checkbox("Enabled", &tPhysicsSettings.bEnabled);
            ImGui::Checkbox("Debug Draw", &ptAppData->bPhysicsDebugDraw);
            ImGui::SliderFloat("Simulation Speed", &tPhysicsSettings.fSimulationMultiplier, 0.01f, 3.0f);
            ImGui::InputFloat("Sleep Epsilon", &tPhysicsSettings.fSleepEpsilon);
            ImGui::InputFloat("Position Epsilon", &tPhysicsSettings.fPositionEpsilon);
            ImGui::InputFloat("Velocity Epsilon", &tPhysicsSettings.fVelocityEpsilon);
            ImGui::InputScalar("Max Position Its.", ImGuiDataType_U32, &tPhysicsSettings.uMaxPositionIterations);
            ImGui::InputScalar("Max Velocity Its.", ImGuiDataType_U32, &tPhysicsSettings.uMaxVelocityIterations);
            ImGui::InputFloat("Frame Rate", &tPhysicsSettings.fSimulationFrameRate);
            if(ImGui::Button("Wake All")) gptPhysics->wake_up_all();
            if(ImGui::Button("Sleep All")) gptPhysics->sleep_all();

            gptPhysics->set_settings(tPhysicsSettings);

        }

        if(ImGui::CollapsingHeader(ICON_FA_SCREWDRIVER_WRENCH " Tools"))
        {
            ImGui::Checkbox("Device Memory", ptAppData->pbShowDeviceMemoryAnalyzer);
            ImGui::Checkbox("Memory Allocations", ptAppData->pbShowMemoryAllocations);
            ImGui::Checkbox("Profiling", ptAppData->pbShowProfiling);
            ImGui::Checkbox("Statistics", ptAppData->pbShowStats);
            ImGui::Checkbox("Logging", ptAppData->pbShowLogging);
            ImGui::Checkbox("Entities", &ptAppData->bShowEntityWindow);
        }
        if(ImGui::CollapsingHeader(ICON_FA_USER_GEAR " User Interface"))
        {
            ImGui::Checkbox("UI Demo", &ptAppData->bShowUiDemo);
            ImGui::Checkbox("UI Debug", &ptAppData->bShowUiDebug);
            ImGui::Checkbox("UI Style", &ptAppData->bShowUiStyle);
        }

        if(ImGui::CollapsingHeader(ICON_FA_PHOTO_FILM " Renderer"))
        {

            if(ImGui::Button("Reload Shaders"))
                gptRendererEditor->reload_scene_shaders(ptAppData->ptScene);
        }
    }
    ImGui::End();

    if(bSceneExists)
    {
        gptRenderer->set_tonemap_options(ptAppData->ptView, &tTonemapOptions);
        gptRenderer->set_lighting_options(ptAppData->ptScene, &tLightingOptions);
        gptRendererEditor->set_scene_options(ptAppData->ptScene, &tEditorSceneOptions);
        gptRendererEditor->set_view_options(ptAppData->ptView, &tEditorViewOptions);
        gptRenderer->set_bloom_options(ptAppData->ptView, &tBloomOptions);
        gptRenderer->set_fog_options(ptAppData->ptScene, &tFogOptions);
        gptRenderer->set_shadow_options(ptAppData->ptScene, &tShadowOptions);
        gptRendererDebug->set_scene_options(ptAppData->ptScene, &tDebugOptions);
    }
}

void
pl__create_scene(plAppData* ptAppData)
{
    plIO* ptIO = gptIO->get_io();
    plSceneDesc tSceneInit = {ptAppData->ptCompLibrary};
    ptAppData->ptScene = gptRenderer->create_scene(&tSceneInit);

    // create main camera
    plCamera* ptMainCamera = nullptr;
    ptAppData->tMainCamera = gptCameraEcs->create_perspective(ptAppData->ptCompLibrary, "main camera", pl_create_vec3_d(-4.7, 4.2, -3.256), PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 48.0f, true, &ptMainCamera);
    gptCamera->set_pitch_yaw(ptMainCamera, 0.0f, 0.911f);
    gptCamera->update(ptMainCamera);

    // create cull camera
    plCamera* ptCullCamera = nullptr;
    ptAppData->tCullCamera = gptCameraEcs->create_perspective(ptAppData->ptCompLibrary, "cull camera", pl_create_vec3_d(0, 0, 5.0), PL_PI_3, ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y, 0.1f, 25.0f, true, &ptCullCamera);
    gptCamera->set_pitch_yaw(ptCullCamera, 0.0f, PL_PI);
    gptCamera->update(ptCullCamera);

    // create secondary camera
    plCamera* ptSecondaryCamera = nullptr;
    ptAppData->tSecondaryCamera = gptCameraEcs->create_perspective(ptAppData->ptCompLibrary, "secondary camera", pl_create_vec3_d(-4.7, 4.2, -3.256), PL_PI_3, 1.0f, 0.1f, 20.0f, true, &ptSecondaryCamera);
    gptCamera->set_pitch_yaw(ptSecondaryCamera, -0.1f, 0.911f);
    gptCamera->update(ptSecondaryCamera);
    plTransformComponent* ptSecondaryCameraTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), ptAppData->tSecondaryCamera);
    ptSecondaryCameraTransform->tTranslation = pl_create_vec3(-4.7f, 4.2f, -3.256f);

    // create lights
    plLightComponent* ptLight = nullptr;
    plEntity tDirectionLight = gptRendererEcs->create_directional_light(ptAppData->ptCompLibrary, "direction light", pl_create_vec3(-0.375f, -1.0f, -0.085f), &ptLight);
    ptLight->uCascadeCount = 4;
    ptLight->fIntensity = 1.0f;
    ptLight->fRange = 1.0f;
    ptLight->uShadowResolution = 1024 * 2;
    ptLight->afCascadeSplits[0] = 0.10f;
    ptLight->afCascadeSplits[1] = 0.25f;
    ptLight->afCascadeSplits[2] = 0.50f;
    ptLight->afCascadeSplits[3] = 1.00f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;

    plEntity tPointLight = gptRendererEcs->create_point_light(ptAppData->ptCompLibrary, "point light", pl_create_vec3(0.0f, 2.0f, 2.0f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptPLightTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), tPointLight);
    ptPLightTransform->tTranslation = pl_create_vec3(9.316f, 1.497f, -1.042f);

    plEntity tSpotLight = gptRendererEcs->create_spot_light(ptAppData->ptCompLibrary, "spot light", pl_create_vec3(0.0f, 4.0f, -1.18f), pl_create_vec3(0.0, -1.0f, 0.376f), &ptLight);
    ptLight->uShadowResolution = 1024;
    ptLight->fRange = 5.0f;
    ptLight->fRadius = 0.025f;
    ptLight->fIntensity = 20.0f;
    ptLight->tFlags |= PL_LIGHT_FLAG_CAST_SHADOW | PL_LIGHT_FLAG_VISUALIZER;
    plTransformComponent* ptSLightTransform = (plTransformComponent* )gptEcs->add_component(ptAppData->ptCompLibrary, gptEcs->get_ecs_type_key_transform(), tSpotLight);
    ptSLightTransform->tTranslation = pl_create_vec3(0.0f, 4.0f, -1.18f);

    plEnvironmentProbeComponent* ptProbe = nullptr;
    plEntity tProbeEntity = gptRendererEcs->create_environment_probe(ptAppData->ptCompLibrary, "Main Probe", pl_create_vec3(0.0f, 3.0f, 0.0f), &ptProbe);
    ptProbe->fRange = 30.0f;
    ptProbe->uResolution = 128;
    ptProbe->tFlags |= PL_ENVIRONMENT_PROBE_FLAGS_INCLUDE_SKY;

    gptRendererEcs->add_probes_to_scene(ptAppData->ptScene, 1, &tProbeEntity);

    gptRendererEcs->add_lights_to_scene(ptAppData->ptScene, 1, &tPointLight);
    gptRendererEcs->add_lights_to_scene(ptAppData->ptScene, 1, &tSpotLight);
    gptRendererEcs->add_lights_to_scene(ptAppData->ptScene, 1, &tDirectionLight);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "demo_camera.cpp"
#include "demo_entities.cpp"
#include "demo_ui_demo.cpp"

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"