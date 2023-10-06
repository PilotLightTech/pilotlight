/*
   app.c (just experimental)
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

#include <stdio.h>
#include "pilotlight.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_os.h"
#include "pl_memory.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"

// extensions
#include "pl_image_ext.h"
#include "pl_stats_ext.h"
#include "pl_graphics_ext.h"
#include "pl_debug_ext.h"

// app specific
#include "camera.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct plAppData_t
{
    plGraphics tGraphics;

    // drawing
    plDrawList   tDrawlist;
    plDrawLayer* ptFgDrawLayer;
    plDrawLayer* ptBgDrawLayer;
    plFontAtlas  tFontAtlas;

    // ui options
    plDebugApiInfo tDebugInfo;
    bool           bShowUiDemo;
    bool           bShowUiDebug;
    bool           bShowUiStyle;

    // texture
    plTexture     tTexture;
    plTextureView tTextureView;

    // bind groups
    plBindGroup atBindGroups0[2];
    plBindGroup tBindGroup1_0;
    plBindGroup tBindGroup1_1;
    plBindGroup tBindGroup2;
    
    // shaders
    plShader tShader0;
    plShader tShader1;

    // global index/vertex/data buffers
    plBuffer tVertexBuffer;
    plBuffer tIndexBuffer;
    plBuffer tStorageBuffer;
    plBuffer tShaderSpecificBuffer;
    plBuffer atGlobalBuffers[2];

    // scene
    plCamera     tMainCamera;
    plDrawList3D t3DDrawList;

    plVec3*   sbtVertexPosBuffer;
    plVec4*   sbtVertexDataBuffer;
    uint32_t* sbuIndexBuffer;
} plAppData;

typedef struct _BindGroup_0
{
    plMat4 tCameraView;
    plMat4 tCameraProjection;   
    plMat4 tCameraViewProjection;   
} BindGroup_0;

typedef struct _BindGroup_2
{
    plVec4 tShaderSpecific;
} BindGroup_2;

typedef struct _DynamicData
{
    int    iDataOffset;
    int    iVertexOffset;
    int    iPadding[2];
    plMat4 tModel;
} DynamicData;

//-----------------------------------------------------------------------------
// [SECTION] global apis
//-----------------------------------------------------------------------------

const plApiRegistryApiI*       gptApiRegistry       = NULL;
const plDataRegistryApiI*      gptDataRegistry      = NULL;
const plStatsApiI*             gptStats             = NULL;
const plExtensionRegistryApiI* gptExtensionRegistry = NULL;
const plFileApiI*              gptFile              = NULL;
const plGraphicsI*             gptGfx               = NULL;
const plDeviceI*               gptDevice            = NULL;
const plDebugApiI*             gptDebug             = NULL;
const plImageApiI*             gptImage             = NULL;

//-----------------------------------------------------------------------------
// [SECTION] pl_app_load
//-----------------------------------------------------------------------------

PL_EXPORT void*
pl_app_load(plApiRegistryApiI* ptApiRegistry, plAppData* ptAppData)
{
    gptApiRegistry  = ptApiRegistry;
    gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
    pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
    pl_set_context(gptDataRegistry->get_data("ui"));

    if(ptAppData) // reload
    {
        pl_set_log_context(gptDataRegistry->get_data("log"));
        pl_set_profile_context(gptDataRegistry->get_data("profile"));

        // reload global apis
        gptStats  = ptApiRegistry->first(PL_API_STATS);
        gptFile   = ptApiRegistry->first(PL_API_FILE);
        gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
        gptDevice = ptApiRegistry->first(PL_API_DEVICE);
        gptDebug  = ptApiRegistry->first(PL_API_DEBUG);
        gptImage  = ptApiRegistry->first(PL_API_IMAGE);

        return ptAppData;
    }

    plProfileContext* ptProfileCtx = pl_create_profile_context();
    plLogContext*     ptLogCtx     = pl_create_log_context();
    
    // add some context to data registry
    ptAppData = PL_ALLOC(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));
    gptDataRegistry->set_data("profile", ptProfileCtx);
    gptDataRegistry->set_data("log", ptLogCtx);

    // create log context
    pl_add_log_channel("Default", PL_CHANNEL_TYPE_CONSOLE);
    pl_log_info("Setup logging");

    // load extensions
    const plExtensionRegistryApiI* ptExtensionRegistry = ptApiRegistry->first(PL_API_EXTENSION_REGISTRY);
    ptExtensionRegistry->load("pl_image_ext",    "pl_load_image_ext", "pl_unload_image_ext", false);
    ptExtensionRegistry->load("pl_stats_ext",    "pl_load_stats_ext", "pl_unload_stats_ext", false);
    ptExtensionRegistry->load("pl_graphics_ext", "pl_load_ext",       "pl_unload_ext",       false);
    ptExtensionRegistry->load("pl_debug_ext",    "pl_load_debug_ext", "pl_unload_debug_ext", true);

    // load apis
    gptStats  = ptApiRegistry->first(PL_API_STATS);
    gptFile   = ptApiRegistry->first(PL_API_FILE);
    gptGfx    = ptApiRegistry->first(PL_API_GRAPHICS);
    gptDevice = ptApiRegistry->first(PL_API_DEVICE);
    gptDebug  = ptApiRegistry->first(PL_API_DEBUG);
    gptImage  = ptApiRegistry->first(PL_API_IMAGE);

    // create command queue
    gptGfx->initialize(&ptAppData->tGraphics);
    gptDataRegistry->set_data("device", &ptAppData->tGraphics.tDevice);

    // new demo

    // create camera
    plIO* ptIO = pl_get_io();
    ptAppData->tMainCamera = pl_camera_create((plVec3){-6.211f, 3.647f, 0.827f}, PL_PI_3, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1], 0.01f, 400.0f);
    pl_camera_set_pitch_yaw(&ptAppData->tMainCamera, -0.244f, 1.488f);
    pl_camera_update(&ptAppData->tMainCamera);

    // shader specific buffer
    const BindGroup_2 tShaderSpecificBufferDesc = {
        .tShaderSpecific = {0}
    };
    const plBufferDescription tShaderBufferDesc = {
        .pcDebugName          = "shader buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_2),
        .uInitialDataByteSize = sizeof(BindGroup_2),
        .puInitialData        = (uint8_t*)&tShaderSpecificBufferDesc
    };
    ptAppData->tShaderSpecificBuffer = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &tShaderBufferDesc);

    // mesh 0
    {
        const uint32_t uStartIndex = pl_sb_size(ptAppData->sbuIndexBuffer);

        // indices
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, -0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 0.5f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, -0.5f, 0.0f}));

        // vertices (data - uv, color)
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 0.0f, 0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 0.0f, 1.0f, 1.0f}));
    }
    
    // mesh 1
    {
        const uint32_t uStartIndex = pl_sb_size(ptAppData->sbtVertexPosBuffer);

        // indices
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 1);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 0);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 2);
        pl_sb_push(ptAppData->sbuIndexBuffer, uStartIndex + 3);

        // vertices (position)
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 1.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){-0.5f, 2.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 2.0f, 0.0f}));
        pl_sb_push(ptAppData->sbtVertexPosBuffer, ((plVec3){0.5f, 1.0f, 0.0f}));

        // vertices (data - color)
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 1.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){1.0f, 0.0f, 0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 1.0f, 0.0f, 1.0f}));
        pl_sb_push(ptAppData->sbtVertexDataBuffer, ((plVec4){0.0f, 0.0f, 1.0f, 1.0f}));
    }
    
    const plBufferDescription tIndexBufferDesc = {
        .pcDebugName          = "index buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(ptAppData->sbuIndexBuffer),
        .uInitialDataByteSize = sizeof(uint32_t) * pl_sb_size(ptAppData->sbuIndexBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbuIndexBuffer
    };
    ptAppData->tIndexBuffer = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &tIndexBufferDesc);
    pl_sb_free(ptAppData->sbuIndexBuffer);

    const plBufferDescription tVertexBufferDesc = {
        .pcDebugName          = "vertex buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(ptAppData->sbtVertexPosBuffer),
        .uInitialDataByteSize = sizeof(plVec3) * pl_sb_size(ptAppData->sbtVertexPosBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbtVertexPosBuffer
    };
    ptAppData->tVertexBuffer = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &tVertexBufferDesc);
    pl_sb_free(ptAppData->sbtVertexPosBuffer);

    const plBufferDescription tStorageBufferDesc = {
        .pcDebugName          = "storage buffer",
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(ptAppData->sbtVertexDataBuffer),
        .uInitialDataByteSize = sizeof(plVec4) * pl_sb_size(ptAppData->sbtVertexDataBuffer),
        .puInitialData        = (uint8_t*)ptAppData->sbtVertexDataBuffer
    };
    ptAppData->tStorageBuffer = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &tStorageBufferDesc);
    pl_sb_free(ptAppData->sbtVertexDataBuffer);

    static float image[] = {
        1.0f,   0,   0, 1.0f,
        0, 1.0f,   0, 1.0f,
        0,   0, 1.0f, 1.0f,
        1.0f,   0, 1.0f, 1.0f
    };
    plTextureDesc tTextureDesc = {
        .tDimensions = {2, 2, 1},
        .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers = 1,
        .uMips = 0
    };
    ptAppData->tTexture = gptDevice->create_texture(&ptAppData->tGraphics.tDevice, tTextureDesc, sizeof(float) * 4 * 4, image, "texture");

    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    plSampler tSampler = {
        .tFilter = PL_FILTER_NEAREST,
        .fMinMip = 0.0f,
        .fMaxMip = 64.0f,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    ptAppData->tTextureView = gptDevice->create_texture_view(&ptAppData->tGraphics.tDevice, &tTextureViewDesc, &tSampler, &ptAppData->tTexture, "texture view");

    const plBufferDescription atGlobalBuffersDesc = {
        .pcDebugName          = "global buffer",
        .tMemory              = PL_MEMORY_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_0),
        .uInitialDataByteSize = 0,
        .puInitialData        = NULL
    };
    ptAppData->atGlobalBuffers[0] = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &atGlobalBuffersDesc);
    ptAppData->atGlobalBuffers[1] = gptDevice->create_buffer(&ptAppData->tGraphics.tDevice, &atGlobalBuffersDesc);

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 2,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1
            },
        }
    };
    ptAppData->atBindGroups0[0] = gptDevice->create_bind_group(&ptAppData->tGraphics.tDevice, &tBindGroupLayout0);
    ptAppData->atBindGroups0[1] = gptDevice->create_bind_group(&ptAppData->tGraphics.tDevice, &tBindGroupLayout0);
    size_t szBufferRangeSize[2] = {sizeof(BindGroup_0), tStorageBufferDesc.uByteSize};

    plBuffer atBindGroup0_buffers0[2] = {ptAppData->atGlobalBuffers[0], ptAppData->tStorageBuffer};
    plBuffer atBindGroup0_buffers1[2] = {ptAppData->atGlobalBuffers[1], ptAppData->tStorageBuffer};
    gptDevice->update_bind_group(&ptAppData->tGraphics.tDevice, &ptAppData->atBindGroups0[0], 2, atBindGroup0_buffers0, szBufferRangeSize, 0, NULL);
    gptDevice->update_bind_group(&ptAppData->tGraphics.tDevice, &ptAppData->atBindGroups0[1], 2, atBindGroup0_buffers1, szBufferRangeSize, 0, NULL);

    plBindGroupLayout tBindGroupLayout1_0 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    ptAppData->tBindGroup1_0 = gptDevice->create_bind_group(&ptAppData->tGraphics.tDevice, &tBindGroupLayout1_0);
    gptDevice->update_bind_group(&ptAppData->tGraphics.tDevice, &ptAppData->tBindGroup1_0, 0, NULL, NULL, 1, &ptAppData->tTextureView);

    plBindGroupLayout tBindGroupLayout1_1 = {
        .uTextureCount  = 1,
        .aTextures = {
            {.uSlot = 0}
        }
    };
    ptAppData->tBindGroup1_1 = gptDevice->create_bind_group(&ptAppData->tGraphics.tDevice, &tBindGroupLayout1_1);
    gptDevice->update_bind_group(&ptAppData->tGraphics.tDevice, &ptAppData->tBindGroup1_1, 0, NULL, NULL, 1, &ptAppData->tTextureView);

    plBindGroupLayout tBindGroupLayout2_0 = {
        .uBufferCount  = 1,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0
            },
        }
    };
    ptAppData->tBindGroup2 = gptDevice->create_bind_group(&ptAppData->tGraphics.tDevice, &tBindGroupLayout2_0);
    size_t szGroup2BufferRange = sizeof(BindGroup_2);
    gptDevice->update_bind_group(&ptAppData->tGraphics.tDevice, &ptAppData->tBindGroup2, 1, &ptAppData->tShaderSpecificBuffer, &szGroup2BufferRange, 0, NULL);

    plShaderDescription tShaderDescription0 = {

#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
#else // VULKAN
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
#endif

        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0 | PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_0,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    }
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    ptAppData->tShader0 = gptGfx->create_shader(&ptAppData->tGraphics, &tShaderDescription0);

    plShaderDescription tShaderDescription1 = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
#else // linux
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
#endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_COLOR_0,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_DEPTH_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulShaderTextureFlags = PL_SHADER_TEXTURE_FLAG_BINDING_NONE,
            .ulStencilMode        = PL_STENCIL_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 2,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1
                    },
                },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0
                    }
                 },
            },
            {
                .uBufferCount  = 1,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0
                    },
                },
            }
        }
    };
    ptAppData->tShader1 = gptGfx->create_shader(&ptAppData->tGraphics, &tShaderDescription1);

    // create draw list & layers
    pl_register_drawlist(&ptAppData->tDrawlist);
    ptAppData->ptBgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Background Layer");
    ptAppData->ptFgDrawLayer = pl_request_layer(&ptAppData->tDrawlist, "Foreground Layer");
    
    // create font atlas
    pl_add_default_font(&ptAppData->tFontAtlas);
    pl_build_font_atlas(&ptAppData->tFontAtlas);
    gptGfx->create_font_atlas(&ptAppData->tFontAtlas);
    pl_set_default_font(&ptAppData->tFontAtlas.sbtFonts[0]);

    // 3D tDrawlist
    gptGfx->register_3d_drawlist(&ptAppData->tGraphics, &ptAppData->t3DDrawList);
    
    return ptAppData;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_shutdown
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_shutdown(plAppData* ptAppData)
{
    gptGfx->destroy_font_atlas(&ptAppData->tFontAtlas);
    pl_cleanup_font_atlas(&ptAppData->tFontAtlas);

    gptGfx->cleanup(&ptAppData->tGraphics);
    pl_cleanup_profile_context();
    pl_cleanup_log_context();
    PL_FREE(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plAppData* ptAppData)
{
    plIO* ptIO = pl_get_io();
    pl_camera_set_aspect(&ptAppData->tMainCamera, ptIO->afMainViewportSize[0] / ptIO->afMainViewportSize[1]);
    gptGfx->resize(&ptAppData->tGraphics);

}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    // begin profiling frame
    pl_begin_profile_frame();
    pl_begin_profile_sample(__FUNCTION__);

    if(!gptGfx->begin_frame(&ptAppData->tGraphics))
    {
        pl_end_profile_sample();
        pl_end_profile_frame();
        return;
    }

    plIO* ptIO = pl_get_io();

    gptStats->new_frame();

    static double* pdFrameTimeCounter = NULL;
    if(!pdFrameTimeCounter)
        pdFrameTimeCounter = gptStats->get_counter("framerate");
    *pdFrameTimeCounter = (double)pl_get_io()->fFrameRate;

    // camera
    static const float fCameraTravelSpeed = 8.0f;

    // camera space
    if(pl_is_key_pressed(PL_KEY_W, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_S, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(pl_is_key_pressed(PL_KEY_A, true)) pl_camera_translate(&ptAppData->tMainCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(pl_is_key_pressed(PL_KEY_D, true)) pl_camera_translate(&ptAppData->tMainCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(pl_is_key_pressed(PL_KEY_F, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);
    if(pl_is_key_pressed(PL_KEY_R, true)) pl_camera_translate(&ptAppData->tMainCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f);

    bool bOwnMouse = ptIO->bWantCaptureMouse;
    if(!bOwnMouse && pl_is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = pl_get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        pl_camera_rotate(&ptAppData->tMainCamera,  -tMouseDelta.y * 0.1f * ptIO->fDeltaTime,  -tMouseDelta.x * 0.1f * ptIO->fDeltaTime);
        pl_reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    pl_camera_update(&ptAppData->tMainCamera);

    const BindGroup_0 tBindGroupBuffer = {
        .tCameraProjection = ptAppData->tMainCamera.tProjMat,
        .tCameraView = ptAppData->tMainCamera.tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptAppData->tMainCamera.tProjMat, &ptAppData->tMainCamera.tViewMat)
    };
    memcpy(ptAppData->atGlobalBuffers[ptAppData->tGraphics.uCurrentFrameIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    gptGfx->begin_recording(&ptAppData->tGraphics);

    pl_new_frame();

    pl_set_next_window_pos((plVec2){0, 0}, PL_UI_COND_ONCE);

    if(pl_begin_window("Pilot Light", NULL, false))
    {

        const float pfRatios[] = {1.0f};
        pl_layout_row(PL_UI_LAYOUT_ROW_TYPE_DYNAMIC, 0.0f, 1, pfRatios);
        
        if(pl_collapsing_header("Tools"))
        {
            pl_checkbox("Device Memory Analyzer", &ptAppData->tDebugInfo.bShowDeviceMemoryAnalyzer);
            pl_checkbox("Memory Allocations", &ptAppData->tDebugInfo.bShowMemoryAllocations);
            pl_checkbox("Profiling", &ptAppData->tDebugInfo.bShowProfiling);
            pl_checkbox("Statistics", &ptAppData->tDebugInfo.bShowStats);
            pl_checkbox("Logging", &ptAppData->tDebugInfo.bShowLogging);
            pl_end_collapsing_header();
        }

        if(pl_collapsing_header("User Interface"))
        {
            pl_checkbox("UI Debug", &ptAppData->bShowUiDebug);
            pl_checkbox("UI Demo", &ptAppData->bShowUiDemo);
            pl_checkbox("UI Style", &ptAppData->bShowUiStyle);
            pl_end_collapsing_header();
        }
        pl_end_window();
    }

    gptDebug->show_windows(&ptAppData->tDebugInfo);

    if(ptAppData->bShowUiDemo)
    {
        pl_begin_profile_sample("ui demo");
        pl_demo(&ptAppData->bShowUiDemo);
        pl_end_profile_sample();
    }
        
    if(ptAppData->bShowUiStyle)
        pl_style(&ptAppData->bShowUiStyle);

    if(ptAppData->bShowUiDebug)
        pl_debug(&ptAppData->bShowUiDebug);

    pl_add_line(ptAppData->ptFgDrawLayer, (plVec2){0}, (plVec2){300.0f, 500.0f}, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 1.0f);

    const plMat4 tTransform0 = pl_identity_mat4();
    gptGfx->add_3d_transform(&ptAppData->t3DDrawList, &tTransform0, 10.0f, 0.02f);
    gptGfx->add_3d_triangle_filled(&ptAppData->t3DDrawList, (plVec3){0}, (plVec3){0.0f, 0.0f, 1.0f}, (plVec3){0.0f, 1.0f, 0.0f}, (plVec4){1.0f, 0.0f, 0.0f, 0.25f});

    const plMat4 tMVP = pl_mul_mat4(&ptAppData->tMainCamera.tProjMat, &ptAppData->tMainCamera.tViewMat);

    plDynamicBinding tDynamicBinding0 = gptDevice->allocate_dynamic_data(&ptAppData->tGraphics.tDevice, sizeof(DynamicData));
    DynamicData* ptDynamicData0 = (DynamicData*)tDynamicBinding0.pcData;
    ptDynamicData0->iDataOffset = 0;
    ptDynamicData0->iVertexOffset = 0;
    ptDynamicData0->tModel = pl_mat4_rotate_xyz(0.05f * (float)ptIO->dTime, 0.0f, 1.0f, 0.0f);

    plDynamicBinding tDynamicBinding1 = gptDevice->allocate_dynamic_data(&ptAppData->tGraphics.tDevice, sizeof(DynamicData));
    DynamicData* ptDynamicData1 = (DynamicData*)tDynamicBinding1.pcData;
    ptDynamicData1->iDataOffset = 32;
    ptDynamicData1->iVertexOffset = 4;
    ptDynamicData1->tModel = pl_identity_mat4();

    plDraw tDraw[2] = {
        {
            .uDynamicBuffer = tDynamicBinding0.uBufferHandle,
            .uVertexBuffer = ptAppData->tVertexBuffer.uHandle,
            .uIndexBuffer = ptAppData->tIndexBuffer.uHandle,
            .uIndexCount = 6,
            .uVertexCount = 4,
            .aptBindGroups = {
                &ptAppData->atBindGroups0[ptAppData->tGraphics.uCurrentFrameIndex],
                &ptAppData->tBindGroup1_0,
                &ptAppData->tBindGroup2
            },
            .uShaderVariant = ptAppData->tShader0.uHandle,
            .auDynamicBufferOffset = {
                tDynamicBinding0.uByteOffset
            }
        },
        {
            .uDynamicBuffer = tDynamicBinding1.uBufferHandle,
            .uVertexBuffer = ptAppData->tVertexBuffer.uHandle,
            .uIndexBuffer = ptAppData->tIndexBuffer.uHandle,
            .uIndexCount = 6,
            .uVertexCount = 4,
            .uIndexOffset = 6,
            .aptBindGroups = {
                &ptAppData->atBindGroups0[ptAppData->tGraphics.uCurrentFrameIndex],
                &ptAppData->tBindGroup1_1,
                &ptAppData->tBindGroup2
            },
            .uShaderVariant = ptAppData->tShader1.uHandle,
            .auDynamicBufferOffset = {
                tDynamicBinding1.uByteOffset
            }
        }
    };

    plDrawArea tArea = {
        .uDrawOffset = 0,
        .uDrawCount = 2
    };
    gptGfx->draw_areas(&ptAppData->tGraphics, 1, &tArea, tDraw);

    // submit 3D draw list
    gptGfx->submit_3d_drawlist(&ptAppData->t3DDrawList, pl_get_io()->afMainViewportSize[0], pl_get_io()->afMainViewportSize[1], &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE);

    // submit draw layers
    pl_begin_profile_sample("Submit draw layers");
    pl_submit_layer(ptAppData->ptBgDrawLayer);
    pl_submit_layer(ptAppData->ptFgDrawLayer);
    pl_end_profile_sample();

    pl_render();

    // submit draw lists
    pl_begin_profile_sample("Submit draw lists");
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, &ptAppData->tDrawlist);
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_draw_list(NULL));
    gptGfx->draw_lists(&ptAppData->tGraphics, 1, pl_get_debug_draw_list(NULL));
    pl_end_profile_sample();

    gptGfx->end_recording(&ptAppData->tGraphics);
    gptGfx->end_frame(&ptAppData->tGraphics);
    pl_end_profile_sample();
    pl_end_profile_frame();
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#include "camera.c"