/*
   pl_ref_renderer_ext.c
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] global data & apis
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <float.h> // FLT_MAX
#include "pilotlight.h"
#include "pl_ref_renderer_ext.h"
#include "pl_os.h"
#include "pl_profile.h"
#include "pl_log.h"
#include "pl_ds.h"
#include "pl_string.h"
#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"
#include "pl_ui.h"
#include "pl_stl.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_ecs_ext.h"
#include "pl_resource_ext.h"
#include "pl_image_ext.h"
#include "pl_stats_ext.h"

// misc
#include "cgltf.h"

//-----------------------------------------------------------------------------
// [SECTION] internal structs
//-----------------------------------------------------------------------------

typedef struct _plOBB
{
    plVec3 tCenter;
    plVec3 tExtents;
    plVec3 atAxes[3]; // Orthonormal basis
} plOBB;

typedef struct _plSkinData
{
    plEntity            tEntity;
    plTextureHandle     tDynamicTexture[2];
    plTextureViewHandle tDynamicTextureView[2];
    plBindGroupHandle   tTempBindGroup;
} plSkinData;

typedef struct _plDrawable
{
    plEntity tEntity;
    plBindGroupHandle tMaterialBindGroup;
    uint32_t uDataOffset;
    uint32_t uVertexOffset;
    uint32_t uVertexCount;
    uint32_t uIndexOffset;
    uint32_t uIndexCount;
    uint32_t uMaterialIndex;
    uint32_t uShader;
    uint32_t uSkinIndex;
} plDrawable;

typedef struct _plMaterial
{
    plVec4 tColor;
} plMaterial;

typedef struct _BindGroup_0
{
    plVec4 tCameraPos;
    plMat4 tCameraView;
    plMat4 tCameraProjection;   
    plMat4 tCameraViewProjection;
} BindGroup_0;

typedef struct _DynamicData
{
    int    iDataOffset;
    int    iVertexOffset;
    int    iMaterialOffset;
    int    iPadding[1];
    plMat4 tModel;
} DynamicData;

typedef struct _plRefView
{
    plRenderPassHandle       tRenderPass;
    plVec2                   tTargetSize;
    plTextureHandle          tTexture;
    plTextureViewHandle      tTextureView;
    plTextureHandle          tDepthTexture;
    plTextureViewHandle      tDepthTextureView;
    plTextureId              tTextureID;

    // GPU buffers
    plBufferHandle atGlobalBuffers[2];

    // misc
    plDrawable* sbtVisibleDrawables;

    // drawing api
    plDrawList3D t3DDrawList;
} plRefView;

typedef struct _plRefScene
{

    plRenderPassLayoutHandle tRenderPassLayout;

    // shader templates
    plShaderHandle tShader;

    // skybox
    plDrawable          tSkyboxDrawable;
    plTextureHandle     tSkyboxTexture;
    plTextureViewHandle tSkyboxTextureView;
    plBindGroupHandle   tSkyboxBindGroup;

    // CPU buffers
    plVec3*     sbtVertexPosBuffer;
    plVec4*     sbtVertexDataBuffer;
    uint32_t*   sbuIndexBuffer;
    plMaterial* sbtMaterialBuffer;

    // GPU buffers
    plBufferHandle tVertexBuffer;
    plBufferHandle tIndexBuffer;
    plBufferHandle tStorageBuffer;
    plBufferHandle tMaterialDataBuffer;

    // misc
    plDrawable* sbtAllDrawables;
    plDrawable* sbtOpaqueDrawables;
    plDrawable* sbtTransparentDrawables;

    plHashMap tMaterialHashMap;
    plEntity* sbtMaterialEntities;
    plBindGroupHandle* sbtMaterialBindGroups;

    plRefView* sbtViews;

    plSkinData* sbtSkinData;
} plRefScene;

typedef struct _plRefRendererData
{
    uint32_t uLogChannel;

    plGraphics tGraphics;

    // misc textures
    plTextureHandle     tDummyTexture;
    plTextureViewHandle tDummyTextureView;

    // shaders
    plShaderHandle tSkyboxShader;

    // compute shaders
    plComputeShaderHandle tPanoramaShader;

    // offscreen
    plRefScene* sbtScenes;

    // draw stream
    plDrawStream tDrawStream;

    // ecs
    plComponentLibrary tComponentLibrary;

    // gltf data
    plBindGroupHandle tNullSkinBindgroup;
    plHashMap tNodeHashMap;
    plHashMap tJointHashMap;
    plHashMap tSkinHashMap;

    // temp
    plBufferHandle tStagingBufferHandle;
} plRefRendererData;

//-----------------------------------------------------------------------------
// [SECTION] global data & apis
//-----------------------------------------------------------------------------

// context data
static plRefRendererData* gptData = NULL;

// apis
static const plDataRegistryApiI* gptDataRegistry = NULL;
static const plResourceI*        gptResource = NULL;
static const plEcsI*             gptECS      = NULL;
static const plFileApiI*         gptFile     = NULL;
static const plDeviceI*          gptDevice   = NULL;
static const plGraphicsI*        gptGfx      = NULL;
static const plCameraI*          gptCamera   = NULL;
static const plDrawStreamI*      gptStream   = NULL;
static const plImageI*           gptImage    = NULL;
static const plStatsApiI*        gptStats    = NULL;

//-----------------------------------------------------------------------------
// [SECTION] forward declarations
//-----------------------------------------------------------------------------

// main
static void pl_refr_initialize(void);
static void pl_refr_resize(void);
static void pl_refr_cleanup(void);

// scenes
static uint32_t pl_refr_create_scene(void);

// views
static uint32_t pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions);
static void pl_refr_resize_view(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions);

// per frame
static void pl_refr_run_ecs(void);
static void pl_refr_update_scene(uint32_t uSceneHandle);
static void pl_refr_render_scene(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions);

// loading
static void pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcModelPath, int iResolution);
static void pl_refr_load_stl(uint32_t uSceneHandle, const char* pcModelPath, plVec4 tColor, const plMat4* ptTransform);
static void pl_refr_load_gltf(uint32_t uSceneHandle, const char* pcPath, const plMat4* ptTransform);
static void pl_refr_finalize_scene(uint32_t uSceneHandle);

// misc
static plComponentLibrary* pl_refr_get_component_library(void);
static plGraphics*         pl_refr_get_graphics         (void);

// temporary
static plTextureId pl_refr_get_view_texture_id(uint32_t uSceneHandle, uint32_t uViewHandle);

// internal general helpers
static void pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex);

// internal gltf helpers
static void pl__load_gltf_texture(plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptMaterial, plMaterialComponent* ptMaterialOut);
static void pl__refr_load_material(const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial);
static void pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive);
static void pl__refr_load_gltf_object(plRefScene* ptScene, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode);
static void pl__refr_load_gltf_animation(plRefScene* ptScene, const cgltf_animation* ptAnimation);
static bool pl__sat_visibility_test(plCameraComponent* ptCamera, const plAABB* aabb);

//-----------------------------------------------------------------------------
// [SECTION] public api implementation
//-----------------------------------------------------------------------------

const plRefRendererI*
pl_load_ref_renderer_api(void)
{
    static const plRefRendererI tApi = {
        .initialize                = pl_refr_initialize,
        .cleanup                   = pl_refr_cleanup,
        .resize                    = pl_refr_resize,
        .create_scene              = pl_refr_create_scene,
        .create_view               = pl_refr_create_view,
        .run_ecs                   = pl_refr_run_ecs,
        .get_component_library     = pl_refr_get_component_library,
        .get_graphics              = pl_refr_get_graphics,
        .load_skybox_from_panorama = pl_refr_load_skybox_from_panorama,
        .load_stl                  = pl_refr_load_stl,
        .load_gltf                 = pl_refr_load_gltf,
        .finalize_scene            = pl_refr_finalize_scene,
        .update_scene              = pl_refr_update_scene,
        .render_scene              = pl_refr_render_scene,
        .get_view_texture_id       = pl_refr_get_view_texture_id,
        .resize_view               = pl_refr_resize_view,
    };
    return &tApi;
}

//-----------------------------------------------------------------------------
// [SECTION] implementation
//-----------------------------------------------------------------------------

static void
pl_refr_initialize(void)
{

    // shader default values
    gptData->tSkyboxShader    = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // compute shader default values
    gptData->tPanoramaShader = (plComputeShaderHandle){UINT32_MAX, UINT32_MAX};

    // misc textures
    gptData->tDummyTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    gptData->tDummyTextureView = (plTextureViewHandle){UINT32_MAX, UINT32_MAX};

    gptData->tNullSkinBindgroup = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // initialize ecs
    gptECS->init_component_library(&gptData->tComponentLibrary);

    // initialize graphics
    ptGraphics->bValidationActive = true;
    gptGfx->initialize(ptGraphics);
    gptDataRegistry->set_data("device", &ptGraphics->tDevice); // used by debug extension

    // create main render pass
    plIO* ptIO = pl_get_io();

    // create staging buffer
    const plBufferDescription tStagingBufferDesc = {
        .tMemory   = PL_MEMORY_GPU_CPU,
        .tUsage    = PL_BUFFER_USAGE_UNSPECIFIED,
        .uByteSize = 268435456
    };
    gptData->tStagingBufferHandle = gptDevice->create_buffer(&ptGraphics->tDevice, &tStagingBufferDesc, "staging buffer");
    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, gptData->tStagingBufferHandle);

    // create dummy texture
    plTextureDesc tTextureDesc = {
        .tDimensions = {2, 2, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED
    };
    gptData->tDummyTexture = gptDevice->create_texture(&ptGraphics->tDevice, tTextureDesc, "dummy texture");

    // copy data to dummy texture
    static float image[] = {
        1.0f,   0,   0, 1.0f,
        0, 1.0f,   0, 1.0f,
        0,   0, 1.0f, 1.0f,
        1.0f,   0, 1.0f, 1.0f
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, image, sizeof(float) * 4 * 4);
    
    plBufferImageCopy tBufferImageCopy = {
        .tImageExtent = {2, 2, 1},
        .uLayerCount  = 1
    };
    gptDevice->copy_buffer_to_texture(&ptGraphics->tDevice, gptData->tStagingBufferHandle, gptData->tDummyTexture, 1, &tBufferImageCopy);

    // create dummy texture view
    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };
    plSampler tSampler = {
        .tFilter         = PL_FILTER_NEAREST,
        .fMinMip         = 0.0f,
        .fMaxMip         = 1.0f,
        .tVerticalWrap   = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    gptData->tDummyTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tTextureViewDesc, &tSampler, gptData->tDummyTexture, "dummy texture view");

    // create null skin bind group (to be bound when skinning isn't enabled)
    plBindGroupLayout tBindGroupLayout1 = {
        .uTextureCount  = 1,
        .aTextures = {{.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}}
    };
    gptData->tNullSkinBindgroup = gptDevice->create_bind_group(&ptGraphics->tDevice, &tBindGroupLayout1);
    gptDevice->update_bind_group(&ptGraphics->tDevice, &gptData->tNullSkinBindgroup, 0, NULL, NULL, 1, &gptData->tDummyTextureView);
}

static uint32_t
pl_refr_create_scene(void)
{
    const uint32_t uSceneHandle = pl_sb_size(gptData->sbtScenes);
    plRefScene tScene = {0};
    pl_sb_push(gptData->sbtScenes, tScene);
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // shaders default valies
    ptScene->tShader = (plShaderHandle){UINT32_MAX, UINT32_MAX};

    // buffer default values
    ptScene->tVertexBuffer         = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tIndexBuffer          = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tStorageBuffer        = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tMaterialDataBuffer   = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // skybox resources default values
    ptScene->tSkyboxTexture     = (plTextureHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tSkyboxTextureView = (plTextureViewHandle){UINT32_MAX, UINT32_MAX};
    ptScene->tSkyboxBindGroup   = (plBindGroupHandle){UINT32_MAX, UINT32_MAX};

    // create offscreen render pass layout
    const plRenderPassLayoutDescription tRenderPassLayoutDesc = {
        .tDepthTarget = { .tFormat = PL_FORMAT_D32_FLOAT_S8_UINT},
        .atRenderTargets = {
            { .tFormat = PL_FORMAT_R32G32B32A32_FLOAT }
        },
        .atSubpasses = {
            {
                .uRenderTargetCount = 1,
                .auRenderTargets = {0},
                .uSubpassInputCount = 0,
                .bDepthTarget = true
            }
        }
    };
    ptScene->tRenderPassLayout = gptDevice->create_render_pass_layout(&gptData->tGraphics.tDevice, &tRenderPassLayoutDesc);

    // create template shader
    plShaderDescription tShaderDescription = {

#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/primitive.metal",
        .pcPixelShader = "../shaders/metal/primitive.metal",
#else
        .pcVertexShader = "primitive.vert.spv",
        .pcPixelShader = "primitive.frag.spv",
#endif
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_CULL_BACK,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .uConstantCount = 5,
        .tRenderPassLayout = ptScene->tRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 3,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 2,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                }
            },
            {
                .uTextureCount  = 2,
                .aTextures = {
                    {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                    {.uSlot =  1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                }
            },
            {
                .uTextureCount  = 1,
                .aTextures = {
                    {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
                }
            }
        }
    };
    for(uint32_t i = 0; i < tShaderDescription.uConstantCount; i++)
    {
        tShaderDescription.atConstants[i].uID = i;
        tShaderDescription.atConstants[i].uOffset = i * sizeof(int);
        tShaderDescription.atConstants[i].tType = PL_DATA_TYPE_INT;
    }

    int aiConstantData[5] = {0};
    aiConstantData[2] = 0;
    aiConstantData[3] = 0;
    aiConstantData[4] = 0;
    
    aiConstantData[0] = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    int iFlagCopy = (int)PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    while(iFlagCopy)
    {
        aiConstantData[1] += iFlagCopy & 1;
        iFlagCopy >>= 1;
    }
    tShaderDescription.pTempConstantData = aiConstantData;
    ptScene->tShader = gptDevice->create_shader(&gptData->tGraphics.tDevice, &tShaderDescription);

    return uSceneHandle;
}

static uint32_t
pl_refr_create_view(uint32_t uSceneHandle, plVec2 tDimensions)
{

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    const uint32_t uViewHandle = pl_sb_size(ptScene->sbtViews);
    plRefView tView = {0};
    pl_sb_push(ptScene->sbtViews, tView);
    plRefView* ptView = &ptScene->sbtViews[uViewHandle];

    ptView->tTargetSize = tDimensions;

    // buffer default values
    ptView->atGlobalBuffers[0]    = (plBufferHandle){UINT32_MAX, UINT32_MAX};
    ptView->atGlobalBuffers[1]    = (plBufferHandle){UINT32_MAX, UINT32_MAX};

    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;

    // create skybox shader
    plShaderDescription tSkyboxShaderDesc = {
#ifdef PL_METAL_BACKEND
        .pcVertexShader = "../shaders/metal/skybox.metal",
        .pcPixelShader = "../shaders/metal/skybox.metal",
        .pcVertexShaderEntryFunc = "vertex_main",
        .pcPixelShaderEntryFunc = "fragment_main",
#else
        .pcVertexShader = "skybox.vert.spv",
        .pcPixelShader = "skybox.frag.spv",
        .pcVertexShaderEntryFunc = "main",
        .pcPixelShaderEntryFunc = "main",
#endif

        .tGraphicsState = {
            .ulDepthWriteEnabled  = 0,
            .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
            .ulBlendMode          = PL_BLEND_MODE_ALPHA,
            .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP
        },
        .tRenderPassLayout = ptScene->tRenderPassLayout,
        .uBindGroupLayoutCount = 3,
        .atBindGroupLayouts = {
            {
                .uBufferCount  = 3,
                .aBuffers = {
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 1,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                    {
                        .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                        .uSlot = 2,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    },
                }
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                 },
            },
            {
                .uTextureCount = 1,
                .aTextures = {
                    {
                        .uSlot = 0,
                        .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
                    }
                 },
            }
        }
    };
    gptData->tSkyboxShader = gptDevice->create_shader(&ptGraphics->tDevice, &tSkyboxShaderDesc);

    // create offscreen color & depth textures
    const plTextureDesc tTextureDesc = {
        .tDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };
    ptView->tTexture = gptDevice->create_texture(&ptGraphics->tDevice, tTextureDesc, "offscreen texture");

    const plTextureDesc tDepthTextureDesc = {
        .tDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };
    ptView->tDepthTexture = gptDevice->create_texture(&ptGraphics->tDevice, tDepthTextureDesc, "offscreen depth texture");

    // create offscreen color & depth texture views
    const plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };

    const plTextureViewDesc tDepthTextureViewDesc = {
        .tFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };

    const plSampler tSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = 1.0F,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    ptView->tTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tTextureViewDesc, &tSampler, ptView->tTexture, "offscreen texture view");
    ptView->tTextureID = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tTextureView);
    ptView->tDepthTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tDepthTextureViewDesc, &tSampler, ptView->tDepthTexture, "offscreen depth texture view");

    // create offscreen render pass
    const plRenderPassAttachments atAttachmentSets[] = {
        {
            .atViewAttachments = {
                ptView->tTextureView,
                ptView->tDepthTextureView,
            }
        }
    };

    const plRenderPassDescription tRenderPassDesc = {
        .tLayout = ptScene->tRenderPassLayout,
        .tDepthTarget = {
                .tLoadOp         = PL_LOAD_OP_CLEAR,
                .tStoreOp        = PL_STORE_OP_DONT_CARE,
                .tStencilLoadOp  = PL_LOAD_OP_CLEAR,
                .tStencilStoreOp = PL_STORE_OP_DONT_CARE,
                .tNextUsage      = PL_TEXTURE_LAYOUT_DEPTH_STENCIL,
                .fClearZ         = 1.0f
        },
        .atRenderTargets = {
            {
                .tLoadOp     = PL_LOAD_OP_CLEAR,
                .tStoreOp    = PL_STORE_OP_STORE,
                .tNextUsage  = PL_TEXTURE_LAYOUT_SHADER_READ,
                .tClearColor = {0.0f, 0.0f, 0.0f, 1.0f}
            }
        },
        .tDimensions      = {.x = ptView->tTargetSize.x, .y = ptView->tTargetSize.y},
        .uAttachmentCount = 2,
        .uAttachmentSets  = 1,
    };
    ptView->tRenderPass = gptDevice->create_render_pass(&ptGraphics->tDevice, &tRenderPassDesc, atAttachmentSets);


    // register debug 3D drawlist
    gptGfx->register_3d_drawlist(ptGraphics, &ptView->t3DDrawList);

    const plBufferDescription atGlobalBuffersDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_UNIFORM,
        .uByteSize            = sizeof(BindGroup_0)
    };
    ptView->atGlobalBuffers[0] = gptDevice->create_buffer(&ptGraphics->tDevice, &atGlobalBuffersDesc, "global buffer 0");
    ptView->atGlobalBuffers[1] = gptDevice->create_buffer(&ptGraphics->tDevice, &atGlobalBuffersDesc, "global buffer 1");

    return uViewHandle;
}

static void
pl_refr_resize_view(uint32_t uSceneHandle, uint32_t uViewHandle, plVec2 tDimensions)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->sbtViews[uViewHandle];

    // update offscreen size to match viewport
    ptView->tTargetSize = tDimensions;

    // queue old textures & texture views for deletion
    gptDevice->queue_texture_view_for_deletion(ptDevice, ptView->tTextureView);
    gptDevice->queue_texture_view_for_deletion(ptDevice, ptView->tDepthTextureView);
    gptDevice->queue_texture_for_deletion(ptDevice, ptView->tTexture);
    gptDevice->queue_texture_for_deletion(ptDevice, ptView->tDepthTexture);

    // recreate offscreen color & depth textures
    const plTextureDesc tOffscreenTextureDesc = {
        .tDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_SAMPLED | PL_TEXTURE_USAGE_COLOR_ATTACHMENT
    };
    ptView->tTexture = gptDevice->create_texture(&ptGraphics->tDevice, tOffscreenTextureDesc, "offscreen texture");

    const plTextureDesc tOffscreenDepthTextureDesc = {
        .tDimensions = {ptView->tTargetSize.x, ptView->tTargetSize.y, 1},
        .tFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uLayers     = 1,
        .uMips       = 1,
        .tType       = PL_TEXTURE_TYPE_2D,
        .tUsage      = PL_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT
    };
    ptView->tDepthTexture = gptDevice->create_texture(&ptGraphics->tDevice, tOffscreenDepthTextureDesc, "offscreen depth texture");

    // create offscreen color & depth texture views
    const plTextureViewDesc tOffscreenTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };

    const plTextureViewDesc tOffscreenDepthTextureViewDesc = {
        .tFormat     = PL_FORMAT_D32_FLOAT_S8_UINT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 1
    };

    const plSampler tOffscreenSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = 1.0F,
        .tVerticalWrap = PL_WRAP_MODE_CLAMP,
        .tHorizontalWrap = PL_WRAP_MODE_CLAMP
    };
    ptView->tTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tOffscreenTextureViewDesc, &tOffscreenSampler, ptView->tTexture, "offscreen texture view");
    ptView->tTextureID = gptGfx->get_ui_texture_handle(ptGraphics, ptView->tTextureView);
    ptView->tDepthTextureView = gptDevice->create_texture_view(&ptGraphics->tDevice, &tOffscreenDepthTextureViewDesc, &tOffscreenSampler, ptView->tDepthTexture, "offscreen depth texture view");

    // update offscreen render pass attachments
    const plRenderPassAttachments atOffscreenAttachmentSets[] = {
        {
            .atViewAttachments = {
                ptView->tTextureView,
                ptView->tDepthTextureView,
            }
        }
    };
    gptDevice->update_render_pass_attachments(ptDevice, ptView->tRenderPass, ptView->tTargetSize, atOffscreenAttachmentSets);
}

static void
pl_refr_resize(void)
{
    // for convience
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plIO* ptIO = pl_get_io();

    gptGfx->resize(ptGraphics);


}

static void
pl_refr_cleanup(void)
{
    gptECS->cleanup_component_library(&gptData->tComponentLibrary);
    gptStream->cleanup(&gptData->tDrawStream);

    pl_hm_free(&gptData->tNodeHashMap);
    pl_hm_free(&gptData->tSkinHashMap);
    pl_hm_free(&gptData->tJointHashMap);
    
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
    {
        plRefScene* ptScene = &gptData->sbtScenes[i];
        for(uint32_t j = 0; j < pl_sb_size(ptScene->sbtViews); j++)
        {
            plRefView* ptView = &ptScene->sbtViews[j];
            pl_sb_free(ptView->sbtVisibleDrawables);
        }
        pl_sb_free(ptScene->sbtVertexPosBuffer);
        pl_sb_free(ptScene->sbtVertexDataBuffer);
        pl_sb_free(ptScene->sbuIndexBuffer);
        pl_sb_free(ptScene->sbtMaterialBuffer);
        pl_sb_free(ptScene->sbtAllDrawables);
        pl_sb_free(ptScene->sbtOpaqueDrawables);
        pl_sb_free(ptScene->sbtTransparentDrawables);
        pl_sb_free(ptScene->sbtMaterialEntities);
        pl_sb_free(ptScene->sbtMaterialBindGroups);
        pl_sb_free(ptScene->sbtSkinData);
        pl_hm_free(&ptScene->tMaterialHashMap);
    }
    gptGfx->cleanup(&gptData->tGraphics);

    // must be cleaned up after graphics since 3D drawlist are registered as pointers
    for(uint32_t i = 0; i < pl_sb_size(gptData->sbtScenes); i++)
    {
        pl_sb_free(gptData->sbtScenes[i].sbtViews);
    }
    pl_sb_free(gptData->sbtScenes);
    PL_FREE(gptData);
}

static plComponentLibrary*
pl_refr_get_component_library(void)
{
    return &gptData->tComponentLibrary;
}

static plGraphics*
pl_refr_get_graphics(void)
{
    return &gptData->tGraphics;
}

static void
pl_refr_load_skybox_from_panorama(uint32_t uSceneHandle, const char* pcPath, int iResolution)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    int iPanoramaWidth = 0;
    int iPanoramaHeight = 0;
    int iUnused = 0;
    float* pfPanoramaData = gptImage->loadf(pcPath, &iPanoramaWidth, &iPanoramaHeight, &iUnused, 4);
    PL_ASSERT(pfPanoramaData);

    plComputeShaderDescription tSkyboxComputeShaderDesc = {
#ifdef PL_METAL_BACKEND
        .pcShader = "panorama_to_cubemap.metal",
        .pcShaderEntryFunc = "kernel_main",
#else
        .pcShader = "panorama_to_cubemap.comp.spv",
        .pcShaderEntryFunc = "main",
#endif
        .uConstantCount = 3,
        .atConstants = {
            { .uID = 0, .uOffset = 0,               .tType = PL_DATA_TYPE_INT},
            { .uID = 1, .uOffset = sizeof(int),     .tType = PL_DATA_TYPE_INT},
            { .uID = 2, .uOffset = 2 * sizeof(int), .tType = PL_DATA_TYPE_INT}
        },
        .tBindGroupLayout = {
            .uBufferCount = 7,
            .aBuffers = {
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
                { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
            },
        }
    };
    int aiSkyboxSpecializationData[] = {iResolution, iPanoramaWidth, iPanoramaHeight};
    tSkyboxComputeShaderDesc.pTempConstantData = aiSkyboxSpecializationData;
    gptData->tPanoramaShader = gptDevice->create_compute_shader(ptDevice, &tSkyboxComputeShaderDesc);

    plBufferHandle atComputeBuffers[7] = {0};
    const uint32_t uPanoramaSize = iPanoramaHeight * iPanoramaWidth * 4 * sizeof(float);
    const plBufferDescription tInputBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = uPanoramaSize
    };
    atComputeBuffers[0] = gptDevice->create_buffer(ptDevice, &tInputBufferDesc, "panorama input");
    plBuffer* ptComputeBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[0]);
    memcpy(ptComputeBuffer->tMemoryAllocation.pHostMapped, pfPanoramaData, iPanoramaWidth * iPanoramaHeight * 4 * sizeof(float));

    const size_t uFaceSize = ((size_t)iResolution * (size_t)iResolution) * 4 * sizeof(float);
    const plBufferDescription tOutputBufferDesc = {
        .tMemory              = PL_MEMORY_GPU_CPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = (uint32_t)uFaceSize
    };
    
    for(uint32_t i = 0; i < 6; i++)
        atComputeBuffers[i + 1] = gptDevice->create_buffer(ptDevice, &tOutputBufferDesc, "panorama output");

    plBindGroupLayout tComputeBindGroupLayout = {
        .uBufferCount = 7,
        .aBuffers = {
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 0, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 1, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 2, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 3, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 4, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 5, .tStages = PL_STAGE_COMPUTE},
            { .tType = PL_BUFFER_BINDING_TYPE_STORAGE, .uSlot = 6, .tStages = PL_STAGE_COMPUTE},
        },
    };
    plBindGroupHandle tComputeBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tComputeBindGroupLayout);
    size_t szBufferRangeSize[] = {(size_t)uPanoramaSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize, uFaceSize};
    gptDevice->update_bind_group(ptDevice, &tComputeBindGroup, 7, atComputeBuffers, szBufferRangeSize, 0, NULL);

    plDispatch tDispach = {
        .uBindGroup0      = tComputeBindGroup.uIndex,
        .uGroupCountX     = (uint32_t)iResolution / 16,
        .uGroupCountY     = (uint32_t)iResolution / 16,
        .uGroupCountZ     = 2,
        .uThreadPerGroupX = 16,
        .uThreadPerGroupY = 16,
        .uThreadPerGroupZ = 3,
        .uShaderVariant   = gptData->tPanoramaShader.uIndex
    };
    gptGfx->dispatch(ptGraphics, 1, &tDispach);

    // get data
    char* pcResultData = PL_ALLOC(uFaceSize * 6);
    memset(pcResultData, 0, uFaceSize * 6);
    float* pfBlah0 = (float*)&pcResultData[0];
    float* pfBlah1 = (float*)&pcResultData[uFaceSize];
    float* pfBlah2 = (float*)&pcResultData[uFaceSize * 2];
    float* pfBlah3 = (float*)&pcResultData[uFaceSize * 3];
    float* pfBlah4 = (float*)&pcResultData[uFaceSize * 4];
    float* pfBlah5 = (float*)&pcResultData[uFaceSize * 5];

    for(uint32_t i = 0; i < 6; i++)
    {
        plBuffer* ptBuffer = gptDevice->get_buffer(ptDevice, atComputeBuffers[i + 1]);
        memcpy(&pcResultData[uFaceSize * i], ptBuffer->tMemoryAllocation.pHostMapped, uFaceSize);
    }

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(&ptGraphics->tDevice, gptData->tStagingBufferHandle);
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, pcResultData, uFaceSize * 6);

    plTextureDesc tTextureDesc = {
        .tDimensions = {(float)iResolution, (float)iResolution, 1},
        .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
        .uLayers = 6,
        .uMips = 1,
        .tType = PL_TEXTURE_TYPE_CUBE,
        .tUsage = PL_TEXTURE_USAGE_SAMPLED
    };
    ptScene->tSkyboxTexture = gptDevice->create_texture(ptDevice, tTextureDesc, "skybox texture");

    plBufferImageCopy atBufferImageCopy[6] = {0};
    for(uint32_t i = 0; i < 6; i++)
    {
        atBufferImageCopy[i].tImageExtent = (plExtent){iResolution, iResolution, 1};
        atBufferImageCopy[i].uLayerCount = 1;
        atBufferImageCopy[i].szBufferOffset = i * uFaceSize;
        atBufferImageCopy[i].uBaseArrayLayer = i;
    }
    gptDevice->copy_buffer_to_texture(&ptGraphics->tDevice, gptData->tStagingBufferHandle, ptScene->tSkyboxTexture, 6, atBufferImageCopy);

    plTextureViewDesc tTextureViewDesc = {
        .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
        .uBaseLayer  = 0,
        .uBaseMip    = 0,
        .uLayerCount = 6
    };
    plSampler tSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = PL_MAX_MIPS,
        .tVerticalWrap = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };
    ptScene->tSkyboxTextureView = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, ptScene->tSkyboxTexture, "skybox texture view"); 

    // cleanup
    PL_FREE(pcResultData);
    
    for(uint32_t i = 0; i < 7; i++)
        gptDevice->destroy_buffer(ptDevice, atComputeBuffers[i]);

    gptImage->free(pfPanoramaData);

    plBindGroupLayout tSkyboxBindGroupLayout = {
        .uTextureCount  = 1,
        .aTextures = { {.uSlot = 0, .tStages = PL_STAGE_PIXEL | PL_STAGE_VERTEX}}
    };
    ptScene->tSkyboxBindGroup = gptDevice->create_bind_group(ptDevice, &tSkyboxBindGroupLayout);
    gptDevice->update_bind_group(ptDevice, &ptScene->tSkyboxBindGroup, 0, NULL, NULL, 1, &ptScene->tSkyboxTextureView);

    const uint32_t uStartIndex     = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);

    const plDrawable tDrawable = {
        .uIndexCount   = 36,
        .uVertexCount  = 8,
        .uIndexOffset  = uIndexStart,
        .uVertexOffset = uStartIndex,
        .uDataOffset   = uDataStartIndex,
    };
    ptScene->tSkyboxDrawable = tDrawable;

    // indices
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);

    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 3);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 7);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 2);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 6);
    
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 0);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 1);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 5);
    pl_sb_push(ptScene->sbuIndexBuffer, uStartIndex + 4);

    // vertices (position)
    const float fCubeSide = 0.5f;
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide, -fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide, -fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){-fCubeSide,  fCubeSide,  fCubeSide}));
    pl_sb_push(ptScene->sbtVertexPosBuffer, ((plVec3){ fCubeSide,  fCubeSide,  fCubeSide})); 
}

static void
pl_refr_load_stl(uint32_t uSceneHandle, const char* pcModelPath, plVec4 tColor, const plMat4* ptTransform)
{

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // read in STL file
    uint32_t uFileSize = 0;
    gptFile->read(pcModelPath, &uFileSize, NULL, "rb");
    char* pcBuffer = PL_ALLOC(uFileSize);
    memset(pcBuffer, 0, uFileSize);
    gptFile->read(pcModelPath, &uFileSize, pcBuffer, "rb");

    // create ECS object component
    plEntity tEntity = gptECS->create_object(&gptData->tComponentLibrary, pcModelPath);

    // retrieve actual components
    plMeshComponent*      ptMesh          = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tEntity);
    plTransformComponent* ptTransformComp = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tEntity);
    
    // set transform if present
    if(ptTransform)
    {
        ptTransformComp->tWorld = *ptTransform;
        pl_decompose_matrix(&ptTransformComp->tWorld, &ptTransformComp->tScale, &ptTransformComp->tRotation, &ptTransformComp->tTranslation);
    }

    // create simple material
    ptMesh->tMaterial = gptECS->create_material(&gptData->tComponentLibrary, pcModelPath);
    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
    ptMaterial->tBaseColor = tColor;
    ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_ALPHA;
    
    // load STL model
    plStlInfo tInfo = {0};
    pl_load_stl(pcBuffer, (size_t)uFileSize, NULL, NULL, NULL, &tInfo);

    ptMesh->ulVertexStreamMask = PL_MESH_FORMAT_FLAG_HAS_NORMAL;
    pl_sb_resize(ptMesh->sbtVertexPositions, (uint32_t)(tInfo.szPositionStreamSize / 3));
    pl_sb_resize(ptMesh->sbtVertexNormals, (uint32_t)(tInfo.szNormalStreamSize / 3));
    pl_sb_resize(ptMesh->sbuIndices, (uint32_t)tInfo.szIndexBufferSize);

    pl_load_stl(pcBuffer, (size_t)uFileSize, (float*)ptMesh->sbtVertexPositions, (float*)ptMesh->sbtVertexNormals, (uint32_t*)ptMesh->sbuIndices, &tInfo);
    PL_FREE(pcBuffer);

    // calculate AABB
    ptMesh->tAABB.tMax = (plVec3){-FLT_MAX, -FLT_MAX, -FLT_MAX};
    ptMesh->tAABB.tMin = (plVec3){FLT_MAX, FLT_MAX, FLT_MAX};
    
    for(uint32_t i = 0; i < pl_sb_size(ptMesh->sbtVertexPositions); i++)
    {
        if(ptMesh->sbtVertexPositions[i].x > ptMesh->tAABB.tMax.x) ptMesh->tAABB.tMax.x = ptMesh->sbtVertexPositions[i].x;
        if(ptMesh->sbtVertexPositions[i].y > ptMesh->tAABB.tMax.y) ptMesh->tAABB.tMax.y = ptMesh->sbtVertexPositions[i].y;
        if(ptMesh->sbtVertexPositions[i].z > ptMesh->tAABB.tMax.z) ptMesh->tAABB.tMax.z = ptMesh->sbtVertexPositions[i].z;
        if(ptMesh->sbtVertexPositions[i].x < ptMesh->tAABB.tMin.x) ptMesh->tAABB.tMin.x = ptMesh->sbtVertexPositions[i].x;
        if(ptMesh->sbtVertexPositions[i].y < ptMesh->tAABB.tMin.y) ptMesh->tAABB.tMin.y = ptMesh->sbtVertexPositions[i].y;
        if(ptMesh->sbtVertexPositions[i].z < ptMesh->tAABB.tMin.z) ptMesh->tAABB.tMin.z = ptMesh->sbtVertexPositions[i].z;
    }

    // create material bind group (set 1)
    plBindGroupLayout tMaterialBindGroupLayout = {
        .uTextureCount = 2,
        .aTextures = {
            {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
            {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
        }
    };
    plBindGroupHandle tMaterialBindGroup = gptDevice->create_bind_group(&gptData->tGraphics.tDevice, &tMaterialBindGroupLayout);

    const plDrawable tDrawable = {
        .tEntity            = tEntity,
        .tMaterialBindGroup = tMaterialBindGroup
    };

    if(tColor.a == 1.0f)
        pl_sb_push(ptScene->sbtOpaqueDrawables, tDrawable);
    else
        pl_sb_push(ptScene->sbtTransparentDrawables, tDrawable);
    pl_sb_push(ptScene->sbtMaterialEntities, ptMesh->tMaterial);
    pl_sb_push(ptScene->sbtMaterialBindGroups, tMaterialBindGroup);
}

static void
pl_refr_load_gltf(uint32_t uSceneHandle, const char* pcPath, const plMat4* ptTransform)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    cgltf_options tGltfOptions = {0};
    cgltf_data* ptGltfData = NULL;

    char acDirectory[1024] = {0};
    pl_str_get_directory(pcPath, acDirectory);

    cgltf_result tGltfResult = cgltf_parse_file(&tGltfOptions, pcPath, &ptGltfData);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    tGltfResult = cgltf_load_buffers(&tGltfOptions, ptGltfData, pcPath);
    PL_ASSERT(tGltfResult == cgltf_result_success);

    for(size_t szSkinIndex = 0; szSkinIndex < ptGltfData->skins_count; szSkinIndex++)
    {
        const cgltf_skin* ptSkin = &ptGltfData->skins[szSkinIndex];


        plEntity tSkinEntity = gptECS->create_skin(&gptData->tComponentLibrary, ptSkin->name);
        plSkinComponent* ptSkinComponent = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, tSkinEntity);

        pl_sb_resize(ptSkinComponent->sbtJoints, (uint32_t)ptSkin->joints_count);
        pl_sb_resize(ptSkinComponent->sbtInverseBindMatrices, (uint32_t)ptSkin->joints_count);
        for(size_t szJointIndex = 0; szJointIndex < ptSkin->joints_count; szJointIndex++)
        {
            const cgltf_node* ptJointNode = ptSkin->joints[szJointIndex];
            plEntity tTransformEntity = gptECS->create_transform(&gptData->tComponentLibrary, ptJointNode->name);
            ptSkinComponent->sbtJoints[szJointIndex] = tTransformEntity;
            pl_hm_insert(&gptData->tJointHashMap, (uint64_t)ptJointNode, tTransformEntity.ulData);
        }
        if(ptSkin->inverse_bind_matrices)
        {
            const cgltf_buffer_view* ptInverseBindMatrixView = ptSkin->inverse_bind_matrices->buffer_view;
            const char* pcBufferData = ptInverseBindMatrixView->buffer->data;
            memcpy(ptSkinComponent->sbtInverseBindMatrices, &pcBufferData[ptInverseBindMatrixView->offset], sizeof(plMat4) * ptSkin->joints_count);
        }
        pl_hm_insert(&gptData->tSkinHashMap, (uint64_t)ptSkin, tSkinEntity.ulData);
    }

    for(size_t i = 0; i < ptGltfData->scenes_count; i++)
    {
        const cgltf_scene* ptGScene = &ptGltfData->scenes[i];
        for(size_t j = 0; j < ptGScene->nodes_count; j++)
        {
            const cgltf_node* ptNode = ptGScene->nodes[j];
            pl__refr_load_gltf_object(ptScene, acDirectory, (plEntity){UINT32_MAX, UINT32_MAX}, ptNode);
            if(ptTransform)
            {
                const plEntity tNodeEntity = {.ulData = pl_hm_lookup(&gptData->tNodeHashMap, (uint64_t)ptNode)};
                plTransformComponent* ptTransformComponent = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNodeEntity);
                ptTransformComponent->tWorld = pl_mul_mat4(ptTransform, &ptTransformComponent->tWorld);
                pl_decompose_matrix(&ptTransformComponent->tWorld, &ptTransformComponent->tScale, &ptTransformComponent->tRotation, &ptTransformComponent->tTranslation);
            }

        }
    }

    for(size_t i = 0; i < ptGltfData->animations_count; i++)
    {
        const cgltf_animation* ptAnimation = &ptGltfData->animations[i];
        pl__refr_load_gltf_animation(ptScene, ptAnimation);
    }

    pl_hm_free(&gptData->tNodeHashMap);
    pl_hm_free(&gptData->tJointHashMap);
    pl_hm_free(&gptData->tSkinHashMap);
    pl_hm_free(&ptScene->tMaterialHashMap);
}

static void
pl__load_gltf_texture(plTextureSlot tSlot, const cgltf_texture_view* ptTexture, const char* pcDirectory, const cgltf_material* ptGltfMaterial, plMaterialComponent* ptMaterial)
{
    ptMaterial->atTextureMaps[tSlot].uUVSet = ptTexture->texcoord;

    if(ptTexture->texture->image->buffer_view)
    {
        static int iSeed = 0;
        iSeed++;
        char* pucBufferData = ptTexture->texture->image->buffer_view->buffer->data;
        char* pucActualBuffer = &pucBufferData[ptTexture->texture->image->buffer_view->offset];
        ptMaterial->atTextureMaps[tSlot].acName[0] = (char)(tSlot + iSeed);
        strncpy(&ptMaterial->atTextureMaps[tSlot].acName[1], pucActualBuffer, 127);
        
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptMaterial->atTextureMaps[tSlot].acName, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, pucActualBuffer, ptTexture->texture->image->buffer_view->size);
    }
    else if(strncmp(ptTexture->texture->image->uri, "data:", 5) == 0)
    {
        const char* comma = strchr(ptTexture->texture->image->uri, ',');

        if (comma && comma - ptTexture->texture->image->uri >= 7 && strncmp(comma - 7, ";base64", 7) == 0)
        {
            cgltf_options tOptions = {0};
            ptMaterial->atTextureMaps[tSlot].acName[0] = (char)tSlot + 1;
            strcpy(&ptMaterial->atTextureMaps[tSlot].acName[1], ptGltfMaterial->name);
            
            void* outData = NULL;
            const char *base64 = comma + 1;
            const size_t szBufferLength = strlen(base64);
            size_t szSize = szBufferLength - szBufferLength / 4;
            if(szBufferLength >= 2)
            {
                szSize -= base64[szBufferLength - 2] == '=';
                szSize -= base64[szBufferLength - 1] == '=';
            }
            cgltf_result res = cgltf_load_buffer_base64(&tOptions, szSize, base64, &outData);
            PL_ASSERT(res == cgltf_result_success);
            ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptMaterial->atTextureMaps[tSlot].acName, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, outData, szSize);
        }
    }
    else
    {
        strncpy(ptMaterial->atTextureMaps[tSlot].acName, ptTexture->texture->image->uri, PL_MAX_NAME_LENGTH);
        char acFilepath[2048] = {0};
        strcpy(acFilepath, pcDirectory);
        pl_str_concatenate(acFilepath, ptMaterial->atTextureMaps[tSlot].acName, acFilepath, 2048);

        uint32_t uFileSize = 0;
        gptFile->read(acFilepath, &uFileSize, NULL, "rb");
        char* pcBuffer = PL_ALLOC(uFileSize);
        memset(pcBuffer, 0, uFileSize);
        gptFile->read(acFilepath, &uFileSize, pcBuffer, "rb");
        ptMaterial->atTextureMaps[tSlot].tResource = gptResource->load_resource(ptTexture->texture->image->uri, PL_RESOURCE_LOAD_FLAG_RETAIN_DATA, pcBuffer, (size_t)uFileSize);
        PL_FREE(pcBuffer);
    }
}

static void
pl__refr_load_material(const char* pcDirectory, plMaterialComponent* ptMaterial, const cgltf_material* ptGltfMaterial)
{
    ptMaterial->tShaderType = PL_SHADER_TYPE_PBR;
    ptMaterial->tFlags = ptGltfMaterial->double_sided ? PL_MATERIAL_FLAG_DOUBLE_SIDED : PL_MATERIAL_FLAG_NONE;
    ptMaterial->fAlphaCutoff = ptGltfMaterial->alpha_cutoff;

    // blend mode
    if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_opaque)
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_OPAQUE;
    else if(ptGltfMaterial->alpha_mode == cgltf_alpha_mode_mask)
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_ALPHA;
    else
        ptMaterial->tBlendMode = PL_MATERIAL_BLEND_MODE_PREMULTIPLIED;

	if(ptGltfMaterial->normal_texture.texture)
		pl__load_gltf_texture(PL_TEXTURE_SLOT_NORMAL_MAP, &ptGltfMaterial->normal_texture, pcDirectory, ptGltfMaterial, ptMaterial);

    if(ptGltfMaterial->has_pbr_metallic_roughness)
    {
        ptMaterial->tBaseColor.x = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[0];
        ptMaterial->tBaseColor.y = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[1];
        ptMaterial->tBaseColor.z = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[2];
        ptMaterial->tBaseColor.w = ptGltfMaterial->pbr_metallic_roughness.base_color_factor[3];

        if(ptGltfMaterial->pbr_metallic_roughness.base_color_texture.texture)
			pl__load_gltf_texture(PL_TEXTURE_SLOT_BASE_COLOR_MAP, &ptGltfMaterial->pbr_metallic_roughness.base_color_texture, pcDirectory, ptGltfMaterial, ptMaterial);
    }
}

static void
pl__refr_load_attributes(plMeshComponent* ptMesh, const cgltf_primitive* ptPrimitive)
{
    const size_t szVertexCount = ptPrimitive->attributes[0].data->count;
    for(size_t szAttributeIndex = 0; szAttributeIndex < ptPrimitive->attributes_count; szAttributeIndex++)
    {
        const cgltf_attribute* ptAttribute = &ptPrimitive->attributes[szAttributeIndex];
        const cgltf_buffer* ptBuffer = ptAttribute->data->buffer_view->buffer;
        const size_t szStride = ptAttribute->data->stride;
        PL_ASSERT(szStride > 0 && "attribute stride must node be zero");

        unsigned char* pucBufferStart = &((unsigned char*)ptBuffer->data)[ptAttribute->data->buffer_view->offset + ptAttribute->data->offset];

        switch(ptAttribute->type)
        {
            case cgltf_attribute_type_position:
            {
                ptMesh->tAABB.tMax = (plVec3){ptAttribute->data->max[0], ptAttribute->data->max[1], ptAttribute->data->max[2]};
                ptMesh->tAABB.tMin = (plVec3){ptAttribute->data->min[0], ptAttribute->data->min[1], ptAttribute->data->min[2]};
                pl_sb_resize(ptMesh->sbtVertexPositions, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexPositions[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_normal:
            {
                pl_sb_resize(ptMesh->sbtVertexNormals, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec3* ptRawData = (plVec3*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexNormals[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_tangent:
            {
                pl_sb_resize(ptMesh->sbtVertexTangents, (uint32_t)szVertexCount);
                for(size_t i = 0; i < szVertexCount; i++)
                {
                    plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                    ptMesh->sbtVertexTangents[i] = *ptRawData;
                }
                break;
            }

            case cgltf_attribute_type_texcoord:
            {
                pl_sb_resize(ptMesh->sbtVertexTextureCoordinates[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec2* ptRawData = (plVec2*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexTextureCoordinates[ptAttribute->index])[i].y = (float)puRawData[1];
                    }
                }
                break;
            }

            case cgltf_attribute_type_color:
            {
                pl_sb_resize(ptMesh->sbtVertexColors[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    const float fConversion = 1.0f / (256.0f * 256.0f);
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].r = (float)puRawData[0] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].g = (float)puRawData[1] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].b = (float)puRawData[2] * fConversion;
                        (ptMesh->sbtVertexColors[ptAttribute->index])[i].a = (float)puRawData[3] * fConversion;
                    }
                }
                else
                {
                    PL_ASSERT(false);
                }

                break;
            }

            case cgltf_attribute_type_joints:
            {
                pl_sb_resize(ptMesh->sbtVertexJoints[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexJoints[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            case cgltf_attribute_type_weights:
            {
                pl_sb_resize(ptMesh->sbtVertexWeights[ptAttribute->index], (uint32_t)szVertexCount);
                
                if(ptAttribute->data->component_type == cgltf_component_type_r_32f)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        plVec4* ptRawData = (plVec4*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i] = *ptRawData;
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_16u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint16_t* puRawData = (uint16_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                else if(ptAttribute->data->component_type == cgltf_component_type_r_8u)
                {
                    for(size_t i = 0; i < szVertexCount; i++)
                    {
                        uint8_t* puRawData = (uint8_t*)&pucBufferStart[i * szStride];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].x = (float)puRawData[0];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].y = (float)puRawData[1];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].z = (float)puRawData[2];
                        (ptMesh->sbtVertexWeights[ptAttribute->index])[i].w = (float)puRawData[3];
                    }
                }
                break;
            }

            default:
            {
                PL_ASSERT(false && "unknown attribute");
            }
        }
    }

    // index buffer
    if(ptPrimitive->indices)
    {
        pl_sb_resize(ptMesh->sbuIndices, (uint32_t)ptPrimitive->indices->count);
        unsigned char* pucIdexBufferStart = &((unsigned char*)ptPrimitive->indices->buffer_view->buffer->data)[ptPrimitive->indices->buffer_view->offset + ptPrimitive->indices->offset];
        switch(ptPrimitive->indices->component_type)
        {
            case cgltf_component_type_r_32u:
            {
                
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = *(uint32_t*)&pucIdexBufferStart[i * sizeof(uint32_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = *(uint32_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }

            case cgltf_component_type_r_16u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * sizeof(unsigned short)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(unsigned short*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            case cgltf_component_type_r_8u:
            {
                if(ptPrimitive->indices->buffer_view->stride == 0)
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * sizeof(uint8_t)];
                }
                else
                {
                    for(uint32_t i = 0; i < ptPrimitive->indices->count; i++)
                        ptMesh->sbuIndices[i] = (uint32_t)*(uint8_t*)&pucIdexBufferStart[i * ptPrimitive->indices->buffer_view->stride];
                }
                break;
            }
            default:
            {
                PL_ASSERT(false);
            }
        }
    }


}

static void
pl__refr_load_gltf_animation(plRefScene* ptScene, const cgltf_animation* ptAnimation)
{
    plComponentLibrary* ptLibrary = &gptData->tComponentLibrary;

    plEntity tNewAnimationEntity = gptECS->create_animation(ptLibrary, ptAnimation->name);
    plAnimationComponent* ptAnimationComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION, tNewAnimationEntity);

    // load channels
    for(size_t i = 0; i < ptAnimation->channels_count; i++)
    {
        const cgltf_animation_channel* ptChannel = &ptAnimation->channels[i];
        plAnimationChannel tChannel = {.uSamplerIndex = pl_sb_size(ptAnimationComp->sbtSamplers)};
        switch(ptChannel->target_path)
        {
            case cgltf_animation_path_type_translation:
                tChannel.tPath = PL_ANIMATION_PATH_TRANSLATION;
                break;
            case cgltf_animation_path_type_rotation:
                tChannel.tPath = PL_ANIMATION_PATH_ROTATION;
                break;
            case cgltf_animation_path_type_scale:
                tChannel.tPath = PL_ANIMATION_PATH_SCALE;
                break;
            case cgltf_animation_path_type_weights:
                tChannel.tPath = PL_ANIMATION_PATH_WEIGHTS;
                break;
            default:
                tChannel.tPath = PL_ANIMATION_PATH_UNKNOWN;

        }

        const cgltf_animation_sampler* ptSampler = ptChannel->sampler;
        plAnimationSampler tSampler = {0};

        switch(ptSampler->interpolation)
        {
            case cgltf_interpolation_type_linear:
                tSampler.tMode = PL_ANIMATION_MODE_LINEAR;
                break;
            case cgltf_interpolation_type_step:
                tSampler.tMode = PL_ANIMATION_MODE_STEP;
                break;
            case cgltf_interpolation_type_cubic_spline:
                tSampler.tMode = PL_ANIMATION_MODE_CUBIC_SPLINE;
                break;
            default:
                tSampler.tMode = PL_ANIMATION_MODE_UNKNOWN;
        }

        tSampler.tData = gptECS->create_animation_data(ptLibrary, ptSampler->input->name);
        plAnimationDataComponent* ptAnimationDataComp = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_ANIMATION_DATA, tSampler.tData);

        ptAnimationComp->fEnd = pl_maxf(ptAnimationComp->fEnd, ptSampler->input->max[0]);

        const cgltf_buffer* ptInputBuffer = ptSampler->input->buffer_view->buffer;
        const cgltf_buffer* ptOutputBuffer = ptSampler->output->buffer_view->buffer;
        unsigned char* pucInputBufferStart = &((unsigned char*)ptInputBuffer->data)[ptSampler->input->buffer_view->offset + ptSampler->input->offset];
        unsigned char* pucOutputBufferStart = &((unsigned char*)ptOutputBuffer->data)[ptSampler->output->buffer_view->offset + ptSampler->output->offset];


        for(size_t j = 0; j < ptSampler->input->count; j++)
        {
            const float fValue = *(float*)&pucInputBufferStart[ptSampler->input->stride * j];
            pl_sb_push(ptAnimationDataComp->sbfKeyFrameTimes, fValue);
        }

        for(size_t j = 0; j < ptSampler->output->count; j++)
        {
            if(ptSampler->output->type == cgltf_type_scalar)
            {
                const float fValue0 = *(float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue0);
            }
            else if(ptSampler->output->type == cgltf_type_vec3)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                const float fValue0 = fFloatData[0];
                const float fValue1 = fFloatData[1];
                const float fValue2 = fFloatData[2];
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue0);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue1);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue2);
            }
            else if(ptSampler->output->type == cgltf_type_vec4)
            {
                float* fFloatData = (float*)&pucOutputBufferStart[ptSampler->output->stride * j];
                const float fValue0 = fFloatData[0];
                const float fValue1 = fFloatData[1];
                const float fValue2 = fFloatData[2];
                const float fValue3 = fFloatData[3];
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue0);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue1);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue2);
                pl_sb_push(ptAnimationDataComp->sbfKeyFrameData, fValue3);
            }
        }

        const uint64_t ulTargetEntity = pl_hm_lookup(&gptData->tNodeHashMap, (uint64_t)ptChannel->target_node);
        tChannel.tTarget = *(plEntity*)&ulTargetEntity;

        pl_sb_push(ptAnimationComp->sbtSamplers, tSampler);
        pl_sb_push(ptAnimationComp->sbtChannels, tChannel);
    }
}

static void
pl__refr_load_gltf_object(plRefScene* ptScene, const char* pcDirectory, plEntity tParentEntity, const cgltf_node* ptNode)
{
    plComponentLibrary* ptLibrary = &gptData->tComponentLibrary;

    plEntity tNewEntity = {UINT32_MAX, UINT32_MAX};
    const uint64_t ulObjectIndex = pl_hm_lookup(&gptData->tJointHashMap, (uint64_t)ptNode);
    if(ulObjectIndex != UINT64_MAX)
    {
        tNewEntity.ulData = ulObjectIndex;
    }
    else
    {
        tNewEntity = gptECS->create_transform(ptLibrary, ptNode->name);
    }

    plTransformComponent* ptTransform = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_TRANSFORM, tNewEntity);
    pl_hm_insert(&gptData->tNodeHashMap, (uint64_t)ptNode, tNewEntity.ulData);

    // transform defaults
    ptTransform->tWorld       = pl_identity_mat4();
    ptTransform->tRotation    = (plVec4){0.0f, 0.0f, 0.0f, 1.0f};
    ptTransform->tScale       = (plVec3){1.0f, 1.0f, 1.0f};
    ptTransform->tTranslation = (plVec3){0.0f, 0.0f, 0.0f};

    if(ptNode->has_rotation)    memcpy(ptTransform->tRotation.d, ptNode->rotation, sizeof(plVec4));
    if(ptNode->has_scale)       memcpy(ptTransform->tScale.d, ptNode->scale, sizeof(plVec3));
    if(ptNode->has_translation) memcpy(ptTransform->tTranslation.d, ptNode->translation, sizeof(plVec3));

    // must use provided matrix, otherwise calculate based on rot, scale, trans
    if(ptNode->has_matrix)
    {
        memcpy(ptTransform->tWorld.d, ptNode->matrix, sizeof(plMat4));
        pl_decompose_matrix(&ptTransform->tWorld, &ptTransform->tScale, &ptTransform->tRotation, &ptTransform->tTranslation);
    }
    else
        ptTransform->tWorld = pl_rotation_translation_scale(ptTransform->tRotation, ptTransform->tTranslation, ptTransform->tScale);


    // attach to parent if parent is valid
    if(tParentEntity.uIndex != UINT32_MAX)
        gptECS->attach_component(ptLibrary, tNewEntity, tParentEntity);

    // check if node has skin
    plEntity tSkinEntity = {UINT32_MAX, UINT32_MAX};
    if(ptNode->skin)
    {
        const uint64_t ulIndex = pl_hm_lookup(&gptData->tSkinHashMap, (uint64_t)ptNode->skin);

        if(ulIndex != UINT64_MAX)
        {
            tSkinEntity = *(plEntity*)&ulIndex;
            plSkinComponent* ptSkinComponent = gptECS->get_component(ptLibrary, PL_COMPONENT_TYPE_SKIN, tSkinEntity);
            ptSkinComponent->tMeshNode = tNewEntity; 
        }
    }

    // check if node has attached mesh
    if(ptNode->mesh)
    {
        // PL_ASSERT(ptNode->mesh->primitives_count == 1);
        for(size_t szPrimitiveIndex = 0; szPrimitiveIndex < ptNode->mesh->primitives_count; szPrimitiveIndex++)
        {
            // add mesh to our node
            plEntity tNewObject = gptECS->create_object(ptLibrary, ptNode->mesh->name);
            plObjectComponent* ptObject = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_OBJECT, tNewObject);
            plMeshComponent* ptMesh = gptECS->add_component(ptLibrary, PL_COMPONENT_TYPE_MESH, tNewObject);
            ptMesh->tSkinComponent = tSkinEntity;
            
            ptObject->tMesh = tNewObject;
            ptObject->tTransform = tNewEntity;

            const cgltf_primitive* ptPrimitive = &ptNode->mesh->primitives[szPrimitiveIndex];

            // load attributes
            pl__refr_load_attributes(ptMesh, ptPrimitive);

            ptMesh->tMaterial.uIndex      = UINT32_MAX;
            ptMesh->tMaterial.uGeneration = UINT32_MAX;

            // load material
            if(ptPrimitive->material)
            {
                bool bOpaque = true;
                plBindGroupHandle tMaterialBindGroup = {UINT32_MAX, UINT32_MAX};

                // check if the material already exists
                if(pl_hm_has_key(&ptScene->tMaterialHashMap, (uint64_t)ptPrimitive->material))
                {
                    const uint64_t ulMaterialIndex = pl_hm_lookup(&ptScene->tMaterialHashMap, (uint64_t)ptPrimitive->material);
                    ptMesh->tMaterial = ptScene->sbtMaterialEntities[ulMaterialIndex];
                    tMaterialBindGroup = ptScene->sbtMaterialBindGroups[ulMaterialIndex];

                    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
                    if(ptMaterial->tBlendMode != PL_MATERIAL_BLEND_MODE_OPAQUE)
                        bOpaque = false;
                }
                else // create new material
                {
                    ptMesh->tMaterial = gptECS->create_material(&gptData->tComponentLibrary, ptPrimitive->material->name);
                    
                    uint64_t ulFreeIndex = pl_hm_get_free_index(&ptScene->tMaterialHashMap);
                    if(ulFreeIndex == UINT64_MAX)
                    {
                        ulFreeIndex = pl_sb_size(ptScene->sbtMaterialEntities);
                        pl_sb_add(ptScene->sbtMaterialEntities);
                        pl_sb_add(ptScene->sbtMaterialBindGroups);
                    }

                    ptScene->sbtMaterialEntities[ulFreeIndex] = ptMesh->tMaterial;
                    pl_hm_insert(&ptScene->tMaterialHashMap, (uint64_t)ptPrimitive->material, ulFreeIndex);

                    plBindGroupLayout tMaterialBindGroupLayout = {
                        .uTextureCount = 2,
                        .aTextures = {
                            {.uSlot = 0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                            {.uSlot = 1, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL},
                        }
                    };
                    ptScene->sbtMaterialBindGroups[ulFreeIndex] = gptDevice->create_bind_group(&gptData->tGraphics.tDevice, &tMaterialBindGroupLayout);
                    tMaterialBindGroup = ptScene->sbtMaterialBindGroups[ulFreeIndex];

                    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);
                    pl__refr_load_material(pcDirectory, ptMaterial, ptPrimitive->material);

                    if(ptMaterial->tBlendMode != PL_MATERIAL_BLEND_MODE_OPAQUE)
                        bOpaque = false;
                }

                // TODO: separate by opaque/transparent
                const plDrawable tDrawable = {
                    .tEntity = tNewObject, 
                    .tMaterialBindGroup = tMaterialBindGroup
                };
                if(bOpaque)
                    pl_sb_push(ptScene->sbtOpaqueDrawables, tDrawable);
                else
                    pl_sb_push(ptScene->sbtTransparentDrawables, tDrawable);
            }
        }
    }

    // recurse through children
    for(size_t i = 0; i < ptNode->children_count; i++)
        pl__refr_load_gltf_object(ptScene, pcDirectory, tNewEntity, ptNode->children[i]);
}

static plTextureViewHandle
pl__create_texture_helper(plMaterialComponent* ptMaterial, plTextureSlot tSlot, bool bHdr, int iMips)
{
    plDevice* ptDevice = &gptData->tGraphics.tDevice;

    if(gptResource->is_resource_valid(ptMaterial->atTextureMaps[tSlot].tResource) == false)
        return gptData->tDummyTextureView;
    
    size_t szResourceSize = 0;
    const char* pcFileData = gptResource->get_file_data(ptMaterial->atTextureMaps[tSlot].tResource, &szResourceSize);
    int texWidth, texHeight, texNumChannels;
    int texForceNumChannels = 4;

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);
    
    plTextureHandle tTexture = {0};

    plSampler tSampler = {
        .tFilter = PL_FILTER_LINEAR,
        .fMinMip = 0.0f,
        .fMaxMip = PL_MAX_MIPS,
        .tVerticalWrap = PL_WRAP_MODE_WRAP,
        .tHorizontalWrap = PL_WRAP_MODE_WRAP
    };

    plTextureViewHandle tHandle = {UINT32_MAX, UINT32_MAX};

    if(bHdr)
    {

        float* rawBytes = gptImage->loadf_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
        PL_ASSERT(rawBytes);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, rawBytes, sizeof(float) * texWidth * texHeight * 4);
        gptImage->free(rawBytes);

        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)texWidth, (float)texHeight, 1},
            .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };
        gptDevice->copy_buffer_to_texture(ptDevice, gptData->tStagingBufferHandle, tTexture, 1, &tBufferImageCopy);
        gptDevice->generate_mipmaps(ptDevice, tTexture);
        

        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R32G32B32A32_FLOAT,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };
        tHandle = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tTexture, ptMaterial->atTextureMaps[tSlot].acName);
    }
    else
    {
        unsigned char* rawBytes = gptImage->load_from_memory((unsigned char*)pcFileData, (int)szResourceSize, &texWidth, &texHeight, &texNumChannels, texForceNumChannels);
        PL_ASSERT(rawBytes);

        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, rawBytes, texWidth * texHeight * 4);
        gptImage->free(rawBytes);


        plTextureDesc tTextureDesc = {
            .tDimensions = {(float)texWidth, (float)texHeight, 1},
            .tFormat = PL_FORMAT_R8G8B8A8_UNORM,
            .uLayers = 1,
            .uMips = iMips,
            .tType = PL_TEXTURE_TYPE_2D,
            .tUsage = PL_TEXTURE_USAGE_SAMPLED
        };
        tTexture = gptDevice->create_texture(ptDevice, tTextureDesc, ptMaterial->atTextureMaps[tSlot].acName);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {texWidth, texHeight, 1},
            .uLayerCount = 1
        };
        gptDevice->copy_buffer_to_texture(ptDevice, gptData->tStagingBufferHandle, tTexture, 1, &tBufferImageCopy);
        gptDevice->generate_mipmaps(ptDevice, tTexture);
        
        plTextureViewDesc tTextureViewDesc = {
            .tFormat     = PL_FORMAT_R8G8B8A8_UNORM,
            .uBaseLayer  = 0,
            .uBaseMip    = 0,
            .uLayerCount = 1
        };

        tHandle = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tTexture, ptMaterial->atTextureMaps[tSlot].acName);
    }

    return tHandle;
}

static void
pl__add_drawable_data_to_global_buffer(plRefScene* ptScene, uint32_t uDrawableIndex)
{
    plEntity tEntity = ptScene->sbtAllDrawables[uDrawableIndex].tEntity;

    // get actual components
    plObjectComponent*   ptObject   = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
    plMeshComponent*     ptMesh     = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
    plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

    const uint32_t uVertexPosStartIndex  = pl_sb_size(ptScene->sbtVertexPosBuffer);
    const uint32_t uIndexBufferStart     = pl_sb_size(ptScene->sbuIndexBuffer);
    const uint32_t uVertexDataStartIndex = pl_sb_size(ptScene->sbtVertexDataBuffer);
    const uint32_t uIndexCount           = pl_sb_size(ptMesh->sbuIndices);
    const uint32_t uVertexCount          = pl_sb_size(ptMesh->sbtVertexPositions);
    const uint32_t uMaterialIndex        = pl_sb_size(ptScene->sbtMaterialBuffer);

    // add index buffer data
    pl_sb_add_n(ptScene->sbuIndexBuffer, uIndexCount);
    for(uint32_t j = 0; j < uIndexCount; j++)
        ptScene->sbuIndexBuffer[uIndexBufferStart + j] = uVertexPosStartIndex + ptMesh->sbuIndices[j];

    // add vertex position data
    pl_sb_add_n(ptScene->sbtVertexPosBuffer, uVertexCount);
    memcpy(&ptScene->sbtVertexPosBuffer[uVertexPosStartIndex], ptMesh->sbtVertexPositions, sizeof(plVec3) * uVertexCount);

    // stride within storage buffer
    uint32_t uStride = 0;

    // calculate vertex stream mask based on provided data
    if(pl_sb_size(ptMesh->sbtVertexNormals) > 0)               { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_NORMAL; }
    if(pl_sb_size(ptMesh->sbtVertexTangents) > 0)              { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TANGENT; }
    if(pl_sb_size(ptMesh->sbtVertexColors[0]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_0; }
    if(pl_sb_size(ptMesh->sbtVertexColors[1]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_COLOR_1; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[0]) > 0)            { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexWeights[1]) > 0)            { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_WEIGHTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[0]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_0; }
    if(pl_sb_size(ptMesh->sbtVertexJoints[1]) > 0)             { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_JOINTS_1; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_0; }
    if(pl_sb_size(ptMesh->sbtVertexTextureCoordinates[1]) > 0) { uStride += 1; ptMesh->ulVertexStreamMask |= PL_MESH_FORMAT_FLAG_HAS_TEXCOORD_1; }

    pl_sb_add_n(ptScene->sbtVertexDataBuffer, uStride * uVertexCount);

    // current attribute offset
    uint32_t uOffset = 0;

    // normals
    const uint32_t uVertexNormalCount = pl_sb_size(ptMesh->sbtVertexNormals);
    for(uint32_t i = 0; i < uVertexNormalCount; i++)
    {
        ptMesh->sbtVertexNormals[i] = pl_norm_vec3(ptMesh->sbtVertexNormals[i]);
        const plVec3* ptNormal = &ptMesh->sbtVertexNormals[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].x = ptNormal->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].y = ptNormal->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].z = ptNormal->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride].w = 0.0f;
    }

    if(uVertexNormalCount > 0)
        uOffset += 1;

    // tangents
    const uint32_t uVertexTangentCount = pl_sb_size(ptMesh->sbtVertexTangents);
    for(uint32_t i = 0; i < uVertexTangentCount; i++)
    {
        const plVec4* ptTangent = &ptMesh->sbtVertexTangents[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTangent->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTangent->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptTangent->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptTangent->w;
    }

    if(uVertexTangentCount > 0)
        uOffset += 1;

    // texture coordinates 0
    const uint32_t uVertexTexCount = pl_sb_size(ptMesh->sbtVertexTextureCoordinates[0]);
    for(uint32_t i = 0; i < uVertexTexCount; i++)
    {
        const plVec2* ptTextureCoordinates = &(ptMesh->sbtVertexTextureCoordinates[0])[i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptTextureCoordinates->u;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptTextureCoordinates->v;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = 0.0f;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = 0.0f;

    }

    if(uVertexTexCount > 0)
        uOffset += 1;

    // color 0
    const uint32_t uVertexColorCount = pl_sb_size(ptMesh->sbtVertexColors[0]);
    for(uint32_t i = 0; i < uVertexColorCount; i++)
    {
        const plVec4* ptColor = &ptMesh->sbtVertexColors[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptColor->r;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptColor->g;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptColor->b;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptColor->a;
    }

    if(uVertexColorCount > 0)
        uOffset += 1;

    // joints 0
    const uint32_t uVertexJoint0Count = pl_sb_size(ptMesh->sbtVertexJoints[0]);
    for(uint32_t i = 0; i < uVertexJoint0Count; i++)
    {
        const plVec4* ptJoint = &ptMesh->sbtVertexJoints[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptJoint->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptJoint->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptJoint->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptJoint->w;
    }

    if(uVertexJoint0Count > 0)
        uOffset += 1;

    // weights 0
    const uint32_t uVertexWeights0Count = pl_sb_size(ptMesh->sbtVertexWeights[0]);
    for(uint32_t i = 0; i < uVertexWeights0Count; i++)
    {
        const plVec4* ptWeight = &ptMesh->sbtVertexWeights[0][i];
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].x = ptWeight->x;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].y = ptWeight->y;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].z = ptWeight->z;
        ptScene->sbtVertexDataBuffer[uVertexDataStartIndex + i * uStride + uOffset].w = ptWeight->w;
    }

    if(uVertexWeights0Count > 0)
        uOffset += 1;

    PL_ASSERT(uOffset == uStride && "sanity check");

    plMaterial tMaterial = {
        .tColor = ptMaterial->tBaseColor
    };
    pl_sb_push(ptScene->sbtMaterialBuffer, tMaterial);

    ptScene->sbtAllDrawables[uDrawableIndex].uIndexCount      = uIndexCount;
    ptScene->sbtAllDrawables[uDrawableIndex].uVertexCount     = uVertexCount;
    ptScene->sbtAllDrawables[uDrawableIndex].uIndexOffset     = uIndexBufferStart;
    ptScene->sbtAllDrawables[uDrawableIndex].uVertexOffset    = uVertexPosStartIndex;
    ptScene->sbtAllDrawables[uDrawableIndex].uDataOffset      = uVertexDataStartIndex;
    ptScene->sbtAllDrawables[uDrawableIndex].uMaterialIndex   = uMaterialIndex;
}

static void
pl_refr_finalize_scene(uint32_t uSceneHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);

    // update material bind groups
    const uint32_t uMaterialCount = pl_sb_size(ptScene->sbtMaterialEntities);
    for(uint32_t i = 0; i < uMaterialCount; i++)
    {
        plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptScene->sbtMaterialEntities[i]);

        plTextureViewHandle atMaterialTextureViews[] = {
            pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_BASE_COLOR_MAP, true, 0),
            pl__create_texture_helper(ptMaterial, PL_TEXTURE_SLOT_NORMAL_MAP, false, 0)
        };
        gptDevice->update_bind_group(ptDevice, &ptScene->sbtMaterialBindGroups[i], 0, NULL, NULL, 2, atMaterialTextureViews);
    }

    // add opaque items
    const uint32_t uOpaqueDrawableCount = pl_sb_size(ptScene->sbtOpaqueDrawables);
    for(uint32_t i = 0; i < uOpaqueDrawableCount; i++)
        pl_sb_push(ptScene->sbtAllDrawables, ptScene->sbtOpaqueDrawables[i]);

    // add transparent items
    const uint32_t uTransparentDrawableCount = pl_sb_size(ptScene->sbtTransparentDrawables);
    for(uint32_t i = 0; i < uTransparentDrawableCount; i++)
        pl_sb_push(ptScene->sbtAllDrawables, ptScene->sbtTransparentDrawables[i]);

    // fill CPU buffers & drawable list
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtAllDrawables);
    for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
    {
        ptScene->sbtAllDrawables[uDrawableIndex].uSkinIndex = UINT32_MAX;
        plEntity tEntity = ptScene->sbtAllDrawables[uDrawableIndex].tEntity;

        // get actual components
        plObjectComponent*   ptObject   = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tEntity);
        plMeshComponent*     ptMesh     = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptObject->tMesh);
        plMaterialComponent* ptMaterial = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MATERIAL, ptMesh->tMaterial);

        // add data to global buffers
        pl__add_drawable_data_to_global_buffer(ptScene, uDrawableIndex);

        // choose shader variant
        int aiConstantData0[5] = {0};
        aiConstantData0[0] = (int)ptMesh->ulVertexStreamMask;
        aiConstantData0[2] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_BASE_COLOR_MAP].acName[0] != 0); // PL_HAS_BASE_COLOR_MAP;
        aiConstantData0[3] = (int)(ptMaterial->atTextureMaps[PL_TEXTURE_SLOT_NORMAL_MAP].acName[0] != 0); // PL_HAS_NORMAL_MAP
        aiConstantData0[4] = (int)(ptMesh->tSkinComponent.uIndex != UINT32_MAX);
        int iFlagCopy0 = (int)ptMesh->ulVertexStreamMask;
        while(iFlagCopy0)
        {
            aiConstantData0[1] += iFlagCopy0 & 1;
            iFlagCopy0 >>= 1;
        }

        const plShaderVariant tVariant = {
            .pTempConstantData = aiConstantData0,
            .tGraphicsState = {
                .ulDepthWriteEnabled  = 1,
                .ulVertexStreamMask   = PL_MESH_FORMAT_FLAG_HAS_POSITION,
                .ulBlendMode          = ptMaterial->tBlendMode == PL_MATERIAL_BLEND_MODE_OPAQUE ? PL_BLEND_MODE_NONE : PL_BLEND_MODE_ALPHA,
                .ulDepthMode          = PL_COMPARE_MODE_LESS_OR_EQUAL,
                .ulCullMode           = ptMaterial->tBlendMode == PL_MATERIAL_BLEND_MODE_OPAQUE ? PL_CULL_MODE_CULL_BACK : PL_CULL_MODE_NONE,
                .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
                .ulStencilRef         = 0xff,
                .ulStencilMask        = 0xff,
                .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
                .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
                .ulStencilOpPass      = PL_STENCIL_OP_KEEP
            }
        };

        ptScene->sbtAllDrawables[uDrawableIndex].uShader = gptDevice->get_shader_variant(ptDevice, ptScene->tShader, &tVariant).uIndex;

        if(ptMesh->tSkinComponent.uIndex != UINT32_MAX)
        {

            plSkinData tSkinData = {.tEntity = ptMesh->tSkinComponent};

            plSkinComponent* ptSkinComponent = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptMesh->tSkinComponent);
            unsigned int textureWidth = (unsigned int)ceilf(sqrtf((float)(pl_sb_size(ptSkinComponent->sbtJoints) * 8)));
            pl_sb_resize(ptSkinComponent->sbtTextureData, textureWidth * textureWidth);
            plTextureDesc tTextureDesc = {
                .tDimensions = {(float)textureWidth, (float)textureWidth, 1},
                .tFormat = PL_FORMAT_R32G32B32A32_FLOAT,
                .uLayers = 1,
                .uMips = 1,
                .tType = PL_TEXTURE_TYPE_2D,
                .tUsage = PL_TEXTURE_USAGE_SAMPLED
            };

            tSkinData.tDynamicTexture[0] = gptDevice->create_texture(ptDevice, tTextureDesc, "joint texture");
            tSkinData.tDynamicTexture[1] = gptDevice->create_texture(ptDevice, tTextureDesc, "joint texture");

            plBufferImageCopy tBufferImageCopy = {
                .tImageExtent = {textureWidth, textureWidth, 1},
                .uLayerCount = 1
            };
            memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * textureWidth * textureWidth);
            gptDevice->copy_buffer_to_texture(ptDevice, gptData->tStagingBufferHandle, tSkinData.tDynamicTexture[0], 1, &tBufferImageCopy);
            gptDevice->copy_buffer_to_texture(ptDevice, gptData->tStagingBufferHandle, tSkinData.tDynamicTexture[1], 1, &tBufferImageCopy);

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
                .tVerticalWrap = PL_WRAP_MODE_WRAP,
                .tHorizontalWrap = PL_WRAP_MODE_WRAP
            };
            tSkinData.tDynamicTextureView[0] = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tSkinData.tDynamicTexture[0], "joint texture view 0");
            tSkinData.tDynamicTextureView[1] = gptDevice->create_texture_view(ptDevice, &tTextureViewDesc, &tSampler, tSkinData.tDynamicTexture[1], "joint texture view 1");

            ptScene->sbtAllDrawables[uDrawableIndex].uSkinIndex = pl_sb_size(ptScene->sbtSkinData);
            pl_sb_push(ptScene->sbtSkinData, tSkinData);
        }
    }

    // create GPU buffers

    const plBufferDescription tShaderBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtMaterialBuffer, sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer));
    ptScene->tMaterialDataBuffer = gptDevice->create_buffer(ptDevice, &tShaderBufferDesc, "shader buffer");
    gptDevice->copy_buffer(ptDevice, gptData->tStagingBufferHandle, ptScene->tMaterialDataBuffer, 0, 0, tShaderBufferDesc.uByteSize);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);

    const plBufferDescription tIndexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_INDEX,
        .uByteSize            = sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbuIndexBuffer, sizeof(uint32_t) * pl_sb_size(ptScene->sbuIndexBuffer));
    ptScene->tIndexBuffer = gptDevice->create_buffer(ptDevice, &tIndexBufferDesc, "index buffer");
    gptDevice->copy_buffer(ptDevice, gptData->tStagingBufferHandle, ptScene->tIndexBuffer, 0, 0, tIndexBufferDesc.uByteSize);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);

    const plBufferDescription tVertexBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_VERTEX,
        .uByteSize            = sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtVertexPosBuffer, sizeof(plVec3) * pl_sb_size(ptScene->sbtVertexPosBuffer));
    ptScene->tVertexBuffer = gptDevice->create_buffer(ptDevice, &tVertexBufferDesc, "vertex buffer");
    gptDevice->copy_buffer(ptDevice, gptData->tStagingBufferHandle, ptScene->tVertexBuffer, 0, 0, tVertexBufferDesc.uByteSize);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);

    const plBufferDescription tStorageBufferDesc = {
        .tMemory              = PL_MEMORY_GPU,
        .tUsage               = PL_BUFFER_USAGE_STORAGE,
        .uByteSize            = sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer)
    };
    memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptScene->sbtVertexDataBuffer, sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer));
    ptScene->tStorageBuffer = gptDevice->create_buffer(ptDevice, &tStorageBufferDesc, "storage buffer");
    gptDevice->copy_buffer(ptDevice, gptData->tStagingBufferHandle, ptScene->tStorageBuffer, 0, 0, tStorageBufferDesc.uByteSize);
    ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);
}

static void
pl_refr_run_ecs(void)
{
    pl_begin_profile_sample(__FUNCTION__);
    gptECS->run_animation_update_system(&gptData->tComponentLibrary, pl_get_io()->fDeltaTime);
    gptECS->run_transform_update_system(&gptData->tComponentLibrary);
    gptECS->run_hierarchy_update_system(&gptData->tComponentLibrary);
    gptECS->run_inverse_kinematics_update_system(&gptData->tComponentLibrary);
    gptECS->run_skin_update_system(&gptData->tComponentLibrary);
    gptECS->run_object_update_system(&gptData->tComponentLibrary);
    pl_end_profile_sample();
}

static bool
pl__sat_visibility_test(plCameraComponent* ptCamera, const plAABB* ptAABB)
{
    const float fTanFov = tanf(0.5f * ptCamera->fFieldOfView);

    const float fZNear = ptCamera->fNearZ;
    const float fZFar = ptCamera->fFarZ;

    // half width, half height
    const float fXNear = ptCamera->fAspectRatio * ptCamera->fNearZ * fTanFov;
    const float fYNear = ptCamera->fNearZ * fTanFov;

    // consider four adjacent corners of the AABB
    plVec3 atCorners[] = {
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMax.x, ptAABB->tMin.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMax.y, ptAABB->tMin.z},
        {ptAABB->tMin.x, ptAABB->tMin.y, ptAABB->tMax.z},
    };

    // transform corners
    for (size_t i = 0; i < 4; i++)
        atCorners[i] = pl_mul_mat4_vec3(&ptCamera->tViewMat, atCorners[i]);

    // Use transformed atCorners to calculate center, axes and extents
    plOBB tObb = {
        .atAxes = {
            pl_sub_vec3(atCorners[1], atCorners[0]),
            pl_sub_vec3(atCorners[2], atCorners[0]),
            pl_sub_vec3(atCorners[3], atCorners[0])
        },
    };

    tObb.tCenter = pl_add_vec3(atCorners[0], pl_mul_vec3_scalarf((pl_add_vec3(tObb.atAxes[0], pl_add_vec3(tObb.atAxes[1], tObb.atAxes[2]))), 0.5f));
    tObb.tExtents = (plVec3){ pl_length_vec3(tObb.atAxes[0]), pl_length_vec3(tObb.atAxes[1]), pl_length_vec3(tObb.atAxes[2]) };

    // normalize
    tObb.atAxes[0] = pl_div_vec3_scalarf(tObb.atAxes[0], tObb.tExtents.x);
    tObb.atAxes[1] = pl_div_vec3_scalarf(tObb.atAxes[1], tObb.tExtents.y);
    tObb.atAxes[2] = pl_div_vec3_scalarf(tObb.atAxes[2], tObb.tExtents.z);
    tObb.tExtents = pl_mul_vec3_scalarf(tObb.tExtents, 0.5f);

    // axis along frustum
    {
        // Projected center of our OBB
        const float fMoC = tObb.tCenter.z;

        // Projected size of OBB
        float fRadius = 0.0f;
        for (size_t i = 0; i < 3; i++)
            fRadius += fabsf(tObb.atAxes[i].z) * tObb.tExtents.d[i];

        const float fObbMin = fMoC - fRadius;
        const float fObbMax = fMoC + fRadius;

        if (fObbMin > fZFar || fObbMax < fZNear)
            return false;
    }


    // other normals of frustum
    {
        const plVec3 atM[] = {
            { fZNear, 0.0f, fXNear }, // Left Plane
            { -fZNear, 0.0f, fXNear }, // Right plane
            { 0.0, -fZNear, fYNear }, // Top plane
            { 0.0, fZNear, fYNear }, // Bottom plane
        };
        for (size_t m = 0; m < 4; m++)
        {
            const float fMoX = fabsf(atM[m].x);
            const float fMoY = fabsf(atM[m].y);
            const float fMoZ = atM[m].z;
            const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            const float fP = fXNear * fMoX + fYNear * fMoY;

            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // OBB axes
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3* ptM = &tObb.atAxes[m];
            const float fMoX = fabsf(ptM->x);
            const float fMoY = fabsf(ptM->y);
            const float fMoZ = ptM->z;
            const float fMoC = pl_dot_vec3(*ptM, tObb.tCenter);

            const float fObbRadius = tObb.tExtents.d[m];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // cross products between the edges
    // first R x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { 0.0f, -tObb.atAxes[m].z, tObb.atAxes[m].y };
            const float fMoX = 0.0f;
            const float fMoY = fabsf(tM.y);
            const float fMoZ = tM.z;
            const float fMoC = tM.y * tObb.tCenter.y + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // U x A_i
    {
        for (size_t m = 0; m < 3; m++)
        {
            const plVec3 tM = { tObb.atAxes[m].z, 0.0f, -tObb.atAxes[m].x };
            const float fMoX = fabsf(tM.x);
            const float fMoY = 0.0f;
            const float fMoZ = tM.z;
            const float fMoC = tM.x * tObb.tCenter.x + tM.z * tObb.tCenter.z;

            float fObbRadius = 0.0f;
            for (size_t i = 0; i < 3; i++)
                fObbRadius += fabsf(pl_dot_vec3(tM, tObb.atAxes[i])) * tObb.tExtents.d[i];

            const float fObbMin = fMoC - fObbRadius;
            const float fObbMax = fMoC + fObbRadius;

            // frustum projection
            const float fP = fXNear * fMoX + fYNear * fMoY;
            float fTau0 = fZNear * fMoZ - fP;
            float fTau1 = fZNear * fMoZ + fP;

            if (fTau0 < 0.0f)
                fTau0 *= fZFar / fZNear;

            if (fTau1 > 0.0f)
                fTau1 *= fZFar / fZNear;

            if (fObbMin > fTau1 || fObbMax < fTau0)
                return false;
        }
    }

    // frustum Edges X Ai
    {
        for (size_t obb_edge_idx = 0; obb_edge_idx < 3; obb_edge_idx++)
        {
            const plVec3 atM[] = {
                pl_cross_vec3((plVec3){-fXNear, 0.0f, fZNear}, tObb.atAxes[obb_edge_idx]), // Left Plane
                pl_cross_vec3((plVec3){ fXNear, 0.0f, fZNear }, tObb.atAxes[obb_edge_idx]), // Right plane
                pl_cross_vec3((plVec3){ 0.0f, fYNear, fZNear }, tObb.atAxes[obb_edge_idx]), // Top plane
                pl_cross_vec3((plVec3){ 0.0, -fYNear, fZNear }, tObb.atAxes[obb_edge_idx]) // Bottom plane
            };

            for (size_t m = 0; m < 4; m++)
            {
                const float fMoX = fabsf(atM[m].x);
                const float fMoY = fabsf(atM[m].y);
                const float fMoZ = atM[m].z;

                const float fEpsilon = 1e-4f;
                if (fMoX < fEpsilon && fMoY < fEpsilon && fabsf(fMoZ) < fEpsilon) continue;

                const float fMoC = pl_dot_vec3(atM[m], tObb.tCenter);

                float fObbRadius = 0.0f;
                for (size_t i = 0; i < 3; i++)
                    fObbRadius += fabsf(pl_dot_vec3(atM[m], tObb.atAxes[i])) * tObb.tExtents.d[i];

                const float fObbMin = fMoC - fObbRadius;
                const float fObbMax = fMoC + fObbRadius;

                // frustum projection
                const float fP = fXNear * fMoX + fYNear * fMoY;
                float fTau0 = fZNear * fMoZ - fP;
                float fTau1 = fZNear * fMoZ + fP;

                if (fTau0 < 0.0f)
                    fTau0 *= fZFar / fZNear;

                if (fTau1 > 0.0f)
                    fTau1 *= fZFar / fZNear;

                if (fObbMin > fTau1 || fObbMax < fTau0)
                    return false;
            }
        }
    }

    // no intersections detected
    return true;
}

static void
pl_refr_update_scene(uint32_t uSceneHandle)
{
    pl_begin_profile_sample(__FUNCTION__);
    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];

    // update skin textures
    const uint32_t uSkinCount = pl_sb_size(ptScene->sbtSkinData);
    for(uint32_t i = 0; i < uSkinCount; i++)
    {
        plBindGroupLayout tBindGroupLayout1 = {
            .uTextureCount  = 1,
            .aTextures = {
                {.uSlot =  0, .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL}
            }
        };
        ptScene->sbtSkinData[i].tTempBindGroup = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout1);
        gptDevice->update_bind_group(&ptGraphics->tDevice, &ptScene->sbtSkinData[i].tTempBindGroup, 0, NULL, NULL, 1, &ptScene->sbtSkinData[i].tDynamicTextureView[ptGraphics->uCurrentFrameIndex]);

        plBuffer* ptStagingBuffer = gptDevice->get_buffer(ptDevice, gptData->tStagingBufferHandle);

        plTexture* ptSkinTexture = gptDevice->get_texture(ptDevice, ptScene->sbtSkinData[i].tDynamicTexture[0]);
        plBufferImageCopy tBufferImageCopy = {
            .tImageExtent = {(size_t)ptSkinTexture->tDesc.tDimensions.x, (size_t)ptSkinTexture->tDesc.tDimensions.y, 1},
            .uLayerCount = 1
        };
        
        plSkinComponent* ptSkinComponent = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_SKIN, ptScene->sbtSkinData[i].tEntity);
        memcpy(ptStagingBuffer->tMemoryAllocation.pHostMapped, ptSkinComponent->sbtTextureData, sizeof(float) * 4 * (size_t)ptSkinTexture->tDesc.tDimensions.x * (size_t)ptSkinTexture->tDesc.tDimensions.y);
        gptDevice->copy_buffer_to_texture(ptDevice, gptData->tStagingBufferHandle, ptScene->sbtSkinData[i].tDynamicTexture[ptGraphics->uCurrentFrameIndex], 1, &tBufferImageCopy);
    }
    pl_end_profile_sample();
}

static void
pl_refr_render_scene(plCommandBuffer tCommandBuffer, uint32_t uSceneHandle, uint32_t uViewHandle, plViewOptions tOptions)
{
    pl_begin_profile_sample(__FUNCTION__);

    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->sbtViews[uViewHandle];
    plCameraComponent* ptCamera = tOptions.ptViewCamera;

    // handle culling
    
    const uint32_t uDrawableCount = pl_sb_size(ptScene->sbtAllDrawables);
    pl_begin_profile_sample("cull operations");
    if(tOptions.ptCullCamera)
    {
        pl_sb_reset(ptView->sbtVisibleDrawables);
        for(uint32_t uDrawableIndex = 0; uDrawableIndex < uDrawableCount; uDrawableIndex++)
        {
            const plDrawable tDrawable = ptScene->sbtAllDrawables[uDrawableIndex];
            plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, tDrawable.tEntity);

            if(pl__sat_visibility_test(tOptions.ptCullCamera, &ptMesh->tAABBFinal))
            {
                pl_sb_push(ptView->sbtVisibleDrawables, tDrawable);
            }
        }
        
    }
    else if(pl_sb_size(ptView->sbtVisibleDrawables) != uDrawableCount)
    {
        pl_sb_resize(ptView->sbtVisibleDrawables, uDrawableCount);
        memcpy(ptView->sbtVisibleDrawables, ptScene->sbtAllDrawables, sizeof(plDrawable) * uDrawableCount);
    }
    pl_end_profile_sample();

    plGraphics* ptGraphics = &gptData->tGraphics;
    plDevice* ptDevice = &ptGraphics->tDevice;

    plDrawStream* ptStream = &gptData->tDrawStream;

    // update global buffers & bind groups
    const BindGroup_0 tBindGroupBuffer = {
        .tCameraPos            = ptCamera->tPos,
        .tCameraProjection     = ptCamera->tProjMat,
        .tCameraView           = ptCamera->tViewMat,
        .tCameraViewProjection = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat)
    };
    memcpy(ptGraphics->sbtBuffersCold[ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex].uIndex].tMemoryAllocation.pHostMapped, &tBindGroupBuffer, sizeof(BindGroup_0));

    plBindGroupLayout tBindGroupLayout0 = {
        .uBufferCount  = 3,
        .aBuffers = {
            {
                .tType = PL_BUFFER_BINDING_TYPE_UNIFORM,
                .uSlot = 0,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 1,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
            {
                .tType = PL_BUFFER_BINDING_TYPE_STORAGE,
                .uSlot = 2,
                .tStages = PL_STAGE_VERTEX | PL_STAGE_PIXEL
            },
        }
    };
    plBindGroupHandle tGlobalBG = gptDevice->get_temporary_bind_group(ptDevice, &tBindGroupLayout0);
    size_t szBufferRangeSize[] = {sizeof(BindGroup_0), sizeof(plVec4) * pl_sb_size(ptScene->sbtVertexDataBuffer), sizeof(plMaterial) * pl_sb_size(ptScene->sbtMaterialBuffer)};

    plBufferHandle atBindGroup0_buffers0[] = {ptView->atGlobalBuffers[ptGraphics->uCurrentFrameIndex], ptScene->tStorageBuffer, ptScene->tMaterialDataBuffer};
    gptDevice->update_bind_group(&ptGraphics->tDevice, &tGlobalBG, 3, atBindGroup0_buffers0, szBufferRangeSize, 0, NULL);

    gptStream->reset(ptStream);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~skybox~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    if(ptScene->tSkyboxTexture.uIndex != UINT32_MAX)
    {

        plDynamicBinding tSkyboxDynamicData = gptDevice->allocate_dynamic_data(ptDevice, sizeof(plMat4));
        plMat4* ptSkyboxDynamicData = (plMat4*)tSkyboxDynamicData.pcData;
        *ptSkyboxDynamicData = pl_mat4_translate_vec3(ptCamera->tPos);

        gptStream->draw(ptStream, (plDraw)
        {
            .uShaderVariant       = gptData->tSkyboxShader.uIndex,
            .uDynamicBuffer       = tSkyboxDynamicData.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = ptScene->tSkyboxDrawable.uIndexOffset,
            .uTriangleCount       = ptScene->tSkyboxDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = ptScene->tSkyboxBindGroup.uIndex,
            .uBindGroup2          = gptData->tNullSkinBindgroup.uIndex,
            .uDynamicBufferOffset = tSkyboxDynamicData.uByteOffset
        });
    }

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~visible meshes~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~    

    static double* pdVisibleObjects = NULL;
    if(!pdVisibleObjects)
        pdVisibleObjects = gptStats->get_counter("visible objects");

    const uint32_t uVisibleDrawCount = pl_sb_size(ptView->sbtVisibleDrawables);

    if(tOptions.bCullStats)
        *pdVisibleObjects = (double)uVisibleDrawCount;

    for(uint32_t i = 0; i < uVisibleDrawCount; i++)
    {
        const plDrawable tDrawable = ptView->sbtVisibleDrawables[i];
        plObjectComponent* ptObject = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_OBJECT, tDrawable.tEntity);
        plTransformComponent* ptTransform = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_TRANSFORM, ptObject->tTransform);
        
        plDynamicBinding tDynamicBinding = gptDevice->allocate_dynamic_data(ptDevice, sizeof(DynamicData));

        DynamicData* ptDynamicData = (DynamicData*)tDynamicBinding.pcData;
        ptDynamicData->iDataOffset = tDrawable.uDataOffset;
        ptDynamicData->iVertexOffset = tDrawable.uVertexOffset;
        ptDynamicData->tModel = ptTransform->tWorld;
        ptDynamicData->iMaterialOffset = tDrawable.uMaterialIndex;

        gptStream->draw(ptStream, (plDraw)
        {
            .uShaderVariant       = tDrawable.uShader,
            .uDynamicBuffer       = tDynamicBinding.uBufferHandle,
            .uVertexBuffer        = ptScene->tVertexBuffer.uIndex,
            .uIndexBuffer         = tDrawable.uIndexCount == 0 ? UINT32_MAX : ptScene->tIndexBuffer.uIndex,
            .uIndexOffset         = tDrawable.uIndexOffset,
            .uTriangleCount       = tDrawable.uIndexCount == 0 ? tDrawable.uVertexCount / 3 : tDrawable.uIndexCount / 3,
            .uBindGroup0          = tGlobalBG.uIndex,
            .uBindGroup1          = tDrawable.tMaterialBindGroup.uIndex,
            .uBindGroup2          = tDrawable.uSkinIndex == UINT32_MAX ? gptData->tNullSkinBindgroup.uIndex : ptScene->sbtSkinData[tDrawable.uSkinIndex].tTempBindGroup.uIndex,
            .uDynamicBufferOffset = tDynamicBinding.uByteOffset,
        });
    }

    const plVec2 tDimensions = ptGraphics->sbtRenderPassesCold[ptView->tRenderPass.uIndex].tDesc.tDimensions;

    plDrawArea tArea = {
       .ptDrawStream = ptStream,
       .tScissor = {
            .uWidth  = (uint32_t)tDimensions.x,
            .uHeight = (uint32_t)tDimensions.y,
       },
       .tViewport = {
            .fWidth  = tDimensions.x,
            .fHeight = tDimensions.y,
            .fMaxDepth = 1.0f
       }
    };

    plPassRenderer tPassRenderer = gptGfx->begin_render_pass(ptGraphics, tCommandBuffer, ptView->tRenderPass);
    gptGfx->draw_subpass(ptGraphics, tCommandBuffer, tPassRenderer, 1, &tArea);

    if(tOptions.bShowAllBoundingBoxes)
    {
        for(uint32_t i = 0; i < uDrawableCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptScene->sbtAllDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.01f);
        }
    }
    else if(tOptions.bShowVisibleBoundingBoxes)
    {
        for(uint32_t i = 0; i < uVisibleDrawCount; i++)
        {
            plMeshComponent* ptMesh = gptECS->get_component(&gptData->tComponentLibrary, PL_COMPONENT_TYPE_MESH, ptView->sbtVisibleDrawables[i].tEntity);

            gptGfx->add_3d_aabb(&ptView->t3DDrawList, ptMesh->tAABBFinal.tMin, ptMesh->tAABBFinal.tMax, (plVec4){1.0f, 0.0f, 0.0f, 1.0f}, 0.01f);
        }
    }

    if(tOptions.bShowOrigin)
    {
        const plMat4 tTransform = pl_identity_mat4();
        gptGfx->add_3d_transform(&ptView->t3DDrawList, &tTransform, 10.0f, 0.02f);
    }

    if(tOptions.ptCullCamera && tOptions.ptCullCamera != tOptions.ptViewCamera)
    {
        gptGfx->add_3d_frustum(&ptView->t3DDrawList, &tOptions.ptCullCamera->tTransformMat, tOptions.ptCullCamera->fFieldOfView, tOptions.ptCullCamera->fAspectRatio, tOptions.ptCullCamera->fNearZ, tOptions.ptCullCamera->fFarZ, (plVec4){1.0f, 1.0f, 0.0f, 1.0f}, 0.02f);
    }

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);
    gptGfx->submit_3d_drawlist(&ptView->t3DDrawList, tPassRenderer, tCommandBuffer, tDimensions.x, tDimensions.y, &tMVP, PL_PIPELINE_FLAG_DEPTH_TEST | PL_PIPELINE_FLAG_DEPTH_WRITE, 1);

    gptGfx->end_render_pass(ptGraphics, tCommandBuffer, tPassRenderer);
    pl_end_profile_sample();
}

static plTextureId
pl_refr_get_view_texture_id(uint32_t uSceneHandle, uint32_t uViewHandle)
{
    plRefScene* ptScene = &gptData->sbtScenes[uSceneHandle];
    plRefView* ptView = &ptScene->sbtViews[uViewHandle];
    return ptView->tTextureID;
}

//-----------------------------------------------------------------------------
// [SECTION] extension loading
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_load_ext(plApiRegistryApiI* ptApiRegistry, bool bReload)
{
   gptDataRegistry = ptApiRegistry->first(PL_API_DATA_REGISTRY);
   pl_set_memory_context(gptDataRegistry->get_data(PL_CONTEXT_MEMORY));
   pl_set_profile_context(gptDataRegistry->get_data("profile"));
   pl_set_log_context(gptDataRegistry->get_data("log"));
   pl_set_context(gptDataRegistry->get_data("ui"));

   // apis
   gptResource = ptApiRegistry->first(PL_API_RESOURCE);
   gptECS      = ptApiRegistry->first(PL_API_ECS);
   gptFile     = ptApiRegistry->first(PL_API_FILE);
   gptDevice   = ptApiRegistry->first(PL_API_DEVICE);
   gptGfx      = ptApiRegistry->first(PL_API_GRAPHICS);
   gptCamera   = ptApiRegistry->first(PL_API_CAMERA);
   gptStream   = ptApiRegistry->first(PL_API_DRAW_STREAM);
   gptImage    = ptApiRegistry->first(PL_API_IMAGE);
   gptStats    = ptApiRegistry->first(PL_API_STATS);

   if(bReload)
   {
      gptData = gptDataRegistry->get_data("ref renderer data");
      ptApiRegistry->replace(ptApiRegistry->first(PL_API_REF_RENDERER), pl_load_ref_renderer_api());
   }
   else
   {
      // allocate renderer data
      gptData = PL_ALLOC(sizeof(plRefRendererData));
      memset(gptData, 0, sizeof(plRefRendererData));

      // register data with registry (for reloads)
      gptDataRegistry->set_data("ref renderer data", gptData);

      // add specific log channel for renderer
      gptData->uLogChannel = pl_add_log_channel("Renderer", PL_CHANNEL_TYPE_BUFFER);

      // register API
      ptApiRegistry->add(PL_API_REF_RENDERER, pl_load_ref_renderer_api());
   }
}

PL_EXPORT void
pl_unload_ext(plApiRegistryApiI* ptApiRegistry)
{
}

//-----------------------------------------------------------------------------
// [SECTION] unity build
//-----------------------------------------------------------------------------

#define PL_STL_IMPLEMENTATION
#include "pl_stl.h"
#undef PL_STL_IMPLEMENTATION

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#undef CGLTF_IMPLEMENTATION