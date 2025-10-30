/*
   example_gfx_3.c
     - demonstrates loading APIs
     - demonstrates loading extensions
     - demonstrates hot reloading
     - demonstrates starter extension
     - demonstrates drawing extension (2D & 3D)
*/

/*
Index of this file:
// [SECTION] includes
// [SECTION] structs
// [SECTION] apis
// [SECTION] helper function declarations
// [SECTION] pl_app_load
// [SECTION] pl_app_shutdown
// [SECTION] pl_app_resize
// [SECTION] pl_app_update
// [SECTION] helper function definitions
*/

//-----------------------------------------------------------------------------
// [SECTION] includes
//-----------------------------------------------------------------------------

#include <string.h> // memset
#include "pl.h"

#define PL_MATH_INCLUDE_FUNCTIONS
#include "pl_math.h"

#include "pl_ds.h"

// extensions
#include "pl_graphics_ext.h"
#include "pl_draw_ext.h"
#include "pl_draw_backend_ext.h"
#include "pl_starter_ext.h"

// globe stuff
#include "pl_mesh_ext.h"
#include "pl_shader_ext.h"
#include "pl_vfs_ext.h"
#include "pl_ui_ext.h"

//-----------------------------------------------------------------------------
// [SECTION] structs
//-----------------------------------------------------------------------------

typedef struct _plCamera
{
    plVec3 tPos;
    float  fNearZ;
    float  fFarZ;
    float  fFieldOfView;
    float  fAspectRatio;  // width/height
    plMat4 tViewMat;      // cached
    plMat4 tProjMat;      // cached
    plMat4 tTransformMat; // cached

    // rotations
    float fPitch; // rotation about right vector
    float fYaw;   // rotation about up vector
    float fRoll;  // rotation about forward vector

    // direction vectors
    plVec3 _tUpVec;
    plVec3 _tForwardVec;
    plVec3 _tRightVec;
} plCamera;

typedef struct _plAppData
{
    // window
    plWindow* ptWindow;

    // 3d drawing
    plCamera      tCamera;
    plDrawList3D* pt3dDrawlist;

    // globe testing
    uint32_t       uGlobeIndexCount;
    uint32_t       uGlobeVertexCount;
    plDVec3*       ptGlobeVertexBuffer;
    plVec3*        ptGlobeVertexBuffer0;
    plVec3*        ptGlobeVertexBuffer1;
    uint32_t*      puGlobeIndexBuffer;
    plBufferHandle tGlobeIndexBuffer;
    plBufferHandle tGlobeVertexBuffer0;
    plBufferHandle tGlobeVertexBuffer1;
    plShaderHandle tGlobeShader;
    plShaderHandle tGlobeWireframeShader;

    // globe options
    bool bWireframe;
} plAppData;

typedef struct _plTesselationTriangle
{
    plVec3   atPoints[3];
    uint32_t uDivisionLevel;
} plTesselationTriangle;

//-----------------------------------------------------------------------------
// [SECTION] apis
//-----------------------------------------------------------------------------

const plIOI*            gptIO            = NULL;
const plWindowI*        gptWindows       = NULL;
const plGraphicsI*      gptGfx           = NULL;
const plDrawI*          gptDraw          = NULL;
const plDrawBackendI*   gptDrawBackend   = NULL;
const plStarterI*       gptStarter       = NULL;
const plMeshBuilderI*   gptMeshBuilder   = NULL;
const plShaderI*        gptShader        = NULL;
const plVfsI*           gptVfs           = NULL;
const plUiI*            gptUi            = NULL;

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

// camera helpers
void camera_translate(plCamera*, float fDx, float fDy, float fDz);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_rotate   (plCamera*, float fDPitch, float fDYaw);
void camera_update   (plCamera*);

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

    // if "ptAppData" is a valid pointer, then this function is being called
    // during a hot reload.
    if(ptAppData)
    {
        // re-retrieve the apis since we are now in
        // a different dll/so
        gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
        gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);
        
        gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
        gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
        gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
        gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
        gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
        gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
        gptMeshBuilder = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
        gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);

        return ptAppData;
    }

    // this path is taken only during first load, so we
    // allocate app memory here
    ptAppData = malloc(sizeof(plAppData));
    memset(ptAppData, 0, sizeof(plAppData));

    ptAppData->bWireframe = true;

    // retrieve extension registry
    const plExtensionRegistryI* ptExtensionRegistry = pl_get_api_latest(ptApiRegistry, plExtensionRegistryI);

    // load extensions (makes their APIs available)
    ptExtensionRegistry->load("pl_unity_ext", NULL, NULL, true);
    ptExtensionRegistry->load("pl_platform_ext", NULL, NULL, false);
    
    // load required apis
    gptIO      = pl_get_api_latest(ptApiRegistry, plIOI);
    gptWindows = pl_get_api_latest(ptApiRegistry, plWindowI);

    // load required apis (these are provided though extensions)
    gptGfx         = pl_get_api_latest(ptApiRegistry, plGraphicsI);
    gptDraw        = pl_get_api_latest(ptApiRegistry, plDrawI);
    gptDrawBackend = pl_get_api_latest(ptApiRegistry, plDrawBackendI);
    gptStarter     = pl_get_api_latest(ptApiRegistry, plStarterI);
    gptShader      = pl_get_api_latest(ptApiRegistry, plShaderI);
    gptVfs         = pl_get_api_latest(ptApiRegistry, plVfsI);
    gptMeshBuilder = pl_get_api_latest(ptApiRegistry, plMeshBuilderI);
    gptUi          = pl_get_api_latest(ptApiRegistry, plUiI);


    // use window API to create a window
    plWindowDesc tWindowDesc = {
        .pcTitle = "Example GFX 3",
        .iXPos   = 200,
        .iYPos   = 200,
        .uWidth  = 600,
        .uHeight = 600,
    };
    gptWindows->create(tWindowDesc, &ptAppData->ptWindow);
    gptWindows->show(ptAppData->ptWindow);

    plStarterInit tStarterInit = {
        .tFlags   = PL_STARTER_FLAGS_ALL_EXTENSIONS,
        .ptWindow = ptAppData->ptWindow
    };

    // we want the starter extension to include a depth buffer
    // when setting up the render pass
    tStarterInit.tFlags |= PL_STARTER_FLAGS_DEPTH_BUFFER;
    tStarterInit.tFlags &= ~PL_STARTER_FLAGS_SHADER_EXT;

    // from a graphics standpoint, the starter extension is handling device, swapchain, renderpass
    // etc. which we will get to in later examples
    gptStarter->initialize(tStarterInit);

    // initialize shader extension (we are doing this ourselves so we can add additional shader directories)
    static const plShaderOptions tDefaultShaderOptions = {
        .apcIncludeDirectories = {
            "../examples/shaders/"
        },
        .apcDirectories = {
            "../shaders/",
            "../examples/shaders/"
        },
        .tFlags = PL_SHADER_FLAGS_AUTO_OUTPUT | PL_SHADER_FLAGS_NEVER_CACHE
    };
    gptShader->initialize(&tDefaultShaderOptions);

    // give starter extension chance to do its work now that we
    // setup the shader extension
    gptStarter->finalize();

    // request 3d drawlists
    ptAppData->pt3dDrawlist = gptDraw->request_3d_drawlist();

    // create camera
    ptAppData->tCamera = (plCamera){
        .tPos         = {1737000.0, 0.0f, 0.0f},
        .fNearZ       = 0.01f,
        .fFarZ        = 50.0f,
        .fFieldOfView = PL_PI_3,
        .fAspectRatio = 1.0f,
        .fYaw         = PL_PI + PL_PI_4,
        .fPitch       = -PL_PI_4,
    };
    camera_update(&ptAppData->tCamera);

    plDevice* ptDevice = gptStarter->get_device();

    uint32_t uSubdivisions = 4;

    plMeshBuilderOptions tOptions = {0};
    plMeshBuilder* ptBuilder = gptMeshBuilder->create(tOptions);

    plVec3 tP0 = (plVec3){ 0.0f, 1.0f, 0.0f};
    plVec3 tP1 = (plVec3){ 0.0f, -1.0f / 3.0f, 2.0f * sqrtf(2.0f) / 3.0f};
    plVec3 tP2 = (plVec3){-sqrtf(6.0f) / 3.0f, -1.0f / 3.0f, -sqrtf(2.0f) / 3.0f};
    plVec3 tP3 = (plVec3){ sqrtf(6.0f) / 3.0f, -1.0f / 3.0f, -sqrtf(2.0f) / 3.0f};

    // plVec3 tRadi = {4.0f, 3.0f, 4.0f};
    plVec3 tRadi = {1737000.0, 1737000.0, 1737000.0};

    plTesselationTriangle* sbtTessTris = NULL;
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP1, tP2}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP3, tP1}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP0, tP2, tP3}}));
    pl_sb_push(sbtTessTris, ((plTesselationTriangle){.atPoints = {tP1, tP3, tP2}}));

    while(pl_sb_size(sbtTessTris) > 0)
    {
        plTesselationTriangle tTri = pl_sb_pop(sbtTessTris);

        if(tTri.uDivisionLevel < uSubdivisions)
        {

            plVec3 tP01 = pl_norm_vec3(pl_mul_vec3_scalarf(pl_add_vec3(tTri.atPoints[0], tTri.atPoints[1]), 0.5f));
            plVec3 tP12 = pl_norm_vec3(pl_mul_vec3_scalarf(pl_add_vec3(tTri.atPoints[1], tTri.atPoints[2]), 0.5f));
            plVec3 tP20 = pl_norm_vec3(pl_mul_vec3_scalarf(pl_add_vec3(tTri.atPoints[2], tTri.atPoints[0]), 0.5f));

            plTesselationTriangle tTri0 = {
                .atPoints       = {tP01, tTri.atPoints[1], tP12},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri1 = {
                .atPoints       = {tTri.atPoints[0], tP01, tP20},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri2 = {
                .atPoints       = {tP20, tP12, tTri.atPoints[2]},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };

            plTesselationTriangle tTri3 = {
                .atPoints       = {tP01, tP12, tP20},
                .uDivisionLevel =  tTri.uDivisionLevel + 1
            };
            
            pl_sb_push(sbtTessTris, tTri0);
            pl_sb_push(sbtTessTris, tTri1);
            pl_sb_push(sbtTessTris, tTri2);
            pl_sb_push(sbtTessTris, tTri3);
        }
        else
        {
            tTri.atPoints[0] = pl_mul_vec3(tTri.atPoints[0], tRadi);
            tTri.atPoints[1] = pl_mul_vec3(tTri.atPoints[1], tRadi);
            tTri.atPoints[2] = pl_mul_vec3(tTri.atPoints[2], tRadi);
            gptMeshBuilder->add_triangle(ptBuilder, tTri.atPoints[0], tTri.atPoints[1], tTri.atPoints[2]);
        }
    }

    pl_sb_free(sbtTessTris);

    // build
    gptMeshBuilder->commit(ptBuilder, NULL, NULL, &ptAppData->uGlobeIndexCount, &ptAppData->uGlobeVertexCount);
    ptAppData->puGlobeIndexBuffer = (uint32_t*)malloc(sizeof(uint32_t) * ptAppData->uGlobeIndexCount);
    ptAppData->ptGlobeVertexBuffer0 = (plVec3*)malloc(sizeof(plVec3) * ptAppData->uGlobeVertexCount);
    gptMeshBuilder->commit(ptBuilder, ptAppData->puGlobeIndexBuffer, ptAppData->ptGlobeVertexBuffer0, &ptAppData->uGlobeIndexCount, &ptAppData->uGlobeVertexCount);
    gptMeshBuilder->cleanup(ptBuilder);

    // submit to GPU
    plBufferDesc tVertexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_VERTEX,
        .szByteSize  = sizeof(plVec3) * ptAppData->uGlobeVertexCount,
        .pcDebugName = "vertex buffer",
    };

    plBufferDesc tIndexBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_INDEX,
        .szByteSize  = sizeof(uint32_t) * ptAppData->uGlobeIndexCount,
        .pcDebugName = "index buffer",
    };

    ptAppData->tGlobeVertexBuffer0 = gptGfx->create_buffer(ptDevice, &tVertexBufferDesc, NULL);
    ptAppData->tGlobeIndexBuffer = gptGfx->create_buffer(ptDevice, &tIndexBufferDesc, NULL);

    plBuffer* ptIndexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tGlobeIndexBuffer);

    const plDeviceMemoryAllocation tIndexMemory = gptGfx->allocate_memory(ptDevice,
        ptIndexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptIndexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "clipmap index memory");

    plBuffer* ptVertexBuffer = gptGfx->get_buffer(ptDevice, ptAppData->tGlobeVertexBuffer0);

    const plDeviceMemoryAllocation tVertexMemory = gptGfx->allocate_memory(ptDevice,
        ptVertexBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_DEVICE_LOCAL,
        ptVertexBuffer->tMemoryRequirements.uMemoryTypeBits,
        "clipmap vertex memory");

    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tGlobeIndexBuffer, &tIndexMemory);
    gptGfx->bind_buffer_to_memory(ptDevice, ptAppData->tGlobeVertexBuffer0, &tVertexMemory);


    size_t szMaxBufferSize = tVertexBufferDesc.szByteSize + tIndexBufferDesc.szByteSize;

    // create staging buffer
    plBufferDesc tStagingBufferDesc = {
        .tUsage      = PL_BUFFER_USAGE_STAGING,
        .szByteSize  = szMaxBufferSize,
        .pcDebugName = "staging buffer"
    };

    plBuffer* ptStagingBuffer = NULL;
    plBufferHandle tStagingBuffer = gptGfx->create_buffer(ptDevice, &tStagingBufferDesc, &ptStagingBuffer);

    // allocate memory for the vertex buffer
    const plDeviceMemoryAllocation tStagingBufferAllocation = gptGfx->allocate_memory(ptDevice,
        ptStagingBuffer->tMemoryRequirements.ulSize,
        PL_MEMORY_FLAGS_HOST_COHERENT | PL_MEMORY_FLAGS_HOST_VISIBLE,
        ptStagingBuffer->tMemoryRequirements.uMemoryTypeBits,
        "staging buffer memory");

    // bind the buffer to the new memory allocation
    gptGfx->bind_buffer_to_memory(ptDevice, tStagingBuffer, &tStagingBufferAllocation);

    memcpy(tStagingBufferAllocation.pHostMapped, ptAppData->puGlobeIndexBuffer, tIndexBufferDesc.szByteSize);
    free(ptAppData->puGlobeIndexBuffer);
    ptAppData->puGlobeIndexBuffer = NULL;
    memcpy(&tStagingBufferAllocation.pHostMapped[tIndexBufferDesc.szByteSize], ptAppData->ptGlobeVertexBuffer0, tVertexBufferDesc.szByteSize);
    free(ptAppData->ptGlobeVertexBuffer0);
    ptAppData->ptGlobeVertexBuffer0 = NULL;


    plCommandBuffer* ptCmdBuffer = gptStarter->get_temporary_command_buffer();

    plBlitEncoder* ptBlit = gptGfx->begin_blit_pass(ptCmdBuffer);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, ptAppData->tGlobeIndexBuffer, 0, 0, tIndexBufferDesc.szByteSize);
    gptGfx->copy_buffer(ptBlit, tStagingBuffer, ptAppData->tGlobeVertexBuffer0, (uint32_t)tIndexBufferDesc.szByteSize, 0, tVertexBufferDesc.szByteSize);
    gptGfx->pipeline_barrier_blit(ptBlit, PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_TRANSFER_WRITE, PL_PIPELINE_STAGE_VERTEX_SHADER | PL_PIPELINE_STAGE_COMPUTE_SHADER | PL_PIPELINE_STAGE_TRANSFER, PL_ACCESS_SHADER_READ | PL_ACCESS_TRANSFER_READ);
    gptGfx->end_blit_pass(ptBlit);

    gptStarter->submit_temporary_command_buffer(ptCmdBuffer);

    gptGfx->queue_buffer_for_deletion(ptDevice, tStagingBuffer);

    plShaderDesc tGlobeShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("solid_single.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("solid_single.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_LESS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 0,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            }
        },
        .atBlendStates = {
            {.bBlendEnabled = false}
        },
    };
    ptAppData->tGlobeShader = gptGfx->create_shader(ptDevice, &tGlobeShaderDesc);

    plShaderDesc tGlobeWireframeShaderDesc = {
        .tVertexShader     = gptShader->load_glsl("solid_single.vert", "main", NULL, NULL),
        .tPixelShader      = gptShader->load_glsl("solid_single.frag", "main", NULL, NULL),
        .tRenderPassLayout = gptStarter->get_render_pass_layout(),
        .tMSAASampleCount  = PL_SAMPLE_COUNT_1,
        .tGraphicsState = {
            .ulDepthWriteEnabled  = 1,
            .ulDepthMode          = PL_COMPARE_MODE_LESS,
            .ulCullMode           = PL_CULL_MODE_NONE,
            .ulWireframe          = 1,
            .ulStencilMode        = PL_COMPARE_MODE_ALWAYS,
            .ulStencilRef         = 0xff,
            .ulStencilMask        = 0xff,
            .ulStencilOpFail      = PL_STENCIL_OP_KEEP,
            .ulStencilOpDepthFail = PL_STENCIL_OP_KEEP,
            .ulStencilOpPass      = PL_STENCIL_OP_KEEP,
        },
        .atVertexBufferLayouts = {
            {
                .uByteStride = sizeof(float) * 3,
                .atAttributes = {
                    {.tFormat = PL_VERTEX_FORMAT_FLOAT3 }
                }
            }
        },
        .atBlendStates = {
            {.bBlendEnabled = false}
        },
    };
    ptAppData->tGlobeWireframeShader = gptGfx->create_shader(ptDevice, &tGlobeWireframeShaderDesc);

    // return app memory
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
    gptGfx->destroy_buffer(ptDevice, ptAppData->tGlobeIndexBuffer);
    gptGfx->destroy_buffer(ptDevice, ptAppData->tGlobeVertexBuffer0);
    gptGfx->destroy_shader(ptDevice, ptAppData->tGlobeShader);
    gptGfx->destroy_shader(ptDevice, ptAppData->tGlobeWireframeShader);

    gptStarter->cleanup();
    gptWindows->destroy(ptAppData->ptWindow);
    free(ptAppData);
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_resize
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_resize(plWindow* ptWindow, plAppData* ptAppData)
{
    gptStarter->resize();

    plIO* ptIO = gptIO->get_io();
    ptAppData->tCamera.fAspectRatio = ptIO->tMainViewportSize.x / ptIO->tMainViewportSize.y;
}

//-----------------------------------------------------------------------------
// [SECTION] pl_app_update
//-----------------------------------------------------------------------------

PL_EXPORT void
pl_app_update(plAppData* ptAppData)
{
    if(!gptStarter->begin_frame())
        return;

    // for convience
    plIO* ptIO = gptIO->get_io();

    static const float fCameraTravelSpeed = 10000.0f;
    static const float fCameraRotationSpeed = 0.005f;

    plCamera* ptCamera = &ptAppData->tCamera;

    // camera space
    if(gptIO->is_key_down(PL_KEY_W)) camera_translate(ptCamera,  0.0f,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_S)) camera_translate(ptCamera,  0.0f,  0.0f, -fCameraTravelSpeed* ptIO->fDeltaTime);
    if(gptIO->is_key_down(PL_KEY_A)) camera_translate(ptCamera, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);
    if(gptIO->is_key_down(PL_KEY_D)) camera_translate(ptCamera,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f,  0.0f);

    // world space
    if(gptIO->is_key_down(PL_KEY_F)) { camera_translate(ptCamera,  0.0f, -fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }
    if(gptIO->is_key_down(PL_KEY_R)) { camera_translate(ptCamera,  0.0f,  fCameraTravelSpeed * ptIO->fDeltaTime,  0.0f); }

    if(gptIO->is_mouse_dragging(PL_MOUSE_BUTTON_LEFT, 1.0f))
    {
        const plVec2 tMouseDelta = gptIO->get_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT, 1.0f);
        camera_rotate(ptCamera,  -tMouseDelta.y * fCameraRotationSpeed,  -tMouseDelta.x * fCameraRotationSpeed);
        gptIO->reset_mouse_drag_delta(PL_MOUSE_BUTTON_LEFT);
    }
    camera_update(ptCamera);

    if(gptUi->begin_window("Debug", NULL, 0))
    {
        gptUi->checkbox("Wireframe", &ptAppData->bWireframe);
        gptUi->end_window();
    }

    // 3d drawing API usage
    const plMat4 tOrigin = pl_identity_mat4();
    gptDraw->add_3d_transform(ptAppData->pt3dDrawlist, &tOrigin, 10.0f, (plDrawLineOptions){.fThickness = 0.2f});

    // start main pass & return the encoder being used
    plRenderEncoder* ptEncoder = gptStarter->begin_main_pass();

    const plMat4 tMVP = pl_mul_mat4(&ptCamera->tProjMat, &ptCamera->tViewMat);

    plDevice* ptDevice = gptStarter->get_device();

    plDynamicDataBlock tCurrentDynamicDataBlock = gptGfx->allocate_dynamic_data_block(ptDevice);


    plShaderHandle tShader = ptAppData->bWireframe ? ptAppData->tGlobeWireframeShader : ptAppData->tGlobeShader;

    gptGfx->bind_shader(ptEncoder, tShader);
    gptGfx->bind_vertex_buffer(ptEncoder, ptAppData->tGlobeVertexBuffer0);

    typedef struct _plDynamicGlobeData {
        
        plMat4 tCameraViewProjection;
        plVec3 tCameraPos;
    } plDynamicGlobeData;

    plDynamicBinding tDynamicBinding = pl_allocate_dynamic_data(gptGfx, ptDevice, &tCurrentDynamicDataBlock);
    plDynamicGlobeData* ptDynamicData = (plDynamicGlobeData*)tDynamicBinding.pcData;

    ptDynamicData->tCameraViewProjection = tMVP;
    ptDynamicData->tCameraPos = ptCamera->tPos;

    gptGfx->bind_graphics_bind_groups(ptEncoder, tShader, 0, 0, NULL, 1, &tDynamicBinding);

    plDrawIndex tDraw = {
        .uInstanceCount = 1,
        .uIndexCount    = ptAppData->uGlobeIndexCount,
        .tIndexBuffer   = ptAppData->tGlobeIndexBuffer
    };

    gptGfx->draw_indexed(ptEncoder, 1, &tDraw);

    // submit 3d drawlist
    gptDrawBackend->submit_3d_drawlist(ptAppData->pt3dDrawlist,
        ptEncoder,
        ptIO->tMainViewportSize.x,
        ptIO->tMainViewportSize.y,
        &tMVP,
        PL_DRAW_FLAG_DEPTH_TEST | PL_DRAW_FLAG_DEPTH_WRITE,
        gptGfx->get_swapchain_info(gptStarter->get_swapchain()).tSampleCount);

    // allows the starter extension to handle some things then ends the main pass
    gptStarter->end_main_pass();

    // must be the last function called when using the starter extension
    gptStarter->end_frame(); 
}

//-----------------------------------------------------------------------------
// [SECTION] helper function declarations
//-----------------------------------------------------------------------------

static inline float
wrap_angle(float tTheta)
{
    static const float f2Pi = 2.0f * PL_PI;
    const float fMod = fmodf(tTheta, f2Pi);
    if (fMod > PL_PI)       return fMod - f2Pi;
    else if (fMod < -PL_PI) return fMod + f2Pi;
    return fMod;
}

void
camera_translate(plCamera* ptCamera, float fDx, float fDy, float fDz)
{
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tRightVec, fDx));
    ptCamera->tPos = pl_add_vec3(ptCamera->tPos, pl_mul_vec3_scalarf(ptCamera->_tForwardVec, fDz));
    ptCamera->tPos.y += fDy;
}

void
camera_rotate(plCamera* ptCamera, float fDPitch, float fDYaw)
{
    ptCamera->fPitch += fDPitch;
    ptCamera->fYaw += fDYaw;

    ptCamera->fYaw = wrap_angle(ptCamera->fYaw);
    ptCamera->fPitch = pl_clampf(0.995f * -PL_PI_2, ptCamera->fPitch, 0.995f * PL_PI_2);
}

void
camera_update(plCamera* ptCamera)
{
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~update view~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    // world space
    static const plVec4 tOriginalUpVec      = {0.0f, 1.0f, 0.0f, 0.0f};
    static const plVec4 tOriginalForwardVec = {0.0f, 0.0f, 1.0f, 0.0f};
    static const plVec4 tOriginalRightVec   = {-1.0f, 0.0f, 0.0f, 0.0f};

    const plMat4 tXRotMat   = pl_mat4_rotate_vec3(ptCamera->fPitch, tOriginalRightVec.xyz);
    const plMat4 tYRotMat   = pl_mat4_rotate_vec3(ptCamera->fYaw, tOriginalUpVec.xyz);
    const plMat4 tZRotMat   = pl_mat4_rotate_vec3(ptCamera->fRoll, tOriginalForwardVec.xyz);
    const plMat4 tTranslate = pl_mat4_translate_vec3((plVec3){ptCamera->tPos.x, ptCamera->tPos.y, ptCamera->tPos.z});

    // rotations: rotY * rotX * rotZ
    plMat4 tRotations = pl_mul_mat4t(&tXRotMat, &tZRotMat);
    tRotations        = pl_mul_mat4t(&tYRotMat, &tRotations);

    // update camera vectors
    ptCamera->_tRightVec   = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalRightVec)).xyz;
    ptCamera->_tUpVec      = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalUpVec)).xyz;
    ptCamera->_tForwardVec = pl_norm_vec4(pl_mul_mat4_vec4(&tRotations, tOriginalForwardVec)).xyz;

    // update camera transform: translate * rotate
    ptCamera->tTransformMat = pl_mul_mat4t(&tTranslate, &tRotations);

    // update camera view matrix
    ptCamera->tViewMat   = pl_mat4t_invert(&ptCamera->tTransformMat);

    // flip x & y so camera looks down +z and remains right handed (+x to the right)
    const plMat4 tFlipXY = pl_mat4_scale_xyz(-1.0f, -1.0f, 1.0f);
    ptCamera->tViewMat   = pl_mul_mat4t(&tFlipXY, &ptCamera->tViewMat);

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~update projection~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    const float fInvtanHalfFovy = 1.0f / tanf(ptCamera->fFieldOfView / 2.0f);
    ptCamera->tProjMat.col[0].x = fInvtanHalfFovy / ptCamera->fAspectRatio;
    ptCamera->tProjMat.col[1].y = fInvtanHalfFovy;
    ptCamera->tProjMat.col[2].z = ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[2].w = 1.0f;
    ptCamera->tProjMat.col[3].z = -ptCamera->fNearZ * ptCamera->fFarZ / (ptCamera->fFarZ - ptCamera->fNearZ);
    ptCamera->tProjMat.col[3].w = 0.0f;  
}
