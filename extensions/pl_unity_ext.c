/*
    pl_unity_ext.c
      * unity build for stable extensions
*/

/*
Index of this file:
// [SECTION] unity build #1
// [SECTION] extension loading
// [SECTION] unity build #2
*/

//-----------------------------------------------------------------------------
// [SECTION] unity build #1
//-----------------------------------------------------------------------------

#include "pl.h"
#include "pl_unity_ext.h"
#include "pl_unity_ext.inc"

#include "pl_stage_ext.c"
#include "pl_shader_ext.c"
#include "pl_image_ext.c"
#include "pl_rect_pack_ext.c"
#include "pl_stats_ext.c"
#include "pl_job_ext.c"
#include "pl_string_intern_ext.c"
#include "pl_draw_ext.c"
#include "pl_gpu_allocators_ext.c"
#include "pl_ui_ext.c"
#include "pl_graphics_ext.c"
#include "pl_ecs_ext.c"
#include "pl_camera_ext.c"
#include "pl_platform_ext.h"
#include "pl_resource_ext.c"
#include "pl_model_loader_ext.c"
#include "pl_renderer_ext.c"
#include "pl_tools_ext.c"
#include "pl_profile_ext.c"
#include "pl_log_ext.c"
#include "pl_ecs_tools_ext.c"
#include "pl_gizmo_ext.c"
#include "pl_console_ext.c"
#include "pl_screen_log_ext.c"
#include "pl_physics_ext.c"
#include "pl_collision_ext.c"
#include "pl_bvh_ext.c"
#include "pl_config_ext.c"
#include "pl_starter_ext.c"
#include "pl_animation_ext.c"
#include "pl_mesh_ext.c"
#include "pl_shader_variant_ext.c"
#include "pl_vfs_ext.c"
#include "pl_pak_ext.c"
#include "pl_datetime_ext.c"
#include "pl_compress_ext.c"
#include "pl_dds_ext.c"
#include "pl_dxt_ext.c"
#include "pl_material_ext.c"
#include "pl_script_ext.c"
#include "pl_voxel_ext.c"
#include "pl_path_ext.c"
#include "pl_audio_ext.c"
#include "pl_terrain_ext.c"
#include "pl_freelist_ext.c"
#include "pl_image_ops_ext.c"
#include "pl_gjk_ext.c"

//-----------------------------------------------------------------------------
// [SECTION] public api
//-----------------------------------------------------------------------------

void 
pl_io_new_frame(void)
{
    gptIOI->new_frame();
}

plIO*
pl_io_get_io(void)
{
    return gptIOI->get_io();
}

bool
pl_io_is_key_down(plKey tKey)
{
    return gptIOI->is_key_down(tKey);
}

bool
pl_io_is_key_pressed(plKey tKey, bool bRepeat)
{
    return gptIOI->is_key_pressed(tKey, bRepeat);
}

bool
pl_io_is_key_released(plKey tKey)
{
    return gptIOI->is_key_released(tKey);
}

int
pl_io_get_key_pressed_amount(plKey tKey, float repeatDelay, float rate)
{
    return gptIOI->get_key_pressed_amount(tKey, repeatDelay, rate);
}

bool
pl_io_is_mouse_down(plMouseButton tButton)
{
    return gptIOI->is_mouse_down(tButton);
}

bool
pl_io_is_mouse_clicked(plMouseButton tButton, bool repeat)
{
    return gptIOI->is_mouse_clicked(tButton, repeat);
}

bool
pl_io_is_mouse_released(plMouseButton tButton)
{
    return gptIOI->is_mouse_released(tButton);
}

bool
pl_io_is_mouse_double_clicked(plMouseButton tButton)
{
    return gptIOI->is_mouse_double_clicked(tButton);
}

bool
pl_io_is_mouse_dragging(plMouseButton tButton, float threshold)
{
    return gptIOI->is_mouse_dragging(tButton, threshold);
}

bool
pl_io_is_mouse_hovering_rect(plVec2 tMinVec, plVec2 maxVec)
{
    return gptIOI->is_mouse_hovering_rect(tMinVec, maxVec);
}

void
pl_io_reset_mouse_drag_delta(plMouseButton tButton)
{
    gptIOI->reset_mouse_drag_delta(tButton);
}

plVec2
pl_io_get_mouse_drag_delta(plMouseButton tButton, float fThreshold)
{
    return gptIOI->get_mouse_drag_delta(tButton, fThreshold);
}

plVec2
pl_io_get_mouse_pos(void)
{
    return gptIOI->get_mouse_pos();
}

float
pl_io_get_mouse_wheel(void)
{
    return gptIOI->get_mouse_wheel();
}

bool
pl_io_is_mouse_pos_valid(plVec2 tPos)
{
    return gptIOI->is_mouse_pos_valid(tPos);
}

void
pl_io_set_mouse_cursor(plMouseCursor tCursor)
{
    gptIOI->set_mouse_cursor(tCursor);
}

void
pl_io_add_key_event(plKey tKey, bool bDown)
{
    gptIOI->add_key_event(tKey, bDown);
}

void
pl_io_add_text_event(uint32_t uChar)
{
    gptIOI->add_text_event(uChar);
}

void
pl_io_add_text_event_utf16(uint16_t uChar)
{
    gptIOI->add_text_event_utf16(uChar);
}

void
pl_io_add_text_events_utf8(const char* pcText)
{
    gptIOI->add_text_events_utf8(pcText);
}

void
pl_io_add_mouse_pos_event(float x, float y)
{
    gptIOI->add_mouse_pos_event(x, y);
}

void
pl_io_add_mouse_button_event(int tButton, bool down)
{
    gptIOI->add_mouse_button_event(tButton, down);
}

void
pl_io_add_mouse_wheel_event(float horizontalDelta, float verticalDelta)
{
    gptIOI->add_mouse_wheel_event(horizontalDelta, verticalDelta);
}

void
pl_io_clear_input_characters(void)
{
    gptIOI->clear_input_characters();
}

plVersion
pl_io_get_version(void)
{
    return gptIOI->get_version();
}

const char*
pl_io_get_version_string(void)
{
    return gptIOI->get_version_string();
}

void
pl_data_registry_set_data(const char* pcName, void* pData)
{
    gptDataRegistry->set_data(pcName, pData);
}

void*
pl_data_registry_get_data(const char* pcName)
{
    return gptDataRegistry->get_data(pcName);
}

const plWindowCapabilities*
pl_window_get_capabilities(void)
{
    return gptWindow->get_capabilities();
}

plWindowResult
pl_window_create(plWindowDesc tDesc, plWindow** windowPtrOut)
{
    return gptWindow->create(tDesc, windowPtrOut);
}

void
pl_window_destroy(plWindow* ptWindow)
{
    gptWindow->destroy(ptWindow);
}

void
pl_window_show(plWindow* ptWindow)
{
    gptWindow->show(ptWindow);
}

bool
pl_window_set_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, const plWindowAttributeValue* ptValue)
{
    return gptWindow->set_attribute(ptWindow, tAttribute, ptValue);
}

bool
pl_window_get_attribute(plWindow* ptWindow, plWindowAttribute tAttribute, plWindowAttributeValue* ptValue)
{
    return gptWindow->get_attribute(ptWindow, tAttribute, ptValue);
}

bool
pl_window_set_cursor_mode(plWindow* ptWindow, plCursorMode tMode)
{
    return gptWindow->set_cursor_mode(ptWindow, tMode);
}

plCursorMode
pl_window_get_cursor_mode(plWindow* ptWindow)
{
    return gptWindow->get_cursor_mode(ptWindow);
}

bool
pl_window_set_raw_mouse_input(plWindow* ptWindow, bool bValue)
{
    return gptWindow->set_raw_mouse_input(ptWindow, bValue);
}

bool
pl_window_set_fullscreen(plWindow* ptWindow, const plFullScreenDesc* ptDesc)
{
    return gptWindow->set_fullscreen(ptWindow, ptDesc);
}

void
pl_window_set_callback(plWindow* ptWindow, plWindowEventCallback tCallback, void* userData)
{
    gptWindow->set_callback(ptWindow, tCallback, userData);
}

plWindowEventCallback
pl_window_get_callback(plWindow* ptWindow)
{
    return gptWindow->get_callback(ptWindow);
}

plLibraryResult
pl_library_load(plLibraryDesc tDesc, plSharedLibrary** libraryPtrOut)
{
    return gptLibrary->load(tDesc, libraryPtrOut);
}

bool
pl_library_has_changed(plSharedLibrary* ptLibrary)
{
    return gptLibrary->has_changed(ptLibrary);
}

void*
pl_library_load_function(plSharedLibrary* ptLibrary, const char* pcName)
{
    return gptLibrary->load_function(ptLibrary, pcName);
}

bool
pl_extension_registry_load(const char* name, const char* loadFunc, const char* unloadFunc, bool reloadable)
{
    return gptExtensionRegistry->load(name, loadFunc, unloadFunc, reloadable);
}

bool
pl_extension_registry_unload(const char* name)
{
    return gptExtensionRegistry->unload(name);
}

void
pl_extension_registry_add_path(const char* path)
{
    gptExtensionRegistry->add_path(path);
}

PL_API void*
pl_memory_realloc(void* pData, size_t szSize)
{
    return gptMemory->realloc(pData, szSize);
}

PL_API void*
pl_memory_tracked_realloc(void* pData, size_t szSize, const char* file, int line)
{
    return gptMemory->tracked_realloc(pData, szSize, file, line);
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

void
pl_load_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    gptApiRegistry       = ptApiRegistry;
    gptDataRegistry      = pl_get_api_latest(ptApiRegistry, plDataRegistryI);
    gptWindow            = pl_get_api_latest(ptApiRegistry, plWindowI);
    gptLibrary           = pl_get_api_latest(ptApiRegistry, plLibraryI);
    gptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);
    gptMemory            = pl_get_api_latest(ptApiRegistry, plMemoryI);
    gptString            = pl_get_api_latest(ptApiRegistry, plStringInternI);
    gptIOI               = pl_get_api_latest(ptApiRegistry, plIOI);
    gptFile              = pl_get_api_latest(ptApiRegistry, plFileI);
    gptThreads           = pl_get_api_latest(ptApiRegistry, plThreadsI);
    gptAtomics           = pl_get_api_latest(ptApiRegistry, plAtomicsI);
    gptStats             = pl_get_api_latest(ptApiRegistry, plStatsI);
    gptImage             = pl_get_api_latest(ptApiRegistry, plImageI);
    gptJob               = pl_get_api_latest(ptApiRegistry, plJobI);
    gptRect              = pl_get_api_latest(ptApiRegistry, plRectPackI);
    gptGfx               = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptGpuAllocators     = pl_get_api_latest(ptApiRegistry, plGPUAllocatorsI);
    gptDraw              = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptUI                = pl_get_api_latest(ptApiRegistry, plUiI);
    gptECS               = pl_get_api_latest(ptApiRegistry, plEcsI);
    gptCamera            = pl_get_api_latest(ptApiRegistry, plCameraI);
    gptResource          = pl_get_api_latest(ptApiRegistry, plResourceI);
    gptProfile           = pl_get_api_latest(ptApiRegistry, plProfileI);
    gptLog               = pl_get_api_latest(ptApiRegistry, plLogI);
    gptRenderer          = pl_get_api_latest(ptApiRegistry, plRendererI);
    gptEcsTools          = pl_get_api_latest(ptApiRegistry, plEcsToolsI);
    gptConsole           = pl_get_api_latest(ptApiRegistry, plConsoleI);
    gptGizmo             = pl_get_api_latest(ptApiRegistry, plGizmoI);
    gptScreenLog         = pl_get_api_latest(ptApiRegistry, plScreenLogI);
    gptPhysics           = pl_get_api_latest(ptApiRegistry, plPhysicsI);
    gptCollision         = pl_get_api_latest(ptApiRegistry, plCollisionI);
    gptBvh               = pl_get_api_latest(ptApiRegistry, plBVHI);
    gptConfig            = pl_get_api_latest(ptApiRegistry, plConfigI);
    gptStarter           = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptTools             = pl_get_api_latest(ptApiRegistry, plToolsI);
    gptAnimation         = pl_get_api_latest(ptApiRegistry, plAnimationI);
    gptMesh              = pl_get_api_latest(ptApiRegistry, plMeshI);
    gptMeshBuilder       = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    gptShaderVariant     = pl_get_api_latest(ptApiRegistry, plShaderVariantI);
    gptVfs               = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptPak               = pl_get_api_latest(ptApiRegistry, plPakI);
    gptDateTime          = pl_get_api_latest(ptApiRegistry, plDateTimeI);
    gptCompress          = pl_get_api_latest(ptApiRegistry, plCompressI);
    gptDds               = pl_get_api_latest(ptApiRegistry, plDdsI);
    gptDxt               = pl_get_api_latest(ptApiRegistry, plDxtI);
    gptMaterial          = pl_get_api_latest(ptApiRegistry, plMaterialI);
    gptScript            = pl_get_api_latest(ptApiRegistry, plScriptI);
    gptFreeList          = pl_get_api_latest(ptApiRegistry, plFreeListI);
    gptTerrain           = pl_get_api_latest(ptApiRegistry, plTerrainI);
    gptImageOps          = pl_get_api_latest(ptApiRegistry, plImageOpsI);
    gptPath              = pl_get_api_latest(ptApiRegistry, plPathI);
    gptVoxel             = pl_get_api_latest(ptApiRegistry, plVoxelI);
    gptAudio             = pl_get_api_latest(ptApiRegistry, plAudioI);
    gptStage             = pl_get_api_latest(ptApiRegistry, plStageI);
    gptGjk               = pl_get_api_latest(ptApiRegistry, plGjkI);
    gptIO = gptIOI->get_io();

    pl_load_log_ext(ptApiRegistry, bReload);
    pl_load_image_ext(ptApiRegistry, bReload);
    pl_load_rect_pack_ext(ptApiRegistry, bReload);
    pl_load_stats_ext(ptApiRegistry, bReload);
    pl_load_job_ext(ptApiRegistry, bReload);
    pl_load_string_intern_ext(ptApiRegistry, bReload);
    pl_load_graphics_ext(ptApiRegistry, bReload);
    pl_load_gpu_allocators_ext(ptApiRegistry, bReload);
    pl_load_draw_ext(ptApiRegistry, bReload);
    pl_load_ui_ext(ptApiRegistry, bReload);
    pl_load_vfs_ext(ptApiRegistry, bReload);
    pl_load_shader_ext(ptApiRegistry, bReload);
    gptShader = pl_get_api_latest(ptApiRegistry, plShaderI);

    pl_load_ecs_ext(ptApiRegistry, bReload);
    pl_load_camera_ext(ptApiRegistry, bReload);
    pl_load_resource_ext(ptApiRegistry, bReload);
    pl_load_model_loader_ext(ptApiRegistry, bReload);
    pl_load_renderer_ext(ptApiRegistry, bReload);
    pl_load_animation_ext(ptApiRegistry, bReload);
    pl_load_ecs_tools_ext(ptApiRegistry, bReload);
    pl_load_gizmo_ext(ptApiRegistry, bReload);
    pl_load_console_ext(ptApiRegistry, bReload);
    pl_load_screen_log_ext(ptApiRegistry, bReload);
    pl_load_tools_ext(ptApiRegistry, bReload);
    pl_load_profile_ext(ptApiRegistry, bReload);
    pl_load_physics_ext(ptApiRegistry, bReload);
    pl_load_collision_ext(ptApiRegistry, bReload);
    pl_load_bvh_ext(ptApiRegistry, bReload);
    pl_load_mesh_ext(ptApiRegistry, bReload);
    pl_load_config_ext(ptApiRegistry, bReload);
    pl_load_starter_ext(ptApiRegistry, bReload);
    pl_load_pak_ext(ptApiRegistry, bReload);
    pl_load_datetime_ext(ptApiRegistry, bReload);
    pl_load_shader_variant_ext(ptApiRegistry, bReload);
    pl_load_compress_ext(ptApiRegistry, bReload);
    pl_load_dds_ext(ptApiRegistry, bReload);
    pl_load_dxt_ext(ptApiRegistry, bReload);
    pl_load_material_ext(ptApiRegistry, bReload);
    pl_load_script_ext(ptApiRegistry, bReload);
    pl_load_terrain_ext(ptApiRegistry, bReload);
    pl_load_freelist_ext(ptApiRegistry, bReload);
    pl_load_image_ops_ext(ptApiRegistry, bReload);
    pl_load_voxel_ext(ptApiRegistry, bReload);
    pl_load_path_ext(ptApiRegistry, bReload);
    pl_load_audio_ext(ptApiRegistry, bReload);
    pl_load_stage_ext(ptApiRegistry, bReload);
    pl_load_gjk_ext(ptApiRegistry, bReload);
}

void
pl_unload_ext(plApiRegistryI* ptApiRegistry, bool bReload)
{
    pl_unload_shader_ext(ptApiRegistry, bReload);
    pl_unload_job_ext(ptApiRegistry, bReload);
    pl_unload_image_ext(ptApiRegistry, bReload);
    pl_unload_rect_pack_ext(ptApiRegistry, bReload);
    pl_unload_stats_ext(ptApiRegistry, bReload);
    pl_unload_string_intern_ext(ptApiRegistry, bReload);
    pl_unload_graphics_ext(ptApiRegistry, bReload);
    pl_unload_gpu_allocators_ext(ptApiRegistry, bReload);
    pl_unload_draw_ext(ptApiRegistry, bReload);
    pl_unload_animation_ext(ptApiRegistry, bReload);
    pl_unload_ecs_tools_ext(ptApiRegistry, bReload);
    pl_unload_tools_ext(ptApiRegistry, bReload);
    pl_unload_ui_ext(ptApiRegistry, bReload);
    pl_unload_ecs_ext(ptApiRegistry, bReload);
    pl_unload_mesh_ext(ptApiRegistry, bReload);
    pl_unload_camera_ext(ptApiRegistry, bReload);
    pl_unload_resource_ext(ptApiRegistry, bReload);
    pl_unload_model_loader_ext(ptApiRegistry, bReload);
    pl_unload_renderer_ext(ptApiRegistry, bReload);
    pl_unload_gizmo_ext(ptApiRegistry, bReload);
    pl_unload_console_ext(ptApiRegistry, bReload);
    pl_unload_screen_log_ext(ptApiRegistry, bReload);
    pl_unload_profile_ext(ptApiRegistry, bReload);
    pl_unload_log_ext(ptApiRegistry, bReload);
    pl_unload_physics_ext(ptApiRegistry, bReload);
    pl_unload_collision_ext(ptApiRegistry, bReload);
    pl_unload_bvh_ext(ptApiRegistry, bReload);
    pl_unload_starter_ext(ptApiRegistry, bReload);
    pl_unload_config_ext(ptApiRegistry, bReload);
    pl_unload_vfs_ext(ptApiRegistry, bReload);
    pl_unload_pak_ext(ptApiRegistry, bReload);
    pl_unload_datetime_ext(ptApiRegistry, bReload);
    pl_unload_shader_variant_ext(ptApiRegistry, bReload);
    pl_unload_compress_ext(ptApiRegistry, bReload);
    pl_unload_dds_ext(ptApiRegistry, bReload);
    pl_unload_dxt_ext(ptApiRegistry, bReload);
    pl_unload_material_ext(ptApiRegistry, bReload);
    pl_unload_script_ext(ptApiRegistry, bReload);
    pl_unload_terrain_ext(ptApiRegistry, bReload);
    pl_unload_freelist_ext(ptApiRegistry, bReload);
    pl_unload_image_ops_ext(ptApiRegistry, bReload);
    pl_unload_path_ext(ptApiRegistry, bReload);
    pl_unload_voxel_ext(ptApiRegistry, bReload);
    pl_unload_audio_ext(ptApiRegistry, bReload);
    pl_unload_stage_ext(ptApiRegistry, bReload);
    pl_unload_gjk_ext(ptApiRegistry, bReload);
}

//-----------------------------------------------------------------------------
// [SECTION] unity build #2
//-----------------------------------------------------------------------------

#define PL_STRING_IMPLEMENTATION
#include "pl_string.h"
#undef PL_STRING_IMPLEMENTATION

#define PL_MEMORY_IMPLEMENTATION
#define PL_MEMORY_ALLOC(x) gptMemory->tracked_realloc(NULL, (x), __FILE__, __LINE__)
#define PL_MEMORY_FREE(x)  gptMemory->tracked_realloc((x), 0, __FILE__, __LINE__)
#include "pl_memory.h"
#undef PL_MEMORY_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#define STBI_MALLOC(x) PL_ALLOC(x)
#define STBI_FREE(x) PL_FREE(x)
#define STBI_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBIW_MALLOC(x) PL_ALLOC(x)
#define STBIW_FREE(x) PL_FREE(x)
#define STBIW_REALLOC(x, y) PL_REALLOC(x, y)
#include "stb_image_write.h"
#undef STB_IMAGE_WRITE_IMPLEMENTATION

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#undef STB_RECT_PACK_IMPLEMENTATION

#ifdef PL_USE_STB_SPRINTF
    #define STB_SPRINTF_IMPLEMENTATION
    #include "stb_sprintf.h"
    #undef STB_SPRINTF_IMPLEMENTATION
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#define PL_JSON_IMPLEMENTATION
#include "pl_json.h"
#undef PL_JSON_IMPLEMENTATION

#define STB_DXT_IMPLEMENTATION
#include "stb_dxt.h"
#undef STB_DXT_IMPLEMENTATION

#undef CGLTF_IMPLEMENTATION